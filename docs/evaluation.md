# Text evaluation

The acceptance target is the **normalized output text**. Evaluation checks exact
reference text, semantic/language metadata, and equality between batch and
incremental processing. It requires only the native CLI and Python's standard
library. It never calls a synthesis backend or evaluates generated speech.

## Reproduce a run

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
python3 tools/evaluation/evaluate.py --ttf build/ttf --resources resources \
  --corpus tests/evaluation/cases.jsonl --output-dir artifacts/evaluation-text
```

The output directory must be new. Each case runs once through `ttf normalize` and
through `ttf stream-json` with **1, 7, and 64 input bytes per write**, including
splits inside Hangul UTF-8 sequences. Override the sizes with `--chunk-bytes 1 13
128`. Native streaming tests additionally exercise all single split positions and
random chunk partitions.

A case passes only when:

1. The concatenated streaming output exactly equals its independently specified
   expected string, including whitespace, punctuation, case, and Unicode form.
2. Every streaming chunk variant equals the batch output.
3. Every requested semantic and language metadata check matches.
4. Protocol events and spans cover the complete input and output, in order, with
   valid UTF-8 byte boundaries and a completed end event.

A malformed or incomplete protocol is an evaluation error, never a passing case.

## Corpus and coverage

`tests/evaluation/cases.jsonl` contains 109 explicit reference cases in 18
categories. They include:

| Area | Examples |
| --- | --- |
| Korean normalization | Native and Sino counters, spaced units, signed decimals, scaled currency, labelled/dotted dates, leap dates, seconds, fractions, phones |
| English normalization | Cardinals, ordinal suffixes including 11th/12th/13th/21st, singular quantities, spaced currency/percent/units, dates, seconds, fractions |
| Code mixing | Both contexts of `1st`, Apple preservation/localization, embedded and quoted English, multiword ordinal rendering |
| Foreign pronunciation | Brands, initialisms versus acronyms, technical words, an unknown word through prototype G2P |
| Invalid structured input | Wrong ordinal suffixes, malformed comma groups, impossible dates/times, zero denominators, partial unit matches |
| Protected syntax | URLs, email, versions, IP addresses |
| Streaming | Incomplete numbers, empty input, tabs, repeated newlines, CRLF preservation, three chunk sizes for every case |
| Customization | Default range reading, finance domain, customer overriding domain |

There are 75 explicit span checks. These verify semantic classification and, for
key code-mixed cases, source/context/render language distinctions. Native tests
cover additional resource priorities, user lexicon changes, dictionary removal,
emergency fallback, third-party language packs, and concurrent engine use.

References are authored from the intended reading policy. They must not be
regenerated from the frontend's current output. When an output differs, determine
whether the reading policy or implementation is wrong, document the decision,
and add the smallest regression that captures the failure.

This corpus is a regression set, not a representative estimate of arbitrary
real-world accuracy. It deliberately overlaps native golden cases while adding
protocol and routing assertions. No claim of general language coverage follows
from a 100% result on these cases. Expanding independent customer/domain examples
and unknown-word references remains necessary.

## Corpus schema

Each JSONL object needs `id`, `text`, and `expected`. Optional fields:

- `category`: stable category label; defaults to `uncategorized`.
- `default_language`: defaults to `ko-KR`.
- `notes`: explanation of the intended reading.
- `expected_spans`: assertions for a source substring and its zero-based
  non-overlapping `occurrence`. A match must cover exactly the source substring's
  UTF-8 byte range; substring matching against larger spans is not sufficient.
- `resource_overrides`: ordered `{ "layer": "domain" | "customer", "path": ... }`
  entries. Paths are relative to `--resources`; traversal and symlinks escaping
  that directory are rejected. Runtime resource precedence still applies.

For example:

```json
{
  "id": "ordinal-context",
  "category": "code-mixed",
  "text": "1st는 첫 번째입니다.",
  "expected": "퍼스트는 첫 번째입니다.",
  "expected_spans": [{
    "surface": "1st",
    "source_lang": "en-US",
    "context_lang": "ko-KR",
    "render_lang": "ko-KR",
    "semantic_type": "ORDINAL",
    "value": "1",
    "normalized": "퍼스트"
  }]
}
```

Store that object on one line in the JSONL corpus. Span assertions may also check
`script`, `unit`, `rule`, `resource_layer`, `pronunciation_layer`, `lexicon`,
`verbalizer`, and `context_trace`. Prefer stable semantic expectations over exact
heuristic score strings unless testing the scoring policy itself.

## Metrics and artifacts

- **Exact match rate:** matching canonical streaming strings divided by cases.
- **Pass/fail:** combines exact text, all chunk variants, and metadata checks.
  A metadata or parity failure may occur even when exact match rate is 100%.
- **Character edit distance:** Unicode codepoint Levenshtein distance; insertion,
  deletion, and substitution each cost one. No normalization or folding is done.
- **Character error rate (CER):** sum of distances divided by
  `max(1, sum(reference codepoints))`. Individual cases use the same formula.
  Rates can exceed one. The denominator convention also covers empty references.
- **Category summaries:** the same metrics for each category, so easy cases do
  not hide a failed area.

Artifacts:

| File | Content |
| --- | --- |
| `report.json` | Version 2 report, all input/reference/actual strings, metrics, metadata failures, chunk variants, resource paths, SHA-256 provenance |
| `report.html` | Static escaped report with failures first, category table, character differences, and inspect metadata |
| `cases.csv` | Per-case pass status and exact-match/CER/metadata/parity metrics |
| `<case-id>/input.txt` | Exact UTF-8 input bytes |
| `<case-id>/segments.jsonl` | Events for the first configured chunk size |
| `<case-id>/segments-<n>.jsonl` | Events for other chunk sizes |

Display differences use `difflib` alignment and Unicode codepoint offsets. That
alignment aids inspection; it is not the minimal Levenshtein edit sequence.
Protocol spans use UTF-8 byte offsets, a different unit.

`fallback_spans` counts paths containing `prototype` or `fallback`. Lexicon hits,
source dictionary hits, initialism paths, and source/context/render routes remain
visible. A fallback occurrence is a diagnostic, not automatically an error: the
reference output determines whether the text is correct.

The report records CLI, corpus, and resource SHA-256 hashes. Process timings
include CLI startup and resource loading; use the native benchmark for committed
callback latency and throughput. No content is sent to an external API.

## Exit status and failure tests

The CLI prints its summary as JSON:

- `0`: every case passes.
- `1`: text, metadata, or batch/stream mismatch; complete reports are written.
- `2`: invalid corpus, failed subprocess, invalid protocol, or filesystem error.

Existing output directories are refused so earlier evidence is preserved. A
failed run can leave partial files; only a completed `report.json` represents a
completed evaluation.

`tests/evaluation/test_evaluation.py` runs the reference corpus and deliberately
introduces text errors, wrong metadata, and batch/stream divergence to prove the
gates reject each independently. It also tests codepoint metrics, malformed
corpora, missing/gapped/invalid UTF-8 spans, safe HTML escaping, and preservation
of an existing report.

## Finance/content corpus and revision comparison

`tests/evaluation/domain_cases.jsonl` contains 60 original finance/content/customer
scenarios with frozen reference text. It is a strict CTest and CI gate. The separate
15-case `challenge_cases.jsonl` is reported as advisory; text mismatches remain
visible, while malformed protocol or execution errors still fail CI. See
[corpus provenance and limits](domain-corpus.md). No real customer accuracy is
inferred from these synthetic examples. Since v0.5, `advanced_cases.jsonl` adds 30
strict regressions: 13 adopted challenge references plus 17 added finance/content
references. The unchanged challenge report therefore measures known-case progress,
not unseen generalization. Native/Python integration checks separately validate
range endpoints, identifier cue names and AM/PM stored in `secondary`.

```sh
python3 tools/evaluation/compare.py \
  --before artifacts/domain-baseline-0.3.0/report.json \
  --after artifacts/domain-candidate/report.json \
  --output-dir artifacts/domain-comparison
