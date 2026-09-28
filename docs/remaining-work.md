# 남은 요구사항과 완료 증거

감사 기준: **2026-09-17, v0.13.0 (`ea0de28`)**. 이 문서는 원래 요청의 범위를
유지하면서 후속 작업을 추적한다. 감사 중 다른 작업자가 수정하는 파일은 완료
증거로 사용하지 않았다. 아래 체크박스는 관련 수정, 독립적인 계약 검사, 실제
검증 결과를 함께 기록한 뒤 닫는다. 새 기능 추가나 테스트 통과만으로
"남은 한계가 없다"고 판단하지 않는다.

금융을 먼저 다루되 콘텐츠 제작, 일반 한국어·영어, 코드믹스도 범위에 포함한다.
평가는 **출력 텍스트·Semantic IR·발음 표현·콜백 동작**이다. 합성음 생성이나
음성 비교는 완료 조건에 추가하지 않는다. 일본어 전체 구현, 동적 plugin loader,
음소 전용 최상위 API는 최초 구현의 필수 조건이 아니며, 그 부재와 확장 구조의
결함을 구분한다.

## 이미 확인한 기반과 증거의 범위

### 현재 우선순위: 한국어 공백 차이 (2026-09-28)

사용자 요청으로 공백 차이를 우선 검토한다. 첫 범위는 한글 맞춤법 제44항의
**만 단위 수 묶음**이다. `만이천오백 → 만 이천오백`을 pack 리소스로 적용했다.
같은 외부 Training 자료에서 문장 정확 일치16,967→18,182(+1,215),
공백 제거 일치80,669는 동일하다. 변경16,459문장 전부 공백 외 문자열과 의미 IR을
유지했다. 참조 수정/제외는 없다. 자세한 검증은 [HANDOFF](HANDOFF.md),
[정책·편집 가이드](korean-spacing.ko.md)에 기록한다.

후속 **KO-SPACING-02**에서는 나이·차수·학년의 수량 명사를 처리했다.
`15세인 → 십오 세인`, `1차 → 일 차`, `3학년 → 삼 학년`이며 자원표로
단위·정확한 조사/어미·숫자와 붙은 형태 제한을 관리한다. 공백 뒤 명사 ‘차’는
차수로 보지 않아 `0987 차는 → 공구팔칠 차는`을 유지한다.
나이·생년월일5,439문장의 정확 일치119→511, 공백 제거5,054→4,999다.
전체는 정확18,182→18,977, 공백 제거80,669→80,523이다. 기존 수량 범위 정책이
양끝 단위를 읽으므로 끝 단위만 쓰는 참조와의 공백 제거 일치148건을 잃는다.
이 차이를 숨기거나 참조를 바꾸지 않았다. 신규32건과 정책 원장, 전수 비교는
[HANDOFF](HANDOFF.md)에 있다. `코로나19`의 관용명 읽기는 아직 남았다.

annotated 위치 제어 `{@break:1|2|3}`도 추가했다. 공백/쉼표+공백/마침표+공백을
그대로 삽입하며 3단계는 출력 callback을 확정한다. 새30건과 임의 byte 분할로
검증한다. 구문·공백 중복 정책은 [입력 규격](input-format.md)에 있다.

현재 남은 공백 차이61,546문장은 해결 완료로 보지 않는다. 별도 진단기는 **검토 후보**를
유형별로 보여준다. 다음은 단위·접사와 나머지 경계의 실제 예문을 검토하여 한 규칙
계열만 고른다. 순서 표현·합성어 예외를 포함한 독립 계약을 먼저 작성한다.
`십 이/열 두/유 월`, 전화·식별자 자리 간격은 맞춤법 오류와 구분해서 읽기 목적을
검토한다. 발화 간격 프로필이나 전역 공백 교정은 아직 구현하지 않았다.

| 원래 요구사항 | 현재 구현·증거 | 이 증거가 보장하지 않는 것 |
| --- | --- | --- |
| C11, static/shared, CLI, CTest | [CMake](../CMakeLists.txt), [public headers](../include/tts_text_frontend/ttf.h), GCC/Clang/sanitizer [v0.13 실행 기록](research/english-dates-v13-results.md) | 설치 후 재배치, 이전 binary ABI와의 자동 비교, Windows/macOS 실행 |
| tokenizer, source/context/render, Semantic IR | [tokenizer](../src/tokenizer/tokenizer.c), [core](../src/core/normalize.c), [IR](semantic-ir.md), 1st/Apple/GPU/WiFi golden | 전체 Unicode 문자 속성, 모든 문맥의 정확한 언어 판정 |
| 언어 독립적인 pack·발음 dispatch | [pack ABI](../include/tts_text_frontend/ttf_language_pack.h), `languages/*/pack.c` 자동 등록, `xx-Test` 외부 등록 검사 | 제3언어의 실제 TN/phone inventory 완전성 |
| resource·lexicon 우선순위, deterministic rule 선택 | [resource](../src/resource/resource.c), [rule](../src/rule/rule.c), layer/길이/specificity/선언 순서 검사 | resource 의미 오류 전부의 사전 검출, 대형 사전에서의 일정한 성능 |
| 입력 streaming, 문장/phrase 출력 callback | [stream](../src/stream/stream.c), [segmenter](../src/segmenter/segmenter.c), batch도 같은 stream 경로 사용 | 모든 입력의 일정한 TTFB, 모든 경계 조합을 검사했다는 주장 |
| shared immutable engine·ownership | freeze 이후 읽기 경로, [API 계약](api.md), 8 threads × 100 requests 검사 | 모든 pack/plugin의 thread safety, 오류·할당 실패 경로 전체 |
| 웹 테스트 도구 | [server](../tools/web/server.py), [API 검사](../tests/web/test_workbench.py), [browser 검사](../tests/web/browser.cjs) | 일반 공개 서비스의 인증·다중 테넌트 운영. 현재 목적은 private 개발 도구 |
| 필수 외부 조사·importer·evaluator | [연구 기록](research/external-benchmark-results.md), [importers](../benchmark/importers), [evaluator](../benchmark/evaluators), [이용 조건](THIRD_PARTY.md) | 아직 획득하지 않은 한국어 원본의 실측 정확도, 데이터별 이용 권한 |

이번 감사에서 기존 build에 대해 API/tokenizer, 제3 pack 등록, resource/lexicon
우선순위, 임의 두 조각 streaming, 동시 요청, 한국어 importer, CMU importer,
compiled resource의 **8개 CTest를 다시 실행해 모두 통과**했다. 이는 해당 테스트의
현재 범위에 대한 증거이며 아래 미해결 항목을 닫지 않는다.

## P0 — 값과 출처를 잘못 전달하는 경로

- [x] **NUM-01: Unicode grouping으로 금액·수량 값이 조용히 바뀌는 문제.**
  v0.15에서 NBSP/NNBSP/thin space를 resource-driven 동일 3자리 정수 묶음으로
  처리한다. 통화·단위·퍼센트·범위·지수와 조합하며 손상·혼합 그룹 및 지원하지
  않는 날짜/전화/IP 그룹은 전체 보존한다. ASCII 공백은 별개 숫자로 유지한다.
  고정 원문 57건에서 v0.14 text/value exact 0→57, 독립 Decimal 90값 및 전체
  두 조각 byte 분할이 근거다. [숫자 가이드](unicode-numbers.md).
  **범위:** locale 전체, 소수/지수부 grouping, 모든 Unicode digit 지원은 별도
  확장 과제이며 이 체크가 해당 범위까지 완료했다는 뜻은 아니다.

- [x] **KO-01: 한국어 서수·횟수·번호 counter의 기본 의미 분리.**
  v0.15에서 `번째`, 명시적으로 단서가 있는 횟수/번호 `번`, `제N`을 분리했다.
  첫/두/세/스무 계열, 100 이상 Sino 정책, 쉼표/Unicode 묶음, 조사, 범위 및
  malformed 전체 보존을 독립 원문 계약과 기본/짧은 분절에서 확인한다.
  [카운터 정책](korean-counters.md), [전후 측정](research/numeric-counters-v15-results.md)이 근거다.
  **범위:** 문맥 없는 `번`은 기존 Sino 기본값이고 한국어 형태소/어미 전체를
  이해하는 것은 아니다. 접두 서수 범위는 현재 전체 원문 보존으로 처리한다.
  새 동사·조사·업무별 번호 문맥의 확대와 독립 사람 검수는 계속 필요하다.
  **조사표 진행 (2026-09-27):** 의무적인 `(이)랑`, `(이)나`, `(이)나마`, `(이)라도`,
  `(이)든`, `(이)든지`, `(이)야말로`, `(이)라든지` 8쌍을 `particle_form`과 `particle`에
  추가했다. 변경 전 고정한 72건에서 35 → 72 통과, 회귀 0이며 금액 IR/고객 override와
  1,390회 byte 분할도 검사한다. [편집 가이드](particle-resources.md).
  기존 문자 치환 계약을 지키도록 미분류 OTHER identity / unreadable-cluster 뒤에서는
  조사 교정을 생략한다. 문자 이름은 명사 경계의 근거로 보지 않는다.
  전체 조사 span만 교정하므로 `이랑은` 같은 연쇄, 붙여 쓴 서술어, 선택적 서술격 축약과
  호격 모호성은 남긴다. 문맥 신호는 여전히 접두사 heuristic이고 형태소 분석을 보장하지 않는다.

