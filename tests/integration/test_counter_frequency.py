"""Count cues: fixed references, byte streams, identifier guards and overrides."""
import json
import random
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT/'tools/web'), str(ROOT/'tools/evaluation')]
from native import Engine, flatten
from evaluate import frontend_settings

library, resources = (Path(p).resolve() for p in sys.argv[1:])
rows = [json.loads(s) for s in (ROOT/'tests/evaluation/counter_frequency_v22.jsonl').read_text().splitlines()]
checks = 0
for profile in ({}, dict(first_target=1, normal_target=3, max_length=12)):
    engines = {}
    try:
        for row in rows:
            options = dict(frontend_settings(row), **profile, default_language='ko-KR', detailed_trace=True)
            key = json.dumps(options, sort_keys=True)
            if key not in engines:
                engines[key] = Engine(library, resources, options)
            engine = engines[key]
            raw = row['text'].encode()
            batch = engine.normalize(raw)
            assert batch['text'] == row['expected'], (row['id'], batch)
            for expected in row.get('expected_spans', []):
                selected = [s for s in batch['spans'] if s['surface'] == expected['surface']]
                assert len(selected) == 1, (row['id'], selected)
                assert all(selected[0][k] == v for k, v in expected.items()), (row['id'], expected, selected)
            schedules = [[i, len(raw)-i] for i in range(1, len(raw))] + [[1]*len(raw)]
            rng = random.Random(20260928)
            remaining, schedule = len(raw), []
            while remaining:
                width = rng.randint(1, min(remaining, 13))
                schedule.append(width)
                remaining -= width
            schedules.append(schedule)
            for schedule in schedules:
                assert flatten(engine.stream(raw, schedule)) == batch, (row['id'], schedule)
                checks += 1
    finally:
        for engine in engines.values():
            engine.close()

# Effective-key overrides and longest right-word match remain independent.
for overlay, source, expected, role, layer in [
    ('counter_role_after\tko-KR\t연습\t0\n', '3번 연습합니다.', '삼 번 연습합니다.', '', 'system'),
    ('counter_role_after\tko-KR\t연습\tidentifier\n', '3번 연습합니다.', '삼 번 연습합니다.', 'identifier', 'customer'),
    ('counter_role_before\tko-KR\t하루에\tidentifier\n', '하루에 3번 확인합니다.', '하루에 삼 번 확인합니다.', 'identifier', 'customer'),
    ('counter_role_after\tko-KR\t연\tfrequency\n', '3번 연습문제입니다.', '삼 번 연습문제입니다.', 'identifier', 'system'),
    ('counter_role_tail\tko-KR\t씩\t0\n', '3번씩 확인합니다.', '삼 번씩 확인합니다.', '', 'system'),
]:
    engine = Engine(library, resources, dict(output_mode='speech', detailed_trace=True,
                    customer_tsv=overlay, first_target=1, normal_target=3, max_length=12))
    try:
        raw = source.encode(); result = engine.normalize(raw)
        assert result['text'] == expected, result
        span = next(s for s in result['spans'] if s['surface'] == '3번')
        assert (span['secondary'], span['resource_layer']) == (role, layer), span
        assert flatten(engine.stream(raw, [1]*len(raw))) == result
    finally:
        engine.close()

engine = Engine(library, resources, dict(output_mode='speech'))
try:
    prefix = '먼저 안내합니다.\n'.encode()
    source = prefix + '12번이나 연습합니다.'.encode()
    events = []
    result = flatten(engine.stream(source, [len(prefix)] + [1]*(len(source)-len(prefix)), emit=events.append))
    assert result['text'] == '먼저 안내합니다.\n열두 번이나 연습합니다.', result
    first = next(e for e in events if e['event'] == 'segment')
    assert first['stage'] == 'write' and first['input_end'] <= len(prefix), events
finally:
    engine.close()
print(f'{len(rows)} references; {checks} text/span/trace byte splits; overrides, identifier precedence and early callback passed')
