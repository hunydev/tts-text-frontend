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

- [ ] **NUM-01: Unicode 숫자 묶음의 부분 정규화 방지.** 실제 CLI에서
  `금액은 1\u202f250원입니다.`의 U+202F를 문자로 넣으면
  `금액은 일 이백오십 원입니다.`가 된다. [tokenizer](../src/tokenizer/tokenizer.c)는
  U+202F를 whitespace로 분류하지 않는다. 숫자 구분자 정책과 유효한 묶음 문법을
  정하고 하나의 정확한 `1250/KRW` IR로 인식하거나 의심 구간 전체를 보존해야 한다.
  공백을 무조건 지워 서로 다른 수를 합치지 않는다. **닫는 증거:** NBSP, NNBSP,
  thin space, ASCII space의 정상/비정상 grouping, 통화·단위·identifier 조합에 대한
  독립 값 검사와 모든 두 조각 UTF-8 byte 분할 검사.

- [ ] **KO-01: 한국어 서수·횟수·번호 counter를 분리.** 현재
  `1번째 2번째 3번째 20번째입니다.` → `일 번째 이 번째 삼 번째 이십 번째입니다.`.
  [counter resource](../resources/languages/ko-KR/rules/numeric.tsv)의 `번=sino`가
  `번째`에 적용된다. `번째`, 횟수의 `번`, 식별번호의 `번`, `제N`의 의미를 분리하고
  읽기 정책을 resource로 기록한다. **닫는 증거:** 첫/두/세/스무 계열과 큰 수,
  조사 결합, 번호·횟수 최소 대조 문장, 범위, invalid input, streaming IR 검사.

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

- [ ] **RES-02: resource 의미 검증을 setup 단계에서 완료.** compiler는 일반 TSV
  field 구조를 검증하며 각 pack policy의 의미까지 검증하지 않는다. 예를 들어
  날짜의 잘못된 읽기 style은 실제 사용 시 `TTF_ERESOURCE`가 된다.
  **닫는 증거:** 종류/언어/키별 value schema, 알 수 없는 policy의 명시적 처리,
  compiler와 load/freeze 검증 일치, 실패 load의 원자성. 확장 pack의 새 resource를
  core 고정 allowlist 때문에 사용할 수 없게 만들지 않는다.

- [ ] **TRACE-01: source dictionary/G2P의 실제 layer와 선택 경로 전달.**
  임시 customer resource `pronunciation\ten-US\tabacus\tAE B AH K AH S`를 읽히면
  `abacus를 읽습니다.`의 inspect는 `pronunciation-dictionary`와
  `pronunciation_layer=common`을 출력한다. [EN pronounce](../languages/en-US/pack.c)의
  resource 조회가 layer를 반환하지 않고 [foreign processor](../src/pronunciation/foreign.c)는
  직접 lexicon hit에만 layer를 전달한다. **닫는 증거:** common/system/domain/customer/user
  source dictionary 선택과 target mapper 선택을 구분하는 정확한 trace; compound
  발음도 출처를 잃지 않는 검사. 기존 ABI v1 layout을 조용히 바꾸지 않는다.

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

- [ ] **TN-01: 전화·digit·주소·electronic text의 명시적 읽기 정책.** 외부 표본의
  TELEPHONE 0/13, ELECTRONIC 0/22, ADDRESS 0/4를 원문 보존 정책과 인식 실패로
  분리해야 한다. 현재 `Call me at +1 212 555 0100.`은
  `Call me at plus one two hundred twelve five hundred fifty five one hundred.`가 된다.
  **닫는 증거:** 국제/국내 전화, extension, 계좌/번호와 숫자 수량의 최소 대조;
  digit order·leading zero 보존; URL/email/IP/version의 preserve/spoken 정책과 trace.
  Sproat 표기를 맞추기 위해 제품의 원문 보존 계약을 몰래 바꾸지 않는다.

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
  집계는 374항목: `source-checked` 85, `pending` 289, `human-reviewed` 0이다.
  금융/콘텐츠의 실제 빈도·miss를 기준으로 category와 domain scope를 확충한다.
  **닫는 증거:** 명확한 출처·발음 정책·권리·검수자/날짜가 있는 원장, canonical collision
  검사, 실제 문장 내 도달성, customer/user 우선순위, 독립 holdout coverage.
  source에 단어가 있다는 사실을 한국어 발음 검증으로 표시하지 않는다.

