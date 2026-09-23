# 현재 작업 인수인계

갱신: 2026-09-22. Release **0.20.0**, public ABI **v1**.

## 최근 작업: speech 최종 관문 (2026-09-22, 커밋·미배포)

사용자 사례(www., 가격대, k7, top100, 20F, 유&미, 1+1, 이메일, 제어 문법)는 v0.20에서
이미 기대대로 나옴을 재현했다. 변형 50여 건에서 찾은 약점 중 **C. 최종 관문**을 처리했다.

- 변경: `src/core/speech_output.c`(기호 이름 띄어쓰기, 늘임표, 문맥 인자),
  `src/core/normalize.c`(TN 단계 늘임표, drop 뒤 공백), ko/en validator,
  `resources/{common,languages/*/rules}/speech.tsv`, en `numeric.tsv` 호환 단위 alias.
- 계약: `tests/evaluation/speech_final_gate_v21.jsonl` 40건. 구현 전에 작성했다.
  단어 사이 제거는 사용자 결정(2026-09-22)으로 한 칸을 남긴다 (`가🙂나` → `가 나`).
  v0.20 corpus 4건과 통합·웹·browser 기대값을 같은 결정으로 갱신하고 notes에 기록했다.
- 결과: `tn_cycle cycle` fixed 31 / unchanged-pass 9 / regressed 0
  (`artifacts/tn/final-gate/comparison/cycle.json`, 40건). 단어 사이 제거 결정 반영 후
  Debug CTest 98/98, ASan/UBSan 86/86(단독), Release text-evaluation 33/33,
  browser gate 통과(임시 port 8123, `/tmp/ttf-web-browser`의 Node 22·Playwright).
  외부 영어 3,785문장 results SHA `2de85573…` v0.20과 동일
  (`benchmark/generated/sproat-v21-final-gate`). `build-web` 배포 library는 바꾸지 않았다.

### A. `&` 이니셜·대문자 한 글자 (2026-09-22, 커밋·미배포)

- 변경: `languages/en-US/pack.c`(`ampersand_initialism` 분류), `src/pronunciation/foreign.c`
  (`&` 분해 렌더링, 대문자 한 글자 이니셜). 계약 `tests/evaluation/letters_v21.jsonl` 26건.
- 결과: cycle fixed 20 / unchanged-pass 6 / regressed 0
  (`artifacts/tn/amp-initialism/comparison/cycle.json`). Debug 99/99, ASan/UBSan 86/86(단독).
  외부 영어 출력·exact 동일, `A&M`/`DP&I`/`D&D` IR만 INITIALISM
  (`benchmark/generated/sproat-v21-letters-comparison.json`).
- 남음: `Johnson&Johnson` → `Johnson앤Johnson` 혼합 렌더, 단독 2글자 대문자 단어 충돌.

### B. 이메일·URL 단어·버전·IP (2026-09-22, 커밋·미배포)

- 변경: `src/pronunciation/electronic.c`(주소 렌더러), `src/semantic/classifier.c`
  (`bare_domain_end`, `v` 접두 버전), ko/en `dotted_numbers`, validator 4종
  (`url_tld`, `electronic_word|number|separator`), 사전 11항목(pending).
- 계약 `tests/evaluation/electronic_v21.jsonl` 34건. `test@`는 기존 사전 항목이라
  `테스트`가 맞다는 작성 오류를 커밋 전에 바로잡고 notes에 기록했다.
  v0.20 이메일 계약 2건은 정책 변경으로 갱신했다(`닷 씨오엠` → `닷 컴`).
- 결과: cycle fixed 26 / unchanged-pass 8 / regressed 0
  (`artifacts/tn/electronic/comparison/cycle.json`). Debug 99/99(+신규 gate 통과),
  ASan/UBSan 88/88(단독), Release text-evaluation 35/35, browser gate 통과. 외부 영어 출력 동일
  (`benchmark/generated/sproat-v21-electronic-comparison.json`).

### D. 수식·비교·해시태그 (2026-09-22, 커밋·미배포)

- 변경: `src/semantic/classifier.c`(`math_expression_end`), ko/en `expression` 렌더러,
  `src/core/normalize.c`(식의 문맥 중립), `src/core/speech_output.c`(`speech_symbol_prefix`),
  `C#` semantic_term·사전, `tools/lexicon/manage.py`(`c#` 키 허용).
