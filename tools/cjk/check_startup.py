"""CJK §9.2: real CLI process startup, including create/load/freeze/normalize.

No persistent engine or evaluator batching. A before build and an after build
start in alternating order, 25 samples each per mode after two warm-up rounds,
warm filesystem cache; the gate is the after/before ratio of the passthrough
median, 1.25 at most. Enabled modes are informational only. Use a quiet VM,
without concurrent test suites.

A relative gate since 2026-10-27 (owner decision): the fixed 16.5 ms reference
predated the seven language packs, and on the shared VM the same build measured
55 ms and 100 ms an hour apart, so no fixed reference holds. Compare the change
with the build before it, as check_throughput.py does.
"""
import argparse
import json
from pathlib import Path
import statistics
import subprocess
import time

MODES = {'passthrough': [], 'hanja': ['--hanja', 'korean'],
         'kana': ['--kana', 'korean'], 'both': ['--hanja', 'korean', '--kana', 'korean']}


def start_ms(ttf, resources, options):
    start = time.perf_counter()
    result = subprocess.run([str(ttf), 'normalize', '--resources', str(resources), *options, '가'],
                            capture_output=True, check=True)
    elapsed = time.perf_counter() - start
    assert result.stdout == '가\n'.encode(), result.stdout
    return elapsed * 1000


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('before', 'before-resources', 'after', 'after-resources', 'report'):
        p.add_argument('--' + name, type=Path, required=True)
    a = p.parse_args()
    builds = {'before': (a.before.resolve(), a.before_resources.resolve()),
              'after': (a.after.resolve(), a.after_resources.resolve())}
    samples = {side: {mode: [] for mode in MODES} for side in builds}
    for i in range(27):
        modes = list(MODES) if i % 2 == 0 else list(reversed(MODES))
        for mode in modes:
            for side in (('before', 'after') if i % 2 == 0 else ('after', 'before')):
                ms = start_ms(*builds[side], MODES[mode])
                if i >= 2:
                    samples[side][mode].append(ms)
    medians = {side: {mode: statistics.median(v) for mode, v in by.items()} for side, by in samples.items()}
    ratios = {mode: medians['after'][mode] / medians['before'][mode] for mode in MODES}
    report = dict(threshold=1.25, samples_ms=samples, median_ms=medians, ratios=ratios,
                  passed=ratios['passthrough'] <= 1.25, informational_modes=['hanja', 'kana', 'both'])
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'samples_ms'}))
    return int(not report['passed'])


if __name__ == '__main__':
    raise SystemExit(main())
