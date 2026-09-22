# TN 실패 사례를 엔진 개선으로 연결하는 작업 절차

## 범위와 비용

한 작업은 **한 규칙군**으로 제한한다. 금융 값 손실·오독을 먼저, 콘텐츠 제작의
이름·문자·단위·서식을 다음으로 다룬다. 일반 모델로 재현·리소스 편집·계약 검사를
진행할 수 있도록 도구는 Python 표준 라이브러리와 기존 C CLI만 사용한다.
어떤 LLM API나 특정 모델 등급에도 의존하지 않는다. 메모리 손상, ABI 변경,
동시성 같은 증거가 있는 문제만 별도 심층 검토 대상으로 올린다.

시작할 때 HANDOFF → 작업 큐 → 해당 pack/resource 순으로 읽는다. 전체 대화,
외부 논문, 모든 과거 보고서를 매번 다시 읽지 않는다. 한 이슈당 목표 12~30개의
독립 변형/부정 예를 먼저 정하고, 변경·검증·보고 후 종료한다. 정확도를 100%로
만드는 무기한 goal을 자동으로 재개하지 않는다.

## 1. 사례 접수: 정답을 먼저 고정

필요한 입력: 원문, 원하는 출력, ko/en 기본 문맥, text/speech 모드, plain/annotated,
보존/제거, 고객·도메인 리소스, 원하는 읽기의 근거. 민감한 계좌/개인정보는
구조를 유지해 익명화한다. 정답이 불분명하면 **검토 필요**로 남기고 자동 생성한
출력을 정답으로 승격하지 않는다. 제품명은 보통 pronunciation lexicon, 일반
숫자/단위 문법은 classifier/verbalizer, 일회성 읽기는 annotated 제어에 속한다.

```sh
python3 tools/quality/tn_cycle.py case \
  --id content-k7 --category content/alnum --text 'k7 타고 다닌다' \
  --expected '케이칠 타고 다닌다' --source user-report \
  --reason '한 글자 이름과 정수, 차종 관용형은 사용자 override' \
  --out artifacts/tn/k7/case.jsonl
```

생성 파일은 일반 평가기 JSONL이다. 같은 파일을 덮어쓰지 않는다. `notes`에는
출처와 검토 대기 상태를 남긴다. expected_spans에 `value`, `secondary`, `unit`,
`source_lang/context_lang`, semantic_type도 추가한다. 예를 들어 숫자가 읽혔어도
원래 금액이 바뀌면 실패다. 범위 양끝, 선행 0, 조사, 단위, 부호, 공백, 문장 끝,
영어 문맥, 잘못된 형태와 사용자 override를 골라 변형을 만든다.

## 2. 수정 전에 baseline 고정

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
python3 tools/quality/tn_cycle.py snapshot --build build --resources resources \
  --out artifacts/tn/k7/baseline
```

snapshot은 해당 checkout의 CMake build를 끝낸 후 CLI·shared library·리소스를
복사하고 해시를 남긴다. 기존 출력 폴더는 거부한다. 다른 revision은 그 revision의
독립 checkout에서 snapshot을 만든다. git HEAD만 같아도 dirty build는 다를 수
있으므로 snapshot.json의 artifact hash가 실제 기준이다. snapshot은 로컬 OS용이며
외부 corpus나 binary를 Git에 올리지 않는다.

## 3. 원인별 최소 변경

| 증거 | 수정 위치 | 피할 것 |
|---|---|---|
| 경계/소비 범위가 잘못됨 | tokenizer/classifier/segmenter | 문장 전체 exact 예외 |
| 값·단위·범위 IR이 틀림 | semantic classifier/pack classifier | 출력 문자열만 임시 치환 |
| IR은 맞고 읽기만 틀림 | pack verbalizer, reading resources | classifier에 발음 문자열 혼합 |
| 관용 제품명/약어 | managed lexicon, 사용자/고객 사전 | G2P 출력을 검수 없이 정답 등록 |
| 문맥 선택이 틀림 | pack context cues/공유 경계 | core에 언어별 if 추가 |
| 미지원 문자/기호 | speech_character/speech_symbol 정책 | default 원문 보존을 몰래 제거로 변경 |
| 참조 자체의 차이 | 근거를 기록하고 별도 reference 검토 | exact 점수를 casefold로 대체 |

리소스는 `validate-resource --require-complete-validation`과
`inspect --trace`로 검사한다. [일반 리소스 가이드](resource-editing-guide.ko.md),
[발화 정책](speech-resources.md), [입력 제어](input-format.md)를 참고한다.

## 4. 같은 정답으로 candidate 비교

```sh
python3 tools/quality/tn_cycle.py snapshot --build build --resources resources \
  --out artifacts/tn/k7/candidate
python3 tools/quality/tn_cycle.py cycle \
  --baseline artifacts/tn/k7/baseline --candidate artifacts/tn/k7/candidate \
  --corpus artifacts/tn/k7/case.jsonl --out artifacts/tn/k7/comparison
