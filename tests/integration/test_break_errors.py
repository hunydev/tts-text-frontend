"""Position controls: malformed syntax, exact limits and no accidental binding."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
cli, resources = sys.argv[1:]
options=['--resources',resources,'--input-format','annotated','--output-mode','speech']
invalid=['{@break:0}','{@break:4}','{@break:01}','{@break:-1}','{@break:1.0}',
         '{@break:}', '{@break: 1}', '{@Break:1}', '{@break:1', '{@break:',
         '{@break:1}}', '앞{@break:9}뒤', '[[앞]]{@break:1}',
         '{@native}', '1{@native:1}', '앞{@break}', '{@break:1x}', '{@break:가}']
for source in invalid:
 for mode in ('normalize','stream-json'):
  extra=['--chunk-bytes','1','-'] if mode=='stream-json' else [source]
  result=subprocess.run([cli,mode,*options,*extra],input=source if mode=='stream-json' else None,
                        capture_output=True,text=True,timeout=10)
  assert result.returncode and 'invalid argument' in result.stderr,(source,mode,result)
  if mode=='normalize':assert not result.stdout,result
  else:
   events=[json.loads(s) for s in result.stdout.splitlines()]
   assert not any(e['event'] in ('segment','end') for e in events),(source,events)
for limit in (9,10):
 # The long preceding atom is not part of a position directive's byte limit.
 source='가'*100+'{@break:2}나'
 for mode in ('normalize','stream-json'):
  extra=['--chunk-bytes','1','-'] if mode=='stream-json' else [source]
  r=subprocess.run([cli,mode,*options,'--max-control-bytes',str(limit),*extra],
                   input=source if mode=='stream-json' else None,capture_output=True,text=True,timeout=10)
  assert (r.returncode==0)==(limit==10),(limit,mode,r)
  if limit==9:assert 'limit' in r.stderr,r
  elif mode=='normalize':assert r.stdout.rstrip('\n')=='가'*100+', 나',r
# Language independent, and adjacent directives insert exactly what was requested.
for source,expected in [('앞{@break:1}{@break:2}{@break:3}뒤','앞 , . 뒤'),
 ('[[앞]]{앞\\{@break:1\\}}','앞{@break:1}')]:
 r=subprocess.run([cli,'normalize',*options,source],capture_output=True,text=True,timeout=10)
 assert r.returncode==0 and r.stdout.rstrip('\n')==expected,r
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory)/'bad.tsv'
 for kind in ('counter_noun_unit','counter_noun_tail','counter_noun_attached'):
  path.write_text(f'{kind}\tko-KR\t세\tmaybe\n')
  r=subprocess.run([cli,'normalize',*options,'--customer',str(path),'15세'],capture_output=True,text=True,timeout=10)
  assert r.returncode and 'resource' in r.stderr,r
print(f'{len(invalid)} malformed controls in batch/byte streams; exact marker limits, adjacent/literal controls and 3 invalid resources passed')
