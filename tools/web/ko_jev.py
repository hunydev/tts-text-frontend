"""Jev on the Korean benchmark's `different` sentences, next to the Decisions stage
(2026-10-12, project-owner request: keep the sentences both judges reject).

Same sentences and acceptance rule as tools/web/ko_decisions.py (score 0.7 or more,
nothing left unread, no changed number value); the judge is TypeSafe Jev
(tools/web/jev.py). Writes RUN/jev-final.json in the same shape as
decisions-final.json; pairs already scored come from
benchmark/generated/jev-cache/ko-KR.jsonl.

  python3 tools/web/ko_jev.py RUN [--root REPORTS]
"""
import argparse
import datetime
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import jev  # noqa: E402
import ko_decisions  # noqa: E402
import ko_reports  # noqa: E402
import value_check  # noqa: E402

FILE = ko_reports.JEV_FILE
CACHE = HERE.parents[1] / "benchmark/generated/jev-cache/ko-KR.jsonl"


def score(root, run, cache=CACHE, batch=None, progress=None):
    path = ko_reports.report_path(root, run)
    rows = ko_decisions.candidates(root, run)
    pairs = list(dict.fromkeys((r["text"], r["actual"]) for r in rows))
    results, fresh = (batch or jev.batch)(pairs, "ko-KR", cache, progress=progress)
    scores = {}
    for r in rows:
        noul, _ = results[(r["text"], r["actual"])]
        left = jev.unread(r["actual"], "ko-KR")
        value = value_check.check(r["text"], r["actual"]).get("status")
        scores[r["id"]] = dict(p=noul, category=r["category"], unread=bool(left), value=value,
                               accepted=noul >= jev.PASS and not left and value != "mismatch")
    models = sorted({results[p][1] for p in pairs})
    data = dict(schema_version=1, judge="typesafe-jev", model=", ".join(models), question=jev.QUESTION_ID,
                threshold=jev.PASS, rule=ko_decisions.RULE.replace("Decisions", "Jev"),
                scored_at=datetime.datetime.now(datetime.timezone.utc).isoformat(), newly_scored=fresh,
                scores=scores)
    tmp = path / (FILE + ".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False) + "\n", encoding="utf-8")
    tmp.replace(path / FILE)
    return data


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("run")
    p.add_argument("--root", type=Path, default=ko_reports.DEFAULT_ROOT)
    p.add_argument("--cache", type=Path, default=CACHE)
    a = p.parse_args()
    d = score(a.root, a.run, a.cache, progress=lambda x, n: x % 2000 == 0 and print("jev %d/%d" % (x, n), file=sys.stderr))
    r = ko_reports.report(a.root, a.run)
    print(json.dumps(dict(run=a.run, newly_scored=d["newly_scored"], jev=r["jev"]["sentences"], both=r["both"]["sentences"])))
