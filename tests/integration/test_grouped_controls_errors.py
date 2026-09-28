"""Strict control contracts through the CLI, also under ASan/UBSan."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

cli, resources = sys.argv[1:]
base = [cli, "normalize", "--resources", resources, "--input-format", "annotated",
        "--output-mode", "speech"]
invalid = [
    "[[a1@b2]]{@missing}", "[[ ]]{{x}}", "[[a1@b2]]{@Email}",
    "[[a1@b2]]{@email", "[[a1@b2] {@email}", "[[[[a1]]]]{@email}",
    "[[a1 b2]]{@email}", "[[a1]]{@email}", "[[@b2]]{@email}",
    "[[a1@]]{@email}", "[[a1@@b2]]{@email}", "[[.a1@b2]]{@email}",
    "[[a1..b2@c3]]{@email}", "[[a1@.b2]]{@email}", "[[a1@b2.]]{@email}",
    "[[a1@-b2]]{@email}", "[[a1@b2-]]{@email}", "[[a1@b2..c3]]{@email}",
    "[[a1@한글]]{@email}", "[[a1 b2]]{@url}", "[[a1/%zz]]{@url}",
    "[[a1/%2]]{@url}", "[[a1/<tag>]]{@url}", "[[서울시 -12]]{@address}",
    "[[서울시 12--3]]{@address}", "[[서울시 12-]]{@address}",
    "[[서울시 12~3]]{@address}", "[[서울시 １２]]{@address}",
    "[[서울시 🙂]]{@address}", "[[AB CD]]{@alphabet}", "[[100]]{@native}",
    "[[1,000]]{@sino}",
]
for source in invalid:
    result = subprocess.run([*base, source], capture_output=True, text=True, timeout=10)
    assert result.returncode and not result.stdout and "invalid argument" in result.stderr, (source, result)
    result = subprocess.run([cli, "stream-json", "--resources", resources, "--input-format",
                             "annotated", "--output-mode", "speech", "--chunk-bytes", "1", "-"],
                            input=source, capture_output=True, text=True, timeout=10)
    events = [json.loads(line) for line in result.stdout.splitlines()]
    assert result.returncode and not any(e["event"] in ("segment", "end") for e in events), (source, result)

# Address is deliberately KO-only. A wrong language cannot silently bypass it.
result = subprocess.run([*base, "--language", "en-US", "[[101 Main St]]{@address}"],
                        capture_output=True, text=True, timeout=10)
assert result.returncode and "invalid argument" in result.stderr, result
# Exact whole-control byte limit, including brackets and directive.
source = "[[서울시 12-3]]{@address}"
size = len(source.encode())
for limit, accepted in ((size, True), (size - 1, False)):
    result = subprocess.run([*base, "--max-control-bytes", str(limit), source],
                            capture_output=True, text=True, timeout=10)
    assert (result.returncode == 0) == accepted, result
    if not accepted:
        assert "limit" in result.stderr, result
# Old atom-value size contract stays bounded; long group @alphabet has a surface.
for source in ("A" * 96 + "{@alphabet}", "[[" + "1" * 96 + "]]{@sino}"):
    result = subprocess.run([*base, source], capture_output=True, text=True, timeout=10)
    assert result.returncode and "limit" in result.stderr, result

with tempfile.TemporaryDirectory(prefix="ttf-controls-") as directory:
    path = Path(directory) / "customer.tsv"
    for row in ("address_policy\tko-KR\tnumber\tnative\n",
                "address_policy\tko-KR\tleading_zero\tdrop\n",
                "address_policy\tko-KR\tother\tx\n",
                "address_policy\tko-KR\thyphen\t" + "x" * 96 + "\n"):
        path.write_text(row, encoding="utf-8")
        result = subprocess.run([*base, "--customer", str(path), "[[서울시 1]]{@address}"],
                                capture_output=True, text=True, timeout=10)
        assert result.returncode and "resource" in result.stderr, result
print(f"{len(invalid)} malformed controls rejected in batch and byte streams; language, limits and resource contracts passed")
