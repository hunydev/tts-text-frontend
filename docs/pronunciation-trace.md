# 발음 경로와 리소스 layer 추적

v0.15는 **source 발음 리소스 layer**를 기존 ABI 안에서 전달한다.
Customer `pronunciation` 사전으로 만든 음소가 `common`으로 표시되던 오류를 수정한다.
이 문서는 source 단어→음소와 target 음소→표기를 구분한다. Target mapper의 개별
규칙 출처나 사전 행 번호까지 제공하는 전체 trace는 아직 구현하지 않았다.

## inspect 필드의 의미

| `lexicon` 경로 | `pronunciation_layer` 의미 |
| --- | --- |
| `lexicon:system`, `lexicon:customer` 등 | 최종 표기를 직접 제공한 **target pronunciation lexicon**의 layer |
| `pronunciation-dictionary` | source `pronunciation` 또는 `initialism_pronunciation`에서 선택한 행의 layer |
| `grapheme-g2p-prototype` | 실제 음소 생성에 사용한 source `g2p` 행들 중 가장 높은 layer |
| `alnum-initialism` | 사용한 source `digit_pronunciation` 행들 중 가장 높은 layer; 문자 이름은 생성된 symbolic phones |
| `initialism` | source symbolic phones가 알고리즘으로 생성됨. `common`은 리소스 행이 없다는 기본값 |
| `grapheme-fallback` | target 문자 fallback 경로. 현재 ABI에서 그 mapper layer는 미보고(`common`) |
| 기존 외부 pack의 자유 형식 provenance | 기존 문자열 유지. source layer가 제공되지 않아 `common` |
| `phrase:…` | 각 부분 단어의 성공 경로가 보고한 layer 중 최댓값. 부분별 provenance 목록은 아님 |

`resource_layer`는 **Semantic IR 분류 규칙**의 layer이며 발음 출처와 별개다.
`source_lang`/`context_lang`/`render_lang`도 그대로 유지한다.

`common` 값을 항상 “Common 파일에서 찾았다”로 해석하면 안 된다. ABI v1에는
별도의 unknown/not-applicable layer 값이 없으므로 생성된 음소, 미보고 경로,
실제 Common 행이 같은 값을 사용할 수 있다. `lexicon` 경로와 함께 해석한다.
`pronunciation_layer`는 source와 target의 모든 행을 합친 우선순위나 신뢰도 점수가 아니다.
특히 Customer target `phone_vowel` 행으로 표기를 바꾸더라도 source G2P가 Domain이면
`grapheme-g2p-prototype`의 layer는 계속 `domain`이다.

## 사용자 편집 예

다음 네 필드는 실제 탭으로 구분한다.

```tsv
pronunciation	en-US	abacus	AE B AH K AH S
initialism_pronunciation	en-US	us	AH S
```

```sh
build/ttf inspect --resources resources --customer /path/to/customer-pronunciation.tsv \
  'abacus는 단어입니다.'
```

`abacus` span은 `lexicon=pronunciation-dictionary`,
`pronunciation_layer=customer`를 보고한다. 이는 Customer에서 선택한 음소의 출처를
검증하는 예이며, fallback Hangul 표기를 독립적으로 검수한 정답이라는 뜻은 아니다.

일반 소문자 단어 `us`와 `U.S.` 같은 initialism은 서로 다른 사전 namespace를 사용한다.
`pronunciation en-US us …`만 추가하면 `U.S.`의 발음은 바뀌지 않는다.
명시적인 `initialism_pronunciation` 행은 해당 initialism 음소를 대체한다.

음소 사전보다 최종 표기의 우선순위가 높다:

```tsv
lexicon	ko-KR	en-US:abacus	사용자검수표기
```

User > Customer > Domain > System target lexicon을 먼저 조회한다. System target
lexicon도 Customer source pronunciation dictionary보다 먼저 적용된다. 이것은
pipeline 단계의 순서이며, 같은 namespace 안에서의 리소스 layer 순서와 다르다.

## ABI v1 optional provenance envelope

`ttf_pronunciation_t`나 pack descriptor의 크기와 callback signature는 바꾸지 않는다.
기존 `char provenance[64]`에 다음의 선택적인 envelope를 전달한다.

```text
ttf-pron-v1:<layer>:<path>
```

- 전체 문자열은 NUL 포함 64바이트 안에 들어가야 한다.
- `<layer>`는 소문자 `common`, `system`, `domain`, `customer`, `user` 중 하나다.
- `<path>`는 비어 있지 않은 ASCII 식별자 `[a-z0-9][a-z0-9._-]*`다.
- 예: `ttf-pron-v1:customer:pronunciation-dictionary`.
- `ttf-pron-` prefix 전체를 protocol용으로 예약한다. 알 수 없는 version, 잘못된 layer,
  빈 경로, 추가 구분자, 비ASCII 경로, NUL 없는 고정 버퍼는 `TTF_ERESOURCE`다.
- Core는 성공한 source pronunciation의 envelope를 검증한 뒤 target mapper를 호출한다.
  오류를 숨기고 문자 fallback으로 진행하지 않는다.
- `phones[1024]`, `alphabet[24]`, `provenance[64]` 모두 NUL 종료되어야 한다.
- 예약 prefix를 쓰지 않는 기존 문자열은 그대로 허용하고 표시한다. 새 envelope를
  사용하는 경로도 inspect에서는 prefix를 제거한 기존 `<path>` 문자열을 표시한다.
- Source phones를 target mapper가 거절하거나 빈 출력을 반환하면 그 임시 출력을
  되돌리고 문자 fallback을 적용한다. 실패한 source layer를 성공한 fallback에 붙이지 않는다.

Source pack은 `ttf_resource_get(..., &layer)`로 **선택된 행**의 layer를 받아야 한다.
G2P에서 더 긴 Domain 항목이 선택되었다면 소비되지 않은 짧은 Customer 항목은 출처에
포함하지 않는다. 여러 행을 소비한 경우에만 그 source 행들의 최댓값을 보고한다.

기존 바이너리 pack의 unprefixed provenance는 새 runtime에서도 동작한다. 반대로
이 envelope를 모르는 구 runtime은 문자열을 그대로 표시하고 layer를 추출하지 못한다.
이것은 구조체 ABI 변경이 없는 추가 의미 계약이며, 신뢰된 native callback을 실행하는
데 필요한 일반적인 memory safety 책임을 없애지는 않는다.

## 검증과 남은 범위

- `tests/unit/test_pronunciation_provenance.c`: 언어에 종속되지 않은 custom callbacks,
  모든 layer, legacy 경로, malformed envelope, 고정 버퍼 종료 검사,
  실패한 target mapper의 부분 출력 rollback, lexicon 우선순위.
- `tests/integration/test_pronunciation_trace.py`: ordinary/initialism 사전, G2P,
  alnum digit, 사용되지 않은 높은 layer, target mapper override,
  User lexicon 우선순위와 모든 두 조각 UTF-8 byte split의 text/metadata 일치.

Target phone/grapheme mapper가 사용한 파일·키·행, source 사전의 복수 발음 variant와
선택 이유, phrase의 단어별 provenance는 여전히 별도의 trace 확장이 필요하다.
현재 `const ttf_pronunciation_t *`를 받는 target callback에는 trace 출력 parameter가
없다. 그 정보를 전달하려고 const를 제거하거나 descriptor 뒤에 필드를 몰래 추가하지
않는다. 향후 size/version negotiation이 있는 추가 API로 확장할 항목이다.
