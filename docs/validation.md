# Text validation — 0.3.0

Validated on Linux on 2026-09-16 with GCC 13.3, Clang 18.1, C11 and CMake 3.28.
Acceptance uses normalized output text, semantic/language metadata and deterministic
streaming. Speech synthesis, audio comparison and listening scores are excluded.

## Executed checks

| Check | Result |
| --- | --- |
| GCC Debug | 21/21 CTest tests passed |
| Clang Release | 21/21 CTest tests passed |
| Clang ASan + UBSan + leak detection | 21/21 CTest tests passed |
| C golden corpus | 270 explicit input/output cases passed (previously 73) |
| Text evaluation corpus | 109/109 passed, across 18 categories |
| Explicit metadata assertions | 75 passed |
| Evaluation streaming | Each case checked with 1-, 7-, and 64-byte writes |
| Text reference errors | 0 character edits across 1,394 reference codepoints |
| New semantic/quotation streaming checks | 1,058 exhaustive two-chunk byte splits, including tiny segment targets |
| Existing streaming checks | 717 representative comparisons plus spaced-date splits |
| Generative checks | 200 mixed inputs, 1,000 chunk comparisons, 5,000 scanner probes |
| Shared-engine concurrency | Eight threads, 800 requests |

Golden categories contain KO 96, EN 128, code mixed 12, foreign pronunciation 30,
streaming 2, domain 1 and customer 1 cases. The separate JSONL evaluation corpus
also exercises invalid input shapes, spacing, protected syntax and metadata.
Some cases overlap; these counts are not a count of unique linguistic phenomena.

Additional gates cover source/compiled resource equivalence, transactional resource
loading, precedence, third-pack registration without core changes, CLI JSONL
protocol, early callback delivery and evaluator failure handling. Static and shared
libraries build together with SONAME `libtts_text_frontend.so.1`; ABI v1 public
headers are unchanged. No compiler warnings or sanitizer errors were observed.

These are regression results for explicit, curated references, not an estimate of
accuracy on unseen production text. The corpus is small, purpose-built and lacks
independent held-out domain samples. Passing it is not production text-quality
approval.

## Concrete defects corrected

The previous outputs below were captured before the changes, rather than inferred
from the new implementation.

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

The report directory must be new. `report.html` shows text differences and metadata;
`report.json` contains metrics and hashes; `cases.csv` supports case triage. The CI
`text-evaluation` job publishes this text-only artifact. No speech backend is installed
or called by the test/evaluation workflow.

## Remaining work, in priority order

1. Collect independently annotated application/domain text and add held-out cases.
   Review disagreements in output-text policy before adding rules.
2. Improve mixed-language context around unquoted code switches, short ambiguous
   Latin runs, names, homographic Korean counters and technical identifiers.
3. Expand licensed/reviewed pronunciation data beyond the 155-entry seed lexicon;
   replace prototype English G2P and Hangul phone mapping where text error analysis
   shows systematic failures. The dictionary-free fallback guarantees a usable
   string, not the right spelling.
4. Complete composite Korean amounts (`1억2천3백만원`), scientific notation,
   decimal ranges, combined datetime/time-zone grammar and optional text policies.
   June/October currently follow the documented spaced-unit `유 월` / `시 월` policy.
5. Profile large workloads/lexicons, index compiled resources and rule candidates,
   and add Windows/macOS plus automated binary ABI compatibility CI.

Native callback and JSONL consumers can begin integrating with a frontend whose
ownership and streaming contracts are tested. Unrestricted production text quality
still requires the work above. [Benchmark methodology](benchmark.md) separately
measures frontend commitment timing; timing is not a text-correctness metric.
