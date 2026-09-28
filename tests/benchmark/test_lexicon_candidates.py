"""Original fixtures for review-candidate counting and immutable report checks."""
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest
import zlib
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from benchmark.evaluators.lexicon_candidates import collect,run
from benchmark.common import sha256

def span(written,spoken,actual,status='different',types=('WORD',)):
    return dict(written=written,spoken=spoken,actual=actual,status=status,engine_types=list(types))

class Candidates(unittest.TestCase):
    def test_reading_variants_not_majority_gold(self):
        result=collect([('a','content',False,span('TV','티비','티브이')),
                        ('a','content',False,span('tv','티 브이','티브이','spacing')),
                        ('b','finance',True,span('KB','케이비',None,'unaligned',('MEASURE',)))])
        tv=next(r for r in result['candidates'] if r['term']=='tv')
        self.assertEqual((tv['occurrences'],tv['sentences'],tv['mismatches']),(2,1,1))
        self.assertEqual(tv['references'],{'티비':1,'티브이':1})
        self.assertIn('multiple-reference-readings',tv['review_signals'])
        kb=next(r for r in result['candidates'] if r['term']=='kb')
        self.assertEqual((kb['unaligned'],kb['flagged']),(1,1))
        self.assertIn('classifier-or-context-review',kb['review_signals'])
        self.assertEqual(kb['outputs'],{})
    def test_shapes_and_whitespace(self):
        r=collect([(str(i),'fixture',False,span(w,'발음','다름')) for i,w in enumerate(['GS   SHOP','gs shop','1234','Windows 10','AB123','a1b','한글'])])
        self.assertEqual(r['summary']['terms'],2)
        self.assertEqual(r['rejected'],{'unsupported-lexical-shape':4})
        self.assertEqual(r['candidates'][0]['term'],'gs shop')
        self.assertEqual(r['candidates'][0]['occurrences'],2)
    def test_completed_immutable_report(self):
        with tempfile.TemporaryDirectory() as temp:
            p=Path(temp);db=p/'report.sqlite'
            with sqlite3.connect(db) as con:
                con.executescript('CREATE TABLE cases(id TEXT,flagged INTEGER); CREATE TABLE spans(id INTEGER,case_id TEXT,category TEXT,route TEXT,details BLOB);')
                con.execute('INSERT INTO cases VALUES (?,?)',('original',0))
                con.execute('INSERT INTO spans VALUES (1,?,?,?,?)',('original','fixture','lexicon',zlib.compress(json.dumps(span('Widget','위젯','다름')).encode())))
            summary=p/'summary.json';summary.write_text(json.dumps(dict(schema_version=1,status='complete',run_id='fixture',database_sha256=sha256(db))))
            out=p/'out.json';r=run(p,out);self.assertEqual(r['summary']['terms'],1)
            with self.assertRaisesRegex(ValueError,'output exists'):run(p,out)
            with sqlite3.connect(db) as con:con.execute('DELETE FROM cases')
            with self.assertRaisesRegex(ValueError,'checksum'):run(p,p/'corrupt.json')
            self.assertFalse((p/'corrupt.json').exists())

if __name__=='__main__':unittest.main()
