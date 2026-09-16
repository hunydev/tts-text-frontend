"""Explicit identifier labels, exact zeros/groups, overlays and safe negatives.

These are invented text fixtures; they do not represent customer account data.
"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile


cli, resources = sys.argv[1:]
profiles = [(), ("--first-target", "1", "--normal-target", "3", "--max-length", "12")]
checks = 0


def inspect(text, chunk=1, profile=(), overrides=()):
    command = [cli, "stream-json", "--resources", resources, "--chunk-bytes", str(chunk),
               *profile, *overrides, "-"]
    run = subprocess.run(command, input=text.encode(), capture_output=True, timeout=10)
    assert run.returncode == 0, (text, run.stderr)
    events = [json.loads(line) for line in run.stdout.splitlines()]
    assert events[-1]["event"] == "end", events
    segments = events[:-1]
    output = "".join(segment["text"] for segment in segments)
    spans = [span for segment in segments for span in segment["spans"]]
    return output, spans


positive = [
    ("입금 계좌는 110-123-456789입니다.", "입금 계좌는 일일공 일이삼 사오육칠팔구입니다.",
     "110-123-456789", "계좌는"),
    ("거래 참조 번호는 001204입니다.", "거래 참조 번호는 공공일이공사입니다.",
     "001204", "참조 번호는"),
    ("계좌번호는 001-204입니다.", "계좌번호는 공공일 이공사입니다.",
     "001-204", "계좌번호는"),
    ("참조 번호: 0000", "참조 번호: 공공공공", "0000", "참조 번호"),
    ("계좌 번호는\t001204입니다.", "계좌 번호는\t공공일이공사입니다.",
     "001204", "계좌 번호는"),
    ("참조  \t번호는 012-340입니다.", "참조  \t번호는 공일이 삼사공입니다.",
     "012-340", "참조 번호는"),
    ("거래번호:001204", "거래번호:공공일이공사", "001204", "거래번호"),
    ("계좌는 010-1234-5678입니다.", "계좌는 공일공 일이삼사 오육칠팔입니다.",
     "010-1234-5678", "계좌는"),
    ("참조번호는 123-456입니다.", "참조번호는 일이삼 사오육입니다.",
     "123-456", "참조번호는"),
    ("참조 번호는 2026-09-16입니다.", "참조 번호는 이공이육 공구 일육입니다.",
     "2026-09-16", "참조 번호는"),
]
for text, expected, value, cue in positive:
    for profile in profiles:
        batch = subprocess.run([cli, "normalize", "--resources", resources, *profile, "--", text],
                               capture_output=True, check=True, timeout=10).stdout.decode()[:-1]
        assert batch == expected, (text, batch, expected)
        for chunk in (1, 7, 4096):
            output, spans = inspect(text, chunk, profile)
            assert output == expected, (text, profile, chunk, output)
            ids = [span for span in spans if span["semantic_type"] == "DIGIT_SEQUENCE"]
            assert len(ids) == 1, (text, spans)
            span = ids[0]
            assert span["value"] == value and span["secondary"] == cue, (text, span)
            assert span["unit"] == "financial_identifier", (text, span)
            assert span["rule"] == "ko:labeled-identifier", (text, span)
            assert span["source_lang"] == "und", (text, span)
            assert span["context_lang"] == span["render_lang"] == "ko-KR", (text, span)
            assert text.encode()[span["input_begin"]:span["input_end"]].decode() == value, span
            checks += 1

# Numeric quantities and weak labels must keep their usual meanings.
negative = [
    ("번호는 12입니다.", "번호는 십이입니다."),
    ("계좌는 3개입니다.", "계좌는 세 개입니다."),
    ("계좌는 12,500원입니다.", "계좌는 만이천오백 원입니다."),
    ("참조 번호는 1.5입니다.", "참조 번호는 일 점 오입니다."),
    ("계좌는 -12입니다.", "계좌는 마이너스 십이입니다."),
    ("내계좌는 001204입니다.", "내계좌는 천이백사입니다."),
    ("참조\n번호는 001204입니다.", "참조\n번호는 천이백사입니다."),
    ("001204", "천이백사"),
]
for text, expected in negative:
    for profile in profiles:
        for chunk in (1, 4096):
            output, spans = inspect(text, chunk, profile)
            assert output == expected, (text, profile, output)
            assert all(span["semantic_type"] != "DIGIT_SEQUENCE" for span in spans), (text, spans)
            checks += 1

# New labels, zero/spacing policy and disabling a cue require resources only.
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "identifiers.tsv"
    path.write_text("identifier_label\tko-KR\t정산코드는\tdigits\n"
                    "identifier_render\tko-KR\tzero\t영\n"
                    "identifier_render\tko-KR\tseparator\t 번에 \n", encoding="utf-8")
    for profile in profiles:
        output, spans = inspect("정산코드는 001-204입니다.", profile=profile,
                                overrides=("--customer", str(path)))
        assert output == "정산코드는 영영일 번에 이영사입니다.", output
        identifier = [span for span in spans if span["semantic_type"] == "DIGIT_SEQUENCE"]
        assert len(identifier) == 1 and identifier[0]["resource_layer"] == "customer", spans
        checks += 1
    path.write_text("identifier_label\tko-KR\t계좌는\tdisabled\n", encoding="utf-8")
    output, spans = inspect("계좌는 001204입니다.", overrides=("--customer", str(path)))
    assert output == "계좌는 천이백사입니다.", output
    assert all(span["semantic_type"] != "DIGIT_SEQUENCE" for span in spans), spans
    checks += 1

print(f"{len(positive)} identifier and {len(negative)} negative cases; "
      f"{checks} text/metadata/chunk/overlay checks passed")
