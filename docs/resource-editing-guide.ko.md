# 사람이 수정하는 읽기 리소스 안내서

이 프로젝트의 런타임 읽기 문구와 발음 데이터는 UTF-8 TSV 파일이다. 관리되는
기본 발음사전의 원본은 v0.10부터 `lexicons/` JSONL이며 TSV는 생성물이다.
발음 항목·근거·검토 상태를 수정할 때는 [사전 관리 가이드](lexicon-maintenance.ko.md)를
따른다. 아래의 고객 TSV overlay와 TN 규칙 편집은 그대로 사용할 수 있다.
이미 지원하는 문법의
읽기 정책은 C 소스를 다시 빌드하지 않고 바꿀 수 있다. 먼저 고객 overlay 파일로
검증한 뒤 저장소에 보관하는 방식을 권한다. 시스템 파일을 직접 덮어쓸 필요는 없다.
검증 대상은 출력 텍스트, 의미·언어 메타데이터, 스트리밍 일치성이다.

v0.12의 금융 배수·통화명·영어 보조단위 정책은
[`currencies.tsv` 수정 안내](currency-resources.md)에 설명되어 있다.
`currency_scale`은 `million` 등의 인식 표기를 10의 지수와 연결하고,
`currency`는 의미를 유지한 채 통화 읽기를 바꾼다. `currency_minor_digits`는
영어의 소수 금액 읽기 정책이며 반올림이나 환산 설정이 아니다.
[지원 문법과 IR 예제](scaled-currency.md)를 함께 확인하고, 웹의
**금융 금액 배수·통화·범위** 코퍼스로 출력·금액 값·통화 단위를 검증한다.

v0.11의 추가 단위·통화 규칙은
`resources/languages/en-US/rules/measurements.tsv`와
`resources/languages/ko-KR/rules/measurements.tsv`에 있다. `numeric_suffix`는
표기와 의미 단위를 연결하고 `unit`/`quantity_prefix`는 언어별 읽기만 정한다.
예를 들어 `mph`는 숫자 값을 바꾸지 않고 영어에서는 `miles per hour`,
한국어에서는 `시속 … 마일`로 읽는다. `MW`와 `mW`는 다른 표기이며 대소문자를
구별한다. 단위를 자동 환산하지 않는다. 통화 prefix와 suffix가 충돌하는
`US$15원`은 원문을 유지하고, 같은 통화인 `US$15달러`는 한 번만 읽는다.

외부 코퍼스에서 새 표기를 발견했다면 [외부 평가 가이드](../benchmark/README.md)의
동일 reference 비교 절차로 개선량과 퇴행을 확인한다. corpus의 발음 결과를
그대로 제품 사전에 옮기지 말고 의미·읽기 정책·출처·이용 조건을 각각 검토한다.

## 1. 가장 짧은 실습: 금액 범위를 “부터 … 까지”로 읽기

저장소 최상위 디렉터리에서 다음 Python 명령을 실행한다. 문자열의 `\t`는 실제 TAB,
`\n`은 줄바꿈으로 저장된다. `부터 ` 끝의 공백은 의도한 값이다.

```sh
python3 - <<'PY'
from pathlib import Path
path = Path("resources/customers/local/ko-KR/reading.tsv")
path.parent.mkdir(parents=True, exist_ok=True)
path.write_text(
    "policy\tko-KR\tquantity_range_separator\t부터 \n"
    "policy\tko-KR\tquantity_range_repeat_unit\t1\n"
    "policy\tko-KR\tquantity_range_suffix\t까지\n",
    encoding="utf-8",
)
PY

build/ttf normalize --resources resources \
  --customer resources/customers/local/ko-KR/reading.tsv \
  '100원~200원'
```

기대 문자열은 `백 원부터 이백 원까지`다. 기본 리소스만 사용하면
`백 원에서 이백 원`이다. 이 예제는 단위를 양 끝에 읽고, 범위를 나타내는 연결어와
마지막 어미만 바꾼다. 입력의 두 숫자를 식별번호처럼 한 자리씩 읽는 설정이 아니다.

같은 파일로 `build/ttf inspect ...`를 실행하면 `RANGE`, 양쪽 값과 `KRW` 단위를
확인할 수 있다. 의미가 잘못 분류되었는데 출력 문자열만 그럴듯해진 경우도 있으므로
출력과 함께 이 정보를 확인한다.

