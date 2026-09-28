# 전화번호와 금융 식별자 리소스 편집

v0.18은 전화번호의 `+`, 선행 0, 구분 그룹과 내선 값을 Semantic IR에 보존한다.
이 기능은 텍스트 읽기 정책이다. 국가별 번호 할당, 실제 개통 여부, 계좌번호의
기관·체크섬 유효성을 검증하지 않는다. 아래 기본 읽기 또한 독립적인 언어
검수가 완료되었다는 뜻은 아니다.

## 전화번호와 식별자의 구분

전화번호는 `PHONE`으로 분류하고 각 숫자를 읽는다. 국제 표기의 `+`는 별도
plus label로 읽으며, 숫자 그룹 구분과 내선도 분리한다. IR의 `value`에는
선행 `+`, 모든 숫자와 canonical `-` 그룹을 담고, `secondary`에는 내선
숫자를 담는다. 새 전화 grammar의 `unit`은 `international` 또는 `national`이다.
숫자값으로 변환하지 않으므로 선행 0이 사라지지 않는다. 기존 `syntax.phone`
형식은 호환성을 위해 빈 `unit`을 유지하지만 읽기 정책은 같은 `phone_render`를
사용한다. 따라서 zero/separator 수정은 기존 형식과 새 형식에 모두 적용된다.

`Account`, `Account number`, `Reference`, `Transaction ID` 등의 활성화된
명시적 cue 뒤에 오는 숫자/하이픈 그룹은 `DIGIT_SEQUENCE`다. `unit`은
`financial_identifier`, `secondary`는 소문자 접기를 한 matched cue이며,
`value`는 원래 숫자와 하이픈을 유지한다. 기존 한국어 금융 식별자 cue와
읽기 정책도 계속 사용한다. 명시적 식별자 cue는 전화번호 추정보다 우선한다.
금액과 일반 숫자를 모두 digit sequence로 바꾸는 정책은 아니다.

`(56 km/h)`나 `1385 (2006)`처럼 괄호가 있다는 이유만으로 전화번호로
분류하지 않는다. 전화 cue 또는 전화 형태의 숫자 그룹이라는 근거가 필요하다.
반대로 전화 형태로 인식한 입력이 손상되어 있으면 일부 그룹만 수량으로
읽지 않고 해당 후보를 보존한다. 일반 분수·범위·부호 있는 수의 의미는
각 숫자 grammar가 계속 처리한다.

라벨 없는 괄호 표기의 초기 인식 범위는 보수적이다. 첫 숫자 그룹은 3자리
이하, 전체 숫자는 7자리 이상이며, 괄호 밖 subscriber 그룹이 필요하다.
그룹이 세 개 이상이거나, 두 그룹이면 두 번째가 6자리 이상이어야 한다.
닫는 괄호가 없을 때는 하이픈 근거가 있는 손상 후보만 보존한다. 명시적인
전화 label은 이 추정 조건을 우회하지만 전체 숫자 개수 제한은 그대로 적용한다.
국가별 지역번호 규칙 전체나 단축번호를 지원한다는 의미는 아니다.
라벨과 국제 `+`가 없는 국내 하이픈 표기는 기존 세 그룹 형태를 유지한다.
네 그룹 이상은 ISBN 같은 식별번호와 혼동하지 않도록 전체 보존하며,
명시적인 전화 label이나 국제 `+`가 있을 때만 확장 그룹을 허용한다.

### 번호 꼬리와 맨 선행 0 숫자 (2026-09-28)

확정된 전화 형태 뒤의 `번`, `번호`는 수량 접미사 때문에 우회하지 않는다.
`phone_number_tail / ko-KR / 번 = 1`로 선언하며 `0`으로 끌 수 있다.
키는 공백 없는 UTF-8 문자열, 96 byte 미만이다. 임의 앞자리 예외가 아닌
기존 전화 구조에 대한 꼬리 정책이므로 서울 `02`, 세 자리 지역번호 및 영어
전화 구조를 그대로 사용한다. 숫자 개수·그룹·수량 단위 검사는 유지한다.

