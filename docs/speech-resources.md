# 발화용 텍스트와 읽기 리소스

v0.20의 `speech` 출력은 TTS에 전달할 **텍스트**다. 합성음 비교나 발음 품질 인증 결과가 아니다. 표기 정책과 정답 코퍼스는 사람이 별도로 검토해야 한다.

## 출력 모드

기존 `ttf_engine_create()`와 CLI의 기본값은 `plain` 입력 / `text` 출력이다. `speech`를 명시하면 이메일·URL 철자 읽기와 최종 문자 처리를 적용한다. 기존 `ttf_config_t`와 결과·언어팩 ABI v1 구조체는 바꾸지 않았다.

```sh
build/ttf normalize --output-mode speech 'a1@b2.com'
# 에이일 앳 비이 닷 씨오엠
build/ttf normalize --output-mode text 'a1@b2.com'
# a1@b2.com
build/ttf normalize --output-mode speech '유&미'
# 유앤미
```

추가 API는 `tts_text_frontend/ttf_frontend_options.h`에 있다. 초기화 함수의 기본값은 `plain` / `speech` / `preserve`다. 설정은 엔진 freeze 전에 적용한다.

```c
#include "tts_text_frontend/ttf_frontend_options.h"

ttf_frontend_options_t options;
ttf_frontend_options_init(&options);
options.output_mode = TTF_OUTPUT_SPEECH;
options.unreadable_policy = TTF_UNREADABLE_PRESERVE;
ttf_status_t status = ttf_engine_set_frontend_options(engine, &options);
/* status를 확인한 후 ttf_engine_freeze(engine)를 호출한다. */
```

등록되지 않은 한자·이모지는 기본적으로 보존한다. `--unreadable drop` 또는 `TTF_UNREADABLE_DROP`은 그런 문자를 제거한다. 코드 등 보호 영역과 모호해서 의도적으로 보존한 의미 객체는 별도의 보존 계약을 가진다. `20F`처럼 문맥 없는 식별자를 임의의 층수로 바꾸지 않는다.

## 일반 패턴과 제어 읽기

다음은 한국어 기본 언어와 `speech` 출력에서의 프로젝트 기본값이다. 단어마다 완성 문자열을 사전에 넣는 대신 lexical 부분·숫자·조사·연산자·단위를 분리해 처리한다.

| 입력 | 기본 출력 | 정책 |
|---|---|---|
| `www.는` | `더블유더블유더블유는` | 붙은 조사 앞의 점은 약어 표시 |
| `www.` | `더블유더블유더블유.` | 마지막 점은 문장 부호로 유지 |
| `k7` | `케이칠` | 한 글자 이름 + 수사 |
| `k007` | `케이영영칠` | 선행 0이 있는 식별 숫자는 자릿수 보존 |
| `top100` | `탑백` | 단어 발음 + 수사 |
| `1~2만 원대면` | `만 원에서 이만 원대면` | 공유 배율 범위 + 생산적인 조사 경계 |
| `건물 20F` | `건물 이십 층` | 명시적인 건물 문맥 |
| `20F 로비` | `이십 층 로비` | 뒤따르는 건물 문맥 |
| `20F` | `20F` | 모호한 식별자 보존 |
| `20°F` | `화씨 이십 도` | 도 기호가 있는 온도 |
| `1+1` | `일 더하기 일` | 두 수 사이의 이항 연산자 |
| `+1` | `플러스 일` | 단항 부호 |

두 글자 이상 대문자 initialism의 기존 모델명 읽기는 별도 정책이다. 기본 `alnum_number ko-KR initialism source-digits`에서 `GPT-4`는 기존 `지피티포`를 유지한다. 고객이 `target-cardinal`을 선택하면 대상 언어 수사로 바꿀 수 있다. 한 글자 `K7`과 단어 `top100`은 이 initialism 숫자 정책 대상이 아니다.

영어 문맥의 speech 출력은 `k7 → kay seven`, `top100 → top one hundred`, `top007 → top zero zero seven`처럼 단어와 숫자를 공백으로 구분한다. 대문자 initialism `GPT-4`는 `gee pee tee four`다. EN의 기본 `alnum_number initialism`은 `target-cardinal`이며, `source-digits`로 설정하면 숫자 부분을 영어 한 자리 이름으로 읽는다. Text 모드에서는 기존 영문 표기를 유지한다.

`top → 탑`은 `lexicons/ko-KR/pattern-additions.jsonl`의 **pending** 항목이다. 개발자가 정한 초기 정책이며 외부 출처 확인·사람의 발음 승인을 주장하지 않는다. 사전 전체 항목과 수정 절차는 [lexicons/README.md](../lexicons/README.md)를 따른다. 완성된 `top100` 같은 User/Customer 사전 항목이 있으면 일반 분해보다 먼저 적용한다.

