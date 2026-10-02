"""Particle resources: byte offsets, trace provenance and customer/user precedence.

Read the independently authored corpus; never derive references from the table.
The shared library tests every two-part UTF-8 split under two segment profiles.
"""
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/web"))
from native import Engine as _Engine, flatten
# Particle agreement is off by default since 2026-10-02; these expectations include it.
def Engine(lib, res, opts=None, *a, **k):
    return _Engine(lib, res, dict({"particle_agreement": "on"}, **(opts or {})), *a, **k)

library, resources = map(lambda p: Path(p).resolve(), sys.argv[1:3])
corpus = [json.loads(line) for line in
          (ROOT / "tests/evaluation/particle_table_v21.jsonl").read_text().splitlines()]
checks = 0
for profile in ({}, dict(first_target=1, normal_target=3, max_length=12)):
    for mode in ("speech", "text"):
        engine = Engine(library, resources, dict(profile, output_mode=mode, detailed_trace=True))
        try:
            for case in corpus:
                if case["output_mode"] != mode:
                    continue
                source = case["text"].encode()
                batch = engine.normalize(source)
                assert batch["text"] == case["expected"], (case["id"], profile, batch)
                for cut in range(1, len(source)):
                    streamed = flatten(engine.stream(source, [cut, len(source) - cut]))
                    assert streamed == batch, (case["id"], profile, cut, streamed, batch)
                    checks += 1
        finally:
            engine.close()


def selected(result, key, layer):
    events = [event for event in result["trace"] if event["kind"] == "particle_form"
              and event["key"] == key and event["disposition"] == "selected"]
    assert len(events) == 1 and events[0]["layer"] == layer, events
    return events[0]


engine = Engine(library, resources, dict(output_mode="speech", detailed_trace=True))
try:
    result = engine.normalize("1만원라도 보태세요".encode())
    money = [span for span in result["spans"] if span["semantic_type"] == "CURRENCY"]
    assert len(money) == 1 and money[0]["value"] == "10000" and money[0]["unit"] == "KRW", money
    event = selected(result, "라도", "system")
    assert (event["input_begin"], event["input_end"]) == (7, 13), event
finally:
    engine.close()

# A customer may choose a different connective. The consumed resource and
# its layer must be visible, including in callbacks.
engine = Engine(library, resources, dict(output_mode="speech", detailed_trace=True,
    customer_tsv="particle_form\tko-KR\t랑\t하고|하고\n"))
try:
    source = "3랑".encode()
    result = engine.normalize(source)
    assert result["text"] == "삼하고", result
    selected(result, "랑", "customer")
    assert flatten(engine.stream(source, [1] * len(source))) == result
finally:
    engine.close()

# Agreement follows the chosen user reading, not the spelling or system lexicon.
engine = Engine(library, resources, dict(output_mode="speech", detailed_trace=True,
    user_lexicon=[dict(word="Apple", pronunciation="사과")]))
try:
    source = "Apple이랑".encode()
    result = engine.normalize(source)
    assert result["text"] == "사과랑", result
    selected(result, "이랑", "system")
    assert flatten(engine.stream(source, [1] * len(source))) == result
finally:
    engine.close()

# A character-name substitution is not a classified noun or a pronunciation
# lexicon hit. It must remain local, even next to a particle-shaped syllable.
engine = Engine(library, resources, dict(output_mode="speech", detailed_trace=True,
    customer_tsv="speech_character\tko-KR\t漢\t한\nspeech_character\tko-KR\t1️⃣\t키캡일\n"))
try:
    for text, expected in [("가漢나", "가한나"), ("漢나", "한나"), ("가漢이", "가한이"),
                           ("가1️⃣나", "가키캡일나")]:
        source = text.encode()
        result = engine.normalize(source)
        assert result["text"] == expected, result
        assert not any(event["kind"] == "particle_form" for event in result["trace"]), result
        for cut in range(1, len(source)):
            assert flatten(engine.stream(source, [cut, len(source) - cut])) == result
finally:
    engine.close()
print(f"{checks} corpus UTF-8 split/metadata/trace comparisons; quantity, override and character-locality contracts passed")
