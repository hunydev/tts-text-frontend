#!/usr/bin/env python3
"""Exercise the public CLI transport used by external TTS adapters."""

import json
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading


CLI, RESOURCES = sys.argv[1:3]
FIELDS = {
    "begin", "end", "input_begin", "input_end", "script", "source_lang",
    "context_lang", "render_lang", "semantic_type", "value", "unit", "rule",
    "resource_layer", "pronunciation_layer", "lexicon", "verbalizer", "context_trace",
}


def command(mode, *args):
    return [CLI, mode, "--resources", RESOURCES, *args]


def run(mode, *args, data=None, success=True):
    result = subprocess.run(command(mode, *args), input=data, capture_output=True, timeout=15)
    assert (result.returncode == 0) == success, (result.args, result.returncode, result.stderr)
    return result


def events_from(raw):
    assert not raw or raw.endswith(b"\n"), raw
    lines = raw.split(b"\n")[:-1]
    assert all(lines), lines
    return [json.loads(line.decode("utf-8")) for line in lines]


def validate(events, source, expected):
    assert events[-1]["event"] == "end", events
    segments = events[:-1]
    assert events[-1] == {
        "version": 1, "event": "end", "segment_count": len(segments), "input_bytes": len(source),
    }
    offset = 0
    output = []
    for index, event in enumerate(segments):
        assert event["event"] == "segment" and event["version"] == 1
        assert event["sequence"] == index
        assert event["input_begin"] == offset
        assert offset < event["input_end"] <= len(source)
        encoded = event["text"].encode("utf-8")
        local_offset = 0
        for span in event["spans"]:
            assert set(span) == FIELDS, span
            assert span["begin"] == local_offset
            assert span["begin"] <= span["end"] <= len(encoded)
            assert offset <= span["input_begin"] <= span["input_end"] <= event["input_end"]
            encoded[span["begin"]:span["end"]].decode("utf-8")
            source[span["input_begin"]:span["input_end"]].decode("utf-8")
            assert all(isinstance(span[key], str) for key in FIELDS if key not in {
                "begin", "end", "input_begin", "input_end",
            })
            local_offset = span["end"]
        assert local_offset == len(encoded)
        offset = event["input_end"]
        output.append(event["text"])
    assert offset == len(source)
    assert "".join(output) == expected, (output, expected)


text = (
    "1st는 첫 번째라는 뜻입니다.\n영어로는 다음과 같은 예문이 있습니다.\n"
    "Please go to the 1st floor.\nApple은 사과입니다. GPU를 사용합니다. "
    "WiFi를 사용합니다. 가격은 12,500원입니다. ver. 2.1.3을 사용합니다."
)
payload = text.encode("utf-8")
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory) / "input.txt"
    path.write_bytes(payload)
    for options in [[], ["--first-target", "12", "--normal-target", "24", "--max-length", "40"]]:
        expected = run("normalize", *options, text).stdout[:-1].decode("utf-8")
        for chunk in [1, 2, 7, 4096, 1000000]:
            result = run("stream-json", *options, "--chunk-bytes", str(chunk), str(path))
            assert not result.stderr
            events = events_from(result.stdout)
            validate(events, payload, expected)
        assert run("stream", *options, "--chunk-bytes", "3", "-", data=payload).stdout.decode("utf-8") == expected
    spans = [span for event in events[:-1] for span in event["spans"]]
    ordinals = [span for span in spans if span["semantic_type"] == "ORDINAL"]
    assert any(span["source_lang"] == "en-US" and span["context_lang"] == "ko-KR" for span in ordinals)
    assert any(span["source_lang"] == "en-US" and span["context_lang"] == "en-US" for span in ordinals)
    assert any(span["lexicon"] == "lexicon:system" and span["render_lang"] == "ko-KR" for span in spans)
    assert any(span["context_trace"] and span["rule"] for span in spans)

# Every allowed ASCII control, literal quotes/backslashes, and Unicode survive
# JSON escaping. NUL is deliberately rejected by the existing library scanner.
controls = '"\\' + "".join(chr(code) for code in range(1, 32)) + "한글😀"
expected = run("normalize", controls).stdout[:-1].decode("utf-8")
result = run("stream-json", "--chunk-bytes", "1", "-", data=controls.encode("utf-8"))
validate(events_from(result.stdout), controls.encode("utf-8"), expected)
assert b"\\u0001" in result.stdout and b"\\b" in result.stdout and b"\\f" in result.stdout
validate(events_from(run("stream-json", "-", data=b"").stdout), b"", "")

for invalid in [b"\xff", b"\xe3\x81", b"\xc0\xaf", b"\xed\xa0\x80", b"\x00"]:
    result = run("stream-json", "--chunk-bytes", "1", "-", data=b"Apple. Next " + invalid, success=False)
    assert result.stderr and all(event["event"] != "end" for event in events_from(result.stdout))

for option in ["--chunk-bytes", "--first-target", "--normal-target", "--max-length"]:
    for invalid in ["0", "-1", "+1", "1x", "", "9" * 100]:
        result = run("stream-json", option, invalid, "-", data=b"", success=False)
        assert not result.stdout and result.stderr
    run("stream-json", option, data=b"", success=False)
run("normalize", "--chunk-bytes", "1", "Apple", success=False)
run("stream-json", "--unknown", "-", data=b"", success=False)
run("stream-json", str(Path(directory) / "missing.txt"), success=False)

# The first segment must arrive while stdin is still open and far below the
# requested 4096-byte read. This catches both fread fill waits and stdout buffers.
process = subprocess.Popen(command("stream-json", "--chunk-bytes", "4096", "-"),
                           stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
first_line = queue.Queue()
reader = threading.Thread(target=lambda: first_line.put(process.stdout.readline()), daemon=True)
reader.start()
try:
    prefix = "Apple은 사과입니다. GPU".encode("utf-8")
    suffix = "를 사용합니다.".encode("utf-8")
    process.stdin.write(prefix)
    process.stdin.flush()
    first = first_line.get(timeout=5)
    assert json.loads(first)["event"] == "segment", first
    process.stdin.write(suffix)
    process.stdin.close()
    process.stdin = None
    remainder, error = process.communicate(timeout=10)
    assert process.returncode == 0, error
    expected = run("normalize", (prefix + suffix).decode("utf-8")).stdout[:-1].decode("utf-8")
    validate(events_from(first + remainder), prefix + suffix, expected)
finally:
    if process.poll() is None:
        process.kill()
        process.wait()
    reader.join(timeout=1)

print("CLI JSON protocol: batch equivalence, metadata, escaping, errors, options, and early pipe delivery passed")
