# TTS Text Frontend

Deterministic, resource-driven, multilingual and code-mixed text frontend for TTS.

C11 native library for text normalization, context routing, foreign pronunciation,
span metadata and incremental TTS callbacks. The initial packs are **ko-KR** and
**en-US**. This is a working initial implementation; pronunciation quality and
language detection are intentionally small, deterministic prototypes.

```text
1st는 첫 번째라는 뜻입니다.
영어로는 다음과 같은 예문이 있습니다.
Please go to the 1st floor
↓
퍼스트는 첫 번째라는 뜻입니다.
영어로는 다음과 같은 예문이 있습니다.
Please go to the first floor

Apple은 사과입니다.          → 애플은 사과입니다.
Apple released the iPhone.  → Apple released the iPhone.
GPU를 사용합니다.            → 지피유를 사용합니다.
WiFi를 사용합니다.           → 와이파이를 사용합니다.
```

`Apple → 애플` is pronunciation rendering. Source, context and render languages
remain distinct in the semantic IR and output metadata.

## Continue quality work with another model

Start with [HANDOFF](docs/HANDOFF.md) and the [TN improvement playbook](docs/tn-improvement-playbook.ko.md).
`python3 tools/quality/tn_cycle.py --help` provides case intake, immutable artifact
snapshots and same-reference before/after evaluation without an LLM dependency.
The [measured work queue](benchmark/manifests/quality-work-queue.json) prioritizes
finance value errors and recurring finance/content input patterns. Work on one
rule family at a time; pending human review and operating limits remain explicit.

## Explicit readings and final speech text (v0.20)

The web workbench defaults to **speech text**, with unknown Han characters and
emoji **preserved**. Choose **제거** to drop unregistered characters. This is still
text output; no speech synthesis or audio evaluation is involved. Legacy C engine
creation and CLI commands retain `text` mode for compatibility; use
`--output-mode speech` for electronic-address spelling and the final character pass.

```sh
build/ttf normalize --output-mode speech 'k7 타고 다닌다'
# 케이칠 타고 다닌다
build/ttf normalize --output-mode speech '오늘의 top100'
# 오늘의 탑백
build/ttf normalize --output-mode speech '1+1은 귀요미'
# 일 더하기 일은 귀요미
build/ttf normalize --output-mode speech '이메일 abc@example.com'
# 이메일 에이비씨 앳 이그잼플 닷 컴
build/ttf normalize --output-mode speech --unreadable drop '가漢🙂나'
# 가 나
build/ttf normalize --input-format annotated --output-mode speech '1{첫}번째 1{@native} 1{@sino} ABC{@alphabet}마트'
# 첫번째 하나 일 에이비씨마트
```

Annotated input is explicit: plain input never interprets reading controls.
Literal aliases take precedence over lexicons, and grouped aliases such as
`[[1+1]]{원플러스원}` can cover a phrase or expression. Unknown/malformed patterns
fail with an error. Streaming retains incomplete controls until they can be
validated. `inspect --trace` records override and final preserve/drop decisions.
Protected code, literal aliases and intentionally ambiguous semantic objects
remain explicit preservation contracts, including in drop mode.

