# 발음 사전 관리 가이드

이 사전은 영어를 한국어 문맥에서 읽을 때 사용하는 **발음** 데이터다.
번역, 숫자 TN 규칙, 영어 문맥 보존 정책과는 별개다. v0.10에서 기존 215항목을
원본 관리 체계로 옮기고 금융 86항목·콘텐츠 73항목을 추가했다.

## 1. 어디를 수정하나

| 파일 | 역할 |
| --- | --- |
| `lexicons/ko-KR/seed.jsonl` | 기존 기본 발음과 대안 |
| `lexicons/ko-KR/finance-additions.jsonl` | 금융 용어·기관명·약어 |
| `lexicons/ko-KR/content-additions.jsonl` | 제작 도구·기술·콘텐츠 용어 |
| `lexicons/sources/*.json` | 근거 자료 URL·자료 종류·확인 날짜 |
| `resources/languages/ko-KR/lexicon/**/*.tsv` | 자동 생성되는 시스템 런타임 사전 |
| `resources/domains/finance/ko-KR/lexicon.tsv` | 명시적으로 켜는 금융 발음 사전 |

**관리되는 기본 사전은 JSONL 원본을 수정한다.** 생성된 TSV를 직접 바꾸면 CI가
차이를 잡는다. 고객별 변경은 별도 customer TSV를 만들어 로드할 수 있다.
TSV·바이너리는 C에서 읽으므로 운영 런타임에 Python/JSON 파서가 필요하지 않다.

현재 총 374항목, 별칭을 포함한 376키다. 시스템은 328항목, 금융을 켰을 때
추가되는 항목은 46개다. 기본 금융 단어·기관명은 시스템에 있고, `ABS`, `PER`,
`ROE` 등 문맥에 따라 해석이 달라질 수 있는 신규 약어는 금융 scope에 있다.
ASCII 대소문자를 구분하지 않으므로 금융 scope에서는 `per`도 `PER`와 같은
발음 후보다. 운영 서비스가 문서의 도메인을 선택해야 하며, 자동 도메인 판별이나
일반적인 동음이의어 해소를 구현한 것은 아니다.

## 2. 웹에서 확인·시험·내보내기

테스트 도구의 **사전 관리**(`/lexicon`)를 연다.

1. 단어·발음을 검색하고 분류·검토 상태로 좁힌다.
2. 항목의 근거 링크, 선택 이유, 별칭과 대체 발음을 확인한다.
3. 실제 문장으로 **현재 사전으로 검사**한다. 금융 scope 항목은 금융 사전을
   포함해 시험한다. 영어 문장도 넣어 원문 보존을 확인할 수 있다.
4. 발음을 고쳐 **수정안 시험**한다. 이 요청에서만 User 계층으로 적용된다.
5. **고객 TSV 내려받기** 또는 **수정 제안 JSON**을 받는다. 다운로드/시험만으로
   서버 원본은 바뀌지 않는다. JSON은 검토용 제안이며 compiler 입력은 아니다.
6. 기본값 변경으로 채택할 때 아래 원본 수정·회귀 검증 절차를 따른다.

**입력 문서의 사전 사용 현황**은 원고에서 직접 사전, 원어 발음 사전, fallback
경로를 집계한다. 금융 원고는 금융 사전을 선택한다. 원문 영어 보존, 보호된 코드,
숫자 정규화는 집계에서 제외된다. 미등록 용어의 빈도와 실제 출력은 검토 우선순위를
정하는 자료이며, fallback 출력을 자동으로 정답 사전에 등록하지 않는다.
입력은 이 VM의 서버에서 처리하며 원고를 파일로 저장하거나 외부로 전송하지 않는다.

## 3. 원본 항목 작성

한 줄에 JSON 객체 하나를 쓴다. 다음은 기존 항목 형식의 예시다.
같은 키를 새 행으로 중복 추가하지 말고 기존 항목을 찾아 수정한다.

```json
{"id":"example-term","source_lang":"en-US","target_lang":"ko-KR","term":"Example Term","aliases":[],"pronunciation":"이그잼플 텀","category":"content","review_status":"pending","evidence":["project-seed-v09"],"notes":"편집자가 선택한 임시 발음. 실제 사용 문장과 근거 대조 필요."}
```

`evidence`에는 실제 `lexicons/sources/*.json`에 있는 ID를 넣는다. 예시의
`project-seed-v09`는 편집 정책을 뜻하며 공식 발음의 근거로 승격할 수 없다.
새 항목에는 해당 단어의 근거를 추가하고 notes에 선택 이유를 쓴다.

| 필드 | 작성 기준 |
| --- | --- |
| `id` | 변경 이력을 추적하는 고유 ID. 영문 소문자·숫자·`-`·`_` |
| `term`, `aliases` | 원어와 같은 발음인 별칭. 대소문자만 다른 별칭은 중복이다 |
| `pronunciation` | 한글 음절과 단어 사이 한 칸 공백. 자동 조사 교정은 하지 않는다 |
| `category` | 기존 분류를 우선 사용. 새 시스템 파일은 manifest에도 등록 |
| `scope` | 생략/`system` 또는 `finance`. 도메인 한정 발음은 범위를 명시 |
| `evidence`, `notes` | 출처 ID와 해당 자료가 뒷받침하는 주장, 선택 이유·모호성 |
| `alternatives` | `SQL`, `GIF`처럼 유효한 대안. 자동 선택 규칙으로 쓰이지 않는다 |
| `review_status` | 아래 상태 정의를 따른다 |

