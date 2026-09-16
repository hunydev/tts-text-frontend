"""Synthetic format/guard tests. No licensed corpus text and no corpus accuracy claims."""

import hashlib
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


KSP = load("kspon_importer", "benchmark/importers/ksponspeech/import_ksponspeech.py")
COREA = load("corea_importer", "benchmark/importers/coreaspeech/import_coreaspeech.py")


class LocalImporterTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ttf-korean-import-")
        self.path = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def source(self, text, encoding="cp949", name="source.txt"):
        path = self.path / name
        path.write_bytes(text.encode(encoding))
        return path

    def export(self, rows):
        path = self.path / "export.jsonl"
        path.write_text(
            "".join(json.dumps(row, ensure_ascii=False) + "\n" for row in rows),
            encoding="utf-8",
        )
        return path

    def run_cli(self, module, *args):
        return subprocess.run(
            [sys.executable, module.__file__, *map(str, args)],
            capture_output=True,
            text=True,
            check=False,
        )

    def kspon(self, text, encoding="cp949"):
        return KSP.import_file(
            self.source(text, encoding), split="synthetic", encoding=encoding
        )

    def test_kspon_numbers_are_candidates_never_sentence_gold(self):
        row = self.kspon(
            "합계 (17,200원)/(만 칠천 이백 원)입니다. (그러니)/(그라니) 말입니다."
        )
        self.assertEqual(row["text"], "합계 17,200원입니다. 그러니 말입니다.")
        self.assertIsNone(row["expected"])
        self.assertEqual(row["reference_status"], "candidate")
        self.assertEqual(len(row["spans"]), 1)
        self.assertEqual(row["spans"][0]["semiotic_class"], "CURRENCY")
        self.assertEqual(row["spans"][0]["spoken"], "만 칠천 이백 원")
        self.assertEqual(len(row["metadata"]["excluded_dual_annotations"]), 1)

    def test_kspon_raw_cp949_and_output_utf8_offsets_are_distinct(self):
        source = "앞 (24%)/(이십 사 퍼센트), 뒤 (8개)/(여덟 개).\r\n"
        path = self.source(source)
        row = KSP.import_file(path, split="synthetic")
        self.assertEqual(
            row["provenance"]["source_sha256"],
            hashlib.sha256(path.read_bytes()).hexdigest(),
        )
        for span in row["spans"]:
            output_part = (
                row["text"].encode("utf-8")[span["begin"] : span["end"]].decode("utf-8")
            )
            original_part = path.read_bytes()[
                span["raw_begin_byte"] : span["raw_end_byte"]
            ].decode("cp949")
            self.assertEqual(output_part, span["written"])
            self.assertEqual(original_part, f"({span['written']})/({span['spoken']})")
        self.assertTrue(row["text"].endswith("\r\n"))

    def test_kspon_utf8_bom_offsets(self):
        source = "앞 (9)/(구)"
        path = self.source(source, "utf-8-sig")
        row = KSP.import_file(path, split="synthetic", encoding="utf-8-sig")
        span = row["spans"][0]
        self.assertEqual(
            path.read_bytes()[span["raw_begin_byte"] : span["raw_end_byte"]],
            b"(9)/(" + "구".encode() + b")",
        )
        self.assertEqual(row["text"], "앞 9")

    def test_kspon_excludes_disfluency_and_ordinary_pronunciation(self):
        row = self.kspon("(거기)/(저기) (7+ 7)/(칠 칠) (8)/(팔*) (정말)/(정말)")
        self.assertEqual(row["spans"], [])
        self.assertEqual(row["reference_status"], "unlabeled")
        self.assertEqual(len(row["metadata"]["excluded_dual_annotations"]), 4)

    def test_kspon_selects_classes_without_turning_them_into_approved_gold(self):
        examples = {
            "031-456-7890": "PHONE",
            "2027-08-19": "DATE",
            "11:45": "TIME",
            "2~7": "RANGE",
            "3/8": "FRACTION",
            "7th": "ORDINAL",
            "8개": "COUNTER",
            "16kg": "MEASURE",
            "Q8": "ALNUM_TERM",
            "37": "NUMBER",
            "USB": "INITIALISM",
            "Mixer": "WORD",
            "&": "SYMBOL",
        }
        for written, expected in examples.items():
            with self.subTest(written=written):
                klass, _ = KSP.select_class(written, "합성 참조")
                self.assertEqual(klass, expected)

    def test_kspon_literal_parentheses_preserved(self):
        self.assertEqual(KSP.reconstruct("설명 (참고) (6)/(육)")[0], "설명 (참고) 6")

    def test_kspon_malformed_annotations_fail_without_guessing(self):
        for text in ("(5)/(오", "(안 (5))/(오)", "(5)/((오))", "()/(공)", "앞 )"):
            with self.subTest(text=text), self.assertRaises(ValueError):
                KSP.reconstruct(text)

    def test_kspon_rejects_invalid_encoding_and_nul(self):
        path = self.path / "bad.txt"
        path.write_bytes(b"\xff")
        with self.assertRaises(UnicodeError):
            KSP.import_file(path, split="synthetic")
        with self.assertRaisesRegex(ValueError, "NUL"):
            self.kspon("(5)/(오)\0")

    def test_kspon_stable_ids_depend_on_original_bytes(self):
        first = self.kspon("(4)/(사)")
        self.assertEqual(first["id"], self.kspon("(4)/(사)")["id"])
        self.assertNotEqual(first["id"], self.kspon("(4)/(네)")["id"])

    def test_kspon_requires_rights_before_reading_source(self):
        result = self.run_cli(
            KSP,
            self.path / "missing",
            "--output",
            self.path / "out",
            "--split",
            "synthetic",
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("--acknowledge-data-rights", result.stderr)
        self.assertFalse((self.path / "out").exists())

    def test_kspon_cli_filters_non_tn_files_and_reports_counts(self):
        self.source("(25%)/(이십오 퍼센트)")
        self.source("(그렇다)/(그렇따)", name="other.txt")
        output = self.path / "out.jsonl"
        result = self.run_cli(
            KSP,
            self.path,
            "--output",
            output,
            "--split",
            "synthetic",
            "--acknowledge-data-rights",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        rows = [json.loads(line) for line in output.read_text().splitlines()]
        self.assertEqual(len(rows), 1)
        self.assertTrue(rows[0]["provenance"]["rights_acknowledged_by_invoker"])
        self.assertEqual(json.loads(result.stderr)["counts"]["excluded_dual_spans"], 1)

    def test_corea_two_fields_are_separate_unlabeled_utterances(self):
        path = self.export(
            [
                {
                    "text1": "먼저 7개를 준비합니다.",
                    "text2": "이제 19%를 확인합니다.",
                    "N2gkPlus": "자동 생성 값",
                }
            ]
        )
        rows = list(COREA.import_rows(path, split="numeric"))
        self.assertEqual(len(rows), 2)
        self.assertNotEqual(rows[0]["id"], rows[1]["id"])
        for row in rows:
            self.assertIsNone(row["expected"])
            self.assertEqual(row["reference_status"], "unlabeled")
            self.assertEqual(row["usage"], "noncommercial-benchmark-only")
            self.assertTrue(row["provenance"]["license_conflict"])
            self.assertTrue(row["metadata"]["generated_normalization_ignored"])
            self.assertTrue(all(span["spoken"] is None for span in row["spans"]))

    def test_corea_training_generated_field_is_not_gold(self):
        path = self.export([{"text": "17kg입니다.", "N2gkPlus": "틀린 참조"}])
        row = next(COREA.import_rows(path, split="train", dataset_part="training"))
        self.assertIsNone(row["expected"])
        self.assertEqual(row["text"], "17kg입니다.")
        self.assertEqual(
            row["provenance"]["revision"], COREA.SOURCES["training"]["revision"]
        )

    def test_corea_does_not_silently_accept_prompt_target_schema(self):
        path = self.export([{"prompt_text": "가", "target_text": "나"}])
        with self.assertRaisesRegex(ValueError, "text1"):
            list(COREA.import_rows(path, split="numeric"))

    def test_corea_span_offsets_preserve_korean_context(self):
        text = "길이는 3.25cm이며 GPU를 사용합니다."
        for span in COREA.discover_spans(text):
            self.assertEqual(
                text.encode()[span["begin"] : span["end"]].decode(), span["written"]
            )
            self.assertEqual(span["reference_status"], "unlabeled")

    def test_corea_both_explicit_gates_required(self):
        base = [
            self.path / "missing",
            "--output",
            self.path / "out",
            "--split",
            "numeric",
        ]
        first = self.run_cli(COREA, *base)
        self.assertIn("--allow-noncommercial", first.stderr)
        second = self.run_cli(COREA, *base, "--allow-noncommercial")
        self.assertIn("--acknowledge-data-rights", second.stderr)
        self.assertFalse((self.path / "out").exists())

    def test_corea_provided_reference_needs_exact_source_and_is_not_human_approval(
        self,
    ):
        path = self.export([{"text1": "7개", "text2": "8개"}])
        original = list(COREA.import_rows(path, split="numeric"))
        reference = {"id": original[0]["id"], "text": "7개", "expected": "일곱 개"}
        rows = list(
            COREA.import_rows(
                path, split="numeric", references={reference["id"]: reference}
            )
        )
        self.assertEqual(rows[0]["expected"], "일곱 개")
        self.assertEqual(rows[0]["reference_status"], "provided-reference")
        self.assertIn("no human approval", rows[0]["metadata"]["reference_warning"])
        self.assertEqual(rows[1]["reference_status"], "unlabeled")
        reference["text"] = "다른 문장"
        with self.assertRaisesRegex(ValueError, "does not match"):
            list(
                COREA.import_rows(
                    path, split="numeric", references={reference["id"]: reference}
                )
            )

    def test_corea_provided_span_offsets_validated(self):
        path = self.export([{"text1": "앞 7개", "text2": "8개"}])
        original = next(COREA.import_rows(path, split="numeric"))
        span = {
            "begin": 4,
            "end": 5,
            "written": "7",
            "spoken": "일곱",
            "semiotic_class": "NUMBER",
        }
        reference = {"id": original["id"], "text": original["text"], "spans": [span]}
        rows = list(
            COREA.import_rows(
                path, split="numeric", references={reference["id"]: reference}
            )
        )
        self.assertIsNone(rows[0]["expected"])
        self.assertEqual(rows[0]["spans"][0]["reference_status"], "provided-reference")
        span["begin"] = 0
        with self.assertRaises(ValueError):
            list(
                COREA.import_rows(
                    path, split="numeric", references={reference["id"]: reference}
                )
            )

    def test_corea_unknown_reference_ids_fail(self):
        path = self.export([{"text1": "가", "text2": "나"}])
        with self.assertRaisesRegex(ValueError, "not found"):
            list(
                COREA.import_rows(
                    path,
                    split="numeric",
                    references={"unknown": {"text": "가", "expected": "가"}},
                )
            )

    def test_corea_reference_file_validation(self):
        path = self.path / "references.jsonl"
        for rows in (
            [{"id": "x", "text": "가"}],
            [{"id": "x", "text": "가", "spans": []}],
            [{"id": "x", "text": "가", "expected": 1}],
            [{"id": "x", "text": "가", "expected": "나"}] * 2,
        ):
            path.write_text("".join(json.dumps(row) + "\n" for row in rows))
            with self.assertRaises(ValueError):
                COREA.load_references(path)

    def test_corea_successful_cli_and_local_reference_hash(self):
        path = self.export([{"text1": "7개", "text2": "8개"}])
        original = next(COREA.import_rows(path, split="numeric"))
        refs = self.path / "references.jsonl"
        refs.write_text(
            json.dumps(
                {"id": original["id"], "text": original["text"], "expected": "일곱 개"}
            ),
            encoding="utf-8",
        )
        output = self.path / "out.jsonl"
        result = self.run_cli(
            COREA,
            path,
            "--output",
            output,
            "--split",
            "numeric",
            "--references",
            refs,
            "--allow-noncommercial",
            "--acknowledge-data-rights",
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        row = json.loads(output.read_text().splitlines()[0])
        self.assertEqual(
            row["provenance"]["provided_reference"]["source_sha256"],
            hashlib.sha256(refs.read_bytes()).hexdigest(),
        )

    def test_failure_does_not_replace_output_with_partial_corpus(self):
        path = self.export(
            [{"text1": "7개", "text2": "8개"}, {"text1": "missing text2"}]
        )
        output = self.path / "out.jsonl"
        output.write_text("keep previous result")
        result = self.run_cli(
            COREA,
            path,
            "--output",
            output,
            "--split",
            "numeric",
            "--allow-noncommercial",
            "--acknowledge-data-rights",
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(output.read_text(), "keep previous result")

    def test_product_paths_and_input_overwrite_are_blocked(self):
        source = self.source("(7)/(칠)")
        for module in (KSP, COREA):
            with self.subTest(module=module.__name__):
                with self.assertRaisesRegex(ValueError, "replace an input"):
                    module.safe_output(source, [source])
                for name in (
                    "resources/test.jsonl",
                    "lexicons/test.jsonl",
                    "tests/evaluation/test.jsonl",
                ):
                    with self.assertRaisesRegex(ValueError, "benchmark/generated"):
                        module.safe_output(ROOT / name, [source])
                self.assertEqual(
                    module.safe_output(
                        ROOT / "benchmark/generated/test.jsonl", [source]
                    ),
                    ROOT / "benchmark/generated/test.jsonl",
                )


if __name__ == "__main__":
    unittest.main()
