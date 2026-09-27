"""Immutable CJK contracts, with explicit executed pending cases at stage 2."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import subprocess

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/web"), str(ROOT / "tools/evaluation")]
from native import Engine
from evaluate import frontend_settings, frontend_flags, read_corpus

CORPUS = ROOT / "tests/evaluation/cjk_reading_v22.jsonl"
# User-revised reference: Hiragana absorption plus preserve/drop corrections (2026-09-27).
SHA256 = "721c212c3ab21c156578725b917de8c6b0c8e9dc8968ce75af85805d6a7e558e"
PENDING = {"kana-loanword-seoul": "cjk_word", "kana-loanword-udon": "cjk_word",
           "combo-both-on": "hanja_reading"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    backend = parser.add_mutually_exclusive_group(required=True)
    backend.add_argument("--library", type=Path)
    backend.add_argument("--cli", type=Path, help="instrumented executable for ASan/UBSan CI")
    parser.add_argument("--resources", type=Path, required=True)
    parser.add_argument("--stage", choices=(2, 3), type=int, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    assert hashlib.sha256(CORPUS.read_bytes()).hexdigest() == SHA256, "Frozen contract changed"
    rows = read_corpus(CORPUS)
    kana = [r for r in rows if r["kana"] == "korean"]
    assert len(rows) == 86 and len(kana) == 30
    assert set(PENDING) <= {r["id"] for r in kana}
    cases, engines = [], {}
    try:
        for row in kana if args.stage == 2 else rows:
            opts = dict(frontend_settings(row), default_language=row["default_language"],
                        detailed_trace=True)
            key = json.dumps(opts, sort_keys=True)
            if args.cli:
                process = subprocess.run([str(args.cli.resolve()), "normalize",
                    "--resources", str(args.resources.resolve()), "--language", row["default_language"],
                    *frontend_flags(row), "--", row["text"]], check=True, capture_output=True, text=True)
                assert process.stdout.endswith("\n"), "CLI framing newline missing"
                result = dict(text=process.stdout[:-1])
            else:
                if key not in engines:
                    engines[key] = Engine(args.library.resolve(), args.resources.resolve(), opts)
                result = engines[key].normalize(row["text"].encode())
            pending = args.stage == 2 and row["id"] in PENDING
            status = "pending" if pending else "passed" if result["text"] == row["expected"] else "failed"
            case = dict(id=row["id"], status=status, text=row["text"], expected=row["expected"],
                        actual=result["text"], dependency=PENDING[row["id"]] if pending else None,
                        note="3단계 대기" if pending else "", result=result)
            cases.append(case)
            if status != "passed":
                print(f"{status.upper()} {row['id']}: {result['text']!r}; "
                      f"gold={row['expected']!r}" + (f" (3단계 대기: {PENDING[row['id']]})" if pending else ""))
        counts = {s: sum(c["status"] == s for c in cases) for s in ("passed", "failed", "pending")}
        assert counts["pending"] == (3 if args.stage == 2 else 0)
        report = dict(stage=args.stage, corpus_sha256=SHA256, **counts, cases=cases,
                      kana_total=30, kana_passed=sum(c["status"] == "passed" for c in cases
                          if c["id"] in {r["id"] for r in kana}))
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(dict(stage=args.stage, **counts, kana_total=30, kana_passed=report["kana_passed"])))
        return int(bool(counts["failed"]))
    finally:
        for engine in engines.values():
            engine.close()


if __name__ == "__main__":
    raise SystemExit(main())
