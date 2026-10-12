"""OpenAI Decisions judgement of a normalization result (2026-10-12, project-owner
request: a second judge next to Jev, which is weak in Korean).

The same question as tools/web/jev.py (is `spoken` a correct way to read `written`
aloud?), asked as one Decisions `predicate`; the answer is its probability. The
context and the Korean reading conventions are Jev's (jev.context), so the two
judges see the same instruction.

The API key stays on the server (~/.config/ttf/openai_api_key or OPENAI_API_KEY)
and is never printed. Scored pairs are cached in
benchmark/generated/decisions-cache/<language>.jsonl.
"""
import hashlib
import json
import os
import sys
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import jev  # noqa: E402

URL = "https://api.openai.com/v1/decisions"
MODEL = "gpt-6-luna"
QUESTION_ID = "spoken-reading-correct-v3"
ROOT = HERE.parents[1]
CACHE_DIR = ROOT / "benchmark/generated/decisions-cache"


def predicate(language):
    q = jev.question(language)
    return {
        "type": "predicate",
        "name": "reading_correct",
        "instructions": "%s True when: %s False when: %s" % (
            q["instructions"], q["criteria"]["true"], q["criteria"]["false"]),
    }


def evidence(written, spoken, language):
    return "%s\n\nwritten: %s\nspoken: %s" % (jev.context(language), written, spoken)


def instructions(language="ko-KR"):
    """What every Decisions request carries, with the texts as placeholders (no key)."""
    return dict(endpoint=URL, model=MODEL, question_id=QUESTION_ID,
                request={"model": MODEL, "input": evidence("<원문>", "<TN 결과>", language),
                         "questions": [predicate(language)]})


class Judge:
    def __init__(self, key=None, language="ko-KR"):
        if key is None:
            key = os.environ.get("OPENAI_API_KEY")
            path = Path.home() / ".config/ttf/openai_api_key"
            if not key and path.exists():
                key = path.read_text().strip()
        self.key = key or None
        self.language = language
        self.lock = threading.Lock()
        self.path = CACHE_DIR / ("%s.jsonl" % language)
        self.cache = {}
        self.tokens = 0
        if self.path.exists():
            for line in self.path.read_text(encoding="utf-8").splitlines():
                if line.strip():
                    row = json.loads(line)
                    self.cache[row["key"]] = row

    def key_of(self, written, spoken):
        return hashlib.sha256(json.dumps([QUESTION_ID, self.language, written, spoken]).encode()).hexdigest()

    def score(self, written, spoken, retries=4):
        k = self.key_of(written, spoken)
        with self.lock:
            if k in self.cache:
                return self.cache[k]["p"]
        body = {"model": MODEL, "input": evidence(written, spoken, self.language),
                "questions": [predicate(self.language)]}
        request = urllib.request.Request(URL, data=json.dumps(body).encode(), method="POST",
                                         headers={"Authorization": "Bearer " + self.key,
                                                  "Content-Type": "application/json"})
        for attempt in range(retries):
            try:
                with urllib.request.urlopen(request, timeout=60) as response:
                    answer = json.loads(response.read())
                break
            except urllib.error.HTTPError as exc:
                if exc.code in (429, 500, 502, 503, 529) and attempt + 1 < retries:
                    time.sleep(2 ** attempt * 2)
                    continue
                raise RuntimeError("Decisions HTTP %d: %s" % (exc.code, exc.read()[:300].decode("utf-8", "replace")))
        item = next(a for a in answer["answers"] if a.get("name") == "reading_correct")
        if item.get("type") == "refusal":
            p = None
        else:
            p = float(item["probability"])
        usage = answer.get("usage") or {}
        row = {"key": k, "p": p, "model": answer.get("model", MODEL), "question": QUESTION_ID,
               "input_tokens": usage.get("input_tokens")}
        with self.lock:
            self.cache[k] = row
            self.tokens += usage.get("input_tokens") or 0
            CACHE_DIR.mkdir(parents=True, exist_ok=True)
            with self.path.open("a", encoding="utf-8") as f:
                f.write(json.dumps(row) + "\n")
        return p


if __name__ == "__main__":
    j = Judge()
    print("key present:", j.key is not None)
    body = {"model": MODEL, "input": evidence("오후 3시에 2층에서 만나요.", "오후 세 시에 이 층에서 만나요.", "ko-KR"),
            "questions": [predicate("ko-KR")]}
    req = urllib.request.Request(URL, data=json.dumps(body).encode(), method="POST",
                                 headers={"Authorization": "Bearer " + j.key, "Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=60) as r:
            raw = json.loads(r.read())
        print(json.dumps(raw, ensure_ascii=False)[:1200])
    except urllib.error.HTTPError as exc:
        print("HTTP", exc.code, exc.read()[:600].decode("utf-8", "replace"))