- [x] **RES-01: resource compiler/runtime 행 구분 문법 일치.**
  [compiler](../tools/resource_compiler/compile_resources.py)는 `str.splitlines()`를
  사용하지만 [runtime](../src/resource/resource.c)의 `parse_tsv`는 LF만 행 구분자로
  사용한다. 두 정상 TSV 행을 vertical tab으로 연결하면 compiler는 TTFB를 만들고
  runtime은 `invalid resource`로 거부한다. **닫는 증거:** 공통 LF/CRLF 정책,
  VT/FF/NEL/U+2028/U+2029/control/NUL 검사, compiler가 승인한 artifact의 runtime
  roundtrip, 실패 compile이 기존 artifact를 바꾸지 않는 검사.
  **수정·검증:** 이번 후속 변경에서 compiler를 LF split + 끝의 CR 한 개 제거로
  맞췄다. `tests/integration/test_compiled_resources.py`가 네 가지 LF/CRLF/EOF 조합,
  잘못된 행 구분자 아홉 종류, field/comment 안의 separator 다섯 종류를 실제 CLI와
  대조한다. 직접 실행 및 `compiled_resources`, `external_cmudict_importer` CTest 통과.
  데이터 안의 separator를 임의로 삭제하거나 의미를 바꾸지 않았다. commit ID는
  해당 변경을 포함하는 후속 release에서 기록한다.

- [x] **RES-02: resource 의미 검증을 setup 단계에서 완료.** v0.16에서 공통 및
  KO/EN pack schema를 별도 validator descriptor로 등록한다. scalar는 transactional
  load, effective 날짜 범위·필수 label·streaming cue 의존성은 validate/freeze에서
  검사한다. CLI/compiler/web이 같은 C 검증기를 사용한다. 미등록 namespace는
  허용하면서 수를 보고하고 strict mode에서 거부하므로 제3언어 확장을 막지 않는다.
  **증거:** `resource_semantic_validation`, `resource_validation_cli`,
  `resource_compiler_protocol` 및 웹 진단 테스트. TSV/TTFB 원자성, freeze 실패 후
  복구, 기존 pack ABI, 제3언어 validator, 선언 순서와 layer, compiler 산출물
  보존을 검사한다. [계약·편집 가이드](resource-validation.md).
  **범위:** 알려진 정책의 구조·소비자 제약·의존성 검증이다. 일반 읽기 label의
  언어학적 정답이나 아직 검증기를 등록하지 않은 외부 namespace를 인증하지 않는다.

- [x] **TRACE-01: source와 target pronunciation의 실제 선택 경로 전달.**
  v0.17의 additive trace API가 실제 정규화 중 선택한 lexicon/source G2P/target
  phone·grapheme 행의 key/value/layer/선언 순서를 분리해 복사한다. 생성 결정은
  출처 미상으로 표시하고, 실패한 phone 시도는 rejected로 남긴다. 복합 발음의
  부분 범위와 명시적인 variant ID/선택 이유도 보존한다. 기존 ABI v1은 유지한다.
  **증거:** `detailed_pronunciation_trace`와 `detailed_pronunciation_trace_cli`의
  source/target 계층·미사용 probe 제외·부분 offset·variant·임의 byte 분할·동시 요청·
  callback 수명·누적 제한 검사. 웹은 batch/stream trace까지 비교한다.
  [API 및 편집 가이드](detailed-pronunciation-trace.md).
  **범위:** built-in 발음 경로와 instrumentation을 호출하는 외부 pack에 해당한다.
  외부 pack 내부의 미보고 행, TN 전체의 자동 resource trace, 원본 파일 경로·행 번호,
  음절별 정렬, heteronym 자동 문맥 선택과 언어학적 정답 검수는 포함하지 않는다.

- [ ] **DOC-01: 실제 출력과 문서 예제의 정합성.** 감사 기준 README의 금융 예제에는
  `1.5-2.5%`와 `1만원~2만원`을 아직 `다시`로 읽는 구문이 있으나 실제 출력은
  각각 `일 점 오 퍼센트에서 이 점 오 퍼센트`, `만 원에서 이만 원`이다.
  API 문서는 release `0.4.0`이라고 적혀 있고 pack 문서는 구현이 모두 `pack.c`라고
  적혀 있다. **닫는 증거:** 현재 예제의 수정과 실행 가능한 문서 예제 검사;
  역사적 결과는 버전·리소스를 명시하고 현재 동작으로 소개하지 않는다.

## P1 — 금융·콘텐츠·일반 TN coverage

- [ ] **EN-01: 약어·initialism·acronym을 문맥에 맞게 처리.** v0.13 외부 개발 표본의
  LETTERS exact는 35/712. 현재 진행 중인 영어 약어 작업은 source/context/render,
  일반 단어·이름·문장 끝 점·소유격·단위와의 충돌을 포함해 검증한다.
  **닫는 증거:** 독립 계약 corpus, 같은 외부 표본 전후 class별 비교, regression
  사례 검토, code/protected 경계와 모든 선택 layer의 우선순위 유지.

- [ ] **EN-02: written currency names와 다른 단위 충돌 처리.** v0.13 MONEY exact
  30/33은 33개 표본만의 결과다. 영어 통화명 확장을 진행하되 `pound`의 질량/통화,
  접두 통화와 후행 이름의 일치/불일치, scale, 소수 minor unit, range, 소유격을
  값·통화 ID까지 확인한다. **닫는 증거:** 독립 decimal oracle, 큰 수·부호·정밀도·
  잘못된 혼합 표기의 보존 계약, 같은 표본의 비교 및 streaming metadata 일치.

- [ ] **TN-01: 전화·digit·주소·electronic text의 명시적 읽기 정책.** v0.18에서
  국가 표기·괄호·공백 그룹·내선과 영어 금융 식별자 cue를 보강했다. 본번호와 내선을
  분리한 IR, 선행 0, resource override, 잘못된 전화 표기의 보존 및 작은 segment
  target의 임의 byte 분할을 검사한다. `+1250000` 수량과 `x` 연산자를 전화로
  만들지 않도록 명시적 label/extension hint를 구분했다. [전화 편집 가이드](phone-resources.md).
  **남은 범위:** locale별 추가 전화 표기·shortcode·모호한 문맥, 주소와
  URL/email/IP/version의 명시적인 preserve/spoken 정책 및 class별 독립 reference다.
  전화 문법의 통과가 실제 번호 할당/연결 가능성을 보장하지는 않는다. Sproat 표기를
  맞추기 위해 제품의 원문 보존 계약을 몰래 바꾸지 않는다.

- [ ] **TN-02: 날짜·시간의 남은 문법.** 현재 `09/17/2026`, ISO datetime
  `2026-09-17T14:30:00Z`는 전체 literal로 남는다. 날짜 범위, era/decade, relative date,
  locale 모호성, timezone/DATETIME은 추가 설계가 필요하다. **닫는 증거:** locale별
  명시 정책, 윤년/범위/시간대의 독립 값 검사, 지원하지 않는 형태의 전체 보존,
  기존 full/partial DATE IR과 tiny-target streaming 계약 유지. 상대 날짜는 암묵적
  현재 시각을 쓰지 않고 요청 설정으로 기준을 주어 deterministic 성질을 유지한다.

- [ ] **TN-03: symbol과 수학 표현의 역할 구분.** 현재
  `기호는 ± & # @ 입니다.`는 그대로 남는다. standalone 이름, 수량의 부호/오차,
  hashtag/email/operator, protected literal은 다른 정책이다. **닫는 증거:**
  의미·단위·range가 바뀌지 않는 독립 문장 corpus와 고객별 읽기 override, 단독 `~`
  읽기 및 수량 범위의 `에서` 회귀 유지.

