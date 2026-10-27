"""[[...]]{@dictionary:NAME}, {@exact:NAME} and {@nodict} through the CLI (2026-10-27)."""
import os
import subprocess
import sys
import tempfile

cli, resources = sys.argv[1:]
with tempfile.TemporaryDirectory() as tmp:
    kpop = os.path.join(tmp, "kpop.tsv")
    with open(kpop, "w", encoding="utf-8") as f:
        f.write("exact\tko-KR\t알쓰\t알콜 쓰레기\nlexicon\tko-KR\ten-US:samsung\t쌤쑹\n")
    base = [cli, "--resources", resources, "--language", "ko-KR", "--output-mode", "speech",
            "--input-format", "annotated", "--dictionary", "kpop=" + kpop]
    cases = [
        ("알쓰라서 [[알쓰]]{@dictionary:kpop}라서", "알쓰라서 알콜 쓰레기라서"),
        ("Samsung 폰, [[Samsung]]{@exact:kpop.tsv} 폰", "삼성 폰, 쌤쑹 폰"),
        ("Samsung 폰과 [[Samsung]]{@nodict} 폰", "삼성 폰과 샘성 폰"),
        ("Pokémon 카드, [[Pokémon]]{@nodict} 카드", "포켓몬 카드, 포케먼 카드"),
        ("삼성 3개 [[3개]]{@nodict}", "삼성 세 개 세 개"),
        ("[[알쓰]]{@dictionary:missing}", "알쓰"),
    ]
    for text, want in cases:
        got = subprocess.run([base[0], "normalize", *base[1:], text], capture_output=True, text=True,
                             timeout=20).stdout.strip()
        assert got == want, (text, got, want)
        streamed = subprocess.run([base[0], "stream", *base[1:], "--chunk-bytes", "1", "-"], input=text + "\n",
                                  capture_output=True, text=True, timeout=20).stdout.strip()
        assert streamed == want, ("stream", text, streamed, want)
    strict = [base[0], "normalize", *base[1:], "--invalid-control", "error"]
    for text in ("[[알쓰]]{@dictionary:missing}", "[[알쓰]]{@dictionary:bad/name}", "[[알쓰]]{@dictionary:}",
                 "[[알쓰]]{@exact:x.y}", "[[알쓰]]{@nodict:x}"):
        result = subprocess.run([*strict, text], capture_output=True, text=True, timeout=20)
        assert result.returncode and "invalid argument" in result.stderr, (text, result)
    bad = subprocess.run([base[0], "normalize", "--resources", resources, "--dictionary", "kpop", "x"],
                         capture_output=True, text=True, timeout=20)
    assert bad.returncode, bad
print("dictionary controls: ok")
