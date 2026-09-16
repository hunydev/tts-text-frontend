"""Financial amounts: independently calculated values and complete byte spans."""
from decimal import Decimal
import json
import random
import subprocess
import sys

cli, resources = sys.argv[1:]
rng = random.Random(104729)
cases = [
    ("1억2천3백만원", "123000000"),
    ("1억 2,500만원", "125000000"),
    ("2조3억4만원", "2000300040000"),
    ("-1억2천만원", "-120000000"),
    ("1억2.5천만원", "125000000"),
]
for index in range(40):
    large = rng.randrange(1, 100)
    thousands, hundreds, tens, ones = [rng.randrange(1, 10) for _ in range(4)]
    small = thousands * 1000 + hundreds * 100 + tens * 10 + ones
    space = ("", " ", " \t ")[index % 3]
    expression = space.join((f"{large}억", f"{thousands}천", f"{hundreds}백",
                             f"{tens}십", f"{ones}만원"))
    cases.append((expression, str(large * 100_000_000 + small * 10_000)))
    fraction = Decimal(rng.randrange(1, 9999)) / 1000
    expression = f"{large}억{space}{fraction}만원"
    cases.append((expression, str(large * 100_000_000 + int(fraction * 10_000))))

checks = 0
for expression, expected_value in cases:
    canonical = subprocess.run([cli, "normalize", "--resources", resources, "--", expected_value + "원"],
                               capture_output=True, check=True, timeout=10).stdout[:-1]
    for chunk in (1, 64):
        command = [cli, "stream-json", "--resources", resources, "--chunk-bytes", str(chunk),
                   "--first-target", "1", "--normal-target", "3", "--max-length", "12", "-"]
        run = subprocess.run(command, input=expression.encode(), capture_output=True, timeout=10)
        assert run.returncode == 0, (expression, run.stderr)
        events = [json.loads(line) for line in run.stdout.decode().splitlines()]
        segments = [event for event in events if event["event"] == "segment"]
        assert events[-1]["event"] == "end", expression
        assert len(segments) == 1 and len(segments[0]["spans"]) == 1, (expression, segments)
        span = segments[0]["spans"][0]
        assert span["semantic_type"] == "CURRENCY" and span["unit"] == "KRW", (expression, span)
        assert span["value"] == expected_value, (expression, expected_value, span)
        assert (span["input_begin"], span["input_end"]) == (0, len(expression.encode())), span
        assert segments[0]["text"].encode() == canonical, (expression, segments, canonical)
        checks += 1

for expression, numeric in (("3백과사전", "3"), ("3천국", "3"), ("1.5천국", "1.5")):
    run = subprocess.run([cli, "stream-json", "--resources", resources, "-"],
                         input=expression.encode(), capture_output=True, check=True, timeout=10)
    event = json.loads(run.stdout.decode().splitlines()[0])
    assert event["spans"][0]["value"] == numeric, (expression, event)
    assert event["spans"][0]["input_end"] == len(numeric), (expression, event)
    assert event["spans"][1]["semantic_type"] == "WORD", (expression, event)

print(f"{len(cases)} independently calculated compound amounts; {checks} value/span/chunk checks "
      "and 3 ordinary-word metadata guards passed")
