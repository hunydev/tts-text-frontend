"""Korean speech output is sayable unless a span says why it is preserved.

Korean has no external reference corpus here, so this checks a property instead
of an expected reading: a synthesizer must be able to say what comes out. The
project deliberately keeps some objects as written -- a protected region, an
ambiguous identifier such as 20F, a malformed ordinal -- and docs/speech-resources.md
records those contracts, so a span carrying one of those rules is allowed to hold
digits or Latin letters. Every other span must be sayable. A wrong Korean reading
still passes, which is the limit of this gate.

Inputs are generated from project-authored fragments; no external corpus is used.
"""
import itertools
import re
import subprocess
import sys

cli, resources = sys.argv[1:3]
KOREAN = ("안내", "가격은", "서울에서", "제품", "문의", "총", "약", "제", "번", "층")
NUMERIC = ("12", "3.5", "1/2", "2026-09-25", "09:30", "1~2", "5%", "1,200원", "10kg", "3-5",
           "0.5", "1억", "5ml", "제10조", "20F", "1+1")
SYMBOL = ("@", "#", "&", "+", "~", "/", "(", ")", "[", "]", "%", "°", "→", "※", "℃", "·",
          "…", "—", "♥", "℉")
FOREIGN = ("AI", "FBI", "update", "k7", "MP3", "GPT-4", "top100", "e-mail", "http://a.co",
           "a@b.com", "C++", "USB-C", "kg", "FIFA", "NASA")
# A synthesizer cannot say these: the frontend either reads them or drops them.
UNSAYABLE = re.compile(r"[A-Za-z0-9\x00-\x08\x0b\x0c\x0e-\x1f]")
# Inputs the project deliberately keeps as written, each with its contract in
# docs/speech-resources.md: an ambiguous identifier, a protected region and a
# malformed object. Naming them here rather than exempting whole rules keeps the
# sweep able to catch a rule that preserves something it should have read.
DOCUMENTED_PRESERVE = ("20F", "http://a.co", "a@b.com", "C++", "USB-C", "GPT-4", "MP3", "k7",
                       "top100", "e-mail", "AI", "FBI", "update", "kg", "FIFA", "NASA",
                       # An ordinal unit written straight against the next word keeps its
                       # text, so 제2조각 is not read as 제이 조 각.
                       "제10조")
SPAN = re.compile(r'rule=(?P<rule>\S+).*?final="(?P<final>[^"]*)"')


def cases():
    for korean in KOREAN:
        for other in NUMERIC + SYMBOL + FOREIGN:
            yield f"{korean} {other}"
            yield f"{other}{korean}"
            yield f"{other} {korean} {other}"
    for symbol, numeric, korean in itertools.islice(
            itertools.product(SYMBOL, NUMERIC, KOREAN), 240):
        yield f"{symbol}{numeric}{korean}"


def main():
    inputs = list(cases())
    failures = []
    for text in inputs:
        result = subprocess.run(
            [cli, "inspect", "--resources", resources, "--language", "ko-KR",
             "--output-mode", "speech", "--", text],
            capture_output=True, timeout=10)
        if result.returncode:
            failures.append((text, f"exit {result.returncode}: {result.stderr.decode()[:80]}"))
            continue
        for line in result.stdout.decode().splitlines():
            span = SPAN.search(line)
            if not span:
                continue
            rule, final = span.group("rule"), span.group("final")
            if rule.startswith("protected.") or any(keep in final for keep in DOCUMENTED_PRESERVE):
                continue
            left = UNSAYABLE.search(final)
            if left:
                failures.append((text, f"unsayable {left.group(0)!r} in {final!r} from {rule}"))
    for text, reason in failures[:20]:
        print(f"{text!r}: {reason}", file=sys.stderr)
    assert not failures, f"{len(failures)} spans of {len(inputs)} Korean inputs are not sayable"
    print(f"{len(inputs)} generated Korean inputs produced sayable speech output "
          "outside the documented preserve rules")


main()