- [ ] **UNICODE-01: 확장 문자·정규화와 원문 offset.** fullwidth
  `１２，５００원`은 현재 정규화되지 않으며 tokenizer는 제한된 범위표와 ASCII
  digit만 사용한다. `résumé를 읽습니다.`는 `르으스우므으를 읽습니다.`가 된다.
  **닫는 증거:** fullwidth/combining marks/NFC-NFD/확장 Latin/Unicode whitespace
  정책과 데이터 revision; 원문 byte offset을 유지하는 변환 mapping; protected text의
  byte identity; 미지원 script가 사라지지 않는 검사. 전역 NFKC로 단위/identifier를
  훼손하지 않는다.

- [ ] **CONTEXT-01: route 정확도와 segmentation 의존성 측정.**
  [streaming 계약](streaming.md)은 target 변경에 따라 context 읽기가 달라질 수 있다고
  명시한다. 짧은 제목, 외국어 인명, predicate 없는 영어 구절, 한국어 조사/인용,
  금융 약어의 문맥은 유한 cue에 의존한다. **닫는 증거:** 같은 surface의 KO/EN
  최소 대조, 독립적으로 정한 source/context/render reference, 잘못된 locality 사례의
  기록, target 변경 영향 보고. batch와 stream이 서로 같다는 사실만으로 route를
  정답으로 보지 않는다.

## P1 — 사전과 발음 품질

- [ ] **LEX-01: 검수 가능한 production 사전 확충.** 실제 `lexicons/**/*.jsonl`
  2026-09-28 집계는 453항목(456키): `source-checked` 95, `pending` 358,
  `human-reviewed` 0이다. 시스템407 / 금융46항목이며 선택형 고객 예제9키는 별도다.
  금융/콘텐츠의 실제 빈도·miss를 기준으로 category와 domain scope를 확충한다.
  **닫는 증거:** 명확한 출처·발음 정책·권리·검수자/날짜가 있는 원장, canonical collision
  검사, 실제 문장 내 도달성, customer/user 우선순위, 독립 holdout coverage.
  source에 단어가 있다는 사실을 한국어 발음 검증으로 표시하지 않는다.
  **진행 (2026-09-28):** 제공 한국어 Training 참조에서 후보4,290키를 모으는
  수집기를 추가하고, 공통23항목/선택형9키를 구분해 반영했다. [보강 가이드](lexicon-coverage-v22.ko.md).
  TV처럼 허용형이 섞인 다수 표기를 기본값으로 자동 채택하지 않는다. KB/MG의 단위·브랜드
  충돌, 모델/사이즈 코드의 읽기 의도, 독립 검수와 held-out은 후속이다.

- [ ] **LEX-02: CMU 복수 발음의 runtime 선택.** v0.17에서 명시적인 수동 선택을
  구현했다. `--variant-policy all`은 base와 numbered variant를 runtime에 보존하고,
  `pronunciation_choice`로 선택한 ID·이유·실제 행을 상세 trace로 전달한다.
  상위 base와 user/customer target lexicon 우선순위, missing variant freeze 실패,
  pinned CMU 전체 export를 검사했다. [편집 가이드](detailed-pronunciation-trace.md).
  **남은 범위:** `read`/`lead` 같은 이형동음·동형이음 단어의 문맥별 선택 정책과
  독립 reference 검수. 현재 ID 순서에서 빈도/POS 의미를 발명하지 않으며, 명시적
  choice가 없으면 기존 base/default fallback을 사용한다. 수동 선택이 작동한다는
  사실을 자동 문맥별 발음 선택의 완료로 간주하지 않는다.

- [ ] **PRON-01: unknown Latin fallback의 텍스트 정확도.** 현재 greedy G2P는 최대
  5글자 resource match이고 KO mapper는 제한된 음절화 및 stress 무시 경로다.
  발화 가능한 한글이 나온다는 보장은 올바른 발음 보장이 아니다.
  **닫는 증거:** 사전에서 의도적으로 제외한 독립 어휘의 한국어 reference, known-hit와
  OOV 성능 분리, NIKL 원칙/관용형의 적용 이유, 다중 음절·자음군·모음·강세 변형과
  장문 단어 검사. 오류를 시스템 사전에 하나씩 추가한 뒤 OOV 개선으로 세지 않는다.
  **진행 (2026-09-25):** 사전에서 제외한 측정 세트가 127단어이고 124건이 맞는다
  (28 + 30 + 18 + 27 + 24, 게이트는 124건). 세트는 모두 규칙 변경 **전에** 작성했고, 3·5차는
  앞 세트가 부른 규칙의 이웃 단어만 모아 과적합을 잡는 용도다
  (`docs/research/oov-guess-v21-results.md`). 남은 실패 3건은 national(강세에 따른 모음 축소),
  michael(고유명사는 사전 지식), visit(기대값 자체가 다툼의 여지). 축약형·하이픈 합성어·
  6음절 이상 파생형은 아직 표본이 적다. 이 항목은 열어 둔다.
  **약어 구분 (2026-09-25):** 한국어가 단어로 읽는 약어(피파·나토·유네스코)는 추측 대상이
  아니라 사전 대상이다. 7건을 `lexicons/ko-KR/acronym-additions.jsonl`에 넣었고, 이 추가를
  추측 발음 개선으로 세지 않는다(`docs/research/ko-acronym-v21-results.md`).

- [x] **NUM-ZERO: 맨 선행 0 숫자의 읽기 정책 (2026-09-28).** 사용자 결정에 따라
  부호·구분자·단위 없는 두 자리 이상 열은 자리별로 읽는다. KO `0046 → 공공사육`,
  EN `0046 → zero zero four six`. 이전의 짧은 열 기수 정책을 대체한다. 수량·시간·날짜,
  명시 식별자와 제어는 별도이며 `01~02만 원`도 수량 계수로 유지한다.
  일반 임계값과 KO `공/영`은 리소스로 재정의할 수 있다. 기존 참조 정책 변경 원장을
  보존했고 새 53계약/byte-stream 검증을 추가했다. [설계·편집 가이드](phone-zero-policy.ko.md).
  **후속:** 개인 번호의 하나/둘/셋 등 화자 선택은 자동 추론과 분리해 명시 제어로 다룬다.
  아래 이전 작업 기록의 NUM-ZERO 미결 문구는 이 사용자 결정으로 대체되었다.

- [x] **NUM-WIDTH / 전각 숫자 (2026-09-28).** U+FF10–FF19를 공유 scanner view에서
  폴딩해 universal/phone/EN date/KO counters가 기존 값 해석을 재사용한다. 원문 byte offset과
  보호 구간은 보존한다. 자체 참조30건, KO/EN 변형·부정·streaming 및 기존 corpus 비교.
  [정책·한계·검증](numeric-width.ko.md). 일반 NFKC나 제어 예약어의 ASCII 입력 확장은 아니다.
- [ ] **NUM-WIDTH / 위·아래 첨자 후속.** `¹²³`·`₁₂₃`은 여전히 보존한다. 각주/지수/수량
  모호성 때문에 전각 숫자와 분리한다. **닫는 증거:** 실제 입력에서 역할별 표본, 처리 의도
  정책, 독립 계약을 먼저 고정한다. 전각 구두점/단위 문자 확장도 새 문법·부정 계약이 필요하다.

- [ ] **PRON-02: 확장 가능한 pronunciation IR 검증.** 현재 public phone buffer는
  1024 bytes, alphabet 24, provenance 64의 고정 구조이며 초기 ARPABET subset이다.
  **닫는 증거:** capacity 초과·unknown alphabet·invalid phone·실패 fallback의 명시적
  동작, 다른 source/target pack의 contract test, 동적 길이나 structured phones가
  필요하면 별도 versioned API. 음소 전용 API는 실제 추가 시의 호환성 과제로 둔다.

## P1 — 정답과 측정의 신뢰성

- [ ] **EVAL-01: 독립 reference 검수.** 감사 실행에서
  `tools/evaluation/audit_references.py --self-test`는 18파일 779건, 구조 결함 0,
  독립 검수 대기 779, human-approved 0을 보고했다. **닫는 증거:** 평가 대상별
  의미·정규화 정책·허용 variant·출처·검수자와 날짜가 있는 reference 변경 이력.
  구조 검사나 후보 출력과의 일치를 승인으로 자동 승격하지 않는다. 현재 15건의
  `needs-review`는 결정 이유와 함께 처리한다.

