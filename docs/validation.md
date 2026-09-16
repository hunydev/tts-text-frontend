# Text validation — 0.7.0

Validated on Linux on 2026-09-16 with GCC 13.3, Clang 18.1, C11 and CMake 3.28.
Acceptance uses normalized output text, semantic/language metadata and deterministic
streaming. Speech synthesis, audio comparison and listening scores are excluded.

## Executed checks

| Check | Result |
| --- | --- |
| GCC Debug | 39/39 CTest tests passed |
| Clang Release | 39/39 CTest tests passed |
| Clang ASan + UBSan + leak detection | 38/38 CTest tests passed |
| C golden corpus | 346 explicit input/output cases passed |
| Text regression corpus | 109/109 passed, across 18 categories |
| Finance/content development corpus | 60/60 passed, previously 47/60 including metadata |
| Advanced finance/content corpus | 30/30 passed; frozen v0.4 baseline 8/30 |
| Protected text development corpus | 40/40 passed; frozen v0.5 baseline 4/40 including metadata |
| Repeated quantity interval corpus | 24/24; frozen v0.6 baseline 0/24 |
| Content title/vocabulary corpus | 40/40; frozen v0.6 baseline 3/40 including metadata |
| New interval/content checks | 223 quantity/IR/resource checks + 312 content/stream checks |
| Web workbench | 37 API/ABI/policy/corpus/early-callback checks; Chromium desktop/mobile flow checks |
| Historical challenge corpus | 15/15 fixed known references passed |
| Protected grammar/stream checks | Native parser, flush/cap/cache checks; 316 integration checks |
| Financial quantity checks | 516 exact-value/unit/endpoint/chunk checks, including 50 Decimal reference amounts |
| Explicit financial identifier checks | 10 positive + 8 negative cases; 95 text/metadata/chunk/override checks |
| Content cue/lexicon checks | 88 checks: title scope, overrides, hyphenated words and bounded long-title commitment |
| Independent financial value checks | 85 generated amounts, 170 value/span/chunk checks plus 3 word-boundary guards |
| Context routing checks | 17 cases, 90 text/language/trace checks |
| Explicit metadata assertions | 75 regression + 33 scenario + 20 advanced + 45 protected + 24 interval + 59 content span checks passed |
| Evaluation streaming | Each case checked with 1-, 7-, and 64-byte writes |
| Regression text reference errors | 0 character edits across 1,394 reference codepoints |
| New semantic/quotation streaming checks | 4,769 exhaustive two-chunk byte splits, including tiny segment targets |
| Existing streaming checks | 717 representative comparisons plus spaced-date splits |
| Generative checks | 200 mixed inputs, 1,000 chunk comparisons, 5,000 scanner probes |
| Shared-engine concurrency | Eight threads, 800 requests |

Golden categories contain KO 147, EN 128, code mixed 37, foreign pronunciation 30,
streaming 2, domain 1 and customer 1 cases. The separate JSONL evaluation corpus
also exercises invalid input shapes, spacing, protected syntax and metadata.
Some cases overlap; these counts are not a count of unique linguistic phenomena.

Additional gates cover source/compiled resource equivalence, transactional resource
loading, precedence, third-pack registration without core changes, CLI JSONL
protocol, early callback delivery and evaluator failure handling. Static and shared
libraries build together with SONAME `libtts_text_frontend.so.1`; ABI v1 public
headers are unchanged. No compiler warnings or sanitizer errors were observed.

These are regression results for explicit, curated references, not an estimate of
accuracy on unseen production text. All new scenario data is original synthetic
text, not customer traffic or independently annotated human references. The
unchanged challenge file now passes 15/15, compared with 13/15 in v0.5. All fifteen
references have been adopted into development regressions, so it is no longer an
unseen holdout. The implementation remains a bounded lexical frontend, not a
general document parser. See [protected text contracts](protected-text.md); no
speech synthesis was used.

## v0.7 changes against the frozen v0.6 baseline

The fixed 24-case repeated-quantity interval corpus improved from 0/24 to 24/24.
Forty fixed title/pronunciation references improved from 3/40 to 40/40 including
metadata (text-only: 5/40 to 40/40). New vocabulary entries are explicit project
seed choices; they do not solve unknown-word G2P or establish reviewed pronunciation
accuracy. See [versioned evidence](evidence/0.7.0-text-results.json),
[interval contracts](finance-ranges.md), and [content scope](content-v7.md).

The web workbench calls the native ABI, copies callback data within its lifetime,
and streams actual events to the browser. Its HTTP/ctypes gate runs in ordinary
GCC/Clang builds. It is excluded from sanitizer builds because the Python host
has no injected sanitizer runtime; native sanitizer gates remain enabled.
Browser checks exercise normalization, metadata, overrides, streaming, parity,
downloads, cancellation, inert HTML text, corpus results and responsive layout.
See [workbench](web-workbench.md).

Tiny phrase targets can lose surrounding language evidence. The English financial
range exhaustive test uses a quoted context window; the corresponding unquoted
English corpus case uses its specified default language. Content tests enforce
chunk parity at tiny targets without claiming that different segmentation
configurations have identical readings.

## Historical v0.6 changes against the frozen v0.5 baseline

The v0.5 executable and resources at commit `ff5f847` were copied before changes.
Forty synthetic protected-text references were fixed before either baseline or
candidate execution. Complete text/metadata/parity acceptance improved 4/40 to
40/40; text-only matches improved 8/40 to 40/40. The two historical challenge
failures now pass without changing their reference strings.

Checks cover unchanged literal bytes, neighboring TN, neutral language metadata,
customer policies, rule isolation, invalid UTF-8, every representative byte split,
pre-EOF callbacks and pending limits. A third-party pack that exceeds its
classification view is rejected. Long-literal tests verify caching does not defer
a completed span's callback or hide UTF-8 errors. See [protected text](protected-text.md).

