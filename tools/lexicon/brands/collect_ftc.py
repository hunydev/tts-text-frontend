#!/usr/bin/env python3
"""Collect the Fair Trade Commission franchise brand list from data.go.kr.

    collect_ftc.py OUT_DIR YEAR [YEAR ...]

Service FftcBrandRlsInfo2_Service/getBrandinfo (공정거래위원회_가맹정보_브랜드 목록
정보 제공 서비스, 이용허락범위 제한 없음). The service key is read from
~/.config/ttf/ftc_api_key and never printed; errors never echo the request URL.
Writes OUT_DIR/brands-<year>.jsonl with every item of that base year. Keep
OUT_DIR under the ignored benchmark/generated/.
"""
import json
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

BASE = "https://apis.data.go.kr/1130000/FftcBrandRlsInfo2_Service/getBrandinfo"
UA = "tts-text-frontend-lexicon/0.1 (https://github.com/hunydev/tts-text-frontend)"


def page(key, year, number, rows=1000):
    query = urllib.parse.urlencode({"serviceKey": key, "pageNo": number, "numOfRows": rows, "resultType": "json",
                                    "jngBizCrtraYr": year})
    req = urllib.request.Request(BASE + "?" + query, headers={"User-Agent": UA})
    try:
        with urllib.request.urlopen(req, timeout=90) as r:
            body = r.read().decode("utf-8", "replace")
    except Exception as exc:  # noqa: BLE001 - the URL carries the key
        raise SystemExit("request failed: %s %s" % (type(exc).__name__, getattr(exc, "code", "")))
    try:
        data = json.loads(body)
    except ValueError:
        raise SystemExit("not JSON: %s" % body[:300].replace(key, "<key>"))
    if data.get("resultCode") != "00":
        raise SystemExit("result %s %s" % (data.get("resultCode"), data.get("resultMsg")))
    return data


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    key = (Path.home() / ".config/ttf/ftc_api_key").read_text().strip()
    out = Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    for year in sys.argv[2:]:
        first = page(key, year, 1)
        total = int(first.get("totalCount") or 0)
        items = list(first.get("items") or [])
        number = 1
        while len(items) < total:
            number += 1
            more = page(key, year, number).get("items") or []
            if not more:
                break
            items += more
            time.sleep(0.3)
        (out / ("brands-%s.jsonl" % year)).write_text(
            "".join(json.dumps(i, ensure_ascii=False) + "\n" for i in items), encoding="utf-8")
        print(year, "total", total, "fetched", len(items))


if __name__ == "__main__":
    main()
