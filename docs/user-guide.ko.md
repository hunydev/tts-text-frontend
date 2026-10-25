# TTF 사용자 가이드 (2026-10-26)

TTF(TTS Text Frontend)는 TTS에 넣을 문장을 **읽는 그대로의 문장**으로 바꾸는 C 라이브러리다. `3개`는 `세 개`, `$150`은 `백오십 달러`, `Apple은`은 `애플은`이 된다. 한국어·영어·일본어·중국어·베트남어·태국어·아랍어 팩이 들어 있고, 문장마다 언어를 정해 그 언어의 팩이 읽는다.

이 문서는 엔진을 붙여 쓰는 사람을 위한 안내다. 세부 계약은 각 절의 링크를 본다.

## 1. 빌드와 링크

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix /opt/ttf      # 선택
```

- 결과물: `libtts_text_frontend.so`, `libtts_text_frontend.a`, CLI `ttf`.
- 헤더: `tts_text_frontend/ttf.h`(엔진·정규화·사전), `ttf_frontend_options.h`(출력 옵션).
- 리소스: 실행할 때 `resources/` 폴더가 필요하다. 설치했다면 `share/tts-text-frontend/resources`를 `config.resource_root`로 준다.
- 외부 런타임 의존성은 없다(Linux, C11).

## 2. 엔진 준비

엔진 준비는 **프로그램 시작 때 한 번, 한 스레드에서** 한다.

```c
#include <tts_text_frontend/ttf.h>
#include <tts_text_frontend/ttf_frontend_options.h>

ttf_config_t config;
ttf_frontend_options_t options;
ttf_engine_t *engine = NULL;

ttf_config_init(&config);                       /* 기본값을 채운다 */
config.resource_root = "/opt/ttf/share/tts-text-frontend/resources";
config.default_language = "ko-KR";              /* 요청 언어 */
ttf_engine_create(&config, &engine);            /* 7개 언어 팩과 시스템 리소스 적재 */

ttf_frontend_options_init(&options);
options.output_mode = TTF_OUTPUT_SPEECH;        /* TTS용 출력 (아래 4절) */
ttf_engine_set_frontend_options(engine, &options);
ttf_engine_set_option(engine, "brackets", "skip");               /* 이름 있는 옵션 */

ttf_resource_load(engine, "domain/finance.tsv", TTF_LAYER_DOMAIN);   /* 사전 (5절) */
ttf_resource_load(engine, "customer/acme.tsv", TTF_LAYER_CUSTOMER);
ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "Qorvex", "코르벡스");

