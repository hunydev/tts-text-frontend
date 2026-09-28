#!/usr/bin/env python3
"""Collect review candidates from a completed Korean report; never edit lexicons."""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import sqlite3
import sys
from urllib.parse import quote
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from benchmark.common import sha256
from tools.lexicon.manage import canonical


def collect(rows):
    groups = {}
    rejected = Counter()
    for case_id, category, flagged, raw in rows:
        span = json.loads(zlib.decompress(raw)) if isinstance(raw, bytes) else raw
        written = span.get('written')
        try:
            key = canonical(written)
        except (ValueError, TypeError):
            rejected['unsupported-lexical-shape'] += 1
            continue
        spoken = span.get('spoken')
        if not isinstance(spoken, str) or not spoken.strip():
            rejected['missing-reference'] += 1
            continue
        group = groups.setdefault(key, dict(term=key, occurrences=0, mismatches=0,
            unaligned=0, flagged=0, sentence_ids=set(), examples=[],
            spellings=Counter(), references=Counter(), outputs=Counter(),
            categories=Counter(), engine_types=Counter()))
        group['occurrences'] += 1
        group['mismatches'] += span['status'] not in ('exact', 'spacing')
        group['unaligned'] += span['status'] == 'unaligned'
        group['flagged'] += bool(flagged)
        group['sentence_ids'].add(case_id)
        if case_id not in group['examples'] and len(group['examples']) < 5:
            group['examples'].append(case_id)
        group['spellings'][written] += 1
        group['references'][''.join(spoken.split())] += 1
        if span.get('actual') is not None:
            group['outputs'][''.join(span['actual'].split())] += 1
        group['categories'][category] += 1
        group['engine_types'].update(span.get('engine_types', []))
    candidates = []
    for group in groups.values():
        group['sentences'] = len(group.pop('sentence_ids'))
        signals = []
        if len(group['references']) > 1:
            signals.append('multiple-reference-readings')
        if group['unaligned']:
            signals.append('alignment-review')
        if group['flagged']:
            signals.append('source-structure-review')
        numeric = set(group['engine_types']) - {'WORD', 'TEXT', 'INITIALISM', 'ACRONYM', 'ALNUM_TERM', 'ABBREVIATION'}
        if numeric:
            signals.append('classifier-or-context-review')
        group['review_signals'] = signals
        for k in ('spellings', 'references', 'outputs', 'categories', 'engine_types'):
            group[k] = dict(sorted(group[k].items(), key=lambda item: (-item[1], item[0])))
        candidates.append(group)
    candidates.sort(key=lambda row: (-row['mismatches'], -row['occurrences'], row['term']))
    return dict(candidates=candidates, rejected=dict(rejected),
                summary=dict(terms=len(candidates), occurrences=sum(r['occurrences'] for r in candidates),
                             mismatches=sum(r['mismatches'] for r in candidates)))


def run(report_dir, output):
    report_dir, output = Path(report_dir).resolve(), Path(output)
    if output.exists():
        raise ValueError('output exists; use a new candidate collection')
    report = json.loads((report_dir / 'summary.json').read_text(encoding='utf-8'))
    db_path = report_dir / 'report.sqlite'
    identity = dict(summary_sha256=sha256(report_dir/'summary.json'), database_sha256=sha256(db_path))
    if report.get('status') != 'complete' or report.get('schema_version') != 1:
        raise ValueError('a completed Korean report is required')
    if identity['database_sha256'] != report.get('database_sha256'):
        raise ValueError('report database checksum mismatch')
    with sqlite3.connect('file:' + quote(str(db_path)) + '?mode=ro', uri=True) as db:
        result = collect(db.execute("SELECT s.case_id,s.category,c.flagged,s.details FROM spans s "
                                    "JOIN cases c ON c.id=s.case_id WHERE s.route='lexicon' ORDER BY s.id"))
    if identity != dict(summary_sha256=sha256(report_dir/'summary.json'), database_sha256=sha256(db_path)):
        raise ValueError('report changed during collection')
    result.update(schema_version=1, report=report['run_id'], source=identity,
                  policy='Review queue only. Counts are spans, not predicted sentence gains. Multiple readings, alignment and classifier conflicts require review. No dictionary generation or gold approval.',
                  rights='Inherits input data restrictions; keep this output under ignored artifacts/ or benchmark/generated/.')
    output.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation prevents concurrent writers from replacing a review queue.
    with output.open('x', encoding='utf-8', newline='\n') as f:
        json.dump(result, f, ensure_ascii=False, indent=2)
        f.write('\n')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    result = run(args.report, args.out)
    print(json.dumps(result['summary']))
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