- [ ] **EVAL-02: 남은 외부 실패와 회귀의 case-level 판정.**
  [실제 v0.13 집계](research/english-dates-v13-summary.json)에는 DATE 319/1463,
  TN-only 982/3293, **157개 개선과 6개 회귀**가 있다. 회귀는 5 CARDINAL + 1 PLAIN,
  총 unaligned는 161이다. **닫는 증거:** 기존 상세 결과의 불변 hash, 각 실패를
  값/읽기 정책/reference/경계 문제로 나눈 adjudication, 수정 전후 같은 표본의 보고,
  새 손실의 원인과 처리. 경계가 합쳐졌다는 설명만으로 실패를 지우지 않는다.
  DATE의 casefold-only 698건도 exact에서는 계속 실패다. 진단용 대소문자 허용 지표를
  추가할 때는 별도 이름·분모로 보고한다.
  **진행 (2026-09-25):** 진단 지표를 넣었다. `mismatch_kinds`(case-only / space-only /
  case-and-space / alignment / no-output / different)와 클래스별
  `diagnostic_case_insensitive`를 별도 이름·분모로 보고하고, 정확도는 그대로 byte 비교다
  (`docs/research/date-diagnostic-v21-results.md`). 이 항목이 요구하는 adjudication 전체와
  불변 hash 보고는 아직 남아 있다.
  **진행 (2026-09-26):** `merged-agreement`를 추가했다. 참조가 `6 - 5`를 세 span으로 나누고
  엔진이 하나의 RANGE로 묶지만 읽기가 같은 경우를 alignment 실패와 구분한다. CARDINAL 79 →
  alignment 35 + merged 44, PLAIN 40 → 14 + 26, MEASURE 3 → 0 + 3. 정확도는 바뀌지 않는다.

- [ ] **EVAL-04: speech 모드의 외부 평가 범위.** 2026-09-25에 `--output-mode speech`를
  넣어 speech baseline(span 35,236)을 만들었고 이니셜 읽기를 그 위에서 측정했다. 다만
  데이터셋의 참조는 자기 표기 관행(월 이름 소문자, `n j` 형태의 글자 나열, 구두점 생략)을
  따르므로 speech 정확도를 그대로 발화 품질로 읽으면 안 된다. **닫는 증거:** 관행별 진단
  분리(현재 `mismatch_kinds`로 일부), 한국어 speech 표본, 합성 결과에 대한 독립 검수.

- [ ] **EVAL-03: 개발 표본과 분리된 held-out 평가.** 현재 외부 3,785문장은
  Sproat training-prefix 개발 표본이며 MONEY 33, TIME 4, FRACTION 2처럼 불균형하다.
  **닫는 증거:** corpus/source/reference hashes, source split과 overlap 검사,
  semiotic class별 충분한 표본, 금융·콘텐츠 구분, 실패/정렬불가를 포함한 분모,
  변경 전에 봉인한 holdout. 현 개발 표본 개선을 production 정확도로 소개하지 않는다.

- [ ] **EVAL-04: 한국어 외부 원본의 실제 검증.**
  **2026-09-28 진행:** 사용자 제공 Training 스크립트21파일·142,255문장의 첫 기준선과
  importer/evaluator/웹 `/ko-quality`를 구축했다. 문장 정확11.88%, 공백 제거56.32%,
  참조 구조 경고1,349건, 미채점7건, 엔진 오류0. [계약·검토 절차](korean-reference-benchmark.ko.md).
  이는 **제공 참조 일치도**이며 독립 언어 검수/이용허락 검증/held-out 확보 완료가 아니다.
  KsponSpeech/CoreaSpeech 원본에 대한 평가도 별개로 남는다. 원본은 ignored 경로에만 보관한다.
  **닫는 증거:** 제공 참조의 독립 검수와 정책 차이 분리, 권리 조건 확인, 개발에 사용한
  문장·템플릿과 분리한 검증자료에서 재측정. NC 자료를 제품 사전에 반입하지 않는다.

### 후속 평가: 명시 제어를 사용한 읽기 (사용자 결정, 2026-09-28)

개인 고유 번호는 문맥뿐 아니라 말하는 사람의 선택에 따라 읽는다. 예를 들어
`2722`를 `둘일곱둘둘`로 전달하는 요구를 자동 TN의 단일 정답으로 강제하지 않는다.
식별번호 계열은 이번 자동 규칙 개선의 우선 대상에서 제외한다.

- 추후 사람이 문장별 의도를 확인한 부분집합에서 **자동 TN / 제어 적용 TN**을 나란히
  측정한다. 양쪽 모두 정확 일치와 공백 제거 일치를 제공하고, 검토한 분모·제어 적용
  범위·남은 미검토 건수를 표시한다. 전체 자동 TN 점수와 섞지 않는다.
- 원문, annotated 입력, 선택한 예약어/리소스, 기대 읽기, 검토자·사유·fingerprint를
  보존한다. 정답을 복사한 literal 별칭과 패턴 제어 적용은 별도 분류해야 한다.
  literal 강제 치환의 일치를 엔진의 일반화 성능으로 소개하지 않는다.
- 기존 `@native`는 정수 전체를 읽는 제어이며 **각 자릿수를 하나·둘로 읽는 모드가 아니다**.
  자릿수별 고유어·공/영·묶음 정책의 입력 규격과 평가는 별도 작업이다.
- 이번에는 요구를 기록만 했다. annotation 자동 생성·보조 정확도 수치·새 제어 구현 없음.

## P2 — runtime·배포·운영 검증

- [ ] **PERF-01: 대형 사전과 규칙 인덱스.** 현재 resource hash는 1,021 fixed buckets,
  phrase key는 8 words/255 bytes로 제한되고 rule matcher는 rule 목록을 순회한다.
  TTFB는 checksum이 있는 TSV container이며 runtime에서 다시 parse/allocate한다.
  **닫는 증거:** storage abstraction을 유지한 확장 인덱스, 동일 우선순위 결과,
  큰 licensed 또는 합성 사전/충돌 집중 key/multiword prefix의 p50·p95·RSS·load/freeze
  비교; compiler/runtime format version과 corruption 검사. 사전 수만 늘리지 않는다.

- [ ] **STREAM-01: 최악 조건의 pending·CPU 비용.** 현재 열린 인용/코드/opaque literal은
  closer 또는 flush까지 보류하며 일부 delimiter가 많은 입력은 반복 scan한다.
  v0.18 검토에서 30,000개의 한 자리 수를 공백으로 나열하고 분절 target을 입력
  전체로 크게 설정하면 v0.17 Release도 30초 안에 끝나지 않음을 재현했다.
  v0.19는 동일 단위/인용 scope의 run 경계와 KO/EN 점수를 요청 내부에서 재사용한다.
  30,000개 수의 전체 단위가 완료되고 기존 텍스트·metadata·context trace와 같음을
  검사한다. 부분 semantic 소비로 token이 변경되면 캐시를 무효화하며, 두 번
  무효화된 단위는 기존 비캐시 경로로 전환해 반복 재구축 비용을 막는다.
  이 경로에서는 여전히 문맥을 반복 순회하므로 최악 비용은 남아 있다.
  [검증 보고서](research/context-cache-v19-results.md)의 전체 입력 처리 시간은
  기본 target의 TTFB로 일반화하지 않는다. 열린/중첩 scope와 delimiter 입력까지
  해결했다는 의미도 아니다.
  **닫는 증거:** 1-byte 입력, 긴 무구분 문자열, 열린/중첩 인용, delimiter 집중 입력,
  다수 rule/resource에 대한 시간·memory 증가율과 실제 callback 시점; 초과 시 일관된
  오류; metadata까지 동일한 batch/stream 결과. 첫 callback 전 전체 TN을 하는
  우회로를 도입하지 않는다.

- [ ] **ABI-01: 이전 배포 ABI와의 자동 비교.** 현재 struct-size/version 검사는
  기존 binary consumer가 새 library를 사용할 수 있음을 직접 검사하지 않는다.
  **닫는 증거:** 공개 symbol/struct layout의 버전별 snapshot, 이전 header로 빌드한
  consumer와 새 shared library의 호출 검사, SOVERSION·Windows export·C++ ABI smoke.
  `tests/web/abi_layout.c`의 현재 ctypes 일치 검사는 유지한다.

- [ ] **DEPLOY-01: 설치·재배치·downstream linking.** 현재 build 정의는
  `TTF_DEFAULT_RESOURCE_ROOT="/home/exedev/tts-text-frontend/resources"`이고 install은
  `share/tts-text-frontend/resources`에 복사한다. API의 명시적 resource root는 작동하지만
  기본 경로가 installed path로 바뀌지는 않는다. **닫는 증거:** source/build directory
  없이 설치 prefix에서 CLI/shared/static consumer를 실행; 명시적/기본 resource 경로
  계약; CMake package/export 또는 명확한 downstream link metadata; staged install 검사.

