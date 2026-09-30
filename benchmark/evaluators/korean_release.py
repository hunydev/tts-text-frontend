#!/usr/bin/env python3
"""Auditable Korean reference agreement reports (batch text, never speech quality)."""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import sqlite3
import subprocess
import sys
import time
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from benchmark.common import digest_json, ensure_output, sha256, tree_hash
from benchmark.evaluators.evaluate import Backend, align_span
from benchmark.importers.korean_release.import_release import compact, ROUTES

sys.path.insert(0, str(ROOT / 'tools/web'))
import jev as jev_judge  # noqa: E402
JEV_CACHE = ROOT / 'benchmark/generated/jev-cache/ko-KR.jsonl'


def jev_metric():
    return dict(scored=0, total=0.0, passed=0, low=0, unread=0,
                reference_different_passed=0, reference_agreed_failed=0)


def jev_add(m, value, unread, status):
    """Jev pass = noul >= 0.7 and no digits, Latin or symbols left unread."""
    ok = value >= jev_judge.PASS and not unread
    m['scored'] += 1; m['total'] += value; m['passed'] += ok
    m['low'] += value < 0.4; m['unread'] += bool(unread)
    if status == 'different':
        m['reference_different_passed'] += ok
    elif status in ('exact', 'spacing', 'variant'):
        m['reference_agreed_failed'] += not ok


def jev_finish(m):
    n = m['scored']
    return dict({k: v for k, v in m.items() if k != 'total'}, mean=round(m['total'] / n, 4) if n else None,
                pass_rate=m['passed'] / n if n else None)

METRIC_VERSION = 'ko-reference-agreement-v2'
OPTIONS = dict(input_format='plain',output_mode='speech',unreadable='preserve',
               hanja='passthrough',kana='passthrough',render_policy='phonetic',
               brackets='read',unknown_foreign='guess',decorative='follow',initialism='spell')


# Policy variants that people read either way (reference convention, not TN
# errors), folded symmetrically on both sides and only inside runs of digit
# syllables: zero 영/공, the separator 다시/에 between digits, and a leading
# 일만/만. Whitespace is already ignored by compact. Mixed native digits
# (하나, 둘) and Sino/native choices (삼십/서른) are readings, not folded.
DIGIT = '공영일이삼사오육칠팔구'
VARIANT_FOLDS = (
    (re.compile('(?<=[%s])(?:다시|에)(?=[%s])' % (DIGIT, DIGIT)), ''),
    (re.compile('(?<=[%s])영|영(?=[%s])' % (DIGIT, DIGIT)), '공'),
)
# Before compaction: 통일만 is a word, and 십 일만 (110,000 spaced by unit) is a number.
LEADING_ILMAN = re.compile('(?<![가-힣])(?<![공영일이삼사오육칠팔구십백천만억조] )일만')


def variant(text):
    text = compact(LEADING_ILMAN.sub('만', text))
    for pattern, replacement in VARIANT_FOLDS:
        text = pattern.sub(replacement, text)
    return text


def metric():
    return dict(total=0,scorable=0,unscorable=0,errors=0,unaligned=0,exact=0,
                compact=0,spacing_only=0,variant=0,different=0)


def add(m, expected, actual, error=None, unaligned=False):
    m['total'] += 1
    m['errors'] += bool(error)
    m['unaligned'] += bool(unaligned)
    if expected is None:
        m['unscorable'] += 1
        return
    m['scorable'] += 1
    exact = actual is not None and actual == expected
    loose = actual is not None and compact(actual) == compact(expected)
    folded = loose or (actual is not None and variant(actual) == variant(expected))
    m['exact'] += exact; m['compact'] += loose
    m['spacing_only'] += loose and not exact
    m['variant'] += folded
    m['different'] += not folded


def finish(m):
    return dict(m,exact_rate=m['exact']/m['scorable'] if m['scorable'] else None,
                compact_rate=m['compact']/m['scorable'] if m['scorable'] else None,
                variant_rate=m['variant']/m['scorable'] if m['scorable'] else None)


