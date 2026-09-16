# External text benchmark: first measured iteration (v0.11)

Measured 2026-09-16. This is a **development measurement on a training-prefix
sample**, not a held-out score, a reproduction of the paper's experiment, or a
production accuracy claim. Evaluation covers text and metadata only. No speech
was synthesized. Aggregate machine-readable evidence is
[external-benchmark-summary.json](external-benchmark-summary.json); corpus text and
per-case failures remain in ignored local storage.

## Materials actually inspected and exercised

| Source | Work performed | Not performed |
| --- | --- | --- |
| NeMo | Pinned classifier/verbalizer, whitelist, FAR export/deployment inspection | Runtime comparison or code/resource port |
| Sproat/Jaitly | Pinned paper/repo, distribution format/license inspection, real bounded sample import and before/after evaluation | Full corpus, held-out test, human adjudication |
| KsponSpeech | Paper/transcription rules and source inspection; selective dual-transcript importer and synthetic tests | Acquisition or evaluation of restricted raw corpus |
| CoreaSpeech/N2gk+ | Paper, normalizer, actual official cards and Numeric schema inspection; optional local importer | Downloaded NC testset, generated output treated as gold |
| CMUdict | Full pinned dictionary import, variant/stress preservation and explicit native compiled export/load | All-word Korean pronunciation correctness certification |
| NIKL | Official transcription principles, English rules, conventional forms and use-policy review | Bulk copying of tables/dictionaries or complete rule implementation |

Details: [NeMo/Sproat](nemo-sproat.md), [Korean sources](korean-sources.md),
[CMU/NIKL](pronunciation-sources.md), [artifact-specific rights](../THIRD_PARTY.md).

## Fixed inputs and policies

- Baseline: commit `ba1928db94e3f947dce8f0e83f82505bb065480b`, v0.10.0 shared
  library and resources copied before modifications. Candidate: v0.11.0 changes
  accompanying this record. Library binary hashes are recorded; this is not a
  speed comparison between build configurations.
- Input: 3,785 complete sentences / 46,213 spans / 16 source classes, from the
  beginning of `en_with_types/output-00058-of-00100` (train). The bounded archive
  read obtained 49,998 TSV lines / 916,200 bytes through a complete EOS.
- Source TSV SHA256:
  `072964db038fbf9091658de7c8d95b0f02689e22c047173a0b3f70899d60c567`.
- Imported JSONL SHA256:
  `6b0a0a56a4955166ca1786dc6778ab744e372219cc4d8ab6bcf4d51943c2dd51`.
- The same `v2-exact-context-complete-output` evaluator and native batch adapter
  processed both runs. No reference, case order, spelling, case or whitespace
  was changed between them. Engine default language was `en-US`; no domain overlay.
- Every sentence was additionally streamed in chunks of **1, 7 and 64 bytes**.
  All flattened text and native metadata were compared against batch output.

## Before and after

| Metric | v0.10 baseline | v0.11 candidate | Change |
| --- | ---: | ---: | ---: |
| MEASURE span exact | 64/92 (69.57%) | 76/92 (82.61%) | +12 spans |
| MONEY span exact | 17/33 (51.52%) | 21/33 (63.64%) | +4 spans |
| TN-only span exact (excludes PLAIN/PUNCT) | 805/3,293 (24.45%) | 821/3,293 (24.93%) | +0.49 percentage points |
| All span exact | 34,669/46,213 (75.02%) | 34,685/46,213 (75.05%) | +16 spans |
| TN-only macro class accuracy | 31.65% | 33.45% | +1.80 percentage points |
| Literal sentence exact | 4/3,785 (0.11%) | 4/3,785 (0.11%) | unchanged |
| Secondary spoken projection exact | 2,017/3,785 (53.29%) | 2,023/3,785 (53.45%) | +6 sentences |
| Engine errors | 0 | 0 | unchanged |
| Chunk comparisons / failures | 11,355 / 0 | 11,355 / 0 | unchanged |

The comparison reports **16 fixed spans, 0 regressed spans**. All other classes
were unchanged on this sample. These counts mean agreement with the supplied
Kestrel references; they do not prove correctness on unseen inputs.

Literal sentence scores include the frontend's punctuation preservation, while
the dataset uses `sil` for most punctuation. The separate projection only removes
unchanged punctuation with an explicit empty reference and joins aligned outputs
with spaces. It does not alter lexical/numeric readings. **58 sentences cannot
be projected and still count as failures in the 3,785 denominator**; projection
coverage is 3,727/3,785. Native span alignment fails for 153 spans in each run,
which also remain failures. PLAIN constitutes 34,001 spans, so reporting only the
75% all-span score would conceal the much lower TN coverage.

