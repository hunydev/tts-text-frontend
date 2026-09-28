#!/usr/bin/env python3
"""Import local bracket-annotated Korean TN references without editing source data."""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from benchmark.common import digest_json, ensure_output, sha256

SCHEMA = 1
ROUTES = ('pattern', 'lexicon', 'context-control', 'review')


def compact(text):
    # Deliberately no case, punctuation, NFC/NFKC or pronunciation normalization.
    return ''.join(c for c in text if not c.isspace())


def parse_tn(text):
    literals, spoken, at = [], [], 0
    for match in re.finditer(r'\[([^\[\]]*)\]', text):
        literal = text[at:match.start()]
        if '[' in literal or ']' in literal or not match[1].strip():
            raise ValueError('malformed-tn-brackets')
        literals.append(literal)
        spoken.append(match[1])
        at = match.end()
    tail = text[at:]
    if '[' in tail or ']' in tail:
        raise ValueError('malformed-tn-brackets')
    literals.append(tail)
    expected = ''.join(a + b for a, b in zip(literals, spoken)) + tail
    return literals, spoken, expected


def align(text, literals, readings, pairs):
    """Unique whole-template alignment, ignoring whitespace only. No first-hit guess.

    Source theme order can differ from sentence order (English is usually last).
    Match TN bracket readings to the multiset of supplied (written, spoken) pairs,
    then require unchanged non-annotated text to reconstruct the entire input.
    """
    raw_positions = [i for i, c in enumerate(text) if not c.isspace()]
    source = compact(text)
    keys = list(dict.fromkeys((compact(p['ITN']), compact(p['TN'])) for p in pairs))
    remaining = Counter((compact(p['ITN']), compact(p['TN'])) for p in pairs)
    if Counter(compact(s) for s in readings) != Counter(compact(p['TN']) for p in pairs):
        return None, 'bracket-metadata-disagreement'
    plans, steps = [], 0
    def visit(i, pos, chosen):
        nonlocal steps
        steps += 1
        if steps > 4096 or len(plans) > 1:
            return
        literal = compact(literals[i])
        if not source.startswith(literal, pos):
            return
        pos += len(literal)
        if i == len(readings):
            if pos == len(source) and not any(remaining.values()):
                plans.append(chosen)
            return
        for key in keys:
            written, spoken = key
            if written and remaining[key] and spoken == compact(readings[i]) and source.startswith(written, pos):
                remaining[key] -= 1
                visit(i + 1, pos + len(written), chosen + [(pos, pos + len(written), key)])
                remaining[key] += 1
    visit(0, 0, [])
    if steps > 4096:
        return None, 'alignment-search-limit'
    if len(plans) != 1:
        return None, 'ambiguous-source-alignment' if plans else 'unaccounted-source-or-reference-text'
    by_key = defaultdict(list)
    for p in pairs:
        by_key[compact(p['ITN']), compact(p['TN'])].append(p)
    if any(len({p['theme'] for p in values}) > 1 for values in by_key.values()):
        return None, 'ambiguous-metadata-theme'
    result = []
    for i, (begin, end, key) in enumerate(plans[0]):
        pair = by_key[key].pop(0)
        a, b = raw_positions[begin], raw_positions[end - 1] + 1
        result.append(dict(pair, begin=len(text[:a].encode()), end=len(text[:b].encode()),
                           written=text[a:b], spoken=readings[i]))
    return result, None


