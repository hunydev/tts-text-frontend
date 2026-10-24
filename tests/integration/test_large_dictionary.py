"""A large user dictionary stays fast (2026-10-25): 300,000 exact rules in three customer files.

Exact rules were matched by walking a list of every rule at each position, so a million
rules took seconds a sentence. They are now found by hash in their own table; this test
loads 300,000 rules, checks that a hit and a miss read correctly, and bounds the time of a
text with 200 sentences (generous: it took far longer before the index).
"""
import subprocess
import sys
import tempfile
import time
from pathlib import Path

cli, resources = sys.argv[1:]
DIGITS = "영일이삼사오육칠팔구"
with tempfile.TemporaryDirectory() as tmp:
    files = []
    for part in range(3):
        path = Path(tmp) / f"user{part}.tsv"
        with path.open("w", encoding="utf-8") as f:
            for v in range(1000000 + part * 100000, 1000000 + (part + 1) * 100000):
                f.write(f"exact\tko-KR\t{v}\t{''.join(DIGITS[int(c)] for c in str(v))}\n")
        files += ["--customer", str(path)]
    sentence = "주문 번호 1234567번과 회원 2345678님, 사과 3개를 확인했습니다. "
    text = sentence * 200
    start = time.perf_counter()
    result = subprocess.run([cli, "normalize", "--resources", resources, "--language", "ko-KR",
                             "--output-mode", "speech", *files, text],
                            capture_output=True, text=True, timeout=120)
    elapsed = time.perf_counter() - start
    assert result.returncode == 0, result.stderr
    out = result.stdout.strip()
    first = out[: len(out) // 200 + 20]
    # 1234567 is in the dictionary; 2345678 is not (the engine reads it its own way)
    assert "일이삼사오육칠" in first, first
    assert "이삼사오육칠팔" not in first, first
    assert out.count("일이삼사오육칠") == 200, out.count("일이삼사오육칠")
    assert elapsed < 30, f"{elapsed:.1f}s for 300,000 rules and 200 sentences"
print(f"large dictionary: 300000 rules, 200 sentences, {elapsed:.2f}s")
