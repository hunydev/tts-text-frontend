# 발음 처리의 상세 출처와 명시적 발음 선택

v0.17의 trace는 출력 텍스트가 어떤 사전 행, source phone, target mapper를
거쳤는지 확인하는 도구다. 기존 span의 `lexicon` / `pronunciation_layer` 요약은
그대로 유지하며, 필요할 때 별도의 상세 event 목록을 요청한다. 이 기록이
외래어 표기나 TTS 발음의 정답 승인을 의미하지는 않는다.

## 빠른 실행

저장소 루트에서 현재 CLI를 빌드한 뒤 실행한다.

```sh
build/ttf inspect --resources resources --trace 'foobar를 사용합니다.'
printf '%s' 'foobar를 사용합니다. Apple은 사과입니다.' | \
  build/ttf stream-json --resources resources --trace -
```

`inspect`는 기존 span 설명 뒤에 `trace=[...]`를 표시한다. `stream-json`은
각 segment에 `trace` 배열을 추가한다. `--trace`를 생략하면 기존 출력 계약을
유지한다. 모든 값은 텍스트 처리 결과이며 음성 합성이나 음성 비교는 수행하지 않는다.

웹 도구에서는 normalization 후 span을 선택하고 **발음 상세 경로**를 연다.
선택/폐기 여부, source/target 단계, 언어, 계층, kind/key/value, part 범위,
phone 번호, 선언 순서를 볼 수 있다. 입력 분할 검증의 **발음 trace** 열은
batch와 각 streaming 분할의 결정을 함께 비교한다. trace가 없는 일반 한국어
TN span이나 영어 보존 span에 발음 경로를 만들어 표시하지 않는다.

## event를 읽는 방법

| 필드 | 의미 |
| --- | --- |
| `stage` | `lexicon`, `source-pronunciation`, `target-phonetic`, `target-grapheme`, `policy` |
| `disposition` | `selected`는 채택한 경로, `rejected`는 이후 폐기한 시도의 기록 |
| `language` | 그 resource를 조회하거나 결정을 수행한 언어 |
| `resource_language` | 실제 선택한 행의 언어. wildcard 행이면 `*` |
| `kind`, `key`, `value` | 실제 소비한 resource 행 또는 명시적인 코드 생성 결정 |
| `layer_known`, `layer` | resource 계층을 알고 있는지와 그 값 |
| `declaration_order` | 해당 엔진에서 행이 선언된 순서. 파일 경로나 파일의 행 번호가 아님 |
| `text`, `detail` | 결정 관련 텍스트와 경로/선택 이유. 필드 의미는 event kind에 따름 |
| `input_begin`, `input_end` | 원본 전체 입력 기준 UTF-8 byte 범위 |
| `output_begin`, `output_end` | 그 결정을 포함하는 **전체 normalized span**의 UTF-8 byte 범위 |
| `part_begin`, `part_end` | 선택한 발음 입력의 부분 범위. 원문 surface, canonical form, verbalizer 결과 중 실제 사용한 문자열 기준 |
| `phone_index` | Pronunciation IR 안의 0부터 시작하는 phone 번호. G2P 행에서는 그 행이 만든 첫 phone |

서로 다른 리소스 버전의 `declaration_order` 절대값은 고정 참조로 비교하지 않는다.
리소스 추가로 선언 번호가 밀릴 수 있다. CJK 기본값 회귀 검사는 출력 텍스트와
번호를 제외한 **모든** trace 필드가 같고, 같은 엔진 설정·layer 내 상대 순서와
동률이 유지되며, 그 layer의 변화가 균일한 상수 오프셋일 때만 허용한다.
`tools/evaluation/trace_order.py`가 이 비교를 담당하고 실제 before/after 번호는
보고서에 남긴다. 정규화 기대값이나 원본 캡처를 고쳐 맞추지 않는다.
검사 자체의 부정 사례: `python3 tests/evaluation/test_trace_order.py`.

Batch의 output 범위는 전체 결과 기준이고 streaming에서는 해당 callback
segment 기준이다. `part_*`는 원본 입력 byte offset과 서로 더할 수 있는 값이
아니다. 예를 들어 서수 verbalizer가 여러 단어를 만들면 원래 하나의 span에
대해 각 단어의 part 범위가 기록된다. `output_*`는 음절별 정렬을 제공하지
않는다. 여러 onset/vowel/coda event가 같은 전체 출력 span을 가리킬 수 있다.
해당하지 않는 index/order는 C에서 `SIZE_MAX`, CLI JSON에서 `null`이다.

Source pronunciation과 target 표기는 별도 단계다. 예를 들어 domain의
`g2p/en-US/ba`가 `B AA`를 생성하고 customer의 `phone_vowel/ko-KR/AA`가
한국어 모음을 정하면 두 행의 계층을 각각 표시한다. 어느 한쪽의 높은 계층을
전체 발음 경로의 계층으로 오인하지 않는다.

