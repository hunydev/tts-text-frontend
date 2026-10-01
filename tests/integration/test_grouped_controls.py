"""Fixed text references, whole-source metadata, streaming and reading overrides."""
import hashlib
import json
from pathlib import Path
import random
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/web"), str(ROOT / "tools/evaluation")]
from native import Engine, flatten
from evaluate import frontend_settings

library, resources = (Path(arg).resolve() for arg in sys.argv[1:])
corpus = ROOT / "tests/evaluation/grouped_controls_v22.jsonl"
assert hashlib.sha256(corpus.read_bytes()).hexdigest() == "aed54be33c8879afdfc8c3105e06a4091427a1cc40561a4cac4628a850362395"
rows = [json.loads(line) for line in corpus.read_text(encoding="utf-8").splitlines()]
checks = 0
rng = random.Random(220928)
for profile in ({}, dict(first_target=1, normal_target=3, max_length=12)):
    engines = {}
    try:
        for row in rows:
            options = dict(frontend_settings(row), **profile,
                           default_language=row["default_language"], detailed_trace=True)
            key = json.dumps(options, sort_keys=True)
            if key not in engines:
                engines[key] = Engine(library, resources, options)
            engine = engines[key]
            raw = row["text"].encode()
            batch = engine.normalize(raw)
            assert batch["text"] == row["expected"], (row["id"], batch)
            if row["category"] != "controls/legacy":
                span = batch["spans"][0]
                end = raw.index(b"]]{@") + len(row["category"].split("/")[1]) + 5
                assert (span["input_begin"], span["input_end"]) == (0, end), span
                assert span["semantic_type"] == "CUSTOM" and span["rule"] == "control.pattern", span
                assert span["secondary"] == "control-surface", span
                source = row["text"][2:row["text"].index("]]{@")]
                # Escapes are decoded; the full input-control trace carries the
                # chosen source when it cannot fit the fixed ABI v1 value field.
                source = source.replace(r"\[", "[").replace(r"\]", "]")
                assert span["value"] == (source if len(source.encode()) < 96 else ""), span
                event, = [e for e in batch["trace"] if e["kind"] == "input-control"]
                assert event["text"] == source and event["key"] == span["unit"], event
            schedules = [[cut, len(raw) - cut] for cut in range(1, len(raw))]
            schedules.append([1] * len(raw))
            schedule, remaining = [], len(raw)
            while remaining:
                size = rng.randint(1, min(remaining, 11))
                schedule.append(size)
                remaining -= size
            schedules.append(schedule)
            for schedule in schedules:
                assert flatten(engine.stream(raw, schedule)) == batch, (row["id"], schedule)
                checks += 1
    finally:
        for engine in engines.values():
            engine.close()


def check(text, expected, **options):
    engine = Engine(library, resources, dict(input_format="annotated", output_mode="speech",
                                            detailed_trace=True, **options))
    try:
        raw = text.encode()
        result = engine.normalize(raw)
        assert result["text"] == expected, result
        assert flatten(engine.stream(raw, [1] * len(raw))) == result
        return result
    finally:
        engine.close()


result = check("[[서울시 12-3]]{@address}", "서울시 일이 번에 삼", customer_tsv=
               "address_policy\tko-KR\tnumber\tdigits\naddress_policy\tko-KR\thyphen\t번에\n")
assert any(e["kind"] == "address_policy" and e["key"] == "hyphen" and
           e["layer"] == "customer" for e in result["trace"]), result
check("[[a1@b2.com]]{@email}", "에이일 앳 비이 닷 닷컴",
      customer_tsv="electronic_word\tko-KR\tcom\t닷컴\n")
check("[[a1@foo.com]]{@email}", "에이일 앳 고객명 닷 컴",
      user_lexicon=[dict(word="foo", pronunciation="고객명")])
check("1{첫}번째 [[서울시 12-3]]{@address}입니다.", "첫번째 서울시 십이 다시 삼입니다.")
check("[[서울시 12-3]]{@address}", "서울시 십이 다시 삼", brackets="skip")
check("`[[서울시 12-3]]{@address}`", "`[[서울시 12-3]]{@address}`")
check("q1{@url}", "큐일")

# Incomplete controls cannot emit their source; a completed earlier sentence can.
# invalid_control=error keeps the strict failure under test (default: ignore).
engine = Engine(library, resources, dict(input_format="annotated", output_mode="speech",
                                         invalid_control="error"))
try:
    prefix = "먼저 안내합니다.\n".encode()
    raw = prefix + "[[서울시 12-3]]{@address}".encode()
    events = []
    streamed = engine.stream(raw, [len(prefix), 5, 7, 1, len(raw)], emit=events.append)
    assert flatten(streamed)["text"] == "먼저 안내합니다.\n서울시 십이 다시 삼"
    segments = [e for e in events if e["event"] == "segment"]
    assert segments[0]["stage"] == "write" and segments[0]["input_end"] <= len(prefix), segments
    events = []
    try:
        bad = prefix + b"[[a1@]]{@email}"
        engine.stream(bad, [len(prefix), len(bad)], emit=events.append)
        raise AssertionError("invalid email was accepted")
    except ValueError as error:
        assert "invalid argument" in str(error), error
    assert "".join(e["text"] for e in events if e["event"] == "segment") == prefix.decode(), events
finally:
    engine.close()
print(f"{checks} UTF-8 split/text/span/trace comparisons; full source, overrides, protection and callback commitment passed")
