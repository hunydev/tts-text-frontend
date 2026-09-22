# 한국어 서수·횟수·번호

v0.15는 숫자의 의미를 판별한 뒤 한국어 pack에서 읽습니다. `번`을 포함하는
모든 표면형에 하나의 수사 규칙을 적용하지 않습니다. 일반적인 문맥 전체를
이해하는 모델은 아니며, 아래에 명시한 deterministic 문법과 단서를 사용합니다.

| 입력 | 출력 | IR |
|---|---|---|
| `1번째` | `첫 번째` | ORDINAL, value=1, unit=번째 |
| `11번째` | `열한 번째` | ORDINAL, value=11 |
| `20번째` | `스무 번째` | ORDINAL, value=20 |
| `21번째` | `스물한 번째` | ORDINAL, value=21 |
| `1001번째` | `천일 번째` | ORDINAL, value=1001 |
| `총 3번` | `총 세 번` | COUNTER, secondary=frequency |
| `3번을 반복합니다.` | `세 번을 반복합니다.` | COUNTER, secondary=frequency |
| `3번 출구` | `삼 번 출구` | COUNTER, secondary=identifier |
| `번호는 3번 반복합니다.` | `번호는 삼 번 반복합니다.` | identifier 단서 우선 |
| `3번` | `삼 번` | 기존의 문맥 없는 기본 정책 |
| `제3장` | `제삼 장` | ORDINAL, secondary=prefix, unit=장 |
| `제1,001호` | `제천일 호` | ORDINAL, value=1001, unit=호 |
| `1~3번째` | `첫 번째에서 세 번째` | RANGE, value=1, secondary=3 |
| `3번째~5` | `세 번째에서 다섯 번째` | 왼쪽 서수 단위 상속 |
| `2번~3번 반복합니다.` | `두 번에서 세 번 반복합니다.` | RANGE, rule=ko:frequency-range |

`번째`는 양의 정수에만 적용합니다. 1은 `첫`, 2~99는 고유어 수사,
100 이상은 한자어 수사로 읽는 프로젝트 기본 정책입니다. 큰 서수의 다른
읽기를 언어적으로 틀린 것으로 판정하지 않습니다. `01번째`의 숫자 값 문자열은
`01`로 보존하면서 읽기는 `첫 번째`로 만듭니다. 유효한 쉼표·Unicode 3자리
묶음은 공통 숫자 parser를 사용하며, 원문 byte offset은 그대로 유지합니다.

`제` 접두형은 한자어 수사로 읽습니다. 초기 단위 목록은 장·회·차·조·항·절·권·부·호·번입니다.
`제3`도 지원하며 `경제3장` 안의 `제`를 접두사로 가로채지 않습니다.
`0번째`, `-3번째`, `1.5번째`, `제0장`, 손상된 `제1,00호` 등은 원문 `TEXT`로
보존합니다. 현재 지원하지 않는 접두 서수 범위 `제3-5장`도 하나의 원문 객체로
남깁니다. 잘못된 서수 범위의 왼쪽만 읽어 값을 바꾸지 않습니다.

## 문맥 단서

- 왼쪽의 총·모두·매일 또는 오른쪽의 반복·시도·실행·누르/누릅 계열은 횟수 단서입니다.
- 왼쪽 번호·번호는·버스·노선·문항·항목 또는 오른쪽 출구·버스·문항은 번호 단서입니다.
- 양쪽 단서가 충돌하면 번호를 우선합니다. 단서가 없으면 기존 `번=sino`가 유지됩니다.
- 오른쪽 단서는 등록한 접두 stem으로 비교합니다. 한국어 형태소 분석 전체를 구현한
  것이 아니므로 새로운 어미·동사·복합 문맥에는 별도 단서와 회귀 예제가 필요합니다.
- 이미 등록한 조사·수량 꼬리(`을`, `씩` 등)는 읽기 대상의 뒤에 그대로 남깁니다.
- 단서는 줄바꿈을 넘지 않습니다. 원래 구문 바깥의 공백도 유지하므로
  `매일3번씩`은 `매일세 번씩`입니다. 문장 전체의 맞춤법 교정 기능은 아닙니다.

## 리소스 수정 가이드

파일: `resources/languages/ko-KR/rules/counters.tsv`. TSV 네 열은
`kind<TAB>language<TAB>key<TAB>value`입니다. 변경 내용은 별도 고객 파일로 적용할 수 있습니다.

