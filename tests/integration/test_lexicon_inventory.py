"""Mechanical all-key reachability, layer and stream contracts.

Expected readings are taken from the authored inventory to check compilation and
lookup only. Passing these checks does NOT independently validate pronunciation.
The separate lexicon_v10 corpus contains manually authored contextual references.
"""

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "ttf_lexicon_manage", ROOT / "tools/lexicon/manage.py"
)
MANAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MANAGE)
cli, resources = sys.argv[1:]
resources = str(Path(resources).resolve())
checks = 0
failures = []


def run_stream(text, chunk=4096, overlay=()):
    run = subprocess.run(
        [
            cli,
            "stream-json",
            "--resources",
            resources,
            "--chunk-bytes",
            str(chunk),
            *overlay,
            "-",
        ],
        input=text.encode(),
        capture_output=True,
        timeout=20,
    )
    assert run.returncode == 0, (text, run.stderr.decode())
    events = [json.loads(line) for line in run.stdout.decode().splitlines()]
    assert events and events[-1]["event"] == "end", events
    segments = [event for event in events if event["event"] == "segment"]
    output = "".join(event["text"] for event in segments)
    spans = []
    raw = text.encode()
    for segment in segments:
        normalized = segment["text"].encode()
        for span in segment["spans"]:
            assert 0 <= span["input_begin"] <= span["input_end"] <= len(raw), span
            assert 0 <= span["begin"] <= span["end"] <= len(normalized), span
            spans.append(
                (
                    raw[span["input_begin"] : span["input_end"]].decode(),
                    normalized[span["begin"] : span["end"]].decode(),
                    span,
                )
            )
    return output, spans, events


records, _ = MANAGE.load(ROOT)
MANAGE.build(ROOT, check=True)
# lexicon_case ... upper: the entry applies only to a capital surface (5G, not 5g grams).
UPPER = set()
for tsv in (Path(resources) / "languages/ko-KR/rules").glob("*.tsv"):
    for line in tsv.read_text(encoding="utf-8").splitlines():
        cells = line.split("\t")
        if len(cells) == 4 and cells[0] == "lexicon_case" and cells[3] == "upper":
            UPPER.add(cells[2].split(":", 1)[1])
for row in records:
    for term in [row["term"], *row["aliases"]]:
        if term in UPPER:
            term = term.upper()
        text = term + "의 발음을 확인합니다."
        expected = row["pronunciation"] + "의 발음을 확인합니다."
        layer = "domain" if row.get("scope") == "finance" else "system"
        overlay = (
            ("--domain", str(Path(resources) / "domains/finance/ko-KR/lexicon.tsv"))
            if layer == "domain"
            else ()
        )
        output, spans, _ = run_stream(text, overlay=overlay)
        hits = [
            (rendered, span) for surface, rendered, span in spans if surface == term
        ]
        if output != expected or len(hits) != 1:
            failures.append(
                (
                    row["id"],
                    term,
                    expected,
                    output,
                    [(surface, span["lexicon"]) for surface, _, span in spans],
                )
            )
            continue
        rendered, span = hits[0]
        for key, value in {
            "source_lang": "en-US",
            "context_lang": "ko-KR",
            "render_lang": "ko-KR",
            "lexicon": "lexicon:" + layer,
            "pronunciation_layer": layer,
        }.items():
            if span[key] != value:
                failures.append((row["id"], term, key, value, span[key]))
        assert rendered == row["pronunciation"], (row["id"], rendered)
        checks += 1
assert not failures, "unreachable or incorrect system source entries:\n" + "\n".join(
    repr(failure) for failure in failures
)

# References in this file were manually written before candidate execution.
corpus = ROOT / "tests/evaluation/lexicon_v10.jsonl"
for line in corpus.read_text(encoding="utf-8").splitlines():
    case = json.loads(line)
    previous = None
    overlay = tuple(
        part
        for resource in case.get("resource_overrides", [])
        for part in ("--" + resource["layer"], str(Path(resources) / resource["path"]))
    )
    batch = subprocess.run(
        [cli, "normalize", "--resources", resources, *overlay, case["text"]],
        capture_output=True,
        text=True,
        timeout=15,
    )
    assert batch.returncode == 0 and batch.stdout == case["expected"] + "\n", (
        case["id"],
        batch.stdout,
        batch.stderr,
    )
    for chunk in (1, 7, 4096):
        output, spans, events = run_stream(case["text"], chunk, overlay)
        assert output == case["expected"], (case["id"], chunk, output)
        for expected in case.get("expected_spans", []):
            matching = [
                (rendered, span)
                for surface, rendered, span in spans
                if surface == expected["surface"]
            ]
            assert len(matching) == 1, (case["id"], expected, spans)
            rendered, span = matching[0]
            for key, value in expected.items():
                if key != "surface":
                    assert (rendered if key == "normalized" else span[key]) == value, (
                        case["id"],
                        key,
                        rendered,
                        span,
                    )
        if case["category"].endswith("english-preserve"):
            assert all(span["lexicon"] == "" for _, _, span in spans), (
                case["id"],
                spans,
            )
        if previous is not None:
            assert previous == events, (case["id"], chunk, "callback metadata differs")
        previous = events
        checks += 1

# Ambiguous finance initials are explicitly scoped, so ordinary default reads
# cannot silently acquire a financial dictionary pronunciation.
for term, pronunciation in (
    ("abs", "에이비에스"),
    ("per", "피이알"),
    ("roe", "알오이"),
):
    text = term + "의 발음을 확인합니다."
    output, spans, _ = run_stream(text)
    default_hits = [span for surface, _, span in spans if surface == term]
    assert len(default_hits) == 1 and not default_hits[0]["lexicon"].startswith(
        "lexicon:"
    ), (term, spans)
    output, spans, _ = run_stream(
        text,
        overlay=(
            "--domain",
            str(Path(resources) / "domains/finance/ko-KR/lexicon.tsv"),
        ),
    )
    assert output == pronunciation + "의 발음을 확인합니다.", (term, output)
    assert spans[0][2]["lexicon"] == "lexicon:domain", spans
    checks += 2

# A newly added single word and phrase both obey Customer > Domain > System.
with tempfile.TemporaryDirectory(prefix="ttf-inventory-layer-") as temp:
    temp = Path(temp)
    domain = temp / "domain.tsv"
    customer = temp / "customer.tsv"
    domain.write_text(
        "lexicon\tko-KR\ten-US:illustrator\t도메인발음\nlexicon\tko-KR\ten-US:covered bond\t도메인채권\n",
        encoding="utf-8",
    )
    customer.write_text(
        "lexicon\tko-KR\ten-US:illustrator\t고객발음\nlexicon\tko-KR\ten-US:covered bond\t고객채권\n",
        encoding="utf-8",
    )
    for term, domain_pron, customer_pron in (
        ("Illustrator", "도메인발음", "고객발음"),
        ("covered bond", "도메인채권", "고객채권"),
    ):
        for overlay, expected, layer in (
            (("--domain", str(domain)), domain_pron, "domain"),
            (
                ("--domain", str(domain), "--customer", str(customer)),
                customer_pron,
                "customer",
            ),
        ):
            output, spans, _ = run_stream(term + "의 발음을 확인합니다.", 1, overlay)
            assert output == expected + "의 발음을 확인합니다.", output
            assert (
                spans[0][0] == term and spans[0][2]["lexicon"] == "lexicon:" + layer
            ), spans
            assert spans[0][2]["pronunciation_layer"] == layer, spans
            checks += 1

print(
    f"{checks} lexicon source/runtime/layer/context/chunk conformance checks; no linguistic approval inferred"
)
