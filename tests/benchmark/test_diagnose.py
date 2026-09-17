"""Original diagnostic fixtures: case differences never become exact matches."""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "diagnose", ROOT / "benchmark/evaluators/diagnose.py"
)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class DiagnosticTest(unittest.TestCase):
    def test_reasons_and_statuses(self):
        def span(actual, spoken, status="dataset-reference", error=None):
            return dict(
                actual=actual, spoken=spoken, reference_status=status,
                semiotic_class="DATE", alignment_error=error,
                engine_semantics=["DATE"],
            )

        records = [
            dict(spans=[span("March first", "March first"),
                        span("March first", "march first"),
                        span("March second", "march first"),
                        span(None, "March first", error="crossed-boundary"),
                        span("March first", "March first", "candidate"),
                        span("March first", None, "unlabeled")]),
            dict(engine_error="backend failed", spans=[span(None, "March first")]),
        ]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            summary = dict(status="completed", summary=dict(cases=2), metric_policy="fixture")
            (path / "summary.json").write_text(json.dumps(summary))
            (path / "results.jsonl").write_text("\n".join(map(json.dumps, records)) + "\n")
            report = module.diagnose(path)
            self.assertEqual(report["reasons"]["DATE/dataset-reference"], {
                "exact": 1, "casefold-only-difference": 1, "other-text-difference": 1,
                "unaligned": 1, "engine-error": 1,
            })
            self.assertEqual(report["reasons"]["DATE/candidate"], {"exact": 1})
            self.assertEqual(report["reasons"]["DATE/unlabeled"], {"unlabeled": 1})
            self.assertNotIn("March", json.dumps(report))
            summary["summary"]["cases"] = 3
            (path / "summary.json").write_text(json.dumps(summary))
            with self.assertRaises(ValueError):
                module.diagnose(path)
            summary["status"] = "running"
            (path / "summary.json").write_text(json.dumps(summary))
            with self.assertRaises(ValueError):
                module.diagnose(path)


if __name__ == "__main__":
    unittest.main()