주석 입력은 `--input-format annotated`로 명시한다. 일반 `plain` 입력과 구분해야 한다. 파싱·이스케이프 규약은 입력 형식 문서를 따른다.

```sh
build/ttf normalize --input-format annotated --output-mode speech '21{@native}'
# 스물하나
build/ttf normalize --input-format annotated --output-mode speech '-12{@sino}'
# 마이너스 십이
build/ttf normalize --input-format annotated --output-mode speech 'ABC{@alphabet}마트'
# 에이비씨마트
```

| 제어 | 지원 범위 | 제한 |
|---|---|---|
| `@native` | KO 정수 0..99, 최대 두 ASCII 숫자 | 0은 프로젝트 편의 정책 `영`; 음수·소수·100 이상은 오류 |
| `@sino` | KO 부호 있는 십진수, 정수와 선택적인 소수부 | 쉼표·지수 표기 불가; 유효 정수 최대 44자리, 전체 IR 95바이트 |
| `@alphabet` | KO/EN ASCII 영문자·숫자 | 구두점·공백·한글은 오류; 영문 대소문자는 같은 글자 이름 |

`@native`는 독립형 `하나/둘/셋/넷/스물`을 사용한다. 기존 `3개 → 세 개`의 관형형 정책과 사전을 공유하지 않는다. EN 팩의 `@native`, `@sino`와 알 수 없는 제어는 `TTF_EINVAL`을 반환한다. `@alphabet`은 pronunciation lexicon이나 English G2P를 호출하지 않는다.

## 이메일·URL 읽기

`EMAIL`과 `URL` Semantic IR을 대상 언어팩이 읽는다. Text 모드는 원문을 보존한다. Speech 모드는 ASCII 문자·숫자를 철자로 읽고, 기호 앞뒤를 공백으로 구분한다. 한국어의 연속 영문자·숫자는 기본적으로 붙여 읽는다.

- KO: `a1@b2.com` → `에이일 앳 비이 닷 씨오엠`
- EN: `a1@b2.com` → `ay one at bee two dot see oh em`

도메인 이름을 번역하거나 URL에 접속하지 않는다. 이 방식은 계정명·경로의 철자를 전달하는 보수적인 초기 정책이다. IDNA/국제화 이메일·의미 단위 도메인 발음은 지원 범위 밖이며, 일반 단어 G2P의 품질과 동일시하면 안 된다.

## 사람이 수정하는 TSV

열은 실제 TAB으로 분리한다: `kind`, `language`, `key`, `value`. 기본값은 `resources/languages/{ko-KR,en-US}/rules/speech.tsv`에 있다. `--customer FILE`이나 API `ttf_resource_load(..., TTF_LAYER_CUSTOMER)`로 overlay를 적용한다. User > Customer > Domain > System > Common 순서다. 운영 중인 frozen engine을 수정하지 않고 새 엔진을 설정·검증한 뒤 교체한다.

| 리소스 종류 | 역할 / 예 |
|---|---|
| `speech_symbol` | 주소의 ASCII 구두점 이름: `ko-KR`, `@`, `앳` |
| `speech_character` | 최종 문자 처리의 등록된 UTF-8 문자열: `ko-KR`, `漢`, `한` |
| `speech_preserve` | 읽지 않고 남길 문장 부호 등: `*`, `.`, `1`; `0`이면 그 보존 규칙 해제 |
| `speech_join` | `alphabet` 키에 `none` 또는 `space`: 연속 글자 이름의 결합 |
| `phone_letter` | KO `letter.A` → `에이`; 강제 알파벳 읽기에도 사용 |
| `letter_name` | EN `a` → `ay` |
| `digit` / `number` | KO/EN 숫자 한 자리 읽기 |
| `native_standalone` | KO `1` → `하나`, `20` → `스물`; 0..99 완성값 override 가능 |
| `math_operator` | 현재 `+` → KO `더하기` / EN `plus`; 단항 부호와 분리 |
| `alnum_number` | `initialism=source-digits`는 두 글자 이상 대문자 initialism의 숫자에 기존 source pronunciation 정책을 유지; `target-cardinal`로 바꾸면 대상 언어 수사 사용 |
| `context_neutral` | EN `web_initialism=1`: `www.` 자체가 영어 문장 증거가 되지 않도록 함; `alnum_label=1`: speech 모드의 숫자 결합 영문 레이블이 기본/주변 문맥을 상속 |

