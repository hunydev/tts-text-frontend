#!/usr/bin/env python3
"""Bounded, model-independent TN case intake and immutable before/after evaluation.

No model API, automatic reference rewriting, engine editing, publishing, or data download.
Exit 0: successful command/candidate passes; 1: quality failure; 2: invalid/incomplete run.
"""
import argparse
import hashlib
import json
import platform
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/evaluation"))
from evaluate import read_corpus, EvaluationError


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def inventory(directory):
    return {str(p.relative_to(directory)): sha(p) for p in sorted(directory.rglob("*"))
            if p.is_file() and p != directory / "snapshot.json"}


def fresh(path):
    path = path.resolve()
    path.mkdir(parents=True, exist_ok=False)
    return path


def command(args, log, allowed=(0,)):
    with log.open("wb") as output:
        run = subprocess.run([str(x) for x in args], cwd=ROOT, stdout=output,
                             stderr=subprocess.STDOUT, timeout=1800)
    if run.returncode not in allowed:
        raise ValueError(f"command failed ({run.returncode}); see {log}")
    return run.returncode


def new_case(args):
    row = dict(id=args.id, category=args.category, text=args.text, expected=args.expected,
               default_language=args.language, output_mode=args.output_mode,
               input_format=args.input_format, unreadable=args.unreadable, hanja=args.hanja, kana=args.kana,
               expected_spans=[], notes=f"Source: {args.source}. Pending independent review. {args.reason}")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    # Never overwrite a reference, even when the requested id is identical.
    target = args.out.open("x", encoding="utf-8")
    try:
        with target:
            target.write(json.dumps(row, ensure_ascii=False) + "\n")
        read_corpus(args.out)
    except (ValueError, UnicodeError):
        args.out.unlink()
        raise
    print(args.out)
    return 0


def snapshot(args):
    build, resources = args.build.resolve(strict=True), args.resources.resolve(strict=True)
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    if f"CMAKE_HOME_DIRECTORY:INTERNAL={ROOT}" not in cache.splitlines():
        raise ValueError("build must be configured from this checkout; snapshot each revision in its own checkout")
    destination = args.out.resolve()
    if destination == resources or resources in destination.parents or destination in resources.parents:
        raise ValueError("snapshot destination must not overlap resources")
    out = fresh(destination)
    # Finish the build before capture. Build logs stay outside the immutable payload.
    command(["cmake", "--build", build, "--parallel", "2"], out / "build.log")
    exe = "ttf.exe" if (build / "ttf.exe").exists() else "ttf"
    shutil.copy2(build / exe, out / exe)
    for pattern in ("libtts_text_frontend.so*", "libtts_text_frontend*.dylib", "*tts_text_frontend*.dll"):
        for path in build.glob(pattern):
            shutil.copy2(path, out / path.name)
    shutil.copytree(resources, out / "resources")
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    diff = subprocess.check_output(["git", "diff", "HEAD", "--"], cwd=ROOT)
    status = subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True)
    write_json(out / "snapshot.json", dict(schema_version=1, executable=exe,
        git_head=revision, worktree_status=status, tracked_diff_sha256=hashlib.sha256(diff).hexdigest(),
        platform=platform.platform(), files=inventory(out),
        note="Artifact hashes identify the capture, including uncommitted builds; git_head alone is not a release identity."))
    print(out)
    return 0


def verify_snapshot(path):
    path = path.resolve(strict=True)
    record = json.loads((path / "snapshot.json").read_text(encoding="utf-8"))
    if record.get("schema_version") != 1 or inventory(path) != record["files"]:
        raise ValueError(f"snapshot changed or has an unsupported format: {path}")
    if record.get("executable") not in ("ttf", "ttf.exe"):
        raise ValueError("invalid executable in snapshot")
    return path, record


def flags(case, resources):
    result = ["--resources", str(resources), "--language", case["default_language"]]
    for key in ("input_format", "output_mode", "unreadable", "max_control_bytes", "hanja", "kana"):
        if key in case:
            result += ["--" + key.replace("_", "-"), str(case[key])]
    for row in case["resource_overrides"]:
        result += ["--" + row["layer"], str(resources / row["path"])]
    return result


