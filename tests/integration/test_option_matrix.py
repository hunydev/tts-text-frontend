"""Every frontend option combination runs, and batch equals the stream.

The six frontend options are independent settings, so a change to one must not
break another, and a byte-by-byte stream must still produce what one call
produces. This walks the whole matrix rather than the combinations someone
thought of. It also checks where `--unreadable read` stops: the promise is that
every segment is sayable, but `--unknown-foreign preserve` asks for a word to be
kept as written, which is what a caller does when a bilingual voice will read it,
so that word stays Latin and read says everything else.

Inputs are project-authored fragments; no external corpus is used.
"""
import itertools
import json
import re
import subprocess
import sys

cli, resources = sys.argv[1:3]
OPTIONS = (
    ("--output-mode", ("text", "speech")),
    ("--unreadable", ("preserve", "drop", "read")),
    ("--brackets", ("read", "skip", "preserve")),
    ("--unknown-foreign", ("guess", "spell", "preserve")),
    ("--decorative", ("follow", "read")),
    ("--initialism", ("spell", "word")),
)
TEXTS = (("ko-KR", "FBI 안내 (비고) 20F update 🙂 1+1"),
         ("ko-KR", "제 10kg 문의 12 , k007 · 5‰"),
         ("en-US", "the FBI ( note ) 20F update 1+1"))
LATIN_OR_DIGIT = re.compile(r"[A-Za-z0-9]")


def run(language, text, args, stream=False):
    command = [cli, "stream-json" if stream else "normalize", "--resources", resources,
               "--language", language, *args]
    if stream:
        command += ["--chunk-bytes", "1", "-"]
        result = subprocess.run(command, input=text.encode(), capture_output=True, timeout=20)
    else:
        result = subprocess.run(command + ["--", text], capture_output=True, timeout=20)
    assert not result.returncode, (language, args, stream, result.stderr.decode()[:200])
    if not stream:
        return result.stdout.decode().rstrip("\n")
    events = [json.loads(line) for line in result.stdout.decode().splitlines()]
    return "".join(e["text"] for e in events if e.get("event") == "segment")


checks = 0
for values in itertools.product(*(choices for _, choices in OPTIONS)):
    args = [part for (flag, _), value in zip(OPTIONS, values) for part in (flag, value)]
    settings = dict(zip((flag for flag, _ in OPTIONS), values))
    for language, text in TEXTS:
        batch = run(language, text, args)
        assert batch == run(language, text, args, stream=True), (language, args, batch)
        checks += 2
        if (language != "ko-KR" or settings["--output-mode"] != "speech"
                or settings["--unreadable"] != "read"):
            continue
        # read says everything it is allowed to say; a preserved foreign word is
        # the one thing the caller asked to keep.
        if settings["--unknown-foreign"] == "preserve":
            remainder = batch.replace("update", "")
        else:
            remainder = batch
        left = LATIN_OR_DIGIT.search(remainder)
        assert not left, (language, args, f"unsayable {left.group(0)!r} in {batch!r}")
        checks += 1

print(f"{checks} option-matrix checks passed over "
      f"{len(list(itertools.product(*(c for _, c in OPTIONS))))} combinations")