| kind | key 예 | value | 용도 |
|---|---|---|---|
| `ordinal_first` | `1` | `첫` | 특정 서수의 읽기 override |
| `native_ordinal_suffix` | `번째` | `native` / `sino` | 서수 접미어 읽기 |
| `ordinal_policy` | `large` | `sino` | 100 이상 기본 정책; 현재 지원값은 sino |
| `ordinal_marker` | `prefix` | `제` | 접두 서수 형태 |
| `ordinal_prefix_unit` | `호` | `1` | 접두 서수 단위 |
| `counter_role_before` | `총` | `frequency` / `identifier` | 왼쪽 완전한 단어 |
| `counter_role_after` | `반복` | `frequency` / `identifier` | 오른쪽 단어의 접두 stem |
| `boundary_word` | `총` | `1` | 뒤의 수량을 기다리는 streaming 단서 |
| `classifier_priority` | `ko:suffix-ordinal` | `10` | 같은 범위에서 generic 후보보다 높은 specificity |

예: 고객 정책에서 첫 번째를 “맨 첫 번째”로 읽고 “수정”을 횟수 단서에 추가합니다.

```python
from pathlib import Path
Path('customer-counter.tsv').write_text(
    'ordinal_first\tko-KR\t1\t맨 첫\n'
    'counter_role_after\tko-KR\t수정\tfrequency\n'
    'counter_role_before\tko-KR\t재생\tfrequency\n'
    'boundary_word\tko-KR\t재생\t1\n', encoding='utf-8')
```

```sh
build/ttf normalize --customer customer-counter.tsv '1번째'
# 맨 첫 번째
build/ttf normalize --customer customer-counter.tsv '재생 3번'
# 재생 세 번
```

새 왼쪽 단서에는 `boundary_word`도 함께 선언해 streaming에서 그 단어 뒤를
미리 확정하지 않게 합니다. 겹치는 오른쪽 stem은 가장 긴 유효한 키를 선택하고,
동일 키의 resource layer는 Customer > Domain > System > Common입니다.
읽기에 사용하지 않은 짧은 Customer 키를 provenance로 표시하지 않습니다.

`ORDINAL`과 역할이 확인된 `COUNTER`의 source는 `ko-KR`입니다. 문맥 없는
숫자 `3번`은 기존대로 `und`입니다. RANGE의 `secondary`에는 오른쪽 숫자가
들어가므로 역할은 matched rule에 보존합니다. evaluator도 `secondary`를 검사합니다.

## 검증과 근거

`tests/integration/test_korean_counters.py`는 엔진 출력을 복사하지 않은 고정
참조를 사용합니다. 1~99 서수, 큰 수 정책, 최소 문맥 대조, 조사·Unicode 묶음,
잘못된 형식, 고객 override를 검사하고 기본/짧은 분절 및 모든 선택 입력의
두 조각 UTF-8 byte 분할에서 텍스트와 전체 metadata를 대조합니다.
`tests/evaluation/korean_counters_v15.jsonl`은 웹에서도 실행할 수 있습니다.
참조는 독립적인 사람의 검수 완료로 표시하지 않습니다.

사실 확인 자료(2026-09-17 조회, 원문/예문 vendoring 없음):

- [국립국어원 329611](https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?mn_id=216&pageIndex=1&qna_seq=329611):
  열한 번째의 읽기와 큰 서수의 복수 가능한 읽기를 검토했습니다.
- [국립국어원 325931](https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?mn_id=27&pageIndex=1&qna_seq=325931):
  횟수·번호의 문맥 구분에 일률적인 기준을 적용하기 어렵다는 설명을 참고했습니다.
- [국립국어원 306013](https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?mn_id=216&pageIndex=1&qna_seq=306013):
  제-와 한자어 수사의 결합을 확인했습니다.

이 자료는 프로젝트의 모든 출력에 대한 기관의 승인이나 독립 검수를 뜻하지 않습니다.

## `대`: 대수와 구간

`대`는 기계를 세는 단위(`3대` → `세 대`)이지만, 10~90의 십 단위 수(`20대` → `이십 대`)나
만 이상의 배수 금액(`5억대`)에 붙으면 연령·가격 **구간**이다. 구간은 한자어 수로 읽고,
범위에서는 끝에 한 번만 `대`를 둔다 (`20~30대` → `이십에서 삼십 대`, `1~2억대` → `일억에서 이억 대`).
바로 앞/뒤 단어 cue가 기본값보다 먼저다: `counter_band_before ko-KR 차량 count`
(`차량 20대` → `차량 스무 대`), `counter_band_after ko-KR 남성 band`. 구간 unit은 `band`이며
`range_repeat_unit ko-KR band 0`이 범위의 단위 반복을 끈다. `20대의 차량`처럼 조사를 사이에 둔
cue는 보지 않으므로 기본값(구간)을 따른다.