기록 대상은 **실제로 소비한 행**이다. 긴 G2P 행이 선택되었을 때 사용하지
않은 짧은 후보나, `phone_letter`가 선택되어 쓰이지 않은 onset/vowel 조회는
selected resource로 기록하지 않는다. 보류한 자음 onset은 실제 음절에
사용할 때 기록한다. coda가 다음 phone을 소비하면 그 다음 phone의 번호를
기록한다. W/Y 처리, 삽입 모음, 이중모음 꼬리, prototype의 말미 R 생략 등
코드 결정은 `layer_known=false`로 표시한다.

`layer_known=false`의 `layer` 숫자 저장값을 common 계층의 증거로 사용하면
안 된다. CLI는 이를 `unknown`, 웹은 **코드 생성 / 미보고**로 표현한다.
기존 API의 `pronunciation_layer`는 호환성을 위한 요약이고 새 상세 기록과
구분해서 해석한다. 상세 instrumentation이 없는 외부 언어팩에서는 core가
callback의 Pronunciation IR과 성공/실패 경로까지 기록할 수 있지만 내부에서
어떤 행을 사용했는지는 알 수 없다. 그런 경로의 source/target resource를
추정해서 표시하지 않는다.

Phone mapper가 일부 텍스트를 만든 뒤 실패하면 그 source pronunciation과
부분 target 시도는 `rejected`가 되고, 최종 grapheme fallback은 별도 selected
경로가 된다. 폐기된 event는 최종 출력에 기여한 증거로 집계하지 않는다.
메모리/trace limit 오류로 요청 자체가 실패한 경우에는 그 실패를 성공한
fallback으로 취급하지 않는다.

## 명시적인 복수 발음 선택

기본 발음은 `pronunciation`, 대안은 `pronunciation_variant`, 선택 정책은
`pronunciation_choice`로 분리한다. 아래는 **재현 가능한 동작 예시**로,
`read`의 문법이나 문맥을 자동 판별하는 규칙이 아니다.

```sh
printf 'pronunciation\ten-US\tread\tR IY D\npronunciation_variant\ten-US\tread#past\tR EH D\npronunciation_choice\ten-US\tread\tpast\n' \
  > /tmp/ttf-read-variants.tsv
build/ttf validate-resource --resources resources /tmp/ttf-read-variants.tsv
build/ttf inspect --trace --resources resources \
  --customer /tmp/ttf-read-variants.tsv 'read는 예시입니다.'
```

현재 한국어 mapper의 출력은 `레드는 예시입니다.`다. Trace에는 선택 정책
`pronunciation_choice/en-US/read = past`와 실제 source 행
`pronunciation_variant/en-US/read#past = R EH D`가 별개로 나타난다.
선택 정책을 제거하면 기본 `R IY D`를 사용한다. 상위 계층에
`pronunciation_choice en-US read base`를 두어 기본 선택으로 되돌릴 수도 있다.
실제 파일은 네 필드를 **탭**으로 구분한다.

- headword는 기존 source pronunciation 조회와 같은 ASCII 소문자 접기를
  사용하고 96 byte 미만이어야 한다. choice의 headword에는 `#`를 넣지 않는다.
- variant ID는 `[a-z0-9][a-z0-9._-]*`, 1–31 byte다. `base`는 기본 선택을
  나타내므로 variant 정의 ID로 사용할 수 없다.
- 선택 정책이 없으면 기존 base 발음/default fallback을 그대로 사용한다.
  `base` 선택에 base 행이 없으면 기본 fallback으로 돌아간다.
- 선택 정책의 계층이 effective base 행보다 낮으면 base가 우선한다.
  Domain 정책이 Customer의 명시적인 base 발음을 바꾸지 않는다.
- 선택 정책의 계층이 base와 같거나 높으면 그 ID의 effective variant를
  선택한다. 상위 정책이 하위 계층의 variant를 지정하는 것은 허용한다.
- 적용 가능한 선택 정책이 가리키는 variant가 없으면 validate/freeze에서
  오류가 난다. 선택/variant 파일은 순서와 관계없이 로드할 수 있으며 최종
  freeze 전에 의존성을 충족하면 된다.
- 상위 base 때문에 적용되지 않는 낮은 정책은 variant 의존성을 만들지 않는다.
- 기존 User > Customer > Domain > System target 발음 lexicon이 source
  pronunciation과 그 선택 정책보다 앞선다.
- `INITIALISM`의 별도 `initialism_pronunciation`에는 이 정책을 적용하지 않는다.

선택 이유는 `default-base`, `explicit-base`, `explicit-choice`,
`higher-layer-base`로 구분한다. 사용하지 않은 낮은 choice 행을 선택된
정책으로 기록하지 않는다. variant ID의 숫자나 이름에서 빈도, 품사,
고유명사 여부를 추론하지 않는다.

CMUdict importer의 `--export-runtime --variant-policy all`은 기존 base와
번호 있는 variant를 함께 보존한다. 선택 정책은 자동 설치하지 않는다.
`--variant-policy base`의 기본 발음 선택 계약도 유지한다. 외부 원본과
생성된 대용량 사전은 Git에 포함하지 않으며, 별도 다운로드/라이선스·revision
검증을 거친 opt-in 리소스로 사용한다. 자세한 명령, 수량과 생략 사유의 의미는
[CMUdict importer](../benchmark/importers/cmudict/README.md)를 참고한다.