## Historical v0.5 changes against the frozen v0.4 baseline

The v0.4 executable and resources at commit `493f966` were copied before changes.
The new advanced reference file was written before its first baseline/candidate
execution and kept unchanged. Eight of 30 baseline cases passed; all 30 candidate
cases pass, including the 20 explicit span assertions. The original 60-case corpus
and 109-case core corpus continue to pass. Known challenge cases improved from
3/15 to 13/15 without changing their reference strings.

New tests independently check scientific values with Python Decimal, semantic
range endpoints and units, zero-preserving identifier values, cue resource layers,
AM/PM markers, tiny segmentation targets and malformed objects. Direct lexicon
entries fix three observed foreign-word spellings; this is vocabulary coverage,
not evidence that unknown-word G2P was solved. Details: [financial contracts](financial-text.md).

## Historical v0.4 changes against the frozen v0.3 baseline

The 60-case scenario file was unchanged throughout comparison. Thirteen cases
changed from failing to passing when text, metadata and parity are combined;
none regressed in that development set. Exact string matches increased from
52 to 60. Compound amounts now carry one verified currency value even where
several old spans happened to concatenate into the expected string.

An independent generator verifies 85 amounts with Python integer/Decimal
arithmetic, checks their canonical values and complete input spans, then compares
rendering against the equivalent flat amount. One- and 64-byte writes with tiny
segment targets exercise 170 checks. This validates numerical meaning, rather
than relying on output spelling alone.

At the v0.4 checkpoint, 12/15 challenge cases failed and an ambiguous title had
more character errors under the noun-phrase policy. Historical outputs remain in
[scenario validation](domain-corpus.md) and the versioned evidence JSON.

## Retained v0.3 regression evidence

The previous outputs below were captured against v0.2 before the v0.3 fixes.
They remain regression checks in v0.6; they are not additional v0.6 improvements.

| Input | Previous text | Current text |
| --- | --- | --- |
| `3 kg` | `three kg` | `삼 킬로그램` |
| `3 개` | `삼 개` | `세 개` |
| `3.5만원입니다.` | `삼 점 오만원입니다.` | `삼만오천 원입니다.` |
| `14:30:05` | `열네 시 삼십 분:오` | `열네 시 삼십 분 오 초` |
| `1/2` | `일/이` | `이 분의 일` |
| `11st` | `eleventh` | `11st` (invalid ordinal preserved) |
| `12,34` | `십이,삼십사` | `12,34` (invalid grouping preserved) |
| `10kmh` | `ten kilometersh` | `10kmh` (unsupported identifier preserved) |
| `21st는 스물한 번째입니다.` | `트우에느트이 피르스트는 스물한 번째입니다.` | `트웬티 퍼스트는 스물한 번째입니다.` |
| `C++를 사용합니다.` | `크++를 사용합니다.` | `씨플러스플러스를 사용합니다.` |
| `그는 "Please go to the 1st floor"라고 말했다.` | `그는 "Please go to the first 프루"라고 말했다.` | `그는 "Please go to the first floor"라고 말했다.` |

Conservative preservation is a defined policy for unsupported structured text;
it does not supply a complete spoken rendering for that structure.

## Required examples

```text
Input:
1st는 첫 번째라는 뜻입니다.
영어로는 다음과 같은 예문이 있습니다.
Please go to the 1st floor

Output:
퍼스트는 첫 번째라는 뜻입니다.
영어로는 다음과 같은 예문이 있습니다.
Please go to the first floor
```

| Input | Normalized output |
| --- | --- |
| Apple은 사과입니다. | 애플은 사과입니다. |
| Apple released the iPhone. | Apple released the iPhone. |
| GPU를 사용합니다. | 지피유를 사용합니다. |
| WiFi를 사용합니다. | 와이파이를 사용합니다. |
| OpenAI가 발표했습니다. | 오픈에이아이가 발표했습니다. |
| foobar는 예시입니다. | 푸바는 예시입니다. |
| 123-456, system | 백이십삼 다시 사백오십육 |
| 123-456, finance | 일이삼 사오육 |
| 123-456, customer | 일이삼 번에 사오육 |

## Reproduction

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 tools/evaluation/evaluate.py --ttf build/ttf --resources resources \
  --corpus tests/evaluation/cases.jsonl --output-dir artifacts/text-evaluation
```

The report directory must be new. Additional CI reports use
`tests/evaluation/domain_cases.jsonl`, `advanced_cases.jsonl` and
`protected_cases.jsonl` (strict), plus
`challenge_cases.jsonl` (advisory). `report.html` shows text differences and metadata;
`report.json` contains metrics and hashes; `cases.csv` supports case triage. The CI
`text-evaluation` job publishes this text-only artifact. No speech backend is installed
or called by the test/evaluation workflow.

## Remaining work, in priority order

1. Collect authorized finance/content samples with agreed readings. Decide document
   ingestion policies beyond literal preservation (entities, markup removal, Unicode
   paths and quoted source versus speech content).
2. Extend locale-specific quantities, currency intervals, datetime/time zones and
   ambiguity around unquoted titles, names, short imperatives and identifiers.
3. Expand reviewed pronunciation data beyond the 203-entry seed lexicon and
   improve prototype G2P/phone mapping. Nonempty fallback is not correct spelling.
4. Profile large lexicons and worst-case streaming inputs; add a real third language
   pack, Windows/macOS and automated binary ABI compatibility CI.

Native callback and JSONL consumers can begin integrating with a frontend whose
ownership and streaming contracts are tested. Unrestricted production text quality
still requires the work above. [Benchmark methodology](benchmark.md) separately
measures frontend commitment timing; timing is not a text-correctness metric.
