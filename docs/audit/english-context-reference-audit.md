# English / 문맥 reference 감사

검토일: 2026-09-16. 대상: v0.8까지의 저장소 reference. 이 문서는 에이전트의
텍스트 검토 결과다. 원어민·고객의 독립 검수나 합성음 평가를 받았다는 뜻이 아니다.
엔진 실행 결과를 정답으로 복사하지 않고 입력의 의미, 작성된 기대 문자열, 명시된
프로젝트 정책을 대조했다. 아래 검토 수는 정확도 점수가 아니다.

## 검토 범위와 수

영어 golden 128건과 code-mixed golden 37건을 모두 읽었다. JSONL 9개 파일,
408개 행을 목록화하고 아래 기준의 137개 행과 해당 span assertion 113개를 검토했다.
이 137개 행에는 과거 버전의 중복 사례가 있으며, `(입력, 기대 문자열, 기본 언어)`
기준으로는 113개다. 총 읽은 reference 행 수는 302건이다.

JSONL 선택 기준은 `default_language=en-US`, `en/*`, category에 `english`,
`code-mixed`, `context` 포함, `content/title`, `content/guard`,
`multiword/title-preserve`, `protected-syntax`, `invalid-input`이다. 한국어 수량·금융
reference 전체와 사전 발음의 적절성은 별도 감사의 범위다. 이 감사에서 제외한 행을
검토 완료로 계산하지 않았다.

| 파일 | 전체 행 | 이 감사에서 검토한 행 | 검토한 span assertion |
| --- | ---: | ---: | ---: |
| `tests/golden/en/cases.tsv` | 128 | 128 | 해당 없음 |
| `tests/golden/code-mixed/cases.tsv` | 37 | 37 | 해당 없음 |
| `tests/evaluation/advanced_cases.jsonl` | 30 | 10 | 4 |
| `tests/evaluation/cases.jsonl` | 109 | 51 | 43 |
| `tests/evaluation/challenge_cases.jsonl` | 15 | 4 | 0 |
| `tests/evaluation/content_v7.jsonl` | 40 | 14 | 27 |
| `tests/evaluation/domain_cases.jsonl` | 60 | 20 | 17 |
| `tests/evaluation/domain_cases_v8.jsonl` | 60 | 20 | 16 |
| `tests/evaluation/finance_ranges_v7.jsonl` | 24 | 0 | 0 |
| `tests/evaluation/multiword_v8.jsonl` | 30 | 13 | 0 |
| `tests/evaluation/protected_cases.jsonl` | 40 | 5 | 6 |

숫자·서수의 값과 자리, Gregorian 날짜의 월·일 및 윤년, 달러·센트 단위, 퍼센트와
퍼센트포인트의 구별, 음수 부호, 영어 보존과 한국어 발음화의 문맥을 읽어 대조했다.
정수·서수·날짜·금액의 위 검토 범위에서는 추가로 명백한 값 변경 오류를 발견하지
않았다. 이는 미검토 입력이나 모든 읽기 방식이 올바르다는 보증은 아니다.

## 1. 수정해야 하는 reference

### EN-001: `00:00 → zero o'clock`

- 위치: 감사 시작 시 `tests/golden/en/cases.tsv:48`.
- 문제: 일반 영어 시각에서 `o'clock` 앞에는 1~12시를 사용한다. 자정의 0에 이를
  붙인 `zero o'clock`을 자연스러운 기본 정답으로 고정하면 안 된다.
- 권고 정답: `midnight`. 입력에 있는 자정 의미를 유지하며 12 AM/PM의 혼동도 피한다.
- 재현 대상 확장: `0:00`, `00:00:00`. `00:30`을 자동으로 12시간제로 바꾸는 것은
  별도 정책 변경이며 이 정답 수정의 근거로 포함하지 않는다.
- 조사 중 코드에서 확인한 같은 분기의 빈틈: 13~23시의 정각 및 `00:00:01`에도
  `o'clock`을 붙일 수 있다. 이 입력들은 기존 golden의 정확도 분모에 없었다.
  새 reference를 작성해 같은 문법 오류를 막아야 한다.

