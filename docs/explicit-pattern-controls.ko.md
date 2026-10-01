# 구간을 지정하는 읽기 제어

2026-09-28. Plain 입력은 그대로 두고 **annotated 입력**에서만 사용한다.
사용자가 지정한 구간에 읽기 규칙을 적용한다. 규칙이나 사전을 영구 등록하는 기능은 아니다.

## 두 가지 지정 방법

| 목적 | 입력 | 결과 |
|---|---|---|
| 완성 발음을 직접 지정 | `1{첫}번째` | `첫번째` |
| 여러 단어의 완성 발음 | `[[Bank of America]]{뱅크 오브 아메리카}` | `뱅크 오브 아메리카` |
| 하나의 값에 규칙 적용 | `21{@native}` | `스물하나` |
| 전체 구간에 규칙 적용 | `[[서울시 12-3]]{@address}` | `서울시 십이 다시 삼` |

`[[...]]`가 적용 범위를 정하고, `{...}`가 처리 방법을 정한다. 공백·문장 부호가 있는
원문은 그룹으로 묶는다. `abc@example.com{@email}`은 마지막 `com`에만 제어가 붙으므로
잘못된 이메일로 거부된다. **`[[abc@example.com]]{@email}`**로 쓴다.

```sh
build/ttf normalize --input-format annotated --output-mode speech \
  '배송지는 [[서울시 강남구 테헤란로 123-4 101동 1203호]]{@address}입니다.'
# 배송지는 서울시 강남구 테헤란로 백이십삼 다시 사 백일동 천이백삼호입니다.

build/ttf normalize --input-format annotated --output-mode speech \
  '[[a1@b2.com]]{@email}'
# 에이일 앳 비이 닷 컴

build/ttf normalize --input-format annotated --output-mode speech \
  '[[q1/a2]]{@url}'
# 큐일 슬래시 에이이
```

웹에서는 **발음 제어 표기 사용**을 켠다. 예문 선택에 주소·이메일·URL 사례가 있으며,
`grouped-controls-v22` 코퍼스로 정답과 비교할 수 있다. 청크 검증은 구분자·한글이
여러 write로 나뉘어도 결과와 metadata가 같은지 검사한다.

## 지원 범위

- `@address`: 한국어 도로명·지번 및 건물·동·호의 **명시적 읽기**. 지명은 원문 한글을
  유지하고 숫자는 한자어 수사로 읽는다. 숫자 사이 `-`는 `다시`이며 일반 금액/수량 범위의
  `에서` 정책은 바뀌지 않는다. `101동 1203호` → `백일동 천이백삼호`.
- 선행 0은 보존한다: `01동 002호` → `영일동 영영이호`. 일반 숫자/연도/전화 정책으로
  재분류하지 않는다. 선행 0 없는 우편번호를 자릿수로 읽으려면 고객 정책이나 직접 발음을
  지정한다. ASCII 글자는 글자 이름이다 (`B동` → `비동`).
- `@email`: 한국어·영어. `a1@b2` 같은 내부 호스트도 허용하며 TLD 목록에 의존하지 않는다.
  한 개의 `@`, 비어 있지 않은 local/host, 점과 하이픈의 기본 경계는 검증한다.
- `@url`: 한국어·영어. 상대 경로나 내부 URL처럼 자동 인식이 놓치는 표기도 전체를 읽는다.
  기존 `electronic_word`·발음 사전·기호 읽기를 재사용한다. 링크에 접속하지 않는다.
- `@native`, `@sino`, `@alphabet`도 그룹으로 쓸 수 있지만 각 원문 제약은 유지한다.
  `[[ABC]]{@alphabet}`은 가능하고 `[[AB CD]]{@alphabet}`은 공백 때문에 오류다.