ttf_engine_freeze(engine);                      /* 여기서부터 읽기 전용 */
/* … 요청 처리 … */
ttf_engine_destroy(engine);                     /* 모든 요청이 끝난 뒤 한 번 */
```

| `ttf_config_t` 필드 | 기본값 | 뜻 |
|---|---|---|
| `resource_root` | 빌드 때 정한 경로 | 리소스 폴더 |
| `default_language` | `ko-KR` | 요청 언어. 문장 언어를 정하지 못할 때 쓴다 |
| `first_segment_target`, `normal_segment_target`, `max_segment_length` | 48, 112, 200바이트 | 스트림이 콜백으로 내보내는 조각 길이 |
| `max_pending_bytes` | 1MB | 끊을 수 없는 토큰 하나의 최대 길이 |
| `sentence_split`, `phrase_split` | 1, 1 | 스트림을 문장·구 경계에서 끊을지 |

- **고정(freeze) 뒤에는 바뀌지 않는다.** 리소스 적재, 옵션 변경은 `TTF_ESTATE`가 된다.
- **언어와 옵션은 엔진 단위다.** 요청 언어나 출력 옵션 조합이 여러 가지면 조합마다 엔진을 하나씩 만들어 고정해 두고, 요청을 맞는 엔진으로 보낸다.
- **적재 시간·메모리:** 엔진 생성과 고정에 약 50–80ms, 엔진 메모리 약 12MB(사용자 사전 없음, VM 2코어 기준). 백만 개 사용자 사전이면 약 1초, 220MB다.

## 3. 정규화

### 한 번에

```c
ttf_result_t result;
if (ttf_normalize(engine, text, strlen(text), &result) == TTF_OK) {
    send_to_tts(result.text, result.length);    /* UTF-8, NUL로 끝남 */
    ttf_result_free(&result);                   /* 성공한 결과는 꼭 해제 */
}
```

- 입력과 출력은 UTF-8이고 길이는 바이트다.
- `result.spans`에는 바뀐 구간마다 원문 위치(`input_begin`, `input_end`), 출력 위치(`begin`, `end`), 의미(날짜·통화…), 언어, 적용한 규칙과 사전 층이 들어 있다. 하이라이트나 디버깅에 쓴다.
- 상태 코드: `TTF_OK`, `TTF_EINVAL`(잘못된 인자), `TTF_EUTF8`(잘못된 UTF-8), `TTF_ENOMEM`, `TTF_ELIMIT`(길이 한도), `TTF_ESTATE`(고정 전 정규화, 고정 뒤 변경), `TTF_ERESOURCE`, `TTF_EIO`, `TTF_ECALLBACK`.
- 속도: 한국어 문장(평균 58자) 하나에 약 0.3ms. 입력 길이에 비례해서 3만 자 한 덩어리도 약 0.13초다.

### 스트림 (조금씩 들어오는 텍스트)

```c
static int on_segment(const ttf_segment_t *segment, void *user) {
    enqueue_tts(segment->text, segment->length);  /* 콜백 안에서 복사한다 */
    return 0;                                     /* 0이 아니면 스트림 취소 */
}
ttf_stream_t *stream;
ttf_stream_create(engine, on_segment, queue, &stream);
ttf_stream_write(stream, chunk, chunk_length);   /* 여러 번. 글자·숫자 중간에서 끊겨도 된다 */
ttf_stream_flush(stream);                        /* 끝 */
ttf_stream_destroy(stream);
```

- 안정된 경계(문장·구)가 오면 그 앞까지를 콜백으로 보낸다. 콜백 조각을 이으면 한 번에 정규화한 결과와 같다.
- LLM 응답처럼 토큰 단위로 들어오는 텍스트를 바로 TTS로 넘길 때 쓴다. 상세: [streaming.md](streaming.md).

## 4. 멀티스레드

| 단계 | 스레드 |
|---|---|
| 엔진 준비(create·load·옵션·freeze), 해제(destroy) | 한 스레드에서 한 번 |
| `ttf_normalize` | **고정된 엔진 하나를 모든 스레드가 락 없이 함께 쓴다** |
| 스트림 | 스트림 하나는 한 스레드만 쓴다. 스트림끼리는 동시에 돌아도 된다 |
| 요청 사전 | 요청이 쓰는 동안 바꾸지 않는다. 아무도 바꾸지 않는 사전은 여러 요청이 함께 써도 된다 |

```c
/* 채널(스레드)마다 */
void *channel(void *arg) {
    for (;;) {
        request_t *q = next_request();
        ttf_result_t r;
        if (ttf_normalize_with(engine, q->dict, q->text, q->length, &r) == TTF_OK) {  /* dict는 NULL 가능 */
            reply(q, r.text);
            ttf_result_free(&r);
        }
    }
}
```

- 처리량은 코어 수까지 늘어난다(2코어 VM: 1스레드 약 3,500 → 2스레드 약 7,300문장/초). 코어보다 채널이 많으면 대기 시간이 길어지므로, 동시 처리 채널은 코어 수 정도로 두고 나머지는 대기열로 받는다.
- 서비스 중 엔진 사전을 바꾸려면 새 엔진을 만들어 고정하고 새 요청부터 보낸 뒤, 옛 엔진은 진행 중인 요청이 끝나면 해제한다. 요청·고객사마다 다른 낱말은 엔진을 새로 만들지 않고 요청 사전(6절)으로 준다.
- 상세와 측정: [threading.ko.md](threading.ko.md).

## 5. 출력 옵션

엔진을 고정하기 전에 정한다. 예는 한국어 엔진, 입력 `NASA(미국 항공우주국)의 Zorbly 엔진은 國語 ★ 국물이 맛있다`.

| 옵션 | 값 (굵게: 기본) | 효과 |
|---|---|---|
| `output_mode` | text / **speech**(옵션 초기화 기준) | speech는 이메일·URL·기호까지 읽는다. text는 `a1@b2.com`을 그대로 둔다 |
| `brackets` | **read** / skip / preserve | skip: `나사의 조블리 엔진은…` (괄호 속을 읽지 않음) |
| `unknown_foreign` | **guess** / spell / preserve | 사전에 없는 영어 낱말. spell: `제트오알비엘와이` |
| `initialism` | **spell** / word | 대문자 줄임말을 글자로 읽을지 낱말로 읽을지 (사전 항목이 먼저) |
| `hanja`, `kana` | **passthrough** / korean | korean: `國語` → `국어` |
| `unreadable_policy` | **preserve** / drop / read | 읽을 수 없는 문자. drop: `國語 ★`가 빠짐 |
| `decorative` | **follow** / read | 별·하트·이모지의 이름을 읽을지 |
| `symbol_fallback` | **on** / native / off | 규칙이 없는 기호·이모지를 유니코드 이름으로 읽을지 |
| `pronunciation` | **orthographic** / hangul | hangul: `국물이 맛있다` → `궁무리 마싣따` |
| `spacing` | **orthographic** / reading-unit | 한국어 수 읽기의 띄어쓰기 |
| `particle_agreement` | **언어 기본**(한국어: 그대로) / off / on | on: 읽기 뒤 조사를 읽기에 맞춤(`Apple를` → `애플을`, 기본은 `애플를`) |
| `numeric_fallback` | **read** / preserve | 문법이 받지 않은 수를 그래도 읽을지 |
| `matrix` | **auto** / declared / legacy | 문장 언어를 문장마다 판정할지, 요청 언어로 정할지 |
| `input_format` | **plain** / annotated | annotated: 문장 안 읽기 제어(7절)를 해석 |

`output_mode`, `unreadable_policy`, `input_format`, `hanja`, `kana`는 `ttf_frontend_options_t` 필드로, 나머지는 `ttf_engine_set_option(engine, 이름, 값)`으로 준다. CLI에서는 `--brackets skip`처럼 쓴다.

## 6. 사전과 리소스 (TSV)

### 6.1 형식

UTF-8 텍스트, 한 줄에 **탭으로 나눈 4칸**이다. 칸 안의 띄어쓰기는 값의 일부다.

```text
종류<TAB>언어<TAB>키<TAB>값
```

```tsv
exact	ko-KR	알쓰	알콜 쓰레기
exact	*	🍔👌	햄버거 좋아
lexicon	ko-KR	en-US:Qorvex	코르벡스
pattern	ko-KR	내선{digits}	내선 번호
policy	ko-KR	range_separator	 번에 
```

- 언어는 `ko-KR`, `en-US`, `ja-JP`, `zh-CN`, `vi-VN`, `th-TH`, `ar` 또는 `*`(모든 언어)다.
- `#`으로 시작하는 줄은 주석, 빈 줄은 무시한다. 빈 값은 오류다.
- 칸 구분에 공백을 쓰면 안 된다. 엑셀에서 4열로 작성해 "탭으로 구분된 텍스트"로 저장해도 된다.
- 같은 층에 같은 종류·언어·키를 두 번 쓰면 먼저 쓴 줄이 이긴다.