- [ ] **LEX-02: CMU 복수 발음의 runtime 선택.**
  [importer](../benchmark/importers/cmudict/README.md)는 stress와 모든 variant를 보존하나
  runtime export는 명시적으로 `base` 하나만 고른다. 빈도/POS 의미를 제공하지 않는
  순번으로 우선순위를 발명하지 않는다. **닫는 증거:** 수동/context 정책을 포함한
  deterministic variant 선택, `read`/`lead` 같은 대조, 모든 variant 보존과 출처 trace,
  user/customer target lexicon이 언제나 source fallback보다 우선하는 검사.

- [ ] **PRON-01: unknown Latin fallback의 텍스트 정확도.** 현재 greedy G2P는 최대
  4글자 resource match이고 KO mapper는 제한된 음절화 및 stress 무시 경로다.
  발화 가능한 한글이 나온다는 보장은 올바른 발음 보장이 아니다.
  **닫는 증거:** 사전에서 의도적으로 제외한 독립 어휘의 한국어 reference, known-hit와
  OOV 성능 분리, NIKL 원칙/관용형의 적용 이유, 다중 음절·자음군·모음·강세 변형과
  장문 단어 검사. 오류를 시스템 사전에 하나씩 추가한 뒤 OOV 개선으로 세지 않는다.

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

- [ ] **EVAL-03: 개발 표본과 분리된 held-out 평가.** 현재 외부 3,785문장은
  Sproat training-prefix 개발 표본이며 MONEY 33, TIME 4, FRACTION 2처럼 불균형하다.
  **닫는 증거:** corpus/source/reference hashes, source split과 overlap 검사,
  semiotic class별 충분한 표본, 금융·콘텐츠 구분, 실패/정렬불가를 포함한 분모,
  변경 전에 봉인한 holdout. 현 개발 표본 개선을 production 정확도로 소개하지 않는다.

- [ ] **EVAL-04: 한국어 외부 원본의 실제 검증.** KsponSpeech와 CoreaSpeech는
  importer와 합성 fixture가 있으나 원본을 확보·평가하지 않았다.
  **닫는 증거:** 권리가 확보된 환경에서 실제 encoding/dual annotation/export schema를
  검증하고 원문·pair·제외 사유를 추적; TN 관련 span만 후보로 추출한 후 독립 검수.
  Corea의 `text1/text2`나 N2gk 출력 자체를 정답 쌍으로 만들지 않는다. 해당 데이터가
  없어도 별도 허용된 한국어 corpus의 확보·평가 작업은 계속한다.

## P2 — runtime·배포·운영 검증

- [ ] **PERF-01: 대형 사전과 규칙 인덱스.** 현재 resource hash는 1,021 fixed buckets,
  phrase key는 8 words/255 bytes로 제한되고 rule matcher는 rule 목록을 순회한다.
  TTFB는 checksum이 있는 TSV container이며 runtime에서 다시 parse/allocate한다.
  **닫는 증거:** storage abstraction을 유지한 확장 인덱스, 동일 우선순위 결과,
  큰 licensed 또는 합성 사전/충돌 집중 key/multiword prefix의 p50·p95·RSS·load/freeze
  비교; compiler/runtime format version과 corruption 검사. 사전 수만 늘리지 않는다.

- [ ] **STREAM-01: 최악 조건의 pending·CPU 비용.** 현재 열린 인용/코드/opaque literal은
  closer 또는 flush까지 보류하며 일부 delimiter가 많은 입력은 반복 scan한다.
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
