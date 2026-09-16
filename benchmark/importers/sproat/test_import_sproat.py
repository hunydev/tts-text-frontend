#!/usr/bin/env python3
"""Offline tests use original synthetic fixtures, not redistributed dataset text."""

import argparse
import gzip
import importlib.util
import io
import json
import tarfile
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent


def load(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


IMPORT = load("import_sproat")
FETCH = load("fetch_sample")


class SproatTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def args(self, text, **options):
        source = self.directory / "source.tsv"
        source.write_text(text, encoding="utf-8")
        values = {
            "input": source,
            "output": self.directory / "cases.jsonl",
            "source_file": "en_with_types/output-00058-of-00100",
            "source_url": IMPORT.DATASET_URL,
            "revision": "kaggle-version-1",
            "language": "en-US",
            "split": None,
            "limit": None,
            "partial_source": True,
            "drop_incomplete": False,
        }
        values.update(options)
        return argparse.Namespace(**values)

    def convert(self, text, **options):
        args = self.args(text, **options)
        report = IMPORT.convert(args)
        return (
            args,
            report,
            [json.loads(line) for line in args.output.read_text().splitlines()],
        )

    def test_sentence_mapping_and_offsets(self):
        _, report, cases = self.convert(
            "PLAIN\t꽃\t<self>\nCARDINAL\t21\ttwenty one\nPUNCT\t!\tsil\n<eos>\t<eos>\n"
        )
        self.assertEqual(report["sentences"], 1)
        self.assertEqual(cases[0]["text"], "꽃 21 !")
        self.assertEqual(cases[0]["expected"], "꽃 twenty one")
        self.assertEqual(
            [(s["begin"], s["end"]) for s in cases[0]["spans"]],
            [(0, 3), (4, 6), (7, 8)],
        )
        for span in cases[0]["spans"]:
            self.assertEqual(
                cases[0]["text"].encode()[span["begin"] : span["end"]].decode(),
                span["written"],
            )

    def test_plain_nonself_reference_is_not_overwritten(self):
        _, _, cases = self.convert("PLAIN\tcolour\tcolor\n<eos>\t<eos>\n")
        self.assertEqual(cases[0]["expected"], "color")
        self.assertEqual(cases[0]["spans"][0]["metadata"]["original_spoken"], "color")

    def test_multiword_token_and_unknown_class_preserved(self):
        _, _, cases = self.convert("FUTURE_CLASS\t7 July\tseventh july\n<eos>\t<eos>\n")
        self.assertEqual(len(cases[0]["spans"]), 1)
        self.assertEqual(cases[0]["spans"][0]["end"], 6)
        self.assertEqual(cases[0]["spans"][0]["semiotic_class"], "FUTURE_CLASS")

    def test_self_mapping_is_marker_based(self):
        _, _, cases = self.convert("LETTERS\tZZ\t<self>\n<eos>\t<eos>\n")
        self.assertEqual(cases[0]["expected"], "ZZ")

    def test_sil_and_empty_output(self):
        _, _, cases = self.convert("PUNCT\t.\tsil\n<eos>\t<eos>\n")
        self.assertEqual(cases[0]["expected"], "")
        self.assertEqual(cases[0]["spans"][0]["spoken"], "")

    def test_split_inference(self):
        for shard, split in [
            (0, "train"),
            (89, "train"),
            (90, "dev"),
            (94, "dev"),
            (95, "test"),
            (99, "test"),
        ]:
            self.assertEqual(IMPORT.infer_split(f"output-{shard:05}-of-00100"), split)
        for name in ["training", "output-00100-of-00100", "output-00000-of-00050"]:
            with self.assertRaises(ValueError):
                IMPORT.infer_split(name)

    def test_hash_and_prefix_selection(self):
        args, report, cases = self.convert(
            "PLAIN\ta\t<self>\n<eos>\t<eos>\nPLAIN\tb\t<self>\n<eos>\t<eos>\n", limit=1
        )
        self.assertEqual(len(cases), 1)
        self.assertEqual(report["last_source_line"], 2)
        self.assertEqual(
            cases[0]["provenance"]["source_sha256"], IMPORT.digest(args.input)
        )
        self.assertEqual(cases[0]["metadata"]["selection"], "input-prefix")
        self.assertEqual(cases[0]["provenance"]["source_artifact_scope"], "prefix")

    def test_incomplete_requires_explicit_option(self):
        text = "PLAIN\ta\t<self>\n<eos>\t<eos>\nPLAIN\tb\t<self>\n"
        with self.assertRaisesRegex(ValueError, "incomplete"):
            IMPORT.convert(self.args(text))
        _, report, cases = self.convert(text, drop_incomplete=True)
        self.assertEqual(report["sentences"], 1)
        self.assertEqual(cases[0]["text"], "a")

    def test_bad_input_keeps_existing_output(self):
        args = self.args("PLAIN\ta\t<self>\n<eos>\t<eos>\nbroken\n")
        args.output.write_text("existing\n")
        with self.assertRaises(ValueError):
            IMPORT.convert(args)
        self.assertEqual(args.output.read_text(), "existing\n")

    def test_invalid_formats(self):
        for text in [
            "\n",
            "<eos>\t<eos>\n",
            "PLAIN\t\t<self>\n",
            "plain\tx\tx\n",
            "PLAIN\tx\tx\textra\n",
        ]:
            with self.subTest(text=text), self.assertRaises(ValueError):
                IMPORT.convert(self.args(text))

    def test_input_output_alias_rejected(self):
        args = self.args("PLAIN\tx\tx\n<eos>\t<eos>\n")
        args.output = args.input
        with self.assertRaisesRegex(ValueError, "must differ"):
            IMPORT.convert(args)

    def test_limit_rejected(self):
        for limit in (0, -1):
            with self.assertRaises(ValueError):
                IMPORT.convert(self.args("PLAIN\tx\tx\n<eos>\t<eos>\n", limit=limit))

    def test_line_bound(self):
        args = self.args("PLAIN\t" + "x" * IMPORT.MAX_LINE_BYTES + "\t<self>\n")
        with self.assertRaisesRegex(ValueError, "line byte limit"):
            IMPORT.convert(args)

    def test_sentence_token_bound(self):
        args = self.args("PLAIN\tx\t<self>\n" * (IMPORT.MAX_SENTENCE_TOKENS + 1))
        with self.assertRaisesRegex(ValueError, "bounded-memory"):
            IMPORT.convert(args)

    def test_crlf(self):
        _args, _, cases = self.convert("PLAIN\tx\t<self>\r\n<eos>\t<eos>\r\n")
        self.assertEqual(cases[0]["text"], "x")

    def test_utf8_errors(self):
        args = self.args("")
        args.input.write_bytes(b"PLAIN\t\xff\t<self>\n")
        with self.assertRaises(UnicodeError):
            IMPORT.convert(args)

    def test_ids_stable_when_only_limit_changes(self):
        text = "PLAIN\tx\t<self>\n<eos>\t<eos>\nPLAIN\ty\t<self>\n<eos>\t<eos>\n"
        _, _, first = self.convert(text, limit=1)
        _, _, all_cases = self.convert(text)
        self.assertEqual(first[0]["id"], all_cases[0]["id"])

    def test_zip_tar_prefix_uses_actual_member_and_drops_partial_sentence(self):
        sample = b"PLAIN\tx\t<self>\n<eos>\t<eos>\nPLAIN\ty\t<self>\n"
        tar_bytes = io.BytesIO()
        with tarfile.open(fileobj=tar_bytes, mode="w") as archive:
            member = tarfile.TarInfo("en_with_types/output-00058-of-00100")
            member.size = len(sample)
            archive.addfile(member, io.BytesIO(sample))
        zip_bytes = io.BytesIO()
        with zipfile.ZipFile(
            zip_bytes, "w", compression=zipfile.ZIP_DEFLATED
        ) as archive:
            archive.writestr("en_with_types.tgz", gzip.compress(tar_bytes.getvalue()))
        name, result, lines = FETCH.extract_prefix(zip_bytes.getvalue(), 50000)
        self.assertEqual(name, "en_with_types/output-00058-of-00100")
        self.assertEqual(result, b"PLAIN\tx\t<self>\n<eos>\t<eos>\n")
        self.assertEqual(lines, 2)

    def test_fetch_rejects_unbounded_options_before_network(self):
        for size, lines in [(0, 5), (1048577, 5), (100000, 0)]:
            with self.assertRaises(ValueError):
                FETCH.fetch(self.directory, size, lines)

    def test_fetch_rejects_unexpected_container(self):
        for blob in [b"", b"notzip" * 20]:
            with self.assertRaises(ValueError):
                FETCH.extract_prefix(blob, 20)


if __name__ == "__main__":
    unittest.main()
