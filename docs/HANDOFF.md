# 현재 작업 인수인계

갱신: 2026-09-27. Release **0.20.0**, public ABI **v1**.

## 이번 인계: CJK 정규화 — 1~4단계 완료, 승인된 기준선 커밋

권위 문서: [cjk-korean-reading.ko.md](cjk-korean-reading.ko.md), 특히 §5.4/§5.5/§9/§18.
Base `4386841`, HEAD `d9d3342` (이전 승인된 trace 비교 게이트 커밋).
사용자가 CJK 기준선 커밋 후 KO/EN 품질 개선 재개를 승인했다.
이 커밋은 사용자 준비 설계·생성기·데이터·86계약 및 검증된 CJK 구현을 함께 고정한다.
push·배포·서비스 재시작은 수행하지 않았다. 다음은 KO/EN 자원표 한 규칙군이다.

### 확정된 동작과 범위

- 한자·가나 기본값은 `passthrough`; CJK source는 `und` 유지. 두 옵션 독립.
  `--hanja korean`: `내 이름은 張勳입니다.` → `내 이름은 장훈입니다.`.
  `--kana korean`: `ソウル うどん ボウル` → `서울 우동 보우루`.
  둘 다 켜면 `東京とうきょう` → `동경도쿄`.
- ABI v1 유지, frontend options는 v0.20 prefix32B부터 current40B까지 받는다.
  부족한 필드 기본값, 과대 크기 거부. legacy initializer는32B만 기록한다.
- Han/Kana는 script enum 끝에 추가; 기존 토큰 병합/문맥 점수는 legacy-equivalence 유지.
  unified scanner, runtime 두음/어두 변이, preferred첫 독음, 전용 word index,
  촉음/발음의 받침 결합 및 미결합 원문 보존, halfwidth 최장 fold를 구현했다.
- 한자8,116 / 가나301 / 단어5행 고정. 기대값/TSV 수동 변경 없음.
  계약86 SHA256 `721c212c3ab21c156578725b917de8c6b0c8e9dc8968ce75af85805d6a7e558e`.
  가나27규칙 + 이전pending3 =30/30, 전체86/86, pending0.
- 미등록/간체/신자체는 원문 보존. speech drop은 낱말 간격 유지(`山𠮷水`→`산 수`).
  가타카나 모음 장음 흡수 없음(`トウキョウ`→`도우쿄우`); 히라가나만 흡수한다.
  형태소 분석/원어 복원/독립 언어 감수/외부 한국어 TN gold는 범위 밖이다.
- 스트리밍은 기존 phrase 보호를 최대6cp CJK word 길이만큼 확장했다.
  별도 전체 문장 버퍼 없음. 86건×2profile, 모든 2조각 byte경계/1byte/random
  **2,488회 text/span/trace 일치**, flush 전 문장/구 callback 확인.
- `generate.py build --check`는 세 TSV를 임시 재생성해서 LF 포함 바이트 비교한다.
  자동 외부 다운로드 없음. 사용자 contract는 simulator로 재생성하지 않는다.

### 마지막 수정: §5.5 조건부 로드 + §5.4 전용 힙 인덱스

공용 해시에 CJK 행을 남기지 않는 것만으로 warm throughput은 회복됐지만,
사용자가 기동마다 미사용8,422행을 파싱하는 비용을 확인했다(§9.2).
manifest를 유지하는 (b)를 채택했다. create는 CJK 세 경로를 건너뛰고,
freeze에서 최종 옵션에 맞는 reading 파일과 공통 word 파일을 SYSTEM으로 읽는다.
둘 다 passthrough 및 setter 없는 legacy는 CJK 파일을 **열지 않는다**.

- 원래 manifest/validator/레이어를 유지. `.tsv`, `.bin`, `.ttfb` 예약 basename 지원.
  실패 시 이번 freeze의 행과 declaration 번호를 모두 되돌리고 mutable 상태로 남긴다.
  마지막 파일 오류, final validator 오류, 수정 후 재시도, 중복 freeze를 검증했다.
  CJK index 할당은 마지막 실패 가능 작업; 성공 후 원래 엔트리만 옮긴다.
- 먼저 로드한 동일 SYSTEM 행은 계속 우선, customer/user override 불변.
  공용 해시는3,459행. 전용 인덱스는 heap, 엔진은포인터1개/17,320B.
  `e->rules` 미사용. word bucket은 원래 행을 빌리며 destroy가 owning 행을 한 번씩 해제.
- `resource_root=NULL`도 create에서 확정된 같은root를 사용한다.
  공개 freeze 이후load는 `TTF_ESTATE`. validation/get은 **현재 로드된 모든 행**을 본다.

| 옵션 (추가 override 없음) | freeze 전 record | freeze 후 record |
|---|---:|---:|
| 둘 다 passthrough / legacy | 3,459 | 3,459 |
| hanja만 korean | 3,459 | 11,580 |
| kana만 korean | 3,459 | 3,765 |
| 둘 다 korean | 3,459 | 11,881 |

### 검증 결과

- 이전 GCC 전체 **161/161 통과**는 그대로 보존하고 전체를 다시 실행하지 않았다.
  이번 수정 영향 범위 **CJK9/9 + 리소스·사전·trace·golden 등23/23** 통과.
- passthrough **2,235건 출력 회귀0**; 허용script9곳 외 메타데이터 차이0.
  기존 declaration offset7건도 이제0. 상대순서/선택행 불변.
- GCC CLI 기동25표본 중앙값: **17.972ms /16.5ms =1.089×**, 한도1.25 통과.
  참고용 Korean: hanja43.889ms, kana19.065ms, both44.704ms.
- 13,776B 순수한글 warm throughput: 기준71.910ms → **74.632ms (1.038×)**,
  +18,000단어74.910ms (**1.042×**), 한도1.25 통과.
  두 옵션 Korean/비CJK입력 참고75.0ms. 모든 성능 측정은 suite와 겹치지 않았다.
- Clang ASan+UBSan `evaluation_reports` **30.37초 통과**, 기존60초 제한 불변.
  corpus109건 및 harness의 후속 검사를 통과. 평가 러너/기동 횟수/코퍼스 변경 없음.
- 새 로드/롤백/해제·동시읽기 sanitizer **7/7**, 메모리·누수·UB 진단0.
- 이전에 못 실행한 sanitizer **24/24 통과 (276.04초)**. 새 전체테스트 수는 GCC162/ASan145;
  이번에 전체를 처음부터145/145 실행했다고 보고하지 않는다. 이름 목록으로 이전24개를 재개했다.
- compiler manifest3파일 네 옵션의 출력/trace일치; lexicon inventory/references/
  generation--check 통과. `git diff --check` 통과. 최종 snapshot도 보존했다.