명시적인 제어는 text/speech **두 출력 모드 모두 읽기를 수행**한다. 자동 인식된
이메일·URL의 text 모드 원문 보존 정책과 구분한다. 직접 지정한 발음/보호된 코드는
재해석하지 않는다. 주소의 실재성 확인, 영어 주소, 국제화 이메일·URL, 따옴표 local-part,
주소 표기의 모든 관용 독음은 이번 지원 범위가 아니다. 예외 지명은 literal 제어를 쓴다.

## 사람이 수정하는 주소 정책

기본 파일은 `resources/languages/ko-KR/rules/controls.tsv`다. 고객 TSV에서 다음 세 종류를
덮어쓸 수 있다. 열 사이에는 **실제 탭**을 넣는다.

```tsv
address_policy	ko-KR	number	cardinal
address_policy	ko-KR	leading_zero	digits
address_policy	ko-KR	hyphen	다시
```

`number`, `leading_zero`는 `cardinal|digits`, `hyphen`은 비어 있지 않은 95 UTF-8 byte
이하 읽기다. 예를 들어 number=digits, hyphen=번에 이면 `[[서울시 12-3]]{@address}`가
`서울시 일이 번에 삼`이 된다. Customer > Domain > System이며 같은 layer는 먼저 선언한
행이 우선한다. 이메일·URL은 기존 `electronic_word`, `speech_symbol`, 발음 사전을 사용한다.

```sh
build/ttf validate-resource --resources resources --require-complete-validation customer.tsv
build/ttf inspect --input-format annotated --output-mode speech --trace \
  --customer customer.tsv '[[서울시 12-3]]{@address}'
```

## 길이·오류·스트리밍 계약

- 그룹 중첩은 불가하다. 예약 문자는 기존 `\\`, `\{`, `\}`, `\[`, `\]`로 이스케이프한다.
- 제어 전체는 기본 4096 UTF-8 byte 제한이며 `max_control_bytes`로 설정한다. 그룹은
  95byte IR 필드보다 길 수 있다. 긴 원문을 잘라서 다른 의미로 읽지 않는다.
- 묶음 없이 쓴 숫자 atom은 숫자 사이의 `- + * / = :` 하나를 포함한다(`4-3{@math}`,
  `10:10{@score}`, `2026-10-01{@date}`). 글자가 섞이거나 공백이 있는 식은 `[[...]]`로 묶는다.
- 기존 단일 atom의 95byte 제한은 유지한다. 주소의 개별 숫자는 cardinal 최대44자리,
  digits 최대95자리. 전각 숫자·지원하지 않는 문자/제어/형식은 오류로 알린다.
- 열린 그룹은 pending 상태다. 닫힌 후 하나의 구간으로 처리하므로 그룹 내부에서는
  callback을 쪼개지 않는다. 앞서 확정된 문장은 먼저 callback한다. 미완성 상태에서
  flush하면 오류이며, 이미 전달된 앞 문장을 되돌리지는 않는다.
- 문맥은 기존 주변 문장/기본 언어를 따른다. 그룹 내부 내용이나 `@address` 문자열이
  주변 문장의 언어 점수에 투표하지 않는다. 영어 문맥의 `@address`는 현재 지원하지 않는다.

## 구현 결정과 확장 계약

새 문법이나 공개 ABI를 만들지 않고 기존 그룹 파서와 `CUSTOM` IR, language-pack의
`verbalize` 콜백을 확장했다. `unit=@address|@email|@url`, `rule=control.pattern`으로
명시적인 요청임을 표시한다. EMAIL/URL로 자동 판별됐다고 메타데이터를 꾸미지 않는다.

그룹의 `secondary=control-surface`는 콜백 `surface/length`에 디코딩한 전체 원문이 있다는
뜻이다. 원문이 들어가면 `value`에도 보존하고, 95byte를 넘으면 value는 빈 문자열이다.
버퍼는 호출 중에만 유효하고 trace의 `input-control.text`는 전체 원문을 보존한다.
원문 offset은 구분자와 제어까지 포함한다. resource trace의 part 범위는 실제 읽는
디코딩 원문 또는 숫자/철자 하위 입력을 기준으로 한다.

