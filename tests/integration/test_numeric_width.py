"""Independent gold plus ASCII-equivalence, original offsets, protection and streams."""
import hashlib,json,random,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tools/web'),str(ROOT/'tools/evaluation')]
from native import Engine,flatten
from evaluate import frontend_settings
library,resources=[Path(x).resolve() for x in sys.argv[1:]]
corpus=ROOT/'tests/evaluation/numeric_width_v22.jsonl'
# 2026-09-28 user policy: width-short-zero now retains each digit; preimage and
# reason remain in docs/audit/phone-zero-policy-20260928.json. Keep the exact pin.
# 2026-09-30 (v0.24): range rows follow the range-reading decision.
# Hash updated 2026-10-02: width-short-zero span unit leading_zero -> financial_identifier (five-digit policy).
assert hashlib.sha256(corpus.read_bytes()).hexdigest()=='0e893b4512853d8997f704907e11383d8a6cabf6df65c9b21cf44beb7aa81c20'
rows=[json.loads(x) for x in corpus.read_text().splitlines()]
# These negatives are fixed before the unsupported fullwidth-punctuation guard.
for i,text in enumerate(['１２，５００원','₩１２，５００','$12，500','－１２３','１２．５０%',
                          '０９：０５','September 17, ２０２６．５']):
 rows.append(dict(id=f'wide-punctuation-{i}',text=text,expected=text,default_language='ko-KR' if i!=6 else 'en-US',output_mode='speech'))
# 2026-10-02: a trailing fullwidth percent sign reads like % (it cannot split an
# amount); １２３％ was the eighth guard row above (expected: kept as written).
rows.append(dict(id='wide-percent',text='１２３％',expected='백이십삼 퍼센트',default_language='ko-KR',output_mode='speech'))
checks=0;rng=random.Random(20260928)
for profile in ({},dict(first_target=1,normal_target=3,max_length=12)):
 engines={}
 try:
  for row in rows:
   options=dict(frontend_settings(row),**profile,default_language=row['default_language'],detailed_trace=True)
   key=json.dumps(options,sort_keys=True)
   if key not in engines:engines[key]=Engine(library,resources,options)
   e=engines[key];raw=row['text'].encode();batch=e.normalize(raw)
   assert batch['text']==row['expected'],(row['id'],batch)
   for span in batch['spans']:
    assert span['surface']==raw[span['input_begin']:span['input_end']].decode(),span
   schedules=[[p,len(raw)-p] for p in range(1,len(raw))]+[[1]*len(raw)]
   schedule=[];left=len(raw)
   while left:
    k=rng.randint(1,min(left,13));schedule.append(k);left-=k
   schedules.append(schedule)
   for schedule in schedules:
    assert flatten(e.stream(raw,schedule))==batch,(row['id'],schedule)
    checks+=1
 finally:
  for e in engines.values():e.close()

# All-width / mixed-width spelling must reuse the same semantics. This sweep is
# a metamorphic value/offset guard, not additional independent linguistic gold.
templates=['12,500원입니다.','2026. 9. 16. 기준입니다.','September 17, 2026',
           '3번째 21살 3명','대표 번호 02-123-4567 확인','Call +1 (212) 555-1212 ext. 9.',
           '09:05:03 확인','1~2kg 입니다.','001204','참조 번호는 001204입니다.',
           '1e3원','1억2천3백만원','v2.01.3 버전','192.168.0.1 주소',
           'k7 타고 다닌다','Please go to the 1st floor.']
for lang in ('ko-KR','en-US'):
 e=Engine(library,resources,dict(default_language=lang,output_mode='speech',first_target=1,normal_target=3,max_length=12,detailed_trace=True))
 try:
  for source in templates:
   ascii_result=e.normalize(source.encode())
   for mixed in (False,True):
    wide=''.join(chr(ord(c)+0xfee0) if '0'<=c<='9' and (not mixed or i%2) else c for i,c in enumerate(source));raw=wide.encode()
    result=e.normalize(raw)
    assert result['text']==ascii_result['text'],(lang,source,wide,ascii_result['text'],result['text'])
    values=lambda r:[(s['semantic_type'],s['value'],s['secondary'],s['unit']) for s in r['spans']]
    assert values(result)==values(ascii_result),(source,result,ascii_result)
    assert flatten(e.stream(raw,[1]*len(raw)))==result,(source,result)
 finally:e.close()

# 2026-10-02: a short digit-first label reads as number + letter names (２０F 이십 에프;
# it was kept as written).
# Earlier fullwidth fields must not shift subsequent control/code input offsets,
# or make the annotated ASCII-only value contracts accept wide digits.
# invalid_control=error keeps the strict failure under test (default: ignore).
e=Engine(library,resources,dict(input_format='annotated',output_mode='speech',detailed_trace=True,
                                invalid_control='error'))
try:
 for source,expected in [('１２３원 [[１２３]]{@preserve}','백이십삼 원 １２３'),
                         ('３개 `１２３원`','세 개 `１２３원`'),
                         ('３개 /tmp/１２３.txt','세 개 /tmp/１２３.txt'),
                         ('３개 [[note]]{@skip}끝','세 개 끝'),
                         ('２０F 타워','이십 에프 타워'), ('2０F 타워','이십 에프 타워')]:
  raw=source.encode();r=e.normalize(raw);assert r['text']==expected,r
  assert flatten(e.stream(raw,[1]*len(raw)))==r
 for source in ['３개 [[１２]]{@digits}','３개 [[서울시 １２]]{@address}']:
  try:e.normalize(source.encode());raise AssertionError('wide explicit value accepted')
  except ValueError:pass
 prefix='먼저 안내합니다.\n'.encode();source=prefix+'가격은 １２,５００원입니다.'.encode();events=[]
 out=e.stream(source,[len(prefix),14,1,1,2,len(source)],emit=events.append)
 assert flatten(out)['text']=='먼저 안내합니다.\n가격은 만 이천오백 원입니다.'
 first=next(x for x in events if x['event']=='segment');assert first['stage']=='write' and first['input_end']<=len(prefix),events
finally:e.close()
# Exact overrides remain keyed by original bytes, not the folded scanner view.
e=Engine(library,resources,dict(output_mode='speech',detailed_trace=True,
    customer_tsv='exact\tko-KR\t１２３원\t고객금액\nexact\tko-KR\t123원\t일반금액\n'))
try:
 for source,expected in [('１２３원','고객금액'),('123원','일반금액')]:
  raw=source.encode();r=e.normalize(raw);assert r['text']==expected,r
  assert flatten(e.stream(raw,[1]*len(raw)))==r
finally:e.close()
print(f'{checks} split/text/span/trace comparisons, 64 width-equivalence scenarios, preserved controls and early callbacks passed')
