# v0.19: 긴 단위의 문맥 근거 재사용

## 문제와 변경

v0.18의 큰 단위에서 `context_score`가 각 token마다 같은 Latin/digit run과
Hangul scope를 다시 순회했다. 숫자 3,000개(5,999 token)의 profile에서
KO/EN 각각의 `context_run_member`가 35,988,001번 호출됐다. 입력 10배에
같은 탐색 횟수가 약 100배로 증가하는 경로였다.

v0.19는 `src/language/context_memo.*`에 요청 단위의 내부 memo를 두고
공통 run 경계, 한국어 scope/조사 점수, 영어 run의 어휘 근거를 재사용한다.
Core는 언어별 점수를 계산하지 않으며 기존 pack이 정책과 resource lookup을
계속 소유한다. 영어 어휘 처리 순서, 점수 상한, 조사 범위와 인용 범위를 유지했다.

- Key: evidence identity + text pointer/length + token view/count. 현재 token index는
  key에서 제외한다. Factory는 해당 view 전체의 근거를 생성한다.
- 부분 semantic 소비로 `token.begin`이 바뀌면 다음 판단 전에 memo를 전부
  무효화한다. 동일 배열 주소만으로 변경 전 근거를 재사용하지 않는다.
- 두 번 무효화된 단위의 나머지는 기존 비캐시 정책으로 전환한다. 최초
  후보의 반복 counter 입력에서 발견한 재구축 비용 증가를 막는 보호다.
  한 번만 부분 소비하는 흔한 입력은 새 근거를 한 번 재생성할 수 있다.
- 중첩 normalize/stream은 독립 상태를 사용하며 공유 frozen engine을 수정하지 않는다.
- 성공·실패 경로에서 해제한다. Factory 실패와 재귀 오류는 요청 오류로 전파하고,
  불완전하게 판단한 unit의 callback을 보내지 않는다.
