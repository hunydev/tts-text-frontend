"""Character-local output, provenance, protected spans and byte streaming.

Expected text comes from a corpus frozen before resource edits, not from TSVs.
"""
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
corpus = ROOT / "tests/evaluation/speech_character_width_v21.jsonl"
assert hashlib.sha256(corpus.read_bytes()).hexdigest() == "f9bd0134d1905ffe2e19456f1c636794e748d01f8ea6463a3bb793a5ffbec726"
rows = [json.loads(line) for line in corpus.read_text(encoding="utf-8").splitlines()]
checks = 0
rng = random.Random(210927)
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
            schedules = [[cut, len(raw) - cut] for cut in range(1, len(raw))]
            schedules.append([1] * len(raw))
            schedule, remaining = [], len(raw)
            while remaining:
                size = rng.randint(1, min(remaining, 7))
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
    engine = Engine(library, resources, dict(output_mode="speech", detailed_trace=True, **options))
    try:
        raw = text.encode()
        result = engine.normalize(raw)
        assert result["text"] == expected, result
        assert flatten(engine.stream(raw, [1] * len(raw))) == result
        return result
    finally:
        engine.close()


result = check("계좌 ＡＢＣ 확인", "계좌 에이비씨 확인")
span, = [span for span in result["spans"] if span["surface"] == "ＡＢＣ"]
assert (span["input_begin"], span["input_end"]) == (7, 16), span
assert (span["script"], span["source_lang"], span["context_lang"]) == ("OTHER", "und", "ko-KR"), span
events = [event for event in result["trace"] if event["kind"] == "speech_character"]
assert [(e["key"], e["value"], e["part_begin"], e["part_end"]) for e in events] == [
    ("Ａ", "에이", 0, 3), ("Ｂ", "비", 3, 6), ("Ｃ", "씨", 6, 9)], events
assert all(e["stage"] == "policy" and e["layer"] == "system" and
           e["disposition"] == "selected" for e in events), events

result = check("가Ａ나 Ｃ가", "가아나 씨가",
               customer_tsv="speech_character\tko-KR\tＡ\t아\n")
assert any(e["kind"] == "speech_character" and e["key"] == "Ａ" and
           e["value"] == "아" and e["layer"] == "customer" for e in result["trace"]), result
assert not any(e["kind"] == "particle_form" for e in result["trace"]), result
# Self-mapping is the documented way to preserve an individual character.
# It must not recurse or lose the override under unreadable=drop.
check("가Ａ나", "가Ａ나", unreadable="drop", customer_tsv="speech_character\tko-KR\tＡ\tＡ\n")
check("가Ａ나", "가에이나", unreadable="drop")
check("가Ａ나", "가에이나", unreadable="read")
check("[ＡＢＣ]", "[ＡＢＣ]", brackets="preserve")
check("Ａ{Ａ}", "Ａ", input_format="annotated")
check("ＡＢＣ{브랜드}", "브랜드", input_format="annotated")
# Replacements are final text, even if they contain another mapped glyph.
check("Ａ", "Ｂ", customer_tsv="speech_character\tko-KR\tＡ\tＢ\n")
print(f"{checks} UTF-8 split/text/span/trace comparisons; locality, offsets, overrides and protection passed")
