"""Strict interpretation errors: no partial matches, invented units, or raw bypass."""
import json
from pathlib import Path
import subprocess
import sys

cli, resources = sys.argv[1:]
invalid = {
    'cardinal': ['12.5', '1e3', '12,34', '1234,567', '1,,234', '+', ' 1', '1 ', '１２', '1x'],
    'digits': ['1.2', '1 2', '-12', '12,500', '１２'],
    'decimal': ['12', '.5', '1.', '1.2.3', '1e-2'],
    'ordinal': ['0', '000', '-1', '1st', '1.5'],
    'year': ['0', '0000', '99', '10000', '2026년'],
    'date': ['2023-02-29', '1900-02-29', '2026-04-31', '2026-00-01', '0000-01-01',
             '2026-13-01', '2026-01-00', '26-01-01', '03/04/2026', '2026-01/01', '2026-01-01x'],
    'date-mdy': ['31/12/2026', '02/29/2023'],
    'date-dmy': ['12/31/2026', '31/04/2026'],
    'time': ['24:00', '09:60', '12', '09:05x', '09:05:60'],
    'currency': ['12', '$', '$12 dollars later', '12kg', '$12,34'],
    'percent': ['12', '12%%', '12% extra'],
    'measure': ['3', '3xyz', '3kg extra', '3개'],
    'counter': ['3', '3개입니다', '3kg'],
    'fraction': ['1/0', '1/00', '1/-2', '1/+2', '1/2/3', '1.5/2', '/2', '1/'],
    'range': ['1~', '~2', '1~2~3', '1~2kg extra'],
    'phone': ['+', '-01', '01-', '01--02', '01 - 02', '01a02', '(01)02', '01.02'],
    'version': ['1', 'v1', '.1', '1.', '1..2', '1.2beta', 'v1.2-rc1'],
    'ip': ['256.1.1.1', '1.2.3', '1.2.3.4.5', '1.2.3.', 'v1.2.3.4', '::1', '0000.1.1.1'],
    'missing': ['1'],
}
checks = 0
for pattern, sources in invalid.items():
    for source in sources:
        for language in ('ko-KR', 'en-US'):
            text = f'[[{source}]]{{@{pattern}}}'
            options = ['--resources', resources, '--language', language,
                       '--input-format', 'annotated', '--output-mode', 'speech']
            result = subprocess.run([cli, 'normalize', *options, text], capture_output=True,
                                    text=True, timeout=10)
            assert result.returncode and not result.stdout and 'invalid argument' in result.stderr, (text, language, result)
            result = subprocess.run([cli, 'stream-json', *options, '--chunk-bytes', '1', '-'],
                                    input=text, capture_output=True, text=True, timeout=10)
            events = [json.loads(line) for line in result.stdout.splitlines()]
            assert result.returncode and not any(e['event'] in ('segment', 'end') for e in events), (text, language, result)
            checks += 1

for pattern, value in [('cardinal', '1'*19), ('decimal', '1'*19+'.0'), ('ordinal', '1'*19),
                       ('digits', '1'*96), ('currency', '$'+'1'*19), ('measure','1'*19+'kg'),
                       ('fraction', '1/'+'1'*19), ('version', '1.'+'1'*19),
                       ('phone', '1'*96), ('cardinal', '1'*200)]:
    result = subprocess.run([cli, 'normalize', '--resources', resources, '--input-format', 'annotated',
                             f'[[{value}]]{{@{pattern}}}'], capture_output=True, text=True, timeout=10)
    assert result.returncode and not result.stdout and 'limit' in result.stderr, (pattern, result)
# The EN pack explicitly does not opt into KO counted-noun controls.
r = subprocess.run([cli,'normalize','--resources',resources,'--language','en-US',
                    '--input-format','annotated','[[3개]]{@counter}'], capture_output=True,text=True,timeout=10)
assert r.returncode and 'invalid argument' in r.stderr, r
print(f'{checks} invalid pattern/language combinations rejected in batch and byte streams; 10 size limits and EN counter refusal passed')