## 2. TSV 파일의 정확한 형식

한 행은 **비어 있지 않은 네 열**이며, 열 사이에는 실제 TAB이 하나씩 들어간다.

| 열 | 의미 | 예 |
| --- | --- | --- |
| 1 `kind` | 리소스 종류 | `policy`, `lexicon`, `month` |
| 2 `language` | 적용 언어 또는 공통 fallback | `ko-KR`, `en-US`, `*` |
| 3 `key` | 바꿀 항목 | `quantity_range_separator` |
| 4 `value` | 그 항목의 값 | `부터 ` |

아래 블록에는 실제 TAB이 있다. 편집기에서 공백/탭 표시를 켜면 구분하기 쉽다.

```tsv
policy	ko-KR	quantity_range_repeat_unit	1
policy	ko-KR	quantity_range_suffix	까지
```

- `<TAB>`이라는 글자를 입력하거나 네 칸의 공백으로 바꾸면 TSV가 아니다.
- 값의 앞뒤 공백은 지워지지 않는다. `에서 `, `부터 `에는 뒤 공백을 남긴다.
- 필드 안에는 실제 TAB이나 줄바꿈을 넣을 수 없다.
- 빈 행은 무시한다. 주석은 줄 맨 앞의 `#`로 시작한다.
- 네 번째 값이 빈 문자열이면 오류다. `none`은 아래 suffix 정책에서만 “붙이지 않음”을
  뜻한다. 모든 리소스의 공통 삭제 명령이 아니다.
- 같은 계층·종류·언어·키를 중복 작성하면 **먼저 선언한 값**이 유지된다. 파일 끝에
  같은 키를 덧붙이기보다 기존 행을 수정한다.

공백을 확인하려면 다음처럼 값의 표현을 출력할 수 있다.

```sh
python3 - <<'PY'
from pathlib import Path
for line in Path("resources/customers/local/ko-KR/reading.tsv").read_text(encoding="utf-8").splitlines():
    if line and not line.startswith("#"):
        print([repr(field) for field in line.split("\t")])
PY
```

## 3. 범위 읽기 정책

설정 위치는 `resources/languages/ko-KR/rules/numeric.tsv`다. 고객 파일에는 바꿀
행만 쓴다. 인식 가능한 범위 문법 안에서 다음 항목이 사용된다.

| 종류 / 키 | 기본값 | 용도 |
| --- | --- | --- |
| `policy / range_separator` | `에서 ` | 단위 없는 양의 정수 하이픈 범위 연결어 |
| `policy / quantity_range_separator` | `에서 ` | 명시적 `~` 또는 단위·소수·부호가 있는 수량 범위 연결어 |
| `policy / quantity_range_repeat_unit` | `1` | 첫 끝점에도 단위를 읽음. `0`이면 첫 단위를 생략 |
| `policy / quantity_range_suffix` | `none` | 수량 범위 마지막에 붙일 말. 예: `까지` |
| `policy / range_mode` | `cardinal` | 단위 없는 하이픈 숫자열의 기존 읽기 방식 |

기본 정책의 예:

```text
100원~200원 → 백 원에서 이백 원
3~5개       → 세 개에서 다섯 개
3~5         → 삼에서 오
```

단위를 반복하면 `원`, `퍼센트`, `개` 등을 각 끝점과 함께 읽는다. `개`, `명`, `살`,
`시`처럼 고유어 수사를 사용하는 counter는 그 수사 정책을 따른다. counter에는
기본 `quantity_range_repeat_unit=1`을 권한다. `0`은 첫 끝점을 단위 없는 한자어
수사로 읽으므로 counter의 축약 표현이 자연스러운지 별도로 검토해야 한다.

통화에서 첫 단위를 생략하는 명시적 선택은 다음과 같다.

```tsv
policy	ko-KR	quantity_range_repeat_unit	0
```

이때 `100원~200원`의 기본 연결어 출력은 `백에서 이백 원`이다. 정확한 수량을
확인하는 금융 안내에서는 양 끝의 단위를 반복하는 기본값이 더 분명하다.

