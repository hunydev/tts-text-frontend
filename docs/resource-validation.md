# 리소스 정책 검증 (v0.16)

리소스 편집 오류를 요청 처리 전에 발견한다. C runtime이 공통 문법과 pack별
정책을 검사하며 CLI, Python compiler, 웹 도구도 같은 검증기를 사용한다.
**검사 통과는 읽기 문장의 정답 승인과 다르다.** 예를 들어 `애플` 대신 다른
한글을 넣어도 유효한 발음 문자열일 수 있다. 의미·관용형은 별도 corpus와
독립 검토로 확인해야 한다.

## 편집 → 검사 → 적용

```sh
cmake -S . -B build
cmake --build build -j4
# 아래 예제 파일은 저장소에 있다. 다른 파일을 지정해도 된다.
build/ttf validate-resource --resources resources \
  --resource-layer customer --require-complete-validation \
  resources/customers/example/ko-KR/rules.tsv

python3 tools/resource_compiler/compile_resources.py \
  resources/customers/example/ko-KR/rules.tsv /tmp/customer.ttfb \
  --ttf build/ttf --resources resources --require-complete-validation

build/ttf normalize --customer /tmp/customer.ttfb '123-456'
```

`validate-resource`는 JSON 한 개를 stdout에 쓴다. 성공 status는 `success`,
실패는 0이 아닌 exit code이다. `records`, `validated_records`,
`unvalidated_records`는 로드한 **전체 엔진**의 검사 행 수이며 overlay만의 수가
아니다. 성공 시 세 수의 관계는 `records = validated + unvalidated`다. 같은
키의 낮은 layer 행도 개별 검사에 포함한다. 검사 도중 실패한 report는 실패
직전까지 통과한 행만 센다. 로드 실패 시 최종 검사는 실행되지 않아 0이다.

```text
# 두 번째 실제 행의 값 오류. 실제 TSV에는 탭을 사용한다.
context<TAB>ko-KR<TAB>hangul_weight<TAB>9999
```

오류 JSON의 `diagnostic`에는 `path`, 1부터 시작하는 물리적 `line`, `kind`,
`language`, `key`, `layer`, `message`가 있다. 빈 줄과 주석도 행 번호에 포함된다.
TTFB에서도 payload의 원래 행 번호를 사용한다. CLI stderr에는 같은 원인의
짧은 설명을 쓴다. UTF-8 진단 문자열은 고정 용량에서 글자 중간을 자르지 않는다.

기본 리소스 경로는 `--resources`, 선행 overlay는 `--domain`, `--customer`,
검사 대상 파일의 layer는 `--resource-layer`로 지정한다. 기본 layer는 customer다.
컴파일러도 `--language`, 반복 가능한 `--domain`/`--customer`를 같은 순서로 받는다.
실제 배포와 같은 root 및 overlay 순서로 검사해야 한다. 같은 layer의 동일 키는
첫 선언이 우선하며, 검증을 통과했다고 이 우선순위가 바뀌지 않는다.

## 검사 시점과 원자성

1. `ttf_engine_create_ex`가 manifest 및 system 파일을 읽고 각 행을 검사한다.
2. `ttf_resource_load_ex`가 새 파일을 임시 저장소에 파싱하며 등록된 행 검증기를
   적용한다. 실패하면 기존 자원, 선언 순서, 기존 lookup 결과가 유지된다.
3. `ttf_engine_validate_resources`가 선언 순서대로 모든 저장 행을 검사한 후,
   최종 layer 우선순위를 적용한 값 사이의 관계를 검사한다.
4. `ttf_engine_freeze`도 동일 최종 검사를 수행한 뒤 인덱스를 만든다. 실패한
   엔진은 mutable 상태를 유지하므로 높은 layer의 올바른 값으로 수정할 수 있다.

행 검사는 타입, 범위, 문법을 확인한다. 날짜의 `min_year <= max_year`, 필요한
영어 읽기 label, 스트리밍 경계 힌트 등은 모든 overlay를 적용한 뒤 검사한다.
따라서 관련 파일의 로드 순서를 불필요하게 제한하지 않는다. 최종 관계 검사에서는
원래 파일 위치를 보관하지 않으므로 `path`는 빈 문자열, `line`은 0이다. 이때
진단은 관계에 문제가 있거나 누락된 key를 가리킨다. 현재 pack의 최종 관계
진단에서 `layer=common`은 초기값이다. 문제를 만든 원본 행의 layer를 추적한
결과로 해석하면 안 된다.