Core는 이름 문법과 구간만 처리한다. 새 패턴의 원문 유효성·읽기는 해당 pack에서 추가한다.
`@address`의 숫자는 CARDINAL/DIGIT_SEQUENCE IR을 거쳐 기존 수사를 재사용하며 재귀 TN을
호출하지 않는다. 새로운 이름을 추가할 때는 독립 참조, 잘못된 값, 보호 구간, byte 분할,
옵션/리소스 우선순위, 긴 원문 및 명시적 오류 계약을 함께 추가한다.

## 의미를 지정하는 예약어 20개 (2026-09-28 추가)

자동 인식이 모호하거나 주변 단서가 없는 경우, **값의 해석**을 지정한다. `9/25`를
날짜 대신 분수로, `1984`를 일반 숫자 대신 연도로 읽게 할 수 있다. 단위·통화는 원문에
써야 한다. `[[12]]{@currency}`처럼 통화를 알 수 없는 입력에 임의의 원화를 붙이지 않는다.
기존 여섯 예약어와 합쳐 **26개**다. 이름은 소문자이며 다른 철자의 별칭은 제공하지 않는다.

| 예약어 | 허용 값 / 예 | 한국어 출력 또는 설명 |
|---|---|---|
| `@cardinal` | `[[09]]{@cardinal}` | `구` (일반 수량; 정수만) |
| `@digits` | `[[0210]]{@digits}` | `영이일영` (선행 0 포함 모든 자릿수) |
| `@decimal` | `[[12.50]]{@decimal}` | `십이 점 오영` (소수부 0 유지) |
| `@ordinal` | `[[21]]{@ordinal}` | `제이십일`; 영어 `twenty first` |
| `@year` | `[[1984]]{@year}` | `천구백팔십사 년`; 영어 `nineteen eighty four` |
| `@date` | `[[2026-09-16]]{@date}` | `이천이십육 년 구 월 십육 일` |
| `@date-mdy` | `[[03/04/2026]]{@date-mdy}` | `이천이십육 년 삼 월 사 일` |
| `@date-dmy` | `[[03/04/2026]]{@date-dmy}` | `이천이십육 년 사 월 삼 일` |
| `@time` | `[[09:05]]{@time}` | `아홉 시 오 분` |
| `@currency` | `[[12,500원]]{@currency}` | `만이천오백 원` |
| `@percent` | `[[12.5%]]{@percent}` | `십이 점 오 퍼센트` |
| `@measure` | `[[3kg]]{@measure}` | `삼 킬로그램` |
| `@counter` | `[[3개]]{@counter}` | `세 개` (한국어 팩 전용) |
| `@fraction` | `[[9/25]]{@fraction}` | `이십오 분의 구` |
| `@range` | `[[1~2kg]]{@range}` | `일 킬로그램에서 이 킬로그램` |
| `@score` | `10:10{@score}`, `[[40-30]]{@score}` | `십 대 십`, `사십 대 삼십`; 영어 `ten to ten` |
| `@math` | `4-3{@math}`, `[[12-3+5]]{@math}` | `사 빼기 삼`, `십이 빼기 삼 더하기 오`; 영어 `four minus three` |
| `@phone` | `[[0210-09]]{@phone}` | `공이일공 공구` (국번 자동 인식 불필요) |
| `@version` | `[[12.03.4]]{@version}` | `십이 점 영삼 점 사` |
| `@ip` | `[[192.168.0.1]]{@ip}` | `일구이 점 일육팔 점 영 점 일` |
| `@preserve` | `[[Apple 123 🙂]]{@preserve}` | `Apple 123 🙂` 그대로, 최종 문자 제거도 적용 안 함 |
| `@skip` | `가[[녹음 지시]]{@skip}나` | `가나` (공백을 삽입하거나 제거하지 않음) |