근거: [Cambridge의 o'clock 정의](https://dictionary.cambridge.org/us/dictionary/english/o-clock)는
해당 표현의 시각 범위를 설명한다. [GOV.UK 날짜·시각 지침](https://www.gov.uk/government/publications/open-standards-for-government/date-times-and-time-stamps-standard)은
`00:00`을 자정 표기로 다룬다. 이 자료는 시간 의미와 영어 표현 판단의 근거이며,
이 프로젝트의 전체 TTS 출력 품질을 인증하지 않는다.

## 2. 기대 문자열이 아니라 분류·설명을 고쳐야 하는 사례

### EN-002: `24:00`을 보편적으로 잘못된 시각이라고 부름

- `tests/evaluation/cases.jsonl`의 `invalid-hour-out-of-range`.
- `tests/golden/en/cases.tsv`의 `24:00` 보존 행.
- `docs/en-coverage.md`의 Invalid times 목록.

`24:00`은 어떤 날짜의 끝을 표현하는 관례로 사용된다. 현재 parser가 00~23시만
지원하므로 원문과 `TEXT`를 유지하는 보수적 동작은 허용할 수 있다. 그러나 fixture
분류는 `unsupported-end-of-day-time` 같은 지원 범위 표현이어야 한다. `12:60`과
동일한 의미의 일반적인 잘못된 시각으로 설명하지 않는다.
[GOV.UK의 날짜·시각 지침](https://www.gov.uk/government/publications/open-standards-for-government/date-times-and-time-stamps-standard).

### EN-003: 유효하지만 미지원인 구조와 malformed 구조의 혼재

영어 golden의 Invalid structures 주석 아래에는 `2026-09-16T14:30`, `123abc`,
`3.5.7`, `192.168.0.1` 등도 들어 있다. 이 문자열들은 해당 용도에서 정상일 수 있다.
DATETIME 읽기 미지원, 숫자가 붙은 식별자 보존, VERSION/IP 보존은 서로 다른 계약이다.
`12,34` 같은 en-US 그룹 구문 오류, 지원하지 않는 구조, 보호 리터럴을 별도 분류하고,
모두 보존한다고 해서 같은 이유로 정답이라고 설명하지 않는다. 기대 원문 자체를
일괄 변경할 이유는 없다.

### EN-004: 독립 검수를 암시하는 문구

영어 golden의 `expected text is a reviewed reading policy` 주석과
`docs/en-coverage.md`의 `independent ... expected text` 표현에는 외부 검수자·검토일·근거가
붙어 있지 않다. 에이전트가 작성한 정책 fixture라는 사실과 독립적인 사람 검토 여부를
구분해야 한다. 정책 reference가 있다는 사실만으로 독립 검수 완료라고 부르지 않는다.

## 3. 허용 가능한 정책 또는 추가 합의가 필요한 읽기

아래 항목은 이번 검토에서 오류로 판정하지 않았다. 범용 자연스러움의 정답으로
내세우기보다 읽기 정책을 명시하고 수정 가능하게 해야 한다.

| 사례 / 위치 | 판단 | 권고 |
| --- | --- | --- |
| `101 → one hundred one`, `21st → twenty first` | en-US에서 허용되는 and 생략. 하이픈 대신 공백은 출력 표현 정책 | 값·순서 검증과 문장 표기 스타일 검증을 구분 |
| `2026-09-16 → September sixteenth two thousand twenty six` | 영어 월·일·연도의 읽기로 허용 | 연도를 `twenty twenty six`으로 읽는 대안은 정책 선택 |
| `14:30 → fourteen thirty` | 24시간제 값을 유지하는 정책 | 12시간제 변환과 AM/PM 추가를 임의로 섞지 않음 |
| `09:00:01 → nine o'clock and one second` | 값을 유지하지만 일반 시각 안내로는 다듬을 여지가 있음 | 명확한 clock-with-seconds 스타일을 따로 검수. 이번 자정 수정의 자동 근거로 사용하지 않음 |
| `1.0kg → one point zero kilogram`, `-1 kg → minus one kilogram` | 정확한 수치의 절댓값 1을 단수 처리하는 현재 정책 | 소수의 자릿수 표현과 문법 일치를 분리해 문서화. 오류라고 단정해 plural로 바꾸지 않음 |
| `$0.50 → zero dollars and fifty cents` | 값과 통화를 보존하지만 장황함 | `fifty cents`를 선호하는 안내문 모드는 별도 선택 |
| `1/2 → one over two` | 수학적 분수 읽기로 허용 | 일반 서술형 글의 `one half`/`half`와 별도 정책 |
| `Use 1/2 of the sample. → Use one over two of the sample.` (`content-english-fraction`, domain 원본/v8) | 수학 정책으로 해석 가능하지만 콘텐츠 안내문으로는 부자연스러움 | 콘텐츠 프로필의 독립 reference 후보는 `Use half of the sample.`. 현재 정책을 일반 자연스러움 정답이라고 표기하지 않음 |
| `9:05 AM → nine oh five a m` | AM/PM 구별을 텍스트로 명시하는 정책 | 실제 TTS가 철자를 어떻게 읽는지는 이번 텍스트 감사에서 평가하지 않음 |
| URL / email / version / IP 원문 보존 | 문자열과 의미를 손상하지 않는 보호 정책 | 발화 가능한 읽기까지 검증했다고 간주하지 않음 |

단위 표기의 참고로 [NIST의 SI 작성 지침](https://www.nist.gov/pml/owm/writing-si-metric-system-units)은
수치에 따른 단복수 표기를 설명하지만 발음 방식은 그 문서의 범위에 포함하지 않는다고
명시한다. 따라서 이를 모든 TTS 단위 읽기의 단일 정답 근거로 확대하지 않았다.

## 4. KO/EN 문맥 및 제목 reference

### 사용자 요구에서 직접 정해진 계약

- `1st는 ... → 퍼스트는 ...`, 영어 문장의 `1st → first`.
- `Apple은 ... → 애플은 ...`, 영어 문장에서는 `Apple` 보존.
- 한글 발음화는 번역이 아니다. `GPU and CPU를`의 `and → 앤드` 역시 같은 정책이다.

이 사례는 프로젝트의 명시된 요구와 일치한다. 한국어 서수 `첫 번째`로 바꿔야만
정답이라고 재정의하면 원래 요구와 충돌한다.

### 휴리스틱 정책으로 분류할 계약

`제목은 The 1st day입니다.` 및 v0.7의 영상·작품·에피소드 label은 제목의 영어
lexical run을 보존하는 정책이다. 반면 `제목은 21st입니다.`처럼 서수만 있는 제목은
한국어 발음화한다. 두 출력 모두 명시된 정책과 일치하지만, 이 구별이 모든 콘텐츠
제작자의 의도와 일치하는 객관적 언어 정답은 아니다.

`multiword-title`, `multiword-title-finance`, `multiword-title-news`에서 브랜드·기관명
문자열을 보존하는 것도 같은 cue 정책이다. `새영상제목은`이 등록된 `영상제목은`과
다른 토큰이라는 negative reference는 토큰/리소스 동작 검증이다. 사람의 제목 의도를
정확하게 이해했다는 근거로 계산하지 않는다.

코드·파일 경로 안의 문자는 언어 점수에 기여하지 않는다는 보호 reference와, 영어
완결절의 마지막 단어까지 영어로 유지하는 reference는 기능 계약으로 유지할 수 있다.
고객별로 제목을 한국어 발음으로 읽을지 영어로 유지할지 선택할 수 있어야 한다.

### span 변경과 언어 정답 변경의 구별

`domain_cases.jsonl`의 `content-product-watch`는 `Apple`, `Watch`를 두 span으로
검사하고, `domain_cases_v8.jsonl`은 `Apple Watch`를 하나로 검사한다. 입력과 기대
문자열은 같다. 이 변경은 다단어 사전의 추적 단위 변경이며 언어 품질 상승 사례가
아니다. 기존 원본을 남겨 두고 메타데이터 계약 버전 변경을 기록한 방식이 적절하다.

## 5. 권고하는 reference 관리

1. `meaning`, `reading_policy`, `reference_status`, `rationale`, `reviewer`와 검토일을
   별도 ledger에 기록한다. 사람 검수 전에는 `human_reviewed`로 표시하지 않는다.
2. `confirmed-error`, `explicit-project-policy`, `provisional-pronunciation`,
   `unsupported-preserve`를 구분한다. 성공률 하나로 모두 합쳐 품질을 표현하지 않는다.
3. 잘못된 기대값을 발견하면 기존 엔진과의 일치보다 의미를 우선해 새 기대값을 정한다.
   수정 전 값·근거·영향받는 fixture ID를 남긴 뒤, 구현이 새 reference를 통과하는지 본다.
4. 제목 정책·분수 스타일·시간 스타일의 대안은 테스트를 통과시키기 위한 임시 예외가
   아니라 사람이 편집하는 명시적 resource policy로 관리한다.
5. 숫자·단위·문맥별 선택 이유가 다른 실제 고객 입력을 별도 동의된 검토 절차로 수집한다.
   현재 합성 corpus 점수를 고객 입력 전체 정확도로 소개하지 않는다.

## 감사 시작 시 snapshot

다음 값은 수정 전 파일을 식별하며, 이후 정답 수정 때문에 해시가 달라질 수 있다.

- English golden (128행): `743bf0b80d6c44dc3d393661c03e0910fc980600636f258e3d89d1b2ba6b5428`
- Code-mixed golden (37행): `0925601bd1c8b893504b3ec4369c60208a585346ded8b658637d1d85a8fc74a0`
- Evaluation base (109행): `4fdd5145c34f8dcd12d8f6c052e3c084de18f8fea846c09172229f018810b9f7`
- Domain original (60행): `b3a4837a041b99475720fd5566c2e981197282ee0156514b7d73a374fd8882c8`
- Domain v8 (60행): `d3576fe172b36e5aff54c2dc564834337062e344617a640ee11c4c86ec5e0759`

## EN-001 검토 후 반영한 수정

감사 뒤 아래 기대값을 엔진 실행 전에 작성하고 구현을 수정했다. 기존 English
128개 reference 중 `00:00`의 기대값을 변경했고, 관련 정각·자정 경계 9개를 추가하여
English golden은 137개가 되었다. 위 검토 범위 표와 snapshot은 수정 전 기준이다.

| 입력 | 수정된 기본 텍스트 | 성격 |
| --- | --- | --- |
| `00:00`, `0:00`, `00:00:00` | `midnight` | 잘못된 `zero o'clock` 제거 |
| `00:00:01` | `one second after midnight` | 자정 이후 초를 자연스럽게 구별 |
| `00:00:59` | `fifty nine seconds after midnight` | 초의 값 유지 |
| `13:00` | `thirteen hundred hours` | 24시간제 정각을 유지하는 명시적 프로젝트 정책 |
| `23:00:00` | `twenty three hundred hours and zero seconds` | 초가 적힌 정밀도 표현을 유지하는 정책 |

단어는 `resources/languages/en-US/rules/numeric.tsv`의 `clock` 항목
`midnight`, `after`, `h24_full_hour`에서 읽는다. 고객 리소스가 이 문구를 바꿀 수 있다.
`01:00`~`12:00`의 기존 `o'clock` 정책과 분이 0이 아닌 시각의 읽기는 유지했다.
따라서 이번 수정은 모든 24시간제·AM/PM·초 표현을 새로 검수해 완성했다는 뜻이 아니다.
`24:00`과 DATETIME의 parser 지원 범위도 넓히지 않았다.

새 `tests/integration/test_english_clock_reference.py`는 24개 명시적 시각 reference,
본문 문맥, customer/domain 문구 변경, 기본/작은 segment target,
1/7/4096바이트 청크에서 총 174개 text/metadata/stream/resource 검증을 수행한다.
최초 candidate 실행 전에 작성된 파일 SHA-256은 다음과 같고, 실행 후에도 같았다.

```text
8a635ef476855eb51edbda90327c8463cd60e5ab1b9a6f796c5486d16f5b1377
```

로컬 GCC Debug 빌드에서 새 174개 검증과 English golden / code-mixed golden /
language-pack extension의 CTest 3개가 통과했다. 저장소 전체 최종 matrix 결과는
상위 검증 보고서에서 별도로 확인한다.
