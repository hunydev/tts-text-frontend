# v0.17: 실제 발음 선택의 상세 trace와 복수 발음 리소스

## 변경 범위

기존 text/span/pack ABI v1을 유지하면서 `ttf_trace.h`를 추가했다. 실제
정규화 중 소비한 target lexicon, source pronunciation/G2P, target phone/grapheme
행을 별개 event로 기록한다. 키·값·계층·namespace·선언 순서가 포함된다.
미사용 조회는 기록하지 않으며, 생성 결정은 출처 미상으로 표시한다. 실패 후
폐기된 phone 경로와 채택된 fallback을 구분한다. 요청별 engine view와 copied
event가 공유 엔진의 변경이나 재정규화 없이 동시 요청을 분리한다.

`pronunciation_variant`와 `pronunciation_choice`로 명시적인 복수 발음을
선택한다. 상위 base override와 target lexicon 우선순위를 유지한다. 선택이
없으면 기존 기본값을 사용한다. 선택한 variant 누락은 freeze에서 거부한다.
CMU importer의 `--variant-policy all`은 대안을 보존하지만 선택 정책을 자동
설치하지 않는다. 자동 heteronym 문맥 판정기는 이번 범위에 포함하지 않는다.

CLI `inspect --trace`, `stream-json --trace` 및 웹의 **발음 상세 경로**에서
확인할 수 있다. 웹의 입력 분할 검증은 text/span/segment와 상세 trace까지 비교한다.
편집 예제와 lifetime·offset·예산 계약은 [가이드](../detailed-pronunciation-trace.md)에 있다.

## 검증 결과

| 검사 | 결과 |
|---|---:|
| GCC 전체 CTest | 86 / 86 |
| Native 상세 trace | 3,413 assertions / 실패 0 |
| CLI 상세 trace | 3,783 assertions / 실패 0 |
| Clang ASan + UBSan 변경 경로 | 9 / 9 |
| Web API 검사 | 91 checks / 실패 0 |
| Playwright Chromium | 통과 |
| Reference 구조 감사 | 20개 파일, 831건, 기계적 결함 0 |
| 독립 사람 reference 승인 | 0 / 831 |

Native 검사는 실제 소비한 행, wildcard/layer, 선택되지 않은 probe 제외,
source/target 실패 구분, compound/ordinal 부분 좌표, variant override,
모든 두 조각 byte 분할과 고정 seed random 분할, 두 segmentation profile,
callback 수명·취소·재진입, 누적 예산·sticky error, 제3 pack, 동시 요청을 다룬다.
CLI는 JSON 계약, 출력 실패, 예산 초과 때 잘린 trace를 성공으로 내보내지 않는
동작도 확인한다. Sanitizer 9개는 새 native/CLI 두 개, 기존 source trace,
CMU importer, shared-engine concurrency 및 네 streaming 검사다.

전체 sanitizer job은 비계측 Python이 계측 library를 적재하는 ctypes 9개를
제외한 77개를 실행하도록 CI에 구성되어 있다. GCC/Clang job은 각각 86개다.
이 표의 local 실행 결과와 CI 결과는 구분한다.

대표 CLI의 `1st` KO/EN 문맥, Apple/GPU/WiFi, `1~2만 원`, `1~2개`, `1~2시`
출력도 기존 문서의 문자열과 일치했다. 기존 expected text는 수정하지 않았다.

## 외부 개발 표본 회귀

동일 Sproat training prefix 3,785문장 / 46,213 span / 16 class에 v0.16 baseline과
v0.17 Release library를 적용했다. Native adapter가 변경되어 같은 현재 adapter로
baseline도 재실행했다. evaluator/adapter/corpus/평가 record hash가 일치함을
비교기가 확인했다. 이전에 살펴본 개발 표본이며 held-out 성능이 아니다.

| 지표 | v0.16 | v0.17 |
|---|---:|---:|
| 전체 span exact | 34,978 / 46,213 | 34,978 / 46,213 |
| TN-only span exact | 1,118 / 3,293 | 1,118 / 3,293 |
| literal sentence exact | 4 / 3,785 | 4 / 3,785 |
| 보조 spoken-projection exact | 2,197 / 3,785 | 2,197 / 3,785 |
| 엔진 오류 | 0 | 0 |
| batch/stream 비교 | 11,355 / 불일치 0 | 11,355 / 불일치 0 |

class별 fixed/regressed는 0이다. 이 표본은 기본 untraced API를 사용한 출력
회귀 측정이다. 새 trace의 동등성은 별도의 native/CLI/web 계약 검사가 담당한다.
낮은 literal exact를 보조 projection 점수로 대체하지 않으며, 이번 작업으로
TN의 언어학적 정확도가 높아졌다고 주장하지 않는다.

```sh
python3 benchmark/evaluators/evaluate.py \
  --corpus /tmp/ttf-research-v11/sproat-bounded/sentences.jsonl \
  --library benchmark/generated/baseline-0.16.0/libtts_text_frontend.so \
  --resources benchmark/generated/baseline-0.16.0/resources \
  --limit 0 --chunk-bytes 1 7 64 \
  --output-dir benchmark/generated/sproat-v17-baseline-v16
python3 benchmark/evaluators/evaluate.py \
  --corpus /tmp/ttf-research-v11/sproat-bounded/sentences.jsonl \
  --library build-web/libtts_text_frontend.so --resources resources \
  --limit 0 --chunk-bytes 1 7 64 \
  --output-dir benchmark/generated/sproat-v17-release-verified
python3 benchmark/evaluators/compare.py \
  benchmark/generated/sproat-v17-baseline-v16 \
  benchmark/generated/sproat-v17-release-verified \
  --output benchmark/generated/sproat-v17-release-comparison.json
```

## CMU 전체 opt-in export 확인

고정 revision `74790861f652b15e4ac49015a90074ad62a27690`의 원본을 사용했다.
Archive에는 126,052 headword / 135,166 pronunciation을 보존했다. `all` runtime
export는 117,493 base + 8,362 numbered variant를 생성했다. ASCII letter 이외의
headword 8,559개(9,311 pronunciation)는 runtime export 대상에서 제외되고 archive에
남는다. 대안 ID/길이 제한으로 생략한 항목은 0이다. 기본 설치와 자동 choice는 없다.

외부 원본과 생성된 대형 사전은 Git에 포함하지 않았다. Attribution/COPYING을
생성물에 보존한다. [집계·해시](pronunciation-trace-v17-summary.json)에 결과와
provenance를 기록했으며 원문 corpus나 실패 예문은 포함하지 않았다.

## 남은 범위

상세 trace가 발음의 정답을 인증하지는 않는다. 외부 pack의 미보고 내부 규칙,
TN 전체의 자동 trace, 파일/행 출처, 음절별 alignment, 자동 문맥별 heteronym
선택은 별도 작업이다. 한국어 phonetic mapper는 여전히 prototype이고 독립 사람
검수는 0건이다. [남은 요구사항](../remaining-work.md)의 언어 coverage와 외부
한국어 corpus 실측, 검수 절차를 계속 진행해야 한다.
