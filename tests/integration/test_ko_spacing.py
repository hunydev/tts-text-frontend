"""Korean numeral group spacing: text, metadata, overlays and arbitrary byte splits."""
import json,random,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tools/web'),str(ROOT/'tools/evaluation')]
from native import Engine,flatten
from evaluate import frontend_settings
library,resources=(Path(x).resolve() for x in sys.argv[1:])
rows=[json.loads(s) for s in (ROOT/'tests/evaluation/ko_spacing_v22.jsonl').read_text().splitlines()]
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
# A plain 9+ digit run is an identifier in speech (long_digit_identifier_min, 2026-10-01);
# the amount grammar is asserted on the comma-grouped form. Was: 100010001.
overlays=[
 ('number_spacing\tko-KR\tgroup\tcompact\n','100,010,001','일억만일','CARDINAL'),
 ('number_spacing\tko-KR\tgroup\tspace\n','100,010,001','일억 만 일','CARDINAL'),
]
for tsv,text,expected,kind in overlays:
 e=Engine(library,resources,dict(output_mode='speech',customer_tsv=tsv,detailed_trace=True))
 try:
  batch=e.normalize(text.encode());assert batch['text']==expected,(text,batch)
  assert batch['spans'][0]['semantic_type']==kind,batch
  assert flatten(e.stream(text.encode(),[1]*len(text.encode())))==batch
 finally:e.close()
print(f'{len(rows)} references across two segment profiles; {checks} arbitrary byte splits; 2 resource overlays passed')