def cycle(args):
    before, _ = verify_snapshot(args.baseline)
    after, after_record = verify_snapshot(args.candidate)
    cases = read_corpus(args.corpus)
    if len(cases) > 200:
        raise ValueError("one rule-family cycle is limited to 200 cases; use benchmark/evaluators for external datasets")
    out = args.out.resolve()
    if any(out == p or p in out.parents or out in p.parents for p in (before, after)):
        raise ValueError("reports must be separate from both immutable snapshots")
    out = fresh(out)
    frozen_corpus = out / "cases.jsonl"
    # Validate and serialize one reference set once, before either engine executes.
    frozen_corpus.write_text("".join(json.dumps(c, ensure_ascii=False) + "\n" for c in cases), encoding="utf-8")
    tools_used = [ROOT / "tools/evaluation/evaluate.py", ROOT / "tools/evaluation/compare.py"]
    tool_hashes = {str(p.relative_to(ROOT)): sha(p) for p in tools_used}
    exits = {}
    for label, capture in (("before", before), ("after", after)):
        _, record = verify_snapshot(capture)
        exits[label] = command([sys.executable, tools_used[0], "--ttf", capture / record["executable"],
            "--resources", capture / "resources", "--corpus", frozen_corpus,
            "--chunk-bytes", "1", "7", "64", "--output-dir", out / label],
            out / (label + ".log"), allowed=(0, 1))
        verify_snapshot(capture)
    comparison_exit = command([sys.executable, tools_used[1], "--before", out / "before/report.json",
        "--after", out / "after/report.json", "--output-dir", out / "comparison"],
        out / "compare.log", allowed=(0, 1))
    report = json.loads((out / "after/report.json").read_text())
    traces = fresh(out / "failed-traces")
    by_id = {c["id"]: c for c in cases}
    failures = [c for c in report["cases"] if not c["passed"]]
    for case in failures[:20]:
        command([after / after_record["executable"], "inspect", "--trace",
                 *flags(by_id[case["id"]], after / "resources"), "--", case["text"]],
                traces / (case["id"] + ".txt"))
    verify_snapshot(before)
    verify_snapshot(after)
    if tool_hashes != {str(p.relative_to(ROOT)): sha(p) for p in tools_used}:
        raise ValueError("evaluation tools changed during the cycle")
    summary = json.loads((out / "comparison/comparison.json").read_text())["summary"]
    write_json(out / "cycle.json", dict(schema_version=1, status="completed", corpus_sha256=sha(frozen_corpus),
        baseline_snapshot_sha256=sha(before / "snapshot.json"), candidate_snapshot_sha256=sha(after / "snapshot.json"),
        tools=tool_hashes, candidate_failed=len(failures), trace_count=min(20, len(failures)), comparison=summary))
    print(json.dumps(dict(candidate_failed=len(failures), **summary), ensure_ascii=False))
    return int(bool(exits["after"] or comparison_exit))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest="action", required=True)
    new = subs.add_parser("case", help="write a new reference from supplied expected text (no inference)")
    for name in ("id", "text", "expected", "category", "reason", "source"):
        new.add_argument("--" + name, required=True)
    new.add_argument("--language", default="ko-KR")
    new.add_argument("--output-mode", choices=("text", "speech"), default="speech")
    new.add_argument("--input-format", choices=("plain", "annotated"), default="plain")
    new.add_argument("--unreadable", choices=("preserve", "drop"), default="preserve")
    new.add_argument("--hanja", choices=("passthrough", "korean"), default="passthrough")
    new.add_argument("--kana", choices=("passthrough", "korean"), default="passthrough")
    new.add_argument("--out", type=Path, required=True)
    new.set_defaults(run=new_case)
    snap = subs.add_parser("snapshot", help="build and freeze executable, resources and hashes locally")
    snap.add_argument("--build", type=Path, default=Path("build"))
    snap.add_argument("--resources", type=Path, default=ROOT / "resources")
    snap.add_argument("--out", type=Path, required=True)
    snap.set_defaults(run=snapshot)
    run = subs.add_parser("cycle", help="same-reference before/after text and streaming comparison")
    for name in ("baseline", "candidate", "corpus", "out"):
        run.add_argument("--" + name, type=Path, required=True)
    run.set_defaults(run=cycle)
    args = parser.parse_args()
    try:
        return args.run(args)
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError, EvaluationError) as exc:
        print(f"tn-cycle: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
