"""Original synthetic fixtures: denominator, alignment, restriction and parity gates."""

import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from copy import deepcopy
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
CLI = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / "build/ttf"
RESOURCES = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else ROOT / "resources"
sys.path.insert(0, str(ROOT / "benchmark"))
from common import ensure_output, validate


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


EVALUATE = module("ttf_benchmark_evaluator", ROOT / "benchmark/evaluators/evaluate.py")
COMPARE = module("ttf_benchmark_comparison", ROOT / "benchmark/evaluators/compare.py")


def case(identifier="synthetic-one", status="dataset-reference"):
    written = ["I", "have", "21", "cats", "."]
    spoken = ["I", "have", "twenty one", "cats", ""]
    labels = ["PLAIN", "PLAIN", "CARDINAL", "PLAIN", "PUNCT"]
    row = {
        "schema_version": 1,
        "id": identifier,
        "dataset": "synthetic",
        "split": "test",
        "language": "en-US",
        "text": " ".join(written),
        "expected": "I have twenty one cats",
        "reference_status": status,
        "usage": "benchmark-only",
        "provenance": {
            "source_url": "https://example.invalid/synthetic",
            "revision": "authored-v1",
            "source_file": "synthetic.tsv",
            "source_sha256": "0" * 64,
            "license": "Project-authored synthetic fixture",
        },
        "spans": [],
    }
    offset = 0
    for text, expected, label in zip(written, spoken, labels):
        row["spans"].append(
            {
                "begin": offset,
                "end": offset + len(text.encode()),
                "written": text,
                "spoken": expected,
                "semiotic_class": label,
                "reference_status": status,
            }
        )
        offset += len(text.encode()) + 1
    return row


def short_case(text="A", status="dataset-reference"):
    row = case(status=status)
    row.update(text=text, expected=text)
    row["spans"] = [
        {
            "begin": 0,
            "end": len(text.encode()),
            "written": text,
            "spoken": text,
            "semiotic_class": "PLAIN",
            "reference_status": status,
        }
    ]
    return row


def native_span(start, stop, begin, end):
    return {
        "input_begin": start,
        "input_end": stop,
        "begin": begin,
        "end": end,
        "semantic_type": "WORD",
        "rule": "synthetic",
    }


