"""Read-only, paginated access to completed local Korean reference reports.

decisions-final.json (tools/web/ko_decisions.py, 2026-10-12) holds OpenAI
Decisions judgements for the sentences the benchmark leaves `different`: an
accepted one is counted apart, and what is left is wrong even for Decisions.
"""
import json
import hashlib
from pathlib import Path
import re
import sqlite3
import zlib
from urllib.parse import parse_qs, quote

DEFAULT_ROOT=Path(__file__).resolve().parents[2]/'benchmark/generated/ko-release/reports'
DECISIONS_FILE='decisions-final.json'
# Stage filters (sentence scope): accepted by Decisions, and wrong even for Decisions.
STAGE_STATUS=('decisions-pass','final-wrong')


def spacing(root, run):
    report = report_path(root, run)
    summary(root, run)
    path = Path(root).parent/'spacing'/run
    if path.is_symlink() or (path/'summary.json').is_symlink():
        raise ValueError('spacing symlink is not supported')
    if not (path/'summary.json').is_file():
        return dict(available=False, run_id=run)
    data = json.loads((path/'summary.json').read_text(encoding='utf-8'))
    fingerprint = hashlib.sha256((report/'summary.json').read_bytes()).hexdigest()
    if data.get('status') != 'complete' or data.get('run_id') != run or data.get('schema_version') != 1 or data.get('report_summary_sha256') != fingerprint:
        raise ValueError('stale spacing audit')
    return dict(data, available=True)


def arguments(query):
    values=parse_qs(query,keep_blank_values=True)
    if any(len(v)!=1 for v in values.values()):raise ValueError('중복 검색 인자')
    result={k:v[0] for k,v in values.items()}
    if set(result)-{'run','category','theme','route','status','flagged','page','scope','id','q'}:
        raise ValueError('알 수 없는 검색 인자')
    return result


def report_path(root,run):
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]{0,95}',run or ''):
        raise ValueError('잘못된 보고서 ID')
    root=Path(root).resolve();path=root/run
    if path.is_symlink() or not path.is_dir():raise ValueError('보고서를 찾을 수 없습니다')
    if any((path/name).is_symlink() for name in ('summary.json','report.sqlite',DECISIONS_FILE)):
        raise ValueError('보고서 symlink는 지원하지 않습니다')
    return path


def summary(root,run):
    path=report_path(root,run)
    with (path/'summary.json').open(encoding='utf-8') as f:data=json.load(f)
    if data.get('status')!='complete' or data.get('schema_version')!=1 or data.get('run_id')!=run or not (path/'report.sqlite').is_file():
        raise ValueError('완료되지 않은 보고서')
    return data


def catalog(root):
    root=Path(root);reports=[]
    if root.is_dir():
        for path in sorted(root.iterdir(),reverse=True):
            if not path.is_dir() or not (path/'summary.json').is_file():continue
            try:
                r=summary(root,path.name)
                reports.append({k:r[k] for k in ('run_id','label','created_at','comparison_key','engine','sentences','spans')})
            except (ValueError,OSError,KeyError):continue
    reports.sort(key=lambda r:(r['created_at'],r['run_id']),reverse=True)
    return dict(reports=reports,latest=reports[0]['run_id'] if reports else None)


def stage(path):
    """The Decisions stage of a report, or None."""
    file=Path(path)/DECISIONS_FILE
    if not file.is_file():return None
    data=json.loads(file.read_text(encoding='utf-8'))
    if data.get('schema_version')!=1 or not isinstance(data.get('scores'),dict):raise ValueError('잘못된 Decisions 단계 파일')
    return data