`ttf_resource_load`와 `ttf_engine_create`도 새 검사를 수행한다. 기존 API는
상태 코드만 돌려주며, 상세 진단이 필요하면 `_ex` API를 사용한다.

## 주요 schema와 편집 주의사항

| 리소스 | 허용 범위 / 검사 |
| --- | --- |
| `classifier_priority` | rule ID <64 bytes, 정수 -1000..1000 |
| `numeric_group_separator` | 쉼표 또는 지원되는 Unicode 가로 공백 1자; 0 또는 3 |
| `numeric_suffix` | `COUNTER:unit`, `CURRENCY:unit`, `MEASURE:unit`; unit <32 bytes |
| `currency_scale` | 3, 6, 9, 12, 15, 18 |
| `currency_name`, `currency_prefix` | bounded currency ID; 이름은 소문자 ASCII 단어와 source tag |
| `ordinal_suffix` / `ordinal_tail` | source tag / 숫자 tail과 bounded suffix |
| `semantic_term` | `WORD`, `INITIALISM`, `ACRONYM`, `ALNUM_TERM` + source tag |
| 경계·숫자 continuation flag | 0 또는 1; 실제 bounded lookup key |
| `context_connector` | 문장부호/기호 1자와 `number` |
| `protection`, `protected_tag` | 알려진 switch 0/1; tag는 `opaque`/`transparent` |
| `pattern` | `{digits}` placeholder; literal brace는 `exact` 사용 |
| `lexicon` | source tag + bounded 단어/구문 key; 표기 정답은 별도 검토 |
| EN `context` | `latin_weight`, `phrase_ceiling`, `clause_weight`: 0..1000; `context_cue`/`context_word` weight도 0..1000, key <96 bytes |
| EN `context_role` | `finite`, `imperative`, `subject`, `polite`, `0` |
| EN `date_policy` | `named_year_style`, `numeric_year_style`, `year_style`: `calendar`/`cardinal`; `min_year`/`max_year`: 1..9999, min <= max |
| EN `month_alias` | key는 소문자 ASCII 알파벳 1..31자, 값은 정수 1..12 |
| EN `date_year_cue`, `date_year_quantity` | 값은 `0`/`1`; cue key는 소문자 ASCII 단어를 한 칸으로 연결한 1..64 bytes, quantity key는 알파벳 단어 1..63자 |
| EN `initialism_policy` | key는 `dotted`, `known`, `inferred`; 값은 `preserve`, `letters`, `names` |
| EN `initialism_text`, `letter_name` | key는 각각 소문자 ASCII 알파벳 1..255자 / 1자; initialism 값은 위와 같은 3개 정책 |
| EN `currency_minor_digits` | 0 또는 2 |
| EN pronunciation/G2P | phone 문자열 1023 bytes 이하, 최대 128 tokens; 공백으로 구분한 token은 stress 포함 23 bytes 이하; 대문자 알파벳 phone 뒤에 선택적 stress 0..2 또는 `letter.A`..`letter.Z` |
| KO `context` | `hangul_weight`, `particle_weight`, `clause_floor`, `script_weight`: 0..1000 |
| KO `counter`, `native_ordinal_suffix` | `native` 또는 `sino` |
| KO `counter_role_before/after` | `frequency`, `identifier`, `0` |
| KO `ordinal_policy` | `large=sino` |
| KO `scale` | 1..3 또는 4의 배수 4..40 |
| KO phone onset/vowel/coda | 각각 0..18, 0..20, 0..27 |
| KO `phone_letter` | key는 stress 없는 대문자 phone symbol(최대 23자) 또는 `letter.A`..`letter.Z`; 값은 직접 출력할 label |
| KO `policy` | range mode `cardinal`/`digits`, repeat 0/1, 구분 문자열 |
| KO `identifier_label` | `digits` 또는 `disabled` |
| KO `particle`, `quantity_tail` | 존재 여부로 동작하므로 1만 허용 |

