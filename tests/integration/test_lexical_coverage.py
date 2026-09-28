"""Authored lexical readings, byte streams and user/customer precedence."""
import json,random,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
sys.path[:0]=[str(ROOT/'tools/web'),str(ROOT/'tools/evaluation')]
from native import Engine,flatten
from evaluate import frontend_settings
library,resources=(Path(x).resolve() for x in sys.argv[1:])
rows=[json.loads(s) for s in (ROOT/'tests/evaluation/lexical_coverage_v22.jsonl').read_text().splitlines()]
checks=0
for profile in ({},dict(first_target=1,normal_target=3,max_length=12)):
    engines={}
    try:
        for row in rows:
            opts=dict(frontend_settings(row),**profile,default_language=row['default_language'],detailed_trace=True)
            overlays=[(resources/r['path'],3) for r in row.get('resource_overrides',[])]
            key=(json.dumps(opts,sort_keys=True),tuple(overlays))
            if key not in engines:engines[key]=Engine(library,resources,opts,overlays)
            engine=engines[key];raw=row['text'].encode();batch=engine.normalize(raw)
            # A tiny phrase target can change the existing context window.
            # Gold applies to default segmentation; each profile must preserve chunk invariance.
            if not profile:assert batch['text']==row['expected'],(row['id'],batch)
            for expected in ([] if profile else row.get('expected_spans',[])):
                matches=[s for s in batch['spans'] if s['surface']==expected['surface']]
                assert len(matches)==1 and all(matches[0][k]==v for k,v in expected.items()),(row['id'],expected,matches)
            schedules=[[i,len(raw)-i] for i in range(1,len(raw))]+[[1]*len(raw)]
            remaining=len(raw);schedule=[];rng=random.Random(20260928)
            while remaining:
                n=rng.randint(1,min(remaining,13));schedule.append(n);remaining-=n
            for chunks in schedules+[schedule]:
                assert flatten(engine.stream(raw,chunks))==batch,(row['id'],chunks)
                checks+=1
    finally:
        for engine in engines.values():engine.close()
# Three layers conflict; user wins without touching the resource directory.
options=dict(output_mode='speech',customer_tsv='lexicon\tko-KR\ten-US:pin\t고객핀\n',
             user_lexicon=[dict(word='PIN',pronunciation='사용자핀')],detailed_trace=True)
e=Engine(library,resources,options)
try:
    r=e.normalize('PIN의 안내입니다.'.encode());assert r['text']=='사용자핀의 안내입니다.',r
    s=next(s for s in r['spans'] if s['surface']=='PIN');assert s['pronunciation_layer']=='user',s
finally:e.close()
# An English SPEECH acronym can be spelled; Korean lexicon must not localize it.
e=Engine(library,resources,dict(output_mode='speech',default_language='en-US'))
try:
    r=e.normalize(b'Please use PIN and PASS.')
    assert all(s['render_lang']!='ko-KR' for s in r['spans']),r
finally:e.close()
print(f'{len(rows)} references; {checks} byte splits; English context and user/customer/system precedence passed')
