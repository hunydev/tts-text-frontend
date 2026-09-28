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
 def test_assets(self):
  for path in ['/ko-quality','/ko-quality.js','/ko-quality.css']:
   with urllib.request.urlopen(self.base+path) as r:
    self.assertEqual(r.status,200);self.assertIn("default-src 'self'",r.headers['Content-Security-Policy']);self.assertGreater(len(r.read()),100)

if __name__=='__main__':unittest.main()