정수 범위로 검사하는 값은 선택적 ASCII `+`/`-` 뒤에 한 자리 이상의 숫자가
오는 표현이다. 공백은 허용하지 않으며 선행 0은 허용한다. 예를 들어 context
weight `+020`은 20이다. 반면 flag와 enum은 문자열을 그대로 비교한다.
`0`/`1` flag에 `+1`, `01`, `true`를 쓰거나 `currency_minor_digits`에 `+2`를
쓰면 거부한다. 숫자 **key**의 문법도 숫자 값과 별도다. 예를 들어 `ordinal_tail`
key에는 부호를 쓸 수 없다. 부분 숫자(`2oops`), overflow, 허용 범위 밖 값은
거부한다. 일반 읽기 label은 비어 있지 않고 ASCII 제어 문자를
포함하지 않아야 한다. 모든 key에 범용 제한 하나를 씌우는 대신 소비자의 실제
lookup 용량을 따른다. 자세한 구현은 `src/resource/validation.c`,
`languages/en-US/validate.c`, `languages/ko-KR/validate.c`에 있다.

ASCII 소문자로 lookup하는 EN `lexical`, `context_word`, `context_role`,
pronunciation 계열, `g2p` key에 ASCII 대문자를 넣으면 거부한다. 단어 key의
apostrophe나 hyphen 등을 일괄 금지하지는 않는다. `native`, `native_tens`,
`ordinal_first`는 숫자를 정규화한 key로 조회하므로 선행 0을 쓰지 않는다.
입력 `01번째`를 바꾸려면 `ordinal_first`의 key `1`을 편집한다.

Pronunciation 검사 대상은 `pronunciation`, `initialism_pronunciation`,
`digit_pronunciation`, `g2p`다. source lookup key는 255 bytes 이하이며
`g2p`는 5 bytes 이하(앞의 `^`와 뒤의 `$` 문맥 표시는 길이에서 제외.
2026-09-25에 4에서 5로 늘렸다. `-ation`, `^reaso`처럼 영어 접미사·어간을 한 행으로
적으려면 5자가 필요하고, matcher도 같은 상한으로 가장 긴 조각부터 찾는다),
`digit_pronunciation`은 `0`..`9` 한 자리다. phone 열에는 `.` 한 글자를 형태소
경계로 쓸 수 있다. 음소가 아니라 KO mapper에게 "여기서 음절을 끊으라"는
표시이며, 앞 자음이 받침이 된다(`^up` → `AH P .`, uploading 업로딩).
경계를 모르는 mapper는 이 token을 무시하면 된다. 고정된
CMU phone 목록만 허용하는 검사는 아니므로 추가 대문자 phone symbol도
받아들인다. 따라서 통과해도 KO mapper가 모든 phone을 지원한다는 뜻은 아니다.
행의 128-token 제한은 현재 builtin KO mapper 용량에 맞춘 것이다. 모르는
phone으로 인한 매핑 실패에는 기존 grapheme fallback을 적용하지만
`TTF_ELIMIT` 용량 오류는 호출자에게 반환한다. 여러 G2P 행을 합쳐 생성하는
결과의 길이는 입력에 따라 달라지므로 행 단위 검증만으로 모든 요청의 용량
오류를 배제하지는 않는다. 향후 mapper별 용량 계약은 source IR의 문자열
용량과 구분하여 확장해야 한다.

### 의존 관계

EN pack이 등록되어 있으면 freeze 전에 다음 effective label을 검사한다.
`en-US` 조회에는 일반 lookup과 동일한 wildcard/layer 우선순위가 적용된다.

| Kind | 필수 key |
| --- | --- |
| `number` | `0`..`19`, `20`, `30`, …, `90`, `100`, `1000`, `1000000`, `1000000000`, `1000000000000`, `1000000000000000`, `1000000000000000000` |
| `number` 연결어 | `minus`, `plus`, `point`, `percent`, `and`, `oh`, `oclock`, `range`, `fraction_over` |
| `ordinal` | `0`..`19`, `20`, `30`, …, `90`, `suffix` |
| `month` | `1`..`12` |
| `date_word` | `the`, `of` |
| `clock` | `midnight`, `after`, `h24_full_hour` |
| `unit` | `s.one`, `s.many` |
| `meridiem` | `AM`, `PM` |

공통 classifier가 실제 조회하는 `*` namespace의 effective `currency_prefix`,
`numeric_suffix=CURRENCY:…`, `currency_name`에서 얻은 통화 ID에는 EN
`currency`의 `<ID>.one`, `<ID>.many`가 필요하다. 해당 통화의 effective
`currency_minor_digits=0`이면 minor label은 요구하지 않는다. 값이 `2`이거나
정책 자체가 없어서 기본값 2를 사용하면 `<ID>.minor.one`, `<ID>.minor.many`도
필요하다. 낮은 layer에 가려진 currency identity는 의존성을 추가하지 않는다.