- [ ] **PORT-01: Windows/macOS 실제 CI.** 현재 workflow는 모든 job이 Ubuntu이고
  MSVC 옵션·Windows export annotation만으로 다른 OS의 성공을 증명하지 못한다.
  **닫는 증거:** macOS Clang, Windows MSVC configure/build/CTest/install 실행,
  UTF-8 argv/path·binary stdin/out·shared/static library 이름·callback 검사.

- [ ] **ROBUST-01: 오류·동시성·parser 범위 확장.** 현재 concurrent test는 같은
  문장 800회이고 random streaming은 제한된 atom 조합이다. **닫는 증거:** 독립 stream
  동시성, 다른 corpus/resource 읽기, callback 취소/재진입 계약, malformed UTF-8/resource,
  OOM fault injection, tokenizer/classifier/stream fuzz seed와 재현 corpus,
  지원 환경에서 race 검사. 기존 ASan/UBSan을 통과해야 하며 별도 stress 결과도 기록한다.

- [ ] **WEB-01: 새 기능의 재현·편집·결과 export 연계.** 이미 native worker와
  callback UI가 있으므로 새 class의 input/output만 추가하지 않는다. **닫는 증거:**
  class·IR·source/context/render·선택 layer·reference 상태·batch/stream 차이를 확인하고
  실패를 local regression 후보로 export하는 browser 검사; 사용자 lexicon/resource
  preview가 global engine이나 committed system resource를 덮어쓰지 않는 검사.

## 지속적으로 지킬 권리·scope 조건

- [ ] **RIGHTS-01: 새 resource의 권리 추적과 배포 inventory.**
  [THIRD_PARTY](THIRD_PARTY.md)와 [source catalog](../benchmark/manifests/sources.json)는
  NeMo, Sproat, Kspon, CoreaSpeech/N2gk+, CMUdict, NIKL의 코드/데이터/문헌 조건을
  구분한다. 이번 감사는 새 법률 판단을 내리지 않았다. **닫는 증거:** 실제 shipped
  resource와 provenance/license/attribution의 대응 검사; 출처 revision 변경 시 재조사;
  NC/미확인 자료 및 외부 대량 corpus가 product manifest와 Git에 들어가지 않는 gate.
  importer의 opt-in flag를 이용 허가로 해석하지 않는다.

- [ ] **SCOPE-01: 다음 변경마다 전체 요구사항 재검사.** tokenizer부터 callback까지의
  통합, ko/en 기본 TN, code mixed 1st/Apple/acronym/OOV, user/customer/domain override,
  inspect, compiler, static/shared, tests/docs/web, commit/main push의 증거를 유지한다.
  **닫는 증거:** 해당 release의 clean worktree, main commit, build/test/CI URL, 배포
  health version, 실제 대표 CLI 출력. 처음 요청의 24개 acceptance 조건을 충족해도
  위 production 품질 미해결 항목이 자동 완료되는 것은 아니다.

## 작업 순서와 체크박스 갱신

현재 진행 중인 EN-01/EN-02의 계약·외부 비교를 완료하면서 P0의 재현 사례를 우선
수정한다. 이후 전화/digit·Unicode·한국어 counter, 검수 가능한 사전/발음, held-out
평가, 배포·ABI·성능 검증을 병행한다. 특정 데이터의 사용 권리나 독립 검수가
준비되지 않았다고 전체 개발을 중단하지 않는다.

항목을 닫을 때는 이 문서에 **수정 commit, 실행한 검사/결과, 남은 범위**를 덧붙인다.
독립 reference의 의미를 바꾸어 통과시키거나, 문서의 한계 문장을 지운 사실만으로
완료 처리하지 않는다. 이 목록은 감사 시점의 증거 목록이며 새 실패가 발견되면
동일한 방식으로 추가한다.

## v0.14 진행 기록

위 감사의 재현 기준은 v0.13이다. 다음 변경은 일부 항목을 진전시켰으며,
상위 항목 전체의 완료를 주장하지 않는다.

- **RES-01 완료:** LF/CRLF compiler/runtime 동등성 검사를 통과했다.
- **EN-01/EN-02 진행:** 영어 dotted initials/title 및 written currency 인식,
  명시적 통화·무게 모호성, 범위·오류 보존 검사와 24건 웹 corpus를 추가했다.
  일반 단어 발음과 이니셜 발음 resource를 분리하여 `us`/`U.S.` 충돌을 수정했다.
- **LEX/CONTEXT 진행:** 원문/정규 표기 alias의 layer 우선순위를 고쳤고,
  dotted initials 단독 표기의 언어 투표를 중립화했다. 한국어 title 4건은
  managed lexicon에 추가했으나 독립 검수는 여전히 대기다.
- **EVAL-02 진행:** comparator가 양쪽 결과 파일의 건수, 전체 지표 재계산,
  reference 필드·순서 digest, text/exact flag, 실행 전후 hash를 검사한다.
  잘린 결과 두 개를 정상 비교로 수용하던 경로를 차단했다.
- **DOC-01 진행:** 오래된 README 범위 읽기를 현재 `에서` 결과로 수정하고,
  API release/version과 language-pack 파일 구성 설명을 갱신했다.

측정 수치와 regression 원인은 [v0.14 결과](research/english-forms-v14-results.md)에
기록한다. 다음 작업은 NUM-01/KO-01의 실제 값·counter 오류와 TRACE-01의 출처
계약을 우선하며, 독립 검수·holdout·권리 확보 조건을 임의로 완료 처리하지 않는다.

## v0.15 진행 기록

Unicode grouping과 KO 카운터의 값 보존, source pronunciation layer를 보강했다.
`secondary`가 평가기 metadata 계약에서 빠져 있던 부분도 수정하여 범위의 오른쪽
값과 count/identifier 역할을 검사한다. 웹에 한국어 카운터 28건과 Unicode 예제를
추가했다. 지표·compiler/sanitizer/byte-split 검증은
[v0.15 결과](research/numeric-counters-v15-results.md)에 기록한다.

다음 우선순위는 RES-02의 setup 단계 정책 검증, TRACE-01의 target mapper 출처,
실제 금융/콘텐츠 입력의 독립 검수와 corpus 확대다. 기존 사전의 pending review를
통과 처리하거나 합성 음성의 품질을 검증했다고 주장하지 않는다.

## v0.16 진행 기록

RES-02 setup-time 정책 검증과 상세 진단을 구현했다. 기존 공개 ABI v1 구조체를
변경하지 않고 validator registry를 추가했다. 웹 port 8000에서도 고객 TSV 오류의
파일·행·키 및 미검증 행 수를 확인한다. Compiler는 실제 배포 root/overlay/language
문맥으로 검사하고 성공한 artifact만 교체한다.

측정과 회귀 결과는 [v0.16 결과](research/resource-validation-v16-results.md)에
기록한다. 다음 우선순위는 TRACE-01 target mapper/variant별 상세 출처, 외부
benchmark의 남은 semiotic class 오류, 한국어 금융·콘텐츠 reference의 독립 검수다.
검사 대상 schema가 완비됐다는 사실로 전체 언어 품질 완료를 선언하지 않는다.

## v0.20 후속 적용 기록

사용자 사례를 계기로 코드믹스 ALNUM, 공유 금액 범위의 가격대 조사,
층수 단서, 이항 더하기, 약어 www., 이메일 철자 읽기를 보강했다.
옵션형 annotated 입력, 최종 문자 처리, 기본 미지원 문자 보존 및 명시적
제거, trace와 웹 검증 도구를 추가했다. 새 51건 참조와 임의 바이트
검증은 [실행 기록](research/speech-controls-v20-results.md)에 남긴다.
이는 사전 coverage, 사람 검수, Unicode 전체 지원이나 운영 성능 과제를
완료한 것으로 처리하지 않는다.

## v0.21 후속(미릴리스) 적용 기록 — speech 최종 관문

TN-03/UNICODE-01의 일부를 진전시켰다. 원형 숫자·로마 숫자·분수·`± × ÷ ㈜`의
기호 이름, 단어 뒤 늘임표 `~`, 호환 단위 문자, drop 뒤 공백 정리를 추가했다.
독립 작성 40건 계약(`tests/evaluation/speech_final_gate_v21.jsonl`)이 근거다. 외부 영어
개발 표본은 v0.20과 결과 파일이 동일하다. 연산자 IR(`3-1=2`, `a+b`), hashtag
역할, fullwidth 숫자 정규화는 남아 있다.

## v0.21 후속(미릴리스) — `&` 이니셜과 대문자 한 글자 (EN-01 일부)