별도로 맨 `0046`, `09`는 자리 읽기(`공공사육`, `공구`)를 기본값으로 한다.
수량과 날짜·시간에는 이 정책을 덮어쓰지 않는다. `공/영`, 이전 임계값,
명시 기수 제어 및 검증 범위는 [선행 0 정책](phone-zero-policy.ko.md)에 있다.

## 편집할 파일

| 파일 | 역할 |
| --- | --- |
| `resources/common/phone.tsv` | 전화번호/내선의 숫자 개수 제한 |
| `resources/languages/en-US/rules/phone.tsv` | 영어 전화 cue, extension marker, 읽기 label |
| `resources/languages/ko-KR/rules/phone.tsv` | 한국어 전화 cue, 내선 marker, 읽기 label |
| `resources/languages/en-US/rules/identifiers.tsv` | 영어 금융 식별자 cue와 digit 읽기 |
| `resources/languages/ko-KR/rules/identifiers.tsv` | 기존 한국어 금융 식별자 정책 |

TSV의 네 필드는 `kind`, `language`, `key`, `value` 순서다. 실제 파일에서는
탭으로 구분한다. 기본 파일을 직접 고치거나 Domain/Customer overlay로
동일 key를 재정의할 수 있다. 배포별 편집은 overlay를 권장한다.

### 전화 cue와 내선 marker

`phone_label` 값은 `1`(사용) 또는 `0`(사용하지 않음)이다. 기본 영어 cue는
`phone`, `tel`, `telephone`, `call`, `call me at`, `call us at`, `contact`,
`telephone number`, 한국어 cue는 `전화`, `전화번호`, `문의`, `연락처`,
`대표전화`, `대표 전화`, `문의전화`, `문의 전화`, `전화번호는`, `연락처는`다. ASCII 대문자는 입력에서 소문자로 접으므로 리소스에는
소문자로 쓴다. 내부 공백은 한 칸으로 쓰며 앞뒤 공백이나 줄바꿈을 넣지 않는다.
key는 96 byte 미만이다.

`phone_extension` 값도 `1` 또는 `0`이다. 기본 marker는 영어의
`ext.`, `ext`, `x`, 한국어의 `내선`이다. marker는 32 byte 미만이며
공백이 없어야 한다. `ext.`처럼 끝에 마침표가 오는 형식은 허용한다.
특정 cue를 사용하지 않게 해도 다른 활성 cue 또는 구조적인 국제 전화번호
형태까지 모두 비활성화하는 것은 아니다.

`phone_extension_hint`도 같은 marker key와 `1`/`0`을 사용한다. 구분 없는
`+숫자`는 양의 수와 구별이 어려우므로 전화 cue 없이 읽을 때 이 hint가
추가 근거가 된다. 기본값은 `ext.`, `ext`, `내선`이 1, `x`가 0이다.
따라서 `+1250000 x2`를 내선 전화로 추정하지 않는다. 이미 label 또는
형태로 전화번호가 확정된 경우에는 `phone_extension / x=1`에 따라 x marker를
사용할 수 있다. Hint는 해당 `phone_extension` marker가 활성화되어 있을
때만 작동한다.

영어 label 뒤의 neutral 숫자·기호는 `context / numeric_label_weight`(기본 24,
허용 0–1000)의 약한 영어 문맥 신호를 받는다. `Phone: +...`가 엔진의 기본
언어만 따라가는 현상을 막으면서, 주변 한국어 문장/조사 신호가 더 강하면
한국어 문맥을 유지한다. 이 값은 강제 출력 언어 설정이 아니다.

### 읽기 label

`phone_render`는 언어별 네 key를 사용한다.

| key | ko-KR 기본값 | en-US 기본값 |
| --- | --- | --- |
| `plus` | `플러스 ` | `plus ` |
| `extension` | ` 내선 ` | ` extension ` |
| `separator` | ASCII 공백 한 칸 | ASCII 공백 한 칸 |
| `zero` | `공` | `zero` |

