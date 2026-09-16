"""Explicit title cues and layered pronunciations, with independent references."""
import json
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading

cli, resources = sys.argv[1:]
checks = 0


def check(text, expected, references=(), options=(), tiny=False):
    global checks
    source = text.encode()
    segmentation = ("--first-target", "1", "--normal-target", "3", "--max-length", "12") if tiny else ()
    batch = subprocess.run([cli, "normalize", "--resources", resources,
                            *segmentation, *options, text], capture_output=True, timeout=10)
    assert batch.returncode == 0, batch.stderr
    assert batch.stdout.decode() == expected + "\n", (text, batch.stdout, expected)
    previous_spans = None
    for chunk in (1, 7, 4096):
        run = subprocess.run([cli, "stream-json", "--resources", resources,
                              "--chunk-bytes", str(chunk), *segmentation, *options, "-"],
                             input=source, capture_output=True, timeout=10)
        assert run.returncode == 0, (text, run.stderr)
        events = [json.loads(line) for line in run.stdout.splitlines()]
        assert events[-1]["event"] == "end", events
        segments = events[:-1]
        assert "".join(segment["text"] for segment in segments) == expected, (text, segments)
        spans = []
        for segment in segments:
            rendered = segment["text"].encode()
            for span in segment["spans"]:
                surface = source[span["input_begin"]:span["input_end"]].decode()
                output = rendered[span["begin"]:span["end"]].decode()
                spans.append((surface, output, span))
        if previous_spans is not None:
            assert spans == previous_spans, (text, "chunk-dependent metadata")
        previous_spans = spans
        for surface, output, fields in references:
            hits = [(text, span) for raw, text, span in spans if raw == surface]
            assert len(hits) == 1, (surface, hits)
            actual_output, actual_span = hits[0]
            assert actual_output == output, (surface, actual_output, output)
            for key, value in fields.items():
                assert actual_span[key] == value, (surface, key, actual_span, value)
        checks += 1


english = {"source_lang": "en-US", "context_lang": "en-US", "render_lang": "en-US"}
korean = {"source_lang": "en-US", "context_lang": "ko-KR", "render_lang": "ko-KR"}
lexicon = {**korean, "lexicon": "lexicon:system", "pronunciation_layer": "system"}
hyphen = {**lexicon, "semantic_type": "WORD", "rule": "pack.hyphenated-word"}

for label in ("제목은", "타이틀은", "영문제목은"):
    for colon in ("", ":"):
        text = f"{label}{colon} The 1st day입니다."
        expected = f"{label}{colon} The first day입니다."
        references = (("The", "The", english), ("1st", "first", english),
                      ("day", "day", english), ("입니다", "입니다", {"context_lang": "ko-KR"}))
        check(text, expected, references)
        check(text, expected, references, tiny=True)

check("제목은 The 1st day입니다. Apple Watch를 구매했습니다.",
      "제목은 The first day입니다. 애플 워치를 구매했습니다.",
      (("1st", "first", english), ("Apple", "애플", lexicon), ("Watch", "워치", lexicon)))
check("고객은 The 1st day를 확인합니다.", "고객은 더 퍼스트 데이를 확인합니다.",
      (("The", "더", korean), ("1st", "퍼스트", korean)))
check('제목은 "Apple은 사과입니다."라고 적습니다.',
      '제목은 "애플은 사과입니다."라고 적습니다.', (("Apple", "애플", lexicon),))
check("제목은\nThe 1st day입니다.", "제목은\n더 퍼스트 데이입니다.",
      (("1st", "퍼스트", korean),))
for delimiter in (";", ":"):
    check(f"제목은 First{delimiter} Apple은 사과입니다.",
          f"제목은 First{delimiter} 애플은 사과입니다.",
          (("First", "First", english), ("Apple", "애플", lexicon)))
check("오늘의 제목은 21st입니다.", "오늘의 제목은 트웬티 퍼스트입니다.",
      (("21st", "트웬티 퍼스트", korean),))

