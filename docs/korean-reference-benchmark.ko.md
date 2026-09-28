# 한국어 제공 참조: 가져오기·평가·웹 보고서

2026-09-28. **텍스트** 평가만 수행한다. 반복 가능한 첫 기준선과 검토 절차를 구축했고,
아래의 개선 이력에는 동일한 참조로 다시 측정한 제한된 엔진 변경을 기록한다.

## 자료와 사용 범위

사용자 업로드 `tn_release/summary.json`에 따르면 원본은 `159.숫자가 포함된 패턴 발화
데이터 / Training`의 스크립트다. 21개 카테고리·142,255문장·숫자263,135구간·영어9,190구간.
Validation이 Training과 같아 제외했다는 원본 설명도 보존한다. 독립 held-out 점수로 부르지 않는다.

JSON `ITN`은 엔진 입력, `TN`의 `[ ... ]`는 발화형 참조 구간이다. 대괄호 주석만 제거한
문장을 문장 참조로 사용한다. `patterns`의 `patternTheme`는 **원본 주제**다. 의미 분류가
아니므로 ‘가지’ 주제의 `94.2%`를 COUNTER로 강제하지 않는다.

원본과 문장별 결과는 ignored `benchmark/generated/ko-release/` 아래에 보관한다.
제품 rule/lexicon과 분리하고 Git에 올리지 않는다. 허가 조건은 업로드에 포함되지 않았으므로
[THIRD_PARTY](THIRD_PARTY.md)에 미확인 상태로 기록했다. 자동 다운로드는 없다.

## 지표의 의미

| 지표 | 비교 | 분모 |
|---|---|---|
| 문장 정확 일치 | 실제 출력 == 주석을 해제한 제공 참조 | 유효한 문장 참조 전체; 엔진 오류 포함 |
| 문장 공백 제거 일치 | 양쪽에서 `str.isspace()`에 해당하는 Unicode 공백만 제거 | 동일 |
| 공백만 다름 | 정확 불일치이지만 공백 제거 후 일치 | 별도 건수 |
| 구간 일치 | 전체 문장을 처리한 출력에서 원본 byte 경계로 해당 참조 구간을 투영 | 선언된 참조 구간 전체 |
| 구간 정렬 실패 | 엔진 span 경계가 참조 경계를 가로지르거나 참조 정렬 자체가 불가 | 분모에서 제거하지 않고 실패로 집계 |
| 구조 검사 통과 부분집합 | 자동 구조 경고가 없는 문장 | 별도 분모. 언어적 정답 검수라는 뜻 아님 |
| 중복 제거 부분집합 | 공백 제거 입력·참조 쌍이 같은 행을 한 번 계산 | 별도 분모 |
| 검토 파일 부분집합 | accepted/corrected 판정 문장 | 별도 분모; 원본 기준 점수는 그대로 |

공백 외의 대소문자·기호·Unicode 형태·숫자 값은 그대로 비교한다. NFC/NFKC/casefold,
기호 제거 또는 근사 발음 매칭을 적용하지 않는다. 공백 제거 일치가 의미 정확도까지 보증하지
않는다. 띄어쓰기만 다른 경우와 값·단어·기호 읽기 차이를 구별하는 보조 지표다.

불완전한 대괄호 주석은 문장 참조를 만들지 않고 **미채점**으로 표시한다. 그 문장은 전체
입력 수·사례 목록에 남는다. ITN/TN 필드 자체가 없거나 NUL 등 구조가 잘못된 행은
`quarantine.jsonl`에 원본과 사유를 보존하고 import manifest에 격리 수를 명시한다.

## 정렬·참조 품질 검사

Importer는 원문을 수정하지 않는다. TN의 literal 부분과 bracket 부분, metadata의
(written, spoken) 다중집합으로 **문장 전체를 유일하게 복원할 수 있을 때만** 위치를 부여한다.
공백만 다른 literal은 허용하되 기호·어미·내용 변경은 허용하지 않는다. 동일 surface가 여러 번
있다고 첫 위치를 임의로 선택하지 않는다. 영어 목록이 문장 끝에 선언되어도 발화 순서대로
정렬하며, 복수 해석·서로 다른 주제의 중복 metadata·탐색 한도 초과는 검토 신호다.