세 `initialism_policy` 중 하나가 `names`이면 `letter_name`의 `a`..`z` 전체가
필요하다. 특정 `initialism_text`만 `names`이면 그 key에 포함된 글자만 요구한다.
선택적인 pronunciation/G2P 사전은 없어도 기존 fallback 경로를 사용할 수 있다.
KO의 코드 fallback이 있는 label을 EN과 같은 필수 테이블로 강제하지 않는다.

EN `month_alias`, 활성 `date_year_cue`, KO 활성 `counter_role_before`,
`ordinal_marker prefix`는 각 단어에 유효한 `boundary_word=1`이 있어야 한다.
분할 지점에서 문맥을 잃지 않기 위한 조건이다. multiword cue라면 마지막 단어만
추가하지 말고 각 단어에 힌트를 추가한다. `date_year_cue=0`,
`counter_role_before=0`인 비활성 cue에는 요구하지 않는다. `month_alias`에는
0 비활성 값이 없고 `ordinal_marker`의 값은 marker 자체다. 경계 힌트는
segmenter와 동일하게 `*` 또는 등록된 pack 중 한 곳에서 effective 값이 `1`이면
충족한다.
새 cue와 힌트는 같은 파일이나 다른 overlay 파일에 나눠 둘 수 있다.
활성 EN `abbreviation_expansion`에는 같은 key의 `abbreviation=preserve`가 필요하다.
음소 수 제한은 개별 resource 행의 제약이다. 여러 G2P 조각을 합친 결과에는
기존 pronunciation IR와 출력 buffer의 용량 제한도 적용된다.

활성 `abbreviation_expansion`에는 같은 key의 EN `abbreviation=preserve`
marker가 필요하다. 공통 `abbreviation * … protected` marker가 이미 있어도
충족한다. 문장 경계에서 `Rev.`와 뒤의 이름이 갈라져 새 title 정책이 무시되는
것을 방지한다. `abbreviation_expansion=preserve`로 비활성화했거나 더 높은
layer에 가려진 expansion은 이 의존성을 추가하지 않는다.

## 알 수 없는 리소스와 확장 pack

알 수 없는 namespace는 기본적으로 로드 가능하며 `unvalidated_records`에 센다.
새 언어/고객 기능의 resource를 core allowlist 때문에 막지 않기 위한 선택이다.
알려진 pack의 고정 policy kind에서 key 오타나 잘못된 enum은 거부한다. 알 수 없는
종류 자체를 오타로 취급하고 싶다면 CLI/compiler에
`--require-complete-validation`을 사용한다. 기본 manifest는 미검증 행이 0이다.

KO/EN 검증기는 자신의 정확한 language tag 및 `*` 행만 담당한다. 예를 들어
`date_policy`의 `ja-JP` 행은 EN 정책으로 제한하지 않는다. 반면 `*`는 여러
pack이 소비할 수 있으므로 해당 검증기를 모두 통과해야 한다. 공유 `context`
kind에서 `context * hangul_weight`는 KO 검증기가 담당한다. 어느 pack도 모르는
wildcard context key는 미검증으로 남지만 `context ko-KR`의 모르는 고정 key는
거부한다. 공통 문법 검증기는 언어와 관계없이 자신이 담당하는 kind를 검사한다.
`abbreviation * … protected`는 공통 경계 marker이고, EN 전용 marker는
`abbreviation en-US … preserve`이므로 값을 서로 바꾸지 않는다.

새 header `tts_text_frontend/ttf_resource_validation.h`의
`ttf_resource_validator_t`를 `ttf_engine_register_resource_validator`로 등록한다.
이 descriptor는 별도 ABI version 1을 갖는다. 기존 config, language pack,
semantic IR, result, span 구조체 layout은 바뀌지 않았다. 기존 제3언어 pack도
새 필드를 채우지 않고 등록할 수 있다.

- `validate_record(record, handled, diagnostic, userdata)`는 담당 행에만
  `handled=1`을 설정한다. 여러 검증기가 담당하면 모두 통과해야 한다.
- `validate_engine(engine, diagnostic, userdata)`는 `ttf_resource_get`으로 최종
  layered 값을 읽어 관계를 검사한다. 자체 저장소 조회가 필요하면 callback의
  caller-owned userdata를 사용할 수 있다.
- descriptor, 이름, userdata는 caller 소유이며 엔진보다 오래 살아야 한다.
  record의 문자열은 callback 동안만 빌린다. 진단 출력은 caller-owned 복사본이다.
