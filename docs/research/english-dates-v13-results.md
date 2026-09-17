# Named dates and contextual years: measured iteration (v0.13)

Measured **2026-09-17**. This follows the [v0.12 currency iteration](scaled-currency-v12-results.md)
using the same Sproat **training-prefix development sample**, not held-out test
data. Evaluation concerns normalized text, semantic values and streaming metadata.
No audio is generated or evaluated. The dataset references are not human-approved
gold; agreement is reported literally, including capitalization and alignment.

## Fixed inputs and metrics

Baseline commit: `adecf20d225f8088f420e934e042ad1dda0def6e`, v0.12. Candidate:
v0.13 changes accompanying this document. Both runs use the same 3,785 sentences,
46,213 reference spans, 16 source classes and `v2-exact-context-complete-output`
policy. The comparator verified identical corpus, evaluated records, reference
digests, evaluator/schema/native adapter and chunk schedules.

| Metric | v0.12 | v0.13 |
| --- | ---: | ---: |
| DATE span exact | 163/1,463 (11.14%) | 319/1,463 (21.80%) |
| CARDINAL span exact | 445/536 | 441/536 |
| PLAIN span exact | 33,864/34,001 | 33,863/34,001 |
| MONEY span exact | 30/33 | 30/33 |
| TN-only span exact, excluding PLAIN/PUNCT | 830/3,293 (25.20%) | 982/3,293 (29.82%) |
| All span exact | 34,694/46,213 (75.07%) | 34,845/46,213 (75.40%) |
| Literal sentence exact | 4/3,785 | 4/3,785 |
| Secondary spoken projection exact | 2,029/3,785 (53.61%) | 2,146/3,785 (56.70%) |
| Unaligned spans | 153 | 161 |
| Engine errors | 0 | 0 |
| Chunk comparisons / failures | 11,355 / 0 | 11,355 / 0 |

The comparison records **157 fixed spans and 6 regressed spans**: 156 DATE fixes
and 1 CARDINAL fix; 5 CARDINAL and 1 PLAIN regression. Other class exact counts
are unchanged. These losses remain in the published score.

All six regressions are newly unavailable alignments after a DATE or invalid-date
TEXT span groups source fields. Manual inspection of the local diagnostic records
found three references with broken, space-separated year fragments, two spans
in an unsupported month/three-digit-number expression, and one valid full named
date whose year was separately annotated CARDINAL. The implementation preserves
unsupported date-shaped text and combines valid full dates. We did not distort
that policy or repartition references to erase these losses. This is a bounded
engineering review of those cases, not independent linguistic adjudication.

Overall, ten spans became unaligned and two previously unaligned spans aligned:
net eight. Projection now has 61 unavailable sentences (coverage 3,724/3,785);
all 61 remain failures. Literal sentence exact still includes punctuation that
the frontend preserves while many source references mark it silent.

## Why exact DATE remains low

The new generic diagnostic tool reports these disjoint DATE categories:

| Result | Count |
| --- | ---: |
| Exact | 319 |
| Same after Unicode casefold only | 698 |
| Other text difference | 419 |
| Unaligned | 27 |

The 698 casefold-only differences remain **failures** in exact accuracy. Most
named-date references use lowercase months while project output retains ordinary
capitalized English month names. The diagnostic does not silently lowercase a
reference or improve the headline metric. Other differences include bare years
without permitted cues, alternate year readings, ISO date order and unsupported
forms. A DATE semantic label alone is not proof that its value is right.

[Aggregate evidence](english-dates-v13-summary.json) includes before/after program,
resource and input hashes plus class counts and diagnostics. It contains no corpus
sentences or case identifiers. External source/use restrictions remain documented
in [THIRD_PARTY](../THIRD_PARTY.md); generated detailed reports remain ignored.

## Independent contracts and implementation

The [date guide](../english-dates.md) defines full/partial value shapes, calendar
validation, source order and reading policies. Named-date classification stays in
the English pack; Korean rendering reads those shared values through its own
resources. A bare four-digit number stays a cardinal unless a configured cue and
quantity guards establish a year. Source language for a numeric-only year remains
neutral. ISO date text and established Korean month spacing are unchanged.

The 665 original text/calendar/IR/chunk/resource checks include a Python Gregorian
calendar oracle, leap centuries, all month lengths, missing components, signs,
units, identifiers, finance/content quantity nouns, Korean adjacency, invalid
ordinals, literal protection, customer policies and invalid reading styles.
There are also **1,174 native comparisons at every two-way byte split** across
selected inputs under default and tiny phrase targets. These expectations were
written independently of candidate output and do not contain external corpus text.

The web corpus **영어 날짜·연도·코드믹스** contains 20 original contracts; all are
pending independent human review. The reference audit does not turn passing
implementation tests into linguistic approval.

Final local gates: **GCC CTest 66/66**, **Clang 66/66**, combined **ASan/UBSan
63/63**. The sanitizer configuration excludes three Python ctypes loading tests;
the native C tests and CLI date contracts still run under sanitizers. Chromium
verification passed, including the 20-case date corpus, callbacks, custom writes,
literal protection and mobile layout. The port-8000 service runs v0.13.

## Reproduce

Use an appropriately permitted local corpus as described in
[benchmark/README.md](../../benchmark/README.md). This sample's JSONL SHA256 is
`6b0a0a56a4955166ca1786dc6778ab744e372219cc4d8ab6bcf4d51943c2dd51`.

```sh
python3 benchmark/evaluators/evaluate.py \
  --corpus /path/to/the/same/sentences.jsonl \
  --library build/libtts_text_frontend.so --resources resources \
  --limit 0 --chunk-bytes 1 7 64 \
  --output-dir benchmark/generated/sproat-v13-final
python3 benchmark/evaluators/compare.py \
  benchmark/generated/sproat-v12-release benchmark/generated/sproat-v13-final \
  --output benchmark/generated/sproat-v13-comparison.json
python3 benchmark/evaluators/diagnose.py benchmark/generated/sproat-v13-final \
  > benchmark/generated/sproat-v13-diagnostics.json
```

For a fresh baseline, build the baseline commit separately with its matching
resources and use the same current evaluator. Keep development and future held-out
measurements separate.

## Limits and next work

Year cues and quantity guards are finite inventories, not general language
understanding. Locale-ambiguous numeric dates, eras, decades, date ranges,
relative dates and arbitrary broken input need further work. Do not interpret
this result as full financial/content production accuracy. Next priorities are
English abbreviation/initial classes, context-safe written currency names,
class-balanced held-out inputs and independent reference review; pronunciation
dictionary variant selection remains a separate pipeline task.