`R&D`/`AT&T` 같은 `&` 결합 이니셜과 한국어 문맥의 대문자 한 글자를 글자 이름으로
읽는다. 독립 작성 26건 계약(`tests/evaluation/letters_v21.jsonl`)이 근거다. 외부
영어 표본은 출력 텍스트·exact 수가 동일하고 3개 span의 IR만 INITIALISM이 됐다.
`Johnson&Johnson`처럼 영어 단어를 잇는 `&`는 한국어 렌더러로 `앤`이 되는 혼합
출력이 남아 있으며, 두 글자 대문자가 일반 단어(`AT`, `US`)와 겹치는 단독 표기는
기존 lexical 정책을 따른다.

## v0.21 후속(미릴리스) — 이메일·URL 단어, 버전, IP (TN-01 일부)

주소의 영문 묶음은 리소스 근거(`electronic_word`, 발음 사전)가 있으면 단어로, 없으면
철자로 읽는다. `url_tld`로 bare 도메인을 인식하고, speech 모드에서 버전·IP를 읽는다.
독립 작성 34건(`tests/evaluation/electronic_v21.jsonl`)이 근거다. 외부 영어 표본의
출력은 동일하고 bare 도메인 9건의 IR만 URL이 됐다. 남은 범위: 경로·쿼리의 의미 단위
읽기, 대문자 TLD, IDNA, `iOS 17.2` 같은 두 부분 버전(현재 소수), `파일명 report.txt`의
확장자 읽기.

## v0.21 후속(미릴리스) — 수식·비교·해시태그 (TN-03 일부)

관계/명확한 연산자가 있는 식을 `@expression` IR로 읽고, 한국어 `#`를 `샵`(사용자 결정)으로,
영어 해시태그를 `hashtag`로 읽는다. 독립 작성 37건(`tests/evaluation/math_hashtag_v21.jsonl`).
외부 영어 결과 파일은 동일하다. 남은 범위: `2x`·`x²` 같은 계수/지수, 괄호 식,
`x-y 좌표`의 소문자 한 글자(`크스`), 메뉴 경로 `설정>일반`의 `>`(현재 `크다`), `#1`의
번호 읽기, 연쇄 부등식.

## v0.21 후속(미릴리스) — `대` 구간·대수, 양끝 배수 범위 (KO-01 일부)

`20대`·`1~2억대`를 구간으로, cue가 있는 `차량 20대`를 대수로 읽는다. `1억~2억 원`을 오름차순
범위로 읽는다. 독립 작성 25건(`tests/evaluation/counter_band_v21.jsonl`). 남은 범위: 조사 너머 cue
(`20대의 차량`), `100대 기업` 같은 순위 표현, 내림차순·혼합 배수 범위.

## v0.21 후속(미릴리스) — 층·지하층 (TN-01 일부)

`B1층`/`B1F`와 범위, `지하`·`주차장` cue를 추가했다. 독립 작성 19건(`tests/evaluation/floor_v21.jsonl`).
남은 범위: 붙여 쓴 `지하2F`의 띄어쓰기(`지하이 층`), cue 없는 `12F~15F`, `RF`(옥상),
그리고 발견된 별도 규칙군인 **조사 교정**(`3F로` → `삼 층로`, `5kg로` → `오 킬로그램로`,
`1+1는` → `일 더하기 일는`): 읽은 결과의 받침에 맞춰 로/으로·는/은·를/을·가/이를 고쳐야 한다.

## v0.21 후속(미릴리스) — 소문자 한 글자·메뉴 경로 (D 잔여)

소문자 한 글자 이름, `x-y`, 수식 밖 `<`·`>` 무음 처리를 추가했다. 독립 작성 20건
(`tests/evaluation/letters_paths_v21.jsonl`). 남은 범위: `Z`의 한국어 이름(현재 `지`, `Z세대`는
보통 `제트세대`; 정책 결정 필요), `->` 화살표(`하이픈`), `2x`·`x²`·괄호 식, `#1`의 번호 읽기.

## v0.21 후속(미릴리스) — Z 읽기와 `#`숫자 (사용자 결정)

`Z` 한국어 이름을 `제트`로 바꾸고(`G`=`지`와 구분), `#1`을 `일 번`/`number one`으로 읽는다.
독립 작성 17건(`tests/evaluation/letter_number_sign_v21.jsonl`). `#1위`처럼 숫자 뒤에 한글이
붙는 형태와 `#0`, 선행 0은 기존 기호 읽기를 유지한다.

## v0.21 후속(미릴리스) — 조사 교정 (KO-01 일부)

숫자·기호·영문 읽기 뒤에 붙은 조사를 실제 발음의 받침에 맞춰 고른다. 독립 작성 20건
(`tests/evaluation/particle_v21.jsonl`). 기존 fixture 4곳의 비문 기대값(`고객는`,
`유에스커스텀를`)을 교정형으로 갱신하고 사유를 기록했다. 남은 범위: 한 낱말 안에서
읽기가 바뀌는 경우(`네이버로`처럼 토큰이 하나인 형태), 서술격 조사 축약 정책,
`에/에서` 같은 음운 무관 조사의 문맥 오류.

## v0.21 후속(미릴리스) — 혼합 대소문자 이름과 숫자+문자 표기 (LEX-01 일부)

`iOS`·`macOS`·`TikTok` 등은 사전 항목으로, `5G`·`2D`는 `semantic_term`과 사전으로 읽는다.
`3D`와 같은 방식이며 코드 변경은 없다. 독립 작성 20건(`tests/evaluation/mixed_case_v21.jsonl`).
남은 범위: `4K`/`8K`처럼 읽기가 갈리는 표기(미등록 보존), 선언하지 않은 혼합 표기의 일반 규칙,
모든 항목의 사람 검수.

## v0.21 후속(미릴리스) — 대표번호·괄호 전화번호 (TN-01 일부)

선언된 접두사의 대표번호와 괄호 안 전화번호를 읽는다. v0.18의 `1588-0000` 범위 계약은
명시적 단서가 생겨 전화로 갱신했고 미등록 접두사 negative를 새 corpus에 남겼다.
독립 작성 17건(`tests/evaluation/phone_service_v21.jsonl`). 남은 범위: 접두사 목록의 실제 검수,
`1588-0000~1588-9999` 같은 번호 범위, 해외 표기, `A/S` 같은 슬래시 약어(현재 `A slash S`).

## v0.21 후속(미릴리스) — 슬래시 약어 (EN-01 일부)

`A/S`, `I/O`처럼 한 글자끼리 슬래시로 이은 표기를 한 철자로 읽는다. 독립 작성 13건
(`tests/evaluation/slash_letters_v21.jsonl`). 남은 범위: `AS/400`처럼 두 글자 이상 약어의 슬래시,
`찬성/반대`의 슬래시 휴지, `km/h` 단위, `SAVE20`처럼 대문자 약어 뒤 여러 자리 숫자의 읽기
(현재 영어 자릿수 이름: `투지로`).

## v0.21 후속(미릴리스) — 정시 읽기 (TN-02 일부)

정시의 `영 분`·`영 초`를 생략한다. 독립 작성 13건(`tests/evaluation/clock_v21.jsonl`).
기존 fixture 3곳의 `영 분`/`영 초` 기대값을 갱신했다. 남은 범위: 24시간 표기의 오전·오후
변환, `00:00`의 `자정` 표현, 영어 `1:00 PM`(현재 `one o'clock p m`).

## v0.21 후속(미릴리스) — 슬래시 날짜 (TN-02 일부)

`9/25`, `2026/9/25`, `5/8(수)`처럼 모호하지 않은 슬래시 날짜만 날짜로 읽고 나머지는 분수로
둔다. 독립 작성 16건(`tests/evaluation/slash_date_v21.jsonl`). 슬래시 약어 corpus의 부정 예
`2026/09/23`은 이 변경으로 갱신했다. 남은 범위: locale이 갈리는 `9/25/2026`(현재 원문 보존),
`9/25~9/30` 날짜 범위(현재 `물결`), 요일 단서 없는 `5/8` 같은 모호한 표기.

## v0.21 후속(미릴리스) — 붙여 쓴 범위 표시 (TN-03 일부)

두 읽기 사이에 붙은 `~`/`～`를 범위 말로 읽는다. 독립 작성 15건
(`tests/evaluation/range_mark_v21.jsonl`). 슬래시 날짜 corpus의 `9/25~9/30` 기대값을 갱신했다.
남은 범위: `3월 1일~3월 5일`, `10시 30분~11시`처럼 여러 토큰 구간이 `invalid-range`로 보존되는
문제, 범위의 Semantic IR(현재 텍스트만 범위로 읽고 RANGE IR은 만들지 않음), `부터/까지` 조사.

