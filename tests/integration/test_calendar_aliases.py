"""Calendar labels: frozen references, byte splits, bracket and resource contracts."""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/web"))
from native import Engine, flatten

library, resources = [Path(p).resolve() for p in sys.argv[1:3]]
corpus = [json.loads(line) for line in
          (ROOT / "tests/evaluation/calendar_alias_v21.jsonl").read_text().splitlines()]
checks = 0
for profile in ({}, dict(first_target=1, normal_target=3, max_length=12)):
    for language in ("ko-KR", "en-US"):
        for mode in ("text", "speech"):
            engine = Engine(library, resources, dict(profile, default_language=language,
                            output_mode=mode, detailed_trace=True))
            try:
                for case in corpus:
                    if (case["default_language"], case["output_mode"]) != (language, mode):
                        continue
                    source = case["text"].encode()
                    batch = engine.normalize(source)
                    assert batch["text"] == case["expected"], (case["id"], profile, batch)
                    if case["id"].startswith("calendar-alias-v21-ko-"):
                        weekday, = [s for s in batch["spans"] if s["unit"] == "weekday"]
                        assert (weekday["source_lang"], weekday["context_lang"],
                                weekday["render_lang"]) == ("en-US", "ko-KR", "ko-KR"), weekday
                    for cut in range(1, len(source)):
                        streamed = flatten(engine.stream(source, [cut, len(source) - cut]))
                        assert streamed == batch, (case["id"], profile, cut, streamed, batch)
                        checks += 1
            finally:
                engine.close()


def run_cases(options, cases):
    engine = Engine(library, resources, dict(default_language="en-US", output_mode="speech",
                                           detailed_trace=True, **options))
    try:
        results = []
        for text, expected in cases:
            source = text.encode()
            result = engine.normalize(source)
            assert result["text"] == expected, (text, result)
            assert flatten(engine.stream(source, [1] * len(source))) == result
            results.append(result)
        return results
    finally:
        engine.close()


result, = run_cases({}, [("5/8(Fri.)", "May eighth(Friday)")])
weekday, = [s for s in result["spans"] if s["unit"] == "weekday"]
assert (weekday["semantic_type"], weekday["value"], weekday["rule"],
        weekday["source_lang"], weekday["context_lang"], weekday["render_lang"],
        weekday["surface"], weekday["input_begin"], weekday["input_end"]) == (
        "DATE", "5", "en.date.weekday", "en-US", "en-US", "en-US", "Fri.", 4, 8), weekday

# Parentheses remain independent so they can be preserved or skipped.
run_cases(dict(brackets="preserve"), [("5/8(Fri.)", "May eighth(Fri.)")])
run_cases(dict(brackets="skip"), [("5/8(Fri.)", "May eighth")])

# Customer output labels and new input aliases are separate resources.
result, = run_cases(dict(customer_tsv="weekday\ten-US\t5\tFriday closing\n"
                       "weekday_label\ten-US\tfridayclose\t5\n"),
                    [("5/8(FridayClose)", "May eighth(Friday closing)")])
weekday, = [s for s in result["spans"] if s["unit"] == "weekday"]
assert weekday["resource_layer"] == "customer", weekday
event, = [e for e in result["trace"] if e["kind"] == "weekday" and e["key"] == "5"]
assert event["layer"] == "customer" and event["disposition"] == "selected", event

# Disabling one abbreviation must not disable its full-name sibling.
run_cases(dict(customer_tsv="weekday_label\ten-US\tfri\t0\n"),
          [("5/8(Fri)", "five eighths(Fri)"),  # was five over eight (fractions, 2026-10-07)
           ("5/8(Friday)", "May eighth(Friday)")])

# The supported 31-byte key also accepts the optional abbreviation period.
alias = "f" * 31
run_cases(dict(customer_tsv=f"weekday_label\ten-US\t{alias}\t5\n",
               first_target=1, normal_target=3, max_length=12),
          [(f"5/8({alias}.)", "May eighth(Friday)")])

# Invalid dates, identifiers and ordinary words supply no calendar evidence.
engine = Engine(library, resources, dict(default_language="en-US", output_mode="speech",
                                        detailed_trace=True))
try:
    for text in ("5/32(Fri)", "February 30, 2026(Fri)", "2026(Fri)",
                 "May 2026(Fri)", "code2026-05-08(Fri)", "sun(Fri)",
                 "5/8(Fri!)", "5/8 (Fri)"):
        source = text.encode()
        result = engine.normalize(source)
        assert not any(s["unit"] == "weekday" for s in result["spans"]), (text, result)
        assert flatten(engine.stream(source, [1] * len(source))) == result, text
finally:
    engine.close()
print(f"{checks} UTF-8 split/text/span/trace comparisons; resource and bracket contracts passed")
