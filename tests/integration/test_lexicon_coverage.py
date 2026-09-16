"""Exercise the coverage CLI's denominator, routes and local-file contract."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "tools/lexicon/coverage.py"


def main():
    cli, resources = (str(Path(arg).resolve()) for arg in sys.argv[1:])
    checks = 0

    def run(path, *options):
        return subprocess.run(
            [
                sys.executable,
                str(TOOL),
                str(path),
                "--ttf",
                cli,
                "--resources",
                resources,
                *map(str, options),
            ],
            capture_output=True,
            timeout=30,
        )

    def report(path, *options):
        result = run(path, *options)
        assert result.returncode == 0, result.stderr.decode()
        return json.loads(result.stdout)

    with tempfile.TemporaryDirectory(prefix="ttf-coverage-test-") as directory:
        directory = Path(directory)
        manuscript = directory / "manuscript.txt"
        # Independent contract: exactly four foreign lexical occurrences are
        # rendered in Korean. English, protected code and numbers add none.
        manuscript.write_text(
            "Apple의 안내입니다.\n"
            "foobarquux의 안내입니다.\n"
            "foobarquux의 설명입니다.\n"
            "ETF의 비용입니다.\n"
            "Apple released the iPhone.\n"
            "Please compare the ETF fees.\n"
            "`Apple foobarquux ETF 123`\n"
            "숫자는 123입니다. 금액은 12,500원입니다.\n",
            encoding="utf-8",
        )
        basic = report(manuscript)
        finance = report(manuscript, "--finance")
        for data in (basic, finance):
            assert data["eligible_foreign_spans"] == 4, data
            assert sum(data["routes"].values()) == 4, data
            assert sum(data["review_status"].values()) == 4, data
            terms = {row["canonical_key"]: row for row in data["terms"]}
            assert set(terms) == {"apple", "foobarquux", "etf"}, terms
            assert {key: row["count"] for key, row in terms.items()} == {
                "apple": 1,
                "foobarquux": 2,
                "etf": 1,
            }, terms
            assert all(
                row["source_lang"] == "en-US" and row["render_lang"] == "ko-KR"
                for row in terms.values()
            ), terms
            assert terms["apple"]["route"] == "direct-lexicon", terms
            assert terms["apple"]["trace"] == "lexicon:system", terms
            assert terms["apple"]["entry_id"], terms
            unknown = terms["foobarquux"]
            assert unknown["route"] == "fallback", unknown
            assert unknown["review_status"] == "unreviewed-rendering", unknown
            assert unknown["entry_id"] is None, unknown
            assert unknown["rendered_examples"] and all(unknown["rendered_examples"]), (
                unknown
            )
            # A fallback is a review candidate, never an automatically approved
            # dictionary pronunciation. Do not assert its provisional spelling.
            candidates = {row["canonical_key"] for row in data["review_candidates"]}
            assert "foobarquux" in candidates and "apple" not in candidates, data
            checks += 14

        base_terms = {row["canonical_key"]: row for row in basic["terms"]}
        finance_terms = {row["canonical_key"]: row for row in finance["terms"]}
        assert base_terms["etf"]["route"] == "fallback", base_terms
        assert base_terms["etf"]["entry_id"] is None, base_terms
        assert finance_terms["etf"]["route"] == "direct-lexicon", finance_terms
        assert finance_terms["etf"]["trace"] == "lexicon:domain", finance_terms
        assert finance_terms["etf"]["entry_id"] == "finance-etf", finance_terms
        assert finance_terms["etf"]["review_status"] == "pending", finance_terms
        assert basic["routes"] == {"direct-lexicon": 1, "fallback": 3}, basic
        assert finance["routes"] == {"direct-lexicon": 2, "fallback": 2}, finance
        assert basic["direct_lexicon_rate"] == 1 / 4, basic
        assert finance["direct_lexicon_rate"] == 2 / 4, finance
        assert {row["canonical_key"] for row in basic["review_candidates"]} == {
            "foobarquux",
            "etf",
        }, basic
        assert {row["canonical_key"] for row in finance["review_candidates"]} == {
            "foobarquux",
        }, finance
        checks += 12

        output = directory / "report.json"
        result = run(manuscript, "--finance", "--output", output)
        assert result.returncode == 0 and result.stdout == b"", result
        assert json.loads(output.read_text(encoding="utf-8")) == finance
        checks += 2

        empty = directory / "empty.txt"
        empty.write_text("", encoding="utf-8")
        data = report(empty)
        assert data["eligible_foreign_spans"] == 0, data
        assert data["direct_lexicon_rate"] is None, data
        assert data["terms"] == [] and data["review_candidates"] == [], data
        checks += 3

        invalid = directory / "invalid-utf8.txt"
        invalid.write_bytes(b"\xff\xfe")
        for path in (directory / "missing.txt", invalid):
            failed_output = directory / "failed-report.json"
            result = run(path, "--output", failed_output)
            assert result.returncode != 0, result
            assert result.stderr and result.stdout == b"", result
            assert not failed_output.exists(), failed_output
            checks += 3

    print(f"lexicon coverage CLI: {checks} checks passed")


if __name__ == "__main__":
    main()
