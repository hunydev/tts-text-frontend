"""Frozen 86-case text/metadata/trace parity across every byte split and schedules."""
import hashlib
import json
from pathlib import Path
import random
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/web"), str(ROOT / "tools/evaluation"),
                str(ROOT / "tests/integration")]
from native import Engine, flatten
from evaluate import frontend_settings, read_corpus
from test_cjk_reading import CORPUS, SHA256

library, resources = (Path(s).resolve() for s in sys.argv[1:])
assert hashlib.sha256(CORPUS.read_bytes()).hexdigest() == SHA256
rng = random.Random(220927)
checks = 0
for profile in ({}, dict(first_target=1, normal_target=3, max_length=12)):
    engines = {}
    try:
        for row in read_corpus(CORPUS):
            opts = dict(frontend_settings(row), default_language=row["default_language"],
                        detailed_trace=True, **profile)
            key = json.dumps(opts, sort_keys=True)
            if key not in engines:
                engines[key] = Engine(library, resources, opts)
            engine = engines[key]
            raw = row["text"].encode()
            batch = engine.normalize(raw)
            assert batch["text"] == row["expected"], (row["id"], batch)
            schedules = [[cut, len(raw)-cut] for cut in range(1, len(raw))]
            schedules.append([1] * len(raw))
            for _ in range(4):
                sizes, remaining = [], len(raw)
                while remaining:
                    size = rng.randint(1, min(remaining, 11))
                    sizes.append(size)
                    remaining -= size
                schedules.append(sizes)
            for sizes in schedules:
                assert flatten(engine.stream(raw, sizes)) == batch, (row["id"], profile, sizes)
                checks += 1
    finally:
        for engine in engines.values():
            engine.close()

# Actual incremental callback, including source offsets; both a phrase and a sentence.
for profile, separator in [(dict(first_target=1, normal_target=3, max_length=12), " "),
                           (dict(phrase_split=False), ". ")]:
    engine = Engine(library, resources, dict(profile, hanja="korean", kana="korean",
        detailed_trace=True, customer_tsv="cjk_word\tko-KR\t男女老少音樂\t모두음악\n"))
    try:
        first = ("男女老少音樂" + separator).encode()
        raw = first + "다음 문장입니다.".encode()
        events = []
        segments = engine.stream(raw, [9, len(first)-9+3, len(raw)], emit=events.append)
        assert flatten(segments) == engine.normalize(raw)
        early = [e for e in events if e["event"] == "segment" and e["stage"] == "write"]
        assert early and early[0]["write_index"] == 1, events
        assert early[0]["text"].startswith("모두음악")
        assert early[0]["input_begin"] == 0 and early[0]["input_end"] <= len(first)
    finally:
        engine.close()
print(f"86 contracts, 2 segment profiles: {checks} split/schedule text+metadata+trace comparisons; early callbacks passed")