def read_reviews(path):
    rows = {}
    if path is None: return rows
    raw = path.read_text(encoding='utf-8')
    try:
        parsed = json.loads(raw)
        source = parsed if isinstance(parsed,list) else [parsed]
    except json.JSONDecodeError:
        source = [json.loads(line) for line in raw.splitlines() if line.strip()]
    for r in source:
        if not isinstance(r,dict) or not all(isinstance(r.get(k),str) and r[k].strip() for k in ('id','record_sha256','reviewer','reason','reviewed_at')):
            raise ValueError('review requires id, source fingerprint, reviewer, reason and reviewed_at')
        if not re.fullmatch(r'[0-9a-f]{64}', r['record_sha256']) or r['id'] in rows:
            raise ValueError('invalid/duplicate review fingerprint/id')
        datetime.fromisoformat(r['reviewed_at'].replace('Z','+00:00'))
        if r.get('status') not in ('accepted','corrected','excluded','needs-review'):
            raise ValueError('unknown review status')
        if r['status']=='corrected' and (not isinstance(r.get('expected'),str) or not r['expected'] or '\0' in r['expected']):
            raise ValueError('corrected review requires nonempty expected text')
        if r.get('route') is not None and r['route'] not in ROUTES:
            raise ValueError('unknown reviewed route')
        rows[r['id']] = r
    return rows


