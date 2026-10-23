"""Explicit semantics, unchanged resource policies, and arbitrary byte boundaries."""
import hashlib
import json
from pathlib import Path
import random
import sys
ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT/'tools/web'), str(ROOT/'tools/evaluation')]
from native import Engine, flatten
from evaluate import frontend_settings
library, resources = [Path(x).resolve() for x in sys.argv[1:]]
corpus = ROOT/'tests/evaluation/typed_controls_v22.jsonl'
# 2026-09-30 (v0.24): range rows follow the range-reading decision.
# 2026-10-02: {@digits} reads 0 as 공 (typed-digits-ko, typed-atom).
# 2026-10-23: an IP address reads 0 as 공 (typed-ip-ko; project-owner decision).
assert hashlib.sha256(corpus.read_bytes()).hexdigest() == '321b03939290f8ec1e2ff52660f1ed5c3f1b79aa8e14b7af76f305edeb6a1eaf'
rows = [json.loads(x) for x in corpus.read_text().splitlines()]
rng = random.Random(20260928)
checks = 0
for profile in ({}, dict(first_target=1, normal_target=3, max_length=12)):
    engines = {}
    try:
        for row in rows:
            options = dict(frontend_settings(row), **profile, default_language=row['default_language'], detailed_trace=True)
            key = json.dumps(options, sort_keys=True)
            if key not in engines:
                engines[key] = Engine(library,resources,options)
            engine = engines[key]
            raw = row['text'].encode()
            batch = engine.normalize(raw)
            assert batch['text'] == row['expected'], (row['id'],batch)
            if row['category'] != 'controls/composition':
                span, = batch['spans']
                pattern = row['category'].split('/')[1]
                assert span['semantic_type'] == 'CUSTOM' and span['unit'] == '@'+pattern and span['rule'] == 'control.pattern', span
                assert (span['input_begin'],span['input_end']) == (0,len(raw)), span
                if pattern not in ('preserve','skip'):
                    event, = [e for e in batch['trace'] if e['kind'] == 'control-semantic']
                    expected_type = {'decimal':'CARDINAL','digits':'DIGIT_SEQUENCE','year':'DATE',
                                     'date-mdy':'DATE','date-dmy':'DATE','ip':'IP_ADDRESS'}.get(pattern,pattern.upper())
                    assert event['value'] == expected_type and event['key'] == '@'+pattern, event
                    assert 'secondary=' in event['detail'] and 'unit=' in event['detail'], event
            schedules = [[cut,len(raw)-cut] for cut in range(1,len(raw))] + [[1]*len(raw)]
            random_schedule, left = [],len(raw)
            while left:
                step = rng.randint(1,min(left,13));random_schedule.append(step);left -= step
            schedules.append(random_schedule)
            for schedule in schedules:
                assert flatten(engine.stream(raw,schedule)) == batch, (row['id'],schedule)
                checks += 1
    finally:
        for engine in engines.values(): engine.close()

def check(source,expected,**opts):
    e = Engine(library,resources,dict(input_format='annotated',output_mode='speech',detailed_trace=True,**opts))
    try:
        raw = source.encode(); batch = e.normalize(raw)
        assert batch['text'] == expected, batch
        assert flatten(e.stream(raw,[1]*len(raw))) == batch
        return batch
    finally: e.close()

# Customer rows are still authoritative; explicit control only picks meaning.
check('[[01-02]]{@phone}','영일 영이',customer_tsv='phone_render\tko-KR\tzero\t영\n')
check('[[1.2.3.4]]{@ip}','one point two point three point four',default_language='en-US',
      customer_tsv='electronic_separator\ten-US\tip\tpoint\n')
check('[[1~2kg]]{@range}','일부터 이 킬로그램',
      customer_tsv='policy\tko-KR\tquantity_range_separator\t부터 \n')
check('[[Apple 123 漢🙂]]{@preserve}','Apple 123 漢🙂',unreadable='drop')
check('[[1\\{2\\}]]{@preserve}','1{2}')
check('[[3개]]{@counter}','세 개',brackets='skip')
# Plain mode and protected code do not execute directives.
plain = Engine(library,resources,dict(input_format='plain',output_mode='speech'))
try:
    result = plain.normalize(b'[[09]]{@digits}')
    assert not any(s['rule'].startswith('control.') for s in result['spans']), result
finally: plain.close()
check('`[[09]]{@digits}`','`[[09]]{@digits}`')
long = '漢🙂'*100
check('[['+long+']]{@preserve}',long)
check('가[['+long+']]{@skip}나','가나')
check('[[2000-02-29]]{@date}','이천 년 이 월 이십구 일')
check('[[255.000.0.1]]{@ip}','이오오 점 공공공 점 공 점 일')  # 2026-10-23: IP zero 공 (was 영)
check('[[v2.1.3]]{@version}','브이 이 점 일 점 삼')
check('[[12,500.50]]{@decimal}','만 이천오백 점 오영')
# {@digits} reads 0 as 공 since 2026-10-02 (number digits_zero; was 영).
check('[['+'0'*95+']]{@digits}','공'*95)
check('[[123456789012345678]]{@digits}','일이삼사오육칠팔구공일이삼사오육칠팔')
# Semantic forcing must differ from automatic interpretation in these cases.
# invalid_control=error keeps the strict failure under test (default: ignore).
e = Engine(library,resources,dict(input_format='annotated',output_mode='speech',invalid_control='error'))
try:
    assert e.normalize(b'9/25')['text'] != e.normalize(b'[[9/25]]{@fraction}')['text']
    prefix='먼저 안내합니다.\n'.encode();raw=prefix+b'[[9/25]]{@fraction}'
    events=[];result=e.stream(raw,[len(prefix),2,3,5,len(raw)],emit=events.append)
    assert flatten(result)['text']=='먼저 안내합니다.\n이십오 분의 구'
    segments=[x for x in events if x['event']=='segment']
    assert segments[0]['stage']=='write' and segments[0]['input_end']<=len(prefix),segments
    events=[]
    try:
        e.stream(prefix+b'[[2023-02-29]]{@date}',[len(prefix),1,100],emit=events.append)
        raise AssertionError('invalid control accepted')
    except ValueError as error:
        assert 'invalid argument' in str(error), error
    assert ''.join(x['text'] for x in events if x['event']=='segment')==prefix.decode(),events
finally: e.close()
print(f'{checks} arbitrary byte split/text/span/trace comparisons; resource overrides, long controls, semantic forcing and callbacks passed')