## Deterministic changes motivated by failures

The implementation adds independently authored generic inventories for `mph`,
`km/h`, `ft`, `mi`, `nm`, `KB`, `MW`, `Wh/km`, `km²`, `US$` and `€`. Language
resources provide singular/plural English readings and Korean labels/prefixes.
Numeric values and units remain distinct in IR; no conversion of physical units
or currency values takes place. Keys are case sensitive: `mW` is not `MW`.

The classifier's Latin fast path now defers to a declared currency prefix, so
`US$` and customer-defined prefixes reach the shared currency grammar. Currency
prefix/suffix consistency uses the same contract for a single amount and range
endpoint. A matching explicit currency is consumed once; a conflicting unit
is retained as TEXT. Resource trace keeps the highest participating layer.

Original regression examples, **not corpus extracts**:

```text
The speed is 28 mph.       → The speed is twenty eight miles per hour.
속도는 47 km/h입니다.      → 속도는 시속 사십칠 킬로미터입니다.
금액은 US$15달러입니다.    → 금액은 십오 달러입니다.
금액은 US$15원입니다.      → 금액은 US$15원입니다.
```

The new contracts verify text, semantic units, repeated-unit ranges, byte chunks,
literal/URL protection, unknown suffixes, customer prefixes and layer provenance.
They do not derive expected output from the candidate engine. Human resource
maintenance is documented in the [Korean editing guide](../resource-editing-guide.ko.md).

## Remaining measured gaps and next iteration

| Class | Candidate reference agreement | Interpretation and next step |
| --- | ---: | --- |
| DATE | 163/1,463 | English month-name dates and contextual year readings need dedicated grammar/IR tests. Do not read every four-digit number as a year. |
| LETTERS | 35/712 | Source spelling/letter conventions differ from English preservation. Separate abbreviations, initials and ordinary words before choosing policy. |
| VERBATIM | 0/295 | Symbol/lexical reading conventions need explicit resource policy, including a decision on preserved symbols. |
| CARDINAL | 445/536 | Many source tokens are crossed by native RANGE/FRACTION spans. Review semantic grouping and reconstructed whitespace before changing number rules. |
| MONEY | 21/33 | Scaled English amounts, extra currency spellings and malformed mixed forms remain. Finance review prioritizes preserving exact value and currency. |
| MEASURE | 76/92 | Compound units and malformed/unsupported shapes remain; preserve case-sensitive unit semantics. |
| DIGIT | 10/35 | Context-dependent digit vs cardinal policy needs independent references. |
| ELECTRONIC / TELEPHONE / ADDRESS | 0/22, 0/13, 0/4 | URL preservation is an intentional policy difference; telephone/address formats also need broader grammar. Do not conflate them. |
| TIME / FRACTION | 1/4, 0/2 | Too few cases for a reliable rate. Enlarge independent samples and separate reading variants from value errors. |

Suggested order: scaled financial amounts and conflicting units; English dates
and abbreviation classes; expanded class-balanced held-out data with adjudication;
then CMU variant selection and improved phonetic feature/target rendering. Content
titles, names and literal snippets must retain their context/protection contracts.
Dictionary growth alone cannot resolve the syntax, context and reference-policy
gaps above.

## Reproduction and verification

The importer and evaluator commands are in [benchmark/README.md](../../benchmark/README.md).
Local detailed outputs for this run are:

```text
benchmark/generated/sproat-v10-final/
benchmark/generated/sproat-v11-final/
benchmark/generated/sproat-v11-comparison.json
```

Their summaries record corpus, selected-record, resource-tree, program, evaluator,
schema and adapter SHA256. Hashes are checked before and after each evaluation;
comparison rejects a different corpus or metric implementation. To reconstruct
the old engine later, build the baseline commit in a separate checkout and run
the **same current evaluator** against its library and matching resource tree.

Local verification: GCC CTest **59/59**, Clang **59/59**, combined ASan/UBSan
**58/58** (ctypes web test intentionally excluded under sanitizers). The final
currency trace change was also rebuilt and tested with the external/financial
subset on both Clang configurations. External import/parser tests are original
offline fixtures. Real CMU full import preserved 126,052 headwords and 135,166
pronunciations; explicit native export contains 117,493 supported keys with
COPYING/provenance. Kspon/Corea corpus measurements remain unperformed until
appropriately authorized local input and independent references are available.
