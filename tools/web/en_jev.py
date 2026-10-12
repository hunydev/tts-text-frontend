"""Jev as the last stage of the English benchmark (2026-10-12, project-owner request).

The reference benchmark decides first (english_sentences.py levels). Only its
misses - no level, not a reading the dataset marks wrong, no engine error - go
to Jev (tools/web/jev.py, the same question as the web page's Jev button). A
miss Jev accepts (noul >= jev.PASS) is shown apart on /en-quality; what is left
is wrong even for Jev, the final number.

  python3 tools/web/en_jev.py RUN [--root REPORTS] [--cache CACHE.jsonl]

Writes RUN/jev-final.json; pairs already scored come from the cache, so a rerun
or an unchanged output costs nothing. The API key stays on the server
(~/.config/ttf/typesafe_api_key or TYPESAFE_API_KEY).
"""
import argparse
import datetime
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import en_reports  # noqa: E402
import jev  # noqa: E402

FILE = en_reports.JEV_FILE
CACHE = HERE.parents[1] / "benchmark/generated/jev-cache/en-US.jsonl"


def score(root, run, cache=CACHE, batch=None, progress=None):
    path = en_reports.report_path(root, run)
    scored, _ = en_reports.rows(root, run)
    misses = [r for r in scored if r["result"] == "miss" and r.get("actual")]
    pairs = list(dict.fromkeys((r["text"], r["actual"]) for r in misses))
    results, fresh = (batch or jev.batch)(pairs, "en-US", cache, progress=progress)
    models = sorted({results[p][1] for p in pairs})
    data = dict(schema_version=1, question=jev.QUESTION_ID, model=", ".join(models), threshold=jev.PASS,
                scored_at=datetime.datetime.now(datetime.timezone.utc).isoformat(), newly_scored=fresh,
                scores={r["id"]: results[(r["text"], r["actual"])][0] for r in misses})
    tmp = path / (FILE + ".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False) + "\n", encoding="utf-8")
    tmp.replace(path / FILE)
    return data


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("run")
    p.add_argument("--root", type=Path, default=en_reports.DEFAULT_ROOT)
    p.add_argument("--cache", type=Path, default=CACHE)
    a = p.parse_args()
    d = score(a.root, a.run, a.cache, progress=lambda done, n: print("jev %d/%d" % (done, n), file=sys.stderr))
    s = en_reports.report(a.root, a.run)["jev"]["sentences"]
    print(json.dumps(dict(run=a.run, newly_scored=d["newly_scored"], **s)))
