"""Import -> actual native batch -> reference span agreement on original fixtures."""
import json
from pathlib import Path
import sys
import tempfile
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT))
from test_korean_release import fixture,row
from benchmark.importers.korean_release.import_release import run as import_run
from benchmark.evaluators.korean_release import run as evaluate
library,resources=(Path(x).resolve() for x in sys.argv[1:])
with tempfile.TemporaryDirectory() as d:
 root=Path(d)
 fixture(root/'source',[row('3개','[세 개]','3개','세 개'),row('１２３원','[백이십삼 원]','１２３원','백이십삼 원')])
 import_run(root/'source',root/'import')
 result=evaluate(root/'import',library,resources,root/'report')
 assert result['sentences']['exact']==2 and result['spans']['exact']==2,result
 assert result['sentences']['errors']==0
 assert (root/'report/harness/benchmark/evaluators/korean_release.py').is_file()
 print('Korean release native: 2/2 sentence and span agreements; provenance snapshot passed')
