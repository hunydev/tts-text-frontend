# Written currencies and English initials: measured iteration (v0.14)

Measured **2026-09-17** against v0.13 commit
`ea0de28c0accc3ec7d9e701da820658e9ef1ebad`. This is the same Sproat
**training-prefix development sample**, not held-out production accuracy.
The scope is normalized text, semantic metadata and arbitrary input chunking.
No audio is generated or compared.

## Same input, references and scoring

Both runs contain 3,785 sentences, 46,213 reference spans and 16 source classes.
The comparator verifies unchanged corpus, evaluated records, reference fields,
metric policy, evaluator/schema/native adapter and chunk schedules. It also
reconstructs all summary counts from result rows and checks complete-file hashes.
Corpus JSONL SHA256:
`6b0a0a56a4955166ca1786dc6778ab744e372219cc4d8ab6bcf4d51943c2dd51`.

| Metric | v0.13 | v0.14 |
| --- | ---: | ---: |
| LETTERS span exact | 35/712 (4.92%) | 174/712 (24.44%) |
| DATE span exact | 319/1,463 | 319/1,463 |
| MONEY span exact | 30/33 | 30/33 |
| CARDINAL span exact | 441/536 | 440/536 |
| DECIMAL span exact | 28/35 | 26/35 |
| PLAIN span exact | 33,863/34,001 | 33,860/34,001 |
| TN-only span exact | 982/3,293 (29.82%) | 1,118/3,293 (33.95%) |
| All span exact | 34,845/46,213 (75.40%) | 34,978/46,213 (75.69%) |
| Literal sentence exact | 4/3,785 | 4/3,785 |
| Secondary spoken projection exact | 2,146/3,785 (56.70%) | 2,197/3,785 (58.04%) |
| Unaligned spans | 161 | 167 |
| Engine errors | 0 | 0 |
| Chunk comparisons / failures | 11,355 / 0 | 11,355 / 0 |

There are **139 fixed spans and 6 regressed spans**. All fixes are LETTERS.
The six losses comprise three amount/name pairs in two sentences: a new CURRENCY
span combines a numeric reference and its separately annotated currency word.
The complete normalized text of both sentences is byte-for-byte identical before
and after; alignment is unavailable for the separate reference spans. The losses
remain exact failures. No reference boundary or score was rewritten to hide them.
Projection has 63 unavailable sentences rather than 61; these remain failures too.

The classifier now expresses written money as CURRENCY even when the final
English text was already naturally readable. MONEY's unchanged 30/33 therefore
cannot be used to measure this feature's entire coverage. Independent value and
currency-ID contracts cover the new forms. Undotted uppercase words are preserved
by default, so this release does not pretend to fix every LETTERS annotation.

[Aggregate evidence](english-forms-v14-summary.json) contains before/after program,
resource and input hashes, report integrity checks, class counts and diagnostics.
It contains no external corpus text or case identifiers. Detailed local reports
remain ignored. Source restrictions remain in [THIRD_PARTY](../THIRD_PARTY.md).

## Independent contracts

- Written currency integration: 380 text/IR/guard/resource/chunk checks and
  2,156 native comparisons covering every two-way byte split of selected inputs.
- Abbreviation integration: 277 text/IR/policy/guard checks and 1,166 native
  byte-split/override checks. Includes ordinary `us` dictionary versus `U.S.`
  letter pronunciation, titles, default-context routing and terminal punctuation.
- Pronunciation aliases: 164 layer/IR/text checks and 372 native byte comparisons.
  Original and canonical keys compete by layer; exact spelling breaks ties.
- Resource compiler: normal LF/CRLF/EOF layouts roundtrip; unsupported record
  separators are rejected instead of producing a binary the runtime cannot load.
  Separators inside field/comment content are preserved by both implementations.
- Comparator: equally truncated reports, inconsistent exact flags, modified
  reference fields and forged summary totals are rejected by original fixtures.
- Web: 24 independently authored text/IR cases under **영어 통화명·약어·이니셜**.

All references and the four new Korean title pronunciations are pending independent
human review. The reference audit reports 803 cases in 19 files with no mechanical
reference defects, and **zero independently approved cases**. Passing tests cannot
supply that approval.

Final local gates: GCC CTest **73/73**, Clang **73/73**, combined ASan/UBSan
**67/67**. Six Python ctypes loading tests are excluded from the sanitizer
configuration; the native C tests and CLI integration contracts run under it.
Chromium verification passed, including the new 24-case corpus, live callbacks,
arbitrary writes, download, user overrides and mobile layout. The port-8000
service reports version 0.14.0. Original ordinal/Apple/GPU/WiFi CLI examples still
produce the documented output.

## Reproduce

```sh
python3 benchmark/evaluators/evaluate.py \
  --corpus /path/to/the/same/sentences.jsonl \
  --library build/libtts_text_frontend.so --resources resources \
  --limit 0 --chunk-bytes 1 7 64 \
  --output-dir benchmark/generated/sproat-v14-final
python3 benchmark/evaluators/compare.py \
  benchmark/generated/sproat-v13-final benchmark/generated/sproat-v14-final \
  --output benchmark/generated/sproat-v14-final-comparison.json
python3 benchmark/evaluators/diagnose.py benchmark/generated/sproat-v14-final
```

For a fresh baseline, build the v0.13 commit separately with its own resources,
then evaluate using the same current evaluator and corpus. Generated directories
must be new so previous evidence is not overwritten.

## Remaining work

The [editing guide](../english-forms.md) and [written currency guide](../written-currencies.md)
state finite grammar, name and currency ambiguity policies. The full
[remaining-work checklist](../remaining-work.md) retains Unicode number grouping,
Korean counter roles, international phones, source pronunciation layer trace,
held-out evaluation, independent references, dictionary scale and portability.
These measured improvements do not close those requirements.