“부터 … 까지”를 원하면 1절의 세 행을 함께 적용한다. `부터 `에는 뒤 공백이 있고,
`까지`는 마지막 단위 또는 숫자에 붙는다. 명시적 물결 범위 `3~5`도 수량 정책을
사용하므로 위 설정에서 `삼부터 오까지`가 된다. 단위 없는 양의 정수 하이픈 `3-5`는
기존 `range_separator` 정책을 사용하며 수량 suffix의 적용 대상이 아니다. 이를 모든
하이픈의 문법적 의미를 판별하는 기능으로 오해하지 않는다.

### `~` 기호와 범위는 다르다

단독 `~`의 한국어 기호 이름은 다음 리소스에서 `물결`로 읽는다.

```tsv
symbol	ko-KR	~	물결
```

숫자 범위 안의 `~`는 `RANGE`의 구분자다. 기호 이름을 `까지`로 바꿔 범위를
고치는 방식은 범위의 문법·단위 처리와 연결되지 않는다. 범위는 `policy` 항목을
바꾸고, 단독 기호는 `symbol` 항목을 바꾼다.

### 식별번호와 범위의 구별

`resources/domains/finance/ko-KR/rules.tsv`의 `range_mode=digits`는 예전 예제와의
호환을 위한 넓은 정책이다. 단위 없는 `123-456`을 `일이삼 사오육`으로 읽을 수 있지만,
모든 하이픈을 계좌번호라고 이해한 것은 아니다. 실제 수량 범위에도 오적용될 여지가
있다. 물결 `~` 범위는 이 digit 모드에서 제외된다.

계좌·거래번호는 식별 label을 쓰는 편이 더 명확하다.

```tsv
identifier_label	ko-KR	고객번호는	digits
identifier_render	ko-KR	zero	공
```

`고객번호는 001-234입니다.`의 원하는 읽기는
`고객번호는 공공일 이삼사입니다.`다. 이 label은 번호 읽기 신호이며 실제 계좌나
고객번호의 유효성을 확인하지 않는다. 기본 label은
`resources/languages/ko-KR/rules/identifiers.tsv`에 있고, 같은 label 값의 `disabled`는
상속된 신호를 끈다. 그룹 사이에는 기본 공백 한 개가 들어간다. 이 separator를
명시하려면 Python 문자열 `"identifier_render\tko-KR\tseparator\t \n"`을 저장한다.

## 4. 발음·월·시각 문구를 바꾸는 위치

### 외국어 발음사전

영어 단어를 한국어 문맥에서 어떻게 읽을지는 `lexicon`이다. 언어 열은 출력 언어,
키의 `en-US:`는 원어 언어다.

```tsv
lexicon	ko-KR	en-US:sql	시퀄
lexicon	ko-KR	en-US:after effects	애프터 이펙트
```

이것은 `SQL`의 음역 선택이며 번역이 아니다. 영어 문맥과 보호된 code/path 안에서는
이 한국어 발음 override를 적용하지 않는다. 다단어 키는 최대 8단어, 정규화한 키
95바이트, 입력 대응 구간 255바이트라는 현재 제한이 있다. ASCII 대소문자를 구분하지
않고 단어 사이 space/tab은 정규화하지만 구두점·줄바꿈을 건너뛰지 않는다.
자세한 범위는 [다단어 사전 문서](multiword-lexicon.md)를 따른다.

`3개 → 세 개`는 수사+counter 정규화다. 이를 영어 발음사전에 넣어 고치지 않는다.
시스템 발음 원본은 `lexicons/ko-KR/`에서 관리하며,
`python3 tools/lexicon/manage.py build`가
`resources/languages/ko-KR/lexicon/`의 분야별 TSV를 생성한다.

### 6월·10월의 어간

현재 월 이름 리소스는 전체 단어가 아닌 어간이다.

```tsv
month	ko-KR	6	유
month	ko-KR	10	시
```

한국어팩이 이 값 뒤에 `월`을 바로 붙여 `유월`, `시월`을 만든다. 값에 `유월`,
`시월` 전체를 넣으면 `월`이 중복된다. 날짜와 지원하는 월 표현은 공통된 월 읽기
정책을 사용하지만, 리소스 한 행을 추가한다고 새 날짜 문법을 인식하는 것은 아니다.

### 단위 이름 앞의 말