다음은 자동 경고이지 확정된 정답 오류가 아니다:

- 괄호/metadata 불일치, NumberWord/EnglishWord와 patterns의 불일치.
- 주석 밖 원문·참조 내용 변경, source에 없는 구간, 유일하지 않은 정렬.
- 같은 공백 제거 입력에 서로 다른 공백 제거 참조가 있는 충돌 그룹.

점수를 올리려고 참조를 엔진 출력으로 교체하거나 경고 행을 전체 점수에서 제거하지 않는다.
원본 exact/공백 제거/구간 정렬/엔진 오류 지표를 함께 유지한다.

## 패턴·사전·명시 제어의 분리

| 수정 방향 제안 | 근거 예 | 다음 조치 |
|---|---|---|
| pattern | 수량·달력·단위·통화·퍼센트·전자주소의 표기 | 값/단위 IR과 문맥을 대조, 일반 규칙 변형·부정 계약 작성 |
| lexicon | Latin 발음, 관용 제품명·혼합 코드 | 관용형/약어 여부와 적용 범위를 검토; user/customer/system 사전 구분 |
| context-control | 번호·식별자 또는 복잡한 주소형 구간 | 문맥으로 결정 가능한지 먼저 검토; 불명확하면 @digits/@phone/@address 등 명시 의도 활용 |
| review | 구조 경고 또는 원인 판단 불가 | 원문·참조·정책을 사람에게 확인 |

이는 입력 형태와 원본 주제에 따른 **제안**이다. 해당 범주의 모든 항목을 그 방식으로
고쳐야 한다는 결론이 아니다. 특히 번호류도 충분한 문맥이 있으면 일반 규칙으로 개선할 수 있다.
평가 시 사전이나 제어 구문을 자동 삽입하지 않는다. 자동 TN과 보조 제어 결과를 섞지 않는다.
구간별 엔진 의미 분류/규칙, 원본 주제, 제안 근거를 웹 사례 상세에 함께 표시한다.

개인고유번호의 `2722`를 `둘일곱둘둘`처럼 읽는 의도는 사용자가 선택할 수 있다.
후속 제어 평가에서는 문장별 의도를 먼저 검토하고, 같은 부분집합에서 자동 입력과
제어 입력의 exact/공백 제거 일치 및 적용 건수를 나란히 측정한다. literal alias로
참조를 직접 준 결과와 일반 패턴 제어를 분리한다. 현재는 이 평가를 구현하거나
제어 정확도를 발행하지 않았다. `@native`는 정수 전체의 고유어 수사이며 자리별
고유어 숫자 읽기 모드가 아니다. 자세한 순서는 [remaining-work](remaining-work.md)에 있다.

## 개선 이력: 횟수 `N번` (2026-09-28)

첫 대상은 **고유어기수**의 횟수다. 초기 조사에서 `번(횟수)` 주제의 `N번` 구간
불일치786건, 해당 구간만 불일치인 문장581건을 확인했다. 이 수는 개선 완료 건수가 아니다.
문맥이 직접 드러나는 `하루에 3번`, `3번 연습`, `3번씩`을 먼저 보강하고,
번호·노선·문항의 반례를 자체 계약으로 추가했다. 구현·편집법은 [횟수 가이드](korean-counters.md).

동일한 제공 참조·설정·평가기에서 `20260928-baseline`과 `20260928-frequency-v3`을 비교했다.
기준선·최종 스냅샷과 실패했던 중간안도 로컬 `artifacts/tn/ko-frequency/`에 보존한다.

| 문장 지표 | 이전 | 이후 |
|---|---:|---:|
| 고유어기수 정확 일치 (13,267건) | 5,630 (42.44%) | 5,689 (42.88%) |
| 고유어기수 공백 제거 일치 | 11,040 (83.21%) | 11,145 (84.01%) |
| 전체 정확 일치 (유효 참조142,248건) | 16,896 (11.88%) | 16,963 (11.92%) |
| 전체 공백 제거 일치 | 80,114 (56.32%) | 80,242 (56.41%) |