## v0.21 후속(미릴리스) — 여러 토큰 구간 (TN-02 일부)

`3월 1일~3월 5일`, `10시 30분~11시`를 각 끝을 읽는 방식으로 처리한다. 독립 작성 11건
(`tests/evaluation/multi_range_v21.jsonl`). 남은 범위: 이런 구간의 RANGE IR(현재는 두 객체와
범위 표시 텍스트), 공백을 둔 `3월 1일 ~ 3월 5일`(가운데가 `물결`), `부터/까지` 표현.

## v0.21 후속(미릴리스) — 아이디·핸들 (TN-01 일부)

밑줄 식별자와 `@` 핸들을 주소와 같은 정책으로 읽는다. 독립 작성 11건
(`tests/evaluation/handle_v21.jsonl`). 남은 범위: `my_file.txt`처럼 점이 붙은 파일명,
밑줄 없는 camelCase 식별자, 그리고 근본 원인인 미등록 영어 단어의 G2P 품질(PRON-01).

## v0.21 후속(미릴리스) — 영어 연도 문맥 (EVAL-02/TN-02 일부)

`date_year_cue`에 전치사·구간어를 추가했다. 외부 영어 표본에서 DATE exact 319 → 343,
회귀 0, CARDINAL 변화 없음(`benchmark/generated/sproat-v21-yearcue-final-comparison.json`).
독립 작성 13건(`tests/evaluation/year_cue_v21.jsonl`).

**후속 해결(2026-09-23):** planner가 `date_year_cue`를 층 단서처럼 직접 읽게 해서 단서 단어를
일반 `boundary_word`로 만들지 않아도 되게 했다. 관사류를 단서로 되돌리고, `from X to Y` /
`between X and Y`는 `date_year_interval`로 처리하되 **앞 끝이 단서로 확인된 연도일 때만** 적용한다.
그래서 `rose to 1965`, `( 2003 and 2005 )`는 수량으로 남는다. DATE exact 343 → 394, 회귀 0,
CARDINAL 변화 없음(`benchmark/generated/sproat-v21-planner-comparison.json`). v0.18 호환 참조 4건의
`context_trace` 점수는 분절이 넓어져 갱신했고 텍스트·offset·의미·언어는 그대로다.
**후속 해결(2026-09-25):** 진단 지표로 맨 연도 실패 258건의 앞 문맥을 세었더니 108건이
`( 1965 )` 인용 형태였다. `date_year_cue`가 여는 괄호도 key로 받게 하되, 괄호가 숫자만 감싸고
바로 닫힐 때만 단서로 본다. `( 2003 and 2005 )`는 위에 적은 대로 계속 수량이다.
DATE exact 394 → 504, 다른 클래스 동일(`benchmark/generated/sproat-v21-yearbracket`).
phone-negative 계약 2건의 기대를 CARDINAL → DATE로 바꾼 정책 변경과 되돌릴 조건은
`docs/research/date-diagnostic-v21-results.md`에 있다.

**연대 표기 해결(2026-09-25):** `1960s`·`80s`·`'90s`를 `en.date.decade`로 분류하고
`decade_plural` 자원으로 읽는다. DATE exact 504 → 532.

남은 범위: `to`/`and`/`by` 자체를 단서로 쓰는 문맥(현재는 앞 끝 연도 근거 필요), 괄호 밖의
맨 연도 150건, ISO 날짜 어순 46건(관행 차이), `9/25/2026` 같은 locale 모호 표기.
대소문자만 다른 698건은 관행이며 진단 지표로 따로 센다.

## v0.21 후속(미릴리스) — 괄호 처리 옵션 (WEB-01/사용자 요청)

괄호 안을 읽기/건너뛰기/원문 유지로 고르는 옵션을 추가했다. 공개 구조체를 늘리지 않고
`ttf_engine_set_option` 함수로 전달한다(예전 헤더로 빌드한 소비자의 배치를 지키기 위함).
독립 작성 11건(`tests/evaluation/bracket_v21.jsonl`). 사용자가 요청한 남은 옵션:
미등록 외국어 단어 정책(철자/추측/원문), 장식 문자 이름 읽기, 대문자 약어 읽기 방식.

## v0.21 후속(미릴리스) — 미등록 외국어 단어 정책 (PRON-01/사용자 요청)

추측 발음 대신 철자 읽기나 원문 유지를 고를 수 있다(`unknown_foreign`). 근본적인 G2P 품질
문제(PRON-01)를 해결한 것이 아니라, 고객이 추측을 끌 수 있게 한 것이다. 독립 작성 15건
(`tests/evaluation/unknown_foreign_v21.jsonl`). 남은 사용자 옵션: 장식 문자 이름 읽기,
대문자 약어 읽기 방식.

## v0.21 후속(미릴리스) — 장식 문자 이름 읽기 (사용자 요청)

`decorative` 옵션으로 선언된 장식 문자를 이름으로 읽는다. 기본값은 기존 동작이다.
독립 작성 13건(`tests/evaluation/decorative_v21.jsonl`). 이름 목록은 검수 대기이며 이모지에는
이름을 주지 않았다. 남은 사용자 옵션: 대문자 약어 읽기 방식.

## v0.21 후속(미릴리스) — 대문자 약어 읽기 방식 (사용자 요청)

`initialism` 옵션(기본 `spell`)으로 `IT`/`AT`/`BE`처럼 영어 단어와 겹치는 대문자 약어도 철자로
읽는다. 선언된 acronym·고유명사·사전 항목은 그대로다. 독립 작성 15건
(`tests/evaluation/initialism_v21.jsonl`). 외부 영어 출력은 동일하고 span 10건의 IR만 바뀌었다.
사용자가 요청한 옵션 네 가지(괄호, 미등록 외국어, 장식 문자, 대문자 약어)를 모두 구현했다.

## v0.21 후속(미릴리스) — 대문자 코드 단어 (사용자 결정)

`SAVE20` → `세이브 이십`. 대문자 묶음이 발음 사전에 단어로 있으면 코드 단어로 보고 대상 언어
수사를 쓴다. 사전 근거가 없으면 기존 initialism 정책(`GPT-4` → `지피티포`)이 그대로다.
독립 작성 10건(`tests/evaluation/code_word_v21.jsonl`), 사전 7항목 추가(검수 대기).
남은 범위: 코드 단어 사전의 실제 검수. 2026-09-24 결정으로 사전 단어 라벨은 원어 수사를 띄어 읽고(`top10` → `탑 텐`, `SAVE20` → `세이브 트웬티`), `top100`처럼 관용 표기는 전체를 사전에 등록해 예외로 둔다(`탑백`).

## v0.21 후속(미릴리스) — 글자+숫자 라벨의 수사 (사용자 결정)

`k7` → `케이세븐`처럼 글자 라벨 뒤 한 자리 숫자는 원어 수사로 읽는다. 두 자리 이상은
대상 언어 수사(`G80` → `지팔십`), 선행 0은 자릿수 유지(`k007` → `케이영영칠`)다.
독립 작성 15건(`tests/evaluation/letter_label_v21.jsonl`). v0.20 포함 기존 계약 10건을 같은
사유로 갱신했다. 남은 범위: `S23`·`G80`처럼 두 자리인데 원어 수사로 읽는 관용 표기는 사전 등록 필요.

## v0.21 후속(미릴리스) — 미등록 단어 추측 발음 (PRON-01 일부)

영어 G2P에 단어 경계 문맥과 묵음 표시를 넣고, 한국어 매퍼가 모음 뒤 `r`를 지운다. 독립 작성
28단어에서 정확 일치 0 → 14이며 남은 14건은 `docs/research/oov-guess-v21-results.md`에 남겼다.
사전 추가로 통과시키지 않았다. 남은 범위: 모음 질(`o` → AA 고정), 어말 파열음 받침(`마켓`),
`-tion`/`-sion` 밖의 접미사, 그리고 known-hit와 OOV를 분리한 더 큰 표본.


## v0.21 후속(미릴리스) — 날짜 뒤 영문 요일 / 자원표 점검 (TN-02 일부)

월 alias 24개 및 기존 한글 weekday cue 14개를 점검했다. 영문 weekday alias 14개와
KO/EN 출력 label 14개를 추가하고 `date(Fri.)`를 DATE/weekday IR로 읽는다.
`마감은 5/8(Fri)입니다.` → `마감은 오 월 팔 일(금요일)입니다.`.
독립 작성 72건: 같은 고정 corpus에서 33/72 → 72/72, fixed 39 / regressed 0.
최초 기준문 분수 표현 3건의 구현 전 정책 정정 이력을 [편집 가이드](calendar-alias-resources.md)에
남겼으며 개선에 포함하지 않았다. 외부 영어 3,785문장 결과 파일은 이전과 동일하다.