숫자 앞에 붙는 단위 구분말은 `quantity_prefix`로 관리한다. 예를 들어 명시적
섭씨 기호 `°C`는 현재 `quantity_prefix / °C`의 값 `섭씨 `와 `unit / °C`의 값 `도`를
함께 사용한다. 따라서 `32°C`의 기대 텍스트는 `섭씨 삼십이 도`다. prefix 끝의
공백도 값에 포함된다. 값 `섭씨`를 `unit / °C`에 넣으면 숫자 뒤의 단위 이름이
바뀌는 것이므로 같은 동작이 아니다.

새 단위를 이름표만 추가한다고 숫자 문법이 자동으로 인식하지는 않는다. 이미 정의된
unit과 prefix의 문구 변경, 새 `numeric_suffix`의 지원 가능한 타입 선언, 새로운
문법 구현은 별도의 작업이다.

### 영어 자정과 정각

`resources/languages/en-US/rules/numeric.tsv`의 세 항목이다.

```tsv
clock	en-US	midnight	midnight
clock	en-US	after	after
clock	en-US	h24_full_hour	hundred hours
```

기본 예는 `00:00 → midnight`, `00:00:01 → one second after midnight`,
`13:00 → thirteen hundred hours`다. 마지막은 24시간제 값을 유지하기 위한 프로젝트
읽기 스타일이다. 일반적인 모든 영어 시각의 유일한 정답으로 간주하지 않는다.
`00:30`까지 12시간제로 바꾸는 기능이나 AM/PM 추론을 이 문구 교체로 추가할 수는 없다.

## 5. 우선순위와 적용 시점

일반 리소스는 다음 순서로 우선한다.

```text
Customer > Domain > Language System > Common
```

발음사전에는 C API 또는 웹 사용자 사전의 `User`가 추가된다.

```text
User > Customer > Domain > System > dictionary / G2P / emergency fallback
```

다단어 발음은 계층을 먼저 비교하고, 같은 계층에서 가장 긴 일치를 고른다. 따라서
User의 짧은 단어 override가 System의 긴 구절보다 우선할 수 있다. 같은 계층의
기본 resource 중복 선언은 앞 행이 이기지만, `ttf_lexicon_add`는 지정한 계층의 기존
항목을 교체하는 명시적 API다.

TN 규칙 `exact`/`pattern`은 발음사전과 별개다. 선택 순서는 높은 계층 → 그 계층의
exact 우선 → 긴 입력 일치 → 구체적인 literal → 먼저 선언한 행이다. 같은 계층이면
언어별 리소스가 `*` fallback보다 우선한다. 무조건 치환하는 `exact`를 범용 숫자
문법의 대용으로 대량 추가하면 다른 문맥을 잘못 바꿀 수 있다.

### CLI

새 CLI 실행은 새 engine을 만들므로 다음 실행에서 파일 변경을 읽는다.

```sh
build/ttf inspect --resources resources \
  --customer resources/customers/local/ko-KR/reading.tsv \
  '100원~200원'

build/ttf normalize --resources resources \
  --domain resources/domains/finance/ko-KR/rules.tsv \
  --customer resources/customers/local/ko-KR/reading.tsv \
  '100원~200원'
```

고객 파일을 `--customer`로 명시하면 `manifest.ttfm`에 추가할 필요가 없다. 시스템
리소스 파일을 새로 만들었다면 manifest의 상대 경로 목록에 넣어야 기본 로딩된다.
같은 파일을 시스템 manifest와 고객 인자로 중복 로드할 이유는 없다.

선택적으로 텍스트 리소스를 `.ttfb`로 포장할 수 있다. 이는 C 빌드와 무관하다.
포장은 형식·무결성 검사를 돕지만 개별 읽기 정책의 언어적 타당성을 검수하지 않는다.

```sh
python3 tools/resource_compiler/compile_resources.py \
  resources/customers/local/ko-KR/reading.tsv /tmp/reading.ttfb
build/ttf normalize --resources resources --customer /tmp/reading.ttfb '100원~200원'
```

### 웹 도구 — 이 VM의 8000 포트

`http://127.0.0.1:8000/` 또는 VM의 8000 포트 접속 주소에서
**발음사전 / 고객 리소스 실험 → 고객 리소스 TSV**에 실제 TAB으로 된 행을 붙여 넣는다.
정규화 후 상세 Semantic IR을 보고, 같은 설정으로 청크 검증도 실행한다.
User 발음사전 항목이 남아 있으면 고객 발음보다 먼저 적용되므로 실험 설정을 함께 확인한다.