출력171문장이 바뀌었다. 공백 제거 기준 새 일치129건, 일치 상실1건, 순증128건이다.
상실1건은 `26번의 우승`: 제공 참조의 한자어 수사와 횟수 정책의 고유어 수사가 다르다.
이 항목을 제외하거나 참조를 바꾸지 않았으며 독립 검토 대기로 기록했다.
문장 전체가 아직 불일치인 나머지41건도 부분 변화까지 살펴봤다. 초기 후보에서
노선명·번호 문맥을 잘못 바꾼 단서를 제거한 뒤 최종 자료 전체를 다시 평가했다.

구간 공백 제거 일치는173,929 → 174,100건이다. 정렬 실패39,782건·미채점 문장7건·
구조 경고1,349건은 그대로이고 엔진 오류는0이다. 참조/overlay 수정과 제어 자동 삽입은 없다.
이 자료는 여전히 탐색에 사용한 Training 참조이며, 이 수치를 held-out 성능으로 부르지 않는다.

## 개선 이력: 공통 발음 사전 (2026-09-28)

사용자 결정에 따라 공통23항목(24키)은 system에, 선택형9키는 명시적으로 적용하는
고객/사용자 예제로 나눴다. 기본 보고서 `20260928-lexicon-v2`에는 선택형을 넣지 않았다.
직전 횟수 보고서 대비 문장 정확 일치16,963→16,967, 공백 제거 일치80,242→80,318,
구간 공백 제거 일치174,100→174,388이다. 새 문장 일치77 / 상실1을 모두 남긴다.
상실은 IELTS의 철자 읽기와 공식 한국어 표기의 차이며 참조를 수정하지 않았다.
후보 수집기, 실제 선택 근거, 붙은 숫자 읽기에 미치는 영향 및 재현 경로는
[사전 보강 가이드](lexicon-coverage-v22.ko.md)에 있다. 독립 held-out 측정이 아니다.

## 반복 가능한 실행

```sh
# 최초 한 번: 사용자 자료를 영구 로컬 경로로 보관 (Git ignored)
mkdir -p benchmark/generated/ko-release/source
cp -a /tmp/tn_release/. benchmark/generated/ko-release/source/
python3 benchmark/importers/korean_release/import_release.py \
  --source benchmark/generated/ko-release/source \
  --out benchmark/generated/ko-release/import-v1

# 새 엔진/리소스마다 별도 스냅샷과 보고서 이름을 사용한다.
cmake -S . -B build-ko-report -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
python3 tools/quality/tn_cycle.py snapshot --build build-ko-report --resources resources \
  --out benchmark/generated/ko-release/engine-NEXT
python3 benchmark/evaluators/korean_release.py \
  --import-dir benchmark/generated/ko-release/import-v1 \
  --library benchmark/generated/ko-release/engine-NEXT/libtts_text_frontend.so \
  --resources benchmark/generated/ko-release/engine-NEXT/resources \
  --out benchmark/generated/ko-release/reports/NEXT \
  --label '변경 내용을 식별할 수 있는 이름'
```

기존 output 디렉터리는 덮어쓰지 않는다. source summary·21파일·가공 corpus·라이브러리·
리소스 tree·평가기·adapter·정렬 helper·검토 파일의 hash를 기록한다. 측정 시작/종료 사이
입력·리소스·평가기 코드가 바뀌면 완료 보고서 발행을 거부한다. dirty checkout에서도 재현
근거를 보존하도록 `harness/`에 평가 코드 사본을 저장한다. native library는 freeze한 채
반복 사용하고 CLI를 14만 번 기동하지 않는다.

`summary.json`은 DB와 모든 집계를 완성한 뒤 마지막에 생성한다. 웹은 완료된 보고서만
읽는다. 사례/IR은 SQLite의 압축 JSON으로 저장하고 50개씩 조회한다. 서버에서 benchmark를
실행하거나 리소스를 변경하는 API는 없다. 같은 자료·옵션·평가기·검토 파일인 기록만 증감을
계산한다. library/resource 차이는 개선 측정 대상이므로 비교를 허용한다.

## 참조 검토 파일

웹에서 개별 사례의 ‘검토 양식 JSON’을 내려받는다. 아직 reviewer/reason/date가 비어
있으므로 그 자체로 승인되지 않는다. 단일 JSON 객체·JSON 배열·JSONL을 지원한다.