- registry는 최대 64개이며 같은 이름 재등록은 거부한다. 등록 순서가 검사 순서다.
- callback은 엔진을 변경하거나 파괴하지 않는다. mutable setup의 재진입
  load/freeze/register/validate는 `TTF_ESTATE`다. frozen validation은 읽기 전용이며
  순수 callback일 때 동시 호출 가능하다. callback의 재귀 validation은 금지한다.
- 이미 로드한 행을 담당하는 검증기를 늦게 등록하면 explicit validate 또는
  freeze 때 검사한다. 등록 자체는 기존 행을 변경하지 않는다.

CMake는 `languages/<tag>/validate.c`가 있으면
`ttf_resource_validator_<tag with underscores>`를 자동 등록한다. 새 pack 추가를
위해 core의 언어 분기를 늘릴 필요가 없다.

## API 예제

```c
#include <stdio.h>
#include "tts_text_frontend/ttf_resource_validation.h"

/* cfg는 ttf_config_init으로 초기화한 설정이다. */
ttf_engine_t *engine = NULL;
ttf_resource_diagnostic_t diagnostic;
ttf_resource_validation_report_t report;
ttf_resource_diagnostic_init(&diagnostic);
ttf_resource_validation_report_init(&report);
ttf_status_t status = ttf_engine_create_ex(&cfg, &engine, &diagnostic);
if (status == TTF_OK)
    status = ttf_resource_load_ex(engine, "customer.tsv", TTF_LAYER_CUSTOMER, &diagnostic);
if (status == TTF_OK)
    status = ttf_engine_validate_resources(engine, &report, &diagnostic);
if (status == TTF_OK)
    status = ttf_engine_freeze(engine);
if (status != TTF_OK)
    fprintf(stderr, "%s:%zu [%s/%s/%s] %s\n", diagnostic.path, diagnostic.line,
            diagnostic.kind, diagnostic.language, diagnostic.key, diagnostic.message);
/* 성공한 엔진으로 요청을 처리하고, 모든 요청 종료 후 파괴한다. */
ttf_engine_destroy(engine);
```

report/diagnostic 포인터는 선택적이다. 전달할 때는 init helper로 `struct_size`를
설정한다. 정규화와 stream에서 사용하려면 freeze 성공까지 확인해야 한다.

## compiler와 웹

compiler 기본 모드는 `--ttf`, 환경변수 `TTF_CLI`, 저장소 `build/ttf`, PATH 순서로
현재 CLI를 찾는다. 읽은 바이트 스냅샷을 C로 검사한 뒤 같은 payload를 TTFB로
감싼다. 성공한 임시 파일만 원자적으로 교체하므로 실패 시 기존 산출물은 유지된다.
`--syntax-only`는 아직 C executable이 없는 bootstrap을 위한 명시적 옵션이며,
의미 검증을 했다고 표시하지 않는다. compiler는 CLI 응답의 version, 행 수, diagnostic 구조도 확인하고, strict mode의
미검증 행 수를 직접 검사한다. 그 산출물도 runtime load/freeze에서 다시
검사한다. `--syntax-only`와 `--require-complete-validation`은 함께 쓸 수 없다.
TTFB v1 형식은 유지되며 checksum은 인증 수단이 아니다.

웹의 **발음사전 / 고객 리소스 실험**에 TSV를 넣고 실행하면 오류 파일·행·키가
표시된다. 성공 시 출력 아래에서 정책 검사 행 수와 검사기 미등록 행 수를 볼 수
있으며 다운로드 JSON에도 `resource_validation`을 포함한다. 웹은 원본 파일을
고치지 않고 요청별 임시 customer overlay로 실험한다.

## 검증 범위

native API 테스트는 로드 원자성, TSV/TTFB, layer와 wildcard 우선순위, 실패
freeze 후 복구, 제3언어 ABI와 추가 검증기, callback 재진입 방어를 검사한다.
CLI/compiler 테스트는 진단 좌표, 잘못된 scalar/관계/의존성, strict unknown,
산출물 교체 원자성을 검사한다. 웹 테스트는 진단 전달 및 검사 행 수 표시를
확인한다. 기존 텍스트 corpus와 byte-split 검사를 함께 실행한다.

이 검증기는 가능한 모든 커스텀 언어 정책이나 문맥 문법을 증명하지 않는다.
추가 namespace에는 검증기를 추가하고, 발음/읽기 정답은 버전 관리한 reference와
독립 검토를 유지한다. 리소스 schema 통과만으로 production 언어 품질 완료를
주장하지 않는다.