### 재현 명령과 증거

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure --stop-on-failure -R '^cjk_'
python3 tests/integration/test_cjk_passthrough.py --before artifacts/tn/cjk-v22/baseline/libtts_text_frontend.so --before-resources artifacts/tn/cjk-v22/baseline/resources --after build/libtts_text_frontend.so --after-resources resources --report artifacts/tn/cjk-v22/NEW-passthrough.json
python3 tools/cjk/check_startup.py --ttf build/ttf --resources resources --report artifacts/tn/cjk-v22/NEW-startup.json
python3 tools/cjk/check_throughput.py --before artifacts/tn/cjk-v22/baseline/libtts_text_frontend.so --before-resources artifacts/tn/cjk-v22/baseline/resources --after build/libtts_text_frontend.so --after-resources resources --report artifacts/tn/cjk-v22/NEW-throughput.json
cmake -S . -B build-v20-asan
cmake --build build-v20-asan -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-v20-asan --output-on-failure --stop-on-failure -R '^evaluation_reports$'
# 별도 실행: 같은 환경 변수로 asan-resume-regex.txt의 이름24개 선택
```

`artifacts/tn/cjk-v22/stage4-lazy/`에 gcc/asan 빌드·검증 로그, 이전144개 목록,
재개24개 이름/regex/선택검증JSON, frozen계약보고서, passthrough/startup/throughput.json,
status.json, CJK소스조사표가 있다. `stage4-lazy-candidate/`는 최신 immutable snapshot.
`stage4/`의 이전 전체GCC와 sanitizer실패 로그는 덮거나 삭제하지 않았다.
과거수치/원인변경은 설계§18에 남겼다. `stage4-cycle/`의 stage1→stage4 비교는
8/86→86/86, fixed78/regressed0. 이번 로드 변경은 정확도 개선으로 다시 세지 않는다.

### 다음 작업 (별도 승인/범위)

1. CJK 기준선 커밋 이후 KO/EN 자원표 한 규칙군을 진행한다. push/배포는 별도다.
2. 배포/웹 CJK 선택기는 별도 진행. live 서비스는 이전버전 그대로다.
3. 공용 hash 전체 힙화/동적 크기 및 layer별 counter 변경은 별도 핵심저장소 작업.
4. 독립 감수·실제 입력 검토는 가능하나 간체사전/단어 coverage확장은 이번 범위 밖이다.

## 이전 인계: 날짜 뒤 영문 요일 / 자원표 점검 (base `842dbfd`, 2026-09-27)

이전 조사표 커밋 `842dbfd`의 GitHub Actions 전체 성공을 확인한 뒤, 5번의
month/weekday alias 규칙군 하나를 진행했다. 다음 작업을 자동으로 시작하지 않는다.

- 기존 월 alias 24개는 모두 정상. 기존 한글 weekday cue 14개도 점검했다.
  영문 alias 14개 + KO/EN 읽기 14개를 추가했다. `마감은 5/8(Fri)입니다.` →
  `마감은 오 월 팔 일(금요일)입니다.`.
- `languages/en-US/date.c`가 날짜에 바로 붙은 괄호 안 alias만 분류한다.
  공통 IR은 `DATE(value=1..7, unit=weekday, secondary=접힌 alias)`이며 KO/EN
  verbalizer가 각자의 `weekday` 표로 렌더링한다. Core는 단서의 대소문자·점 처리와
  layer 전달만 일반화했다. 공개 구조체/symbol/ABI 변경 없음.
- 상세 trace에 실제 `weekday_label`/`weekday` 행을 기록한다. 괄호는 독립 span이며
  read/preserve/skip을 유지한다. 사용자 파일의 alias 추가·0 비활성·읽기 override를 검증한다.
- 신규 `calendar_alias_v21.jsonl` 72건을 구현 전에 직접 작성했다. 최초 precheck의
  분수 negative 3건은 `five eighths`로 작성돼 기존 `fraction_over=over`와 달랐다.
  **구현 전** 정책 검토에서 정정하고 원본을 artifacts에 보관했다. 최종 같은 corpus의
  baseline 33/72 → candidate 72/72, fixed 39 / regressed 0. 정정 3건을 개선으로 세지 않았다.
  기존 활성 corpus 기대값 수정은 없다. 독립 사람 검수 및 외부 KO gold는 아직 없다.
- corpus SHA256: `37f7d7d908e12e959195130576917362d489e3e70e5f0b2aa28372a7498d673c`.
  `artifacts/tn/calendar-alias/`에 baseline, 최초 reference/precheck, 개발 결과,
  candidate/comparison을 보존한다. 최종 경계값 수정 후 캡처·검증 위치는 아래에 기록한다.
- CTest·reference audit·웹 `calendar-alias-v21` 등록.
  [달력 별칭 편집 가이드](calendar-alias-resources.md), Semantic IR/validation/THIRD_PARTY 갱신.
- 외부 영어 표본 3,785문장·46,213span 결과는 이전과 동일. exact 35,313 및 chunk
  11,355회 불일치 0. 결과 SHA256 `3f2d4d69e9a7c5466bd884c41a5623d6e2e50d9273e75915b47befe44fa4bf79`.
  이번 합성 표본에서 확인한 개선을 외부 정확도 향상으로 주장하지 않는다.
- 남은 범위: prefix 요일(`Mon., May 8`), 날짜/괄호 사이 공백, 한자 요일 및 한글 약자
  확장, 날짜-요일 불일치 진단. 날짜 lookbehind는 96바이트로 제한한다.
  다음 자원표는 **abbreviation** 또는 **speech_character** 중 하나만 선택한다.
  맨 연도·NUM-ZERO·NUM-WIDTH·로마 숫자·EVAL-03은 별도 범위다.

재현 명령:

```sh
ctest --test-dir build --output-on-failure -R 'calendar_alias|resource_semantic_validation'
python3 tools/quality/tn_cycle.py cycle \
  --baseline artifacts/tn/calendar-alias/baseline \
  --candidate artifacts/tn/calendar-alias/final-candidate \
  --corpus tests/evaluation/calendar_alias_v21.jsonl --out artifacts/tn/calendar-alias/NEW-comparison