`native_standalone`에 완성 숫자 항목이 없으면 십 단위와 일 단위 항목을 결합한다. 필수 조각이 없으면 리소스 오류다. `speech_character`는 1..95바이트 키를 허용한다. 키 길이 검증은 완전한 Unicode grapheme segmentation 지원을 뜻하지 않는다. 초기 문자 처리는 모든 ZWJ 이모지 조합의 경계를 판별하지 않으며, 등록되지 않은 조합에 preserve/drop을 적용한다. 숫자 키캡은 별도로 하나의 그림 문자로 보호하여 숫자 부분만 정규화하지 않는다. 기호 읽기 값은 비어 있을 수 없다. 삭제는 빈 문자열 사전값 대신 `unreadable` 정책으로 선택한다.

예를 들어 TAB으로 저장한 고객 overlay:

```text
speech_symbol	ko-KR	@	골뱅이
speech_character	ko-KR	漢	한
native_standalone	ko-KR	20	스물
math_operator	ko-KR	+	더하기
```

```sh
build/ttf validate-resource --require-complete-validation customer-speech.tsv
build/ttf inspect --trace --customer customer-speech.tsv --output-mode speech 'a1@b2.com'
```

`inspect`의 span, Semantic IR, source/context/render language와 실제 출력을 함께 확인한다. 새 철자 읽기·독립형 수사·이항 연산자는 실제 사용한 리소스 행과 layer를 `policy` 이벤트로 남긴다. 철자 이벤트의 part 범위는 해당 철자 읽기에 전달한 원자 내부의 바이트 위치다. Trace 수집 범위는 콜백의 계측 여부에 따라 다르며, 기존 모든 수사 읽기에 대한 완전한 리소스 추적이나 문자별 음소 정렬·언어학적 정답 보증을 제공하지 않는다.

## 가격대와 층 표기

`resources/languages/ko-KR/rules/patterns.tsv`에서 관리한다.

- `quantity_tail_head`의 `대`와 기존 `particle` / `quantity_tail`의 완전한 뒷부분을 조합한다. `대면`, `대는`, `대입니다`를 다룰 수 있으며 `대박`처럼 나머지가 등록되지 않은 단어는 경계로 인정하지 않는다. 모든 문장을 접두사만 보고 잘라 읽지 않는다.
- `floor_alias`의 대문자 `F`는 semantic unit `floor`다. `floor_cue_before` 또는 `floor_cue_after`에 등록된 바로 옆 단어가 있어야 한다. 수와 F 사이에는 공백을 허용하지 않고, 문맥 단어와 숫자 사이에는 가로 공백을 허용한다. 줄바꿈을 넘어 문맥을 가져오지 않는다.
- `floor_cue_before_suffix`의 `타워`/`빌딩`은 `대륭포스트타워` 같은 건물 이름의 명사형 접미사를 인정한다. 완전한 토큰 cue가 먼저이며, 없을 때 가장 긴 등록 suffix를 찾는다. `0`으로 지정한 완전한 cue나 긴 suffix는 더 짧은 suffix로 우회하지 않는다.
- cue 값은 `1`/`0`이다. Streaming planner도 같은 floor cue 리소스를 참조하여 작은 segment에서 문맥이 먼저 확정되지 않게 한다. 기본 exact cue에는 일반 문장 분할에 사용할 `boundary_word` 항목도 포함되어 있다.
- `unit ko-KR floor 층`이 실제 출력 단위를 정한다. `°F`는 공통 measurement 문법을 쓰므로 `F`의 층 표기와 구별된다.

정답 검토는 독립적으로 작성한 텍스트 코퍼스를 기준으로 한다. 구현 결과에 맞춰 기대값을 자동 수정하지 않는다. 사용자와 고객의 실제 문장, 부정 예, resource override, 임의 바이트 분할 streaming을 함께 검증해야 한다.

## 문장별 예외와 고객 상품명

일반 덧셈은 `더하기`를 유지하고, 행사명은 입력 제어로 지정할 수 있다.

```text
[[1+1]]{원플러스원} 행사
K7{케이세븐} 타고 다닌다
```

매번 주석을 넣지 않으려면 고객 TSV에 별도 종류로 등록한다 (실제 TAB 구분).

```text
exact	ko-KR	1+1	원플러스원
lexicon	ko-KR	en-US:k7	케이세븐
```

`exact`는 TN 치환이고 `lexicon`은 외국어 발음이다. 목적에 맞는 종류를 선택한다.
`--customer FILE` 또는 freeze 전 `ttf_resource_load(..., TTF_LAYER_CUSTOMER)`로
적용한다. 등록 전/후 `inspect --trace`에서 실제 소비 범위와 뒤따르는 조사를
확인한다. 한 번의 직접 alias는 이러한 사전보다 먼저 적용된다.