```

최대 200건을 같은 참조와 1/7/64바이트 streaming으로 평가한다. `cycle.json`,
`comparison/comparison.html`, before/after의 text·span report를 남기고, 실패한
candidate의 처음 20건은 `failed-traces/`에 inspect trace를 저장한다.

- 종료 0: candidate 계약 통과, 회귀/문자 오류 증가 없음.
- 종료 1: candidate 실패 또는 회귀. baseline 실패는 개선 측정의 정상 입력이다.
- 종료 2: 도구 오류, 다른 참조/설정, 훼손된 snapshot, 불완전한 평가.

도구는 엔진을 자동 수정하거나 expected를 바꾸지 않는다. 보고서를 보고 다음
한 번의 수정 여부를 결정한다. 비교 대상 폴더 재사용·snapshot 변경은 거부한다.
음성은 생성하지 않는다. snapshot의 옵션 미지원은 숨기지 않으며, v0.19에는
새 speech 옵션이 없으므로 새 모드의 baseline은 v0.20 이상에서 따로 만든다.

## 5. 회귀 코퍼스로 승격하고 종료

1. 재현이 실제 최신 버전의 문제인지 확인한다. 이미 고쳐졌다면 회귀 사례만 추가한다.
2. 권리·개인정보·정답 근거를 확인한 자체 사례만 tests/evaluation의 해당 코퍼스로
   이동한다. 기존 파일에 합칠 경우 id 중복을 피한다.
3. 새 파일은 CMake의 evaluation gate, tools/web/workbench.py CORPORA,
   tools/evaluation/audit_references.py inventory, reference_reviews.json에 등록한다.
   human-approved는 실제 독립 검수가 있을 때만 사용한다.
4. 개발 중에는 관련 CTest만 실행한다. release 전 build/CTest, 필요 sanitizer,
   새 corpus 및 browser 영향 검사를 한 번 실행하고 CI 결과를 확인한다.
5. HANDOFF에 변경·참조 근거·수치·commit·남은 문제를 기록하고 작업을 끝낸다.
   재시도 횟수/비용이 커지면 최소 재현과 판단이 필요한 지점을 인계한다.

## 실제 데이터의 다음 순서

수치와 순서는 [quality-work-queue.json](../benchmark/manifests/quality-work-queue.json)에
있다. 이는 2026-09-17의 **개발용 plain/text 측정**이며 새 speech 품질 점수가 아니다.

1. 금융 값: MONEY 3/33, MEASURE 16/92, DECIMAL 9/35, CARDINAL 96/536 불일치에서
   실제 값 오류와 span alignment 문제를 분리한다. 값이 바뀌는 오류가 최우선이다.
2. 익명 한국어 금융 안내·콘텐츠 원고: 실제 요청에서 반복되는 조사/단위/식별자를
   하나씩 선택한다. 한국어 외부 원본 접근 권한은 importer 존재와 별개다.
3. 영어 날짜: 불일치 1,144개 중 698개는 대소문자만 다르고, 419개는 다른 출력,
   27개는 alignment 문제다. 문맥 없는 연도/수사 문제부터 작은 묶음으로 검토한다.
4. LETTERS 538건: 이니셜·dotted form·단어 보존을 구분한다. VERBATIM/DIGIT는
   읽기 정책과 실제 오인식을 분리한다.
5. ELECTRONIC/TELEPHONE/TIME/ADDRESS/FRACTION: 한 syntax군씩 다룬다. 새 speech
   모드는 별도 baseline을 만든다. 낮은 표본 수를 전체 언어 성능으로 일반화하지 않는다.
6. 금융/콘텐츠 OOV: 실제 빈도 상위 20~40개씩 managed lexicon을 검토한다.
   User/Customer override를 유지하고 출처·대안·pending review를 남긴다.

PUNCT 8,919건은 대부분 dataset의 생략과 프로젝트의 구두점 보존 정책 차이다.
점수만 올리기 위해 전체 구두점을 지우지 않는다. 데이터 권리·holdout·human review는
[THIRD_PARTY](THIRD_PARTY.md)와 [remaining-work](remaining-work.md)를 따른다.

외부 자료가 있는 환경에서의 재현:

```sh
python3 benchmark/evaluators/diagnose.py benchmark/generated/sproat-v20-final \
  > artifacts/tn/sproat-diagnostics.json
python3 benchmark/evaluators/evaluate.py --corpus LOCAL/sentences.jsonl \
  --library build/libtts_text_frontend.so --resources resources --limit 0 \
  --chunk-bytes 1 7 64 --output-dir benchmark/generated/NEW-report
```

실제 corpus는 Git에 없다. 새 환경은 benchmark/README.md의 importer/manifest를
사용해 권리가 허용하는 데이터만 확보한다. Kspon dual transcription candidate와
CoreaSpeech optional NC benchmark를 product gold/resource로 자동 전환하지 않는다.