```

검증·배포:

- GCC Debug 전체 **152/152 통과**, 692.12초 (`/tmp/ttf-calendar-all-tests.log`).
  이 전체 실행은 `candidate` snapshot의 바이너리다. 이후 허용 최대 31자 alias에
  끝 점을 붙였을 때 공통 단서 버퍼가 부족한 경계를 수정했다(32 → 33바이트).
  최종 바이너리는 관련 **4/4 통과** (`/tmp/ttf-calendar-final-targeted.log`):
  신규 corpus/native byte split, resource semantic validation, compiled resources.
  이 최종 한 바이트 경계 수정 후 로컬 전체 152개를 재실행했다고 표시하지 않는다.
- Native 비교 **2,122회**: 두 분절 profile에서 모든 두 조각 UTF-8 분할의
  text/span/trace 일치. 고객 alias 추가·비활성·읽기 교체, 31자+점, KO context/source,
  bracket read/preserve/skip 및 부정 사례를 별도 검사했다.
- 최종 Clang ASan/UBSan 관련 **6/6 통과**, 73.07초
  (`/tmp/ttf-calendar-sanitize.log`): context score cache, resource validator, 신규 corpus,
  bracket corpus, English date value contracts, compiled resources. sanitizer 전체는
  로컬 재실행하지 않았으며 push CI가 전체 GCC/Clang/ASan/UBSan을 실행한다.
- 최종 `artifacts/tn/calendar-alias/{final-candidate,final-comparison}`: fixed 39,
  unchanged-pass 33, regressed 0. 기준문 SHA는 위와 동일하다.
  외부 재평가 `benchmark/generated/sproat-v21-calendar-alias-final` 및
  `sproat-v21-calendar-alias-final-comparison.json` 역시 기존 결과 SHA와 완전히 같다.
- Reference audit: 50파일·1,618건, 구조 결함 0, 사람 승인 0 / 전체 독립 검수 대기.
  `artifacts/tn/calendar-alias/final-reference-audit.json`.
- Release `build-web` 재빌드, `ttf-workbench.service` 재시작 완료. 브라우저 gate 통과
  (`/tmp/ttf-calendar-browser.log`), live API 신규 corpus **72/72**, 상세 trace와
  chunk parity 확인 (`artifacts/tn/calendar-alias/live-web.json`).
  `/api/health`는 `ok=true, version=0.20.0`; 이번에도 release 번호는 변경하지 않았다.
- 편집 가이드의 TSV 예제를 `validate-resource --require-complete-validation`으로 검사하고
  `마감은 오 월 팔 일(금)입니다.` 출력도 확인했다. 기존 동작을 바꾸는 고객 정책은
  시스템 기본 사전에 합치지 않았다.
- `gh run list --branch main --limit 1`로 이번 push의 전체 CI를 확인한다.
  위는 로컬 실행 증거이며 pending CI를 성공으로 기록하지 않는다.


---

## 이전 인계: 조사 자원표 한 규칙군 (base `aa562a7`, 2026-09-27)

사용자가 제시한 여섯 후보 중 **5번의 조사표**를 선택했다. 다음 작업을 자동으로 시작하지 않는다.

- `resources/languages/ko-KR/rules/context.tsv`에 의무적 조사 쌍 8개를
  `particle_form` 16행 + `particle` 16행으로 선언했다. 공개 ABI 변경 없음.
- 전체 회귀 검사에서 기존 고객 문자표 계약의 오인식이 드러났다: `가漢나`가 `가한이나`,
  `가1️⃣나`가 `가키캡일이나`가 됐다. 기대값은 그대로 두고 `normalize.c`에서 미분류 OTHER
  identity / unreadable-cluster 이름 치환 뒤에는 조사 교정을 생략하도록 좁혔다.
  문자 이름 출력이 명사 경계라는 근거는 없으므로 자기 span만 바꾼다. 이는 해당 문자 뒤의
  실제 조사도 자동 교정하지 않는 보수적 경계이며, 별도의 의미 분류가 필요한 후속 범위다.
- `3랑` → `삼이랑`, `GPU이랑` → `지피유랑`, `1만원라도` → `만 원이라도`.
  마지막 사례는 금액 경계까지 복구하며 CURRENCY(value=10000, unit=KRW)를 유지한다.
- 변경 **전에** 고정한 `particle_table_v21.jsonl` 72건: baseline 35/72 → candidate 72/72,
  fixed 37 / regressed 0. corpus SHA256
  `346d129829bf4d9a942cdfb92fd606ed40902697b86283c2b621fd52bb65d457`.
  기존 기대값은 바꾸지 않았다. 독립적인 사람 검수 및 외부 KO gold는 여전히 없다.
- `tests/integration/test_particle_resources.py`: 두 분절 설정의 모든 두 조각 UTF-8 분할
  1,390회에서 text/span/trace 동일. 금액 값, 선택 resource의 입력 offset·layer,
  customer 조사표 override 및 user 발음에 따른 교정 검사도 통과.
- `tn_cycle` snapshot/cycle: `artifacts/tn/particle-table/{baseline,final-candidate,final-comparison}`.
  최초 resource-only candidate/comparison은 발견 과정의 기록으로 별도 보존했다.
  외부 영어 `sproat-v21-leadzero` 대 `sproat-v21-particle-table-final`: 3,785문장·46,213span,
  exact 35,313 그대로, chunk 11,355회 불일치 0. 결과 파일 SHA256 양쪽 동일:
  `3f2d4d69e9a7c5466bd884c41a5623d6e2e50d9273e75915b47befe44fa4bf79`.
  비교는 `benchmark/generated/sproat-v21-particle-table-final-comparison.json`.
- corpus는 CTest·reference audit·웹 `particle-table-v21`에 등록했다.
  사람용 [조사표 편집 가이드](particle-resources.md)와 THIRD_PARTY 참고 내역을 추가했다.
- 남은 범위: 조사 연쇄(`이랑은`), 서술격 선택 축약, 호격, 문맥 prefix 오인식의 형태소 분석.
  다음에는 5번의 **month/weekday alias** 표를 한 규칙군으로 훑는 작업을 권한다.
  맨 연도·NUM-ZERO·NUM-WIDTH·로마 숫자·EVAL-03은 이번에 다루지 않았다.

재현 명령:

```sh
python3 tools/quality/tn_cycle.py cycle \
  --baseline artifacts/tn/particle-table/baseline \
  --candidate artifacts/tn/particle-table/final-candidate \
  --corpus tests/evaluation/particle_table_v21.jsonl --out artifacts/tn/particle-table/NEW-comparison