- 계약 `tests/evaluation/math_hashtag_v21.jsonl` 37건. 한국어 `#`=샵은 사용자 결정이며
  final-gate 계약 1건(`해시 해시태그`)을 같은 결정으로 갱신했다.
- 결과: cycle fixed 28 / unchanged-pass 9 / regressed 0
  (`artifacts/tn/math-hashtag/comparison/cycle.json`). Debug 101/101, ASan/UBSan 89/89(단독),
  Release text-evaluation 36/36, browser gate 통과. 외부 영어 결과 파일 동일
  (`benchmark/generated/sproat-v21-math-comparison.json`).

### E. `대` 구간·대수, 양끝 배수 범위 (2026-09-22, 커밋·미배포)

- 변경: `languages/ko-KR/pack.c`(`classify` → `band_role` 후처리, `ascending_integers` 범위 상속,
  `range_repeat_unit`), `counters.tsv` cue·band 리소스, validator.
- 재현 중 `20대 남성` → `스무 대`(연령이 대수로 읽힘) 값 오류를 발견해 같은 규칙군으로 처리했다.
  기존 golden `1억-2천만원` 보존 계약을 지키도록 오름차순일 때만 범위로 만든다.
- 계약 `tests/evaluation/counter_band_v21.jsonl` 25건. cycle fixed 14 / unchanged-pass 11 /
  regressed 0 (`artifacts/tn/band/comparison2/cycle.json`). Debug 101/101, ASan/UBSan 89/89(단독),
  외부 영어 결과 파일 동일(`benchmark/generated/sproat-v21-band`).

### F. 층·지하층 (2026-09-22, 커밋·미배포)

- 변경: `languages/ko-KR/patterns.c`(`ttf_ko_classify_basement`), patterns.tsv cue·리소스, validator.
- 계약 `tests/evaluation/floor_v21.jsonl` 19건. cycle fixed 11 / unchanged-pass 8 / regressed 0
  (`artifacts/tn/floor/comparison/cycle.json`). Debug 102/102, ASan/UBSan 90/90(단독),
  외부 영어 결과 파일 동일(`benchmark/generated/sproat-v21-floor`).
- 새로 발견: 조사 교정 규칙군(`3F로` → `삼 층로`, `5kg로`, `1+1는`). remaining-work에 기록.

### D 잔여: 소문자 한 글자·메뉴 경로 (2026-09-22, 커밋·미배포)

- 변경: `src/pronunciation/foreign.c`(소문자 한 글자), `languages/en-US/pack.c`(`hyphen_letters`,
  `joined_letters` 문맥 중립), `src/core/speech_output.c`(`speech_silent`), 리소스·validator.
- 계약 `tests/evaluation/letters_paths_v21.jsonl` 20건. cycle fixed 14 / unchanged-pass 6 /
  regressed 0 (`artifacts/tn/letters-paths/comparison/cycle.json`). Debug 103/103,
  ASan/UBSan 91/91(단독), 외부 영어 결과 파일 동일.
- 결정 필요: `Z` 한국어 이름 `지` vs `제트`(`Z세대`).

### Z 읽기와 `#`숫자 (2026-09-22, 커밋·미배포)

- 사용자 결정: `Z`=제트(G=지와 구분), `#1`=일 번 / number one.
- 변경: `phones.tsv` letter.Z, `src/semantic/classifier.c`(`syntax.number-sign`), ko/en
  `number_sign` 템플릿, 구두점 앞 공백을 넣지 않도록 `@math`/`@number-sign` 간격 규칙 보완.
- 계약 `tests/evaluation/letter_number_sign_v21.jsonl` 17건. D의 `번호#123` 기대값을 같은
  결정으로 갱신했다. cycle fixed 10 / unchanged-pass 7 / regressed 0. Debug 104/104,
  ASan/UBSan 92/92(단독), 외부 영어 결과 파일 동일.

### 조사 교정 (2026-09-22, 커밋·미배포)

- 변경: `src/core/normalize.c`(`particle_agreement`, `following_particle`),
  `src/semantic/classifier.c`(`#N` 뒤 조사 허용), `context.tsv`의 `particle_form`, validator.