```

The comparison refuses differing case sets, reference strings, metadata assertions,
languages, resource override configurations or chunk sizes. It records report hashes
and classifies cases as fixed, regressed, remaining failure or unchanged pass. A
metadata regression counts even when the string remains correct. Increased text
edit distance is also reported, even when the case already failed before. Exit 1
means a newly failing case or increased edit distance; exit 2 means incomparable/malformed evidence. Remaining failures are
reported and never reclassified as passes. The comparison is an analysis tool, not
a replacement for the strict evaluator's acceptance exit status.

## Protected-source corpus (v0.6)

`protected_cases.jsonl` adds 40 strict original synthetic references. Fixed before
baseline/candidate execution, they compare exact source preservation, neighboring
TN, language neutrality and metadata. CTest and CI evaluate them independently of
the 109/60/30-case existing corpora. The original 15 historical challenge references
are unchanged and now all pass; all are known development material, not a held-out
measurement. See [protected text provenance](protected-text.md) and versioned
`docs/evidence/0.6.0-text-results.json` for comparison data.

## v0.8 metadata migration

The current finance/content gate uses `tests/evaluation/domain_cases_v8.jsonl`.
It preserves all 60 original input and expected output strings. Exactly one case,
`content-product-watch`, now expects the combined `Apple Watch` pronunciation span.
The earlier file remains unchanged for historical comparison; applying its exact
old word-level offsets to v0.8 intentionally fails that metadata check while all
60 text strings still match. Two focused context tests likewise assert the new
combined span. This is not a text-accuracy improvement.

The separate frozen `multiword_v8.jsonl` has 30 new text/metadata references,
including English/title/literal preservation. Before/after comparisons use this
same file and its hash. No reference is changed to accommodate a candidate result.