`@counter`를 제외한 새 예약어는 KO/EN에서 지원한다. 영어 날짜에서 `date-mdy/dmy`는
**입력의 순서**만 정한다. 출력은 기존 영어 날짜 정책을 따른다. 영어 `@version`의 구분자는
`point`, `@ip`는 `dot`이다. `@ordinal`은 한국어 `제일` 계열이며, `첫번째`는
`1{첫}번째`, 영어 first의 한국어 발음은 기존 코드믹스드 경로/직접 별칭을 사용한다.

### 입력 검증과 제한

- `@cardinal`: ASCII 부호(선택) + 정수, 쉼표는 올바른 3자리 묶음만 허용한다.
  `@decimal`: 같은 정수 형식 + `.` + 한 자리 이상의 소수부. 지수 표기는 지원하지 않는다.
- `@digits`: **숫자만** 허용한다. 부호·공백·하이픈은 오류다. 전화번호 구분자는 `@phone` 사용.
  `@ordinal`: 0보다 큰 숫자만 허용한다. `1st`가 아니라 `1`을 준다.
- `@year`: 정확히 네 자리(0001~9999). `@date`: YYYY-M-D; `@date-mdy/dmy`: M/D/YYYY,
  D/M/YYYY. 구분자는 `-`, `/`, `.` 중 하나를 일관되게 사용한다. 월·일은 1~2자리이며
  윤년을 포함해 실제 달력 범위를 검증한다. 두 자리 연도의 세기 추정은 하지 않는다.
- 시간·금액·퍼센트·단위·셈단위·범위는 **해당 타입의 기존 문법이 구간 전체와 매칭**되어야
  한다. `3kg extra`, `3개입니다`처럼 뒤에 문장이 붙으면 오류다. `[[3개]]{@counter}입니다`
  처럼 묶는다. 미등록 단위나 서로 다른 종류를 요청하면 오류이며 원문 바이패스가 아니다.
- `@fraction`: 부호 가능한 정수 분자 `/` 부호 없는 양의 정수 분모. 분모 0·연쇄 슬래시는 오류.
- `@score` (2026-10-01): 부호 없는 정수 두 개를 하이픈 하나나 콜론 하나로 이은 값만 허용한다.
  한국어는 `대`(`policy score_separator`)로 읽는다. 콜론은 기본적으로 시간 문법을 따르고
  점수로 추정하지 않으므로, 경기 점수·대비(`40:30`)는 이 제어로 지정한다.
- `@math` (2026-10-01): 숫자(부호·소수 가능)나 ASCII 한 글자 피연산자와 `+ - * / × ÷`,
  관계 기호 하나(`= < > ≤ ≥`)로 된 식 전체가 맞아야 한다. 기본 하이픈은 번호로 읽으므로
  `4-3`을 빼기로 읽으려면 이 제어를 쓴다. 연산자 말은 `math_operator` 리소스이며
  `-`를 `마이너스`로 바꾸려면 고객 TSV에 `math_operator	ko-KR	-	마이너스`를 둔다.
- `@phone`: 선택적 선두 `+`, 숫자 그룹, 그룹 사이 **하이픈 한 개 또는 ASCII 공백**.
  괄호·내선·점·공백과 하이픈의 혼합 구분자는 이번 명시 형식에서 지원하지 않는다.
  전화 번호의 실재성·국가별 번호 계획은 검증하지 않는다.
- `@version`: 선택적 `v`/`V` + 점으로 구분한 2개 이상 숫자 성분. pre-release/build metadata
  같은 SemVer 전체 문법은 지원하지 않는다. `@ip`: 네 성분이 각각 0~255인 IPv4만 지원한다.
  선행 0은 자릿수로 보존한다. IPv6은 오류다.
- 새 의미 제어의 decoded 원문은 최대95 UTF-8 byte. 수량·서수·분수/버전 성분은 정수부
  최대18자리(부호 제외, 쉼표 제외). 일반 단위의 classifier가 확장한 값도 이 범위를 검사한다.
  18자리는 두 팩에서 안전하게 처리하는 공통 한도다. 기존 `@sino` 44자리 계약은 유지한다.
  `@digits`는95자리. `@preserve`·`@skip`은 그룹 전체 `max_control_bytes` 한도를 사용한다.