남은 범위는 `Mon., May 8` prefix 요일, 날짜와 괄호 사이 공백, 한글 약자 확장 및 한자
요일, 달력 불일치 진단이다. `sun`/`sat`/`wed` 단어 전체를 요일로 바꾸지 않는다.
다음 자원표 sweep은 **abbreviation** 또는 **speech_character** 중 하나를 선택한다.
EVAL-03 held-out 분리, 맨 연도, NUM-ZERO, NUM-WIDTH, 로마 숫자는 여전히 별도 범위다.

## v0.21 후속(미릴리스) — 점을 붙인 영문 약어 (TN-02 일부)

자원표 sweep의 abbreviation 한 규칙군을 진행했다. 기존 bare 읽기가 확정된
`vol/ltd/vs/etc/jr`의 점 표기만 별도 `abbreviation_dotted` 표로 연결했다.
문장 끝·개행·다음 대문자 문장 앞 점 보존, vs의 명시적 continuation 정책,
식별자·확장 라틴 경계, 보호 문구, KO User/Customer 발음 override를 검사했다.
독립 작성 고정30건 **9/30→30/30**, fixed21/regressed0이며 기존 기준문은 수정하지 않았다.
고객 편집법/문장 경계 한계는 [영어 약어 가이드](english-forms.md#dotted-word-abbreviations-v021-continuation)에 있다.

다음 자원표는 **speech_character** 한 규칙군이다. 다의적인 `Co./Inc./No./St.`의
전역 확장, `e.g./i.e.` 복합 점 표기, 소유격·일반 문장 경계 추론은 이번 범위 밖이다.
NUM-ZERO/NUM-WIDTH/맨 연도/로마 숫자/EVAL-03은 기존 판단·우선순위를 유지한다.


## 2026-09-28 continuation — speech_character 전각 영문자

- 완료(working tree): KO/EN `speech-characters.tsv` 각52행. Speech에서 전각 영문자를
  KO 기존 글자 이름 / EN 대소문자 유지 ASCII로 읽는다. Core·ABI·숫자 해석 변경 없음.
- 고정30건: 8→30 통과, 개선22/회귀0. 기존2,347건(앞선 점 약어30건 포함) 출력·span·
  선택된 trace 행 회귀0. 단어 발음 복원/입력 folding으로 점수를 해석하지 않는다.
- 편집·보존·단어 override 한계: [speech-resources.md](speech-resources.md#전각-영문자-최종-읽기-v021-continuation).
- 다음 한 규칙군 후보는 **NUM-WIDTH**: 전각 숫자를 네 스캐너에서 값·단위·원문 offset과
  함께 처리하기. 최종 speech_character 숫자 치환으로 대체하지 않는다. NUM-ZERO,
  문맥 없는 맨 연도, 로마 숫자, EVAL-03은 기존 정책/범위를 유지한다.
- 한국어 외부 TN gold 미확보와 independent-review 대기는 그대로다. 이번 자료는
  자체 회귀 계약이며 외부 개발 표본/held-out 정확도 향상을 주장하지 않는다.


## 2026-09-28 — 명시적 구간 패턴 제어

사용자 요청으로 NUM-WIDTH에 앞서 `[[...]]{@pattern}`을 확장했다.
KO `@address`, KO/EN `@email`·`@url`, 긴 그룹의 borrowed surface 전달을 구현했다.
이는 자동 주소/URL 인식 coverage 증가가 아니라 **사용자가 지정한 구간의 읽기**다.

- 한국어 주소: 도로명·지번 숫자, 동/호, 선행0, 숫자 하이픈 `다시`; 고객 `address_policy`.
- 영어 주소, 국제화 이메일/URL, 모든 지명 관용 독음, 중첩 패턴·패턴 인자 DSL은 미지원.
  새 그룹에 `@native` 등을 써도 해당 패턴의 값 제약은 유지된다.
- 원본·길이·오류·스트리밍 계약과 확장 방법: [제어 가이드](explicit-pattern-controls.ko.md).
- 다음 TN 후보 NUM-WIDTH 및 기존 데이터/held-out 작업 순서는 유지한다. 커밋·배포는 별도다.

## 2026-09-28 — 의미 지정 예약어 확장

요청한 주소/이메일/URL 외에도 숫자·날짜·수량·기술 표기·출력 의도를 지정하는 **20개**를
추가했다(총26개). 기존 `[[...]]{@name}`와 CUSTOM/언어팩 callback을 사용하며 새 ABI는 없다.
명시적 연도·분수·날짜 순서·전화/버전/IP 해석을 지원하고, 기존 리소스가 출력 정책을 정한다.
다른 타입/부분 매칭/형식·크기 오류는 명시적으로 거부한다.

- 계약·제한·확장 순서: [제어 가이드](explicit-pattern-controls.ko.md#의미를-지정하는-예약어-20개-2026-09-28-추가).
- `typed_controls_v22.jsonl` 67건, 오류200조합과 바이트 분할2858회, 고객 정책/trace 검증.
- 기존 자동 인식 coverage 개선이나 외부 benchmark 개선으로 집계하지 않는다.
  맨 연도/NUM-ZERO의 **명시적 선택 수단**을 제공했지만 자동 탐지 정책은 여전히 별도 과제다.
- 남은 범위: 국제화 전자 주소/영문 우편 주소/IPv6/SemVer 전체/명시 phone 내선,
  임의 사용자 정의 매개변수·패턴 DSL, 그룹 중첩. 필요한 실사용 사례를 별도 계약으로 받은 뒤 확장한다.
- 이번 요청 범위는 여기까지다. NUM-WIDTH, held-out, 외부 실데이터 순서를 자동으로 재개하지 않는다.

검증 확정: GCC 전체174/174, 마지막 IR 초기화 보완 후 제어/웹7/7, ASan+UBSan 관련11/11,
최종 새67/67 및 기존2407건 회귀0. 상세 명령·근거는 HANDOFF와 artifacts/tn/typed-controls/.


## 2026-09-28 continuation — NUM-WIDTH 전각 숫자

기존 후보 중 전각 숫자만 구현했다. [상세 결과](numeric-width.ko.md), [인계](HANDOFF.md).
두 과거 미지원 참조를 사전에 승격했고 다른 기존 출력·메타데이터 변화는 허용하지 않았다.
다음 후보는 EVAL-03 held-out 분리 또는 사용자가 제공하는 실입력 한 규칙군이다.
맨 연도/NUM-ZERO/로마 숫자/첨자 숫자는 정책 판단이 필요하므로 자동으로 읽기 변경하지 않는다.


## 2026-09-28 continuation — 한국어 제공 참조 기준선

자료 부재가 아니라 참조 검수·읽기 정책·개선 우선순위를 결정하는 단계로 바뀌었다.
현재 원본은 Training이다. 소스 요약의 Validation 중복을 그대로 기록했고 EVAL-03은 닫지 않는다.

1. **참조 점검:** 구조 경고1,349건 중 내용/문장부호 변경과 괄호7건을 우선 분류한다.
   수동 검토는 fingerprint가 있는 overlay로 저장하며 원본 headline 지표는 유지한다.
2. **금융/번호 읽기 정책:** 은행·신분증·사업자·전화번호의 공백 제거 실패를 표본화한다.
   0의 공/영, 그룹 구분자의 발화, 식별자/수량, 번호 내 masking을 분리한다.
   데이터와 다르다는 이유만으로 전역 읽기 정책을 바꾸지 않는다.
3. **패턴 후보:** 값/단위·날짜·셈단위의 일반 변형과 부정 참조를 만들고 한 규칙군씩 개선한다.
   raw topic은 의미 분류가 아니다. 구간 경계 불일치를 숫자 값 오류와 따로 확인한다.
4. **사전/명시 제어 후보:** 제품명·코드의 관용형은 사전, 맥락 없는 의도는 제어 구문 후보로
   분리한다. 보조 설정을 적용한 점수를 자동 TN 기본 점수와 합치지 않는다.
5. **공백 정책:** compact만 일치하는63,218문장을 정책별로 묶어 띄어쓰기 필요를 판단한다.
   제공 참조의 자릿수 사이 공백을 제품의 무조건적 정답으로 강제하지 않는다.
6. 같은 import/options/harness/review-file로 새 engine snapshot을 평가하고 웹 이전 기록과
   비교한다. EVAL-03은 이후 템플릿/중복 계열 단위 분리 계약을 먼저 마련한다.