## C API와 ownership

새 API는 `tts_text_frontend/ttf_trace.h`에 있다. 기존 config/result/segment 및
language-pack descriptor의 C ABI layout을 변경하지 않았다.

```c
#include <tts_text_frontend/ttf_trace.h>
#include <stdio.h>
#include <string.h>

/* engine은 이미 freeze되어 있고 이 호출 동안 유효해야 한다. */
static ttf_status_t inspect_text(const ttf_engine_t *engine, const char *text) {
    ttf_trace_options_t options;
    ttf_traced_result_t result = {0};
    ttf_status_t status;
    ttf_trace_options_init(&options);
    status = ttf_normalize_traced(engine, text, strlen(text), &options, &result);
    if (status == TTF_OK) {
        size_t i;
        puts(result.result.text);
        for (i = 0; i < result.event_count; ++i) {
            const ttf_trace_event_t *event = &result.events[i];
            printf("%s %s %s/%s\n", ttf_trace_stage_name(event->stage),
                   ttf_trace_disposition_name(event->disposition), event->kind, event->key);
        }
    }
    ttf_traced_result_free(&result);
    return status;
}
```

`ttf_traced_result_t`의 event와 모든 문자열은 결과가 소유한다.
`ttf_traced_result_free()`가 내부 normalized result까지 함께 해제하므로
`result.result`를 별도로 해제하지 않는다. 실패한 결과도 같은 free 함수로
정리할 수 있다.

Streaming은 `ttf_stream_create_traced()`로 생성한 뒤 기존
`ttf_stream_write()` / `ttf_stream_flush()` / `ttf_stream_destroy()`를 사용한다.
callback은 `const ttf_traced_segment_t *`를 받고 `segment`와 `events`를
함께 읽는다. callback의 반환값 계약은 기존과 같으며 0은 계속 진행이다.
segment와 event의 포인터는 callback 반환 시 만료된다. 비동기 소비자는
텍스트, span, event 문자열을 소유한 저장 공간으로 복사해야 한다. Engine은
stream이 파괴될 때까지 freeze 상태로 살아 있어야 한다.

Trace 상태는 요청/stream에 속한다. 공유 frozen engine의 resource를 바꾸지
않고 요청용 engine view를 통해 pack callback에 trace 상태를 연결한다.
Pack은 callback으로 받은 engine을 빌린 포인터로 취급하고 보관하거나
파괴하면 안 된다. 같은 frozen engine을 사용하는 독립 요청이 trace를 섞지
않는다. 개별 stream 자체의 write/flush는 동시에 호출하지 않는다.

## 새 언어팩의 instrumentation

`ttf_trace_record_resource()`는 kind/language/key의 실제 effective 행을 찾아
문자열, 계층과 선언 순서를 복사한다. 조회 시도 전체를 기록하기 위해 호출하지
말고, 그 행을 실제로 소비한 위치에서 호출한다. miss는 event를 만들지 않는다.

`ttf_trace_record()`는 코드 결정용 note를 복사한다. `struct_size`를 채우고,
계층 근거가 없는 생성 결정은 `layer_known=0`, phone 번호가 없으면
`phone_index=SIZE_MAX`로 명시한다. 모든 문자열은 NUL-terminated UTF-8이어야
한다. 일반 normalization에서는 두 함수가 trace event를 만들지 않는다.
실패 상태는 요청에 남으므로 pack은 반환된 오류도 전파하는 것이 좋다.
Span/stage/part의 scope는 core가 관리한다. 현재 instrumentation 대상은
`pronounce`, `render_phones`, `render_graphemes` callback의 실제 소비 지점이다.
후보를 탐색하는 classifier/verbalizer에서 선택 전 event를 기록하지 않는다.

상세 trace는 현재 instrument한 경로를 설명한다. 외부 pack의 미보고 내부
규칙이나 모든 TN/verbalizer의 resource 소비를 자동으로 추적하는 API는 아니다.

## 메모리 제한과 오류

기본 제한은 요청 전체 **65,536 events / 16 MiB logical event payload**다.
옵션을 초기화한 후 `max_events` / `max_bytes`로 조절할 수 있다. 두 값은
양수여야 한다. Payload는 event 구조체와 복사한 문자열을 합한 크기이고,
allocator bookkeeping이나 배열 확장 중의 추가 메모리까지 포함한 RSS 제한은
아니다.

폐기된 시도도 예산을 사용한다. Streaming에서 segment를 전달하고 해당
포인터를 해제해도 누적 예산은 초기화하지 않는다. 제한을 넘으면
`TTF_ELIMIT`를 반환하며 기록을 잘라낸 채 성공으로 처리하지 않는다. 현재
미완성 segment는 callback으로 전달하지 않고, 앞서 전달한 segment는 이미
commit된 상태로 남는다. Batch 실패를 정상적인 부분 결과로 소비해서는 안 된다.
큰 문서에서 상세 trace가 불필요하면 기존 untraced API를 사용하거나 배포
환경의 진단 메모리 예산에 맞춰 제한을 명시적으로 조절한다.