현재 authoring profile은 en-US → ko-KR다. 키는 ASCII, 정규화 후 95바이트
이내이며 다단어는 영문 단어 2~8개다. 단일 단어는 끝자리 숫자·제한된 하이픈
결합을 허용한다. `C++`, `3D`는 semantic term 리소스가 지원하는 별도 형태다.
`a1b`, `foo++`, 순수 숫자처럼 native가 한 단어로 조회하지 못하는 형태는
등록 단계에서 거부한다. 새 형태는 classifier 지원과 실제 조회 테스트가 먼저다.
공백 수와 대소문자를 정규화한 키가 동일 scope에서 겹치면 build가 실패한다.

## 4. 검토 상태는 어떻게 결정하나

- `pending`: 근거가 편집 정책이거나 한국어 읽기를 아직 충분히 대조하지 못한 상태.
  서비스 기본값으로 동작할 수는 있지만 확정된 언어적 정답이라는 뜻은 아니다.
- `source-checked`: 원기관/제조사의 한국어 표기, 공식 발음 설명 또는 명시적인
  사용자 계약과 대조한 선택이다. 공식 영문 약어의 존재만 확인했다면 한국어
  발음이 검증된 것이 아니므로 `pending`으로 둔다.
- `human-reviewed`: 별도 검토자가 실제 문맥과 읽기를 검수한 뒤 `reviewer`와
  `reviewed_at`를 남긴 상태. 컴파일러/테스트 성공으로 자동 승격하지 않는다.

이번 버전은 **출처 대조 85, 대기 289, 사람 검수 완료 0**이다. 기존 215항목을
옮겼다는 이유로 검수 완료 표시를 부여하지 않았다. official source는 서로 다른
주장을 뒷받침할 수 있으므로 항목별 notes를 읽어야 한다. 외부 사전의 대량 복사나
자동 수집은 하지 않았다. 대규모 사전을 도입할 때는 배포 허용 범위와 출처를
별도로 기록한다. 자세한 선택 근거는 [금융](lexicon-finance-v10.md),
[콘텐츠](lexicon-content-v10.md) 문서에 있다.

## 5. 생성·검증·적용

```sh
python3 tools/lexicon/manage.py validate
python3 tools/lexicon/manage.py build
python3 tools/lexicon/manage.py build --check
cmake -S . -B build
cmake --build build --parallel
ctest --test-dir build --output-on-failure

# 금융 발음만 추가. 기존 finance/rules.tsv의 숫자 정책과 독립적이다.
build/ttf normalize --domain resources/domains/finance/ko-KR/lexicon.tsv \
  'ETF의 수수료와 NAV를 확인하세요.'

# 내려받은 고객 발음은 system/domain보다 우선한다.
build/ttf normalize --customer customer-pronunciation.tsv 'ETF의 수수료입니다.'
```

원본과 생성 TSV를 함께 commit한다. 기존 발음을 바꿀 때는 입력, 변경 전후 기대
출력, 근거를 [변경 기록](audit/reference-corrections-0.10.0.json)에 남기고 기대
출력도 명시적으로 수정한다. 엔진의 실제 출력을 그대로 복사해 기대값으로 만들지
않는다. 모든 등록 키의 실제 native 조회, User/Customer 우선순위, 영어 문맥
보존, 임의 청크 입력 시 동일 출력, 실제 문장 코퍼스 회귀를 확인한다.

런타임 엔진은 freeze 후 불변이다. 배포 시 새 리소스로 새 엔진을 만들고 검증한
뒤 요청을 전환한다. 파일 수정만으로 이미 freeze된 엔진이 갱신되지는 않는다.
원본→TSV의 여러 파일 전체가 원자적으로 교체되는 기능은 없으므로 운영에서는
완성된 리소스 디렉터리를 버전별로 배포한다.

## 6. 실제 원고에서 다음 단어 고르기

```sh
python3 tools/lexicon/coverage.py input.txt --finance --output coverage.json
python3 tools/lexicon/manage.py inventory --output lexicon-inventory.json
```

자주 나타나는 fallback, 금융 수치 주변의 약어, 인명/기관명, 제작 도구 이름을
우선 검토한다. 직접 사전에 적중해도 틀릴 수 있으므로 `pending` 항목의 검토도
함께 진행한다. 같은 입력에서 수정 전후 결과·trace·청크 검증을 비교하고,
영어 원문 보존 사례와 다른 뜻의 문맥을 최소 하나씩 추가한다.

원어 발음 사전과 G2P는 아직 작은 프로토타입이다. 현재 사전의 374항목이나
회귀 성공률이 임의의 실사용 문서를 포괄한다는 의미는 아니다. 다음 작업은
실제 금융·콘텐츠 원고의 빈도 기반 보강, 문맥별 발음 정책과 별도 검토,
출처/배포 조건이 명확한 대규모 원어 발음 사전, 크기에 따라 확장되는 storage다.
현재 50,000개 합성 키 시험 결과는 [성능 점검](lexicon-scale-v10.md)에 있다.
