"""English report routing, filters, baseline changes and untrusted text transport."""
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
import en_reports
from server import Server,Handler
from test_english_sentences import Fake,data_dir,REVISION
from benchmark.evaluators.english_sentences import run as evaluate
from benchmark.importers.nemo_eval.import_nemo_eval import main as import_main


class Reports(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.temp=tempfile.TemporaryDirectory();root=Path(cls.temp.name)
  corpus=root/'nemo.jsonl';data_dir(root/'data')
  with open(root/'data/GoogleTN.json','a',encoding='utf-8') as f:
   f.write(json.dumps(dict(text='<img src=x onerror=alert(1)> 5 .',gt_normalized='<img src=x onerror=alert(1)> five .'))+'\n')
  Fake.OUT['<img src=x onerror=alert(1)> 5 .']='<img src=x onerror=alert(1)> five .'
  import_main(['--data-dir',str(root/'data'),'--revision',REVISION,'--out',str(corpus)])
  lib=root/'lib';lib.write_text('fixture');resources=root/'resources';resources.mkdir()
  cls.reports=root/'reports';cls.reports.mkdir()
  evaluate([corpus],lib,resources,cls.reports/'before',backend=Fake())
  better=type('Better',(Fake,),{'OUT':dict(Fake.OUT,**{'Henry III':'Henry the third','Room 12':'Room one two'})})
  evaluate([corpus],lib,resources,cls.reports/'after',backend=better())
  evaluate([corpus],lib,resources,cls.reports/'text',mode='text',backend=Fake())
  (cls.reports/'partial').mkdir()
  cls.server=Server(('127.0.0.1',0),Handler);cls.server.en_report_root=cls.reports
  cls.thread=threading.Thread(target=cls.server.serve_forever,daemon=True);cls.thread.start()
  cls.base=f'http://127.0.0.1:{cls.server.server_port}'
 @classmethod
 def tearDownClass(cls):
  cls.server.shutdown();cls.server.server_close();cls.thread.join();cls.temp.cleanup()
 def get(self,path,**args):
  with urllib.request.urlopen(self.base+path+'?'+urlencode(args)) as r:return json.load(r)
 def test_catalog(self):
  r=self.get('/api/en-reports')
  self.assertEqual({x['run_id'] for x in r['reports']},{'before','after','text'})
  self.assertIn(r['latest'],('before','after'))  # speech first
  s=self.get('/api/en-report',run='after');self.assertEqual(s['run_id'],'after');self.assertIn('speech',s['label'])
  with self.assertRaises(urllib.error.HTTPError) as e:self.get('/api/en-report',run='partial')
  self.assertEqual(e.exception.code,400)
 def test_filters(self):
  r=self.get('/api/en-cases',run='before');self.assertEqual(r['total'],5)
  self.assertEqual(self.get('/api/en-cases',run='before',result='miss')['total'],1)  # rejected counts as a miss
  self.assertEqual(self.get('/api/en-cases',run='before',result='rejected')['total'],1)
  self.assertEqual(self.get('/api/en-cases',run='before',result='out-of-scope')['entries'][0]['scripts'],['HAN'])
  self.assertEqual(self.get('/api/en-cases',run='before',dataset='nemo-engconf')['total'],2)
  hit=self.get('/api/en-cases',run='before',q='<img')['entries'][0]
  self.assertEqual(hit['text'],'<img src=x onerror=alert(1)> 5 .')
  self.assertEqual(self.get('/api/en-cases',run='before',dataset="' OR 1=1 --")['total'],0)
 def test_baseline_changes(self):
  gained=self.get('/api/en-cases',run='after',baseline='before',change='gained')
  self.assertEqual([e['text'] for e in gained['entries']],['Henry III'])
  self.assertEqual(gained['entries'][0]['before'],'Henry one')
  lost=self.get('/api/en-cases',run='after',baseline='before',change='lost')
  self.assertEqual([e['text'] for e in lost['entries']],['Room 12'])
  changed=self.get('/api/en-cases',run='after',baseline='before',change='changed')
  self.assertEqual(changed['total'],2)
  with self.assertRaises(urllib.error.HTTPError) as e:  # speech and text do not compare
   self.get('/api/en-cases',run='after',baseline='text',change='gained')
  self.assertEqual(e.exception.code,400)
 def test_traversal_and_limits(self):
  for args in [dict(run='../before'),dict(run='before',page=-1),dict(run='before',result='bad'),
               dict(run='before',q='x'*201),dict(run='before',unknown='x'),dict(run='before',change='gained')]:
   with self.assertRaises(urllib.error.HTTPError) as e:self.get('/api/en-cases',**args)
   self.assertEqual(e.exception.code,400)
  with self.assertRaises(ValueError):en_reports.arguments('run=a&run=b')
  (self.reports/'linked').symlink_to(self.reports/'before',target_is_directory=True)
  with self.assertRaises(ValueError):en_reports.summary(self.reports,'linked')
 def test_jev_stage(self):
  # No Jev stage: the report says so and the Jev filters find nothing.
  self.assertIsNone(self.get('/api/en-report',run='after')['jev'])
  self.assertEqual(self.get('/api/en-cases',run='after',result='jev-pass')['total'],0)
  misses=[e for e in self.get('/api/en-cases',run='after',result='')['entries'] if e['result']=='miss']
  self.assertTrue(misses)
  import en_jev
  calls=[]
  def fake(pairs,language,cache,progress=None):
   calls.append((list(pairs),language));return {p:(0.9,'jev-test') for p in pairs},len(pairs)
  en_jev.score(self.reports,'after',cache=self.reports/'cache.jsonl',batch=fake)
  self.assertEqual(calls[0][1],'en-US');self.assertEqual(len(calls[0][0]),len(misses))
  r=self.get('/api/en-report',run='after')['jev']
  s=r['sentences'];self.assertEqual((s['misses'],s['scored'],s['accepted']),(len(misses),len(misses),len(misses)))
  summary=self.get('/api/en-report',run='after')['sentences']
  self.assertEqual(s['final_wrong'],summary['scorable']-summary['variant']-s['accepted'])
  self.assertEqual(self.get('/api/en-cases',run='after',result='jev-pass')['total'],len(misses))
  wrong=self.get('/api/en-cases',run='after',result='final-wrong')['entries']
  self.assertTrue(all(e['result'] in ('rejected','error') for e in wrong))  # the known wrong reading stays wrong
  self.assertEqual(self.get('/api/en-cases',run='after',result='jev-pass')['entries'][0]['jev'],0.9)
  (self.reports/'after'/'jev-final.json').unlink()
 def test_assets(self):
  for path in ['/en-quality','/en-quality.js']:
   with urllib.request.urlopen(self.base+path) as r:
    self.assertEqual(r.status,200);self.assertIn("default-src 'self'",r.headers['Content-Security-Policy']);self.assertGreater(len(r.read()),100)

if __name__=='__main__':unittest.main()
