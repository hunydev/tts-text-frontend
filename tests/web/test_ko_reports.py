"""Read-only report routing, pagination, filters and untrusted text transport."""
import json
from pathlib import Path
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from urllib.parse import urlencode
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT),str(ROOT/'tools/web'),str(ROOT/'tests/benchmark')]
import ko_reports
from server import Server,Handler
from test_korean_release import fixture,row,FakeBackend
from benchmark.importers.korean_release.import_release import run as import_run
from benchmark.evaluators.korean_release import run as evaluate

class Reports(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory();root=Path(cls.temp.name)
  fixture(root/'source',[row() for _ in range(55)]+[row('<img onerror=alert(1)> 3개','<img onerror=alert(1)> [세 개]')])
  import_run(root/'source',root/'import');(root/'lib').write_text('fixture');(root/'resources').mkdir()
  cls.reports=root/'reports';cls.reports.mkdir()
  evaluate(root/'import',root/'lib',root/'resources',cls.reports/'test',backend=FakeBackend())
  (cls.reports/'partial').mkdir()
  cls.server=Server(('127.0.0.1',0),Handler);cls.server.ko_report_root=cls.reports
  cls.thread=threading.Thread(target=cls.server.serve_forever,daemon=True);cls.thread.start()
  cls.base=f'http://127.0.0.1:{cls.server.server_port}'
 @classmethod
 def tearDownClass(cls):
  cls.server.shutdown();cls.server.server_close();cls.thread.join();cls.temp.cleanup()
 def get(self,path,**args):
  with urllib.request.urlopen(self.base+path+'?'+urlencode(args)) as r:return json.load(r)
 def test_catalog_and_partial(self):
  r=self.get('/api/ko-reports');self.assertEqual(r['latest'],'test');self.assertEqual(len(r['reports']),1)
  with self.assertRaises(urllib.error.HTTPError) as e:self.get('/api/ko-report',run='partial')
  self.assertEqual(e.exception.code,503)
 def test_pagination_and_filters(self):
  r=self.get('/api/ko-cases',run='test');self.assertEqual((r['total'],len(r['entries'])),(56,50))
  r=self.get('/api/ko-cases',run='test',page=1);self.assertEqual(len(r['entries']),6)
  r=self.get('/api/ko-cases',run='test',status='spacing');self.assertEqual(r['total'],55)
  r=self.get('/api/ko-cases',run='test',q='<img');self.assertEqual(r['total'],1)
  full=self.get('/api/ko-cases',run='test',id=r['entries'][0]['id']);self.assertIn('engine_spans',full['entries'][0])
  r=self.get('/api/ko-cases',run='test',scope='span',theme='개수');self.assertEqual(r['total'],56)
  self.assertIn('selected_span',r['entries'][0])
  self.assertEqual(self.get('/api/ko-cases',run='test',category="' OR 1=1 --")['total'],0)
 def test_traversal_and_limits(self):
  for args in [dict(run='../test'),dict(run='test',page=-1),dict(run='test',scope='bad'),dict(run='test',q='x'*201),dict(run='test',unknown='x')]:
   with self.assertRaises(urllib.error.HTTPError) as e:self.get('/api/ko-cases',**args)
   self.assertEqual(e.exception.code,400)
  with self.assertRaises(ValueError):ko_reports.arguments('run=a&run=b')
  (self.reports/'linked').symlink_to(self.reports/'test',target_is_directory=True)
  with self.assertRaises(ValueError):ko_reports.summary(self.reports,'linked')
 def test_decisions_stage(self):
  self.assertIsNone(self.get('/api/ko-report',run='test')['decisions'])
  import ko_decisions
  class Fake:
   tokens=0
   def score(self,written,spoken):return 0.9
  cands=ko_decisions.candidates(self.reports,'test')
  ko_decisions.score(self.reports,'test',judge=Fake(),workers=2)
  d=self.get('/api/ko-report',run='test')['decisions'];m=self.get('/api/ko-report',run='test')['sentences']
  self.assertEqual(d['sentences']['candidates'],len(cands))
  self.assertEqual(d['sentences']['final_wrong'],m['scorable']-m['similar']-d['sentences']['accepted'])
  self.assertEqual(self.get('/api/ko-cases',run='test',status='decisions-pass')['total'],d['sentences']['accepted'])
  self.assertEqual(self.get('/api/ko-cases',run='test',status='final-wrong')['total'],
                   self.get('/api/ko-cases',run='test',status='different')['total']-d['sentences']['accepted']+
                   self.get('/api/ko-cases',run='test',status='error')['total'])
  with self.assertRaises(urllib.error.HTTPError) as e:self.get('/api/ko-cases',run='test',scope='span',status='final-wrong')
  self.assertEqual(e.exception.code,400)
  i=self.get('/api/decisions/instructions');self.assertEqual(i['model'],'gpt-6-luna');self.assertNotIn('sk-',json.dumps(i))
  # A Jev stage next to it: both-wrong lists what neither judge accepts, sorted by score.
  import ko_jev
  with self.assertRaises(urllib.error.HTTPError) as e:self.get('/api/ko-cases',run='test',status='both-wrong')
  self.assertEqual(e.exception.code,400)
  ko_jev.score(self.reports,'test',cache=self.reports/'jev-cache.jsonl',batch=lambda pairs,lang,cache,progress=None:({p:(0.2,'jev-test') for p in pairs},len(pairs)))
  r=self.get('/api/ko-report',run='test');self.assertEqual(r['jev']['sentences']['accepted'],0)
  self.assertEqual(r['both']['sentences']['accepted'],r['decisions']['sentences']['accepted'])
  both=self.get('/api/ko-cases',run='test',status='both-wrong',sort='mean-asc')
  different=self.get('/api/ko-cases',run='test',status='different')['total'];errors=self.get('/api/ko-cases',run='test',status='error')['total']
  self.assertEqual(both['total'],different-r['both']['sentences']['accepted']+errors)
  ordered=self.get('/api/ko-cases',run='test',status='different',sort='jev-desc')['entries']
  self.assertTrue(all('jev' in e for e in ordered))
  for bad in (dict(sort='bad'),dict(sort='mean-asc',scope='span')):
   with self.assertRaises(urllib.error.HTTPError) as e:self.get('/api/ko-cases',run='test',**bad)
   self.assertEqual(e.exception.code,400)
  (self.reports/'test'/'jev-final.json').unlink()
  (self.reports/'test'/'decisions-final.json').unlink()
 def test_assets(self):
  for path in ['/ko-quality','/ko-quality.js','/ko-quality.css']:
   with urllib.request.urlopen(self.base+path) as r:
    self.assertEqual(r.status,200);self.assertIn("default-src 'self'",r.headers['Content-Security-Policy']);self.assertGreater(len(r.read()),100)

if __name__=='__main__':unittest.main()