ctest --test-dir build --output-on-failure -R 'particle|compiled_resources'
```

검증·배포:

- Debug 전체 150개 실행에서 149개 통과, `user_pattern_contracts` 1개 실패로 위 문자 치환
  회귀를 찾았다 (`/tmp/ttf-particle-table-ctest.log`). 경계 수정 뒤 관련 **9/9 통과**
  (`/tmp/ttf-particle-table-guard2-check.log`). 최종 바이너리로 전체 150개를 다시 실행했다고
  표기하지 않는다. push CI의 GCC/Clang 전체 실행은 해당 main commit의 Actions에서 확인한다.
- 최종 ASan/UBSan 관련 **6/6 통과**: user-pattern 계약, 기존/신규 particle corpus,
  resource validation/compiler, context cache. 30k-number는 **58.67초**로 기존 60초 제한 유지.
  `/tmp/ttf-particle-table-final-sanitize.log` (이번에 sanitizer 전체 suite를 재실행하지 않음).
- 기준선부터 작성한 참조 SHA는 그대로이며, 최종 snapshot/cycle fixed 37 / regressed 0.
  추가한 참조 audit는 결함 0, 72건 전부 독립 검수 대기.
- 브라우저 gate 통과 (`/tmp/ttf-particle-table-browser.log`). 문자 경계 수정 후에는
  Release `build-web` 재빌드·서비스 재시작, live API로 금액·약어 조사 및 한자/키캡
  문자 치환 4건을 다시 확인했다. 웹 `particle-table-v21`은 72/72 통과.
  `/api/health` = `{"ok":true,"version":"0.20.0"}`. 릴리스 번호는 이번에 변경하지 않았다.
- `gh run list --branch main --limit 1`로 push CI 결과를 확인한다. 이 절은 로컬 증거이며
  pending CI를 성공으로 표시하지 않는다. 로그와 raw 외부 corpus는 Git에 넣지 않는다.

이후의 과거 기록에서 "커밋·미배포"는 각 작성 당시 상태이므로 현재 상태는 `git log`와
서비스 설정으로 확인한다.

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

### 성능 회귀 수정 (2026-09-23)

push한 `30b6b98`의 CI sanitizer 작업에서 `context_cache_compatibility`의 30k-number 게이트가
60초 제한을 넘겼다(80초). 원인은 두 가지 per-span 비용이었다: ko `classify` 래퍼가 숫자
토큰마다 공통 분류기를 다시 호출한 점, 조사 교정이 모든 span에서 리소스를 조회한 점.
`band_unit_ahead`로 `대`가 뒤따를 때만 재분류하고, 조사 교정은 리소스 조회 전에 비ASCII·
한글 받침 검사를 먼저 한다. 제한은 바꾸지 않았다. 로컬 ASan 단독 실행 80초→50.9초
(기존 기록 48.5초), Debug 108/108, ASan/UBSan 96/96, 외부 영어 결과 파일 동일.

### 슬래시 약어 (2026-09-23, 커밋·미배포)

- 변경: `languages/en-US/pack.c`의 `hyphen_letters`/`joined_letters` 중립 판정에 `/` 추가.
- 계약 `tests/evaluation/slash_letters_v21.jsonl` 13건. cycle fixed 7 / unchanged-pass 6 /
  regressed 0. Debug 108/108, ASan/UBSan 96/96(단독), 외부 영어 결과 파일 동일.
- `SAVE20` → `에스에이브이이투지로`는 영어 발음 경로에서 여러 자리 숫자를 자릿수 영어
  이름으로 읽기 때문이며, 읽기 정책 결정이 필요해 보류했다.

### 정시 읽기 (2026-09-23, 커밋·미배포)

- 변경: ko `TTF_SEM_TIME` 렌더러에서 0분·0초 생략, `clock zero_minutes` 리소스, validator.
- 계약 `tests/evaluation/clock_v21.jsonl` 13건. 기존 fixture 3곳(`golden_ko`, `range_semantics`,
  `semantic_boundary_streaming`)의 `영 분`/`영 초` 기대값을 사유와 함께 갱신했다.
  cycle fixed 10 / unchanged-pass 3 / regressed 0. Debug 109/109, ASan/UBSan 97/97(단독),
  외부 영어 결과 파일 동일. push한 `9767c5f`의 CI는 성공이다.

### 슬래시 날짜 (2026-09-23, 커밋·미배포)

- 실제 문장 훑기에서 `9/25(목)까지` → `이십오 분의 구`(분수 오독)를 발견했다.
- 변경: `src/semantic/classifier.c`의 `slash_date`(연도 우선, 일>12, 괄호 요일 단서),
  ko `weekday_label` 리소스, validator. 영어도 `9/25` → `September twenty fifth`가 된다.
- 계약 `tests/evaluation/slash_date_v21.jsonl` 16건. cycle fixed 9 / unchanged-pass 7 /
  regressed 0. Debug 110/110, ASan/UBSan 98/98(단독), 외부 영어 결과 파일 동일.
  슬래시 약어 corpus의 `2026/09/23` 부정 예를 이 변경에 맞춰 갱신했다.

### 붙여 쓴 범위 표시 (2026-09-23, 커밋·미배포)

- 변경: `src/core/speech_output.c`의 `ttf_speech_range_mark`, `src/core/normalize.c` TN 단계 적용,
  ko/en `speech_range_mark` 리소스, validator. 반복 마크·공백·고객이 끈 구분자는 제외한다.
- 계약 `tests/evaluation/range_mark_v21.jsonl` 15건. cycle fixed 8 / unchanged-pass 7 /
  regressed 0. Debug 111/111, ASan/UBSan 99/99(단독), 외부 영어 결과 파일 동일.
- `1~2`·`1%~2%`의 기존 고객 계약은 `quantity_range_separator ~ 0`을 존중해 그대로 유지된다.

### 여러 토큰 구간 (2026-09-23, 커밋·미배포)

- 변경: `src/semantic/classifier.c`의 `same_unit_family`/`preceding_family_unit`,
  ko `range_unit_family` 리소스, validator. 앞 단어가 같은 계열 단위일 때만 각 끝을 읽는다.
- 계약 `tests/evaluation/multi_range_v21.jsonl` 11건. cycle fixed 4 / unchanged-pass 7 /
  regressed 0. Debug 112/112, ASan/UBSan 100/100(단독), 외부 영어 결과 파일 동일.
  기존 `1시~2분` 보존 계약은 앞 단어 근거 요구로 유지된다.

### 아이디·핸들 (2026-09-23, 커밋·미배포)

- 변경: `src/semantic/classifier.c`의 `@handle` 분류(글자/밑줄 시작, 밑줄 필요),
  `src/core/normalize.c`에서 주소 렌더러 사용·text 보존·문맥 중립.
- 계약 `tests/evaluation/handle_v21.jsonl` 11건. cycle fixed 8 / unchanged-pass 3 /
  regressed 0. Debug 113/113, ASan/UBSan 101/101(단독), 외부 영어 결과 파일 동일.
  숫자로 시작하는 `2millionUSD_foo`의 기존 보존 계약은 글자 시작 조건으로 유지된다.

### 영어 연도 문맥 (2026-09-23, 커밋·미배포)

- 프로젝트의 측정 데이터(외부 영어 표본)에서 가장 큰 실제 오류가 연도 읽기였다
  (`1965` → `one thousand nine hundred sixty five`).
- 변경: `date_year_cue`/`boundary_word`에 전치사·구간어 15개 추가(리소스만).
  DATE exact 319 → 343, 회귀 0. 계약 13건, Debug 114/114, ASan/UBSan 102/102(단독).
- 흔한 기능어(the/a/and/to/by/on)를 넣으면 DATE 401까지 오르지만 boundary_word 등록이
  chunk 크기에 따른 span 불일치를 만든다. remaining-work에 측정치와 함께 기록했다.

### planner의 연도 단서 처리와 구간 연속 (2026-09-23, 커밋·미배포)

- 앞선 "막힌 지점"을 해결했다. `src/segmenter/segmenter.c`가 `date_year_cue`를 floor cue처럼
  직접 읽는다. en validator는 한 단어 연도 단서에 한해 `boundary_word` 요구를 해제하고,
  다른 단서와 여러 단어 단서에는 그대로 요구한다.
- `languages/en-US/date.c`에 `interval_continuation`을 추가했다. `from X to Y`/`between X and Y`는
  **앞 끝이 단서로 확인된 연도**일 때만 뒤 끝을 연도로 본다.
- 결과: DATE exact 343 → 394(기준 319 대비 +75), CARDINAL 변화 없음, chunk 불일치 0.
  Debug 115/115, ASan/UBSan 103/103(단독). 계약 14건.
- v0.18 호환 참조(`context_cache_v18_reference.json`, `..._partial_...`) 4+1건의 `context_trace`만
  갱신하고 사유를 파일 purpose에 적었다. 텍스트·offset·의미·render 언어는 동일하다.

### 사용자 옵션 1: 괄호 안 처리 (2026-09-23, 커밋·미배포)

- 사용자가 요청한 옵션 네 가지(괄호 처리, 미등록 외국어 단어, 장식 문자, 대문자 약어) 중 첫 번째.
- 공개 구조체 `ttf_frontend_options_t`에 필드를 더하면 예전 헤더로 빌드한 소비자가 깨진다
  (`ttf_frontend_options_init`이 더 큰 구조체를 기존 버퍼에 쓴다; 실제로 native 테스트가 SEGFAULT).
  그래서 **새 함수** `ttf_engine_set_option(engine, name, value)`로 전달한다.
- 변경: `src/core/frontend_options.c`, `src/core/internal.h`(엔진 내부 필드),
  `src/core/normalize.c`(괄호 구간 처리), CLI `--brackets`, 웹 선택기, `bracket_pair` 리소스.
- 계약 `tests/evaluation/bracket_v21.jsonl` 11건. Debug 116/116, ASan/UBSan 104/104(단독),
  외부 영어 결과 파일 동일, browser gate 통과.

### 사용자 옵션 2: 미등록 외국어 단어 (2026-09-23, 커밋·미배포)

- `ttf_engine_set_option(engine, "unknown_foreign", "guess|spell|preserve")`, CLI `--unknown-foreign`,
  웹 선택기. 기본 `guess`는 기존 동작이다.
- 변경: `src/pronunciation/foreign.c`(`declared_pronunciation` 확인 후 정책 적용),
  `src/core/normalize.c`(보존된 단어는 최종 문자 패스를 건너뜀), 옵션 배선.
- 사전·발음 사전에 있는 단어(`Apple`, `Google`, `Hello`), 이니셜(`R&D`), 라벨(`k7`), 주소·식별자는
  정책과 무관하게 기존 계약을 유지한다.
- 계약 `tests/evaluation/unknown_foreign_v21.jsonl` 15건. Debug 117/117, ASan/UBSan 104/104(단독),
  외부 영어 결과 파일 동일, browser gate 통과. 기본 정책 사례는 cycle에서 회귀 0.

### 사용자 옵션 3: 장식 문자 이름 읽기 (2026-09-23, 커밋·미배포)

- `ttf_engine_set_option(engine, "decorative", "follow|read")`, CLI `--decorative`, 웹 선택기.
- 변경: `src/core/speech_output.c`(선언된 이름 읽기), `src/core/normalize.c`(이모지 cluster 경로),
  `speech_decorative` 리소스 ko/en 21개, validator.
- 계약 `tests/evaluation/decorative_v21.jsonl` 13건. Debug 117/117, ASan/UBSan 105/105(단독),
  외부 영어 결과 파일 동일.

### 사용자 옵션 4: 대문자 약어 읽기 방식 (2026-09-23, 커밋·미배포)

- `ttf_engine_set_option(engine, "initialism", "spell|word")`, CLI `--initialism`, 웹 선택기.
- 변경: `languages/en-US/pack.c`의 lexical 분류에서 전부 대문자 2글자 이상이면 약어로 본다.
  선언된 ACRONYM/PROPER_NOUN/INITIALISM과 사전 항목은 제외한다(NASA → 나사 유지).
- 계약 `tests/evaluation/initialism_v21.jsonl` 15건. Debug 118/118, ASan/UBSan 106/106(단독),
  외부 영어 출력 동일(IR 10건만 INITIALISM). 사용자 옵션 네 가지를 모두 마쳤다.

### 대문자 코드 단어 (2026-09-24, 커밋·미배포)

- 사용자 결정: `SAVE20` = `세이브 이십`.
- 변경: `src/pronunciation/foreign.c`의 ALNUM 경로에 `declared_word` 판정 추가. 사전 근거가 있으면
  코드 단어로 보고 대상 언어 수사와 띄어쓰기를 쓴다. 사전 7항목(save/sale/best/event/summer/winter)
  추가, `coupon`은 기존 금융 항목과 충돌해 제외했다.
- 사용자 결정(2026-09-24, 2차): 글자 라벨 뒤 **한 자리** 숫자도 원어 수사다 (`k7` → `케이세븐`,
  `A4` → `에이포`, `B2` → `비투`). 두 자리 이상은 대상 언어 수사(`G80` → `지팔십`), 선행 0은
  자릿수 유지(`k007`). 기존 계약 10건(v0.20 `k7`/`K7` 포함)을 같은 사유로 갱신했다.
- 사용자 결정(2026-09-24): 사전 단어 라벨은 **원어 수사**를 띄어 읽는다 (`top10` → `탑 텐`,
  `best5` → `베스트 파이브`, `SAVE20` → `세이브 트웬티`). `top100`은 관용 표기라 전체를 사전에
  등록해 `탑백`으로 둔다. 철자 라벨(`k7`, `MP3`, `GPT-4`)은 대상 언어 수사를 붙여 읽는다.
  v0.20 계약 2건(`top100`, `top10`)을 같은 사유로 갱신했다.
- 계약 `tests/evaluation/code_word_v21.jsonl` 10건. cycle fixed 3 / regressed 0.
  Debug 119/119, ASan/UBSan 107/107(단독), 외부 영어 결과 파일 동일.

### 미등록 단어 추측 발음 (2026-09-24, 커밋·미배포)

- 측정 우선. 사전에 없는 28단어의 기대 표기를 독립 작성하고 0/28을 기록한 뒤 고쳤다.
- 변경: `languages/en-US/pack.c`(g2p 앵커 `^`/`$`, 묵음 `-`), `graphemes.tsv` 규칙 추가,
  `languages/ko-KR/pack.c`(모음 뒤 `r` 삭제)와 `phone_mapping coda_r` 리소스, validator.
- 결과 0/28 → 14/28. 남은 14건과 원인은 `docs/research/oov-guess-v21-results.md`.
  사전 추가로 통과시키지 않았다. Debug 121/121, 외부 영어 결과 파일 동일.
- 기존 corpus 5건의 추측 발음 기대값(`레포르트`→`리포트`, `델이`→`데이리` 등)을 갱신했다.

### 미등록 단어 추측 발음 2차 (2026-09-24, 커밋·미배포)

- 남은 14건을 표기 규칙 세 가지로 처리해 **28/28**이 됐다. 규칙은 모두 리소스로 선언한다.
  - 어말 `P T K`는 짧은 모음 뒤에서만 받침이 된다(`phone_vowel_length`, market 마켓 /
    update 업데이트). 비음·유음 앞에서도 닫지 않는다(partner 파트너).
  - 모음 뒤 `l`은 겹쳐 적는다(`phone_mapping onset_l double`, platform 플랫폼).
  - `ER` 뒤 모음이 오면 r가 다음 음절 초성이 된다(`phone_mapping linking_r link`, 배터리).
  - 구개음 뒤 삽입 모음은 `이`, `SH`는 뒤 모음을 구개음화, 이중모음 꼬리가 받침을 가져간다.
- 변경: `languages/ko-KR/pack.c`, `validate.c`, `phones.tsv`, `graphemes.tsv`
  (`^con`, `nnel`, `^lice`, `nse$`, `ersi`, `on$`, `ake$`), 회귀 게이트 14 → 28건.
- 기존 참조 3건(`snake` 스내크→스네이크, `daily` 데이리→데일리, `IT` word 정책 이트→잇)의
  기대값을 갱신하고 사유를 각 notes에 남겼다. 프로토타입 출력을 적어 둔 값이었다.
- 결과: Debug CTest 122/122, 외부 영어 표본 수치 동일(`sproat-v21-oovguess`, DATE 394).
  자세한 기록은 `docs/research/oov-guess-v21-results.md`.

### 미등록 단어 추측 발음 3차 — 세트 확대와 과적합 점검 (2026-09-24, 커밋·미배포)

- 28단어 세트가 2~3음절 명사에 치우쳐 있어, 자음군·4음절 이상·파생형 30단어를 규칙 변경
  **전에** 작성했다(기준선 11/30 → 30/30).
- 이어서 새 규칙의 **이웃 단어** 18개를 다시 규칙 변경 전에 작성했다(기준선 9/18 → 17/18).
  이 세트가 `cate`(cater 카테), `^pers`(person 퍼스언), `nati`(nation 내션) 같은 한 단어짜리
  규칙을 잡아냈고, 공유 부분(`ateg`, `^per`+`son$`, `atio`)으로 바꿨다.
- 매퍼: 지운 `r`의 모음 길이, 어말 성절 자음, 활음 앞 자음, 구개음 뒤 `ㅖ`, `W`의 `ㅞ`/`ㅝ`.
- g2p 데이터에 형태소 경계 token `.`을 추가했다(`^up` → `AH P .`, uploading 업로딩).
  음운 규칙으로 일반화하면 application 애플리케이션이 깨지므로 데이터로 남겼다.
- 남은 실패 1건 national(네이셔널)은 강세 모음 축소 문제라 사전에 넣지 않고 기록했다.
- 게이트 75건(28+30+17), 측정 76건. Debug CTest 125/125, 외부 영어 수치 동일.

### 미등록 단어 추측 발음 4·5차 (2026-09-25, 커밋·미배포)

- 4차 27단어(동사 활용, 자음 뒤 `w`, 모음 사이 유성 `s`, 고유명사, 6음절 이상):
  기준선 5/27 → 25/27. 5차 24단어(4차 규칙의 이웃): 15/24 → 24/24.
- 새 자원 `phone_glide`(`merge|split`)로 자음 뒤 `w`를 끊는다(스위트·트윈, 퀵은 유지).
  `Y`는 항상 붙으므로 이 표를 보지 않는다 — 처음에 `Y`까지 끊었다가 리뷰가 리브유가 됐다.
- 자소 기본값 `o` → `OW`, 모음 사이 `s` → `Z`(자소 규칙).
- **자소 key 상한을 4자에서 5자로 올렸다**(`languages/en-US/pack.c` matcher와 validator,
  `docs/resource-validation.md`). 접미사를 한 행에 적을 수 있고 과적합도 줄어든다
  (`^chica`는 chicken을 건드리지 않지만 `^chic`였다면 시킨이 됐다).
- 측정 127단어 / 정확 124 / 게이트 124건. 남은 3건은 national(강세 모음 축소),
  michael(고유명사는 사전 대상), visit(기대값 다툼)이며 사전에 넣지 않고 기록했다.
- Debug CTest 126/126. 자세한 기록은 `docs/research/oov-guess-v21-results.md`.

### DATE 진단 지표와 괄호 연도 단서 (2026-09-25, 커밋·미배포)

- 평가기에 `mismatch_kinds`와 `diagnostic_case_insensitive`를 넣었다. 정확도는 그대로
  byte 비교이고 이 둘은 진단이다. DATE 불일치 1,144건 중 698건이 월 이름 대소문자 관행임이
  드러났고, 프로젝트는 입력 대소문자를 계속 보존한다.
- 실제 오류 344건의 갈래: 맨 연도 258, ISO 어순 46, 연대 표기 33, 기타 7.
  맨 연도 중 108건이 `( 1965 )` 인용 형태였다.
- `date_year_cue`가 여는 괄호도 key로 받는다. 단 **괄호가 숫자만 감싸고 바로 닫힐 때만**
  단서다(`( 2003 and 2005 )`는 그대로 cardinal). `born`도 추가했다. 계약 10건을 구현 전에
  작성했다(`year_cue_v21.jsonl` 14 → 24건).
- 기존 phone-negative 계약 2건(`( 2024 )`, `1385 ( 2006 )`)의 기대를 CARDINAL → DATE로
  바꿨다. **정책 변경**이며 근거와 되돌릴 조건을 `docs/research/date-diagnostic-v21-results.md`에
  남겼다.
- 외부 영어: DATE 394 → 504, span 35,053 → 35,163, 다른 클래스 동일. Debug 126/126.

### 영어 연대 표기 (2026-09-25, 커밋·미배포)

- `1960s`는 미정규화 상태였다. `en.date.decade` 분류 + `decade_plural` 자원 + 
  `classifier_priority en.date.decade 10`으로 처리한다. 계약 13건을 구현 전에 작성했다
  (`tests/evaluation/decade_v21.jsonl`).
- 0으로 끝나는 네 자리/두 자리 + `s`만 잡으므로 `1965s`·`787s`·`MP3s`는 그대로다.
- 외부 영어: DATE 504 → 532, span 35,163 → 35,191, 다른 클래스 동일. Debug 127/127.

### speech 모드 측정과 영어 이니셜 읽기 (2026-09-25, 커밋·미배포)

- 평가기에 `--output-mode speech`를 넣었다. 리포트는 `provenance.frontend_options`와
  `metric_policy`(`...-speech`)로 자기 정체를 밝히고, text baseline과 섞이지 않는다.
  speech baseline은 span 35,236이다(text 35,191과 비교할 수치가 아니다).
- LETTERS 538건 실패가 전부 `unchanged`였다. speech 모드에서 `FBI`를 그대로 내보내고 있었다.
- 전부 철자로 읽으면 PLAIN이 124건 퇴행한다(`EXCEL` → `e x c e l`). 판별식 5개를 결과
  파일로 오프라인 평가해 "2자 이하 또는 발음 불가"(+297/−36)를 골랐다. 음소배열 표
  `syllable_onset`/`syllable_coda`는 자원이다.
- `initialism_policy`에 `inferred_speech`(letters), `pronounceable_speech`(preserve) 추가.
  글자만이 아닌 값(`R&D`)은 제외 — 이 규칙이 없으면 `R and D`가 `r d`가 된다(기존 계약이 잡음).
- speech span 35,236 → 35,501, LETTERS 174 → 471, PLAIN −33. **text 모드는 35,191로 동일.**
  Debug 128/128. 기록은 `docs/research/initialism-speech-v21-results.md`.

### 한국어 대문자 약어 (2026-09-25, 커밋·미배포)

- `FIFA` → 에프아이에프에이처럼 단어로 읽어야 할 약어를 글자로 읽고 있었다. 계약 20건을
  먼저 작성했다(`tests/evaluation/ko_acronym_v21.jsonl`, 기준선 13/20).
- 영어 speech에서 쓴 음소배열 판별을 한국어 분류에 올리는 안을 이 세트로 먼저 채점했다:
  18/20이지만 틀리는 `CEO`·`LED`의 손해가 크다(영어는 보존일 뿐이지만 한국어는 `시오` 같은
  오독이 된다). 그래서 **분류를 건드리지 않았다**.
- 런타임은 이미 사전이 글자 읽기를 덮어쓴다(`RAM` → 램). 빠져 있던 것은 항목이었다.
  `lexicons/ko-KR/acronym-additions.jsonl`에 7건(피파·유네스코·나토·오펙·우에파·엑셀·심)을
  넣고 `tools/lexicon/manage.py build`로 다시 빌드했다. 출처는 `project-authored-v21-acronyms`,
  `review_status`는 pending이다. 20/20. 기록은 `docs/research/ko-acronym-v21-results.md`.

### 마침표 없는 약어 (2026-09-25, 커밋·미배포)

- PLAIN 실패 101건을 갈래로 나누니 약 40건이 마침표 없는 약어(`vol 1`, `Records ltd`)였고
  나머지는 영국식→미국식 철자 변환(데이터셋 관행, 따르지 않는다)이었다.
- 후보 key마다 표본의 참조 읽기를 세어 **한 가지로만 읽히는 것**만 declare했다:
  vol·ltd·vs·etc·jr·mr. `st`(saint 8 / street 1), `dr`(doctor 3 / drive 1), `sr`, `no`, `pp`는
  제외했다. 새 자원은 `abbreviation_bare`이고 `preserve`로 끌 수 있다.
- 계약 13건을 구현 전에 작성했다(`tests/evaluation/bare_abbreviation_v21.jsonl`).
- 외부 영어 text: PLAIN 33,860 → 33,881, span 35,191 → 35,212. Debug 130/130.

### per 단위와 누락 단위 (2026-09-25, 커밋·미배포)

- 작은 클래스의 실패를 훑다가 `193.5/km²`가 **기호 그대로** 나가는 것을 발견했다(7건).
  `20 degrees C`, `-2011 kl`도 미정규화였다.
- 원인: `/`로 시작하는 토큰은 경로 보호가 가져가고, 붙여 쓴 형태는 분수 분기가
  `syntax.invalid-fraction`으로 보존했다. **분모가 숫자가 아닐 때만** 단위 접미사를 시도하게
  좁혔다(`5/6`은 그대로 분수, 띄어 쓴 `53.5 /km²`는 그대로 경로 보호).
- 자원만으로 표현: `numeric_suffix * /km²`, `kl`, `degrees C`, `degrees F` + `unit` label.
  표본에 없는 `/kg` 같은 형태는 선언하지 않았다.
- 계약 9건을 구현 전에 작성했다. 외부 영어: MEASURE 76 → 83, span 35,212 → 35,219.
  Debug 131/131.

### 단독 이니셜 (2026-09-25, 커밋·미배포)

- 로마 숫자 후보를 조사하다 같은 목록에서 이니셜 마침표가 더 큰 덩어리임을 발견했다.
  `U.S. Army` → `u s Army`는 되는데 `Cossette , M. , Lecomte`는 그대로였다. 단독 이니셜은
  **이름이 뒤따를 때만** 인식했기 때문이다.
- 표본의 85건을 뒤따르는 문자로 세었다: `(` 35, `;` 16, `,` 14, `"` 5, `&` 3. 문장을 잇는
  문자 뒤에서만 이니셜로 보고 자기 마침표를 지운다. 새 자원 `initial_continuation`.
- 문장 끝 마침표는 그대로 둔다 — 지우면 문장 경계가 같이 사라진다. 닫는 따옴표 앞도
  기존 판단(`terminal_period`)대로 남긴다(`Smith , b. " title "`).
- 계약 9건을 구현 전에 작성했다. 외부 영어 text: LETTERS 174 → 242, span 35,219 → 35,287.
  Debug 132/132.

### 한국어 불변식 스윕 (2026-09-25, 커밋·미배포)

- 한국어는 외부 참조가 없어 정답 대조 대신 **불변식**을 걸었다: speech 모드 한국어 출력에
  라틴 문자가 남으면 안 된다. 조합 1,080건 중 위반 1건(`제 10kg`)이었고, 따라가니 가족이 셋이었다.
- `제`는 서수 접두사이자 보통 낱말이다. 서수 단위를 만나지 못한 후보가 숫자·단위까지 보존해
  원문을 그대로 내보내고 있었다. 단위를 만난 후보만 보존하도록 고쳤다(`0번째` 등은 그대로).
- 단위 약어 단독이 추측 발음이 됐다(`kg` → 크그). 사전 10건을 관리 파이프라인으로 추가했다
  (`lexicons/ko-KR/unit-additions.jsonl`, 출처 `project-authored-v21-units`, pending).
- 소문자 `ml`이 선언되어 있지 않았다. 기존 `mL` 단위 id로 매핑했다.
- 스윕을 `tests/integration/test_ko_speakable.py` 게이트로 만들고 입력을 1,770건으로 넓히자
  같은 성격의 결함이 더 나왔다: 전화 단서 뒤의 짧은 숫자·범위·수식(`문의 12`, `문의 3-5`,
  `문의 1+1`)과 `제` 접두사가 삼킨 범위·소수(`제 3-5`, `제 0.5`).
- 면제 방식이 중요했다. 처음엔 규칙 이름(`*invalid*`)으로 면제했는데 그러면 방금 고친
  `제 10kg`도 통과한다. **문서화된 보존 입력만** 이름으로 면제하도록 바꿨다.
- 프로즌 계약 `제0`이 경계를 잡아 줬다: 붙여 쓴 `제`는 서수 접두사, 띄어 쓴 `제`는 보통 낱말.
- 계약 16 + 10건, 스윕 위반 0. Debug 135/135.
  기록은 `docs/research/ko-unit-word-v21-results.md`.

### 숫자+기호 잔여물 (2026-09-25, 커밋·미배포)

- 같은 속성 검사를 영어 speech에도 돌리고, 숫자와 기호 조합을 양쪽 언어로 훑었다.
  `20°`, `5m²`, `5㎡`, `5㎢`, `5kcal`, `5㎉`, `5‰`가 기호를 그대로 내보내고 있었다.
- 자원만으로 처리했다(`numeric_suffix` + `unit`). 눈금이 붙은 `°C`/`°F`는 가장 긴 접미사가
  먼저 맞으므로 그대로 이긴다.
- 모호한 것은 손대지 않고 계약의 음성 사례로 남겼다: `′`/`″`(피트·인치 대 분·초),
  `¢`(보조 통화 단위), 위첨자 `²`(각주 표시일 수 있음).
- 계약 16건을 구현 전에 작성했다. Debug 136/136.

### 국가 코드 도메인 (2026-09-26, 커밋·미배포)

- speech 기준선을 다시 재니 ELECTRONIC이 0/22였다. 대부분은 데이터셋 표기법(`b_letter`) 차이라
  맞출 수 없지만, `Vecernji.hr`·`Amazon.co.uk`는 **인식 자체가 안 돼** 점이 그대로 나갔다.
- ISO 3166 코드 전체를 overlay로 씌워 외부 표본을 돌렸더니 수치가 그대로였다(회귀 0). 그런데
  표본이 못 보는 실패가 있다: `See fig.no 3`이 도메인이 된다.
- 그래서 **영어 낱말과 겹치는 코드는 빼고** 200개만 넣었다. 계약 9건(음성 사례 3건 포함)을
  구현 전에 작성했다. Debug 138/138.

### 콜론 앞 숫자와 연표 연도 (2026-09-26, 커밋·미배포)

- 맨 연도 148건 중 안전한 부분집합을 찾다가 콜론 형태(7건)를 골랐고, 구현하다 **더 큰 것**을
  발견했다: `Chapter 12: Introduction`의 숫자가 아예 안 읽히고 있었다. 콜론이 뒤에 숫자가 없어도
  구조로 취급돼 `syntax.unsupported-numeric`으로 보존됐기 때문이다.
- 콜론은 **뒤에 숫자가 있을 때만** 구조다(`3:4`, `09:30` 그대로). 숫자 없는 콜론은 시간 후보도
  만들지 않는다(`1912:`가 invalid-time으로 보존되던 것도 해소).
- 연표 형태는 새 자원 `date_year_cue_after`로 읽는다. 연도 범위·수량 명사 규칙은 그대로다.
- 계약 7건을 추가로 작성했다(음성 사례 4건 포함, 총 31건). 외부 영어 DATE 532 → 538,
  span 35,287 → 35,293. Debug 138/138.

### 인용 형태 연도 (2026-09-26, 커밋·미배포)

- 쉼표와 문장 끝 사이의 4자리 숫자는 표본에서 **전부 연도**였다(25건, 반례 0). 다만 표본이
  인용 중심이라 규칙을 좁게 뒀다: 앞뒤 표시가 **둘 다** 맞아야 한다.
- 괄호 전용이던 코드를 `date_year_enclosure` 자원으로 일반화했다. 여는 표시가 자기 닫는 표시
  집합을 선언하고 `$`는 문장 끝을 뜻한다. 쉼표는 `.;$`를 닫는 표시로 갖는다.
- planner도 같은 자원을 읽어야 배치와 스트리밍이 갈리지 않는다 — 옮기자마자 기존 계약
  `( 2024 )`가 스트리밍에서 어긋나며 이 점을 잡아냈다.
- 계약 7건 추가(음성 사례 3건, 총 38건). DATE 538 → 558, span 35,293 → 35,313. Debug 138/138.

### 하이픈 연도 범위 (2026-09-26, 커밋·미배포)

- `merged-agreement` 진단으로 남은 alignment 35건을 열어 보다 발견했다: `from 1965 to 1970`은
  두 끝점을 연도로 읽는데 `the 1988 - 1993 period`는 기수로 읽고 있었다.
- 단서 규칙은 그대로다(첫 끝점이 단서로 확인된 연도여야 한다). 그래서 단서 없는 쌍과
  `p . 57 - 58`은 기수를 유지하고 ISO 날짜도 그대로다.
- `number_at`이 붙은 하이픈을 malformed 필드로 보기 때문에 첫 끝점만 직접 읽는다.
  읽기는 `unit=year`를 단 RANGE 한 개이고 verbalizer가 끝점별로 읽는다.
- 계약 7건 추가(음성 사례 3건, 총 45건). 정확 수치는 35,313 그대로지만 진단이 개선을 보여 준다:
  merged-agreement DATE 0 → 5, PLAIN 26 → 29, alignment DATE 27 → 22. Debug 138/138.

### 한국어 고유어 셈단위 19개 (2026-09-26, 커밋·미배포)

- 작업 큐 2순위(KO-REAL-INPUT)의 취지대로 실제 한국어 상거래 문구를 읽어 보다 발견했다.
  `최대 5회 → 오회`와 `재고 3개 → 세 개`의 띄어쓰기 차이를 따라가니 원인은 **미등록 셈단위**였다:
  선언이 없으면 한자어 수사로 떨어져 `2쌍`이 `이쌍`이 된다.
- 고유어 수사를 쓰는 19개(쌍 통 판 척 알 조각 묶음 상자 봉지 갑 채 그루 그릇 접시 군데 가지
  줄 칸 끼)를 선언했다. 한자어 수사 셈단위(회 배 박 층 호 인분)는 그대로다.
- 셈단위가 더 긴 낱말의 첫 음절이면 기존과 같은 방식으로 잡힌다(`2장미` → `두 장미`).
  새 유형의 오류는 아니지만 노출이 넓어지므로 `docs/ko-coverage.md`에 적었다.
- 계약 29건을 구현 전에 작성했다(한자어 유지·조사·범위·20/100 경계 포함). Debug 140/140.
- **2차 25개 (같은 날):** 처방·리뷰·공공 문구를 더 읽어 `1정 → 일정`(한 정), `8자 → 팔자`
  (여덟 자)를 찾았다. 정 자 곡 편 컷 방 캡슐 포 첩 술 모금 발 자리 바퀴 걸음 쪽 캔 팩 세트
  다발 근 관 짝 사발 공기를 선언했다. 계약 32건. 선언 수는 26개 → 70개가 됐다.
  접두 충돌은 기존과 같은 양상이고 일부는 더 자연스러워진다(`5정도` → 다섯 정도).

### 한국어 식별 번호 라벨 (2026-09-26, 커밋·미배포)

- 실제 고지·주문 문구를 계속 읽다가 발견했다. `우편번호 06236`이 육천이백삼십육(선행 0 소실),
  `주문번호 20260926-0001`이 범위, `송장번호 123456789012`가 거대 기수로 읽혔다.
- 메커니즘(`ko:labeled-identifier`)은 이미 있었고 **라벨 자원이 금융 14행뿐**이었다. 66행을
  더했다(우편·주문·송장·운송장·회원·사업자·예약·인증·승인·카드·등록·문서·고객·접수, 학번·사번·군번).
- 식별 숫자의 0은 `공`이다(전화·계좌와 같은 정책). 작성한 기대값이 `영`이어서 엔진 쪽으로 고쳤다.
- 프로즌 계약 `접수 번호는 123-456` 충돌: 그 계약은 범위 읽기(백이십삼에서 사백오십육)를 담고
  있었다. 접수번호에는 틀린 읽기라 근거와 함께 갱신했고, 고객 범위 구분자는 `range-customer`
  케이스가 계속 검증한다.
- 계약 12건을 구현 전에 작성했다(라벨 없는 금액·연도·셈단위 음성 사례 포함). Debug 141/141.

### 단위 약어 일괄 훑기 (2026-09-26, 커밋·미배포)

- 셈단위에서 통한 "표를 한 번에 훑는" 방식을 단위에도 적용했다. 77개 중 **54개가 미정규화**였다.
- 모호하지 않은 33개를 선언했다(ms sec min hr kHz GW Wh mAh kV mA PB dpi px pt cal kJ Pa hPa
  kPa atm psi mol lux dB rpm Ω inch yd mile lb oz gal ha).
- **한 글자 약어(A V B t s l)는 일부러 제외했다**: `5A`는 `20F`·`4K`와 같은 모호 식별자
  형태다. bps 계열도 한국어 읽기가 정착되지 않아 제외했다. 둘 다 음성 사례로 고정했다.
- 계약 43건을 구현 전에 작성했다.

### 통화 일괄 훑기 (2026-09-26, 커밋·미배포)

- 기호 23개 중 17개, ISO 코드 30개 중 21개가 미정규화였다. 한국어 문장에서 미선언 코드는
  금액까지 영어로 끌고 갔다(`5 CNY` → five c n y).
- 24개 통화를 코드·기호·영/한 라벨까지 선언했다. `¥`는 JPY(그 기호의 관행), 위안은 `CN¥`.
  `¢`는 통화가 아니라 보조 단위라 제외했다. 새 통화에는 보조 단위를 선언하지 않아 소수는
  주 단위로 읽는다(고객 overlay로 추가 가능).
- 문자가 섞인 기호(`HK$`·`NZ$`)는 단독일 때 영어 문맥을 고르는 기존 동작이라 계약을 한국어
  문장 안에 넣었다(`가격은 오 홍콩 달러입니다`).
- 계약 45건을 구현 전에 작성했다. Debug 145/145.

### 기호 일괄 훑기 (2026-09-27, 커밋·미배포)

- 기호 96개 중 76개가 원문 그대로였는데 **대부분은 의도된 보존**이었다: 구두점은 운율용
  `speech_preserve`, 장식 기호는 `--decorative read`로 이름이 붙는다(`★` → 별).
- 진짜 공백 셋: ① 그리스 문자 24개(장식도 구두점도 아니고 이름이 확립됨) ② `∞ √ § №`
  ③ `≠ ≈ ≡`가 `math_relation`에서 누락(`≤`·`≥`는 동작).
- ③은 네 곳을 함께 고쳐야 했다: `src/semantic/classifier.c`의 연산자 표, ko/en pack의
  관계 목록, 그리고 **양쪽 validator**(새 key는 validator를 같은 커밋에 — 메모리 규칙).
- 계약 39건을 구현 전에 작성했다(장식·구두점 불변 음성 사례 포함). Debug 146/146.

### 긴 선행 0 숫자 (2026-09-27, 커밋·미배포)

- 외부 표본의 TELEPHONE/DIGIT 실패를 열어 보니 `0774858583`이 "칠억칠천사백…"으로 읽히고 있었다.
- 처음엔 "선행 0이면 자리열"로 넓게 바꿨는데 프로즌 계약 7건이 걸렸고, 그중 둘은 프로젝트가
  **의도적으로** 정한 위치였다: `advanced-bare-identifier-ambiguous`(id가 "모호"라고 말한다)와
  golden `00010 → 십`. 그래서 뒤집지 않고 **7자리 이상**으로 좁히고 7건을 되돌렸다.
- 자리 구분이 있는 수(`0,123,456`)는 서식 잔재이므로 제외한다(원문이 순수 숫자 열일 때만).
- 짧은 열에 대한 불일치(외부 표본은 `09`도 자리별)와 `공`/`영` 0 읽기 선택은 remaining-work의
  **NUM-ZERO**에 근거와 닫는 증거까지 적었다.
- 구현 중 라벨 경로가 덮이는 문제도 고쳤다: 선택 로직에 DIGIT_SEQUENCE를 더해
  `계좌는 0012345` → `공공일이삼사오`가 유지된다. 계약 15건. Debug 148/148.

이후 후보: 남은 맨 연도 약 120건(문맥 이해 필요), 로마 숫자(이득 8건 남짓),
한국어 외부 참조 확보(EVAL-03), 코드 단어 사전 검수. ISO 어순 46건과 대소문자 698건은 관행
차이로 남긴다.

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

## 배포 상태 (2026-09-26)

`build-web` library와 저장소 `resources`를 각 커밋마다 다시 빌드하고 `ttf-workbench.service`를
재시작했다. `/api/health`는 `{"ok": true, "version": "0.20.0"}`이고 **CMake release 번호는 아직
0.20.0이다**. Unreleased 항목이 v0.20 이후로 상당히 쌓였으므로 번호 상향을 검토할 시점이지만,
릴리스 번호는 여전히 별도 결정이다.

배포마다 서비스 API에서 그 변경의 대표 출력을 확인했다. 최근 것만 적으면
`the f b i and NASA and R and D at UNESCO`(영어 이니셜 speech),
`피파 / 씨이오`(한국어 약어), `킬로그램 단위 / 제 십 킬로그램 / 문의 십이`(말할 수 있음 스윕),
`이십 도 / 오 제곱미터 / 오 퍼밀`(단위 기호),
`20F 안내`가 정책별로 `20F 안내`(preserve) · `20F 안내`(drop) · `이영에프 안내`(read).

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