class EvaluatorTests(unittest.TestCase):
    def test_output_isolation_and_symlink_resolution(self):
        for path in (
            ROOT / "resources/report",
            ROOT / "tests/golden/report",
            ROOT / "README.md",
        ):
            with self.assertRaisesRegex(ValueError, "benchmark/generated"):
                ensure_output(path)
        self.assertEqual(
            ensure_output(ROOT / "benchmark/generated/report"),
            ROOT / "benchmark/generated/report",
        )
        with tempfile.TemporaryDirectory() as directory:
            external = Path(directory)
            self.assertEqual(ensure_output(external / "report"), external / "report")
            (external / "linked").symlink_to(
                ROOT / "resources", target_is_directory=True
            )
            with self.assertRaises(ValueError):
                ensure_output(external / "linked/report")

    def test_native_uncovered_output_is_an_engine_error(self):
        row = short_case()
        for result in (
            {"text": "INJECT A", "spans": [native_span(0, 1, 7, 8)]},
            {"text": "A INJECT", "spans": [native_span(0, 1, 0, 1)]},
            {"text": "A", "spans": []},
        ):
            with self.subTest(result=result):
                scored = EVALUATE.score_case(row, result)
                self.assertIn("invalid-native-result", scored["engine_error"])
                self.assertFalse(scored["sentence_exact"])
                self.assertFalse(scored["spans"][0]["exact"])
                self.assertFalse(scored["spoken_projection_exact"])

    def test_native_input_gaps_overlaps_and_invalid_utf8_boundaries_rejected(self):
        results = [
            {"text": "AB", "spans": [native_span(0, 1, 0, 1)]},
            {"text": "AB", "spans": [native_span(0, 1, 0, 1), native_span(0, 2, 1, 2)]},
            {"text": "AB", "spans": [native_span(0, 1, 0, 1), native_span(2, 2, 1, 2)]},
        ]
        for result in results:
            with self.assertRaises(ValueError):
                EVALUATE.validate_result("AB", result)
        with self.assertRaises(UnicodeError):
            EVALUATE.validate_result(
                "꽃",
                {
                    "text": "꽃",
                    "spans": [native_span(0, 1, 0, 1), native_span(1, 3, 1, 3)],
                },
            )

    def test_native_empty_output_and_surface_integrity(self):
        result = {"text": "", "spans": [native_span(0, 1, 0, 0)]}
        self.assertEqual(EVALUATE.validate_result(".", result), result)
        result = {"text": "A", "spans": [dict(native_span(0, 1, 0, 1), surface="X")]}
        with self.assertRaisesRegex(ValueError, "surface"):
            EVALUATE.validate_result("A", result)

    def test_projection_cannot_discard_lexical_output_on_whitespace_input(self):
        row = short_case("A B")
        row["spans"] = [
            dict(row["spans"][0], begin=0, end=1, written="A", spoken="A"),
            dict(row["spans"][0], begin=2, end=3, written="B", spoken="B"),
        ]
        result = {
            "text": "AWRONG B",
            "spans": [
                native_span(0, 1, 0, 1),
                native_span(1, 2, 1, 7),
                native_span(2, 3, 7, 8),
            ],
        }
        scored = EVALUATE.score_case(row, result)
        self.assertIsNone(scored["engine_error"])
        self.assertFalse(scored["spoken_projection_exact"])
        self.assertEqual(scored["spoken_projection_error"], "non-whitespace-gap-output")
        self.assertIsNone(scored["spoken_projection"])
        # Valid whitespace normalization remains an explicitly permitted projection.
        result = {
            "text": "A\tB",
            "spans": [
                native_span(0, 1, 0, 1),
                native_span(1, 2, 1, 2),
                native_span(2, 3, 2, 3),
            ],
        }
        self.assertTrue(EVALUATE.score_case(row, result)["spoken_projection_exact"])

    def test_projection_checks_leading_and_trailing_input_gaps(self):
        row = short_case(" A ")
        row.update(expected="A")
        row["spans"] = [dict(row["spans"][0], begin=1, end=2, written="A", spoken="A")]
        for output in ("XAX", " A "):
            result = {
                "text": output,
                "spans": [
                    native_span(0, 1, 0, 1),
                    native_span(1, 2, 1, 2),
                    native_span(2, 3, 2, 3),
                ],
            }
            scored = EVALUATE.score_case(row, result)
            self.assertEqual(scored["spoken_projection_exact"], output == " A ")

    def test_projection_denominator_includes_unavailable_and_engine_errors(self):
        ordinary = short_case("A B")
        ordinary["spans"] = [
            dict(ordinary["spans"][0], begin=0, end=1, written="A", spoken="A"),
            dict(ordinary["spans"][0], begin=2, end=3, written="B", spoken="B"),
        ]
        merged = {"text": "A B", "spans": [native_span(0, 3, 0, 3)]}
        rows = [
            EVALUATE.score_case(ordinary, merged),
            EVALUATE.score_case(
                short_case(), {"text": "A", "spans": [native_span(0, 1, 0, 1)]}
            ),
            EVALUATE.score_case(
                short_case(), {"text": "", "spans": []}, "engine failed"
            ),
        ]
        summary = EVALUATE.Summary()
        for row in rows:
            row["streaming"] = []
            summary.add(row)
        projection = summary.finish()["spoken_projection"]["dataset-reference"]
        self.assertEqual(projection["total"], 3)
        self.assertEqual(projection["exact"], 1)
        self.assertEqual(projection["unavailable"], 2)
        self.assertEqual(projection["unaligned"], 2)
        self.assertEqual(projection["available"], 1)
        self.assertEqual(projection["coverage"], 1 / 3)
        self.assertEqual(projection["accuracy"], 1 / 3)
        self.assertEqual(
            projection["unavailable_reasons"],
            {"unaligned-reference-span": 1, "engine-error": 1},
        )

    def test_changed_evaluation_artifacts_prevent_completed_summary(self):
        for changed in ("corpus", "program", "resource", "override"):
            with (
                self.subTest(changed=changed),
                tempfile.TemporaryDirectory() as directory,
            ):
                root = Path(directory)
                corpus, program, resources, override = (
                    root / "corpus.jsonl",
                    root / "ttf",
                    root / "resources",
                    root / "overlay.tsv",
                )
                resources.mkdir()
                resource = resources / "data.tsv"
                corpus.write_text(json.dumps(short_case()) + "\n")
                for path in (program, resource, override):
                    path.write_text("original")
                args = SimpleNamespace(
                    corpus=corpus,
                    ttf=program,
                    library=None,
                    resources=resources,
                    domain=[override],
                    output_dir=root / "report",
                    limit=0,
                    chunk_bytes=[],
                    allow_noncommercial=False,
                )
                target = {
                    "corpus": corpus,
                    "program": program,
                    "resource": resource,
                    "override": override,
                }[changed]

                def run(*_, mutation_target=target):
                    mutation_target.write_text(mutation_target.read_text() + "\n")
                    return {"text": "A", "spans": [native_span(0, 1, 0, 1)]}

                with patch.object(EVALUATE, "Backend") as backend:
                    backend.return_value.run.side_effect = run
                    with self.assertRaisesRegex(ValueError, "changed during execution"):
                        EVALUATE.evaluate(args)
                self.assertFalse((root / "report/summary.json").exists())

    def test_adapter_fingerprint_is_recorded_and_checked(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "cases.jsonl"
            program = root / "ttf"
            resources = root / "resources"
            resources.mkdir()
            corpus.write_text(json.dumps(short_case()) + "\n")
            program.write_text("program")
            hashes = EVALUATE.fingerprint(corpus, program, resources, [], False)
            self.assertIn("tools/evaluation/evaluate.py", hashes["adapter_sha256"])
            native_hashes = EVALUATE.fingerprint(corpus, program, resources, [], True)
            self.assertIn("tools/web/native.py", native_hashes["adapter_sha256"])
            changed = deepcopy(hashes)
            changed["adapter_sha256"]["tools/evaluation/evaluate.py"] = "0" * 64
            args = SimpleNamespace(
                corpus=corpus,
                ttf=program,
                library=None,
                resources=resources,
                domain=[],
                output_dir=root / "report",
                limit=0,
                chunk_bytes=[],
                allow_noncommercial=False,
            )
            with (
                patch.object(EVALUATE, "fingerprint", side_effect=[hashes, changed]),
                patch.object(EVALUATE, "Backend") as backend,
            ):
                backend.return_value.run.return_value = {
                    "text": "A",
                    "spans": [native_span(0, 1, 0, 1)],
                }
                with self.assertRaisesRegex(ValueError, "adapter_sha256"):
                    EVALUATE.evaluate(args)
            self.assertFalse((root / "report/summary.json").exists())

    def test_utf8_offsets_are_validated(self):
        row = case()
        row.update(text="3개", expected=None)
        row["spans"] = [
            {
                "begin": 0,
                "end": 4,
                "written": "3개",
                "spoken": "세 개",
                "semiotic_class": "NUMBER",
                "reference_status": "candidate",
            }
        ]
        self.assertEqual(validate(row), row)
        row["spans"][0]["end"] = 2
        with self.assertRaises((ValueError, UnicodeError)):
            validate(row)

    def test_overlap_and_wrong_written_rejected(self):
        row = case()
        row["spans"][1]["begin"] = 0
        with self.assertRaises(ValueError):
            validate(row)
        row = case()
        row["spans"][0]["written"] = "X"
        with self.assertRaises(ValueError):
            validate(row)

    def test_cross_token_native_span_is_not_fabricated_alignment(self):
        ref = {"begin": 0, "end": 1}
        result = {
            "text": "one two",
            "spans": [{"input_begin": 0, "input_end": 3, "begin": 0, "end": 7}],
        }
        actual, reason, _ = EVALUATE.align_span(ref, result)
        self.assertIsNone(actual)
        self.assertEqual(reason, "crossed-or-missing-input-boundary")

    def test_unlabeled_cannot_claim_gold(self):
        row = case(status="unlabeled")
        with self.assertRaises(ValueError):
            validate(row)
        row["expected"] = None
        with self.assertRaises(ValueError):
            validate(row)
        for span in row["spans"]:
            span["spoken"] = None
        validate(row)

    def test_corea_must_keep_restriction(self):
        row = case()
        row["dataset"] = "coreaspeech"
        with self.assertRaisesRegex(ValueError, "noncommercial"):
            validate(row)

    def test_native_pipeline_denominators_parity_and_compare(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "cases.jsonl"
            ordinary = case()
            candidate = case("synthetic-candidate", "candidate")
            candidate["expected"] = None
            # One deliberate wrong candidate must not reduce reference accuracy.
            candidate["spans"][2]["spoken"] = "wrong candidate"
            unlabeled = case("synthetic-unlabeled", "unlabeled")
            unlabeled["expected"] = None
            for span in unlabeled["spans"]:
                span["spoken"] = None
            corpus.write_text(
                "".join(json.dumps(r) + "\n" for r in [ordinary, candidate, unlabeled])
            )
            command = [
                sys.executable,
                str(ROOT / "benchmark/evaluators/evaluate.py"),
                "--corpus",
                str(corpus),
                "--ttf",
                str(CLI),
                "--resources",
                str(RESOURCES),
                "--limit",
                "0",
                "--chunk-bytes",
                "1",
                "7",
            ]
            for name in ("before", "after"):
                run = subprocess.run(
                    command + ["--output-dir", str(root / name)],
                    capture_output=True,
                    text=True,
                    timeout=45,
                    check=False,
                )
                self.assertEqual(run.returncode, 0, run.stderr)
            summary = json.loads((root / "before/summary.json").read_text())["summary"]
            self.assertEqual(summary["cases"], 3)
            self.assertEqual(summary["spans"]["dataset-reference"]["total"], 5)
            self.assertEqual(summary["spans"]["candidate"]["total"], 5)
            self.assertEqual(summary["unlabeled_spans"], 5)
            self.assertEqual(summary["sentence"]["dataset-reference"]["exact"], 0)
            self.assertEqual(
                summary["spoken_projection"]["dataset-reference"]["exact"], 1
            )
            self.assertEqual(summary["tn_only_reference_spans"]["exact"], 1)
            self.assertEqual(summary["tn_only_reference_spans"]["total"], 1)
            self.assertEqual(summary["chunk_comparisons"], 6)
            self.assertEqual(summary["chunk_failures"], 0)
            report = COMPARE.compare(root / "before", root / "after")
            self.assertEqual(
                report["changes"]["CARDINAL/dataset-reference"]["unchanged-correct"], 1
            )
            self.assertEqual(report["report_integrity"]["verified_cases"], 3)
            # Two equally truncated reports used to pass zip_longest while
            # still advertising the original, larger denominator.
            paths = [root / name / "results.jsonl" for name in ("before", "after")]
            contents = [path.read_text() for path in paths]
            for path, content in zip(paths, contents):
                path.write_text("\n".join(content.splitlines()[:-1]) + "\n")
            with self.assertRaisesRegex(ValueError, "result count"):
                COMPARE.compare(root / "before", root / "after")
            for path, content in zip(paths, contents):
                path.write_text(content)
            original = [json.loads(line) for line in contents[1].splitlines()]
            altered = deepcopy(original)
            altered[0]["spans"][2]["exact"] = False
            paths[1].write_text("".join(json.dumps(row) + "\n" for row in altered))
            with self.assertRaisesRegex(ValueError, "exact flag"):
                COMPARE.compare(root / "before", root / "after")
            altered = deepcopy(original)
            altered[0]["spans"][2]["semiotic_class"] = "PLAIN"
            paths[1].write_text("".join(json.dumps(row) + "\n" for row in altered))
            with self.assertRaisesRegex(ValueError, "reference span fields"):
                COMPARE.compare(root / "before", root / "after")
            paths[1].write_text(contents[1])
            summary_path = root / "after/summary.json"
            saved_summary = summary_path.read_text()
            altered_summary = json.loads(saved_summary)
            altered_summary["summary"]["chunk_failures"] = 999
            summary_path.write_text(json.dumps(altered_summary))
            with self.assertRaisesRegex(ValueError, "result metrics"):
                COMPARE.compare(root / "before", root / "after")
            summary_path.write_text(saved_summary)
            bad = json.loads((root / "after/summary.json").read_text())
            bad["provenance"]["corpus_sha256"] = "changed"
            (root / "after/summary.json").write_text(json.dumps(bad))
            with self.assertRaisesRegex(ValueError, "corpus_sha256"):
                COMPARE.compare(root / "before", root / "after")

    def test_speech_mode_is_a_separate_report(self):
        """A speech run must be recognisable as one, never mistaken for the text baseline."""
        with tempfile.TemporaryDirectory(prefix="ttf-speech-report-") as directory:
            corpus = Path(directory) / "corpus.jsonl"
            corpus.write_text(json.dumps(case(), ensure_ascii=False) + "\n", encoding="utf-8")
            report_dir = ensure_output(ROOT / "benchmark/generated/speech-mode-test")
            shutil.rmtree(report_dir, ignore_errors=True)
            subprocess.run(
                [sys.executable, str(ROOT / "benchmark/evaluators/evaluate.py"),
                 "--corpus", str(corpus), "--ttf", str(CLI), "--resources", str(RESOURCES),
                 "--output-dir", str(report_dir), "--output-mode", "speech"],
                check=True, capture_output=True, text=True)
            report = json.loads((report_dir / "summary.json").read_text("utf-8"))
            shutil.rmtree(report_dir, ignore_errors=True)
        self.assertEqual(report["provenance"]["frontend_options"], {"output_mode": "speech"})
        self.assertIn("speech", report["metric_policy"])
        self.assertNotEqual(report["metric_policy"], "v2-exact-context-complete-output")

    def test_mismatch_kind_separates_convention_from_error(self):
        """A diagnostic only: case and spacing never enter the accuracy itself."""
        kind = EVALUATE.mismatch_kind
        self.assertIsNone(kind("january third", "january third", None))
        self.assertEqual(kind("january third", "January third", None), "case-only")
        self.assertEqual(kind("january third", "january  third", None), "space-only")
        self.assertEqual(kind("january third", "January  third", None), "case-and-space")
        self.assertEqual(kind("january third", "january fourth", None), "different")
        self.assertEqual(kind("january third", None, None), "no-output")
        self.assertEqual(kind("january third", "January third", "unaligned"), "alignment")

    def test_summary_reports_case_insensitive_diagnostic_without_changing_accuracy(self):
        row = case()
        row["spans"][2]["spoken"] = "twenty one"
        summary = EVALUATE.Summary()
        scored = {
            "engine_error": None,
            "sentence_exact": None,
            "spoken_projection_exact": None,
            "spoken_projection_error": None,
            "streaming": [],
            "spans": [
                dict(
                    row["spans"][0],
                    actual="I",
                    alignment_error=None,
                    exact=True,
                    engine_semantics=["WORD"],
                    rules=["identity"],
                    lexicon_routes=[],
                    mismatch_kind=None,
                ),
                dict(
                    row["spans"][2],
                    actual="Twenty one",
                    alignment_error=None,
                    exact=False,
                    engine_semantics=["CARDINAL"],
                    rules=["cardinal"],
                    lexicon_routes=[],
                    mismatch_kind="case-only",
                ),
            ],
        }
        summary.add(scored)
        data = summary.data
        classes = data["semiotic_classes"]
        self.assertEqual(classes["CARDINAL"]["dataset-reference"]["exact"], 0)
        self.assertEqual(data["mismatch_kinds"]["CARDINAL"], {"case-only": 1})
        diagnostic = data["diagnostic_case_insensitive"]["CARDINAL"]
        self.assertEqual((diagnostic["exact"], diagnostic["total"]), (1, 1))
        self.assertEqual(data["diagnostic_case_insensitive"]["PLAIN"]["exact"], 1)

    def test_noncommercial_execution_requires_opt_in(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            corpus = root / "cases.jsonl"
            row = case()
            row.update(dataset="coreaspeech", usage="noncommercial-benchmark-only")
            corpus.write_text(json.dumps(row) + "\n")
            run = subprocess.run(
                [
                    sys.executable,
                    str(ROOT / "benchmark/evaluators/evaluate.py"),
                    "--corpus",
                    str(corpus),
                    "--ttf",
                    str(CLI),
                    "--resources",
                    str(RESOURCES),
                    "--output-dir",
                    str(root / "report"),
                ],
                capture_output=True,
                text=True,
                timeout=15,
                check=False,
            )
            self.assertEqual(run.returncode, 2)
            self.assertIn("--allow-noncommercial", run.stderr)
            self.assertFalse((root / "report/summary.json").exists())


if __name__ == "__main__":
    unittest.main()
