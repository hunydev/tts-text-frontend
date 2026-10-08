"""Japanese TN measurement reports (/ja-quality), read from japanese_jsut.py output.

Runs live in benchmark/generated/ja-jsut (local only): summary-<tag>.json and
results-<tag>.jsonl. Nothing here runs the engine.
"""

import json
import re
from pathlib import Path

DEFAULT_ROOT = Path.home() / "tts-text-frontend/benchmark/generated/ja-jsut"
TAG = re.compile(r"^[A-Za-z0-9._-]{1,64}$")
FILTERS = ("span-miss", "ours-miss", "ours-better", "ours-worse", "reviewed", "all")


def runs(root):
    found = []
    for path in sorted(Path(root).glob("summary-*.json"), key=lambda p: -p.stat().st_mtime):
        tag = path.stem[len("summary-"):]
        if TAG.match(tag) and (Path(root) / f"results-{tag}.jsonl").is_file():
            found.append(dict(tag=tag, mtime=int(path.stat().st_mtime)))
    return found


def _tag(root, tag):
    if not tag:
        listed = runs(root)
        if not listed:
            raise FileNotFoundError("no Japanese measurement yet")
        tag = listed[0]["tag"]
    if not TAG.match(tag):
        raise ValueError("bad run tag")
    return tag


def summary(root, tag=None):
    tag = _tag(root, tag)
    return json.loads((Path(root) / f"summary-{tag}.json").read_text(encoding="utf-8"))


def cases(root, tag=None, kind="span-miss", query="", offset=0, limit=50):
    tag = _tag(root, tag)
    if kind not in FILTERS:
        raise ValueError("bad filter")
    rows = []
    for line in (Path(root) / f"results-{tag}.jsonl").read_text(encoding="utf-8").splitlines():
        r = json.loads(line)
        ours, raw = r.get("ours", {}), r.get("raw", {})
        keep = {
            "span-miss": any(not s["ok"] for s in r.get("spans", [])),
            "ours-miss": not ours.get("exact"),
            "ours-better": ours.get("exact") and not raw.get("exact"),
            "ours-worse": raw.get("exact") and not ours.get("exact"),
            "reviewed": bool(r.get("review")),
            "all": True,
        }[kind]
        if keep and (not query or query in r["text"] or query in r["reference"]):
            rows.append(r)
    offset = max(0, int(offset))
    return dict(tag=tag, total=len(rows), offset=offset, rows=rows[offset:offset + limit])
