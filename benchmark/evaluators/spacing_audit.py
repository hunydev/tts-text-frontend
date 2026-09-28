#!/usr/bin/env python3
"""Explain whitespace-only differences without changing references or scores.

These are diagnostic candidates, not automatic linguistic correctness labels.
Only comparable text (identical non-whitespace characters) can be audited.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import re
import sqlite3
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from benchmark.common import sha256

LABELS = {
    'myriad-group': ('만·억·조 수 묶음 경계', '맞춤법 제44항에 따라 우선 점검. 금액 문서의 붙임 관행과 TTS 출력 목적을 구분합니다.'),
    'numeral-internal': ('수사 내부 발화 간격', '십 이·열 두처럼 수사 내부를 나눈 차이. 일반 맞춤법 오류와 분리하며 기본 수사를 일괄 분해하지 않습니다.'),
    'digit-sequence': ('번호·소수 자릿수 간격', '자릿수·전화 그룹의 발화 선호. 별도 출력 정책 후보이며 읽은 값의 오류가 아닙니다.'),
    'lexical-word': ('단어 내부 간격', '유월·시월 또는 약어·관용형 내부. 사전 표기와 발화 표기를 검토합니다.'),
    'unit-or-affix': ('단위·접사 경계 검토', '수와 단위는 띄어 쓰는 것이 원칙이나 순서 표기·합성어에는 예외가 있습니다. 문맥 검토가 필요합니다.'),
    'layout': ('줄바꿈·중복 공백', '공백 종류나 개수의 차이. 문단 경계와 단어 경계를 구분합니다.'),
    'boundary-review': ('나머지 경계 검토', '원문·조사·보호 구간·고유명사 등을 확인해야 합니다. 자동 오류 판정이 아닙니다.'),
}
NUMERIC = {'NUMBER','CARDINAL','COUNTER','ORDINAL','DATE','TIME','DATETIME',
           'CURRENCY','PERCENT','MEASURE','FRACTION','RANGE'}
SINO = '영공일이삼사오육칠팔구십백천만억조경해자양간정'
NATIVE = ('열한','열두','열세','열네','열다섯','열여섯','열일곱','열여덟','열아홉',
          '스물','서른','마흔','쉰','예순','일흔','여든','아흔')


def gaps(text):
    """Unicode character gaps; no casefold, NFKC or punctuation removal."""
    letters, spaces = [], {}
    for char in text:
        if char.isspace(): spaces[len(letters)] = spaces.get(len(letters), '') + char
        else: letters.append(char)
    return ''.join(letters), spaces


def category(span, local, text):
    if span is None: return 'boundary-review'
    kind, unit = span.get('semantic_type'), span.get('unit','')
    if kind in {'PHONE','DIGIT_SEQUENCE'}: return 'digit-sequence'
    if kind in {'INITIALISM','ACRONYM','ABBREVIATION','ALNUM_TERM','WORD'}:
        return 'lexical-word'
    if kind not in NUMERIC: return 'boundary-review'
    if not 0 < local < len(text): return 'unit-or-affix'
    if text in ('유월','시월'): return 'lexical-word'
    # 일 can be the numeric one or the calendar/counting unit. Prefer the
    # explicit IR unit when the differing boundary is immediately before it.
    if kind == 'COUNTER' and unit and text.endswith(unit) and local == len(text)-len(unit):
        return 'unit-or-affix'
    # 구 is both a digit and an enormous magnitude: never guess that boundary.
    if text[local-1] in '만억조경해자양간정' and text[local] in SINO:
        return 'myriad-group'
    if text[local-1] in SINO and text[local] in SINO:
        point = text.find('점')
        return 'digit-sequence' if 0 <= point < local else 'numeral-internal'
    for stem in NATIVE:
        start=text.find(stem)
        if start>=0 and start < local < start+len(stem): return 'numeral-internal'
        if start==0 and local==len(stem) and text[local:local+1] in '한두세네다여일아':
            return 'numeral-internal'
    return 'unit-or-affix'


def explain(expected, actual, spans):
    if not isinstance(expected,str) or not isinstance(actual,str): return []
    a, wanted = gaps(expected); b, present = gaps(actual)
    if a != b or expected == actual: return []
    # Engine offsets are UTF-8 bytes. Derive character coordinates once, in order.
    positions={0:0}; byte=compact=0
    for char in actual:
        byte+=len(char.encode());compact+=not char.isspace();positions[byte]=compact
    mapped=[]
    for s in spans:
        begin,end=positions.get(s['begin']),positions.get(s['end'])
        if begin is not None and end is not None and begin<end:
            mapped.append((begin,end,s))
    result=[]
    for at in sorted(wanted.keys() | present.keys()):
        w,p=wanted.get(at,''),present.get(at,'')
        if w==p: continue
        owner=next(((lo,hi,s) for lo,hi,s in mapped if lo<at<hi),None)
        if owner:
            lo,hi,s=owner; group=category(s,at-lo,a[lo:hi])
        else: group='boundary-review'
        if w and p: group='layout'
        result.append(dict(offset=at,kind=group,reference_gap=w,output_gap=p,
                           direction='output-missing' if w and not p else 'output-extra' if p and not w else 'layout',
                           left=a[max(0,at-12):at],right=a[at:at+12]))
    return result


def run(report, output):
    report,output=Path(report),Path(output)
    summary=json.loads((report/'summary.json').read_text())
    if summary.get('status')!='complete' or sha256(report/'report.sqlite')!=summary['database_sha256']:
        raise ValueError('incomplete or changed source report')
    if output.exists(): raise ValueError('audit exists; choose a new output')
    counts=Counter();case_counts=Counter();directions=defaultdict(Counter);examples=defaultdict(list)
    combinations=Counter();total=0;changes=0;categories={}
    with sqlite3.connect('file:'+str((report/'report.sqlite').resolve())+'?mode=ro',uri=True) as db:
        for cid,raw in db.execute("SELECT id,details FROM cases WHERE status='spacing' ORDER BY id"):
            row=json.loads(zlib.decompress(raw));events=explain(row['expected'],row['actual'],row['engine_spans'])
            if not events: raise ValueError('spacing row lacks a comparable boundary: '+cid)
            total+=1;changes+=len(events);seen=set()
            cid_group=row['category_en']
            if cid_group not in categories:
                categories[cid_group]=dict(id=cid_group,name=row.get('category',cid_group),sentences=0,
                    boundaries=0,counts=Counter(),case_counts=Counter(),directions=defaultdict(Counter),examples=defaultdict(list))
            cat=categories[cid_group];cat['sentences']+=1;cat['boundaries']+=len(events)
            for event in events:
                key=event['kind'];counts[key]+=1;directions[key][event['direction']]+=1;seen.add(key)
                if len(examples[key])<6 and not any(x['id']==cid for x in examples[key]):
                    examples[key].append(dict(id=cid,category=row['category_en'],**event))
                cat['counts'][key]+=1;cat['directions'][key][event['direction']]+=1
                if len(cat['examples'][key])<6 and not any(x['id']==cid for x in cat['examples'][key]):
                    cat['examples'][key].append(dict(id=cid,category=cid_group,**event))
            case_counts.update(seen);combinations['+'.join(sorted(seen))]+=1
            cat['case_counts'].update(seen)
    if total!=summary['sentences']['spacing_only']: raise ValueError('denominator mismatch')
    result=dict(schema_version=1,status='complete',run_id=summary['run_id'],
        report_summary_sha256=sha256(report/'summary.json'),database_sha256=summary['database_sha256'],
        analyzer_sha256=sha256(Path(__file__)),scope='whitespace-only sentences; diagnostic candidates, not correctness scores',
        sentences=total,boundaries=changes,overlapping_categories=True,
        groups=[dict(id=k,title=LABELS[k][0],guidance=LABELS[k][1],boundaries=counts[k],sentences=case_counts[k],directions=dict(directions[k]),examples=examples[k]) for k in LABELS],
        categories=[dict(id=c['id'],name=c['name'],sentences=c['sentences'],boundaries=c['boundaries'],
            groups=[dict(id=k,title=LABELS[k][0],guidance=LABELS[k][1],boundaries=c['counts'][k],
                sentences=c['case_counts'][k],directions=dict(c['directions'][k]),examples=c['examples'][k]) for k in LABELS])
            for c in sorted(categories.values(),key=lambda c:c['id'])],
        combinations=dict(combinations),source_reference_modified=False)
    output.mkdir(parents=True);(output/'summary.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(dict(sentences=total,boundaries=changes,groups=dict(counts)),ensure_ascii=False))
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--report',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();run(args.report,args.out)
