"""Explicit interval references fixed before running candidate code; text/IR only."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

cli, resources = sys.argv[1:]
resources = str(Path(resources).resolve())
PROFILES = ((), ("--first-target", "1", "--normal-target", "3", "--max-length", "12"))
# Surface, normalized text, first exact endpoint, second exact endpoint, unit.
REFERENCES = (
    ("1~2", "일에서 이", "1", "2", ""),
    ("1 ~ 2", "일에서 이", "1", "2", ""),
    ("1.5~2.5", "일 점 오에서 이 점 오", "1.5", "2.5", ""),
    ("-2~-1", "마이너스 이에서 마이너스 일", "-2", "-1", ""),
    ("1~-2", "일에서 마이너스 이", "1", "-2", ""),
    ("1~2개", "한 개에서 두 개", "1", "2", "개"),
    ("1 ~ 2개", "한 개에서 두 개", "1", "2", "개"),
    ("3개~5개", "세 개에서 다섯 개", "3", "5", "개"),
    ("3개 ~ 5개", "세 개에서 다섯 개", "3", "5", "개"),
    ("1시~2시", "한 시에서 두 시", "1", "2", "시"),
    ("1시 ~ 2시", "한 시에서 두 시", "1", "2", "시"),
    ("1~2시", "한 시에서 두 시", "1", "2", "시"),
    ("3명~5명", "세 명에서 다섯 명", "3", "5", "명"),
    ("3살~5살", "세 살에서 다섯 살", "3", "5", "살"),
    ("3분~5분", "삼 분에서 오 분", "3", "5", "분"),
    ("1~2%", "일 퍼센트에서 이 퍼센트", "1", "2", "percent"),
    ("1~2kg", "일 킬로그램에서 이 킬로그램", "1", "2", "kg"),
    ("09:00~10:30", "아홉 시에서 열 시 삼십 분", "09:00", "10:30", "time"),
    ("09:00 ~ 10:30", "아홉 시에서 열 시 삼십 분", "09:00", "10:30", "time"),
    ("9:00~10:30", "아홉 시에서 열 시 삼십 분", "09:00", "10:30", "time"),
    ("09:00:01~10:30:05", "아홉 시 영 분 일 초에서 열 시 삼십 분 오 초",
     "09:00:01", "10:30:05", "time"),
    ("09:00-10:30", "아홉 시에서 열 시 삼십 분", "09:00", "10:30", "time"),
)
INVALID = (
    "1개~2명", "1시~2분", "1kg~2개", "1개~2", "1개~2개~3개", "1~2~3",
    "1~2개~3개", "09:00~25:30", "09:60~10:30", "09:00~10:30:60",
    "09:00~2시", "1시~10:30", "09:00~10:00~11:00", "09:00 AM~10:00 AM",
    "09:00~10:00 PM", "1~",
)
checks = 0


def run(text, chunk=1, profile=(), overlay=(), language="ko-KR"):
    command = [cli, "stream-json", "--resources", resources, "--language", language,
               "--chunk-bytes", str(chunk), *profile, *overlay, "-"]
    result = subprocess.run(command, input=text.encode(), capture_output=True, timeout=10)
    assert result.returncode == 0, (text, result.stderr)
    events = [json.loads(line) for line in result.stdout.decode().splitlines()]
    assert events[-1]["event"] == "end", events
    segments = [event for event in events if event["event"] == "segment"]
    return "".join(segment["text"] for segment in segments), [
        span for segment in segments for span in segment["spans"]
    ], events


def check(text, expected, first="", last="", unit="", overlay=(), invalid=False):
    global checks
    batch = subprocess.run([cli, "normalize", "--resources", resources, "--language", "ko-KR",
                            *overlay, "--", text], capture_output=True, timeout=10)
    assert batch.returncode == 0 and batch.stdout.decode() == expected + "\n", (
        text, batch.stdout, batch.stderr, expected)
    for profile in PROFILES:
        previous = None
        for chunk in (1, 7, 4096):
            output, spans, events = run(text, chunk, profile, overlay)
            assert output == expected, (text, profile, chunk, output, expected)
            assert len(spans) == 1, (text, profile, spans)
            span = spans[0]
            assert span["input_begin"] == 0 and span["input_end"] == len(text.encode()), span
            assert span["semantic_type"] == ("TEXT" if invalid else "RANGE"), span
            assert (span["value"], span["secondary"], span["unit"]) == (first, last, unit), span
            if not invalid and not unit and "-" not in text.replace(first, "", 1):
                assert span["rule"] == "syntax.explicit-range", span
            if unit == "time":
                assert span["rule"] == "syntax.time-range", span
            if previous is not None:
                assert previous == events, (text, profile, chunk, "chunk-dependent event metadata")
            previous = events
            checks += 1


for reference in REFERENCES:
    check(*reference)
for text in INVALID:
    check(text, text, invalid=True)

finance = ("--domain", str(Path(resources) / "domains/finance/ko-KR/rules.tsv"))
check("1~2", "일에서 이", "1", "2", overlay=finance)
check("1~2개", "한 개에서 두 개", "1", "2", "개", overlay=finance)

with tempfile.TemporaryDirectory(prefix="ttf-explicit-range-") as temp:
    path = Path(temp) / "customer.tsv"
    path.write_text("quantity_range_separator\t*\t|\t1\n"
                    "policy\tko-KR\tquantity_range_separator\t부터 \n"
                    "policy\tko-KR\tquantity_range_suffix\t까지\n", encoding="utf-8")
    overlay = (*finance, "--customer", str(path))
    check("1|2", "일부터 이까지", "1", "2", overlay=overlay)
    check("1개 | 2개", "한 개부터 두 개까지", "1", "2", "개", overlay=overlay)
    check("09:00 | 10:30", "아홉 시부터 열 시 삼십 분까지",
          "09:00", "10:30", "time", overlay=overlay)
    path.write_text("quantity_range_separator\t*\t~\t0\n", encoding="utf-8")
    output, spans, _ = run("1~2", overlay=("--customer", str(path)))
    assert output == "일물결이" and all(span["semantic_type"] != "RANGE" for span in spans), (
        output, spans)
    checks += 1

output, spans, _ = run("~")
assert output == "물결" and len(spans) == 1 and spans[0]["semantic_type"] == "SYMBOL", (output, spans)
checks += 1
print(f"{checks} explicit/counter/clock range reference, IR, layer, chunk and guard checks passed")
