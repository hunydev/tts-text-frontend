"""Break controls: malformed syntax, exact limits and the bound atom."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
cli, resources = sys.argv[1:]
options=['--resources',resources,'--input-format','annotated','--output-mode','speech']
# A break control needs a preceding atom or [[group]] and strength 1, 2 or 3 exactly.
invalid=['{@break:1}', '앞 {@break:1}', '1{@break:0}', '1{@break:4}', '1{@break:01}',
         '1{@break:-1}', '1{@break:1.0}', '1{@break:}', '1{@break: 1}', '1{@Break:1}',
         '1{@break:1', '1{@break:', '1{@break:1}}', '1{@break}', '1{@break:1x}', '1{@break:가}',
         '[[ ]]{@break:1}', '[[--]]{@break:2}', '{@native}', '1{@native:1}']
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
# The byte limit covers the bound atom and the marker, like every attached control.
source='1234{@break:2}'
for limit in (len(source)-1,len(source)):
 for mode in ('normalize','stream-json'):
  extra=['--chunk-bytes','1','-'] if mode=='stream-json' else [source]
  r=subprocess.run([cli,mode,*options,'--max-control-bytes',str(limit),*extra],
                   input=source if mode=='stream-json' else None,capture_output=True,text=True,timeout=10)
  assert (r.returncode==0)==(limit==len(source)),(limit,mode,r)
  if limit<len(source):assert 'limit' in r.stderr,r
  elif mode=='normalize':assert r.stdout.rstrip('\n')=='일, 이, 삼, 사',r
# Only the atom right before the marker is read one character at a time.
for source,expected in [('12{@break:1}34','일 이삼십사'),('[[A-1]]{@break:3}번','에이. 일.번'),
                        ('[[앞]]{앞\\{@break:1\\}}','앞{@break:1}')]:
 r=subprocess.run([cli,'normalize',*options,source],capture_output=True,text=True,timeout=10)
 assert r.returncode==0 and r.stdout.rstrip('\n')==expected,(source,r)
with tempfile.TemporaryDirectory() as directory:
 path=Path(directory)/'bad.tsv'
 for kind in ('counter_noun_unit','counter_noun_tail','counter_noun_attached'):
  path.write_text(f'{kind}\tko-KR\t세\tmaybe\n')
  r=subprocess.run([cli,'normalize',*options,'--customer',str(path),'15세'],capture_output=True,text=True,timeout=10)
  assert r.returncode and 'resource' in r.stderr,r
print(f'{len(invalid)} malformed controls in batch/byte streams; exact limits, bound atoms and 3 invalid resources passed')
