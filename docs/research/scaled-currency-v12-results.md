# Financial text feedback iteration (v0.12)

Measured **2026-09-17**. This iteration addresses the scaled-currency gap found
in the [v0.11 measurement](external-benchmark-results.md). It evaluates normalized
text, exact decimal values, currency identity, source spans and streaming metadata.
No speech synthesis or listening evaluation is involved.

## Same benchmark, measured change

The baseline is v0.11 commit `5fa63287516eac130e207c28917f041001da6ead`.
Its previous completed evaluation is compared with the v0.12 candidate using the
same imported **3,785 sentences, 46,213 reference spans and 16 classes**. This is
the bounded beginning of Sproat training shard 58, not a held-out test or a
representative estimate of financial production accuracy. MONEY has only 33 spans.

The comparator verifies identical corpus, selected records, evaluator, schema,
native adapter, metric policy and chunk schedules. It rejects incompatible runs.
Unaligned spans remain failures. Case, spelling and references were not changed
to raise the result. [Aggregate evidence](scaled-currency-v12-summary.json) records
before/after library and resource hashes. Corpus text, case IDs and detailed
failures remain in ignored local storage.

| Metric | v0.11 | v0.12 |
| --- | ---: | ---: |
| MONEY span exact | 21/33 (63.64%) | 30/33 (90.91%) |
| TN-only span exact, excluding PLAIN/PUNCT | 821/3,293 (24.93%) | 830/3,293 (25.20%) |
| All span exact | 34,685/46,213 (75.05%) | 34,694/46,213 (75.07%) |
| TN-only macro class accuracy | 33.45% | 35.40% |
| Literal sentence exact | 4/3,785 | 4/3,785 |
| Secondary spoken projection exact | 2,023/3,785 (53.45%) | 2,029/3,785 (53.61%) |
| Unaligned reference spans | 153 | 153 |
| Engine errors | 0 | 0 |
| Chunk comparisons / failures | 11,355 / 0 | 11,355 / 0 |

There are **9 fixed and 0 regressed reference spans**. All classes other than MONEY
retain their exact-match counts. Literal sentence scores include the intentional
preservation of punctuation that the dataset often marks `sil`. The secondary
projection removes only unchanged punctuation with an explicit empty reference;
it does not repair word readings. Its 58 unavailable sentences remain failures
in the full denominator.

The three remaining MONEY mismatches have explicit policy differences:

- AUD is rendered with `Australian dollars`; the reference uses generic dollars.
- NOK uses `Norwegian` with a capital N; the reference uses lower case.
- USD uses `dollars`; the reference includes `united states`.

They remain exact-match failures. They are not evidence that all other currency
inputs are correct. Customer labels can change the reading without changing the
IR currency code. The benchmark policy does not silently strip currency names
or ignore case.

## Changes and regression protection

Resources now declare thousand/million/billion/trillion aliases, additional
currency prefixes and ISO suffixes, and English minor-unit reading policies.
The universal classifier expands coefficients with bounded decimal-string
arithmetic. The English renderer can recover the coefficient and named scale;
the Korean pack reads the exact expanded amount. Prefix/suffix disagreement,
overflow and unsupported attached tails are preserved, not partly normalized.

Shared trailing scales in ranges have explicit scope rules, and explicit currency
prefixes on both endpoints prevent accidental scale inheritance. Adjacent negative
signs remain part of the amount, including Korean compound amounts. Physical
meters remain meters when no explicit monetary context establishes a scale.
See [the contract](../scaled-currency.md) and [editable data](../currency-resources.md).

The first candidate evaluation exposed nine regressed spans: short scale aliases
had consumed ordinary following words. Requiring a complete marker after whitespace
and currency-specific suffix evidence fixed those errors. Independent original
prose tests now check both output and input-span boundaries. This is why the
comparison reports fixed **and** regressed cases instead of only a success rate.

## Reproduction and local tests

After obtaining an appropriately permitted local input, use the import/evaluation
steps in [benchmark/README.md](../../benchmark/README.md). This run uses:

```sh
python3 benchmark/evaluators/evaluate.py \
  --corpus /path/to/the/same/sentences.jsonl \
  --library build/libtts_text_frontend.so --resources resources \
  --limit 0 --chunk-bytes 1 7 64 \
  --output-dir benchmark/generated/sproat-v12-release
python3 benchmark/evaluators/compare.py \
  benchmark/generated/sproat-v11-final \
  benchmark/generated/sproat-v12-release \
  --output benchmark/generated/sproat-v12-release-comparison.json
```

The JSONL SHA256 is
`6b0a0a56a4955166ca1786dc6778ab744e372219cc4d8ab6bcf4d51943c2dd51`.
The evaluator policy remains `v2-exact-context-complete-output`. To regenerate
the baseline, build the baseline commit separately, use its own resources and
run the same current evaluator. Do not overwrite an existing completed report.

The new original contracts test 614 CLI text/IR/resource/chunk cases and 1,276
native comparisons over every two-way byte split in selected inputs. A Decimal
oracle checks coefficients and expanded values independently of the C arithmetic.
The web corpus contains 16 original references with expected text and semantic
fields; all remain pending independent human review. That is implementation
verification, not a claim that a human approved the linguistic answer.

Final local CTest: **GCC 62/62**, **Clang 62/62**, combined **ASan/UBSan 60/60**.
The sanitizer configuration excludes the two Python ctypes loading tests;
native C streaming tests and the CLI currency contracts still run under sanitizers.
The Chromium workbench gate passed, including the new 16-case corpus, live
callbacks, custom chunk writes and mobile layout. The port-8000 service runs the
v0.12 release library and its native verification endpoint returned matching
text and streaming results.

## Remaining work

English DATE (163/1,463), LETTERS (35/712), VERBATIM (0/295), compound units and
contextual digit readings remain substantial gaps. Next priorities are contextual
year/month-name dates and abbreviation classes, with class-balanced held-out
inputs and independent reference review. CMU variant selection and richer
pronunciation rendering are separate dictionary/phonetic work. The frontend can
be integrated for a scoped pilot; this result does not make arbitrary finance
or content text production-ready.
