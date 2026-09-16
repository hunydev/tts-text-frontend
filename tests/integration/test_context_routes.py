"""Independent text and language-route references for unquoted code mixing."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

cli, resources = sys.argv[1:]
cases = [
    ("한국어로 Please go to the 1st floor라고 말합니다.",
     "한국어로 Please go to the first floor라고 말합니다.",
     [("Please", "en-US", "Please"), ("1st", "en-US", "first"),
      ("floor", "en-US", "floor")]),
    ("영문 공지에는 Apple released the iPhone.이라고 적혀 있습니다.",
     "영문 공지에는 Apple released the iPhone.이라고 적혀 있습니다.",
     [("Apple", "en-US", "Apple"), ("iPhone", "en-US", "iPhone")]),
    ("고객은 Apple Watch를 구매했습니다.", "고객은 애플 워치를 구매했습니다.",
     [("Apple", "ko-KR", "애플"), ("Watch", "ko-KR", "워치")]),
    ("GPU and CPU를 사용합니다.", "지피유 앤드 씨피유를 사용합니다.",
     [("GPU", "ko-KR", "지피유"), ("and", "ko-KR", "앤드"),
      ("CPU", "ko-KR", "씨피유")]),
    ("GPU and CPU are ready라고 표시합니다.", "GPU and CPU are ready라고 표시합니다.",
     [("GPU", "en-US", "GPU"), ("CPU", "en-US", "CPU"), ("ready", "en-US", "ready")]),
    ("고객은 the cloud server를 확인했습니다.", "고객은 더 클라우드 서버를 확인했습니다.",
     [("the", "ko-KR", "더"), ("cloud", "ko-KR", "클라우드"),
      ("server", "ko-KR", "서버")]),
    ("화면에는 Enter the 3rd digit라고 나옵니다.", "화면에는 Enter the third digit라고 나옵니다.",
     [("Enter", "en-US", "Enter"), ("3rd", "en-US", "third"), ("digit", "en-US", "digit")]),
    ("The total is $12.", "The total is twelve dollars.",
     [("$12", "en-US", "twelve dollars", "und")]),
    ("The rate is -3.5%.", "The rate is minus three point five percent.",
     [("-3.5%", "en-US", "minus three point five percent", "und")]),
    ("Please pay ₩12,500.", "Please pay twelve thousand five hundred won.",
     [("₩12,500", "en-US", "twelve thousand five hundred won", "und")]),
    ("The number is +3.", "The number is plus three.",
     [("+3", "en-US", "plus three", "und")]),
    ("The rate is .5%.", "The rate is zero point five percent.",
     [(".5%", "en-US", "zero point five percent", "und")]),
    ("$12,500 is the total.", "twelve thousand five hundred dollars is the total.",
     [("$12,500", "en-US", "twelve thousand five hundred dollars", "und")]),
    ("-3.5% is the annual rate.", "minus three point five percent is the annual rate.",
     [("-3.5%", "en-US", "minus three point five percent", "und")]),
    ("₩12,500 is the price.", "twelve thousand five hundred won is the price.",
     [("₩12,500", "en-US", "twelve thousand five hundred won", "und")]),
    ("32 °C", "삼십이 도", [("32 °C", "ko-KR", "삼십이 도", "und")]),
    ("32 °C is the temperature.", "thirty two degrees Celsius is the temperature.",
     [("32 °C", "en-US", "thirty two degrees Celsius", "und")]),
]
checks = 0


def check(text, expected, references, chunk, options=()):
    global checks
    source = text.encode()
    command = [cli, "stream-json", "--resources", resources,
               "--chunk-bytes", str(chunk), *options, "-"]
    run = subprocess.run(command, input=source, capture_output=True, timeout=10)
    assert run.returncode == 0, (text, run.stderr)
    events = [json.loads(line) for line in run.stdout.splitlines()]
    assert events[-1]["event"] == "end", events
    segments = events[:-1]
    assert "".join(segment["text"] for segment in segments) == expected, (text, segments)
    actual = []
    for segment in segments:
        output = segment["text"].encode()
        for span in segment["spans"]:
            surface = source[span["input_begin"]:span["input_end"]].decode()
            normalized = output[span["begin"]:span["end"]].decode()
            actual.append((surface, span, normalized))
    for reference in references:
        surface, context, normalized = reference[:3]
        source_lang = reference[3] if len(reference) == 4 else "en-US"
        matches = [(span, text) for raw, span, text in actual if raw == surface]
        assert len(matches) == 1, (surface, matches)
        span, output = matches[0]
        assert span["source_lang"] == source_lang, (surface, span)
        assert span["context_lang"] == span["render_lang"] == context, (surface, span)
        assert output == normalized, (surface, output, normalized)
        assert "en-US=" in span["context_trace"] and "ko-KR=" in span["context_trace"], span
        checks += 1


for text, expected, references in cases:
    for chunk in (1, 7, 4096):
        check(text, expected, references, chunk)

# A data-only customer override can reclassify an ambiguous surface as a verb;
# the source spelling stays English without changing a native library.
with tempfile.TemporaryDirectory() as directory:
    override = Path(directory) / "context.tsv"
    override.write_text("context_role\ten-US\twatch\tfinite\n", encoding="utf-8")
    check("고객은 Apple Watch를 구매했습니다.", "고객은 Apple Watch를 구매했습니다.",
          [("Apple", "en-US", "Apple"), ("Watch", "en-US", "Watch")], 1,
          ("--customer", str(override)))

# A configured default is used for absent signals; attached Korean grammar still
# supplies explicit context when the configured default is English.
check("Apple은 사과입니다.", "애플은 사과입니다.", [("Apple", "ko-KR", "애플")], 1,
      ("--language", "en-US"))
print(f"{len(cases)} independent routing cases; {checks} language/text/trace checks passed")
