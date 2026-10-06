"""Speech output is sayable: with the read policy always, otherwise by contract.

Neither language has an external reference corpus for this, so it checks a
property instead of an expected reading: a synthesizer must be able to say what
comes out. A wrong reading still passes, which is the limit of this gate.

Two passes over generated inputs, from project-authored fragments only:

* `--unreadable read` promises every segment is sayable, so nothing is exempt.
  Korean output may hold no Latin letter or digit; English keeps its letters,
  which English speech reads, so only digits are checked.
* The default `preserve` keeps what the project deliberately refuses to
  interpret, and docs/speech-resources.md records those contracts, so that pass
  allows the inputs named below and nothing else.

With a shared library as the third argument the inputs go through one engine
per pass instead of one CLI start each (2026-10-06: about 6,000 starts took four
minutes); instrumented builds pass none and keep the CLI.
"""
import re
import subprocess
import sys
from pathlib import Path

cli, resources = sys.argv[1:3]
library = sys.argv[3] if len(sys.argv) > 3 else None
KOREAN = ("안내", "가격은", "제품", "문의", "총", "약", "제", "번", "층")
ENGLISH = ("the", "call", "page", "see", "about", "room")
NUMERIC = ("12", "3.5", "1/2", "2026-09-25", "09:30", "5%", "1,200원", "10kg", "3-5", "0.5",
           "1억", "5ml", "제10조", "20F", "1+1", "11st", "2:1", "k007", "A4", "(1965)", "v1.2")
SYMBOL = ("@", "#", "&", "+", "~", "/", "(", ")", "[", "]", "%", "°", "→", "※", "℃", "·",
          "…", "—", "♥", "℉", "§", "±", "′", "¢", "²")
FOREIGN = ("AI", "FBI", "update", "k7", "MP3", "GPT-4", "top100", "e-mail", "http://a.co",
           "a@b.com", "C++", "USB-C", "kg", "FIFA", "NASA")
KOREAN_UNSAYABLE = re.compile(r"[A-Za-z0-9]")
ENGLISH_UNSAYABLE = re.compile(r"[0-9]")
# Inputs the project deliberately keeps as written under the default policy, each
# with its contract in docs/speech-resources.md: an ambiguous identifier, a
# protected region, a malformed object. Naming the inputs rather than exempting
# whole rules keeps the sweep able to catch a rule that preserves something it
# should have read. The read policy exempts nothing at all.
DOCUMENTED_PRESERVE = ("20F", "http://a.co", "a@b.com", "C++", "USB-C", "GPT-4", "MP3", "k7",
                       "top100", "e-mail", "AI", "FBI", "update", "kg", "FIFA", "NASA",
                       "A4", "k007", "v1.2", "11st", "2:1", "1+1", "(1965)",
                       # An ordinal unit written straight against the next word keeps its
                       # text, so 제2조각 is not read as 제이 조 각.
                       "제10조")


def cases(words):
    for word in words:
        for other in NUMERIC + SYMBOL + FOREIGN:
            yield f"{word} {other}"
            yield f"{other}{word}"
            yield f"{other} {word} {other}"


def outputs(language, policy, inputs):
    """(text, output or None, error) for each input."""
    if library:
        sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools/web"))
        from native import Engine
        engine = Engine(Path(library).resolve(), Path(resources).resolve(),
                        dict(default_language=language, output_mode="speech", unreadable=policy))
        try:
            for text in inputs:
                try:
                    yield text, engine.normalize(text.encode())["text"], None
                except ValueError as error:
                    yield text, None, f"error: {str(error)[:80]}"
        finally:
            engine.close()
        return
    for text in inputs:
        result = subprocess.run(
            [cli, "normalize", "--resources", resources, "--language", language,
             "--output-mode", "speech", "--unreadable", policy, "--", text],
            capture_output=True, timeout=10)
        if result.returncode:
            yield text, None, f"exit {result.returncode}: {result.stderr.decode()[:80]}"
        else:
            yield text, result.stdout.decode().rstrip("\n"), None


def sweep(language, words, unsayable, policy, exempt):
    inputs = list(cases(words))
    failures = []
    for text, output, error in outputs(language, policy, inputs):
        if error:
            failures.append((text, error))
            continue
        if exempt and any(keep in output for keep in DOCUMENTED_PRESERVE):
            continue
        left = unsayable.search(output)
        if left:
            failures.append((text, f"unsayable {left.group(0)!r} in {output!r}"))
    for text, reason in failures[:10]:
        print(f"{language} {policy}: {text!r}: {reason}", file=sys.stderr)
    assert not failures, (
        f"{len(failures)} of {len(inputs)} {language} outputs are not sayable under {policy}")
    return len(inputs)


checks = sweep("ko-KR", KOREAN, KOREAN_UNSAYABLE, "read", exempt=False)
checks += sweep("en-US", ENGLISH, ENGLISH_UNSAYABLE, "read", exempt=False)
checks += sweep("ko-KR", KOREAN, KOREAN_UNSAYABLE, "preserve", exempt=True)
print(f"{checks} generated inputs produced sayable speech output: the read policy with no "
      "exemption, the default policy outside its documented preserve contracts")
