# v0.18: 국제전화·내선·금융 식별자

2026-09-17. 비교 기준은 v0.17 `8b01eca`의 Release binary와 같은 commit의
resource snapshot이다. 출력 텍스트와 Semantic IR을 평가하며 합성음은 사용하지 않았다.

## 구현과 편집 범위

- 공통 전화 scanner가 `+`, 명시적인 숫자 그룹, 선행 0과 내선을 보존한다.
  PHONE의 `value`는 본 번호, `secondary`는 내선이다. 기존 public ABI는 유지한다.
- EN/KO pack은 공통 IR을 읽되 plus/zero/separator/extension label은 각 언어
  리소스에서 가져온다. 기존 하이픈 전화번호에도 동일한 읽기·길이 정책을 적용한다.
- 명시적인 Account/Reference 등의 식별자 cue는 PHONE보다 우선한다.
  숫자를 정수로 변환하지 않아 선행 0이 남는다. TN과 발음 사전은 분리되어 있다.
- 경계 검출은 동일한 scanner에 후보 경계까지의 prefix만 전달한다. `ext.`의
  점, 내선과 숫자 그룹을 함께 보류하고 줄바꿈은 내선 범위를 끝낸다.
- 비활성화된 긴 cue도 분절에서 보존하여 짧은 suffix가 다시 적용되지 않게 한다.
- 전화 형태가 손상되면 전체 후보를 보존한다. 괄호 속 일반 수량·연도·서수,
  부호 있는 수·분수·범위와 긴 무표지 식별번호는 별도로 처리한다.

웹 **국제전화·내선·계좌번호 구분** 코퍼스와 inspect에서 결과를 확인할 수 있다.
사람이 읽기 정책을 바꾸는 절차는 [리소스 가이드](../phone-resources.md),
값과 offset 계약은 [Semantic IR](../semantic-ir.md), 보류 범위는
[streaming 문서](../streaming.md)에 있다.

## 같은 참조 55건의 전후 비교

| 지표 | v0.17 | v0.18 |
|---|---:|---:|
| 출력 텍스트 exact | 21 / 55 | 55 / 55 |
| 텍스트·선언한 IR·stream 계약 모두 통과 | 20 / 55 | 55 / 55 |
| IR 계약 실패 사례 | 35 | 0 |
| batch/stream 불일치 사례 | 0 | 0 |

35건 개선, 회귀 0건이다. 이는 새 문법과 보존 정책을 검사하는 개발 참조이며,
모든 전화번호 표기나 일반 문장의 정확도를 대표하는 표본은 아니다.

참조 원문 한 건의 수정 이력을 명시한다. 최초 `en-extension-x` 입력은
문맥 없는 `+숫자 x숫자`를 전화로 가정했다. 금융 계산식과의 충돌을 검토한 뒤
명시적 `Phone:` 라벨을 추가했다. 숫자·내선·IR 값은 바꾸지 않았고 원문 수정은
초기 candidate 실행 후 이루어졌다. 두 버전을 모두 이 최종 원문으로 다시
측정했다. 나머지 54개 참조는 candidate 실행 전에 작성했다. 전체 55건은
독립 사람 검수 대기이며, 모델이 작성한 참조를 사람 승인으로 표시하지 않는다.

최종 corpus SHA256:
`48fc445de0c7eb78958306fd2ba89cd2e9d2bb344e32907cadacaf6b6a1e48d0`.

## 외부 표본과 회귀 수정

기존 Sproat training prefix 3,785문장 / 46,213 span / 16 class를 같은 native
adapter·evaluator·참조·chunk 설정으로 비교했다. 이미 살펴본 개발 표본이며
held-out 성능은 아니다. 집계와 비교 무결성 hash는 [JSON 기록](phone-v18-summary.json)에 있다.

| 지표 | v0.17 | v0.18 |
|---|---:|---:|
| 전체 span exact | 34,978 / 46,213 | 34,978 / 46,213 |
| TN-only span exact | 1,118 / 3,293 | 1,118 / 3,293 |
| literal sentence exact | 4 / 3,785 | 4 / 3,785 |
| 보조 spoken-projection exact | 2,197 / 3,785 | 2,197 / 3,785 |
| 엔진 오류 | 0 | 0 |
| batch/stream 비교 | 11,355 / 불일치 0 | 11,355 / 불일치 0 |

초기 후보에서 괄호를 과도하게 전화 근거로 해석해 CARDINAL 31, DATE 11,
MEASURE 20, ORDINAL 7개가 퇴행했다. 이를 수정하고, exact 점수는 그대로지만
PHONE으로 잘못 바뀐 식별번호 출력 3건도 별도로 검토해 복구했다. 최종적으로
이 표본의 전체 문장 출력 변경은 0건이다. 외부 원문을 새 regression corpus로
복사하지 않고 독립적으로 작성한 괄호·단위·식별자 사례를 테스트에 추가했다.

