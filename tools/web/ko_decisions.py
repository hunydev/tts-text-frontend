"""OpenAI Decisions as the last stage of the Korean benchmark (2026-10-12, project-owner request).

The reference benchmark decides first (korean_release.py: exact, spacing,
policy variant, similar changed region). Only the sentences it leaves as
`different` go to Decisions (tools/web/decisions.py: the Jev question and
Korean reading conventions, asked as one predicate). A sentence is accepted
when Decisions gives 0.7 or more, no digit, Latin letter or symbol is left in
the output (jev.unread), and the number check finds no changed value
(value_check). What is left is wrong even for Decisions, the final number.

  python3 tools/web/ko_decisions.py RUN [--root REPORTS] [--workers 8]

Writes RUN/decisions-final.json; scored pairs stay in
benchmark/generated/decisions-cache/ko-KR.jsonl, so a rerun or an unchanged
output costs nothing. The key stays on the server (~/.config/ttf/openai_api_key).
"""
import argparse
import concurrent.futures
import datetime
import json
import sqlite3
import sys
from pathlib import Path
from urllib.parse import quote

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import decisions  # noqa: E402
import jev  # noqa: E402
import ko_reports  # noqa: E402
import value_check  # noqa: E402

FILE = ko_reports.DECISIONS_FILE
THRESHOLD = 0.7
RULE = ("Decisions %.1f 이상, 결과에 숫자·로마자·기호가 남지 않음, 숫자 값 대조(value_check)에서 바뀐 값 없음" % THRESHOLD)


def candidates(root, run):
    path = ko_reports.report_path(root, run)
    ko_reports.summary(root, run)
    db = sqlite3.connect("file:" + quote(str(path / "report.sqlite")) + "?mode=ro", uri=True, timeout=3)
    try:
        rows = db.execute("SELECT id, category, details FROM cases WHERE status='different' ORDER BY id").fetchall()
    finally:
        db.close()
    out = []
    for cid, category, raw in rows:
        d = ko_reports.decode(raw)
        if d.get("actual"):
            out.append(dict(id=cid, category=category, text=d["text"], actual=d["actual"]))
    return out


def score(root, run, judge=None, workers=8, progress=None):
    path = ko_reports.report_path(root, run)
    rows = candidates(root, run)
    judge = judge or decisions.Judge()
    done = [0]

    def one(r):
        p = judge.score(r["text"], r["actual"])
        done[0] += 1
        if progress and done[0] % 500 == 0:
            progress(done[0], len(rows))
        return p

    with concurrent.futures.ThreadPoolExecutor(workers) as pool:
        probs = list(pool.map(one, rows))
    scores = {}
    for r, p in zip(rows, probs):
        left = jev.unread(r["actual"], "ko-KR")
        value = value_check.check(r["text"], r["actual"]).get("status")
        accepted = p is not None and p >= THRESHOLD and not left and value != "mismatch"
        scores[r["id"]] = dict(p=p, category=r["category"], unread=bool(left), value=value, accepted=accepted)
    data = dict(schema_version=1, judge="openai-decisions", model=decisions.MODEL, question=decisions.QUESTION_ID,
                threshold=THRESHOLD, rule=RULE,
                scored_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                input_tokens=getattr(judge, "tokens", None), scores=scores)
    tmp = path / (FILE + ".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False) + "\n", encoding="utf-8")
    tmp.replace(path / FILE)
    return data


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("run")
    p.add_argument("--root", type=Path, default=ko_reports.DEFAULT_ROOT)
    p.add_argument("--workers", type=int, default=8)
    a = p.parse_args()
    d = score(a.root, a.run, workers=a.workers, progress=lambda x, n: print("decisions %d/%d" % (x, n), file=sys.stderr))
    s = ko_reports.report(a.root, a.run)["decisions"]["sentences"]
    print(json.dumps(dict(run=a.run, input_tokens=d["input_tokens"], **s)))
