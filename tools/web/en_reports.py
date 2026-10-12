"""Read-only, paginated access to completed local English sentence reports.

A report is a directory written by benchmark/evaluators/english_sentences.py:
summary.json, results.jsonl (scored sentences) and out-of-scope.jsonl. Rows are
untrusted text; the page renders them with textContent only.

jev-final.json (tools/web/en_jev.py, 2026-10-12) holds Jev scores for the
benchmark's misses only: a miss Jev accepts is counted apart, and what is left
is wrong even for Jev.
"""
import json
from pathlib import Path
import re
import threading
from urllib.parse import parse_qs

DEFAULT_ROOT = Path(__file__).resolve().parents[2] / "benchmark/generated/en-eval/reports"
LEVELS = ("exact", "compact", "loose", "variant")
RESULTS = LEVELS + ("miss", "rejected", "error", "out-of-scope", "jev-pass", "final-wrong")
JEV_FILE = "jev-final.json"
PAGE = 50
_cache, _lock = {}, threading.Lock()


def arguments(query):
    values = parse_qs(query, keep_blank_values=True)
    if any(len(v) != 1 for v in values.values()):
        raise ValueError("duplicate argument")
    result = {k: v[0] for k, v in values.items()}
    if set(result) - {"run", "baseline", "dataset", "result", "change", "page", "id", "q"}:
        raise ValueError("unknown argument")
    return result


def report_path(root, run):
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,95}", run or ""):
        raise ValueError("bad report id")
    path = Path(root).resolve() / run
    if path.is_symlink() or not path.is_dir():
        raise ValueError("report not found")
    if any((path / name).is_symlink() for name in ("summary.json", "results.jsonl", "out-of-scope.jsonl",
                                                   JEV_FILE)):
        raise ValueError("symlinked report files are not supported")
    return path