The workbench includes a **읽기 제어·기호·이메일** corpus with 51 independently
written development references, plus direct controls and arbitrary-byte streaming
tests. These references still require independent linguistic review.
See the [input grammar](docs/input-format.md),
[editable speech resources and reading policies](docs/speech-resources.md),
[API options](docs/api.md#frontend-input-and-output-options), and
[verification scope](docs/research/speech-controls-v20-results.md).

## Named dates and contextual years (v0.13)

The English pack now recognizes month-first and day-first named dates, partial
month/day and year/month values, and resource-cued years. Calendar validation
rejects impossible dates; missing fields remain absent in Semantic IR.

```text
September 17, 2026 → September seventeenth twenty twenty six
17 September 2026 → the seventeenth of September twenty twenty six
September 2026 → September twenty twenty six
in 1905 → in nineteen oh five
in 2026 copies → in two thousand twenty six copies
마감은 September 17, 2026입니다.
→ 마감은 이천이십육 년 구 월 십칠 일입니다.
```

Bare numbers remain cardinals. Existing ISO date readings keep their previous
policy. The web workbench's **영어 날짜·연도·코드믹스** corpus contains 20 original
text/metadata contracts. See [date resources and limits](docs/english-dates.md)
and the [measured iteration](docs/research/english-dates-v13-results.md).
References still require independent human review.

## Exact scaled currency amounts (v0.12)

Currency scales, explicit currency identities and English minor-unit policies
now have dedicated resources. Amounts are expanded exactly in Semantic IR;
English can retain a named scale while Korean reads its expanded numeric value.

```text
$2.75million                  → two point seven five million dollars
예산은 $2.75million입니다.       → 예산은 이백칠십오만 달러입니다.
A$3M                          → three million Australian dollars
1.25JPY                       → one point two five yen
```

See [scaled currency contracts](docs/scaled-currency.md) and
[currency resource editing and sources](docs/currency-resources.md). In the web
workbench, select **금융 금액 배수·통화·범위** (`scaled-currency-v12`) to run the
[original text and metadata corpus](tests/evaluation/scaled_currency_v12.jsonl).
References remain pending independent review. This extends supported reading
contracts; it does not establish general financial-text accuracy.
The [same-corpus measurement](docs/research/scaled-currency-v12-results.md)
records the improvement, regression checks and remaining coverage gaps.

## External benchmark pipeline (v0.11)

The [external benchmark guide](benchmark/README.md) adds Sproat sentence/span/class
evaluation, selected Kspon dual-transcription candidates, optional local Corea
Numeric inputs, and a pinned CMUdict importer that preserves stress and alternative
pronunciations. Dataset references, unreviewed candidates and unlabeled inputs are
kept separate. Comparisons require the same corpus and evaluation policy; reports
record engine/resource hashes and class-level fixes and regressions.

Read the [research and measured coverage gaps](docs/research/external-benchmark-results.md)
and [artifact-specific licenses](docs/THIRD_PARTY.md). External data and generated
reports stay outside Git and product resources. No audio evaluation is performed.
CI uses original offline fixtures, not downloaded datasets.

The first measured improvements add resource-defined speed, length, storage,
power and area units, `US$` and `€`. Currency prefix/suffix conflicts are preserved
for review instead of producing contradictory readings. This remains an early
frontend; external reference agreement does not establish production accuracy.

```sh
ctest --test-dir build --output-on-failure -R '^external_'
build/ttf normalize --language en-US 'The speed is 28 mph.'
# The speed is twenty eight miles per hour.
build/ttf normalize '금액은 US$15달러입니다.'
# 금액은 십오 달러입니다.
```

## Managed pronunciation dictionaries (v0.10)

The editable source now lives in `lexicons/`: **379 entries / 381 keys**, including
159 new finance and content entries. Each entry records sources, alternatives,
scope and review status. Current status is 85 source-checked, 294 pending, and
zero independently human-reviewed. Dictionary coverage is not pronunciation accuracy.

The web **사전 관리** page at `/lexicon` supports search, evidence inspection, native
preview of edited pronunciations, customer TSV/review proposal downloads, and
manuscript reports identifying fallback vocabulary. See the
[Korean dictionary maintenance guide](docs/lexicon-maintenance.ko.md).

```sh
python3 tools/lexicon/manage.py validate
python3 tools/lexicon/manage.py build --check
build/ttf normalize --domain resources/domains/finance/ko-KR/lexicon.tsv \
  'ETF의 수수료와 NAV를 확인하세요.'
# 이티에프의 수수료와 엔에이브이를 확인하세요.
build/ttf normalize 'Premiere Pro에서 After Effects로 이동합니다.'
# 프리미어 프로에서 애프터 이펙트로 이동합니다.
python3 tools/lexicon/coverage.py input.txt --finance --output coverage.json
```

Forty-six ambiguous financial initialisms are an explicit domain overlay; this
vocabulary file does not enable the legacy finance numeric policy. Source compilation
validates canonical collisions, evidence, review fields, runtime generation and
domain isolation. Native TN rule lookup now uses a frozen rule-only index:
[50,000-key measurements and remaining storage limits](docs/lexicon-scale-v10.md).

## Range readings and reference review (v0.9)

```text
1~2          → 일에서 이
1시~2시      → 한 시에서 두 시
1~2개        → 한 개에서 두 개
1~2만 원     → 만 원에서 이만 원
1.5%~2.5%    → 일 점 오 퍼센트에서 이 점 오 퍼센트
~            → 물결
```

Range meanings are distinct from separator symbols and identifiers. Both range
endpoints keep their units; counter resources choose native or Sino-Korean numbers.
See the [Korean resource editing guide](docs/resource-editing-guide.ko.md) to change
`에서` to `부터 … 까지`, unit labels, or pronunciation lexicons without rebuilding C.

The v0.9 audit corrects erroneous range, month and midnight references as well as
malformed example particles. Corpus passes mean **conformance to expected text**;
they do not certify linguistic correctness. All fixtures carry review classifications;
independent human approval has not been claimed. See [reference changes](docs/audit/reference-corrections-0.9.0.json),
[audit reports](docs/audit/numeric-reference-audit.md), and [validation](docs/validation.md).

## Web testing workbench

`tools/web/` provides a Korean web UI backed by the actual C shared library:
normalization, source/context/render inspection, live sentence/phrase callbacks,
chunk parity checks, editable user lexicons/customer policies, and corpus reports.
It evaluates text and metadata only; no speech synthesis is performed.

```sh
cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-web --parallel 2
python3 tools/web/server.py --host 0.0.0.0 --port 8000 \
  --library build-web/libtts_text_frontend.so --resources resources
```

Open port **8000**. See the [web workbench guide](docs/web-workbench.md) for controls,
report export, deployment, limits and automated browser tests. Python standard
library only at runtime; no npm build or external CDN is needed.


## Build and test

Linux, CMake 3.16+, and a C11 compiler are required. There are no runtime package
dependencies. Python 3 is optional for the resource compiler and its tests.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

cmake -S . -B build-sanitize -DCMAKE_C_COMPILER=clang \
  -DTTF_ENABLE_ASAN=ON -DTTF_ENABLE_UBSAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

Produces `libtts_text_frontend.so`, `libtts_text_frontend.a`, `ttf`, and
`ttf_example`, and `ttf_bench`. Both library variants are built in the same invocation. Public
headers use opaque engine and stream handles, `ttf_` symbols, ABI versions and
structure sizes. Existing ABI v1 signatures/layouts are frozen; incompatible
changes require a new major ABI/SOVERSION. Linux is verified; portable C and Windows export annotations
prepare the project for macOS/Windows validation.

```sh
cmake --install build --prefix "$PWD/install"
install/bin/ttf normalize --resources "$PWD/install/share/tts-text-frontend/resources" \
  "Apple은 사과입니다."
```

The development default resource path points at the source tree. For packaged
installations set `TTF_DEFAULT_RESOURCE_ROOT` at configure time or pass
`config.resource_root` / `--resources`; explicit paths support relocation.

## CLI

```sh
build/ttf normalize '1st는 첫 번째라는 뜻입니다.
영어로는 다음과 같은 예문이 있습니다.
Please go to the 1st floor'
build/ttf normalize 'Apple은 사과입니다.'
build/ttf normalize 'GPU를 사용합니다.'
build/ttf normalize 'WiFi를 사용합니다.'
build/ttf normalize --language en-US 'Please go to the 1st floor.'
build/ttf inspect 'OpenAI가 발표했습니다.'
build/ttf stream examples/code_mixed.txt
build/ttf stream-json --chunk-bytes 64 examples/code_mixed.txt
```

`inspect` includes surface text, script, source/context/render language, semantic
class and value, matched rule, resource layer, lexicon provenance, verbalizer,
per-pack context scores, input/output byte ranges and final text. `stream -` reads standard input.

## C API

```c
#include <stdio.h>
#include <string.h>
#include <tts_text_frontend/ttf.h>

int main(void) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_result_t result = {0};
    const char *input = "Apple은 사과입니다.";
    ttf_status_t status;
    ttf_config_init(&config);
    status = ttf_engine_create(&config, &engine);
    if (status == TTF_OK) status = ttf_engine_freeze(engine);
    if (status == TTF_OK)
        status = ttf_normalize(engine, input, strlen(input), &result);
    if (status == TTF_OK) puts(result.text);
    ttf_result_free(&result);
    ttf_engine_destroy(engine);
    return status == TTF_OK ? 0 : 1;
}
```

Initialize configuration with `ttf_config_init`. Register packs and load overrides
before `ttf_engine_freeze`. After freezing, share an engine among requests and
create a separate stream for each request. Destroy all streams before their engine.
See [API ownership and lifecycle](docs/api.md) and [the complete example](examples/basic.c).

## Resource customization

UTF-8 TSV resources separate normalization rules from pronunciation dictionaries.
The root `manifest.ttfm` lists system files in declaration order. Load explicit
files or compiled resources into a domain/customer layer before freezing:

```c
ttf_resource_load(engine, "resources/domains/finance/ko-KR/rules.tsv", TTF_LAYER_DOMAIN);
ttf_resource_load(engine, "resources/customers/example/ko-KR/rules.tsv", TTF_LAYER_CUSTOMER);
```

The same `123-456` input has resource-selected behavior:

| Resources | Output |
| --- | --- |
| System | 백이십삼에서 사백오십육 |
| Finance domain | 일이삼 사오육 |
| Example customer | 일이삼 번에 사오육 |

```sh
build/ttf normalize --domain resources/domains/finance/ko-KR/rules.tsv '123-456'
build/ttf normalize --domain resources/domains/finance/ko-KR/rules.tsv \
  --customer resources/customers/example/ko-KR/rules.tsv '123-456'
python3 tools/resource_compiler/compile_resources.py \
  resources/domains/finance/ko-KR/rules.tsv /tmp/finance.ttfb
build/ttf normalize --domain /tmp/finance.ttfb '123-456'
```

See [resources and priority](docs/resources.md) for the DSL and compiler boundary.

## Lexicon customization

```c
ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "Apple", "사용자애플");
ttf_lexicon_remove(engine, TTF_LAYER_USER, "en-US", "ko-KR", "Apple");
```

Pronunciation priority is **User > Customer > Domain > System**, followed by the
source pronunciation dictionary, source G2P, target phone renderer, and emergency
grapheme renderer. Removing an override reveals the next layer. Unknown Latin
words in Korean context produce speakable Hangul; this fallback is approximate.
See [lexicon](docs/lexicon.md) and [pronunciation](docs/pronunciation.md).

## Streaming

```c
static int on_segment(const ttf_segment_t *segment, void *user_data) {
    /* Send or copy text and metadata to your TTS queue here. */
    return fwrite(segment->text, 1, segment->length, (FILE *)user_data)
           != segment->length;
}
/* engine has already been frozen */
ttf_stream_t *stream = NULL;
ttf_stream_create(engine, on_segment, stdout, &stream);
ttf_stream_write(stream, "가격은 12", strlen("가격은 12"));
ttf_stream_write(stream, ",500원입니다.", strlen(",500원입니다."));
ttf_stream_flush(stream);
ttf_stream_destroy(stream);
```

Check each status in application code. Writes may end inside a UTF-8 character or
numeric expression. Pending text waits for a stable boundary; `flush` finalizes
it. The same planner is used for batch and streaming so concatenated callback text
matches batch output for the same configuration and resources. Targets for first
and subsequent segments, maximum length, sentence/phrase splitting and pending
buffer limit are configurable. Callback pointers live only during the callback.
See [streaming invariants and latency tradeoffs](docs/streaming.md).

## Output text evaluation

Acceptance is based on normalized output text, semantic/language metadata and
batch/stream equivalence. No speech synthesis or listening comparison is required.
The text report shows independent expected strings, character differences,
category scores, resource provenance and fallback paths. The current regression
suite has 355 golden cases, a 109-case regression corpus, 60 finance/content
scenarios, 30 advanced finance/content regressions and 40 literal-preservation
references, plus 24 repeated-quantity range, 40 content-vocabulary and 30 multiword
pronunciation cases. These are curated regression checks, not a measurement of accuracy on
unseen text. A separate 15-case advisory challenge report exposes unsupported
forms and ambiguous policies. All 15 now pass their fixed references and have
been adopted into strict development suites; the score is not an unseen-data
result or a general accuracy estimate.

```sh
python3 tools/evaluation/evaluate.py --ttf build/ttf --resources resources \
  --corpus tests/evaluation/cases.jsonl --output-dir artifacts/text-evaluation
python3 tools/evaluation/evaluate.py --ttf build/ttf --resources resources \
  --corpus tests/evaluation/domain_cases_v8.jsonl --output-dir artifacts/domain-evaluation
python3 tools/evaluation/evaluate.py --ttf build/ttf --resources resources \
  --corpus tests/evaluation/advanced_cases.jsonl --output-dir artifacts/advanced-evaluation
build/ttf stream-json examples/code_mixed.txt > segments.jsonl
build/ttf_bench --iterations 100 --chunk-bytes 64 examples/code_mixed.txt
```

The current priority is finance, with content narration and general-purpose
multilingual text retained as reference domains. Compound amounts such as
`1억2천3백만원` yield one `CURRENCY` span with value `123000000`, then render as
`일억이천삼백만 원`. See [scenario evidence](docs/domain-corpus.md) and
[context routing](docs/context-routing.md) for measured changes and remaining gaps.

The v0.5 finance and content additions include:

| Input | Normalized text |
| --- | --- |
| `계좌는 110-123-456789입니다.` | `계좌는 일일공 일이삼 사오육칠팔구입니다.` |
| `금리는 0.25%p 상승했습니다.` | `금리는 영 점 이오 퍼센트포인트 상승했습니다.` |
| `예상 수익률은 1.5-2.5%입니다.` | `예상 수익률은 일 점 오 퍼센트에서 이 점 오 퍼센트입니다.` |
| `표시 금액은 1.2e6원입니다.` | `표시 금액은 백이십만 원입니다.` |
| `제목은 The 1st day입니다.` | `제목은 The first day입니다.` |
| `새로운 real-time dashboard를 소개합니다.` | `새로운 리얼타임 대시보드를 소개합니다.` |

Identifiers require explicit resource labels; the engine does not validate bank
accounts. `%`, `%p`, and `bp` have distinct meanings and values are never silently
converted between them. See [financial text contracts](docs/financial-text.md).

### Literal code, markup and paths

```text
<code>GPU 3개</code> 예제는 3개입니다.
→ <code>GPU 3개</code> 예제는 세 개입니다.

설정 파일 /etc/app/config-v2.json을 확인합니다.
→ 설정 파일 /etc/app/config-v2.json을 확인합니다.
```

Selected opaque elements, backtick code and explicit paths remain byte-identical.
Ordinary tag bodies still normalize. Protected spans carry `TEXT`, a `code` /
`markup` / `path` unit, neutral language tags and inspectable rule/layer metadata.
Common resource policies can be overridden per customer. This is literal
preservation, with no tag stripping or HTML/Markdown interpretation. See
[protected text contracts](docs/protected-text.md) for syntax, limits and examples.

Open `artifacts/text-evaluation/report.html` to inspect text failures and metadata.
Output directories must be new on each run. CI publishes the same text report.
The benchmark measures frontend first-callback latency and throughput only.

See [JSONL protocol](docs/stream-protocol.md), [text evaluation](docs/evaluation.md),
and [benchmark definitions](docs/benchmark.md).

## Architecture and scope

[Validation results](docs/validation.md) records the executed checks and required outputs.
[Architecture](docs/architecture.md) describes the six milestones and decisions.
[Language pack contract](docs/language-pack.md) explains extension without core
language branches. [Semantic IR](docs/semantic-ir.md) describes shared types.
[Korean coverage](docs/ko-coverage.md) and [English coverage](docs/en-coverage.md)
record tested readings and linguistic limits.

Current production work remains: broader context heuristics and corpus, full
Unicode properties and numeric grammars, larger licensed pronunciation resources,
higher quality G2P/phonetic mapping, long-input performance profiling, automated ABI compatibility
checks, and Windows/macOS CI. Integration with a real TTS engine can begin using
the callback and span APIs; deployment requires text-corpus acceptance for the target domain. The library does
not synthesize audio.

Private proprietary project. See [LICENSE](LICENSE).

### v0.7 finance and content extensions

- `1만원~2만원` → `만 원에서 이만 원`
- `$10-$20` in English context → `ten to twenty dollars`
- `1kg~2km` remains unchanged because its units differ.
- Eight explicit title cues and 40 original seed pronunciation entries are loaded
  as resources; customer and user pronunciations retain priority.

See [financial interval contracts](docs/finance-ranges.md) and
[content vocabulary scope](docs/content-v7.md). These are bounded syntax and seed
vocabulary improvements, not a general pronunciation or production accuracy claim.

### v0.8 multiword names and streaming experiments

```text
Bank of America가 발표했습니다.
→ 뱅크 오브 아메리카가 발표했습니다.

After Effects를 사용합니다.
→ 애프터 이펙트를 사용합니다.

Adobe Premiere Pro를 사용합니다.
→ 어도비 프리미어 프로를 사용합니다.
```

Multiword pronunciation entries use the existing lexicon API and layered resources.
A higher-priority user/customer entry wins before the longest-match comparison.
Matched phrases retain one source/context/render span. English context and title
cues preserve English; literal code and paths remain protected.

The [web workbench](docs/web-workbench.md) adds exact write-size schedules, splitting
at the input cursor, input-byte history, and a 30-case multiword corpus. Try
`After |Effects를 사용합니다.` to inspect deferred rendering across writes.
See [multiword contracts](docs/multiword-lexicon.md) for lexical bounds and exceptions.

### v0.14 written currency names, abbreviations and pronunciation overrides

| Input | Normalized text |
| --- | --- |
| `The budget is 2.5 million dollars.` | `The budget is two point five million dollars.` |
| `제작비는 2.5 million dollars입니다.` | `제작비는 이백오십만 달러입니다.` |
| `Dr. Rowan spoke.` | `Doctor Rowan spoke.` |
| `The U.S. market opened.` | `The u s market opened.` |
| `U.S.를 확인합니다.` | `유에스를 확인합니다.` |

Written money retains exact value and currency identity; ambiguous bare `pounds`
is not automatically assigned GBP. Dotted initials have an editable text policy,
while ordinary uppercase words retain their existing default. Original-surface
pronunciation overrides survive ordinal/title/initial transformations, with the
same User > Customer > Domain > System order across aliases. Generic word
pronunciations cannot override initialism letter reading without an explicit
initialism pronunciation entry.

The port-8000 workbench includes **영어 통화명·약어·이니셜** (24 original text/IR
contracts). See [abbreviation and alias editing](docs/english-forms.md),
[written currency editing](docs/written-currencies.md),
[measured changes](docs/research/english-forms-v14-results.md), and the
[evidence-based remaining-work checklist](docs/remaining-work.md).

The checklist preserves the full project objective: unclosed items are not treated
as complete because a selected corpus passes. Current references and four new
Korean title pronunciations remain pending independent review. Audio synthesis
and audio comparison are outside the evaluation scope.

### v0.15 Unicode numeric grouping, Korean counters and source trace

| Input | Normalized text |
|---|---|
| `1 250원` (U+202F separator) | `천이백오십 원` |
| `1 250~2 500원` | `천이백오십 원에서 이천오백 원` |
| `1 25원` (invalid grouping) | `1 25원` |
| `1번째 / 11번째 / 20번째` | `첫 번째 / 열한 번째 / 스무 번째` |
| `총 3번` | `총 세 번` |
| `3번 출구` | `삼 번 출구` |
| `제1,001호` | `제천일 호` |

The same exact decimal parser now handles comma, NBSP, narrow NBSP and thin-space
integer grouping without changing source byte offsets. ASCII whitespace still
separates independent numbers. Malformed groups and unsupported ordinal forms
remain whole text. Explicit positive Korean numbers retain `플러스`.

Korean ordinals, count/identifier roles, attached particles and ranges have separate
IR contracts and editable resource policies. The web workbench includes a 28-case
counter corpus and Unicode amount samples. Pronunciation dictionary/G2P source
layers are reported through ABI-compatible provenance; target mapper row/layer
trace is available through the additive v0.17 API described below.

See [Unicode numbers](docs/unicode-numbers.md), [Korean counters and customization](docs/korean-counters.md),
[pronunciation trace](docs/pronunciation-trace.md), and [v0.15 measurements](docs/research/numeric-counters-v15-results.md).

### v0.16 resource policy validation

C runtime, CLI, compiler and web now share setup-time policy checks. Invalid enums,
weights, phone indices, date bounds and missing effective reading dependencies are
rejected before serving requests. Failed file loads preserve the existing engine.

```sh
build/ttf validate-resource --resources resources --require-complete-validation \
  resources/customers/example/ko-KR/rules.tsv
python3 tools/resource_compiler/compile_resources.py \
  resources/customers/example/ko-KR/rules.tsv /tmp/customer.ttfb \
  --ttf build/ttf --resources resources --require-complete-validation
```

Diagnostics include file, physical line and resource key. The web customer TSV editor
shows the same errors; successful output reports validated and unhandled row counts.
New language packs can register validators through an additive API while retaining
ABI v1. Unknown namespaces remain loadable and explicitly counted; strict checking
is available for deployment gates. Compiler validation accepts the deployment's
ordered domain/customer overlays and language setting.

See the [Korean editing and validation guide](docs/resource-validation.md) and
[verification results](docs/research/resource-validation-v16-results.md).
These checks verify policy structure and dependencies. Pronunciation correctness
and production language coverage still need independent reference review and the
work tracked in [remaining requirements](docs/remaining-work.md).

### v0.17 detailed pronunciation trace and explicit variants

`inspect --trace` and `stream-json --trace` now report each consumed source
pronunciation/G2P row and target phone/grapheme mapping row separately, including
layer, key, value and declaration order. Rejected phone attempts remain visible
alongside the selected fallback. Generated decisions explicitly report unknown
resource provenance. The existing text/span ABI and default readings are unchanged.

```sh
build/ttf inspect --trace 'foobar를 사용합니다.'
printf '%s' 'Apple은 사과입니다. foobar를 사용합니다.' | build/ttf stream-json --trace -
```

The web workbench displays **발음 상세 경로** for a selected span and compares trace
as well as text and metadata across input chunk boundaries. `pronunciation_variant`
and `pronunciation_choice` resources allow explicit pronunciation selection; CMUdict
`--variant-policy all` preserves numbered alternatives without selecting one
implicitly. Target pronunciation lexicons retain priority over source dictionaries.

See the [Korean trace and variant editing guide](docs/detailed-pronunciation-trace.md)
for CLI/C API examples, ownership, resource precedence, offset meanings and limits.
The [verification report](docs/research/pronunciation-trace-v17-results.md) records
the regression checks and fixed-source CMU export counts.
This feature provides evidence for reviewing readings; it does not certify their
linguistic accuracy or automatically resolve heteronyms from context.

### v0.18 telephone numbers, extensions and financial identifiers

Phone recognition preserves the leading `+`, every digit including zeros, explicit
groups and the extension in Semantic IR. English account/reference labels select
`DIGIT_SEQUENCE` separately from `PHONE`. Phone and extension reading labels are
editable resources; generic signed quantities and numeric ranges retain their roles.

| Input | Normalized text |
|---|---|
| `Call me at +1 212 555 0100.` | `Call me at plus one two one two five five five zero one zero zero.` |
| `전화: 02-123-4567 내선 0042.` | `전화: 공이 일이삼 사오육칠 내선 공공사이.` |
| `Account: 0012-0034.` (en-US) | `Account: zero zero one two zero zero three four.` |

```sh
build/ttf inspect --language en-US 'Phone: +1 (212) 555-0100 ext. 0042'
build/ttf normalize '전화: 02-123-4567 내선 0042.'
```

The workbench includes **국제전화·내선·계좌번호 구분**, an independently authored
55-case text/IR corpus with editable policies and byte-chunk verification. These
references remain pending independent linguistic review. Recognition does not
validate real subscriber allocation or reachability. See the [Korean editing guide](docs/phone-resources.md)
for labels, extension hints, limits, zero/separator overrides and precedence.
The [measured iteration](docs/research/phone-v18-results.md) records the same-reference
comparison, external regression checks, reference correction and remaining limits.

### v0.19 context scoring for long inputs

Context callbacks now reuse lexical-run boundaries and KO/EN evidence within an
unchanged request scope. Partial token consumption invalidates that evidence;
quotation views and nested requests remain separate. Reading resources, context
scores, segmentation and public ABI v1 layouts retain their previous contracts.

New gates compare 34 authored cases with v0.18 text/span/context traces, exercise
3,000 and 30,000 numerals and repeated partial counters in one unit, and check
arbitrary byte splits, concurrent requests, mutation and error cleanup. The same external development corpus has
identical result files across versions. This is a runtime improvement, with no
claim of improved linguistic accuracy.

See the [measurements and reproduction commands](docs/research/context-cache-v19-results.md)
and [callback ownership contract](docs/language-pack.md#callback-engine-ownership).
Repeated partial token changes and pending scans still need worst-case performance
work; [STREAM-01](docs/remaining-work.md) remains open.
