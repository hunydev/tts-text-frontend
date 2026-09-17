#!/usr/bin/env python3
"""Compare the same original Korean counter contracts on two engine snapshots.

This is a development regression suite, not held-out human-approved TN gold.
Only aggregate counts/hashes are emitted; references remain in the integration test.
"""
import argparse
from collections import defaultdict
import hashlib
import importlib.util
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/integration'))
from test_korean_counters import references, check_reference


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def resource_hash(root):
    h = hashlib.sha256()
    for path in sorted(p for p in root.rglob('*') if p.is_file()):
        h.update(path.relative_to(root).as_posix().encode() + b'\0')
        h.update(bytes.fromhex(sha(path)))
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for side in ('before', 'after'):
        parser.add_argument('--' + side + '-library', required=True, type=Path)
        parser.add_argument('--' + side + '-resources', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    spec = importlib.util.spec_from_file_location('ttf_native', ROOT / 'tools/web/native.py')
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    rows = references()
    results, snapshots = {}, {}
    for side in ('before', 'after'):
        library = getattr(args, side + '_library').resolve()
        resources = getattr(args, side + '_resources').resolve()
        snapshots[side] = dict(library_sha256=sha(library), resources_sha256=resource_hash(resources))
        engine = native.Engine(library, resources, {'default_language': 'ko-KR'})
        outcomes = []
        try:
            for row in rows:
                result = engine.normalize(row['text'].encode())
                text_exact = result['text'] == row['expected']
                try:
                    check_reference(result, row)
                    contract = True
                except (AssertionError, KeyError):
                    contract = False
                outcomes.append(dict(text_exact=text_exact, contract_exact=contract))
        finally:
            engine.close()
        if snapshots[side] != dict(library_sha256=sha(library), resources_sha256=resource_hash(resources)):
            raise RuntimeError('Engine or resources changed during comparison')
        results[side] = outcomes
    classes = defaultdict(lambda: dict(total=0, before_text_exact=0, after_text_exact=0,
                                        before_contract_exact=0, after_contract_exact=0,
                                        text_fixed=0, text_regressed=0))
    for i, row in enumerate(rows):
        for key in (row['semantic_type'], 'ALL'):
            counts = classes[key]
            counts['total'] += 1
            before, after = results['before'][i], results['after'][i]
            for side, values in (('before', before), ('after', after)):
                for metric in ('text_exact', 'contract_exact'):
                    counts[side + '_' + metric] += values[metric]
            counts['text_fixed'] += not before['text_exact'] and after['text_exact']
            counts['text_regressed'] += before['text_exact'] and not after['text_exact']
    report = dict(schema_version=1, benchmark='original-korean-counter-development-contracts',
                  reference_status='pending-independent-review', held_out=False,
                  reference_sha256=hashlib.sha256(json.dumps(rows, ensure_ascii=False,
                      sort_keys=True, separators=(',', ':')).encode()).hexdigest(),
                  reference_code_sha256=sha(ROOT / 'tests/integration/test_korean_counters.py'),
                  snapshots=snapshots, metrics=dict(classes),
                  note='Contract exact includes text and declared span class/value/unit/secondary/language metadata. Counts are development coverage, not population accuracy.')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps(report['metrics']['ALL'], ensure_ascii=False))


if __name__ == '__main__':
    main()