check("새로운 real-time dashboard를 소개합니다.", "새로운 리얼타임 대시보드를 소개합니다.",
      (("real-time", "리얼타임", hyphen), ("dashboard", "대시보드", lexicon)))
check("이번 영상에서는 quasar를 설명합니다.", "이번 영상에서는 퀘이사를 설명합니다.",
      (("quasar", "퀘이사", lexicon),))
check("The real-time dashboard is ready.", "The real-time dashboard is ready.",
      (("real-time", "real-time", {**english, "rule": "pack.hyphenated-word"}),))
for compound in ("near--real-time", "x-" * 100 + "x"):
    # Invalid separators and words beyond the prototype key bound must retain
    # their complete text in English, with stable byte offsets across chunks.
    text = f"Please use {compound}."
    check(text, text)

with tempfile.TemporaryDirectory() as directory:
    override = Path(directory) / "content.tsv"
    override.write_text("context_cue\ten-US\t제목은\t0\n"
                        "context_cue\ten-US\t작품명은\t800\n"
                        "lexicon\tko-KR\ten-US:near-real-time\t니어리얼타임\n"
                        "lexicon\tko-KR\ten-US:real-time\t리얼 타임\n", encoding="utf-8")
    options = ("--customer", str(override))
    check("제목은 The 1st day입니다.", "제목은 더 퍼스트 데이입니다.",
          (("1st", "퍼스트", korean),), options)
    check("작품명은 The 1st day입니다.", "작품명은 The first day입니다.",
          (("1st", "first", english),), options, tiny=True)
    customer_lexicon = {**korean, "rule": "pack.hyphenated-word", "lexicon": "lexicon:customer",
                       "pronunciation_layer": "customer"}
    check("새로운 near-real-time을 소개합니다.", "새로운 니어리얼타임을 소개합니다.",
          (("near-real-time", "니어리얼타임", customer_lexicon),), options)
    check("REAL-TIME을 소개합니다.", "리얼 타임을 소개합니다.",
          (("REAL-TIME", "리얼 타임", customer_lexicon),), options)

# A long title must yield a phrase while stdin remains open. The final text is
# compared with the same batch configuration; this is a streaming contract,
# not an asserted pronunciation for content beyond the bounded cue horizon.
options = ("--first-target", "1", "--normal-target", "3", "--max-length", "12")
prefix = ("제목은 " + "word " * 80).encode()
suffix = ("word " * 320 + "입니다.").encode()
source = prefix + suffix
batch = subprocess.run([cli, "normalize", "--resources", resources, *options, source.decode()],
                       capture_output=True, check=True, timeout=10).stdout.decode()[:-1]
check(source.decode(), batch, tiny=True)
process = subprocess.Popen([cli, "stream-json", "--resources", resources, *options,
                            "--chunk-bytes", "4096", "-"], stdin=subprocess.PIPE,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
first_line = queue.Queue()
reader = threading.Thread(target=lambda: first_line.put(process.stdout.readline()), daemon=True)
reader.start()
try:
    process.stdin.write(prefix)
    process.stdin.flush()
    first = first_line.get(timeout=5)
    # Avoid buffered read-ahead before communicate switches to raw pipe reads.
    reader.join(timeout=1)
    first_event = json.loads(first)
    assert first_event["event"] == "segment", first_event
    assert 0 < first_event["input_end"] < len(prefix), first_event
    process.stdin.write(suffix)
    process.stdin.close()
    process.stdin = None
    remainder, error = process.communicate(timeout=10)
    assert process.returncode == 0, error
    events = [json.loads(line) for line in (first + remainder).splitlines()]
    assert events[-1]["event"] == "end", events
    assert "".join(event["text"] for event in events[:-1]) == batch
    checks += 1
finally:
    if process.poll() is None:
        process.kill()
        process.wait()
    reader.join(timeout=1)

print(f"{checks} content text/language/lexicon/stream checks passed")