def route(written, theme):
    """Source-shape triage suggestions, never reference correctness labels."""
    s = compact(written)
    if '@' in s or re.search(r'(?:https?://|www\.)', s):
        return 'pattern', 'electronic-address-shape; explicit @email/@url is an alternative'
    if re.fullmatch(r'[A-Za-z]+(?:[ .&/-][A-Za-z]+)*', written.strip()):
        return 'lexicon', 'Latin pronunciation/initialism; lexicon or explicit @alphabet'
    if re.search(r'[0-9]', s):
        # Shape takes precedence over the corpus topic (e.g. % in topic 가지).
        if re.search(r'[₩$€£¥%]|(?:원|달러|퍼센트)$', s) or re.search(
            r'\d(?:kg|km|cm|mm|ml|mL|GB|GHz|cc|kcal|℃|도|개|가지|명|년|월|일|시|분|초|살|층|호|동|번|마리)$', s):
            return 'pattern', 'quantity/calendar/unit marker; contextual policy may still differ'
        if re.fullmatch(r'\d+(?:[.,:/~+-]\d+)*', s):
            if any(cue in theme for cue in ('번호', '비밀번호', 'PIN', '학번', '차량', '우편', '코드')):
                return 'context-control', 'identifier topic is only a hint; digit/group/zero reading needs context or explicit control'
            return 'pattern', 'numeric syntax; validate value and contextual reading separately'
        if re.search(r'[A-Za-z]', s):
            return 'lexicon', 'alphanumeric name/code; conventional reading may need a lexicon or explicit control'
        if any(cue in theme for cue in ('주소', '단지', '장소')):
            return 'context-control', 'address-like mixed span; explicit @address is a candidate, not an automatic correction'
        return 'pattern', 'mixed numeric phrase; inspect classifier/context before adding a rule'
    return 'review', 'no supported source-shape diagnosis; manual reference/policy review needed'


def convert(row, file, index, file_hash, category, category_en):
    flags = []
    if not isinstance(row, dict):
        raise ValueError('record must be an object')
    text, tn = row.get('ITN'), row.get('TN')
    if not isinstance(text, str) or not text or not isinstance(tn, str) or not tn:
        raise ValueError('nonempty ITN and TN required')
    if '\0' in text + tn:
        raise ValueError('NUL in text')
    pairs = []
    if not isinstance(row.get('patterns'), list):
        raise ValueError('patterns must be an array')
    for pattern in row['patterns']:
        if not isinstance(pattern, dict) or not isinstance(pattern.get('patternTheme'), str) or not isinstance(pattern.get('spans'), list):
            raise ValueError('malformed pattern')
        for pair in pattern['spans']:
            if not isinstance(pair, dict) or any(not isinstance(pair.get(k), str) or not pair[k].strip() or '\0' in pair[k] for k in ('ITN', 'TN')):
                raise ValueError('malformed pattern span')
            primary, reason = route(pair['ITN'], pattern['patternTheme'])
            pairs.append(dict(pair, theme=pattern['patternTheme'], route=primary, route_reason=reason))
    for key, english in [('NumberWord', False), ('EnglishWord', True)]:
        listed = row.get(key)
        if not isinstance(listed, list) or any(not isinstance(s, str) for s in listed):
            flags.append('invalid-' + key)
        elif Counter(listed) != Counter(p['ITN'] for p in pairs if (p['theme'] == '영어') == english):
            flags.append(key + '-metadata-disagreement')
    try:
        literals, spoken, expected = parse_tn(tn)
        spans, error = align(text, literals, spoken, pairs)
        if error:
            flags.append(error)
        if Counter(spoken) != Counter(p['TN'] for p in pairs):
            flags.append('bracket-metadata-text-difference')
    except ValueError as e:
        expected, spans = None, None
        flags.append(str(e))
    if spans is None:
        spans = [dict(p, begin=None, end=None, written=p['ITN'], spoken=p['TN']) for p in pairs]
    routes = sorted(set(p['route'] for p in pairs))
    primary = 'review' if flags else next((r for r in ('context-control', 'lexicon', 'pattern') if r in routes), 'review')
    fingerprint = digest_json(row)
    return dict(schema_version=SCHEMA, id=f'ko-release/{category_en}/{index:06d}',
                record_sha256=fingerprint, category=category, category_en=category_en,
                text=text, expected=expected, annotated_tn=tn, spans=spans,
                themes=sorted(set(p['theme'] for p in pairs)), route=primary,
                route_candidates=routes, flags=sorted(set(flags)), reference_status='provided-reference',
                provenance=dict(source_file=file, source_index=index, source_sha256=file_hash,
                                split='Training', license='not-verified; local benchmark only'),
                input_group=digest_json(compact(text)), pair_group=digest_json([compact(text), compact(expected or tn)]))


