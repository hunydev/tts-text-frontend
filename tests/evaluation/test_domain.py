"""Strict finance/content scenario acceptance, with readable failure evidence."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

cli, resources = sys.argv[1:]
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="ttf-domain-") as directory:
    output = Path(directory) / "evaluation"
    command = [sys.executable, str(root / "tools/evaluation/evaluate.py"), "--ttf", cli,
               "--resources", resources, "--corpus", str(root / "tests/evaluation/domain_cases.jsonl"),
               "--output-dir", str(output)]
    process = subprocess.run(command, capture_output=True, text=True, timeout=90)
    if process.returncode:
        report = output / "report.json"
        if report.exists():
            cases = json.loads(report.read_text(encoding="utf-8"))["cases"]
            for case in cases:
                if not case["passed"]:
                    print(json.dumps({field: case[field] for field in
                          ("id", "text", "expected", "normalized", "metadata_failures",
                           "batch_stream_match")}, ensure_ascii=False), file=sys.stderr)
        raise SystemExit(process.stderr or process.stdout or "domain evaluation failed")
    summary = json.loads(process.stdout)
    assert summary["cases"] == 60 and summary["failed"] == 0, summary
    print(process.stdout.strip())
