"""Repeated financial unit intervals: exact IR, guards, overlays and streaming."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

cli, resources = sys.argv[1:]
resources = str(Path(resources).resolve())
root = Path(__file__).resolve().parents[2]
profiles = ([], ["--first-target", "1", "--normal-target", "3", "--max-length", "12"])
checks = 0


def stream(text, language="ko-KR", chunk=1, profile=(), overlay=()):
    command = [cli, "stream-json", "--resources", resources, "--language", language,
               "--chunk-bytes", str(chunk), *profile, *overlay, "-"]
    run = subprocess.run(command, input=text.encode(), capture_output=True, timeout=10)
    assert run.returncode == 0, (text, command, run.stderr)
    events = [json.loads(line) for line in run.stdout.decode().splitlines()]
    assert events[-1]["event"] == "end", events
    segments = [event for event in events if event["event"] == "segment"]
    return "".join(event["text"] for event in segments), [
        span for event in segments for span in event["spans"]
    ], events


# Explicit decimal endpoint references supplement the frozen evaluation corpus:
# the report v2 format currently asserts value/unit but has no secondary field.
secondary = {
    "krw": "200", "scaled-krw": "20000", "scaled-mixed": "20000",
    "scaled-second": "20000", "scaled-decimal": "25000", "scaled-spaces": "200000000",
    "prefixed-krw": "2000", "dollars-ko": "20", "dollars-en": "20",
    "dollar-decimals": "2.50", "rate": "2.5", "rate-spaces": "2.5", "rate-en": "2.5",
    "negative-rate": "-0.5", "bp": "50", "pp": "0.2", "pp-en": "2", "kg": "2",
    "kg-en": "2", "scientific": "200", "mismatch-currency": "", "mismatch-units": "",
    "mismatch-rate": "", "mismatch-scale": "",
}
for line in (root / "tests/evaluation/finance_ranges_v7.jsonl").read_text().splitlines():
    case = json.loads(line)
    expected = case["expected_spans"][0]
    raw = case["text"].encode()
    begin = raw.index(expected["surface"].encode())
    end = begin + len(expected["surface"].encode())
    for profile in profiles:
        previous = None
        for chunk in (1, 7, 4096):
            output, spans, events = stream(case["text"], expected["context_lang"], chunk, profile)
            assert output == case["expected"], (case["id"], chunk, profile, output)
            matches = [span for span in spans if (span["input_begin"], span["input_end"]) ==
                       (begin, end)]
            assert len(matches) == 1, (case["id"], spans)
            span = matches[0]
            for key in ("semantic_type", "value", "unit", "context_lang"):
                assert span[key] == expected[key], (case["id"], key, span)
            assert span["secondary"] == secondary[case["id"].removeprefix("range-")], span
            if previous is not None:
                assert events == previous, (case["id"], profile, chunk)
            previous = events
            checks += 1

# Invalid types/units, damaged second endpoint and ambiguous adjacent signs
# preserve the whole candidate. Counter ranges and general interval algebra are
# outside this prototype; no exchange/unit conversions are inferred.
for text in ("1kg~2km", "1%~2%p", "$10~₩20", "1만원~2kg", "1kg~2만원",
             "1kg~2bpx", "1%--2%", "1만원--2만원", "1%~2%~3%", "1kg~2kg~3kg",
             "1만원~2만원~3만원", "1만원~2원~3원"):
    for profile in profiles:
        for chunk in (1, 7, 4096):
            output, spans, _ = stream(text, profile=profile, chunk=chunk)
            assert output == text, (text, output)
            assert len(spans) == 1 and spans[0]["semantic_type"] == "TEXT", (text, spans)
            assert not spans[0]["value"] and not spans[0]["secondary"] and not spans[0]["unit"]
            checks += 1

with tempfile.TemporaryDirectory(prefix="ttf-finance-ranges-") as temp:
    path = Path(temp) / "customer.tsv"
    path.write_text("policy\tko-KR\tquantity_range_separator\t 에서 \n"
                    "quantity_range_separator\t*\t|\t1\n", encoding="utf-8")
    overlay = ("--customer", str(path))
    for text, expected in (("1.5%~2.5%", "일 점 오 에서 이 점 오 퍼센트"),
                           ("1만원|2만원", "만 에서 이만 원"),
                           ("1kg | 2kg", "일 에서 이 킬로그램")):
        for profile in profiles:
            output, spans, _ = stream(text, profile=profile, overlay=overlay)
            assert output == expected, (text, output)
            assert len(spans) == 1 and spans[0]["semantic_type"] == "RANGE", spans
            if "|" in text:
                assert spans[0]["resource_layer"] == "customer", spans
            checks += 1
    path.write_text("quantity_range_separator\t*\t~\t0\n", encoding="utf-8")
    output, spans, _ = stream("1%~2%", overlay=overlay)
    assert output == "일 퍼센트~이 퍼센트" and all(s["semantic_type"] != "RANGE" for s in spans)
    checks += 1

print(f"{checks} repeated-unit interval text/IR/secondary/span/chunk/override checks")