- 공백 없이 붙은 조사 span만 대상으로 한다. `이며/며` 같은 선택적 축약은 표에서 제외했다.
- 계약 `tests/evaluation/particle_v21.jsonl` 20건. cycle fixed 8 / unchanged-pass 12 /
  regressed 0. Debug 105/105, ASan/UBSan 93/93(단독), 외부 영어 결과 파일 동일.
- 기존 fixture 4곳(`고객는`, `정확는`, `패턴는`, `관용형는`, `유에스커스텀를`)은 비문이라
  교정형으로 갱신했고, alias 테스트는 조사 규칙을 테스트 안에서 따로 구현해 비교한다.

### 혼합 대소문자·숫자+문자 (2026-09-23, 커밋·미배포)

- 데이터만 변경: 사전 15항목(pending), `semantic_term` 5G/4G/3G/2D, `tools/lexicon/manage.py`의
  숫자 선행 라벨 키 허용. 코드 변경 없음.
- 계약 `tests/evaluation/mixed_case_v21.jsonl` 20건. cycle fixed 13 / unchanged-pass 7 /
  regressed 0. Debug 106/106, ASan/UBSan 94/94(단독), 외부 영어 결과 파일 동일.

### 대표번호·괄호 전화번호 (2026-09-23, 커밋·미배포)

- 실제 문장 훑기에서 `1588-0000` → `천오백팔십팔에서 영`(값 오독), `연락처(010-...)` 미읽기를 발견.
- 변경: `src/semantic/phone.c`(`service_number`, `bracketed_number`, 닫는 괄호 tail 허용),
  ko `phone_service_prefix` 16개, validator.
- 계약 `tests/evaluation/phone_service_v21.jsonl` 17건. v0.18 `1588-0000` 범위 계약을 전화로
  갱신하고 사유를 기록했다. cycle fixed 9 / unchanged-pass 8 / regressed 0.
  Debug 107/107, ASan/UBSan 95/95(단독), 외부 영어 결과 파일 동일.
- 같은 훑기에서 발견한 남은 문제: `A/S` → `A slash S`, `SAVE20을` → `에스에이브이이투지로를`,
  `09:00` → `아홉 시 영 분`, `13.6인치` 띄어쓰기.

다음 순서(후보): 슬래시 약어(`A/S`)와 영문+숫자 코드(`SAVE20`), 시각의 `영 분`,
`2x`·괄호 식, URL 경로 단어. 한 번에 한 규칙군씩 진행한다.

## v0.20에서 마무리한 범위

- `speech` 텍스트: www. initialism, 가격대 범위, letter/word+number,
  층수 문맥, 이항 더하기, & 및 이메일·URL 철자 읽기.
- `annotated` 입력: `1{첫}번째`, `1{@native}`, `1{@sino}`,
  `ABC{@alphabet}마트`, `[[1+1]]{원플러스원}`, escape/error/byte cap.
- 미지원 한자·이모지는 **원문 보존 기본**. drop은 명시적 선택.
  literal override/보호 코드/의도적 semantic guard의 별도 계약 유지.
- 웹 옵션·예제·51건 corpus, trace의 보존/제거 이유 및 리소스 출처.
- 모델 독립적인 사례 접수·snapshot·수정 전후 비교 도구.

일반 모델이 한 규칙군을 마칠 수 있는 절차는
[tn-improvement-playbook.ko.md](tn-improvement-playbook.ko.md)에 있다.
기존 무기한 goal은 자동 재개하지 않는다. 다음 요청을 받으면 한 이슈/한 규칙군만
선택하고, 결과와 다음 순서를 남긴 뒤 종료한다.

## 배포 상태 (2026-09-23)

`build-web` library와 저장소 `resources`를 커밋 `90bcf59` 기준으로 다시 빌드하고
`ttf-workbench.service`를 재시작했다. `/api/health`는 `{"ok": true, "version": "0.20.0"}`이며
CMake release 번호는 아직 0.20.0이다(릴리스 번호 상향은 별도 결정). 대표 출력으로
`지하 2F 주차장`, `1~2억대 아파트`, `20대 남성`, `5kg로`, `R&D 부서`, `3-1=2`, `#1 인기`,
`Z세대`, `설정>일반`, `x-y 좌표`, `iOS 17.2`, `5G 요금제`를 서비스 API에서 확인했다.
재시작 전에는 실행 중이던 예전 프로세스가 새 참조 목록을 읽으며 health 실패 상태였다.

