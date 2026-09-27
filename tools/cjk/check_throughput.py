"""Section 2.4/9 gate: no-CJK throughput, including 18k indexed word rows.

Frozen before/after libraries are timed via untraced native calls. Setup/loading,
Python metadata conversion and warmups are excluded. Median of 7 alternating
rounds (3 normalizations each), threshold fixed at 1.25 before measurement.
"""
import argparse
import ctypes as C
import json
from pathlib import Path
import statistics
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/web"))
from native import Engine, Result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("before", "before-resources", "after", "after-resources", "report"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    # No CJK, Latin or numbers; policy/resource additions must not penalize it.
    raw = ("오늘 시장이 열렸습니다. 고객에게 자세한 내용을 안내합니다. " * 164).encode()
    engines = []
    try:
        engines.append(Engine(args.before.resolve(), args.before_resources.resolve(), {}))
        engines.append(Engine(args.after.resolve(), args.after_resources.resolve(), {}))
        engines.append(Engine(args.after.resolve(), args.after_resources.resolve(), dict(
            customer_tsv="".join(f"cjk_word\tko-KR\t{chr(0x4e00+i)}\t가\n" for i in range(18000)))))

        def normalize(engine):
            result = Result()
            engine.check(engine.lib.ttf_normalize(engine.handle, raw, len(raw), C.byref(result)))
            try:
                assert C.string_at(result.text, result.length) == raw, "text regression"
            finally:
                engine.lib.ttf_result_free(C.byref(result))

        times = [[] for _ in engines]
        for engine in engines:
            for _ in range(2): normalize(engine)
        for round_id in range(7):
            for index in range(3) if round_id % 2 == 0 else reversed(range(3)):
                start = time.perf_counter()
                for _ in range(3): normalize(engines[index])
                times[index].append((time.perf_counter() - start) / 3)
        medians = list(map(statistics.median, times))
        ratios = [value / medians[0] for value in medians[1:]]
        report = dict(input_bytes=len(raw), threshold=1.25, rounds=7, repeats=3,
                      labels=["before", "candidate", "candidate_plus_18000_words"],
                      seconds=times, median_seconds=medians, ratios=ratios,
                      passed=all(r <= 1.25 for r in ratios))
        # Informational only: do not change the fixed passthrough gate above.
        info_engine = Engine(args.after.resolve(), args.after_resources.resolve(),
                             dict(hanja="korean", kana="korean"))
        engines.append(info_engine)
        samples = []
        for _ in range(2): normalize(info_engine)
        for _ in range(7):
            start = time.perf_counter()
            for _ in range(3): normalize(info_engine)
            samples.append((time.perf_counter() - start) / 3)
        report["informational_both_korean_no_cjk_input"] = dict(
            seconds=samples, median_seconds=statistics.median(samples), gate=False)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report))
        return int(not report["passed"])
    finally:
        for engine in engines: engine.close()


if __name__ == "__main__":
    raise SystemExit(main())
