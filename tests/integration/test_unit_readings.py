"""Every shared unit suffix (numeric_suffix * ... MEASURE:key) has a Korean reading.

A Korean request reads a MEASURE unit from `unit ko-KR <key>`; without one the key
itself would be spelled (MB_per_s read as 엠비 언더스코어 ..., found 2026-10-14).
"""
import sys
from pathlib import Path

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "resources"
keys, korean = {}, set()
for path in sorted(root.rglob("*.tsv")):
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        f = line.split("\t")
        if len(f) >= 4 and f[0] == "numeric_suffix" and f[1] == "*" and f[3].startswith("MEASURE:"):
            keys.setdefault(f[3][len("MEASURE:"):], f"{path.relative_to(root)}:{number}")
        elif len(f) >= 4 and f[0] == "unit" and f[1] == "ko-KR":
            korean.add(f[2])
missing = sorted(k for k in keys if k not in korean)
assert not missing, "shared units without a Korean reading: " + ", ".join(f"{k} ({keys[k]})" for k in missing)
print(f"{len(keys)} shared units, all with a Korean reading")