## 진실의 기준

| 항목 | 위치/계약 |
|---|---|
| 현재 release/빌드 | CMakeLists.txt; main commit은 `git log -1 --oneline` 확인 |
| 런타임/API | include/tts_text_frontend; 기존 C engine/CLI 기본 plain/text |
| 웹 | speech/plain/preserve를 명시적으로 선택, port8000 |
| 제어 문법/리소스 | input-format.md, speech-resources.md |
| 신규 참조/계약 | tests/evaluation/user_patterns_v20.jsonl, tests/integration/test_user_patterns.py |
| 검증 기록 | research/speech-controls-v20-results.md |
| 데이터 기반 다음 순서 | ../benchmark/manifests/quality-work-queue.json |
| 구조·운영의 잔여 범위 | remaining-work.md (전체 완료로 표시하지 않음) |
| 실행 하네스 | ../tools/quality/tn_cycle.py --help |

## 검증과 아직 모르는 것

신규 51/51 text 계약, 813 CLI 계약, 2,164 native split/error/ownership 검사 통과.
웹 API/ABI 133 검사 및 browser gate 통과. 기존 외부 plain/text 개발 corpus는
3,785문장/46,213 span에서 v0.19와 결과 전체 동일, stream11,355회 불일치0.
이는 speech 모드의 일반 정확도 인증이 아니다. TN 대상 exact는1,118/3,293이고,
937개 로컬 참조가 아직 독립 검수 대기다. 사전379항목/381키 중294 pending,
85 source-checked, independently human-reviewed0이다.

GCC96개/sanitizer84개 기존 suite에서 각95/83개가 병렬 실행 통과했고,
남은 30k-number timeout gate는 **단독 재실행 통과**했다 (GCC19.92초,
ASan/UBSan48.50초). 시간 제한을 늘리거나 실패를 지우지 않았다.
Release build-web의 신규 기능·harness·web/CLI 선택 gate **7/7**도 통과했다.
새 harness gate는 실제 CLI로 fixed/regressed/trace/snapshot 훼손/참조 덮어쓰기
방지 검사에 통과했다. 릴리스 CI는 GitHub Actions의 해당 main commit에서 확인한다.

## 이 VM의 운영

- 저장소: `/home/exedev/tts-text-frontend`
- 웹: `https://tts-text-frontend.exe.xyz:8000/`
- 사용자 서비스: `ttf-workbench.service`
- 배포 library: `build-web/libtts_text_frontend.so`, resources는 저장소의 `resources/`
- 빌드/데이터는 ignored `build*`, `artifacts/`, `benchmark/generated/`에 둔다.
- GCC와 sanitizer 전체 suite를 이 2-core VM에서 동시에 실행하지 않는다.
- 배포 시 library와 resource revision을 함께 갱신하고 health/대표 출력 확인.
  환경별 HTTPS 프록시는 exe.dev 공식 문서만 사용한다.

```sh
git status --short
git log -1 --oneline
curl --fail --silent http://127.0.0.1:8000/api/health
systemctl --user status ttf-workbench.service --no-pager
```

## 다음 AI에게 줄 요청 템플릿

> AGENTS.md와 docs/HANDOFF.md를 읽고, 아래 TN 사례 하나의 원인을 최신 버전에서
> 재현해라. docs/tn-improvement-playbook.ko.md의 snapshot/cycle을 사용하고
> expected를 엔진에 맞춰 바꾸지 마라. 같은 규칙군의 변형·부정 예를 포함하고,
> resource/pack 수정 후 관련 gate와 동일 참조 비교 결과를 기록해라.
> 이번 규칙군을 마치면 중단하고 남은 항목을 인계해라.
>
> 원문: … / 기대 출력: … / 언어·문맥: …
> output_mode: speech|text / input_format: plain|annotated
> unreadable: preserve|drop / domain·customer: … / 읽기 근거: …

로컬 마무리 로그: `/tmp/ttf-v20-context-isolated.log`,
`/tmp/ttf-v20-asan-context-isolated.log`, `/tmp/ttf-v20-web-gates.log`.
하네스 실제 snapshot/cycle smoke 결과:
`artifacts/tn/v20-handoff/comparison/cycle.json` (candidate_failed0, regressed0).
이 smoke는 같은 capture의 재현성 검사이며 품질 개선 수치가 아니다.
