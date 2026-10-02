"""User-authorized phone tails and leading-zero policy: IR, resources, byte streams."""
import json,random,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tools/web'),str(ROOT/'tools/evaluation')]
from native import Engine,flatten
from evaluate import frontend_settings
library,resources=(Path(x).resolve() for x in sys.argv[1:])
rows=[json.loads(s) for s in (ROOT/'tests/evaluation/phone_zero_v22.jsonl').read_text().splitlines()]
checks=0
for profile in ({},dict(first_target=1,normal_target=3,max_length=12)):
 engines={}
 try:
  for row in rows:
   opts=dict(frontend_settings(row),**profile,default_language=row['default_language'],detailed_trace=True)
   key=json.dumps(opts,sort_keys=True)
   if key not in engines:engines[key]=Engine(library,resources,opts)
   e=engines[key];raw=row['text'].encode();batch=e.normalize(raw)
   assert batch['text']==row['expected'],(row['id'],profile,batch)
   for expected in row.get('expected_spans',[]):
    matches=[s for s in batch['spans'] if s['surface']==expected['surface']]
    assert len(matches)==1 and all(matches[0][k]==v for k,v in expected.items()),(row['id'],profile,expected,matches)
   schedules=[[i,len(raw)-i] for i in range(1,len(raw))]+[[1]*len(raw)]
   rng=random.Random(20260928);remaining=len(raw);schedule=[]
   while remaining:
    n=rng.randint(1,min(remaining,17));schedule.append(n);remaining-=n
   for chunks in schedules+[schedule]:
    assert flatten(e.stream(raw,chunks))==batch,(row['id'],profile,chunks)
    checks+=1
 finally:
  for e in engines.values():e.close()
# Explicit overlays change policy; no whole-sentence exceptions or code rebuild.
overlays=[
 ('numeric_policy\t*\tleading_zero_min_digits\t7\n','0046','사십육','CARDINAL'),
 ('number\tko-KR\tleading_zero_zero\t영\n','0046','영영사육','DIGIT_SEQUENCE'),
 # 2026-10-02: 번 inside the word 번호 is no longer a counter after a phone, so the override
 # is checked with a standalone 번 (was: 015-2345-6789 번호 kept as written).
 # 2026-10-02: the rejected span is read as written (written fallback; was kept as written).
 ('phone_number_tail\tko-KR\t번\t0\n','015-2345-6789 번','공일오 다시 이삼사오 다시 육칠팔구 번','TEXT'),
 ('phone_render\tko-KR\tseparator\t 다시 \n','02-2345-6789 번호','공이 다시 이삼사오 다시 육칠팔구 번호','PHONE'),
]
for tsv,text,expected,kind in overlays:
 e=Engine(library,resources,dict(output_mode='speech',customer_tsv=tsv,detailed_trace=True))
 try:
  batch=e.normalize(text.encode());assert batch['text']==expected,(text,batch)
  assert batch['spans'][0]['semantic_type']==kind,batch
  assert flatten(e.stream(text.encode(),[1]*len(text.encode())))==batch
 finally:e.close()
print(f'{len(rows)} references across two segment profiles; {checks} arbitrary byte splits; 4 resource overlays passed')