표의 앞뒤 공백은 실제 출력에 필요한 값이다. TSV 값을 trim하지 않는다.
한국어는 같은 그룹의 숫자를 붙여 읽고, 영어는 각 숫자 사이에 공백을 넣는다.
`separator`는 그룹 사이, `extension`은 본 번호와 내선 사이에 그대로 들어간다.
각 label은 비어 있지 않은 UTF-8 문자열, 96 byte 미만이어야 하며 ASCII
제어 문자는 허용하지 않는다.

기존 한국어 `number / phone_zero`에 상위 계층의 사용자 정책이 있으면
하위 `phone_render / zero` 기본값보다 우선한다. 두 key가 같은 계층이면
새 `phone_render / zero`가 우선한다. 이전에 배포한 Customer의 영/공 정책이
새 System 기본값에 의해 무시되지 않게 하기 위한 호환 규칙이다.

`phone_onset`, `phone_vowel`, `phone_coda`는 외국어 pronunciation의 음운을
한국어로 옮기는 별도 리소스다. 전화번호 읽기를 바꾸려고 수정하지 않는다.

같은 묶음에 표기 관행을 선언하는 key가 더 있다. 전화번호와 무관하다.

| key | 값 | 뜻 |
| --- | --- | --- |
| `phone_vowel_length` | `short` \| `long` | 짧은 모음 뒤에서만 어말 `P T K`가 받침이 된다 (market 마켓, update 업데이트) |
| `phone_mapping coda_r` | `drop` \| `keep` | 자음 앞·어말의 `r`를 지운다 (market 마켓) |
| `phone_mapping onset_l` | `double` \| `single` | 모음 뒤 `l`을 받침 ㄹ + 초성 ㄹ로 겹쳐 적는다 (platform 플랫폼) |
| `phone_mapping linking_r` | `link` \| `drop` | `ER` 뒤에 모음이 오면 r를 다음 음절 초성으로 남긴다 (battery 배터리) |
| `phone_glide` | `merge` \| `split` | 자음 뒤 `W`가 같은 음절에 붙는지 (quick 퀵) 끊는지 (sweet 스위트). key는 앞 자음의 음운이고 기본값은 `split`이다. `Y`는 항상 붙으므로 이 표를 보지 않는다 |

`phone_vowel_length`의 key는 음운 이름이고, 선언하지 않은 음운은 `long`으로 본다.

매퍼가 자원 없이 적용하는 표기 규칙도 있다. 활음(`W`, `Y`) 앞 자음은 받침이 아니라
다음 음절의 초성이고(security 시큐리티), 지운 `r`는 앞 모음을 길게 만들어 뒤의 파열음이
받침이 되지 못하게 하며(smart 스마트), 어말 성절 자음은 `으` 음절의 받침이 된다
(simple 심플, prism 프리즘). g2p 데이터의 `.` token은 형태소 경계이며 앞 자음을 받침으로
닫는다(uploading 업로딩, personalized 퍼스널라이즈드).

### 숫자 개수 제한

`phone_policy`는 `language=*`만 허용한다.

| key | 기본값 | 허용 범위 |
| --- | ---: | --- |
| `international_min_digits` | 7 | 1–15 |
| `international_max_digits` | 15 | 1–15 |
| `national_min_digits` | 7 | 1–15 |
| `national_max_digits` | 15 | 1–15 |
| `extension_max_digits` | 8 | 1–95 |

min은 같은 유형의 effective max를 넘을 수 없다. Override 후의 값까지
validate/freeze 단계에서 검사한다. 국내 번호의 제한은 기존 하이픈 형식에도
동일하게 적용한다. 문법의 구조적 조건까지 이 숫자 제한으로
대체하지는 않는다. 국가 번호나 지역 번호의 실제 유효성을 보장하는 설정도
아니다.

공통 phone policy가 하나라도 있으면 등록된 KO/EN pack의 네 `phone_render`
label이 모두 있어야 freeze에 성공한다. 기존 phone policy가 없는 resource
root에서는 기존의 일반 전화번호 digit 읽기를 계속 허용한다. 새로운 국제
`+`/내선 형식은 필요한 읽기 label이 없을 때 명시적 resource 오류를 반환할 수
있으므로 새 배포에는 위 manifest 리소스를 함께 설치한다.

