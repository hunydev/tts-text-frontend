"""Source compiler integrity gates; these do not approve pronunciation quality."""

from copy import deepcopy
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "ttf_lexicon_manage", ROOT / "tools/lexicon/manage.py"
)
MANAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MANAGE)


class LexiconSourceTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Snapshot checked-in sources and resource layout before any mutation.
        cls.snapshot_dir = tempfile.TemporaryDirectory(prefix="ttf-lexicon-snapshot-")
        cls.snapshot = Path(cls.snapshot_dir.name)
        for name in ("lexicons", "resources"):
            shutil.copytree(ROOT / name, cls.snapshot / name)
        MANAGE.build(cls.snapshot)
        cls.rows, cls.sources = MANAGE.load(cls.snapshot)

    @classmethod
    def tearDownClass(cls):
        cls.snapshot_dir.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="ttf-lexicon-source-test-")
        self.root = Path(self.temp.name)
        for name in ("lexicons", "resources"):
            shutil.copytree(self.snapshot / name, self.root / name)

    def tearDown(self):
        self.temp.cleanup()

    def mutate(self, change):
        path = self.root / self.rows[0]["source_file"]
        rows = [
            json.loads(line)
            for line in path.read_text(encoding="utf-8").splitlines()
            if line.strip()
        ]
        change(rows[0], rows)
        path.write_text(
            "".join(json.dumps(row, ensure_ascii=False) + "\n" for row in rows),
            encoding="utf-8",
        )

    def rejected(self, change, message):
        self.mutate(change)
        with self.assertRaisesRegex(ValueError, message):
            MANAGE.load(self.root)

    def test_inventory_accounts_for_every_key(self):
        report = MANAGE.inventory(self.root)
        self.assertEqual(report["summary"]["entries"], len(self.rows))
        self.assertEqual(
            report["summary"]["runtime_keys"],
            sum(len(row["canonical_keys"]) for row in self.rows),
        )
        self.assertEqual(report["summary"]["source_count"], len(self.sources))
        self.assertEqual(
            sum(report["summary"]["review_status"].values()), len(self.rows)
        )
        for row in report["entries"]:
            self.assertEqual(
                [source["id"] for source in row["sources"]], row["evidence"]
            )

    def test_canonicalization_matches_runtime_profile(self):
        self.assertEqual(MANAGE.canonical("After   Effects"), "after effects")
        self.assertEqual(MANAGE.canonical("USB-C"), "usb-c")
        self.assertEqual(MANAGE.canonical("C++"), "c++")
        self.assertEqual(MANAGE.canonical("MP3"), "mp3")
        # v0.24 Latin joiners (docs/latin-joiners.ko.md): - ' & join letter runs
        # inside a phrase word and & stands alone between words. "alpha-beta gamma"
        # was rejected before; a dangling joiner is still outside the grammar.
        self.assertEqual(MANAGE.canonical("Alpha-Beta Gamma"), "alpha-beta gamma")
        self.assertEqual(MANAGE.canonical("Mom's Pizza"), "mom's pizza")
        self.assertEqual(MANAGE.canonical("Procter & Gamble"), "procter & gamble")
        self.assertEqual(MANAGE.canonical("Pull&Bear"), "pull&bear")
        for term in (
            " alpha",
            "alpha ",
            "alpha\tbeta",
            "alpha\nbeta",
            "alpha\x00",
            "alpha\x7f",
            "123",
            "alpha 2",
            "alpha- gamma",
            "& alpha beta",
            "alpha beta &",
            "a b c d e f g h i",
            "a" * 96,
            "a1b",
            "foo++",
        ):
            with self.subTest(term=repr(term)), self.assertRaises(ValueError):
                MANAGE.canonical(term)

    def test_duplicate_record_id_rejected(self):
        def change(row, rows):
            other = deepcopy(row)
            other["term"] = "uniqueauditword"
            other["aliases"] = []
            rows.append(other)

        self.rejected(change, "duplicate record id")

    def test_duplicate_casefolded_key_rejected(self):
        def change(row, rows):
            other = deepcopy(row)
            other["id"] = "audit-duplicate-key"
            other["term"] = row["term"].swapcase()
            other["aliases"] = []
            rows.append(other)

        self.rejected(change, "canonical key collision")

    def test_alias_cannot_duplicate_canonical_case(self):
        self.rejected(
            lambda row, _: row.update(aliases=[row["term"].swapcase()]),
            "canonical key collision",
        )

    def test_alias_cannot_duplicate_whitespace_fold(self):
        self.rejected(
            lambda row, _: row.update(term="unique audit", aliases=["UNIQUE  AUDIT"]),
            "canonical key collision",
        )

    def test_unknown_source_rejected(self):
        self.rejected(
            lambda row, _: row.update(evidence=["no-such-source"]),
            "unknown or duplicate evidence",
        )

    def test_duplicate_evidence_rejected(self):
        self.rejected(
            lambda row, _: row.update(evidence=[row["evidence"][0]] * 2),
            "unknown or duplicate evidence",
        )

    def test_unsupported_phrase_digits_rejected(self):
        self.rejected(
            lambda row, _: row.update(term="audit 2", aliases=[]),
            "multiword keys require",
        )

    def test_unsafe_controls_rejected(self):
        self.rejected(
            lambda row, _: row.update(term="audit\nterm", aliases=[]), "controls"
        )

    def test_pronunciation_cannot_inject_tsv(self):
        self.rejected(
            lambda row, _: row.update(pronunciation="발음\t주입"), "Hangul syllables"
        )

    def test_pronunciation_cannot_be_translation_code(self):
        self.rejected(
            lambda row, _: row.update(pronunciation="<script>"), "Hangul syllables"
        )

    def test_human_review_requires_reviewer(self):
        self.rejected(
            lambda row, _: row.update(
                review_status="human-reviewed", reviewed_at="2026-09-16"
            ),
            "named reviewer",
        )

    def test_human_review_requires_date(self):
        self.rejected(
            lambda row, _: row.update(
                review_status="human-reviewed", reviewer="Synthetic fixture reviewer"
            ),
            "Invalid isoformat",
        )

    def test_human_review_record_is_structurally_valid(self):
        # A fabricated fixture verifies fields only; it does not attest a real reviewer.
        self.mutate(
            lambda row, _: row.update(
                review_status="human-reviewed",
                reviewer="Synthetic fixture reviewer",
                reviewed_at="2026-09-16",
            )
        )
        rows, _ = MANAGE.load(self.root)
        self.assertTrue(any(row["review_status"] == "human-reviewed" for row in rows))

    def test_editorial_policy_cannot_self_approve_source_check(self):
        editorial = next(
            key
            for key, source in self.sources.items()
            if source["kind"] == "editorial-policy"
        )
        self.rejected(
            lambda row, _: row.update(
                review_status="source-checked", evidence=[editorial]
            ),
            "requires a primary",
        )

    def test_unknown_dictionary_scope_rejected(self):
        self.rejected(
            lambda row, _: row.update(scope="unregistered-domain"),
            "unknown dictionary scope",
        )

    def test_explicit_scope_override_is_distinct_from_collision(self):
        def change(row, rows):
            row.update(term="uniqueaudit", aliases=[], scope="system")
            other = deepcopy(row)
            other.update(
                id="audit-scope-override", scope="finance", pronunciation="별도발음"
            )
            rows.append(other)

        self.mutate(change)
        rows, _ = MANAGE.load(self.root)
        matching = [row for row in rows if row["canonical_keys"] == ["uniqueaudit"]]
        self.assertEqual(
            {row.get("scope", "system") for row in matching}, {"system", "finance"}
        )
        self.assertEqual(len(matching), 2)
        paths = {MANAGE.runtime_path(row) for row in matching}
        self.assertEqual(len(paths), 2)
        self.assertIn("resources/domains/finance/ko-KR/lexicon.tsv", paths)

    def test_category_cannot_escape_output_directory(self):
        self.rejected(
            lambda row, _: row.update(category="../outside"), "unsafe category"
        )

    def test_build_is_deterministic_and_check_detects_drift(self):
        first = MANAGE.build(self.root)
        outputs = MANAGE.rendered(MANAGE.load(self.root)[0])
        expected = {name: (self.root / name).read_bytes() for name in outputs}
        self.assertEqual(first, MANAGE.build(self.root))
        self.assertEqual(
            expected, {name: (self.root / name).read_bytes() for name in outputs}
        )
        MANAGE.build(self.root, check=True)
        path = self.root / next(iter(outputs))
        path.write_bytes(path.read_bytes() + b"# unwanted runtime edit\n")
        with self.assertRaisesRegex(ValueError, "generated resources differ"):
            MANAGE.build(self.root, check=True)
        run = subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/lexicon/manage.py"),
                "build",
                "--root",
                str(self.root),
                "--check",
            ],
            capture_output=True,
            text=True,
            timeout=15,
        )
        self.assertNotEqual(run.returncode, 0)
        self.assertIn("generated resources differ", run.stderr)
        self.assertTrue(
            path.read_bytes().endswith(b"# unwanted runtime edit\n"),
            "--check must not mutate runtime data",
        )
        MANAGE.build(self.root)
        self.assertEqual(
            expected, {name: (self.root / name).read_bytes() for name in outputs}
        )

    def test_unmanaged_runtime_file_rejected(self):
        path = self.root / "resources/languages/ko-KR/lexicon/unmanaged.tsv"
        path.write_text("lexicon\tko-KR\ten-US:audit\t감사\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "unmanaged runtime lexicons"):
            MANAGE.build(self.root, check=True)

    def test_missing_manifest_registration_rejected(self):
        path = self.root / "resources/manifest.ttfm"
        generated = next(iter(MANAGE.rendered(MANAGE.load(self.root)[0]))).removeprefix(
            "resources/"
        )
        path.write_text(
            "\n".join(
                line for line in path.read_text().splitlines() if line != generated
            )
            + "\n"
        )
        with self.assertRaisesRegex(ValueError, "missing from manifest"):
            MANAGE.build(self.root, check=True)

    def test_scoped_dictionary_cannot_enter_system_manifest(self):
        path = self.root / "resources/manifest.ttfm"
        path.write_text(path.read_text() + "domains/finance/ko-KR/lexicon.tsv\n")
        with self.assertRaisesRegex(ValueError, "scoped dictionary must not appear"):
            MANAGE.build(self.root, check=True)

    def test_validate_cli_reports_source_inventory(self):
        run = subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/lexicon/manage.py"),
                "validate",
                "--root",
                str(self.root),
            ],
            capture_output=True,
            text=True,
            timeout=15,
        )
        self.assertEqual(run.returncode, 0, run.stderr)
        self.assertEqual(json.loads(run.stdout), MANAGE.inventory(self.root)["summary"])


if __name__ == "__main__":
    unittest.main()