- 빈 그룹·중첩·알 수 없는 예약어·형식 오류는 `TTF_EINVAL`, 크기 초과는 `TTF_ELIMIT`.
  streaming에서 닫히기 전에는 pending이며, 잘못된 구간은 callback으로 노출하지 않는다.
- `@skip`은 명시적 삭제다. `앞 [[지시]]{@skip} 뒤` → `앞  뒤`로 두 공백을 유지한다.
  미등록 문자 drop의 낱말 분리 정책이나 bracket skip의 정책을 바꾸는 기능은 아니다.

### 리소스, inspect와 확장

숫자·전화·단위·날짜·전자 표기의 기존 리소스 우선순위를 그대로 따른다. 예를 들어 고객
`phone_render ko-KR zero 영`, `electronic_separator en-US ip point`는 명시적 제어에도
적용된다. 이 변경은 읽기 정책이며, `@ip`의 0~255 검증 자체를 없애지는 않는다.

```sh
build/ttf inspect --input-format annotated --output-mode speech --trace '[[9/25]]{@fraction}'
build/ttf normalize --language en-US --input-format annotated '[[1984]]{@year}'
```

공개 span은 `CUSTOM`, `unit=@fraction`, `rule=control.pattern`으로 사용자 요청을 보존한다.
`control-semantic` trace는 내부 타입(`FRACTION`), 값(`9`), `detail`의 `secondary=25`와 unit,
선택한 grammar rule을 기록한다. `input-control`은 전체 decoded 원문을 기록한다.
`@preserve/@skip`은 의미를 추론하지 않으므로 input-control만 남긴다. 모든 offset은 UTF-8
byte이고 외부 span은 구분자와 지시문까지 포함한다. 새 ABI나 재귀 normalize는 없다.

공통 helper `src/control/semantic.c`는 언어에 독립적인 검증/IR 생성을 담당한다. 각 팩이
CUSTOM dispatch에서 선택적으로 호출한다. 단위형은 pack classifier → universal classifier
순서로 **완전 일치한 요청 타입**만 채택한 뒤, 같은 팩 verbalizer로 보낸다. 문맥 투표나
전역 자동 탐지 우선순위를 바꾸지 않는다. 새로운 언어는 helper를 선택하고 필요한 타입의
verbalizer를 제공한다. 새 예약어를 추가할 때에는 이 표, pack 지원, 음수/오류/한도,
IR trace와 임의 chunk 계약, 웹 예제/코퍼스를 함께 갱신한다.

웹 corpus는 `typed-controls-v22`이며, 예문에는 자릿수·연도·날짜 순서·분수·셈단위·전화·
버전·보존·생략이 추가되었다. 예문이 annotated와 해당 언어 옵션을 설정한다. 실행 중인
서비스의 엔진 교체는 별도 배포이므로 소스 변경만으로 운영 바이너리 갱신을 주장하지 않는다.

### 참조 검토 기록

새67건은 개발자가 구현 전에 작성한 정책 참조이며 독립 언어 감수/외부 실데이터 점수가 아니다.
최초 평가66/67 중 `typed-ip-en`의 `point`는 작성 오류였다. **기존 커밋0012911**의
`resources/languages/en-US/rules/speech.tsv`는 `electronic_separator en-US ip dot`,
`version point`를 명시한다. 엔진·리소스 정책은 바꾸지 않고 이 사전 정책을 근거로 참조를
정정했다. 원본SHA `848d5d4df4173fde152ec14b818ff0c9926cd252637db99d855e6cd72f115ce5`,
정정본SHA `37c928066f5d65fd9916d9a3331fd3d90b0bcfef487289c5b9da2f91cdf5586f`.
원본·근거는 `artifacts/tn/typed-controls/references-original.jsonl`, `reference-correction.json`;
추적 가능한 review 사유는 `tests/evaluation/reference_reviews.json`에 있다. 사람 승인0 유지.