```json
{
  "id": "ko-release/CATEGORY/000001",
  "record_sha256": "원본 사례의 64자리 SHA256",
  "status": "needs-review",
  "reviewer": "검토자",
  "reviewed_at": "2026-09-28",
  "reason": "판정 근거",
  "route": "context-control"
}
```

- `accepted`: 기존 참조 사용. `corrected`: 검토한 `expected` 필수.
- `excluded`: 검토 부분집합에서 제외, 원본 지표에서는 유지.
- `needs-review`: 계속 대기. `route`는 선택적으로 문장의 제안을 변경하며 문장 사례
  필터에 반영한다. 구간별 제안·집계는 원본 입력에서 얻은 분류를 유지한다.
- 빈 검토자·사유·날짜, 중복/없는 ID, stale fingerprint는 거부한다.
  기록된 검토자 이름의 신원을 인증하는 서비스는 아니다.
- `--reviews LOCAL.jsonl`과 새로운 `--out`으로 실행한다. 검토 후 지표는 별도 표시하고
  원본 제공 참조 지표를 덮어쓰지 않는다. 외부 문장이 포함된 review 파일도 ignored 경로에 둔다.

## 웹과 검증

`/ko-quality`에서 21카테고리, 원본 주제, 수정 방향, 엔진 의미 분류, 기록 비교, 사례 검색,
문장/구간 필터, 참조 경고, 실제 IR, 집계 다운로드를 확인한다.
`--ko-report-root`로 로컬 보고서 경로를 지정할 수 있다. 기본은
`benchmark/generated/ko-release/reports`다. raw source 디렉터리는 HTTP로 제공하지 않는다.

```sh
ctest --test-dir build --output-on-failure -R 'korean_release_|web_workbench|external_benchmark_evaluator'
TTF_PLAYWRIGHT_MODULE=/path/to/playwright node tests/web/korean-quality.cjs http://127.0.0.1:8000
```

CI는 원본 데이터가 없어도 자체 합성 fixture로 정렬·공백 지표·분모·오류·검토 수정·해시
변경·native end-to-end·웹 필터·경로 제한을 검사한다. 전체 외부 데이터는 자동 다운로드하거나
CI artifact로 공개하지 않는다. 전체 평가 결과와 다음 작업 순서는 HANDOFF에 기록한다.

## 첫 전체 기준선 (2026-09-28)

보고서: `benchmark/generated/ko-release/reports/20260928-baseline/`.
자료 `import-v1`, Release 엔진 `engine-20260928`을 고정했다. 이 시점 엔진은 HEAD
`0012911`에 기존 미커밋 제어 구문·NUM-WIDTH 등이 포함되어 있어 commit만으로 식별하지
않는다. 정확한 library/resource/harness SHA는 보고서에 있다.

| 단위 | 전체 | 채점 분모 | 정확 일치 | 공백 제거 일치 | 공백만 다름 |
|---|---:|---:|---:|---:|---:|
| 문장 | 142,255 | 142,248 | 16,896 (11.88%) | 80,114 (56.32%) | 63,218 |
| 참조 구간 | 272,325 | 272,325 | 76,757 (28.19%) | 173,929 (63.87%) | 97,172 |

엔진 오류 0. 문장 참조 주석 오류 7건은 미채점으로 남겼고, 구간 정렬 실패 39,782건은
분모에 포함했다. 자동 구조 경고 문장 1,349건, 중복 입력 행 233건, 입력별 참조 충돌 그룹
0개다. 사람이 승인한 참조는 아직 0건이다. 이 수치는 첫 기준선이며 개선량이 아니다.

원본 주제별 **문장 공백 제거 일치**는 금융·은행 16.90%, 전화번호 15.41%, 날짜·시간
84.64%, 통화·금액 84.69%다. 번호 읽기의 `공/영`, 묶음·구분자, 마스킹 정책을 확인한 후
금융 규칙군을 선택한다. 낮은 일치율만으로 규칙 실패나 사전 부족이라고 확정하지 않는다.
21개 전체 카테고리와 수정 방향 제안은 웹에서 원문·참조·출력·IR과 함께 검토한다.