def run(import_dir, library, resources, output, reviews_path=None, label='', backend=None,
        overlays=(), jev=False, predictions=None, system=None):
    """predictions: a JSON-lines file {id, actual} of another system's outputs
    (legacy TN), scored instead of this engine; jev: add a TypeSafe Jev score
    per sentence (tools/web/jev.py, cached in benchmark/generated/jev-cache)."""
    predicted = None
    if predictions is not None:
        predictions = Path(predictions).resolve(strict=True)
        predicted = {}
        for line in predictions.read_text(encoding='utf-8').splitlines():
            if line.strip():
                p = json.loads(line)
                if not isinstance(p.get('id'), str) or not (p.get('actual') is None or isinstance(p['actual'], str)):
                    raise ValueError('prediction rows need id and actual')
                predicted[p['id']] = p['actual']
    import_dir, output = Path(import_dir).resolve(), ensure_output(output)
    manifest = json.loads((import_dir/'manifest.json').read_text())
    if manifest['status'] != 'complete' or manifest['records_sha256'] != sha256(import_dir/'records.jsonl'):
        raise ValueError('incomplete or modified import')
    if output.exists(): raise ValueError('report exists; create a new immutable run')
    reviews = read_reviews(reviews_path)
    # Fingerprints and known IDs are checked before any native work/output.
    with (import_dir/'records.jsonl').open() as f:
        for line in f:
            r=json.loads(line)
            if r['id'] in reviews and reviews[r['id']]['record_sha256'] != r['record_sha256']:
                raise ValueError('stale review for '+r['id'])
    if reviews:
        with (import_dir/'records.jsonl').open(encoding='utf-8') as f:
            ids=set(json.loads(l)['id'] for l in f)
        if set(reviews)-ids: raise ValueError('review contains unknown ids')
    source_paths = ('benchmark/evaluators/korean_release.py','benchmark/evaluators/evaluate.py','tools/web/jev.py',
                    'benchmark/common.py','benchmark/importers/korean_release/import_release.py',
                    'tools/web/native.py','tools/evaluation/evaluate.py')
    source_bytes = {p:(ROOT/p).read_bytes() for p in source_paths}
    overlays = [Path(o).resolve(strict=True) for o in overlays]
    engine_file = predictions if predicted is not None else library
    identities = dict(library=sha256(engine_file),resources=tree_hash(resources),
                      reviews=sha256(reviews_path) if reviews_path else None,
                      overlays=[sha256(o) for o in overlays])
    output.mkdir(parents=True)
    owned = backend is None
    # Overlays (e.g. the reading-unit spacing profile) load as the domain layer.
    if predicted is not None:
        owned = False
    else:
        backend = backend or Backend(library,None,resources,OPTIONS,overlays)
    started = datetime.now(timezone.utc).isoformat(); clock = time.monotonic()
    db = sqlite3.connect(output/'report.sqlite')
    db.executescript('''
      CREATE TABLE cases(id TEXT PRIMARY KEY,category TEXT,route TEXT,status TEXT,
        flagged INTEGER,duplicate_group TEXT,text TEXT NOT NULL,details BLOB NOT NULL,jev REAL);
      CREATE INDEX case_filter ON cases(category,route,status);
      CREATE INDEX case_flags ON cases(flagged);
      CREATE TABLE spans(id INTEGER PRIMARY KEY,case_id TEXT,category TEXT,theme TEXT,
        route TEXT,status TEXT,details BLOB NOT NULL);
      CREATE INDEX span_filter ON spans(category,theme,route,status);
      CREATE INDEX span_case ON spans(case_id);
    ''')
    sentences, spans_total, clean, unique, reviewed, adjusted = [metric() for _ in range(6)]
    category = defaultdict(lambda: dict(sentences=metric(),adjusted=metric(),spans=metric(),flagged=0))
    themes, routes, engine_types = [defaultdict(metric) for _ in range(3)]
    flags, review_counts, status_counts = Counter(),Counter(),Counter()
    seen, count = set(), 0
    judged = []  # (id, category, text, actual, status) for Jev after the pass
    conflicts=set(manifest['conflicting_input_groups'])
    try:
        with (import_dir/'records.jsonl').open(encoding='utf-8') as handle:
            for line in handle:
                row=json.loads(line); count+=1; flags_here=list(row['flags'])
                if row['input_group'] in conflicts: flags_here.append('conflicting-input-reference')
                actual,result,error=None,None,None
                if predicted is not None:
                    if row['id'] in predicted and predicted[row['id']] is not None:
                        actual=predicted[row['id']];result=dict(text=actual,spans=[])
                    else:
                        error='no prediction for this id'
                else:
                    try:
                        result=backend.run(row['text'],'ko-KR');actual=result['text']
                    except (ValueError,RuntimeError,OSError) as exc:
                        error=str(exc)
                expected=row['expected'];cat=row['category_en'];rev=reviews.get(row['id'])
                primary=rev.get('route',row['route']) if rev else row['route']
                add(sentences,expected,actual,error);add(category[cat]['sentences'],expected,actual,error)
                if not flags_here: add(clean,expected,actual,error)
                if row['pair_group'] not in seen:
                    add(unique,expected,actual,error);seen.add(row['pair_group'])
                flags.update(flags_here);category[cat]['flagged']+=bool(flags_here)
                if rev:
                    review_counts[rev['status']]+=1
                    if rev['status'] in ('accepted','corrected'):
                        add(reviewed,rev.get('expected',expected) if rev['status']=='corrected' else expected,actual,error)
                # Adjusted: every sentence except review exclusions, corrections applied.
                if not (rev and rev['status']=='excluded'):
                    fixed=rev['expected'] if rev and rev['status']=='corrected' else expected
                    add(adjusted,fixed,actual,error);add(category[cat]['adjusted'],fixed,actual,error)
                status=('unscorable' if expected is None else 'error' if error else
                        'exact' if actual==expected else 'spacing' if compact(actual)==compact(expected) else
                        'variant' if variant(actual)==variant(expected) else 'different')
                status_counts[status]+=1
                projected=[]
                for s in row['spans']:
                    part,why,selected=None,'reference-alignment-unavailable',[]
                    if result is not None and s['begin'] is not None:
                        part,why,selected=align_span(s,result)
                    if error: why='engine-error'
                    add(spans_total,s['spoken'],part,error,part is None)
                    add(category[cat]['spans'],s['spoken'],part,error,part is None)
                    add(themes[s['theme']],s['spoken'],part,error,part is None)
                    add(routes[s['route']],s['spoken'],part,error,part is None)
                    types=sorted({p['semantic_type'] for p in selected})
                    add(engine_types['+'.join(types) or 'UNALIGNED'],s['spoken'],part,error,part is None)
                    st=('unaligned' if part is None else 'exact' if part==s['spoken'] else
                        'spacing' if compact(part)==compact(s['spoken']) else
                        'variant' if variant(part)==variant(s['spoken']) else 'different')
                    detail=dict(s,actual=part,alignment_issue=why,engine_types=types,
                                engine_rules=sorted({p['rule'] for p in selected}),status=st)
                    projected.append(detail)
                    db.execute('INSERT INTO spans(case_id,category,theme,route,status,details) VALUES(?,?,?,?,?,?)',
                               (row['id'],cat,s['theme'],s['route'],st,zlib.compress(json.dumps(detail,ensure_ascii=False).encode(),1)))
                detail=dict(row,actual=actual,error=error,status=status,flags=flags_here,route=primary,
                            review=rev,spans=projected,engine_spans=result['spans'] if result else [])
                db.execute('INSERT INTO cases VALUES(?,?,?,?,?,?,?,?,NULL)',
                    (row['id'],cat,primary,status,bool(flags_here),row['input_group'],row['text'],zlib.compress(json.dumps(detail,ensure_ascii=False).encode(),1)))
                if jev and actual:
                    judged.append((row['id'],cat,row['text'],actual,status))
                if count%5000==0:
                    db.commit();print(json.dumps(dict(processed=count,total=manifest['counts']['imported'],seconds=round(time.monotonic()-clock,1))),flush=True)
        if count != manifest['counts']['imported']: raise ValueError('import record count mismatch')
        db.commit()
        jev_report, jev_categories = None, {}
        if jev:
            scores, fresh = jev_judge.batch([(t, a) for _, _, t, a, _ in judged], 'ko-KR', JEV_CACHE,
                                            progress=lambda d, n: print(json.dumps(dict(jev=d, of=n)), flush=True))
            overall, models = jev_metric(), Counter()
            for case_id, cat, text, actual, status in judged:
                value, model = scores[(text, actual)]
                left = jev_judge.unread(actual, 'ko-KR')
                models[model] += 1
                jev_add(overall, value, left, status)
                jev_add(jev_categories.setdefault(cat, jev_metric()), value, left, status)
                raw = db.execute('SELECT details FROM cases WHERE id=?', (case_id,)).fetchone()[0]
                detail = json.loads(zlib.decompress(raw).decode())
                detail['jev'] = dict(noul=value, model=model, unread=left, passed=value >= jev_judge.PASS and not left)
                db.execute('UPDATE cases SET jev=?, details=? WHERE id=?',
                           (value, zlib.compress(json.dumps(detail, ensure_ascii=False).encode(), 1), case_id))
            db.commit()
            jev_report = dict(jev_finish(overall), question=jev_judge.QUESTION_ID, threshold=jev_judge.PASS,
                              models=dict(models), newly_scored=fresh,
                              policy='Pass = Jev noul >= 0.7 ("spoken is a correct way to read written aloud") '
                                     'and no digits, Latin letters or symbols left in the output. Independent of '
                                     'the reference: it accepts other correct readings and can miss fine '
                                     'pronunciation errors. Calibration: tools/web/jev.py.')
        if (identities != dict(library=sha256(engine_file),resources=tree_hash(resources),
                               reviews=sha256(reviews_path) if reviews_path else None,
                               overlays=[sha256(o) for o in overlays])
            or any((ROOT/p).read_bytes()!=data for p,data in source_bytes.items())
            or sha256(import_dir/'records.jsonl')!=manifest['records_sha256']):
            raise ValueError('inputs or harness changed during evaluation; report is not publishable')
        fingerprint = dict(metric_version=METRIC_VERSION,import_id=manifest['import_id'],options=OPTIONS,
                           evaluator_sha256=sha256(Path(__file__)),alignment_sha256=sha256(ROOT/'benchmark/evaluators/evaluate.py'),
                           reviews_sha256=sha256(reviews_path) if reviews_path else None,
                           overlays_sha256=[sha256(o) for o in overlays])
        report=dict(schema_version=1,status='complete',label=label or output.name,run_id=output.name,
                    created_at=started,finished_at=datetime.now(timezone.utc).isoformat(),
                    elapsed_seconds=round(time.monotonic()-clock,3),metric_version=METRIC_VERSION,storage='zlib-json-v1',
                    comparison_key=digest_json(fingerprint),protocol=fingerprint,options=dict(OPTIONS,default_language='ko-KR'),
                    reference_status='provided-reference; source Training; not independent test gold',
                    source=dict(name=manifest['source_summary']['name'],split='Training',import_id=manifest['import_id'],
                                source_summary_sha256=manifest['source_summary_sha256'],source_files=manifest['source_files'],
                                counts=manifest['counts'],flags=manifest['flags'],duplicate_input_records=manifest['duplicate_input_records'],
                                conflicting_input_groups=len(conflicts),rights='Unverified; local benchmark only; no source redistribution'),
                    engine=dict(library_sha256=sha256(engine_file),resources_sha256=tree_hash(resources),
                                system=system or ('external predictions' if predicted is not None else 'tts-text-frontend'),
                                predictions=str(predictions) if predicted is not None else None,
                                git_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                                git_dirty=bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT))),
                    harness=dict(evaluator_sha256=sha256(Path(__file__)),importer_sha256=manifest['importer_sha256'],
                                 adapter_sha256=sha256(ROOT/'tools/web/native.py'),alignment_sha256=sha256(ROOT/'benchmark/evaluators/evaluate.py')),
                    sentences=finish(sentences),spans=finish(spans_total),structurally_clean_sentences=finish(clean),
                    unique_pair_sentences=finish(unique),reviewed_sentences=finish(reviewed),adjusted_sentences=finish(adjusted),review_counts=dict(review_counts),
                    flags=dict(flags),status_counts=dict(status_counts),
                    jev=jev_report,
                    selection=manifest.get('selection'),
                    categories=[dict(id=k,name=next(x['category'] for x in manifest['source_summary']['files'] if x['category_en']==k),
                                     sentences=finish(v['sentences']),adjusted_sentences=finish(v['adjusted']),spans=finish(v['spans']),flagged=v['flagged'],
                                     jev=jev_finish(jev_categories[k]) if k in jev_categories else None) for k,v in category.items()],
                    themes=[dict(name=k,spans=finish(v)) for k,v in sorted(themes.items())],
                    routes=[dict(name=k,spans=finish(v)) for k,v in sorted(routes.items())],
                    engine_types=[dict(name=k,spans=finish(v)) for k,v in sorted(engine_types.items())],
                    policy=dict(whitespace='Remove Python str.isspace Unicode whitespace only; preserve punctuation, case and codepoints.',
                                unaligned='All declared reference spans remain in the denominator; unaligned spans are misses, reported separately.',
                                reviews='Raw headline never changes with review exclusions/corrections. Reviewed sentence subset is separate.',
                                adjusted='All sentences minus review exclusions, with review corrections applied; '
                                         'equals the raw headline when no review file is given. Compare engines only on the same review file.',
                                routes='Source-shape suggestions, not correctness verdicts. No lexicon or controls injected into evaluation.',
                                variant='Compact, plus a symmetric fold inside digit runs only: zero 영/공, the '
                                        'separator 다시/에 between digits and a leading 일만/만. Readings such as '
                                        'native digits or 서른/삼십 are not folded.',
                                themes='Dataset topics, not semiotic class truth.',timing='Wall time of report generation, not native throughput or audio latency.'))
        db.close()
        report['database_sha256']=sha256(output/'report.sqlite')
        # Preserve evaluator code even when the checkout is dirty/uncommitted.
        for relative, data in source_bytes.items():
            target=output/'harness'/relative
            target.parent.mkdir(parents=True,exist_ok=True)
            target.write_bytes(data)
        # Completion marker is written last; servers never expose partial runs.
        (output/'summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        return report
    finally:
        db.close()
        if owned: backend.close()


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--import-dir',type=Path,required=True);p.add_argument('--library',type=Path,required=True)
    p.add_argument('--resources',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--reviews',type=Path);p.add_argument('--label',default='')
    p.add_argument('--overlay',type=Path,action='append',default=[],
                   help='resource overlay TSV, e.g. resources/customers/reading-unit-spacing/ko-KR/rules.tsv')
    p.add_argument('--jev',action='store_true',help='add a TypeSafe Jev score per sentence (cached)')
    p.add_argument('--predictions',type=Path,help='JSON lines {id, actual}: score another system (legacy TN)')
    p.add_argument('--system',help='name of the scored system shown in the report')
    a=p.parse_args();r=run(a.import_dir,a.library.resolve(),a.resources.resolve(),a.out,a.reviews,a.label,
                           overlays=a.overlay,jev=a.jev,predictions=a.predictions,system=a.system)
    print(json.dumps(dict(sentences=r['sentences'],spans=r['spans'],jev=r['jev']),ensure_ascii=False));return 0

if __name__=='__main__': raise SystemExit(main())
