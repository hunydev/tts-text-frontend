"""Independent whitespace diagnostics, immutable reports and stale UI sidecars."""
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
import zlib

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from benchmark.common import sha256
from benchmark.evaluators.spacing_audit import explain, run
from tools.web.ko_reports import spacing


def span(text,kind='CARDINAL',**kw):
    return dict(begin=0,end=len(text.encode()),semantic_type=kind,**kw)


class Audit(unittest.TestCase):
    def test_distinct_policies(self):
        r=explain('만 이천 오백 원','만이천오백 원',[span('만이천오백 원')])
        self.assertEqual([x['kind'] for x in r],['myriad-group','numeral-internal'])
        self.assertEqual([x['offset'] for x in r],[1,3])
        self.assertEqual([x['direction'] for x in r],['output-missing']*2)
        self.assertEqual(explain('공 공 사 육','공공사육',[span('공공사육','DIGIT_SEQUENCE')])[0]['kind'],'digit-sequence')
        self.assertEqual(explain('유 월','유월',[span('유월','COUNTER',unit='월')])[0]['kind'],'lexical-word')
        self.assertEqual(explain('열 두 명','열두 명',[span('열두 명','COUNTER')])[0]['kind'],'numeral-internal')
        self.assertEqual(explain('십오일','십오 일',[span('십오 일','COUNTER',unit='일')])[0]['kind'],'unit-or-affix')
        self.assertEqual(explain('한\n줄','한 줄',[])[0]['kind'],'layout')
        self.assertEqual(explain('만일','만 일',[span('만 일')])[0]['direction'],'output-extra')

    def test_no_score_forgiveness(self):
        for a,b in [('abc','ABC'),('한.','한'),('Ａ','A'),('1','2'),(None,'한'),('한','한')]:
            self.assertEqual(explain(a,b,[]),[])
        # Unknown boundaries stay review candidates, never a mandatory label.
        self.assertEqual(explain('가 나','가나',[])[0]['kind'],'boundary-review')

    def test_report_and_stale_guard(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);report=root/'reports'/'fixture';report.mkdir(parents=True)
            db=sqlite3.connect(report/'report.sqlite')
            db.execute('CREATE TABLE cases(id TEXT, status TEXT, details BLOB)')
            row=dict(id='original',category_en='number',expected='만 이천 오백 원',actual='만이천오백 원',engine_spans=[span('만이천오백 원')])
            db.execute('INSERT INTO cases VALUES(?,?,?)',('original','spacing',zlib.compress(json.dumps(row).encode())))
            db.commit();db.close()
            summary=dict(schema_version=1,status='complete',run_id='fixture',database_sha256=sha256(report/'report.sqlite'),sentences=dict(spacing_only=1))
            (report/'summary.json').write_text(json.dumps(summary))
            before=(sha256(report/'summary.json'),sha256(report/'report.sqlite'))
            self.assertFalse(spacing(root/'reports','fixture')['available'])
            dest=root/'spacing'/'fixture';r=run(report,dest)
            self.assertEqual(r['sentences'],1);self.assertEqual(r['boundaries'],2)
            self.assertEqual(r['categories'][0]['id'],'number')
            self.assertEqual(r['categories'][0]['sentences'],1)
            self.assertEqual(r['categories'][0]['boundaries'],2)
            self.assertEqual(r['categories'][0]['groups'],r['groups'])
            self.assertTrue(spacing(root/'reports','fixture')['available'])
            self.assertEqual(before,(sha256(report/'summary.json'),sha256(report/'report.sqlite')))
            with self.assertRaises(ValueError):run(report,dest)
            summary['label']='changed';(report/'summary.json').write_text(json.dumps(summary))
            with self.assertRaises(ValueError):spacing(root/'reports','fixture')
            with self.assertRaises(ValueError):spacing(root/'reports','../fixture')


if __name__=='__main__':unittest.main()
