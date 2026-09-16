# Text validation — 0.4.0

Validated on Linux on 2026-09-16 with GCC 13.3, Clang 18.1, C11 and CMake 3.28.
Acceptance uses normalized output text, semantic/language metadata and deterministic
streaming. Speech synthesis, audio comparison and listening scores are excluded.

## Executed checks

| Check | Result |
| --- | --- |
| GCC Debug | 25/25 CTest tests passed |
| Clang Release | 25/25 CTest tests passed |
| Clang ASan + UBSan + leak detection | 25/25 CTest tests passed |
| C golden corpus | 346 explicit input/output cases passed (previously 270) |
| Text regression corpus | 109/109 passed, across 18 categories |
| Finance/content development corpus | 60/60 passed, previously 47/60 including metadata |
| Separate advisory challenge corpus | 3/15 passed, 12 failures explicitly retained |
| Independent financial value checks | 85 generated amounts, 170 value/span/chunk checks plus 3 word-boundary guards |
| Context routing checks | 17 cases, 90 text/language/trace checks |
| Explicit metadata assertions | 75 regression + 33 scenario span checks passed |
| Evaluation streaming | Each case checked with 1-, 7-, and 64-byte writes |
| Text reference errors | 0 character edits across 1,394 reference codepoints |
| New semantic/quotation streaming checks | 1,514 exhaustive two-chunk byte splits, including tiny segment targets |
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
separate challenge set passed only 3/15 and its total character error increased
from 109 to 112 because of ambiguous-title routing. See [scenario evidence and
limitations](domain-corpus.md) for both improvements and remaining errors.

## v0.4 changes against the frozen v0.3 baseline

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

The advisory challenge set remains separate: 12/15 fail, and an ambiguous title
has more character errors under the new noun-phrase policy. Details and exact
outputs are preserved in [scenario validation](domain-corpus.md).

## Retained v0.3 regression evidence

The previous outputs below were captured against v0.2 before the v0.3 fixes.
They remain regression checks in v0.4; they are not additional v0.4 improvements.

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
`tests/evaluation/domain_cases.jsonl` (strict) and `challenge_cases.jsonl` (advisory). `report.html` shows text differences and metadata;
`report.json` contains metrics and hashes; `cases.csv` supports case triage. The CI
`text-evaluation` job publishes this text-only artifact. No speech backend is installed
or called by the test/evaluation workflow.

## Remaining work, in priority order

1. Add finance identifier context (account/reference digit sequences), `%p`/`bp`
   units and decimal ranges, driven by the recorded challenge failures. Collect
   authorized application samples and agree on expected policy first.
2. Improve mixed-language context around unquoted code switches, short ambiguous
   Latin runs, names, homographic Korean counters and technical identifiers.
3. Expand licensed/reviewed pronunciation data beyond the 160-entry seed lexicon;
   replace prototype English G2P and Hangul phone mapping where text error analysis
   shows systematic failures. The dictionary-free fallback guarantees a usable
   string, not the right spelling.
4. Extend scientific notation, combined datetime/time-zone grammar, AM/PM and
   explicit code/path treatment; improve unsupported quantity grammar.
   June/October currently follow the documented spaced-unit `유 월` / `시 월` policy.
5. Profile large workloads/lexicons, index compiled resources and rule candidates,
   and add Windows/macOS plus automated binary ABI compatibility CI.

Native callback and JSONL consumers can begin integrating with a frontend whose
ownership and streaming contracts are tested. Unrestricted production text quality
still requires the work above. [Benchmark methodology](benchmark.md) separately
measures frontend commitment timing; timing is not a text-correctness metric.
