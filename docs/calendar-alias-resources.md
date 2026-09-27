# 월·요일 별칭 편집 가이드

2026-09-27, v0.21 작업분 / public ABI v1 유지. 이번 범위는 **날짜에 바로 붙은
괄호 안 영문 요일**이다. 월 별칭 24개는 기존 기능을 전수 확인했다.

## 처리 예

| 입력 | 기본 정규화 결과 |
|---|---|
| `마감은 5/8(Fri)입니다.` | `마감은 오 월 팔 일(금요일)입니다.` |
| `5/8(Fri.)` (en-US) | `May eighth(Friday)` |
| `8 May 2026(Fri)` (en-US) | `the eighth of May twenty twenty six(Friday)` |
| `2026-05-08(Mon)` (en-US) | `May eighth two thousand twenty six(Monday)` |
| `We sat in the sun.` (en-US) | 원문 유지 |
| `5/8 (Fri)` (en-US) | `five over eight (Fri)` |

앞의 날짜가 `DATE`로 분류되고, day를 포함하며, 닫는 위치가 `(`에 바로 이어져야 한다.
숫자 날짜와 영문 월 이름 날짜를 지원한다. `Fri`, `Friday`와 대소문자 변형,
끝의 점 하나를 허용한다. 주어진 요일과 날짜의 실제 달력 일치 여부를 계산하거나
수정하지 않는다. 연도만 있는 `2026(Fri)` 및 월까지인 `May 2026(Fri)`는 대상이 아니다.

`5/8`의 날짜/분수 모호성은 기존 `weekday_label` 단서표로 해결한다. 이번 변경은
그 단서의 ASCII 대소문자와 끝 점 처리를 통일하고 영문 요일을 추가한다.
한글 `(금)` 등 기존 14개 단서는 유지하며, 이 약자를 `금요일`로 확장하는 기능은
이번 범위에 포함하지 않는다.

## 사람이 편집하는 표

TSV는 `kind<TAB>language<TAB>key<TAB>value`의 네 열이다.

| Kind / language | Key / value | 위치 |
|---|---|---|
| `month_alias / en-US` | 소문자 월 이름·약자 → 1..12 | `resources/languages/en-US/rules/dates.tsv` |
| `weekday_label / en-US` | 소문자 영문 요일 → 1..7, 0은 비활성 | 같은 파일 |
| `weekday / en-US` | 1..7 → Monday..Sunday | 같은 파일 |
| `weekday / ko-KR` | 1..7 → 월요일..일요일 | `resources/languages/ko-KR/rules/numeric.tsv` |
| 기존 `weekday_label / ko-KR` | 한글 단서 → 0 또는 1 | 같은 파일 |

요일 identity는 월=1, 화=2, 수=3, 목=4, 금=5, 토=6, 일=7이다.
**KO 단서표의 1은 활성 여부**이며 EN 표의 요일 identity와 다르다.
월 별칭은 기존 [영어 날짜 가이드](english-dates.md)의 `boundary_word` 동반 규칙을 따른다.
새 요일 별칭은 괄호 안에 한정되므로 그 동반 행이 필요하지 않다.

EN 요일 alias key는 1..31자의 ASCII 소문자이며 점을 넣지 않는다. Value는 한 자리
0..7만 허용한다. 출력 `weekday` key는 한 자리 1..7이고, 값은 앞뒤 ASCII 공백과
제어문자가 없는 비어 있지 않은 UTF-8 읽기다. TSV/TTFB 모두 같은 검증을 거친다.
출력표를 제거한 사용자 resource root에는 사용하려는 identity의 읽기를 보충해야 한다.
사전 검증 통과는 언어적 정답에 대한 사람의 승인을 뜻하지 않는다.

## 고객별 변경 예

아래 heredoc의 열 구분자는 실제 탭이다. 고객이 약자 `Thurs`를 추가하고 금요일을
`금`으로 읽도록 선택하는 예다. 기본 표를 고칠 필요가 없다.

