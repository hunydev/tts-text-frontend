#!/usr/bin/env python3
"""Collect South Korean places from Wikidata (CC0): items in South Korea with coordinates and
Korean and English labels.

    collect_places.py OUT_DIR

Writes OUT_DIR/places.jsonl (qid, ko, en, aliases, classes) and summary.json.
"""
import json
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

UA = "tts-text-frontend-lexicon/0.1 (Korean place names; https://github.com/hunydev/tts-text-frontend)"
QUERY = """
SELECT ?item ?en ?ko (GROUP_CONCAT(DISTINCT ?alias; separator="|") AS ?aliases)
       (GROUP_CONCAT(DISTINCT ?cls; separator="|") AS ?classes) WHERE {
  ?item wdt:P17 wd:Q884 ; wdt:P625 ?coord .
  ?item rdfs:label ?ko FILTER(LANG(?ko) = "ko")
  ?item rdfs:label ?en FILTER(LANG(?en) = "en")
  OPTIONAL { ?item skos:altLabel ?alias FILTER(LANG(?alias) = "en") }
  OPTIONAL { ?item wdt:P31 ?cls }
} GROUP BY ?item ?en ?ko ORDER BY ?item LIMIT %d OFFSET %d
"""


def run(query):
    url = "https://query.wikidata.org/sparql?" + urllib.parse.urlencode({"query": query, "format": "json"})
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept": "application/sparql-results+json"})
    with urllib.request.urlopen(req, timeout=180) as r:
        return json.load(r)["results"]["bindings"]


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    rows, offset, page = [], 0, 5000
    while True:
        for attempt in range(4):
            try:
                got = run(QUERY % (page, offset))
                break
            except Exception as exc:  # noqa: BLE001 - network retries
                print("retry", attempt, type(exc).__name__, file=sys.stderr)
                time.sleep(15 * (attempt + 1))
        else:
            raise SystemExit("Wikidata query failed at offset %d" % offset)
        rows += [dict(qid=b["item"]["value"].rsplit("/", 1)[1], en=b["en"]["value"], ko=b["ko"]["value"],
                      aliases=[a for a in b.get("aliases", {}).get("value", "").split("|") if a],
                      classes=[c.rsplit("/", 1)[1] for c in b.get("classes", {}).get("value", "").split("|") if c])
                 for b in got]
        print("offset", offset, "rows", len(rows), flush=True)
        if len(got) < page:
            break
        offset += page
        time.sleep(3)
    (out / "places.jsonl").write_text("".join(json.dumps(r, ensure_ascii=False) + "\n" for r in rows),
                                      encoding="utf-8")
    (out / "summary.json").write_text(json.dumps(dict(places=len(rows), collected_at=time.strftime("%Y-%m-%d")),
                                                 indent=1) + "\n")


if __name__ == "__main__":
    main()
