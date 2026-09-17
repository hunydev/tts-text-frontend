# Unicode 숫자 묶음과 정확한 값

v0.15의 숫자 문법은 ASCII 숫자 사이의 NBSP (`U+00A0`), 좁은 NBSP
(`U+202F`), 얇은 공백 (`U+2009`)을 천 단위 구분자로 인식합니다. 원문을
미리 치환하지 않습니다. Tokenizer는 이 문자를 `WHITESPACE`로 남기고,
Semantic IR의 `value`만 구분자를 제거한 정확한 십진 문자열로 만듭니다.
`input_begin`/`input_end`와 `surface`는 원래 UTF-8 바이트를 가리킵니다.

## 동작 예

아래 표의 ` `는 `U+202F`입니다.

| 입력 | 출력 | Semantic IR |
|---|---|---|
| `1 250원` | `천이백오십 원` | `CURRENCY`, `value=1250`, `unit=KRW` |
| `₩ 1 250` | `천이백오십 원` | `CURRENCY`, `value=1250`, `unit=KRW` |
| `1 250kg` | `천이백오십 킬로그램` | `MEASURE`, `value=1250`, `unit=kg` |
| `-1 250.50%` | `마이너스 천이백오십 점 오영 퍼센트` | `PERCENT`, `value=-1250.50` |
| `1 250e-2kg` | `십이 점 오 킬로그램` | `MEASURE`, `value=12.5`, `unit=kg` |
| `1 250~2 500원` | `천이백오십 원에서 이천오백 원` | `RANGE`, `value=1250`, `secondary=2500`, `unit=KRW` |
| `$1 250` (영어) | `one thousand two hundred fifty dollars` | `CURRENCY`, `value=1250`, `unit=USD` |
| `1 250원` (ASCII 공백) | `일 이백오십 원` | 독립된 `CARDINAL`과 `CURRENCY` |

한국어 출력을 고정해서 검사할 때는 `--language ko-KR`을 사용합니다.
문장에 영어 단위만 있는 경우 자동 context router가 영어 문맥을 선택할 수
있습니다. 한국어 문장 안의 금액과 단위에는 문장 문맥이 적용됩니다.

```sh
build/ttf normalize --language ko-KR '1 250원'
build/ttf inspect --language ko-KR '1 250e-2kg'
```

## 허용 문법과 보존 정책

- 정수부 첫 묶음은 1~3자리이고, 이어지는 묶음은 각각 정확히 3자리입니다.
- 한 숫자 안에서는 동일한 구분자만 사용합니다. 쉼표와 Unicode 공백을
  섞거나 서로 다른 Unicode 공백을 섞으면 전체 숫자 객체를 보존합니다.
- 부호, 소수, 통화 prefix/suffix, 단위, 백분율, 기존 범위 문법과 함께
  사용할 수 있습니다. 통화기호와 값 사이, 값과 단위 사이, 범위 기호
  주위의 지원되는 Unicode 공백도 수량의 공백으로 처리합니다.
- 소수부와 지수부 안의 묶음, 반복된 구분자, 잘못된 자릿수, IR 용량을
  넘는 값은 부분적으로 읽지 않고 원문 `TEXT`로 남깁니다. 선언된 단위와
  범위의 다음 끝점도 같은 보존 객체에 포함합니다.
- ASCII 공백과 탭은 숫자 묶음 구분자가 아닙니다. 일반 문장 속 숫자들을
  자동으로 합치지 않습니다.
- 코드와 지원되는 보호 리터럴은 기존 보호 처리의 우선순위를 유지합니다.
  숫자 이외의 Unicode 공백도 원래 바이트 그대로 유지합니다.

예를 들어 `1 25원`, `1 250,000원`, `1  250원`, `1.25 000원`,
`1e2 000원`, `1 250~2 50원`은 각각 원문 전체를 유지합니다. 잘못된 입력을
추측해서 금액을 바꾸지 않습니다. `inspect`의 `syntax.invalid-*` 규칙과
빈 `value`로 보존 여부를 확인할 수 있습니다.

지원 범위는 **ASCII 십진 숫자의 동일한 3자리 정수 묶음**입니다. 소수 쉼표,
인도식 3/2자리 묶음, 모든 Unicode 숫자 체계, 소수부 묶음, 전체 CLDR locale
파싱을 지원한다고 주장하지 않습니다. 이런 형식은 별도 문법·로케일 설계와
평가가 필요하며, 현재 형식을 억지로 기존 문법의 숫자로 바꾸지 않습니다.
그룹이 있는 가수의 지수 표기는 기존 exact decimal 문법과 합성한 프로젝트
입력 정책이며 CLDR 출력 패턴 준수 주장과는 별개입니다.

## 사람이 수정하는 리소스

`resources/common/numeric-grouping.tsv`는 네 열 TSV입니다.

```text
numeric_group_separator<TAB>*<TAB>구분자 한 문자<TAB>3
```

마지막 열의 `3`은 활성화, `0`은 비활성화입니다. 구분자는 문자 그대로
저장합니다. `\u202f`라는 여섯 글자를 넣으면 Unicode 구분자 키가 되지
않습니다. 파일의 주석에는 각 문자의 코드 포인트를 적어 검토하기 쉽게
관리합니다.