```sh
cat > /tmp/calendar-policy.tsv <<'EOF'
weekday_label	en-US	thurs	4
weekday	ko-KR	5	금
EOF
build/ttf validate-resource --resources resources --require-complete-validation \
  --resource-layer customer /tmp/calendar-policy.tsv
build/ttf normalize --resources resources --language ko-KR --output-mode speech \
  --customer /tmp/calendar-policy.tsv '마감은 5/8(Fri)입니다.'
# 마감은 오 월 팔 일(금)입니다.
build/ttf inspect --resources resources --language ko-KR --trace \
  --customer /tmp/calendar-policy.tsv '마감은 5/8(Fri)입니다.'
```

`weekday_label en-US fri 0`은 `fri` 약자만 끈다. `friday`도 끄려면 별도 행이 필요하다.
고객 > 도메인 > 시스템 순으로 선택하며 같은 layer의 동일 key는 첫 선언을 사용한다.
일반 단어 `sun`, `sat`, `wed`를 통째로 바꾸는 발음 사전 행을 추가하지 않는다.

## IR·추적·스트리밍

`Fri.`는 `DATE(value="5", unit="weekday", secondary="fri")`가 된다.
`source_lang=en-US`, `context_lang` 및 `render_lang`은 주변 문맥에 따라 선택한다.
룰은 `en.date.weekday`다. Inspect의 span은 원래 입력 위치와 분류에 참여한 최대 layer를 유지하고,
상세 trace는 선택된 `weekday_label`과 출력 `weekday` 행의 key/layer를 보여 준다.
날짜와 괄호를 하나의 span으로 합치지 않아 `--brackets read|preserve|skip`이 유지된다.

분류기는 기존 날짜 분류를 재사용하며 앞쪽 최대 96바이트를 확인한다. 날짜와 괄호 사이
공백, `Mon., May 8`처럼 요일이 먼저 오는 형식, 한자 요일, 대체 괄호, 매우 긴 날짜
표기 및 일반 문장 속 요일 약자의 확장은 후속 범위다. 코어에 언어별 요일 이름 분기를
추가하지 않았으며, 출력표와 언어팩 렌더러가 공통 의미값을 처리한다.

## 고정 시험과 기준문 검토 이력

`tests/evaluation/calendar_alias_v21.jsonl`: 월 별칭 24건 + 요일 28건 + 변형 11건 +
보존·분수 9건 = 72건. 제품을 바꾸기 전에 직접 작성한 합성 기준문이다.
최초 precheck의 분수 negative 3건은 `five eighths`였으나 기존 프로젝트 정책
`number/en-US/fraction_over=over`와 충돌했다. 구현 전 기준문 검토에서
`five over eight`로 정정했다. 어느 표현도 보편적으로 틀렸다는 판정이 아니다.
원본과 최초 결과는 `artifacts/tn/calendar-alias/reference-before-policy-review.jsonl`,
`precheck/`에 보존했다. 이후 corpus를 고정해 **양쪽 바이너리에 동일 파일**을 사용한다.
그 정정 3건은 개선 건수에 넣지 않는다. 기존 활성 corpus의 기대값 변경은 없다.

```sh
ctest --test-dir build --output-on-failure -R 'calendar_alias|resource_semantic_validation'
python3 tools/quality/tn_cycle.py cycle \
  --baseline artifacts/tn/calendar-alias/baseline \
  --candidate artifacts/tn/calendar-alias/final-candidate \
  --corpus tests/evaluation/calendar_alias_v21.jsonl \
  --out artifacts/tn/calendar-alias/NEW-comparison
```

현재 기준문은 독립적인 사람 검수 대기 상태다. 외부 한국어 gold나 음성 평가는 없다.
영문 짧은 요일·월 이름의 표기는 [RFC 5322 §3.3](https://www.rfc-editor.org/rfc/rfc5322#section-3.3)을
참고했지만, 여기의 괄호 문법과 요일을 주어진 대로 읽는 정책은 프로젝트 자체 규격이다.
RFC 이메일 날짜 문법 적합성이나 요일-날짜 일치 검증을 주장하지 않는다.
