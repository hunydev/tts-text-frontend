"""Original synthetic fixtures: English scope, NeMo evaluation-set importer, sentence evaluator."""
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from benchmark.english_scope import foreign_scripts
from benchmark.evaluators.english_sentences import level,loose,run as evaluate
from benchmark.importers.nemo_eval.import_nemo_eval import main as import_main

REVISION='0123456789abcdef0123456789abcdef01234567'


def data_dir(root):
 root.mkdir()
 (root/'GoogleTN.json').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in [
  dict(text='It costs $5 .',gt_normalized='It costs five dollars .'),
  dict(text='Hua Shao ( 华 少 ) won in 2019 .',gt_normalized='Hua Shao ( 华 少 ) won in twenty nineteen .')]),encoding='utf-8')
 (root/'LibriTTS.json').write_text(json.dumps(dict(text='CHAPTER IX.',gt_normalized='CHAPTER nine.',transcript='chapter nine'))+'\n')
 (root/'EngConf.txt').write_text('# synthetic header\n# \n\nHenry III~~RAW\nHenry one~~0\nHenry the third~~1\nHenry third~~1\n\n'
                                 'Room 12~~RAW\nRoom twelve~~1\n',encoding='utf-8')
 return root


class Fake:
 OUT={'It costs $5 .':'It costs five dollars .','CHAPTER IX.':'chapter nine','Henry III':'Henry one','Room 12':'Room twelve'}
 def run(self,text,lang):
  out=self.OUT[text]
  return dict(text=out,spans=[dict(input_begin=0,input_end=len(text.encode()),begin=0,end=len(out.encode()),semantic_type='TEXT',rule='fixture')])
 def close(self):pass


class English(unittest.TestCase):
 def test_scope(self):
  for text,found in [('It is 5 pm .',[]),('Nguyễn met the café owner .',[]),('Δ ( x ) is 5 μm , 3 ℓ',[]),
                     ('서울 is 3 km away',['HANGUL']),('Hua Shao ( 华 少 )',['HAN']),('ソウル and とうきょう',['KANA']),
                     ('И л и я',['CYRILLIC']),('Athens ( Ἀθῆναι )',['GREEK']),('東京 and 서울',['HAN','HANGUL'])]:
   with self.subTest(text):self.assertEqual(foreign_scripts(text),found)

 def test_levels(self):
  self.assertEqual(level('a b',['a b']),'exact')
  self.assertEqual(level('ab',['a b']),'compact')
  self.assertEqual(level('Chapter nine',['CHAPTER nine.']),'loose')
  self.assertEqual(level('twenty-one',['twenty one']),'loose')
  self.assertIsNone(level('five dollar',['five dollars']))
  self.assertEqual(loose("I've `` got ''  it ."),'i ve got it')
  for a,b in [('the centre','the Center'),('organisations','organizations'),('colours','colors'),('programme','program'),
              ('travelled','traveled'),('analysed','analyzed'),('defence','defense'),('catalogue','catalog'),('anaemia','anemia')]:
   with self.subTest(a):self.assertEqual(level(a,[b]),'variant')
  for a,b in [('four','for'),('yrs','yours'),('second','seconds'),('twenty hyphen first','twenty first')]:
   with self.subTest(a):self.assertIsNone(level(a,[b]))

 def test_import_and_evaluate(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp);source=data_dir(root/'data');corpus=root/'nemo.jsonl'
   self.assertEqual(import_main(['--data-dir',str(source),'--revision',REVISION,'--out',str(corpus)]),0)
   rows=[json.loads(l) for l in corpus.read_text(encoding='utf-8').splitlines()]
   self.assertEqual([r['dataset'] for r in rows],['nemo-googletn']*2+['nemo-libritts']+['nemo-engconf']*2)
   henry=rows[3];self.assertEqual((henry['text'],henry['expected']),('Henry III','Henry the third'))
   self.assertEqual((henry['accepted_references'],henry['rejected_references']),(['Henry the third','Henry third'],['Henry one']))
   manifest=json.loads(corpus.with_suffix('.manifest.json').read_text())
   self.assertEqual(manifest['counts'],{'nemo-googletn':2,'nemo-libritts':1,'nemo-engconf':2})
   with self.assertRaises(SystemExit):import_main(['--data-dir',str(source),'--revision',REVISION,'--out',str(corpus)])
   (source/'EngConf.txt').write_text('Henry one~~0\n')
   with self.assertRaises(ValueError):import_main(['--data-dir',str(source),'--revision',REVISION,'--out',str(root/'bad.jsonl')])
   lib=root/'lib';lib.write_text('fixture');resources=root/'resources';resources.mkdir()
   report=evaluate([corpus],lib,resources,root/'report',backend=Fake())
   s=report['sentences']
   self.assertEqual((s['sentences'],s['out_of_scope'],s['scorable']),(5,1,4))
   self.assertEqual((s['exact'],s['compact'],s['loose'],s['variant'],s['rejected']),(2,2,3,3,1))
   self.assertEqual(report['datasets']['nemo-libritts']['loose'],1)
   self.assertEqual(report['out_of_scope_scripts'],{'HAN':1})
   excluded=[json.loads(l) for l in (root/'report/out-of-scope.jsonl').read_text(encoding='utf-8').splitlines()]
   self.assertEqual([e['scripts'] for e in excluded],[['HAN']])
   everything=evaluate([corpus],lib,resources,root/'all',scope='all',backend=type('F',(Fake,),{'OUT':dict(Fake.OUT,**{'Hua Shao ( 华 少 ) won in 2019 .':'x'})})())
   self.assertEqual(everything['sentences']['scorable'],5)
   self.assertNotEqual(everything['comparison_key'],report['comparison_key'])
   with self.assertRaises(ValueError):evaluate([corpus],lib,resources,root/'report',backend=Fake())


if __name__=='__main__':unittest.main()
