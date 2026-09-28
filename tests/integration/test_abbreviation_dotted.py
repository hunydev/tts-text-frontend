"""Fixed dotted-abbreviation references, IR/trace, aliases and byte streaming."""
import hashlib
import json
from pathlib import Path
import random
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/web'))
from native import Engine, flatten

library, resources = map(lambda s: Path(s).resolve(), sys.argv[1:])
corpus = ROOT / 'tests/evaluation/abbreviation_dotted_v21.jsonl'
assert hashlib.sha256(corpus.read_bytes()).hexdigest() == 'b7c1d5a34ec18816b996cdb0d189cd1fb2344cf27878cd1026cc029cca521382'
rows = [json.loads(line) for line in corpus.read_text().splitlines()]
checks = 0
rng = random.Random(210927)
for profile in ({}, dict(first_target=1, normal_target=3, max_length=12)):
    for mode in ('text', 'speech'):
        engines = {}
        try:
            for row in rows:
                if mode == 'speech' and row['category'].startswith('guard/'):
                    continue  # Guard expectations are explicitly text-mode contracts.
                lang = row['default_language']
                if lang not in engines:
                    engines[lang] = Engine(library, resources, dict(profile,
                        default_language=lang, output_mode=mode, detailed_trace=True))
                e = engines[lang]
                raw = row['text'].encode()
                batch = e.normalize(raw)
                assert batch['text'] == row['expected'], (row['id'], profile, mode, batch)
                schedules = [[cut, len(raw)-cut] for cut in range(1,len(raw))]
                schedules.append([1]*len(raw))
                sizes, remaining = [], len(raw)
                while remaining:
                    width = rng.randint(1,min(remaining,7));sizes.append(width);remaining-=width
                schedules.append(sizes)
                for sizes in schedules:
                    assert flatten(e.stream(raw,sizes)) == batch, (row['id'], profile, mode,sizes)
                    checks += 1
        finally:
            for e in engines.values(): e.close()

def check(text, expected, **options):
    e = Engine(library,resources,dict(default_language='en-US',output_mode='text',
                                    detailed_trace=True,**options))
    try:
        raw=text.encode();r=e.normalize(raw)
        assert r['text']==expected,(text,r)
        assert flatten(e.stream(raw,[1]*len(raw)))==r
        return r
    finally:e.close()

r=check('Read Vol. 3.','Read volume three.')
s,=[s for s in r['spans'] if s['rule']=='en.abbreviation.dotted']
assert (s['surface'],s['semantic_type'],s['value'],s['secondary'],s['input_begin'],s['input_end'],
        s['source_lang'],s['context_lang'],s['render_lang']) == (
        'Vol.','ABBREVIATION','vol','dotted',5,9,'en-US','en-US','en-US'),s
assert any(e['kind']=='abbreviation_dotted' and e['key']=='vol' and e['value']=='volume'
           and e['layer']=='system' and e['disposition']=='selected' for e in r['trace'])
r=check('Acme Ltd.','Acme limited.')
s,=[s for s in r['spans'] if s['rule']=='en.abbreviation.dotted']
assert s['surface']=='Ltd' and r['spans'][-1]['surface']=='.',r

r=check('Acme Ltd.','Acme custom.', customer_tsv='abbreviation_dotted\ten-US\tltd\tcustom\n')
assert any(e['kind']=='abbreviation_dotted' and e['value']=='custom' and e['layer']=='customer'
           for e in r['trace']),r
check('Read Vol. 3.','Read Vol. three.',customer_tsv='abbreviation_dotted\ten-US\tvol\tpreserve\n')
check('Alpha vs. Beta','Alpha versus. Beta',
      customer_tsv='abbreviation_dotted_period\ten-US\tvs\tsentence\n')
check('Acme ltd. Reports','Acme limited Reports',
      customer_tsv='abbreviation_dotted_period\ten-US\tltd\tcontinuation\n')
check('Acme inc. paid 3 dollars.','Acme incorporated paid three dollars.',
      customer_tsv='abbreviation_dotted\ten-US\tinc\tincorporated\nabbreviation\ten-US\tinc\tpreserve\n')
# Identifier boundaries: these are not free-standing dotted abbreviations.
for text in ('Vol.3','ltd.example.com','x-vol.','vol._data','vol./file','vol.@example.com',
             'Vol.é', 'ltd.Équipe'):
    e=Engine(library,resources,dict(default_language='en-US',output_mode='text'))
    try:
        r=e.normalize(text.encode())
        assert not any(s['rule']=='en.abbreviation.dotted' for s in r['spans']),(text,r)
    finally:e.close()

# A configured Korean pronunciation is deliberate customer data, not guessed gold.
# English semantic expansion still goes through the existing layered alias lookup.
for user,expected in (([], '표기는 리미티드입니다.'),
                       ([dict(word='Ltd.',pronunciation='고객명')], '표기는 고객명입니다.')):
    e=Engine(library,resources,dict(default_language='ko-KR',output_mode='speech',detailed_trace=True,
        customer_tsv='lexicon\tko-KR\ten-US:limited\t리미티드\n',user_lexicon=user))
    try:
        raw='표기는 Ltd.입니다.'.encode();r=e.normalize(raw)
        assert r['text']==expected,r
        s,=[s for s in r['spans'] if s['rule']=='en.abbreviation.dotted']
        assert (s['source_lang'],s['context_lang'],s['render_lang'])==('en-US','ko-KR','ko-KR'),s
        assert s['pronunciation_layer']==('user' if user else 'customer'),s
        assert flatten(e.stream(raw,[1]*len(raw)))==r
    finally:e.close()
print(f'{checks} byte-split/schedule text+IR+trace comparisons; period, resource and KO alias contracts passed')
