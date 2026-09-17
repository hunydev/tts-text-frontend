#!/usr/bin/env python3
"""Detailed trace transport, resource evidence and streaming failure contracts."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

CLI, RESOURCES = map(Path, sys.argv[1:3])
CHECKS = 0
FIELDS = {
    "input_begin", "input_end", "output_begin", "output_end", "part_begin", "part_end",
    "phone_index", "declaration_order", "stage", "disposition", "layer_known", "layer",
    "language", "resource_language", "kind", "key", "value", "text", "detail",
}


def require(condition, detail=None):
    global CHECKS
    CHECKS += 1
    assert condition, detail


def command(mode, *args):
    return [str(CLI), mode, "--resources", str(RESOURCES), *map(str, args)]


def run(mode, *args, data=None, success=True):
    process = subprocess.run(command(mode, *args), input=data, capture_output=True, timeout=30)
    require((process.returncode == 0) == success, (process.args, process.returncode, process.stderr))
    return process


def events(raw):
    require(not raw or raw.endswith(b"\n"), raw)
    return [json.loads(line) for line in raw.splitlines()]


def inspect(text, *options):
    legacy = run("inspect", *options, text).stdout
    traced = run("inspect", "--trace", *options, text).stdout
    require(traced.startswith(legacy), (legacy, traced))
    extra = traced[len(legacy):]
    require(extra.startswith(b"trace=") and extra.endswith(b"\n"), extra)
    trace = json.loads(extra[len(b"trace="):])
    normalized = [line for line in legacy.splitlines() if line.startswith(b"normalized=")]
    require(len(normalized) == 1, legacy)
    return json.loads(normalized[0][len(b"normalized="):]), trace


def flatten(records, source):
    require(records and records[-1]["event"] == "end", records)
    segments = records[:-1]
    text, spans, trace = [], [], []
    output_offset = input_offset = 0
    for sequence, segment in enumerate(segments):
        require(segment["event"] == "segment" and segment["version"] == 1, segment)
        require(segment["sequence"] == sequence and segment["input_begin"] == input_offset, segment)
        require(segment["trace_version"] == 1 and isinstance(segment["trace"], list), segment)
        output = segment["text"].encode("utf-8")
        input_offset = segment["input_end"]
        require(input_offset <= len(source), segment)
        for span in segment["spans"]:
            adjusted = dict(span)
            adjusted["begin"] += output_offset
            adjusted["end"] += output_offset
            spans.append(adjusted)
        for event in segment["trace"]:
            require(set(event) == FIELDS, event)
            require(event["stage"] in {"lexicon", "source-pronunciation", "target-phonetic",
                                       "target-grapheme", "policy"}, event)
            require(event["disposition"] in {"selected", "rejected"}, event)
            require(segment["input_begin"] <= event["input_begin"] <= event["input_end"] <= input_offset, event)
            require(0 <= event["output_begin"] <= event["output_end"] <= len(output), event)
            source[event["input_begin"]:event["input_end"]].decode("utf-8")
            output[event["output_begin"]:event["output_end"]].decode("utf-8")
            require(isinstance(event["layer_known"], bool), event)
            if event["layer_known"]:
                require(event["layer"] in {"common", "system", "domain", "customer", "user"}, event)
                require(isinstance(event["declaration_order"], int), event)
            else:
                require(event["layer"] == "unknown" and event["declaration_order"] is None, event)
            for field in ("part_begin", "part_end", "phone_index"):
                require(event[field] is None or isinstance(event[field], int) and event[field] >= 0, event)
            adjusted = dict(event)
            adjusted["output_begin"] += output_offset
            adjusted["output_end"] += output_offset
            trace.append(adjusted)
        text.append(segment["text"])
        output_offset += len(output)
    require(input_offset == len(source), (input_offset, len(source)))
    require(records[-1] == {"version": 1, "event": "end", "segment_count": len(segments),
                            "input_bytes": len(source)}, records[-1])
    return "".join(text), spans, trace


text = ("Apple은 사과입니다. baba는 표기입니다. 101st는 순서입니다. "
        "After Effects를 씁니다. U.S.는 표기입니다.")
source = text.encode("utf-8")
for profile in ((), ("--first-target", "1", "--normal-target", "3", "--max-length", "12")):
    expected_text, expected_trace = inspect(text, *profile)
    baseline = None
    for chunk in (1, 2, 3, 5, 7, 17, 4096):
        traced = events(run("stream-json", "--trace", *profile, "--chunk-bytes", chunk, "-", data=source).stdout)
        legacy = events(run("stream-json", *profile, "--chunk-bytes", chunk, "-", data=source).stdout)
        # Additive fields preserve the old segment protocol byte values.
        stripped = [{key: value for key, value in item.items() if key not in {"trace_version", "trace"}}
                    for item in traced]
        require(stripped == legacy, (chunk, stripped, legacy))
        actual = flatten(traced, source)
        require(actual[0] == expected_text and actual[2] == expected_trace, (chunk, actual, expected_trace))
        if baseline is not None:
            require(actual == baseline, (chunk, actual, baseline))
        baseline = actual

with tempfile.TemporaryDirectory(prefix="ttf-detailed-trace-") as directory:
    root = Path(directory)
    domain, customer = root / "domain.tsv", root / "customer.tsv"
    domain.write_text("g2p\ten-US\tba\tB AA\n", encoding="utf-8")
    customer.write_text("g2p\ten-US\tb\tB IY\nphone_vowel\tko-KR\tAA\t4\n"
                        "phone_onset\tko-KR\tAA\t0\n", encoding="utf-8")
    output, trace = inspect("baba는 표기입니다.", "--domain", domain, "--customer", customer)
    require(output == "버버는 표기입니다.", output)
    source_events = [event for event in trace if event["kind"] == "g2p"]
    require(len(source_events) == 2 and all(event["key"] == "ba" and event["layer"] == "domain"
                                          for event in source_events), trace)
    vowels = [event for event in trace if event["kind"] == "phone_vowel" and event["key"] == "AA"]
    require(len(vowels) == 2 and all(event["layer"] == "customer" for event in vowels), trace)
    require(not any(event["kind"] == "phone_onset" and event["key"] == "AA" for event in trace), trace)

    customer.write_text("pronunciation\ten-US\tbaba\tAH UNMAPPED\n", encoding="utf-8")
    output, trace = inspect("baba는 표기입니다.", "--customer", customer)
    require(output == "브아브아는 표기입니다.", output)
    require(any(event["detail"] == "target-phonetic-failed" and event["stage"] == "policy"
                and event["disposition"] == "rejected" for event in trace), trace)
    rejected = [event for event in trace if event["stage"] in {"source-pronunciation", "target-phonetic"}]
    require(rejected and all(event["disposition"] == "rejected" for event in rejected), trace)
    require(any(event["stage"] == "target-grapheme" and event["disposition"] == "selected"
                for event in trace), trace)

    # A too-long source word bypasses the bounded source G2P prototype. Its
    # evidence must identify the source failure, without inventing mapper work.
    long_word = "z" * 100
    output, trace = inspect(long_word + "는 표기입니다.")
    require(long_word not in output and output.endswith("는 표기입니다."), output)
    require(any(event["detail"] == "source-pronunciation-failed" and event["stage"] == "policy"
                and event["language"] == "en-US" for event in trace), trace)
    require(not any(event["stage"] == "target-phonetic" for event in trace), trace)
    require(any(event["stage"] == "target-grapheme" for event in trace), trace)

    customer.write_text("pronunciation\ten-US\tabacus\tAH S\n"
                        "pronunciation_variant\ten-US\tabacus#alternate\tB IY\n"
                        "pronunciation_choice\ten-US\tabacus\talternate\n", encoding="utf-8")
    output, trace = inspect("abacus는 표기입니다.", "--customer", customer)
    require(output == "비는 표기입니다.", output)
    variants = [event for event in trace if event["kind"] == "pronunciation_variant"]
    require(len(variants) == 1 and variants[0]["key"] == "abacus#alternate" and
            variants[0]["value"] == "B IY" and variants[0]["layer"] == "customer", trace)
    choices = [event for event in trace if event["kind"] == "pronunciation_choice"]
    require(len(choices) == 1 and choices[0]["value"] == "alternate", trace)
    for choice in ("", "missing"):
        customer.write_text("pronunciation\ten-US\tabacus\tAH S\n"
                            f"pronunciation_choice\ten-US\tabacus\t{choice}\n", encoding="utf-8")
        rejected = run("inspect", "--trace", "--customer", customer, "abacus는 표기입니다.", success=False)
        require(not rejected.stdout and b"invalid resource" in rejected.stderr, rejected)

# Limits apply to the request, not individually to each segment. One lexicon
# event fits the first sentence; the identical second sentence must then fail.
limited_input = "Apple은 사과입니다. Apple은 사과입니다. ".encode("utf-8")
for chunk in (1, 4096):
    process = run("stream-json", "--trace", "--trace-max-events", "1", "--chunk-bytes", chunk,
                  "-", data=limited_input, success=False)
    committed = events(process.stdout)
    require(len(committed) == 1 and committed[0]["event"] == "segment", committed)
    require(committed[0]["text"] == "애플은 사과입니다." and len(committed[0]["trace"]) == 1, committed)
    require(b"limit" in process.stderr, process.stderr)
for flag in ("--trace-max-events", "--trace-max-bytes"):
    process = run("inspect", "--trace", flag, "1", "baba는 표기입니다.", success=False)
    require(not process.stdout and b"limit" in process.stderr, process)
    process = run("stream-json", "--trace", flag, "1", "-", data="baba는 표기입니다.".encode(), success=False)
    require(not process.stdout and b"limit" in process.stderr, process)
    for invalid in ("0", "-1", "+1", "1x", "", "9" * 100):
        process = run("inspect", "--trace", flag, invalid, "Apple은 사과입니다.", success=False)
        require(not process.stdout, process)
    require(not run("inspect", flag, "100", "Apple은 사과입니다.", success=False).stdout)
for mode, value in (("normalize", "Apple은 사과입니다."), ("stream", "-"), ("validate-resource", "absent.tsv")):
    require(not run(mode, "--trace", value, data=b"" if value == "-" else None, success=False).stdout)

# CLI write failures must not be reported as successful trace generation.
if Path("/dev/full").exists():
    for mode, args, payload in (("inspect", ["Apple은 사과입니다."], None),
                                ("stream-json", ["-"], "Apple은 사과입니다.".encode())):
        with open("/dev/full", "wb") as destination:
            process = subprocess.run(command(mode, "--trace", *args), input=payload,
                                     stdout=destination, stderr=subprocess.PIPE, timeout=30)
        require(process.returncode != 0 and
                (b"I/O error" in process.stderr or
                 mode == "stream-json" and b"callback cancelled" in process.stderr), process)

print(f"detailed trace CLI: {CHECKS} assertions passed")
