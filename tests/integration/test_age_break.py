"""Counted nouns and break controls: exact text/IR across arbitrary byte splits."""
import json,random,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tools/web'),str(ROOT/'tools/evaluation')]
from native import Engine,flatten
from evaluate import frontend_settings
library,resources=(Path(x).resolve() for x in sys.argv[1:])
rows=[json.loads(s) for name in ('counted_noun_v22','break_controls_v23') for s in (ROOT/f'tests/evaluation/{name}.jsonl').read_text().splitlines()]
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

print(f'{len(rows)} references across two segment profiles; {checks} arbitrary byte schedules passed')
# Exact unit toggles and reading tables keep Customer precedence.
for overlay,expected in [
 ('counter_noun_unit\tko-KR\t세\t0\n','십오세인 학생'),
 ('counter\tko-KR\t세\tnative\n','열다섯 세인 학생'),
]:
 e=Engine(library,resources,dict(customer_tsv=overlay,detailed_trace=True))
 try:
  raw='15세인 학생'.encode();batch=e.normalize(raw)
  assert batch['text']==expected,batch
  assert flatten(e.stream(raw,[1]*len(raw)))==batch
 finally:e.close()
# A break control is one span over its atom and marker, even when the marker
# arrives a byte at a time with automatic splitting off.
e=Engine(library,resources,dict(input_format='annotated',sentence_split=False,phrase_split=False,
                                output_mode='speech',default_language='ko-KR'))
try:
 raw='번호 1234{@break:3} 끝'.encode()
 batch=e.normalize(raw)
 assert batch['text']=='번호 일. 이. 삼. 사. 끝',batch
 control=[s for s in batch['spans'] if s['unit']=='@break:3']
 assert len(control)==1 and control[0]['surface']=='1234{@break:3}',control
 assert flatten(e.stream(raw,[1]*len(raw)))==batch
finally:e.close()

# Plain text and protected code do not interpret break markers.
for opts,text in [({'input_format':'plain'},'1234{@break:1}'),
                  ({'input_format':'annotated'},'`1234{@break:1}`')]:
 e=Engine(library,resources,opts)
 try:
  result=e.normalize(text.encode())
  assert not any(s['unit'].startswith('@break') for s in result['spans']),result
 finally:e.close()