### 6.2 층과 우선순위

```text
요청 사전 > 사용자 > 고객 > 도메인 > 시스템 > 공통
```

| 층 | 넣는 방법 | 쓰임 |
|---|---|---|
| 시스템·공통 | 엔진이 자동 적재 | 기본 규칙과 기본 사전 |
| 도메인 | `ttf_resource_load(…, TTF_LAYER_DOMAIN)`, CLI `--domain` | 금융·통신 같은 분야 사전(`resources/domains/`) |
| 고객 | `TTF_LAYER_CUSTOMER`, CLI `--customer` | 회사별 이름·읽기 정책 |
| 사용자 | `TTF_LAYER_USER`, `ttf_lexicon_add` | 서비스 운영자의 개별 수정 |
| 요청 사전 | `ttf_dict_*`, CLI `--request-dict` | 요청·통화·고객사마다 다른 낱말 (6.5절) |

### 6.3 자주 쓰는 종류

| 종류 | 키 → 값 | 예 (기본 → 적용 후) |
|---|---|---|
| `exact` | 입력 그대로 → 출력 | `알쓰 모임` → `알콜 쓰레기 모임`, `🍔👌` → `햄버거 좋아`, `2NE1`(en-US 행) → `two N E one` |
| `lexicon` | `원어:낱말` → 읽기 | `Qorvex 신제품`: `코벡스` → `코르벡스`. 여러 낱말 이름도 된다(`en-US:Bank of America`) |
| `pattern` | `{digits}`가 든 틀 → 출력 | `내선2034로`: `내선이천삼십사로` → `내선 번호로` |
| `policy` | 정책 이름 → 값 | `range_mode digits`, `range_separator ␣번에␣` → `123-456`이 `일이삼 번에 사오육` |
| `identifier_label` | 앞말 → `digits`/`disabled` | `사번`, `주문코드` 뒤의 수를 한 자리씩 읽기 |
| `address_policy` | 주소 읽기 방식 | 주소 속 수를 자리 단위로, `-`를 `다시`로 |
| `unit`, `numeric_suffix`, `currency` | 단위·통화 표기와 읽기 | 새 단위 기호, 통화 이름 |

**exact와 lexicon의 차이:**
- `exact`는 문장 언어 기준으로 **입력을 출력으로 그대로 바꾼다.** 이모지, 줄임말, 여러 어절(`알콜 쓰레기` → `알쓰`), 같은 언어의 낱말(영어 문장의 영어 낱말) 모두 된다. 출력은 다시 읽지 않는다(`*` 행의 `two N E one`은 한국어 문장에서도 그대로 나온다).
- `lexicon`은 **다른 언어 낱말의 읽기**다(한국어 문장 속 `Apple` → `애플`). 같은 언어 낱말에는 쓰이지 않는다.

