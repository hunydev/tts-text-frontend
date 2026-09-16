# Korean external sources: provenance, permissions, and text-only benchmarks

Research date: **2026-09-16**. This is a record of the primary sources checked,
not a grant of dataset rights. Only small code repositories and public metadata
were inspected. No KsponSpeech or CoreaSpeech corpus/audio was downloaded,
redistributed, or added to product resources. The importer tests use original
synthetic strings; they are **format tests, not results measured on either corpus**.

## Pinned sources

| Material | Exact revision / identifier | Primary source |
|---|---|---|
| KsponSpeech paper | Applied Sciences 2020, 10(19), 6936; DOI `10.3390/app10196936` | [Publisher article](https://www.mdpi.com/2076-3417/10/19/6936), [publisher PDF](https://mdpi-res.com/d_attachment/applsci/applsci-10-06936/article_deploy/applsci-10-06936.pdf) |
| KsponSpeech preprocessing code | `54fe0cebd890e519d424ae9d339a57040b399108` | [Repository at revision](https://github.com/sooftware/ksponspeech/tree/54fe0cebd890e519d424ae9d339a57040b399108), [preprocess.py](https://github.com/sooftware/ksponspeech/blob/54fe0cebd890e519d424ae9d339a57040b399108/preprocess/preprocess.py) |
| KsponSpeech data | AI Hub dataset `123`; no corpus release/hash acquired | [Official Korean speech dataset](https://www.aihub.or.kr/aihubdata/data/view.do?currMenu=115&topMenu=100&aihubDataSe=data&dataSetSn=123) |
| CoreaSpeech paper | NeurIPS 2025 Datasets and Benchmarks Track | [Proceedings PDF](https://proceedings.neurips.cc/paper_files/paper/2025/file/5b221bc040a287a255a0f311598aec61-Paper-Datasets_and_Benchmarks_Track.pdf) |
| CoreaSpeech code / N2gk+ | `424229a8affea08848e2e647c0d3e471b3ef9d39` | [Repository at revision](https://github.com/CoreaSpeech/CoreaSpeech/tree/424229a8affea08848e2e647c0d3e471b3ef9d39), [normalization.py](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/src/module/data_conditioning/normalization.py) |
| KoreanUniversalTestset official distribution | `aanonyyy/M6A5P1Q7` at `9829037ec950e08db7ef499656cae69c7157bd6b` | [Pinned dataset card](https://huggingface.co/datasets/aanonyyy/M6A5P1Q7/blob/9829037ec950e08db7ef499656cae69c7157bd6b/README.md) |
| CoreaSpeech training distribution | `aanonyyy/F5I9N7A1` at `950d6a50b2b3e338d141313d11440fe47b037745` | [Pinned dataset card](https://huggingface.co/datasets/aanonyyy/F5I9N7A1/blob/950d6a50b2b3e338d141313d11440fe47b037745/README.md) |

The two Hugging Face repository names are unusual but are the datasets linked
by the [official CoreaSpeech project page](https://coreaspeech.github.io/demo/).
Other similarly named uploads were not treated as authoritative mirrors.
The local shallow code checkouts used for inspection were under
`/tmp/ttf-research-v11/`; they are not vendored or build dependencies.

## Licenses must be separated by artifact

| Artifact | Observed terms | Commercial use | Redistribution / vendoring decision |
|---|---|---|---|
| Kspon preprocessing **code** | [MIT license file](https://github.com/sooftware/ksponspeech/blob/54fe0cebd890e519d424ae9d339a57040b399108/LICENSE), copyright Soohwan Kim, 2020 | MIT permits it, subject to its conditions | MIT copyright/permission notice must accompany substantial copied code. No upstream code is copied here. |
| Kspon **paper** | Article states CC BY 4.0 on its final page | Article reuse follows its own license | This does not license corpus transcripts/audio. We cite the article. |
| Kspon **dataset** | AI Hub dataset access and applicable agreement | Cannot infer unrestricted permission from the MIT code or paper | No corpus vendoring. Local use only after confirming applicable purpose, location and access permissions. |
| Corea preprocessing **code** | [README license section](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/README.md#license) claims MIT; no standalone LICENSE at this revision | MIT claim permits code reuse in principle | Before actual vendoring, clarify the copyright/notice text and audit dependencies. This project copies no N2gk+ code, tables or model assets. |
| Corea **dataset, Testset, checkpoints** | README and paper checklist claim CC BY-NC 4.0; official dataset cards add stricter restrictions | No commercial grant established | **Conflict unresolved.** No product inclusion or redistribution. Optional local benchmark requires separately established rights. |

### AI Hub access is not an unconditional benchmark license

The current [AI Hub policy, “AI 허브 개방 데이터” section](https://aihub.or.kr/intrcn/guid/usagepolicy.do?currMenu=151&topMenu=105)
mentions commercial/noncommercial research and development, but its detailed
conditions also restrict use to learning-model training, prohibit unapproved
third-party access/provision, require separate agreement for overseas entities
or export, and require NIA attribution. Dataset sale/commercial exploitation
has a separate consultation clause. The dataset page additionally limits
applications to Korean nationals. These provisions must not be summarized as
“MIT data” or “free commercial benchmark data.” The user's particular agreement
and intended text-only evaluation need confirmation; local possession alone is
not evidence of that permission. The importer records an invocation acknowledgement,
which does not obtain, verify, or grant rights.

### Corea license conflict

The pinned official cards specify research-only use and prohibit modification
and redistribution. This is stricter than the README's general CC BY-NC claim:
[CC BY-NC 4.0 legal code](https://creativecommons.org/licenses/by-nc/4.0/legalcode.en)
permits specified noncommercial sharing/adaptation with attribution and change
notices. Do not silently choose the more permissive text. Obtain clarification
from the rights holder before relying on a grant that the card appears to deny.
`--allow-noncommercial` is an isolation control, not that clarification. Business
context does not automatically become noncommercial merely because a command
is called a benchmark. Where authorized, retain creator/source/license notices,
cite the paper, and identify modifications; none of this resolves conflicting terms.

## KsponSpeech: what the annotations mean

Paper §2.2 describes orthographic/phonetic dual transcription, including
nonstandard pronunciation as well as numbers, units and foreign forms. It also
describes disfluency/noise conventions and special number spacing. The paper
states EUC-KR; the pinned reference preprocessor reads CP949. Its cleanup removes
punctuation and contains file-specific percent corrections, so it is not reused
as a TN gold generator. [Paper §2.2](https://mdpi-res.com/d_attachment/applsci/applsci-10-06936/article_deploy/applsci-10-06936.pdf),
[reference preprocessor](https://github.com/sooftware/ksponspeech/blob/54fe0cebd890e519d424ae9d339a57040b399108/preprocess/preprocess.py).

Our independent importer:

- Reads local `.txt` files only, strictly decoding CP949 by default; EUC-KR and
  UTF-8 are explicit alternatives. Invalid encoding fails without replacement.
- Reconstructs the entire written side of each `(written)/(spoken)` pair. Literal
  balanced parentheses survive. Nested/unbalanced annotations fail visibly.
- Selects numeric, currency, date/time, unit, Latin and symbol shapes as **TN
  candidates**. Ordinary orthography/pronunciation changes and marked
  disfluencies are excluded and recorded with reasons.
- Uses `expected: null` for the sentence. A selected pair is `candidate`, never
  `dataset-reference` or human approval. Selection can contain false positives
  and miss valid TN cases; it is deliberately not an exhaustive classifier.
- Preserves original reference spaces and punctuation. Source-file SHA256 and
  raw encoding byte offsets allow inspection; output span offsets are UTF-8
  bytes in reconstructed `text`. The two coordinate systems are distinct.
- Does not turn spoken-side dialect, phonological change or ASR transcription
  conventions into this frontend's preferred written reading policy.

Correctness comparisons therefore need a reviewer to confirm selected spans and
choose the intended TN policy. Candidate-only agreement is a separate diagnostic;
whole-sentence exact match has no denominator until genuine references exist.

## CoreaSpeech / N2gk+ and Numeric

The NeurIPS paper §2.1 describes LNCat filtering and N2gk+ normalization; §5.2
introduces Clean/Noisy/Numeric cross-sentence TTS tests. Numeric has 100 pairs
of distinct utterances. Its TTS evaluation uses synthesized speech and metrics
such as recognition error and speaker similarity, which are not our text-only
TN scores. [NeurIPS paper](https://proceedings.neurips.cc/paper_files/paper/2025/file/5b221bc040a287a255a0f311598aec61-Paper-Datasets_and_Benchmarks_Track.pdf).

The pinned [Universal Testset card](https://huggingface.co/datasets/aanonyyy/M6A5P1Q7/blob/9829037ec950e08db7ef499656cae69c7157bd6b/README.md)
lists `wav1, duration1, text1, wav2, duration2, text2`. **`text1` is not the
written form of `text2`.** An authorized text-only JSONL export should retain
`text1` and `text2` as two independent inputs; the importer emits separate IDs,
`expected: null`, and unlabeled lexical spans. No audio or Parquet reader is a
dependency. No conversion is claimed for an uninspected local export.

The [training card](https://huggingface.co/datasets/aanonyyy/F5I9N7A1/blob/950d6a50b2b3e338d141313d11440fe47b037745/README.md)
lists `text`, `tr`, `N2gkPlus`, speaker/audio and other fields. The importer can
read `text` via `--dataset-part training`; it never promotes `tr`, `N2gk` or
`N2gkPlus` to a reference. The pinned
[N2gk+ implementation](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/src/module/data_conditioning/normalization.py)
exports generated normalization, not independently annotated answers. The
following comparison traces actual calls rather than crediting every declared
table as an active feature. We do not execute or copy upstream code.

### Implementation coverage comparison

This is a source review against the pinned implementation and the current C
working tree on 2026-09-16, **not a measured head-to-head accuracy result**. Local
CLI probes below are original examples. Neither system's output becomes the
other's reference. Each benchmark family needs independently prepared text
references and an explicit policy for spacing, pronunciation variants and
intentional preservation.

| Area | Pinned N2gk+ implementation | Current C engine: coverage and remaining gap | Benchmark class and independent reference needed |
|---|---|---|---|
| Numerals / counters | `N2gk.__call__` chains numeric helpers; native counters are bounded below 100; decimal paths use floats. [Numeric implementation](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/src/module/data_conditioning/normalization.py#L85-L173) | [KO cardinal/native rendering](../../languages/ko-KR/pack.c) preserves decimal digits as text and supports resource-selected native/Sino counters, dates, clocks, fractions and quantity ranges. [Counter resources](../../resources/languages/ko-KR/rules/numeric.tsv) are finite: ordinal-like `번째` and context-dependent `번` need further policy review; zero and ≥100 use Sino readings. | `NUMBER`, `COUNTER`, `DATE`, `TIME`, `FRACTION`, `RANGE`. Review native/Sino selection independently; include zero, 20/21/99/100, negatives, trailing fractional zeros and ambiguous counters. Kspon selected spans remain candidates until reviewed. |
| English tokens and context | `apply_word_mapping` replaces individual uppercase letters; lowercase characters pass through. No context-language routing appears in this call chain. [Active helper](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/src/module/data_conditioning/normalization.py#L599-L605) | [KO/EN context scoring](../../languages/ko-KR/pack.c) distinguishes source/context/render languages and attached particles; a Korean-context lexicon hit localizes `Apple`, while English sentences preserve it. Routing remains heuristic, particularly quotations, short headings and weakly marked mixed clauses. | `WORD`, `ORDINAL`, `ALNUM_TERM`, plus source/context/render assertions. Use matched Korean/English sentence pairs with independent pronunciation and language-preservation references. |
| Abbreviations / acronyms | `WORD_MAPPING` is assembled, but the active helper never consults it; declared acronym/company spellings cannot be counted as working lookup. [Initialization and helper](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/src/module/data_conditioning/normalization.py#L554-L605) | [Foreign processor](../../src/pronunciation/foreign.c) checks layered lexicons before pronunciation generation; [EN pronunciation](../../languages/en-US/pack.c) supports initialisms and alphanumeric forms. NASA-style word readings and SQL/GIF alternatives depend on lexicon policy. Finance-specific overrides are opt-in; source-checked entries are not equivalent to human approval. | `INITIALISM`, `ACRONYM`, `ALNUM_TERM`. Review letter-name versus word readings, brand case variants and domain scope; test user/customer overrides independently of system defaults. |
| Symbols | Symbol substitution follows deletion; `±` is removed before its own mapping can apply. [Removal and call order](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/src/module/data_conditioning/normalization.py#L571-L597), [pipeline](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/src/module/data_conditioning/normalization.py#L655-L662) | Semantic `%`, currency symbols and interval `~` have context-specific behavior; standalone `~` has a KO resource reading. Standalone `±`, `&`, `#`, `@` currently remain literal, and there is no comprehensive mathematical/fullwidth-symbol verbalizer. Preservation is not proof that Korean TTS will read them correctly. | `SYMBOL`, `PERCENT`, `CURRENCY`, `RANGE`. References must distinguish literal symbol names, operators, identifiers and quantity intervals; include fullwidth forms and protected regions. |
| Unknown foreign words | No pronunciation IR or dictionary→G2P→target-language fallback occurs in the inspected N2gk+ call chain. [Pipeline](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/src/module/data_conditioning/normalization.py#L655-L662) | [Foreign processor](../../src/pronunciation/foreign.c) uses source pronunciation dictionaries, a small grapheme G2P, shared phone IR, KO phone mapping and emergency graphemes. Unknown ASCII Latin remains speakable in KO context, but phonetic accuracy, stress and loanword spelling remain prototypes; non-ASCII fallback can be approximate. | `WORD` / unknown-Latin cases with deliberate dictionary misses. Score pronunciation against independently reviewed references; nonempty Hangul and trace-path checks are only mechanical safety checks. |
| Punctuation / retained content | `remove_symbols` deletes parenthesized content and selected punctuation before other normalization. [Removal helper](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/src/module/data_conditioning/normalization.py#L571-L597) | [Normalization pipeline](../../src/core/normalize.c) preserves ordinary punctuation and [protected regions](../../src/protected/protected.c), with offsets and streaming segmentation. Parenthetical prose remains present. URL/code preservation and punctuation retention need their own contract; they do not supply spoken readings for every symbol. | `TEXT`, `URL`, `EMAIL`, `VERSION` and protected-span contracts. Independent references specify content that must survive, punctuation treatment and exact byte coverage; test batch/stream equivalence separately from linguistic correctness. |

Two static-source cautions matter when selecting reference cases: the N2gk unit
lists contain adjacent string literals without separating commas, and helper
definitions/tables are not evidence that a given path reaches them. Do not
repair upstream output and call the repaired result dataset gold. Review the
[unit declarations](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/src/module/data_conditioning/normalization.py#L49-L59)
and the actual call graph when preparing a baseline run.

Observed current-engine probes, with default resources:

| Original input | Observed text output | Interpretation |
|---|---|---|
| `값은 1.2300원입니다.` | `값은 일 점 이삼영영 원입니다.` | Decimal places remain explicit. |
| `1~2개입니다.` | `한 개에서 두 개입니다.` | Range endpoints receive native counter readings. |
| `3번째입니다.` | `삼 번째입니다.` | Existing `번` policy reaches this form; a dedicated ordinal/counter policy and reviewed reference are needed. |
| `NASA가 발표했습니다.` | `나사가 발표했습니다.` | Lexicon-selected acronym reading. |
| `기호는 ± & # @ 입니다.` | `기호는 ± & # @ 입니다.` | General symbol verbalization remains incomplete. |
| `설명 (중요한 내용)을 읽습니다.` | `설명 (중요한 내용)을 읽습니다.` | Parenthetical content is preserved. |

The Numeric Testset importer can expose relevant unlabeled inputs, but its
paired utterances do not fill the reference column in any of these families.
An authorized, separately reviewed reference sidecar is required before
reporting a text-accuracy result for that dataset.

A caller may supply a separate local reference JSONL keyed by imported ID and
exact input `text`. Such references are `provided-reference`, which identifies
origin without claiming human review or infallibility. Source and reference
files are hashed separately; stale/unknown IDs and mismatched input text fail.

## Local commands and isolation

All examples require that the operator has already resolved the applicable
rights. They perform no acquisition or rights verification.

```sh
python3 benchmark/importers/ksponspeech/import_ksponspeech.py \
  /path/to/authorized/kspon/transcripts \
  --split eval-clean --encoding cp949 --acknowledge-data-rights \
  --output benchmark/generated/kspon-eval-clean.jsonl

python3 benchmark/importers/coreaspeech/import_coreaspeech.py \
  /path/to/authorized/numeric-text-export.jsonl \
  --split numeric --allow-noncommercial --acknowledge-data-rights \
  --output benchmark/generated/corea-numeric-unlabeled.jsonl

# Optional independent local reference file; inspect IDs from the first import.
python3 benchmark/importers/coreaspeech/import_coreaspeech.py \
  /path/to/authorized/numeric-text-export.jsonl \
  --split numeric --allow-noncommercial --acknowledge-data-rights \
  --references /path/to/local/references.jsonl \
  --output benchmark/generated/corea-numeric-with-references.jsonl

python3 tests/benchmark/test_korean_importers.py
```

Use `--revision` when the local export came from a different known upstream
revision. Kspon has no acquired dataset release pin: the default explicitly
says `local-file-sha256`, with the actual file digest always stored. The Kspon
code revision is format research provenance, not the corpus revision. Corea's
default dataset pins identify the cards inspected; operators must ensure their
exports correspond to the declared pin.

Outputs inside the repository are allowed only below `benchmark/generated/`;
external local paths are also supported. Inputs/references cannot be overwritten.
Failures preserve the previous output rather than publishing a partial import.
Imported text, reference sidecars, errors containing source paths, and resulting
reports remain governed by their source terms. Do not commit them or send them
to external services. These importers do not modify `resources/`, `lexicons/`,
the shipped golden corpus, or the build manifest.

## What is validated and what remains

Synthetic tests cover selection/exclusion, strict parsing and decoding, CP949
source bytes versus UTF-8 output offsets, stable IDs/hashes, two-utterance
separation, generated-field exclusion, explicit opt-ins, reference mapping,
output path guards and atomic error handling. Actual authorized corpora have
**not** been evaluated in this work. Next steps are rights clarification,
operator-supplied local exports, manual TN span/reference review, and only then
separately reported text comparison. Audio synthesis, listening tests and
speaker metrics are outside this benchmark's scope.