### 금융 식별자

`identifier_label` 값은 `digits` 또는 `disabled`다. 영어에서는 cue의 ASCII
대문자를 소문자로 접고, 내부 공백을 한 칸으로 정규화해서 조회한다. 긴 cue가
있으면 짧은 cue보다 먼저 해석하며, 긴 cue를 `disabled`로 지정하면 같은
범위의 짧은 cue가 그것을 되살리지 않는다. 예를 들어 `account number`만
끄는 정책과 `account` 전체를 끄는 정책은 구별된다.
분절 target이 짧아도 비활성 긴 cue의 범위를 함께 유지하므로,
문장 분할로 짧은 suffix cue가 다시 활성화되지 않는다. `phone_label`에도
같은 긴 cue 차단 원칙을 적용한다.

영어 `identifier_render / zero`와 `separator`를 편집해 영(0) 또는 그룹
구분 읽기를 바꾼다. `phone_render`를 바꾸어도 금융 식별자 읽기는 바뀌지
않는다. 기존 root에 identifier render가 없으면 zero는 기존 `number / 0`,
separator는 공백을 사용한다.

한국어 `identifier_render / ko-KR / separator` 기본값은 2026-09-28부터 ` 다시 `다
(`사업자번호 123-45-67890` → `일이삼 다시 사오 다시 육칠팔구공`). speech 모드에서
전화·범위 인식기가 거부한 순수 숫자·하이픈 열도 같은 값을 쓴다. 전화번호 하이픈은
`phone_render / separator`로 따로 정하며 이 값의 영향을 받지 않는다. 변경 근거와
바뀐 기존 계약은 [식별번호 대체 읽기](identifier-fallback.ko.md)와
[정책 원장](audit/identifier-separator-policy-20260928.json)에 있다.

## 적용과 검사

아래 Customer 정책은 국제 표기의 +, 내선 연결어와 영어 금융 식별자 0 읽기를
조정한다. 리소스의 공백도 그대로 생성되도록 `printf`를 사용한다.

```sh
printf 'phone_render\tko-KR\tplus\t국제번호 \nphone_render\tko-KR\textension\t 내선번호 \nidentifier_render\ten-US\tzero\toh\n' \
  > /tmp/ttf-phone-customer.tsv
build/ttf validate-resource --resources resources /tmp/ttf-phone-customer.tsv
build/ttf inspect --resources resources --customer /tmp/ttf-phone-customer.tsv \
  '전화 +82 10 1234 5678 내선 09'
build/ttf inspect --resources resources --language en-US \
  --customer /tmp/ttf-phone-customer.tsv 'Account number: 0012-0034.'
```

검사 성공은 문법·의존성·메모리 범위에 맞게 설정되었다는 뜻이다. 사람이
원한 읽기인지는 실제 출력 텍스트와 별도로 확인한다. `inspect`에서 PHONE과
financial_identifier가 구별되는지, `value`와 `secondary`에 선행 0이 남는지도
함께 확인한다. Batch와 임의 byte 분할 streaming의 결과가 같은지 웹 도구의
입력 분할 검증으로 확인할 수 있다.

[리소스 검증](resource-validation.md), [리소스 편집 안내](resource-editing-guide.ko.md),
[Semantic IR](semantic-ir.md)도 참고한다.

## 대표번호와 괄호 안 전화번호

네 자리 접두사가 `phone_service_prefix ko-KR 1588 1`처럼 선언돼 있고 뒤에 네 자리가 오면
전화번호다 (`1588-0000` → `일오팔팔 공공공공`). 선언하지 않은 `1500-1600`은 기존대로 범위다.
고객 TSV로 접두사를 추가하거나 `0`으로 끌 수 있다. 괄호가 번호 전체를 감싸면 괄호는 문장
부호로 남고 안의 번호만 읽는다 (`연락처(010-1234-5678)`). 지역번호만 괄호에 넣는
`(02) 1234-5678` 문법은 그대로다.