고객 파일로 특정 구분자를 끄는 예:

```python
from pathlib import Path
Path('customer-grouping.tsv').write_text(
    'numeric_group_separator\t*\t\u202f\t0\n', encoding='utf-8')
```

```sh
build/ttf normalize --customer customer-grouping.tsv '1 250원'
# 1 250원 (비활성화된 문법을 하나의 TEXT 객체로 보존)
```

같은 방식으로 tokenizer가 인식하는 다른 Unicode 가로 공백을 활성화할
수 있습니다. 예를 들어 `U+2007` FIGURE SPACE를 `3`으로 등록하면 3자리
묶음으로 처리합니다. ASCII 공백·탭·줄 구분자를 숫자 묶음으로 등록하는
것은 허용하지 않습니다. `0`/`3` 이외의 값도 읽기 활성화로 해석하지
않습니다. 리소스 로드 우선순위는 Customer > Domain > System > Common이며
사용한 높은 우선순위의 separator policy는 semantic resource layer에 남습니다.

리소스를 변경한 뒤 원문, 정확한 수치, 의미 class, source/context/render
언어, 입력 바이트 범위와 스트리밍 결과를 함께 검토합니다. 같은 출력
문자열만 비교하면 잘못 분리된 금액을 놓칠 수 있습니다.

## 스트리밍과 테스트

숫자 뒤에 나타난 구분자는 다음 숫자가 오기 전까지 pending 상태로
유지합니다. 구분자가 여러 번 들어간 잘못된 입력도 첫 번째 숫자를 먼저
commit하지 않습니다. 문장/phrase target은 안전한 의미 경계보다 우선하지
않으며 전체 pending buffer 제한은 그대로 적용됩니다.

```sh
python3 tests/integration/test_unicode_grouping.py build/ttf resources
python3 tests/integration/test_unicode_grouping.py build/ttf resources \
  --library build/libtts_text_frontend.so
```

검사는 사람이 작성한 고정 출력, Python `Decimal`로 독립 계산한 90개의
정확한 값, 잘못된 그룹 전체 보존, 정책 override, tokenizer의 공백 분류와
바이트 범위를 확인합니다. CLI의 1/7/64바이트 chunk 및 작은 segment target,
그리고 native API의 모든 두 조각 바이트 분할을 batch 결과와 비교합니다.
UTF-8 구분자의 중간에서 끊긴 입력도 포함합니다.

## 직접 조사한 참고자료

- [Unicode 17.0, Chapter 6](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-6/):
  각 공백 문자의 정의와 NBSP/NNBSP의 구분을 확인했습니다. 화면 너비가
  비슷한 공백도 서로 다른 입력 문자로 보존합니다.
- [UTS #35, Numbers, 확인 시 표기 버전 48.2](https://www.unicode.org/reports/tr35/tr35-numbers.html):
  숫자 grouping symbol과 primary/secondary grouping size의 분리를
  확인했습니다. 여러 locale의 묶음 구조와 소수부 grouping도 설명하므로
  이 구현의 단일 3자리 문법과 전체 CLDR 지원을 구별했습니다.

확인일은 2026-09-17입니다. 코드·데이터 표·문서 원문을 복사하지 않았고
지원 정책 및 테스트 문장은 이 프로젝트에서 작성했습니다. 라이선스와
vendoring 여부는 [THIRD_PARTY.md](THIRD_PARTY.md)에서 별도로 관리합니다.

## 동일 원문으로 이전 버전과 비교

`benchmark/evaluators/compare_unicode_grouping.py`는 이 테스트의 고정된 원문
57건을 두 라이브러리와 각각의 리소스로 실행합니다. Decimal 생성 검사,
잘못된 입력 보호 검사, 스트리밍 반복 횟수는 이 표본 수에 포함하지 않습니다.

v0.14 스냅샷과 v0.15 후보 비교에서 출력 텍스트 완전일치와 정확한 단일
Semantic IR은 각각 **0/57 → 57/57**이었습니다. 클래스별 원문 수는
CARDINAL 6, CURRENCY 24, MEASURE 12, PERCENT 3, RANGE 12입니다.
이것은 기능 개발용으로 직접 작성한 원문들의 결과이며, 독립 검수된
생산 환경 정확도나 held-out benchmark 점수가 아닙니다.

```sh
python3 benchmark/evaluators/compare_unicode_grouping.py \
  --baseline-library benchmark/generated/baseline-0.14.0/libtts_text_frontend.so \
  --baseline-resources benchmark/generated/baseline-0.14.0/resources \
  --candidate-library build/libtts_text_frontend.so \
  --candidate-resources resources \
  --output docs/research/unicode-grouping-v15-summary.json
```

[집계 결과](research/unicode-grouping-v15-summary.json)는 텍스트 일치, 전체
입력 범위를 덮는 단일 span의 class 일치, `value`/`secondary`/`unit` 문자열
일치를 따로 기록합니다. 여러 span으로 잘못 쪼개진 입력은 IR 일치로 인정하지
않습니다. 라이브러리·리소스·원문 집합·평가 코드 해시를 기록하고, 측정 도중
입력이 바뀌면 결과 생성을 중단합니다.
