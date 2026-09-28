"""Original synthetic fixtures: importer, metrics and review invariants, no external data."""
import importlib.util
import json
from pathlib import Path
import sqlite3
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from benchmark.importers.korean_release.import_release import align,compact,convert,parse_tn,route,run as import_run
from benchmark.evaluators.korean_release import add,finish,metric,read_reviews,run as evaluate


def row(text='물건은 3개다.',tn='물건은 [세 개]다.',written='3개',spoken='세 개',theme='개수'):
 return dict(ITN=text,TN=tn,NumberWord=[written],EnglishWord=[],patterns=[dict(patternTheme=theme,spans=[dict(ITN=written,TN=spoken)])])


def fixture(path,rows):
 path.mkdir()
 data=dict(category_id=1,category='자체 검증',category_en='fixture',split='Training',source='original',count=len(rows),data=rows)
 name='01_fixture.json';(path/name).write_text(json.dumps(data,ensure_ascii=False))
 summary=dict(name='Original synthetic fixture',split='Training',totals=dict(files=1,records=len(rows)),files=[dict(file=name,category_id=1,category='자체 검증',category_en='fixture',records=len(rows))])
 (path/'summary.json').write_text(json.dumps(summary,ensure_ascii=False))


class FakeBackend:
 def run(self,text,lang):
  if text=='오류 1':raise ValueError('synthetic engine error')
  actual={'물건은 3개다.':'물건은 세개다.','금액 2원':'금액 삼 원','검토 1':'검토 일'}.get(text,text)
  # Deliberately whole sentence: span boundary misses must stay in denominator.
  return dict(text=actual,spans=[dict(input_begin=0,input_end=len(text.encode()),begin=0,end=len(actual.encode()),semantic_type='TEXT',rule='fixture')])


class Contracts(unittest.TestCase):
 def test_whitespace_only(self):
  m=metric()
  for a,b in [('세 개','세\t개'),('A','a'),('１','1'),('하나.','하나'),('가\u200b나','가나'),('A\u00a0B','AB')]:add(m,a,b)
  r=finish(m);self.assertEqual((r['exact'],r['compact'],r['spacing_only']), (0,2,2))
  add(m,'일',None,'error');add(m,None,'x');self.assertEqual((m['scorable'],m['errors'],m['unscorable']),(7,1,1))
 def test_alignment(self):
  d=row('3개와 3개','[세 개]와 [세 개]');d['NumberWord']*=2;d['patterns'][0]['spans']*=2
  r=convert(d,'fixture',1,'0'*64,'fixture','fixture')
  self.assertEqual([(s['begin'],s['end']) for s in r['spans']],[(0,4),(8,12)])
  self.assertFalse(r['flags']);self.assertEqual(r['expected'],'세 개와 세 개')
  d=row('3개다.','[세 개]였다.');r=convert(d,'fixture',1,'0'*64,'fixture','fixture')
  self.assertIn('unaccounted-source-or-reference-text',r['flags']);self.assertIsNone(r['spans'][0]['begin'])
  for text in ('[일','일]','[[일]]','[]'):
   with self.assertRaises(ValueError):parse_tn(text)
  r=convert(row(tn='물건은 [세 개]다.]'),'x',1,'0'*64,'x','x');self.assertIsNone(r['expected'])
 def test_topic_not_class(self):
  self.assertEqual(route('94.2%','가지')[0],'pattern')
  self.assertEqual(route('K5','외래어결합')[0],'lexicon')
  self.assertEqual(route('01234','우편번호')[0],'context-control')
  self.assertEqual(route('3년','사투리')[0],'pattern')
 def test_pipeline_reviews_and_corruption(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp);source=root/'source';fixture(source,[row(),row('금액 2원','금액 [이 원]','2원','이 원'),row('오류 1','오류 [일]','1','일'),row('검토 1','검토 [일','1','일')])
   imported=root/'import';audit=import_run(source,imported)
   self.assertEqual(audit['counts']['imported'],4);self.assertEqual(audit['counts']['sentence_references'],3)
   with self.assertRaises(ValueError):import_run(source,imported)
   lib=root/'library';lib.write_text('fixture');resources=root/'resources';resources.mkdir();(resources/'rules').write_text('fixture')
   report=evaluate(imported,lib,resources,root/'report',backend=FakeBackend())
   self.assertEqual(report['sentences']['scorable'],3);self.assertEqual(report['sentences']['compact'],1)
   self.assertEqual(report['sentences']['errors'],1);self.assertEqual(report['spans']['unaligned'],4)
   self.assertEqual(report['reviewed_sentences']['total'],0)
   with sqlite3.connect(root/'report/report.sqlite') as db:self.assertEqual(db.execute('select count(*) from cases').fetchone()[0],4)
   records=[json.loads(l) for l in (imported/'records.jsonl').read_text().splitlines()]
   reviews=root/'reviews.jsonl';reviews.write_text(json.dumps(dict(id=records[1]['id'],record_sha256=records[1]['record_sha256'],status='corrected',expected='금액 삼 원',reviewer='fixture reviewer',reviewed_at='2026-09-28',reason='Synthetic test only'))+'\n')
   adjusted=evaluate(imported,lib,resources,root/'reviewed',reviews,backend=FakeBackend())
   self.assertEqual(adjusted['sentences'],report['sentences'])
   self.assertEqual(adjusted['reviewed_sentences']['exact'],1)
   self.assertNotEqual(adjusted['comparison_key'],report['comparison_key'])
   reviews.write_text(json.dumps(json.loads(reviews.read_text()),indent=2))
   self.assertEqual(len(read_reviews(reviews)),1)
   reviews.write_text(reviews.read_text().replace(records[1]['record_sha256'],'0'*64))
   with self.assertRaises(ValueError):evaluate(imported,lib,resources,root/'stale',reviews,backend=FakeBackend())
   (imported/'records.jsonl').write_text('{}\n')
   with self.assertRaises(ValueError):evaluate(imported,lib,resources,root/'corrupt',backend=FakeBackend())

 def test_inputs_mutated_during_evaluation(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);fixture(root/'source',[row()]);import_run(root/'source',root/'import')
   (root/'lib').write_text('fixture');(root/'resources').mkdir()
   class Mutating(FakeBackend):
    def run(self,text,lang):
     (root/'resources/changed').write_text('changed')
     return super().run(text,lang)
   with self.assertRaisesRegex(ValueError,'changed during'):
    evaluate(root/'import',root/'lib',root/'resources',root/'report',backend=Mutating())
   self.assertFalse((root/'report/summary.json').exists())

if __name__=='__main__':unittest.main()
