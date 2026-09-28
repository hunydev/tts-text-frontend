"""Original synthetic fixtures for the engine-independent Korean reference validator."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from benchmark.evaluators.korean_reference_validator import Validator,main
from benchmark.evaluators.korean_release import read_reviews


def record(text,expected,spans,category='fixture'):
 """spans: (written, spoken) in source order; offsets are UTF-8 bytes as the importer writes them."""
 raw,out,at=text.encode(),[],0
 for written,spoken in spans:
  begin=raw.index(written.encode(),at);end=begin+len(written.encode());at=end
  out.append(dict(written=written,spoken=spoken,begin=begin,end=end))
 return dict(id='fixture/'+text,record_sha256=hashlib.sha256(text.encode()).hexdigest(),
             text=text,expected=expected,spans=out,category_en=category)


def codes(r):
 return [code for code,_ in Validator().check_record(r) or []]


class ReferenceErrors(unittest.TestCase):
 def test_flagged(self):
  cases=[
   (record('사과 25개','사과 스물여섯 개',[('25개','스물여섯 개')]),'value-mismatch'),
   (record('학생 15명','학생 십모 명',[('15명','십모 명')]),'non-number-syllable'),
   (record('번호 015-1234-7412','번호 공오일 다시 일이삼사 다시 칠사일이',
           [('015-1234-7412','공오일 다시 일이삼사 다시 칠사일이')]),'identifier-digits'),
   (record('동홍주공 3단지','동흥주공 삼 단지',[('동홍주공 3단지','동흥주공 삼 단지')]),'korean-text-edited'),
   (record('철수는 3개를 샀다.','철수가 세 개를 샀다.',[('3개','세 개')]),'korean-text-edited')]
  for r,code in cases:
   with self.subTest(r['text']):self.assertEqual(codes(r),[code])

 def test_policy_variants_are_not_errors(self):
  cases=[
   record('30명','서른 명',[('30명','서른 명')]),
   record('30명','삼십 명',[('30명','삼십 명')]),
   record('6월','유월',[('6월','유월')]),
   record('10월생','시 월생',[('10월생','시 월생')]),
   record('3주','셋째 주',[('3주','셋째 주')]),
   record('120개','백스무 개',[('120개','백스무 개')]),
   record('31데이','써리 원 데이',[('31데이','써리 원 데이')]),
   record('2019','둘 영 하나 구',[('2019','둘 영 하나 구')]),
   record('123-45','일이삼 사오',[('123-45','일이삼 사오')]),
   record('123-45','일이삼 다시 사오',[('123-45','일이삼 다시 사오')]),
   record('123-45','백이십삼 다시 사십오',[('123-45','백이십삼 다시 사십오')]),
   # The span covers only part of a number: its boundary is wrong, not its reading.
   record('8,206명','팔천이백육 명',[('8','팔천이백육')]),
   record('6358-95114','육삼오팔 구오일일사',[('6358-9511','육삼오팔 구오일일')]),
   # Dialect readings are the purpose of that category.
   record('야 8개','야 야달 개',[('8개','야달 개')],category='dialect'),
   # Unannotated conversions outside spans are the source's choice.
   record('PIN 3개','핀 세 개',[('3개','세 개')])]
  for r in cases:
   with self.subTest(r['text'],spoken=r['spans'][0]['spoken']):self.assertEqual(codes(r),[])

 def test_unscorable_reference_is_skipped(self):
  r=record('25개','스물여섯 개',[('25개','스물여섯 개')]);r['expected']=None
  self.assertIsNone(Validator().check_record(r))

 def test_review_file_for_evaluator(self):
  with tempfile.TemporaryDirectory() as temp:
   root=Path(temp);(root/'import').mkdir()
   rows=[record('사과 25개','사과 스물여섯 개',[('25개','스물여섯 개')]),record('30명','서른 명',[('30명','서른 명')])]
   (root/'import/records.jsonl').write_text(''.join(json.dumps(r,ensure_ascii=False)+'\n' for r in rows),encoding='utf-8')
   out=root/'reviews/errors.jsonl'
   self.assertEqual(main(['--import-dir',str(root/'import'),'--reviews-out',str(out),'--reviewed-at','2026-09-28']),0)
   reviews=read_reviews(out)
   self.assertEqual(list(reviews),[rows[0]['id']])
   review=reviews[rows[0]['id']]
   self.assertEqual((review['status'],review['record_sha256']),('excluded',rows[0]['record_sha256']))
   self.assertTrue(review['reviewer'].startswith('auto:korean-reference-validator/'))
   self.assertIn('value-mismatch',review['reason'])
   with self.assertRaises(SystemExit):
    main(['--import-dir',str(root/'import'),'--reviews-out',str(out)])


if __name__=='__main__':unittest.main()