def report(root,run):
    """summary() with the Decisions stage counted overall and per category, when it was run.

    accepted: `different` sentences Decisions accepts; final_wrong: scorable sentences
    neither the benchmark (up to the similar-region level) nor Decisions accepts."""
    data=summary(root,run)
    s=stage(report_path(root,run))
    if not s:return dict(data,decisions=None)
    count={}
    for row in s['scores'].values():
        for key in ('all',row.get('category')):
            c=count.setdefault(key,dict(candidates=0,scored=0,accepted=0))
            c['candidates']+=1;c['scored']+=row.get('p') is not None;c['accepted']+=bool(row.get('accepted'))
    def finish(c,m):
        scorable,ok=m.get('scorable',0),m.get('similar',m.get('variant',m.get('compact',0)))
        wrong=scorable-ok-c['accepted']
        return dict(c,final_wrong=wrong,final_wrong_rate=wrong/scorable if scorable else None,
                    accepted_rate=c['accepted']/scorable if scorable else None)
    empty=dict(candidates=0,scored=0,accepted=0)
    return dict(data,decisions=dict(judge=s.get('judge'),model=s.get('model'),question=s.get('question'),
        threshold=s.get('threshold'),rule=s.get('rule'),scored_at=s.get('scored_at'),
        sentences=finish(count.get('all',empty),data['sentences']),
        categories={c['id']:finish(count.get(c['id'],empty),c['sentences']) for c in data['categories']}))


def decode(raw):
    return json.loads(zlib.decompress(raw).decode() if isinstance(raw,bytes) else raw)

def cases(root,args):
    run=args.get('run');path=report_path(root,run)
    summary(root,run) # completed runs only
    scope=args.get('scope','sentence')
    if scope not in ('sentence','span'):raise ValueError('잘못된 집계 단위')
    page=int(args.get('page','0'))
    if not 0<=page<=100000:raise ValueError('잘못된 페이지')
    if len(args.get('q',''))>200:raise ValueError('검색어는 200자 이내')
    clauses,params=[],[]
    if args.get('id'):
        clauses.append('c.id=?');params.append(args['id'])
    judged=stage(path)
    accepted=[k for k,v in (judged or {}).get('scores',{}).items() if v.get('accepted')]
    status=args.get('status','')
    if status in STAGE_STATUS:
        if scope!='sentence':raise ValueError('Decisions 단계 필터는 문장 집계에서 사용합니다')
        clauses.append("c.status='different' AND c.id IN (SELECT id FROM temp.accepted)" if status=='decisions-pass'
                       else "((c.status='different' AND c.id NOT IN (SELECT id FROM temp.accepted)) OR c.status='error')")
    for key,column in [('category','c.category'),('theme','s.theme'),('route','s.route' if scope=='span' else 'c.route'),('status','s.status' if scope=='span' else 'c.status')]:
        if key=='status' and status in STAGE_STATUS:continue
        if args.get(key):
            if key=='theme' and scope!='span':raise ValueError('주제 필터는 구간 집계에서 사용합니다')
            clauses.append(column+'=?');params.append(args[key])
    if args.get('flagged'):
        if args['flagged']!='1':raise ValueError('잘못된 검토 신호 필터')
        clauses.append('c.flagged=1')
    if args.get('q'):
        clauses.append("instr(c.text,?)>0");params.append(args['q'])
    table='cases c JOIN spans s ON s.case_id=c.id' if scope=='span' else 'cases c'
    where=' WHERE '+' AND '.join(clauses) if clauses else ''
    db=sqlite3.connect('file:'+quote(str(path/'report.sqlite'))+'?mode=ro',uri=True,timeout=3)
    try:
        db.execute('CREATE TEMP TABLE accepted(id TEXT PRIMARY KEY)')
        db.executemany('INSERT INTO temp.accepted VALUES (?)',[(k,) for k in accepted])
        count=db.execute('SELECT count(*) FROM '+table+where,params).fetchone()[0]
        columns='c.details,s.details' if scope=='span' else 'c.details'
        rows=db.execute('SELECT '+columns+' FROM '+table+where+' ORDER BY '+('s.id' if scope=='span' else 'c.id')+' LIMIT ? OFFSET ?',params+[50,page*50]).fetchall()
        entries=[]
        for row in rows:
            full=decode(row[0])
            if args.get('id'):
                item=full
            else:
                item={k:full[k] for k in ('id','category','category_en','text','expected','actual','status','route','flags')}
            if scope=='span':item['selected_span']=decode(row[1])
            got=(judged or {}).get('scores',{}).get(item['id'])
            if got:item['decisions']=dict(p=got.get('p'),accepted=got.get('accepted'),unread=got.get('unread'),value=got.get('value'))
            entries.append(item)
        return dict(run=run,scope=scope,total=count,page=page,page_size=50,entries=entries)
    finally:db.close()
