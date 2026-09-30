"""Independent range meanings; exact values and arbitrary byte chunk invariance."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

cli, resources = sys.argv[1:]
REFERENCES = (
    ('1~2만원', '일에서 이만 원', '10000', '20000', 'KRW'),
    ('1~2만 원', '일에서 이만 원', '10000', '20000', 'KRW'),
    ('1-2만원', '일에서 이만 원', '10000', '20000', 'KRW'),
    ('1.5~2.5만원', '만 오천에서 이만 오천 원', '15000', '25000', 'KRW'),
    ('1~2천만원', '일에서 이천만 원', '10000000', '20000000', 'KRW'),
    ('-1~-0.5만 원', '마이너스 만 원에서 마이너스 오천 원', '-10000', '-5000', 'KRW'),
    ('1~2억 원', '일에서 이억 원', '100000000', '200000000', 'KRW'),
    ('1~2만개', '일에서 이만 개', '10000', '20000', '개'),
    ('1개~2만개', '한 개에서 이만 개', '1', '20000', '개'),
    ('1만개~2만개', '만 개에서 이만 개', '10000', '20000', '개'),
    ('1만~2만', '만에서 이만', '10000', '20000', ''),
    ('1~2만 달러', '일에서 이만 달러', '10000', '20000', 'USD'),
    ('1~2만 유로', '일에서 이만 유로', '10000', '20000', 'EUR'),
    ('1~2만 엔', '일에서 이만 엔', '10000', '20000', 'JPY'),
    ('1~2e3만원', '1~2e3만원', '', '', ''),
    ('1~2e3만 원', '1~2e3만 원', '', '', ''),
    ('1~2만원~', '1~2만원~', '', '', ''),
    ('1~2만', '만에서 이만', '10000', '20000', ''),
    ('1~2억3천만원', '1~2억3천만원', '', '', ''),
    ('1만원~$2', '1만원~$2', '', '', ''),
)
checks = 0
for text, expected, first, second, unit in REFERENCES:
    for profile in ([], ['--first-target','1','--normal-target','3','--max-length','12']):
        batch = subprocess.run([cli,'normalize','--resources',resources,*profile,text], capture_output=True, text=True, check=True).stdout.rstrip('\n')
        assert batch == expected, (text, batch, expected)
        for chunk in range(1, len(text.encode()) + 1):
            p = subprocess.run([cli,'stream-json','--resources',resources,*profile,'--chunk-bytes',str(chunk),'-'],input=text.encode(),capture_output=True,check=True)
            events = [json.loads(x) for x in p.stdout.splitlines()]
            segments = [x for x in events if x['event']=='segment']
            assert ''.join(x['text'] for x in segments) == expected, (text,chunk,segments)
            spans = [s for x in segments for s in x['spans']]
            assert len(spans)==1, (text,spans)
            s=spans[0]
            assert (s['value'],s['secondary'],s['unit'])==(first,second,unit), (text,s)
            assert s['semantic_type']==('RANGE' if first else 'TEXT'), (text,s)
            checks+=1
# A suffix/connector override changes wording without changing either value.
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'customer.tsv'
    path.write_text('policy\tko-KR\tquantity_range_separator\t부터 \npolicy\tko-KR\tquantity_range_suffix\t까지\n')
    p=subprocess.run([cli,'normalize','--resources',resources,'--customer',str(path),'1~2만원'],capture_output=True,text=True,check=True)
    assert p.stdout.strip()=='일부터 이만 원까지',p.stdout
    checks+=1
print(f'{checks} shared-scale range text/value/byte-chunk and resource checks')

# Range grammar must not consume non-horizontal whitespace between endpoints.
for separator in ('\r', '\n', '\v', '\f', '\r\n'):
    text='1~'+separator+'2만원'
    run=subprocess.run([cli,'stream-json','--resources',resources,'--chunk-bytes','1','-'],input=text.encode(),capture_output=True,check=True)
    events=[json.loads(line) for line in run.stdout.splitlines()]
    output=''.join(e['text'] for e in events if e['event']=='segment')
    assert separator in output,(repr(text),repr(output))
    assert all(s['semantic_type']!='RANGE' for e in events if e['event']=='segment' for s in e['spans']),(repr(text),events)
print('5 non-horizontal whitespace guards passed')
