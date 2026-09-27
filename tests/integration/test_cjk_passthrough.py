"""Compare every existing text corpus against a frozen pre-CJK library.

This is a migration gate, not a gold generator. Only OTHER -> HAN/KANA in a
span's script field and audited engine-relative declaration ordinals (§9.1) may
change; all other fields stay equal. New CJK references are separate.
"""
import argparse
import copy
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools/web"), str(ROOT / "tools/evaluation")]
from native import Engine
from evaluate import frontend_settings, read_corpus
from audit_references import read_cases
from trace_order import TraceOrderAudit
sys.path.insert(0, str(ROOT / "tools/cjk"))
from audit_test_sources import cjk_script, survey

PARTICLE_TEST = ROOT / "tests/integration/test_particle_resources.py"
# The source integration test also verifies callback parity and particle trace.
# Repeat its four character-locality inputs here against the frozen old library.
PARTICLE_CASES = [dict(id="character-locality-" + str(i), text=text, output_mode="speech")
                  for i, text in enumerate(("가漢나", "漢나", "가漢이", "가1️⃣나"), 1)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("before", "after", "before-resources", "after-resources", "report"):
        parser.add_argument("--" + name, type=Path, required=True)
    args = parser.parse_args()
    engines = {}
    comparisons, changed_scripts, files, failures = 0, 0, [], []
    script_changes = []
    order_audit = TraceOrderAudit()
    cjk_files = {row["file"] for row in survey(ROOT)}
    try:
        paths = sorted((ROOT / "tests/evaluation").glob("*.jsonl"))
        paths += sorted((ROOT / "tests/golden").glob("*/cases.tsv"))
        paths.append(PARTICLE_TEST)
        for path in paths:
            if path.name == "cjk_reading_v22.jsonl":
                continue
            golden = path.suffix == ".tsv"
            rows = (PARTICLE_CASES if path == PARTICLE_TEST else
                    read_cases(path, "golden-tsv") if golden else read_corpus(path))
            files.append(str(path.relative_to(ROOT)))
            for row in rows:
                opts = dict(frontend_settings(row), default_language=row.get("default_language", "ko-KR"),
                            detailed_trace=True)
                overrides = row.get("resource_overrides", [])
                if path == PARTICLE_TEST:
                    opts["customer_tsv"] = ("speech_character\tko-KR\t漢\t한\n"
                                            "speech_character\tko-KR\t1️⃣\t키캡일\n")
                if golden:
                    opts["default_language"] = "en-US" if path.parent.name == "en" else "ko-KR"
                    overrides = []
                    if path.parent.name in ("domain", "customer"):
                        overrides.append(dict(layer="domain", path=str(ROOT / "tests/fixtures/domain.tsv")))
                    if path.parent.name == "customer":
                        overrides.append(dict(layer="customer", path=str(ROOT / "tests/fixtures/customer.tsv")))
                key = json.dumps([opts, overrides], sort_keys=True)
                if key not in engines:
                    pair = []
                    for library, resources in ((args.before, args.before_resources),
                                                (args.after, args.after_resources)):
                        loaded = [(resources.resolve() / item["path"],
                                   {"domain": 2, "customer": 3}[item["layer"]]) for item in overrides]
                        pair.append(Engine(library.resolve(), resources.resolve(), opts, loaded))
                    engines[key] = pair
                before, after = [engine.normalize(row["text"].encode()) for engine in engines[key]]
                adjusted = copy.deepcopy(after)
                if len(before["spans"]) == len(after["spans"]):
                    for old, new in zip(before["spans"], adjusted["spans"]):
                        if old["script"] == "OTHER" and new["script"] in ("HAN", "KANA"):
                            script_changes.append(dict(file=str(path.relative_to(ROOT)), id=row["id"],
                                input_begin=old["input_begin"], input_end=old["input_end"],
                                surface=row["text"].encode()[old["input_begin"]:old["input_end"]].decode(),
                                before=old["script"], after=new["script"]))
                            new["script"] = "OTHER"
                            changed_scripts += 1
                comparisons += 1
                try:
                    adjusted["trace"] = order_audit.align(
                        key, before["trace"], adjusted["trace"],
                        str(path.relative_to(ROOT)) + ":" + row["id"])
                except ValueError:
                    # The full comparison below reports the unchanged raw captures.
                    pass
                if before != adjusted:
                    failures.append(dict(file=str(path.relative_to(ROOT)), id=row["id"],
                                         before=before, after=after))
        outside = [change for change in script_changes
                   if change["file"] not in cjk_files or
                   not any(cjk_script(c) == change["after"] for c in change["surface"])]
        order_errors = order_audit.finalize()
        report = dict(cases=comparisons, files=files, allowed_script_changes=changed_scripts,
                      script_changes=script_changes, out_of_scope_script_changes=outside,
                      declaration_order_shifts=order_audit.shifts, declaration_order_errors=order_errors,
                      regressions=len(failures), failures=failures)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
        print(json.dumps({k: report[k] for k in ("cases", "allowed_script_changes", "regressions")}))
        return int(bool(failures or outside or order_errors))
    finally:
        for pair in engines.values():
            for engine in pair:
                engine.close()


if __name__ == "__main__":
    raise SystemExit(main())
