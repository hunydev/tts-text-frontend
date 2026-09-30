#!/usr/bin/env python3
"""Score lexicon entries with TypeSafe Jev: does the Korean reading match the term?

    judge_jev.py [--root DIR] [--status pending] [--limit N] [--workers 8] [--force]

One Noul question per entry (https://docs.typesafe.ai/primitives/noul): the
state is the written term and its Korean reading, the value is the probability
that the reading is a correct Korean pronunciation of the term. Low values sort
first on the /lexicon review page, so a reviewer sees the likeliest mistakes
first. The score is triage, not approval: review_status never changes here.

Calibration on 30 labelled pairs (2026-09-30, jev-1.13.0): AUC 0.90. Wrong
words, translations, garbled or truncated readings score low (탈고해 0.20,
빵농장 0.33, 카카오 for Kakao Games 0.14); a single wrong vowel often does not
(샘소닛 for Samsonite 0.85), so a high score is not proof.

Results go to lexicons/reviews/jev/noul.jsonl, one row per entry id with the
term and pronunciation it judged; an entry whose term or pronunciation changed
is judged again, and the page ignores stale rows. The API key is read from
~/.config/ttf/typesafe_api_key (or TYPESAFE_API_KEY) and never printed.
"""
import argparse
import datetime
import json
import os
import sys
import threading
import time
import urllib.error
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/lexicon"))
import manage  # noqa: E402

URL = "https://api.typesafe.ai/v1/systemone"
MODEL = "jev-latest"
QUESTION_ID = "reading-matches-v1"
CONTEXT = ("Pronunciation dictionary for Korean text-to-speech: how a Latin-script term is read aloud "
           "inside a Korean sentence.")
QUESTION = {
    "type": "noul",
    "instructions": "`korean_reading` is a correct Korean pronunciation of `written_term`: read aloud, it "
                    "sounds like the term as Korean speakers say it.",
    "criteria": {
        "true": "The Hangul transcribes the sound of the whole term, in standard loanword spelling or the "
                "name's established Korean form; small spelling variants are fine.",
        "false": "The Hangul is a wrong or garbled sound, a different word or name, a translation of the "
                 "meaning, or leaves out or adds words.",
    },
}
OUT = "lexicons/reviews/jev/noul.jsonl"


def api_key():
    key = os.environ.get("TYPESAFE_API_KEY")
    if not key:
        path = Path.home() / ".config/ttf/typesafe_api_key"
        key = path.read_text().strip() if path.exists() else ""
    if not key:
        raise SystemExit("TypeSafe API key not found (~/.config/ttf/typesafe_api_key or TYPESAFE_API_KEY)")
    return key


def ask(key, record, attempts=6):
    body = {"model": MODEL,
            "state": {"written_term": record["term"], "korean_reading": record["pronunciation"],
                      "category": record["category"], "context": CONTEXT},
            "questions": {"reading_matches": QUESTION}}
    data = json.dumps(body).encode()
    delay = 1.0
    for attempt in range(attempts):
        req = urllib.request.Request(URL, data=data, method="POST",
                                     headers={"Authorization": "Bearer " + key, "Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                answer = json.loads(r.read())
            return answer["answers"]["reading_matches"]["noul"], answer.get("model", MODEL)
        except urllib.error.HTTPError as exc:
            if exc.code in (401, 403, 422):
                raise SystemExit("TypeSafe rejected the request: HTTP %d" % exc.code)
            if attempt + 1 == attempts:
                raise
        except (urllib.error.URLError, TimeoutError, ConnectionError):
            if attempt + 1 == attempts:
                raise
        time.sleep(delay)
        delay = min(delay * 2, 30)


def load_results(root):
    path = root / OUT
    rows = {}
    if path.exists():
        for line in path.read_text(encoding="utf-8").splitlines():
            if line.strip():
                row = json.loads(line)
                rows[row["id"]] = row
    return rows


def save(root, rows):
    path = root / OUT
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp")
    tmp.write_text("".join(json.dumps(rows[k], ensure_ascii=False, sort_keys=True) + "\n" for k in sorted(rows)),
                   encoding="utf-8")
    tmp.replace(path)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", type=Path, default=ROOT)
    ap.add_argument("--status", help="only entries with this review_status (default: all)")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--workers", type=int, default=8)
    ap.add_argument("--force", action="store_true", help="judge again even when a current row exists")
    args = ap.parse_args()
    key = api_key()
    records, _ = manage.load(args.root)
    rows = load_results(args.root)
    live = {r["id"] for r in records}
    stale = [k for k in rows if k not in live]
    for k in stale:
        del rows[k]
    todo = [r for r in records
            if (not args.status or r["review_status"] == args.status)
            and (args.force or rows.get(r["id"], {}).get("term") != r["term"]
                 or rows[r["id"]].get("pronunciation") != r["pronunciation"]
                 or rows[r["id"]].get("question") != QUESTION_ID)]
    if args.limit:
        todo = todo[:args.limit]
    today = datetime.date.today().isoformat()
    lock = threading.Lock()
    done = [0]

    def work(record):
        value, model = ask(key, record)
        with lock:
            rows[record["id"]] = dict(id=record["id"], term=record["term"], pronunciation=record["pronunciation"],
                                      noul=round(value, 4), model=model, question=QUESTION_ID, checked_at=today)
            done[0] += 1
            if done[0] % 200 == 0:
                save(args.root, rows)
                print("judged", done[0], "of", len(todo), flush=True)

    with ThreadPoolExecutor(args.workers) as pool:
        for future in [pool.submit(work, r) for r in todo]:
            future.result()
    save(args.root, rows)
    values = sorted(r["noul"] for r in rows.values())
    print("judged now", len(todo), "| rows", len(rows), "| dropped stale", len(stale))
    if values:
        low = sum(v < 0.5 for v in values)
        print("median %.2f | below 0.5: %d | below 0.3: %d" % (values[len(values) // 2], low,
                                                              sum(v < 0.3 for v in values)))


if __name__ == "__main__":
    main()