def run(source, output):
    source, output = Path(source).resolve(), ensure_output(output)
    if output.exists():
        raise ValueError('output already exists; imports are immutable')
    summary_path = source / 'summary.json'
    summary = json.loads(summary_path.read_text(encoding='utf-8'))
    output.mkdir(parents=True)
    counts, flags, files, groups = Counter(), Counter(), [], defaultdict(set)
    audit = dict(schema_version=SCHEMA, status='incomplete', source_summary=summary,
                 source_summary_sha256=sha256(summary_path), source_files=files,
                 policy='Original source unchanged; all flags retained; no automatic gold approval.')
    (output/'source-summary.json').write_bytes(summary_path.read_bytes())
    try:
        with (output/'records.jsonl').open('w', encoding='utf-8', newline='\n') as target, (output/'quarantine.jsonl').open('w', encoding='utf-8', newline='\n') as rejects:
            seen_files = set()
            for entry in summary['files']:
                name = entry['file']
                if name != Path(name).name or not re.fullmatch(r'[0-9]{2}_[a-z_]+\.json', name) or name in seen_files:
                    raise ValueError('invalid/duplicate source filename')
                seen_files.add(name)
                path = source/name
                if path.is_symlink():
                    raise ValueError('source symlinks are not accepted')
                digest = sha256(path)
                data = json.loads(path.read_text(encoding='utf-8'))
                if data['count'] != len(data['data']) or entry['records'] != data['count'] or any(data[k] != entry[k] for k in ('category', 'category_en', 'category_id')) or data['split'] != summary['split']:
                    raise ValueError('source summary/file count or category mismatch: ' + name)
                files.append(dict(file=name, sha256=digest, records=len(data['data'])))
                for index, row in enumerate(data['data'], 1):
                    counts['source_records'] += 1
                    try:
                        record = convert(row, name, index, digest, data['category'], data['category_en'])
                    except (ValueError, TypeError, KeyError, UnicodeError) as error:
                        counts['quarantined'] += 1
                        rejects.write(json.dumps(dict(file=name,index=index,error=str(error),raw=row),ensure_ascii=False)+'\n')
                        continue
                    counts['imported'] += 1
                    counts['spans'] += len(record['spans'])
                    counts['aligned_spans'] += sum(p['begin'] is not None for p in record['spans'])
                    counts['flagged_records'] += bool(record['flags'])
                    counts['sentence_references'] += record['expected'] is not None
                    flags.update(record['flags'])
                    groups[record['input_group']].add(record['pair_group'])
                    target.write(json.dumps(record, ensure_ascii=False, separators=(',', ':'))+'\n')
                print(f'imported {name}: {counts["imported"]}', flush=True)
        if counts['source_records'] != summary['totals']['records'] or len(files) != summary['totals']['files']:
            raise ValueError('source total mismatch')
        audit.update(status='complete', counts=dict(counts), flags=dict(flags),
                     unique_input_groups=len(groups), duplicate_input_records=counts['imported']-len(groups),
                     conflicting_input_groups=[k for k,v in groups.items() if len(v)>1],
                     records_sha256=sha256(output/'records.jsonl'), importer_sha256=sha256(Path(__file__)))
        audit['import_id'] = digest_json(dict(summary=audit['source_summary_sha256'], files=files, records=audit['records_sha256']))
    finally:
        (output/'manifest.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return audit


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();r=run(a.source,a.out);print(json.dumps(r['counts']));return 0

if __name__ == '__main__':
    raise SystemExit(main())