**맞추는 규칙 (exact):**
- 적은 그대로만 맞는다. `알쓰는`, `알쓰라서`처럼 한글 뒤에 조사·어미가 붙으면 맞지 않으니 필요한 형태를 따로 넣는다.
- 대소문자를 구분한다(`lol`과 `LOL`은 다른 키).
- 띄어쓰기도 키의 일부다(`알콜 쓰레기` ≠ `알콜␣␣쓰레기`).
- 문자가 바뀌는 경계에서는 맞는다(`2NE1의`, `#알쓰`).
- 같은 층·같은 길이면 문장 언어로 적은 행이 `*` 행보다 먼저다.
- 숫자 문법 대신 exact를 대량으로 넣지 않는다. 문맥과 상관없이 바뀐다.

### 6.4 검증과 확인

```sh
ttf validate-resource --resources resources --resource-layer customer --require-complete-validation acme.tsv
ttf normalize --customer acme.tsv '알쓰 모임'
ttf inspect --customer acme.tsv 'Qorvex 신제품'      # 어느 규칙·어느 층이 적용됐는지
```

`ttf_resource_load`는 파일 하나를 통째로 받거나 통째로 거부한다(일부만 들어가지 않음). 정책 종류의 이름과 값은 [resource-editing-guide.ko.md](resource-editing-guide.ko.md), [resources.md](resources.md), 사전 관리는 [lexicon-maintenance.ko.md](lexicon-maintenance.ko.md)를 본다.

### 6.5 요청 사전

고정된 엔진은 그대로 두고 요청 하나에만 `exact`·`lexicon` 행을 얹는다. 큰 사전은 엔진 층에, 요청마다 다른 작은 사전은 요청 사전에 둔다.

```c
ttf_dict_t *dict;
ttf_dict_create(&dict);
ttf_dict_load(dict, "tenant-acme.tsv");                    /* exact·lexicon 행만 */
ttf_dict_add_exact(dict, "ko-KR", "2NE1", "투에니원");
ttf_normalize_with(engine, dict, text, length, &result);
/* 스트림: ttf_stream_create 다음, 첫 write 전에 ttf_stream_set_dict(stream, dict) */
ttf_dict_destroy(dict);
```

1,000행까지는 속도가 사전 없을 때와 같고, 100,000행이면 약 20% 느려진다. 상세: [request-dictionary.ko.md](request-dictionary.ko.md).

## 7. 문장 안에서 읽기 지정 (annotated 입력)

`input_format = annotated`인 엔진에서는 요청 문장 안에 읽기를 직접 적을 수 있다. 사전을 만들지 않고 그 자리만 바꿀 때 쓴다.

| 입력 | 출력 |
|---|---|
| `1{첫}번째` | 첫번째 |
| `2NE1{two N E one} released` | two N E one released |
| `21{@native}` | 스물하나 |
| `[[서울시 12-3]]{@address}` | 서울시 십이 다시 삼 |
| `[[abc@example.com]]{@email}` | 에이비씨 앳 이그잼플 닷 컴 |

상세: [input-format.md](input-format.md), [explicit-pattern-controls.ko.md](explicit-pattern-controls.ko.md).

## 8. 도구

- **CLI:** `ttf normalize|inspect '문장'`, `ttf stream FILE`, `ttf validate-resource FILE`. 옵션은 `--language`, `--output-mode`, `--customer`, `--domain`, `--request-dict`, 5절의 옵션 이름. `inspect`는 구간마다 언어·규칙·사전 층을 보여준다.
- **웹 워크벤치:** `tools/web/server.py`(포트 8000). 정규화, 구간 확인, 사용자 사전·고객 정책 편집, 스트림 조각 검증. 상세: [web-workbench.md](web-workbench.md).

## 9. 자주 묻는 것

- **사전을 넣었는데 안 바뀐다.** 탭이 아닌 공백으로 칸을 나눴는지, 키에 조사가 붙은 형태가 따로 필요한지, 대소문자가 같은지, 행의 언어가 그 문장의 언어인지(영어 문장이면 `en-US` 또는 `*`) 확인한다. `ttf inspect`가 적용된 규칙을 보여준다.
- **한국어 엔진인데 영어 문장이 영어로 나온다.** 문장 언어는 문장마다 낱말로 정해서, 영어 문장은 영어 팩이 영어 읽기로 낸다(`Meeting at 3 PM` → `Meeting at three p m`). 한국어 문장 속 영어 낱말은 한글로 읽는다(`Apple은` → `애플은`).
- **고정 뒤에 사전을 바꾸고 싶다.** 엔진 사전은 새 엔진으로 교체하고, 요청마다 다른 것은 요청 사전을 쓴다.
- **결과 메모리는 누가 해제하나.** 성공한 `ttf_result_t`는 호출한 쪽이 `ttf_result_free`로 해제한다. 스트림 콜백의 `segment` 포인터는 콜백 안에서만 유효하다.