def summary(root, run):
    path = report_path(root, run)
    if not (path / "summary.json").is_file():
        raise ValueError("incomplete report")
    with (path / "summary.json").open(encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("status") != "complete" or data.get("schema_version") != 1 or \
            not (path / "results.jsonl").is_file():
        raise ValueError("incomplete report")
    mode = data.get("protocol", {}).get("output_mode", "")
    data = dict(data, run_id=run, label=data.get("label") or "%s (%s)" % (run, mode))
    return data


def catalog(root):
    root, reports = Path(root), []
    if root.is_dir():
        for path in root.iterdir():
            if not path.is_dir() or not (path / "summary.json").is_file():
                continue
            try:
                r = summary(root, path.name)
                reports.append({k: r[k] for k in ("run_id", "label", "created_at", "comparison_key",
                                                  "engine", "sentences")} |
                               dict(output_mode=r["protocol"].get("output_mode")))
            except (ValueError, OSError, KeyError):
                continue
    reports.sort(key=lambda r: (r["created_at"], r["run_id"]), reverse=True)
    # Speech is the product output; open its newest report first.
    speech = [r for r in reports if r["output_mode"] == "speech"]
    latest = (speech or reports)[0]["run_id"] if reports else None
    return dict(reports=reports, latest=latest)


def jev_scores(path):
    """The Jev stage of a report: {threshold, model, scored_at, scores {id: noul}}, or None."""
    file = Path(path) / JEV_FILE
    if not file.is_file():
        return None
    with file.open(encoding="utf-8") as handle:
        data = json.load(handle)
    if data.get("schema_version") != 1 or not isinstance(data.get("scores"), dict):
        raise ValueError("bad Jev stage file")
    return data


def rows(root, run):
    """Scored and out-of-scope rows, cached per file size and mtime."""
    path = report_path(root, run)
    summary(root, run)
    files = [path / "results.jsonl", path / "out-of-scope.jsonl", path / JEV_FILE]
    key = (str(path),) + tuple((f.stat().st_mtime_ns, f.stat().st_size) if f.is_file() else None
                               for f in files)
    with _lock:
        if _cache.get(str(path), (None,))[0] == key:
            return _cache[str(path)][1]
    scored = []
    stage = jev_scores(path)
    jev = stage["scores"] if stage else {}
    with files[0].open(encoding="utf-8") as handle:
        for line in handle:
            row = json.loads(line)
            row["result"] = "error" if row.get("error") else row.get("level") or \
                ("rejected" if row.get("rejected") else "miss")
            if row["result"] == "miss" and row["id"] in jev:
                row["jev"] = jev[row["id"]]
            scored.append(row)
    outside = []
    if files[1].is_file():
        with files[1].open(encoding="utf-8") as handle:
            for line in handle:
                row = json.loads(line)
                outside.append(dict(row, result="out-of-scope",
                                    dataset=row["id"].split(":", 1)[0]))
    value = (scored, outside)
    with _lock:
        if len(_cache) > 8:
            _cache.clear()
        _cache[str(path)] = (key, value)
    return value


def matched(row):
    return row["result"] in LEVELS


def report(root, run):
    """summary() with the Jev stage counted overall and per dataset, when it was run.

    accepted: misses Jev accepts; final_wrong: scorable sentences that neither the
    benchmark (any level) nor Jev accepts (Jev-rejected or unscored misses, readings
    the dataset marks wrong, engine errors)."""
    data = summary(root, run)
    stage = jev_scores(report_path(root, run))
    if not stage:
        return dict(data, jev=None)
    threshold = float(stage.get("threshold", 0.7))
    count = {}
    for row in rows(root, run)[0]:
        for key in ("all", row["dataset"]):
            c = count.setdefault(key, dict(misses=0, scored=0, accepted=0))
            if row["result"] == "miss":
                c["misses"] += 1
                if "jev" in row:
                    c["scored"] += 1
                    c["accepted"] += row["jev"] >= threshold

    def finish(c, m):
        scorable, matched_ = m.get("scorable", 0), m.get("variant", 0)
        wrong = scorable - matched_ - c["accepted"]
        return dict(c, final_wrong=wrong, final_wrong_rate=wrong / scorable if scorable else None,
                    accepted_rate=c["accepted"] / scorable if scorable else None)

    empty = dict(misses=0, scored=0, accepted=0)
    return dict(data, jev=dict(
        model=stage.get("model"), question=stage.get("question"), threshold=threshold,
        scored_at=stage.get("scored_at"),
        sentences=finish(count.get("all", empty), data["sentences"]),
        datasets={k: finish(count.get(k, empty), v) for k, v in data["datasets"].items()}))


def kept(row, result, threshold):
    """Does a row pass the result filter?"""
    if not result:
        return True
    accepted = row["result"] == "miss" and row.get("jev", -1) >= threshold
    if result == "jev-pass":
        return accepted
    if result == "final-wrong":
        return not matched(row) and not accepted
    return row["result"] == result or (result == "miss" and row["result"] in ("rejected", "error"))


def cases(root, args):
    run = args.get("run")
    scored, outside = rows(root, run)
    page = int(args.get("page", "0"))
    if not 0 <= page <= 100000:
        raise ValueError("bad page")
    query = args.get("q", "")
    if len(query) > 200:
        raise ValueError("search text is limited to 200 characters")
    result = args.get("result", "")
    if result and result not in RESULTS:
        raise ValueError("bad result filter")
    change = args.get("change", "")
    if change not in ("", "gained", "lost", "changed"):
        raise ValueError("bad change filter")
    before = None
    if change:
        base = args.get("baseline")
        if not base:
            raise ValueError("a change filter needs a baseline")
        if summary(root, base)["comparison_key"] != summary(root, run)["comparison_key"]:
            raise ValueError("baseline was measured under a different protocol")
        before = {r["id"]: r for r in rows(root, base)[0]}
    stage = jev_scores(report_path(root, run))
    threshold = float(stage.get("threshold", 0.7)) if stage else 2.0
    source = outside if result == "out-of-scope" else scored
    selected = []
    for row in source:
        if args.get("id") and row["id"] != args["id"]:
            continue
        if args.get("dataset") and row["dataset"] != args["dataset"]:
            continue
        if result != "out-of-scope" and not kept(row, result, threshold):
            continue
        if query and query not in row["text"]:
            continue
        if before is not None:
            old = before.get(row["id"])
            if old is None:
                continue
            if change == "gained" and not (matched(row) and not matched(old)):
                continue
            if change == "lost" and not (matched(old) and not matched(row)):
                continue
            if change == "changed" and old.get("actual") == row.get("actual"):
                continue
            row = dict(row, before=old.get("actual"), before_result=old["result"])
        selected.append(row)
    entries = selected[page * PAGE:(page + 1) * PAGE]
    return dict(run=run, total=len(selected), page=page, page_size=PAGE, entries=entries)
