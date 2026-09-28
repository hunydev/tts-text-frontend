# 전화번호 문맥과 선행 0 숫자 읽기

2026-09-28 사용자 결정에 따른 KO/EN 자동 TN 정책이다. 음성 평가는 하지 않는다.
전화번호의 실제 할당·개통 여부를 검증하는 기능은 아니다.

## 이번에 바꾼 것

기존 스캐너도 `015`를 금지하지 않았다. 실제 누락은 전화 형태 뒤의 `번으로`,
`번호로`를 `번` 수량 접미사로 판단해 후보 전체를 보존한 데서 생겼다.
전화 형태가 성립한 뒤에는 리소스가 선언한 번호 꼬리를 허용한다. `kg` 같은
수량 꼬리와 잘못된 전화 구조의 방어는 유지한다. 또한 완성된 전화번호가 뒤의
`만` 조사나 `조`로 시작하는 이름 때문에 한국어 공유 배율 범위로 해석되지 않게 한다.

`015` 예외 목록은 없다. 기존 두 자리/세 자리 지역번호와 국제 전화 문법을
공유하며 `02-2345-6789`, `088-234-6789`, 영어 `212-555-0100`, `+1` 등으로
검증한다. 국가별 번호 체계를 모두 지원한다는 뜻은 아니다.

| 입력 | 기본 읽기 |
|---|---|
| `015-2345-6789 번호` | `공일오 이삼사오 육칠팔구 번호` |
| `02-2345-6789 번호` | `공이 이삼사오 육칠팔구 번호` |
| `0046` (한국어) | `공공사육` |
| `09` / `0210` (한국어) | `공구` / `공이일공` |
| `0046` (영어) | `zero zero four six` |
| `09:05` | `아홉 시 오 분` |
| `0046원` | `사십육 원` |
| `01~02만 원` | `만 원에서 이만 원` |

선행 0 정책은 **부호·구분자·단위 없는 두 자리 이상 숫자열**에 적용한다.
한국어는 `공`, 영어는 `zero`로 모든 자리를 보존한다. 전각 숫자는 기존 폴딩 경로를
통해 같은 정책을 적용한다. 단독 `0`, 소수, 날짜, 시간, 금액, 단위, 범위,
부호 있는 수, 쉼표로 묶은 수량은 각 구조의 읽기를 따른다. 코드 `k007`,
명시 금융 식별자, 버전 등에도 각각 기존 문법과 읽기 정책이 있다.

IR은 `DIGIT_SEQUENCE`, `value`는 선행 0을 포함한 문자열, `unit=leading_zero`,
규칙은 `syntax.leading-zero-digits`다. 금융 식별자의 `unit=financial_identifier`와
구분한다. 수량 범위 내부의 계수는 수량으로 해석하므로 `01~02만 원`의 `01`을
별도 식별번호로 읽지 않는다. 숫자를 실수/정수로 변환해 자릿수를 잃지 않는다.

## 사람이 편집할 리소스

아래는 UTF-8 TSV이며 열 사이에는 실제 탭을 사용한다. Domain/Customer overlay로
같은 키를 재정의하면 C 재빌드 없이 선택할 수 있다.

| 파일 | kind / language / key | 기본값 |
|---|---|---|
| `resources/common/numeric-policy.tsv` | `numeric_policy / * / leading_zero_min_digits` | `2` |
| `resources/languages/ko-KR/rules/numeric.tsv` | `number / ko-KR / leading_zero_zero` | `공` |
| `resources/languages/ko-KR/rules/phone.tsv` | `phone_number_tail / ko-KR / 번` | `1` |
| 위 phone.tsv | `phone_render / ko-KR / separator` | 공백 한 칸 |

`leading_zero_min_digits`는 2–95 사이 정수다. `7`이면 이전처럼 짧은 숫자열은
기수로 읽는다. `leading_zero_zero=영`이면 `0046 → 영영사육`이다.
`phone_number_tail=0`은 해당 꼬리에 대한 이전 수량 방어를 복원한다.
전화 그룹 사이를 ‘다시’로 읽으려면 `phone_render / separator`를 ` 다시 `로
재정의한다. 앞뒤 공백을 보존한다. 기본값은 그룹 사이 공백이며, 제공 참조에
맞추기 위해 기본 연결어를 일괄 변경하지 않았다.

```sh
printf 'number\tko-KR\tleading_zero_zero\t영\n' > /tmp/ttf-zero.tsv
build/ttf validate-resource --resources resources /tmp/ttf-zero.tsv
build/ttf inspect --resources resources --customer /tmp/ttf-zero.tsv '0046'
```

전화 0 읽기(`phone_render / zero`), 금융 식별자 읽기, 일반 선행 0 읽기는 별개다.
기존 리소스 root에 새 정책이 없으면 분류 임계값은 종전 7자리이며 한국어 0 읽기도
기존 `digit / 0`로 fallback한다. 새 배포에는 manifest와 새 TSV를 함께 설치한다.

한 번의 입력에서 명시적으로 기수를 원하면 annotated 모드에서
`0046{@cardinal}` 또는 `0046{@sino}`를 사용한다. `@digits`는 별도 명시 자리 읽기
계약이며 이번 자동 정책으로 발음을 바꾸지 않았다. ‘둘일곱둘둘’처럼 화자 의도가
필요한 번호 읽기까지 자동으로 결정하지 않는다. [입력 규격](input-format.md) 참고.

## 회귀와 측정 방법

- `tests/evaluation/phone_zero_v22.jsonl`: 신규 53건. 번호 길이·국가·꼬리·수량 부정,
  선행 0·전각·영어·명시 제어·공유 배율 조합을 포함한다.
- `tests/integration/test_phone_zero.py`: 기본/짧은 분절 두 설정에서 모든 두 조각
  byte boundary, 한 바이트 입력, 고정 난수 분할을 검사한다. text/span/trace 동일성을
  확인하며 Customer 리소스 재정의도 검증한다.
- 사용자 정책이 바뀐 기존 참조는 [변경 전후 원장](audit/phone-zero-policy-20260928.json)에
  기록했다. 외부 제공 참조는 수정·제외하지 않는다. 과거 `leading_zero_v21`의
  `short-...-unchanged` ID는 이력 연결을 위해 유지하지만 이전 정책을 뜻하지 않는다.
- 신규 음성학적 정답을 사람 검수 완료로 표시하지 않는다. 자체 계약의 초기 `123-456`
  기대값 한 건은 기존 baseline의 `에서` 정책과 충돌한 저작 오류로 정정했다.
  수정 전·후와 baseline 근거를 `artifacts/tn/phone-zero/reference-authoring-correction.json`에
  남겼다. 공유 배율 6개 추가 계약은 해당 수정 전에 고정했다.
- 제공 한국어 Training 자료는 엄격 일치와 공백 제거 일치를 함께 보고한다. `영/공`,
  전화 그룹 연결어, 식별번호 읽기 선호의 불일치도 raw 점수에 남긴다. held-out 정확도나
  실제 번호 유효성 점수가 아니다. 실행 수치와 산출물은 [HANDOFF](HANDOFF.md) 참고.

중간 candidate의 `01~02만 원` 계수 해석 문제는 독립 부정 계약으로 보완했다.
`candidate-v2`가 최종 검증 대상이며 실패 중간안은 발행하지 않는다.
