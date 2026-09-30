#!/usr/bin/env python3
"""Collect companies and brands with an English and a Korean label from Wikidata (CC0).

    collect_wikidata.py OUT_DIR

One SPARQL query per organisation class, paged, with a descriptive User-Agent
and pauses. Writes OUT_DIR/<class>.jsonl rows {qid, en, ko, aliases, cls} and
OUT_DIR/summary.json. Keep OUT_DIR under the ignored benchmark/generated/.
"""
import json
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

UA = "tts-text-frontend-lexicon/0.1 (https://github.com/hunydev/tts-text-frontend)"
CLASSES = {
    "business": "Q4830453", "company": "Q783794", "public_company": "Q891723",
    "enterprise": "Q6881511", "brand": "Q431289", "subsidiary": "Q658255",
    "conglomerate": "Q778575", "automobile_manufacturer": "Q786820", "bank": "Q22687",
    "airline": "Q46970", "video_game_developer": "Q210167", "video_game_publisher": "Q1137109",
    "retail_chain": "Q507619", "software_company": "Q1058914", "technology_company": "Q18388277",
    "record_label": "Q18127", "fashion_brand": "Q2616791", "mobile_operator": "Q1941618",
    "telecom_company": "Q2401749", "online_service": "Q19967801", "website": "Q35127",
}
QUERY = """
SELECT ?item ?en ?ko (GROUP_CONCAT(DISTINCT ?alias; separator="|") AS ?aliases) WHERE {
  ?item wdt:P31 wd:%s .
  ?item rdfs:label ?ko FILTER(LANG(?ko) = "ko")
  ?item rdfs:label ?en FILTER(LANG(?en) = "en")
  OPTIONAL { ?item skos:altLabel ?alias FILTER(LANG(?alias) = "en") }
} GROUP BY ?item ?en ?ko LIMIT %d OFFSET %d
"""


def run(query):
    url = "https://query.wikidata.org/sparql?" + urllib.parse.urlencode({"query": query, "format": "json"})
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept": "application/sparql-results+json"})
    with urllib.request.urlopen(req, timeout=120) as r:
        return json.load(r)["results"]["bindings"]


def main():
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    summary = {}
    for name, qid in CLASSES.items():
        rows, offset, page = [], 0, 5000
        while True:
            for attempt in range(3):
                try:
                    got = run(QUERY % (qid, page, offset))
                    break
                except Exception as exc:  # noqa: BLE001 - network retries
                    print(name, "retry", attempt, exc, file=sys.stderr)
                    time.sleep(10 * (attempt + 1))
            else:
                got = []
            rows += [dict(qid=b["item"]["value"].rsplit("/", 1)[1], en=b["en"]["value"], ko=b["ko"]["value"],
                          aliases=[a for a in b.get("aliases", {}).get("value", "").split("|") if a], cls=name)
                     for b in got]
            if len(got) < page:
                break
            offset += page
            time.sleep(2)
        (out / (name + ".jsonl")).write_text("".join(json.dumps(r, ensure_ascii=False) + "\n" for r in rows),
                                             encoding="utf-8")
        summary[name] = len(rows)
        print(name, len(rows), flush=True)
        time.sleep(2)
    (out / "summary.json").write_text(json.dumps(dict(summary, collected_at=time.strftime("%Y-%m-%d")), indent=1) + "\n")


if __name__ == "__main__":
    main()
