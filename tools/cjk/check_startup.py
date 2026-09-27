"""CJK §9.2: real CLI process startup, including create/load/freeze/normalize.

No persistent engine or evaluator batching. Fixed pre-CJK reference: 16.5 ms;
25 alternating samples, warm filesystem cache, 1.25x threshold. Enabled modes
are informational only. Use a quiet VM, without concurrent test suites.
"""
import argparse
import json
from pathlib import Path
import statistics
import subprocess
import time


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--ttf', type=Path, required=True)
    p.add_argument('--resources', type=Path, required=True)
    p.add_argument('--report', type=Path, required=True)
    a = p.parse_args()
    modes = {'passthrough': [], 'hanja': ['--hanja', 'korean'],
             'kana': ['--kana', 'korean'], 'both': ['--hanja', 'korean', '--kana', 'korean']}
    samples = {key: [] for key in modes}
    for i in range(27):
        for name in list(modes) if i % 2 == 0 else reversed(modes):
            start = time.perf_counter()
            result = subprocess.run([str(a.ttf.resolve()), 'normalize', '--resources',
                                     str(a.resources.resolve()), *modes[name], '가'],
                                    capture_output=True, check=True)
            elapsed = time.perf_counter() - start
            assert result.stdout == '가\n'.encode(), result.stdout
            if i >= 2:
                samples[name].append(elapsed * 1000)
    medians = {key: statistics.median(values) for key, values in samples.items()}
    report = dict(reference_ms=16.5, threshold=1.25, samples_ms=samples, median_ms=medians,
                  passthrough_ratio=medians['passthrough'] / 16.5,
                  passed=medians['passthrough'] <= 16.5 * 1.25,
                  informational_modes=['hanja', 'kana', 'both'])
    a.report.parent.mkdir(parents=True, exist_ok=True)
    a.report.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k != 'samples_ms'}))
    return int(not report['passed'])


if __name__ == '__main__':
    raise SystemExit(main())
