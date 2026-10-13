"""{@mark} and {@mark:name}: a position marker with no reading (2026-10-13).

The marker emits no text and one zero-width span (rule control.mark, unit @mark,
value = name) at its place in the output, in batch and in every stream division."""
import json
import subprocess
import sys

cli, resources = sys.argv[1:]
BASE = ["--resources", resources, "--input-format", "annotated"]


def normalize(text, *extra):
    r = subprocess.run([cli, "normalize", *BASE, *extra, text], capture_output=True, text=True, timeout=20)
    return r


def stream(text, chunk, *extra):
    r = subprocess.run([cli, "stream-json", *BASE, *extra, "--chunk-bytes", str(chunk), "-"],
                       input=text.encode(), capture_output=True, timeout=20)
    assert r.returncode == 0, (text, chunk, r.stderr)
    out, spans, base = [], [], 0
    for event in map(json.loads, r.stdout.splitlines()):
        if event["event"] != "segment":
            continue
        for span in event["spans"]:
            spans.append(dict(span, begin=span["begin"] + base, end=span["end"] + base))
        out.append(event["text"])
        base += len(event["text"].encode())
    return "".join(out), spans


cases = [
    ("ko-KR", "speech", "안녕하세요 {@mark:cough} 반갑습니다.", "안녕하세요 반갑습니다.", ["cough"]),
    ("ko-KR", "speech", "기침{@mark:cough} 소리가 났다.", "기침 소리가 났다.", ["cough"]),
    ("ko-KR", "speech", "{@mark} 시작합니다.", "시작합니다.", [""]),
    ("ko-KR", "speech", "가격은 1234{@mark:laugh}원입니다.", "가격은 천이백삼십사원입니다.", ["laugh"]),
    ("ko-KR", "text", "하나 {@mark:breath} 둘 {@mark:sigh_2} 셋", "하나 둘 셋", ["breath", "sigh_2"]),
    ("en-US", "speech", "Hello {@mark:laugh} world, it costs $5.", "Hello world, it costs five dollars.", ["laugh"]),
    ("vi-VN", "speech", "Xin chào {@mark:cough} các bạn.", "Xin chào các bạn.", ["cough"]),
]
checks = 0
for lang, mode, text, expected, names in cases:
    opts = ["--language", lang, "--output-mode", mode]
    r = normalize(text, *opts)
    assert r.returncode == 0 and r.stdout.rstrip("\n") == expected, (text, r.stdout, r.stderr)
    reference = None
    for chunk in (4096, 7, 1):
        out, spans = stream(text, chunk, *opts)
        assert out.rstrip("\n") == expected, (text, chunk, out)
        marks = [s for s in spans if s["unit"] == "@mark"]
        assert [s["value"] for s in marks] == names, (text, chunk, marks)
        for s in marks:
            assert s["begin"] == s["end"] and s["rule"] == "control.mark" and s["semantic_type"] == "CUSTOM", s
            assert text.encode()[s["input_begin"]:s["input_end"]].decode().startswith("{@mark"), s
        positions = [(s["begin"], s["input_begin"]) for s in marks]
        if reference is None:
            reference = positions
        assert positions == reference, (text, chunk, positions, reference)
        checks += 1
# A marker sits where it was written: the reading before it ends at its offset.
out, spans = stream("안녕하세요 {@mark:cough} 반갑습니다.", 4096, "--language", "ko-KR", "--output-mode", "speech")
mark = next(s for s in spans if s["unit"] == "@mark")
assert out.encode()[:mark["begin"]].decode() == "안녕하세요 ", (out, mark)
# Malformed markers are errors with invalid-control=error and ordinary text otherwise.
for bad in ["{@mark:}", "{@mark:Cough}", "{@mark:1a}", "{@mark:a b}", "{@markx}", "[[가]]{@mark}",
            "{@mark:" + "a" * 25 + "}", "{@mark"]:
    r = normalize(bad, "--output-mode", "speech", "--invalid-control", "error")
    assert r.returncode and "invalid argument" in r.stderr, (bad, r)
    checks += 1
# Plain input does not interpret the marker.
r = subprocess.run([cli, "normalize", "--resources", resources, "--output-mode", "text", "가 {@mark} 나"],
                   capture_output=True, text=True, timeout=20)
assert r.returncode == 0 and "{@" in r.stdout and r.stdout.startswith("가 {@"), r
print(f"{checks} marker checks (batch, 3 stream divisions, malformed and plain input) passed")