웹의 고객 TSV는 요청별 임시 overlay다. 저장소 파일을 바꾸거나 다음 사용자 요청의
기본값으로 저장하지 않는다. 영구 적용하려면 검증한 내용을 고객 TSV 파일로 저장해
Git에서 관리하고 해당 파일을 로드하는 서비스 설정에 반영한다. 코퍼스 모드는
사례별 고정 리소스를 사용하므로 화면의 임시 고객 TSV 검증은 정규화/청크 검증 화면에서
한다. 실행 방법은 [웹 도구 안내](web-workbench.md)를 참고한다.

### 실행 중인 서비스와 C API

freeze된 engine은 파일 변경을 자동 감시하지 않는다. 변경 파일로 새 engine을 생성하고
리소스를 로드한 뒤 freeze하여 새 요청에 교체 적용한다. 진행 중인 stream은 원래
engine으로 끝낸다. 이미 freeze된 engine에 `ttf_resource_load`를 호출하면
`TTF_ESTATE`가 반환된다.

현재 웹 도구는 요청마다 engine을 새로 만들므로 리소스 파일 변경은 다음 요청에서
읽는다. 운영 서비스가 engine을 재사용하는 경우에는 교체 또는 서비스 재시작이 필요하다.
현재 VM 서비스 구성을 다시 적용하거나 서버·라이브러리 자체를 바꿨다면 다음처럼
재시작할 수 있다.

```sh
systemctl --user restart ttf-workbench.service
systemctl --user status ttf-workbench.service
```

TSV 문구만 바꿀 때 C core 재컴파일은 필요 없다. 새로운 classifier 문법이나 pack
알고리즘을 바꾼 경우에는 라이브러리를 다시 빌드해야 한다.

## 6. 정답을 확인하고 변경 이력을 남기기

1. 먼저 사람이 원하는 출력과 그 이유를 적는다. 금액인지 번호인지, 범위 양쪽 단위가
   무엇인지, 원어를 유지할지 음역할지를 정한다.
2. 기본/고객 overlay를 비교하고 `semantic_type`, `value`, `secondary`, `unit`,
   source/context/render와 실제 선택 계층을 확인한다.
3. 양성 사례뿐 아니라 숫자 아닌 `~`, 계좌번호, 영어 문장, 보호된 코드, 줄바꿈 경계도
   확인한다. 같은 리소스·설정에서 입력 청크를 바꿔도 출력이 같아야 한다.
4. 변경 전 기대값, 새 기대값, 이유, 정책 상태를 [감사 문서](audit/english-context-reference-audit.md)
   및 reference 변경 ledger에 기록한다. 엔진의 현재 출력만 복사해 정답을 바꾸지 않는다.
5. 합의된 reference를 regression에 넣는다. 프로젝트 읽기 정책, 임시 G2P 출력,
   지원하지 않아 원문을 보존하는 경우, 명백한 오류 수정은 서로 구분한다.

테스트 통과는 기록한 계약과의 일치다. 사람이 독립 검수하지 않은 데이터를
`human approved`로 부르거나, 합성 corpus 성공률을 전체 고객 입력의 정확도로 소개하지
않는다. 문법 오류를 이름표 리소스로 덮을 수는 없다. classifier가 잘못된 의미를
선택했거나 새로운 문법을 지원해야 한다면 그 원인과 필요한 구현 변경을 별도로 고친다.
## 영어 날짜와 연도 리소스

`resources/languages/en-US/rules/dates.tsv`에서 월 이름 별칭, 연도 문맥 단서,
수량 예외와 읽기 스타일을 수정할 수 있습니다. `calendar`/`cardinal` 스타일은
값을 바꾸지 않고 읽는 방법만 고릅니다. 새 월 별칭이나 문맥 단서를 추가할 때는
`boundary_word` 및 필요한 `boundary_period`도 함께 등록하여 짧은 스트리밍 구간에서
검증하세요. 예시와 한계는 [날짜 편집 가이드](english-dates.md)에 있습니다.
웹의 **영어 날짜·연도·코드믹스** 코퍼스는 20개 개발자 작성 계약을 검사합니다.
이 통과 여부는 독립적인 사람의 정답 검수를 대신하지 않습니다.