- Public ABI v1 layout/signature 변경은 없다. 일반 `context_score`에도 borrowed
  request view가 전달될 수 있으므로 [포인터 수명 계약](../language-pack.md#callback-engine-ownership)을 따른다.

## 정확성 근거의 구분

첫 입력 30건과 기대 텍스트는 최초 candidate 실행 전에 작성했다. 인용, 혼합언어,
부분 counter 소비, 어휘 순서, whitespace, resource override를 포함한다.
반복 재구축 문제를 발견한 뒤 4건과 긴 counter 패턴을 별도로 작성하고,
수정 후보를 실행하기 전에 v0.18 기준을 고정했다. 기존 30건은 바꾸지 않았다.
Metadata fixture는 고정 v0.18 commit
`f00948be1657413f58ecfa76214bdc96bfaca35c`에서 얻은 **호환성 기준**이다.
그 버전과 같다는 사실을 독립적인 언어학적 정답 승인으로 취급하지 않는다.

| 검사 | 결과 |
|---|---:|
| GCC Debug 전체 CTest | 93 / 93, 실패 0 |
| Clang 18.1.3 Debug + ASan/UBSan 변경 경로 CTest | 8 / 8, 실패 0 |
| Native cache·score·ownership assertions | 187,356, 실패 0 |
| 34건 × 분절 profile 2개 × chunk 4개, 전체 span/context trace | 272 / 272 일치 |
| 3,000/30,000개 숫자의 모든 span 검사 | 65,998 / 65,998 일치 |
| 300/3,000개 부분 counter의 모든 span 검사 | 9,898 / 9,898 일치 |
| Native 임의 byte split 비교 | 2,334 / 2,334 일치 |
| 기존 CLI inspect/상세 trace 비교 | 83 / 83 전체 출력 일치 |
| 독립 run-boundary 비교 | 7,899 / 7,899 일치 |
| 최종 배포본 Playwright 웹 검사 | 통과 |
| Reference 구조 감사 | 21개 파일, 886건, 기계적 결함 0 |
| 독립 사람 reference 승인 | 0 / 886 |

Native 검사는 직접 호출한 비캐시 pack 경로와 캐시 경로를 모든 token/부분 scope에
대해 비교한다. 동일 cache의 index 변경, 80개 key의 재배치, factory 중첩과 실패,
부분 데이터 해제, token 수정 후 무효화, 4개 동시 요청, 콜백 취소 및 재진입을
포함한다. 이전 descriptor 형태의 추가 pack이 factory 오류를 발생시키는 경로도
batch/stream 오류·후속 요청 복구·미완성 callback 방지까지 확인한다.

Sanitizer 실행은 leak detection과 오류 시 중단을 켰으며 진단은 없었다.
30,000개 숫자의 계측 실행도 39.082초에 모든 span/trace 검사를 완료했다.
전체 GCC/Clang CI에는 93개, sanitizer CI에는 82개 검사가 등록되어 있다.
일반 Python이 계측 shared library를 직접 적재하는 ctypes gate 11개는 sanitizer
구성에서 제외하며, 해당 경로의 native 계측 검사는 유지한다.

웹 검사에서는 정규화, 상세 trace, 사용자 사전, batch/stream parity, UTF-8
중간 분할, 실시간 callback, 취소, corpus, 다운로드, 모바일 UI를 확인했다.
배포 shared library의 hash가 외부 benchmark에 사용한 최종 library와 같으며
문장 콜백·기존 대표 CLI 결과도 유지한다.

## 외부 개발 benchmark 회귀

같은 Sproat training prefix 3,785문장 / 46,213 span / 16 class를 재실행했다.
이전 개발에서 본 표본이며 held-out 정확도가 아니다. Corpus, evaluator, adapter,
reference record, resource hash가 같은지를 비교기가 검사했다.

| 지표 | v0.18 | v0.19 |
|---|---:|---:|
| 전체 span exact | 34,978 / 46,213 | 34,978 / 46,213 |
| TN-only span exact | 1,118 / 3,293 | 1,118 / 3,293 |
| literal sentence exact | 4 / 3,785 | 4 / 3,785 |
| 보조 spoken-projection exact | 2,197 / 3,785 | 2,197 / 3,785 |
| 엔진 오류 | 0 | 0 |
| batch/stream 비교 | 11,355 / 불일치 0 | 11,355 / 불일치 0 |

두 `results.jsonl` 전체 파일의 SHA256도 같았다:
`2de855736e266e6c2e29e33d99de013b6d1bb85a425ba43b39aa5ee95b7f60ef`.
따라서 pass/fail만 같은 경우뿐 아니라, 계속 오답인 사례의 출력 변화도 없었다.
현재 낮은 exact 지표는 그대로 보고하며 정확도 개선으로 해석하지 않는다.
[집계와 provenance](context-cache-v19-summary.json)만 commit하고 외부 원문이나
문장별 결과는 Git 밖에 둔다.

## 처리 시간과 메모리

GCC 13.3.0 Release, x86_64 Linux VM에서 아래 합성 입력을 같은 resource로
실행했다. 모든 분절 target을 전체 입력보다 크게 설정한 `stream` stdin
측정이며 CLI 시작·resource load·정규화·출력 수집을 포함한다. 기본 target의
TTFB 또는 음성 합성 지연을 측정한 것이 아니다. CPU를 격리하지 않은 1회씩의
관측이므로 하드웨어 일반 성능이나 통계적 신뢰구간으로 해석하지 않는다.

| 입력 | v0.18 시간 | v0.19 시간 | v0.19 peak RSS |
|---|---:|---:|---:|
| `7` 3,000개 | 0.795초 | 0.496초 | 7,076 KiB |
| `7` 30,000개 | 30초 제한 초과 | 5.333초 | 50,420 KiB |
| `hello` 1,000개 | 0.441초 | 0.024초 | 3,924 KiB |
| `hello` 3,000개 | 3.558초 | 0.087초 | 7,308 KiB |
| `3개는` 1,000개 | 0.167초 | 0.166초 | 4,748 KiB |
| `3개는` 3,000개 | 0.597초 | 0.615초 | 9,668 KiB |

완료한 모든 실행은 독립적으로 정한 기대 문자열과 일치했다. Timeout은 완료
시간으로 간주하거나 정확한 배속을 계산하지 않는다. 측정 전후 binary/resource
hash도 같다. 도구는 각 관측을 JSON에 기록하고 입력이 도중에 바뀌면 비교를
무효화한다. 기본 경로에는 외부 데이터나 발음 사전이 필요하지 않다.

첫 후보에서 `3개는` 3,000개가 baseline 중앙값 0.605초에서 2.963초로
느려지는 문제를 별도 3회 측정으로 발견했다. 캐시를 매번 다시 만드는 비용이었다.
이를 숨기지 않고 두 번째 무효화 후 기존 경로로 전환하도록 고쳤다. 위 표는 그
보호가 들어간 최종 후보다. 반복 부분 소비의 기존 순회 자체를 제거한 것은 아니다.

```sh
python3 tools/benchmark/context_scaling.py \
  --before benchmark/generated/baseline-0.18.0/ttf \
  --before-resources benchmark/generated/baseline-0.18.0/resources \
  --after build-v19-release/ttf --after-resources resources \
  --counter-counts 1000 3000 --require-after-complete \
  --output benchmark/generated/context-v19-adaptive-scaling.json
```

기본 timeout은 실행당 30초이며 `--repetitions`, `--timeout-seconds`, family별
count를 조절할 수 있다. GNU time이 없는 환경에서는 RSS 없이 시간과 exact
결과를 남긴다. `--transport normalize`는 argv 한계가 있으므로 큰 입력에는
기본 stdin을 사용한다. 원본 report와 초기 후보 회귀 관측을 요약 JSON에 보존한다.

## 재현

Baseline은 위 commit의 별도 checkout에서 Release로 빌드하고 그 commit의
resources와 함께 보존한다. 아래 `baseline-0.18.0`은 로컬 snapshot 경로이며
binary나 외부 데이터는 저장소에 포함하지 않는다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
python3 tests/integration/test_context_cache.py build/ttf resources \
  --library build/libtts_text_frontend.so
python3 benchmark/evaluators/evaluate.py \
  --corpus /tmp/ttf-research-v11/sproat-bounded/sentences.jsonl \
  --library build-v19-release/libtts_text_frontend.so --resources resources \
  --limit 0 --chunk-bytes 1 7 64 \
  --output-dir benchmark/generated/sproat-v19-adaptive-final
python3 benchmark/evaluators/compare.py \
  benchmark/generated/sproat-v18-release-final \
  benchmark/generated/sproat-v19-adaptive-final \
  --output benchmark/generated/sproat-v19-adaptive-final-comparison.json
```

생성 report에는 새 출력 경로를 사용한다. 다른 환경에서는 라이선스가 허용하는
경로로 같은 source revision/prefix를 다시 가져온다. Corpus SHA256은
`6b0a0a56a4955166ca1786dc6778ab744e372219cc4d8ab6bcf4d51943c2dd51`이다.

## 남은 범위

부분 token 변경이 반복되면 비캐시 정책으로 전환해 기존 순회 비용이 남으므로
모든 입력이 선형 시간에 완료된다고 주장하지 않는다. 열린/중첩 인용·code·opaque literal의 대기 및
반복 scan도 별도 검증 대상이다. [STREAM-01](../remaining-work.md)을 닫지 않는다.
사전 coverage, 실제 금융·콘텐츠 입력, G2P 정확도, 독립 사람 검수와 배포 ABI
자동 검증도 남아 있다. 합성음 비교는 수행하지 않았다.