외부 TELEPHONE 13개는 여전히 dataset reference exact 0개다. 이 class에는
전화 이외 식별번호도 포함되어 있으며, dataset의 `o`/`sil` 등 읽기 정책과
프로젝트 정책도 다르다. 점수에 맞추려고 금융·식별자 의미를 PHONE으로
강제하지 않는다. 이번 작업을 외부 benchmark 정확도 향상으로 보고하지 않는다.

## 실행 검증

| 검사 | 결과 |
|---|---:|
| GCC 전체 CTest | 90 / 90 |
| 전화 text/IR/profile/chunk 계약 | 1,673 checks / 실패 0 |
| Native 두 조각·고정 seed random byte 분할 | 2,480 comparisons / 불일치 0 |
| 직접 phone classifier 검사 | 110,003 checks / 실패 0 |
| Clang ASan + UBSan 변경·인접 경로 | 14 / 14 |
| Web API·ABI·정책·콜백 | 101 checks / 실패 0 |
| 실제 Chromium 브라우저 | 통과 |
| Reference 구조 감사 | 21개 파일, 886건, 기계적 결함 0 |
| 독립 사람 reference 승인 | 0 / 886 |

직접 classifier 검사는 정상·손상 PHONE 대조군과 10,000/100,000개의 일반
숫자 suffix를 검사한다. 별도 Debug 측정의 CPU 시간은 0.968/10.025초였다.
전체 frontend의 성능 보증으로 사용하지 않는다. 계측 없이 큰 입력의 증가율을
검사하고 sanitizer에서는 동일 경로를 10,000/30,000개, 40,003 checks로 검사한다.
최초 100,000개 sanitizer 검사는 동시 작업 중 60초 제한을 넘었으므로 규모·제한을
분리했으며, sanitizer를 끄거나 실패한 메모리 진단을 무시하지 않았다.
14개 sanitizer 검사는 인접 13개와 조정한 직접 scanner 1개를 나누어 실행했다.

CI는 GCC/Clang 각각 90개, ASan+UBSan 80개를 실행하도록 구성했다. 비계측
Python에서 계측 library를 여는 ctypes 10개는 sanitizer job에서 제외하며 두
일반 compiler job에서 검사한다. 위 표는 로컬 실행 결과이며 CI 실행 이력과
구별한다. README의 전화 예제, 최초 `1st`·Apple·GPU·WiFi, 금액·수량·시간 범위
출력도 직접 실행해 확인했다.

## 재현

```sh
python3 tools/evaluation/evaluate.py \
  --ttf benchmark/generated/baseline-0.17.0/ttf \
  --resources benchmark/generated/baseline-0.17.0/resources \
  --corpus tests/evaluation/phone_cases_v18.jsonl \
  --output-dir benchmark/generated/phone-v18-baseline-v17-reviewed
# baseline의 실패 exit 1은 기록할 결과다. candidate는 exit 0이어야 한다.
python3 tools/evaluation/evaluate.py --ttf build-web/ttf --resources resources \
  --corpus tests/evaluation/phone_cases_v18.jsonl \
  --output-dir benchmark/generated/phone-v18-release-final
python3 tools/evaluation/compare.py \
  --before benchmark/generated/phone-v18-baseline-v17-reviewed/report.json \
  --after benchmark/generated/phone-v18-release-final/report.json \
  --output-dir benchmark/generated/phone-v18-release-final-comparison
python3 benchmark/evaluators/evaluate.py \
  --corpus /tmp/ttf-research-v11/sproat-bounded/sentences.jsonl \
  --library build-web/libtts_text_frontend.so --resources resources \
  --limit 0 --chunk-bytes 1 7 64 \
  --output-dir benchmark/generated/sproat-v18-release-final
python3 benchmark/evaluators/compare.py \
  benchmark/generated/sproat-v17-release-verified \
  benchmark/generated/sproat-v18-release-final \
  --output benchmark/generated/sproat-v18-release-final-comparison.json
```

출력 디렉터리는 새 경로를 사용한다. 다른 환경에서는 baseline commit을 별도
checkout에서 빌드하고 같은 commit의 resources를 사용한다. 이전 외부 report를
재사용한 비교에서는 evaluator/adapter/corpus/평가 record hash가 모두 같음을
검증했다. 외부 데이터와 binary, 원문별 결과는 Git에 포함하지 않는다.

## 남은 한계

전화 인식은 국가별 할당·실제 개통 여부를 검증하지 않는다. 라벨 없는 괄호
표기는 보수적인 초기 형태 제한이 있고 단축번호·모든 구분 표기는 지원하지
않는다. 국가별 읽기 관행과 새 cue는 독립적으로 검수해야 한다.

전화 후보 탐색은 384 byte로 제한했지만 매우 큰 단위 전체의 context 처리
비용까지 해결한 것은 아니다. 30,000개 숫자를 전체 입력 크기의 분절 target으로
실행하면 기존 v0.17도 30초를 넘었다. 이 조건은 기본 target의 TTFB 수치와
구별하며 [STREAM-01](../remaining-work.md)에 남긴다. 전체 언어 coverage,
phonetic mapper와 독립 사람 검수 역시 production 완료 조건으로 남아 있다.
