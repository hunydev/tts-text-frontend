"""Use real CLI output to verify reference immutability and harness exit gates."""
import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/quality/tn_cycle.py"
spec = importlib.util.spec_from_file_location("cycle", TOOL)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def call(*args, code=0):
    p = subprocess.run([sys.executable, str(TOOL), *map(str, args)], capture_output=True, timeout=60)
    assert p.returncode == code, (p.args, p.returncode, p.stdout, p.stderr)


def main():
    cli, resources = map(lambda s: Path(s).resolve(), sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="tn-cycle-test-") as directory:
        tmp = Path(directory)
        corpus = tmp / "case.jsonl"
        intake = ["case", "--id", "customer-apple", "--category", "foreign/brand",
                  "--text", "Apple은", "--expected", "검증은", "--source", "synthetic-contract",
                  "--reason", "A temporary exact rule must improve this authored reference", "--out", corpus]
        call(*intake)
        original = corpus.read_bytes()
        call(*intake, code=2)
        assert corpus.read_bytes() == original
        for name in ("before", "after"):
            target = tmp / name
            target.mkdir()
            shutil.copy2(cli, target / "ttf")
            shutil.copytree(resources, target / "resources")
            if name == "after":
                (target / "resources/quality-test.tsv").write_text("exact\tko-KR\tApple\t검증\n")
                with (target / "resources/manifest.ttfm").open("a") as f:
                    f.write("\nquality-test.tsv\n")
            module.write_json(target / "snapshot.json", dict(schema_version=1, executable="ttf", files=module.inventory(target)))
        args = ["cycle", "--baseline", tmp / "before", "--candidate", tmp / "after", "--corpus", corpus]
        call(*args, "--out", tmp / "fixed")
        report = json.loads((tmp / "fixed/cycle.json").read_text())
        assert report["comparison"]["fixed"] == 1 and report["candidate_failed"] == 0
        call("cycle", "--baseline", tmp / "after", "--candidate", tmp / "before", "--corpus", corpus,
             "--out", tmp / "regressed", code=1)
        report = json.loads((tmp / "regressed/cycle.json").read_text())
        assert report["comparison"]["regressed"] == 1 and report["trace_count"] == 1
        assert (tmp / "regressed/failed-traces/customer-apple.txt").stat().st_size > 0
        call(*args, "--out", tmp / "before/nested", code=2)
        (tmp / "after/resources/quality-test.tsv").write_text("modified capture")
        call(*args, "--out", tmp / "tampered", code=2)
        assert not (tmp / "tampered").exists() and corpus.read_bytes() == original
    print("TN cycle: fixed/regressed gates, traces, duplicate reference and snapshot integrity passed")


if __name__ == "__main__":
    main()
