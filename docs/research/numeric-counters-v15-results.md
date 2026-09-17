# v0.15: 숫자 묶음·한국어 카운터·발음 출처

2026-09-17. 비교 기준은 v0.14 `e11afa8`의 Release shared library와 같은
commit의 resource snapshot이다. 결과는 출력 텍스트와 Semantic IR을 평가한다.
합성 음성 생성·청취·음성 비교는 수행하지 않았다.

## 변경 내용

- Unicode 숫자 공백을 묶음 문법으로 처리하고, 값·source byte offset을 보존한다.
- 한국어 `번째`, 횟수/번호 `번`, `제N`을 분리하고 finite context cue로 선택한다.
- 그룹 숫자와 ordinal을 공통 scanner로 해석하여 `제1 250호`의 숫자가 나뉘지 않는다.
- 조사 뒤 오른쪽 단서가 올 수 있는 경우 pending을 유지하여 짧은 분절의 오독을 막는다.
- 발음 사전·initialism 사전·G2P source layer를 기존 ABI 구조체를 통해 전달한다.
- evaluator가 `secondary`도 비교한다. 범위 오른쪽 값과 counter 역할의 불일치를 검출한다.
- 웹에 카운터 28건 코퍼스와 Unicode 금액·손상 입력·범위 예제 4개를 등록했다.

## 독립 작성 개발 참조의 전후 비교

| 대상 / 지표 | v0.14 | v0.15 |
|---|---:|---:|
| 한국어 카운터 295개, 출력 exact | 80/295 | 295/295 |
| 같은 295개, 출력 + 선언한 IR/언어 계약 exact | 29/295 | 295/295 |
| Unicode 고정 원문 57개, 출력 exact | 0/57 | 57/57 |
| 같은 57개, 전체 span 의미 class exact | 0/57 | 57/57 |
| 같은 57개, value·secondary·unit exact | 0/57 | 57/57 |

한국어 출력은 215개 개선, 회귀 0개다. Unicode 고정 참조는 57개 개선,
회귀 0개다. 한국어 참조는 전체 숫자 범위를 대표하는 무작위 표본이 아니라
이번 grammar와 보존 계약을 검사하는 개발용 입력이다. 1~99 수사 목록과
기대 문장은 구현 출력에서 생성하지 않았다. Unicode의 위 57건 집계에는
Decimal로 생성한 90개 수치 oracle, malformed guard, streaming 반복을 넣지 않았다.

[카운터 집계](korean-counters-v15-summary.json)와
[Unicode 집계](unicode-grouping-v15-summary.json)에 case/library/resource hash와
클래스별 수치를 기록했다. baseline과 candidate에 같은 참조를 사용한다.

```sh
python3 benchmark/evaluators/compare_korean_counters.py \
  --before-library benchmark/generated/baseline-0.14.0/libtts_text_frontend.so \
  --before-resources benchmark/generated/baseline-0.14.0/resources \
  --after-library build/libtts_text_frontend.so --after-resources resources \
  --output benchmark/generated/korean-counters-comparison.json
python3 benchmark/evaluators/compare_unicode_grouping.py \
  --baseline-library benchmark/generated/baseline-0.14.0/libtts_text_frontend.so \
  --baseline-resources benchmark/generated/baseline-0.14.0/resources \
  --candidate-library build/libtts_text_frontend.so --candidate-resources resources \
  --output benchmark/generated/unicode-grouping-comparison.json
```

Binary snapshots은 Git에 포함하지 않는다. 다른 환경에서는 `e11afa8`을 별도
checkout에서 빌드하고 그 commit의 resources를 사용한다. 현재 resources와
이전 library를 섞어 baseline이라고 보고하지 않는다.

## 외부 영어 개발 benchmark의 회귀 검사

기존과 같은 Sproat training prefix 3,785문장 / 46,213 span / 16 class를
재실행했다. corpus SHA256은
`6b0a0a56a4955166ca1786dc6778ab744e372219cc4d8ab6bcf4d51943c2dd51`이다.
이미 실패를 확인한 개발 표본이며 held-out 성능이 아니다.

| 지표 | v0.14 | v0.15 |
|---|---:|---:|
| 전체 span exact | 34,978 / 46,213 | 34,978 / 46,213 |
| TN-only span exact | 1,118 / 3,293 | 1,118 / 3,293 |
| literal sentence exact | 4 / 3,785 | 4 / 3,785 |
| 보조 spoken-projection exact | 2,197 / 3,785 | 2,197 / 3,785 |
| 엔진 오류 | 0 | 0 |
| batch/stream 비교 | 11,355 / 불일치 0 | 11,355 / 불일치 0 |

class별 exact 판정의 fixed/regressed 모두 0이다. 이번 작업이 영어 benchmark
정확도를 개선했다고 주장하지 않는다. projection은 문장부호·spacing 정책을
변환한 보조 지표이며 낮은 literal exact를 대체하지 않는다. 한국어 실제 업무
정확도 역시 이 영어 표본으로 추정하지 않는다.

[외부 집계](numeric-counters-v15-external-summary.json)는 수치·hash만 저장한다.
원문, 기대 문장, 개별 실패 ID와 대용량 원본은 Git에 추가하지 않았다. 로컬
원본 결과는 `benchmark/generated/sproat-v15-final-verified/`, 비교 결과는
`benchmark/generated/sproat-v15-final-comparison.json`에 있다.

## 검증 결과

- GCC CTest: 81/81.
- Clang CTest: 81/81.
- Clang ASan+UBSan: 72/72. Python ctypes가 비계측 Python에 sanitizer library를
  적재하는 9개 항목은 이 설정에서 제외하며 일반 GCC/Clang 경로에서 검사한다.
- KO: 원문 295개, 출력/IR/문맥/정책/chunk 검사 2,122건, 모든 선택 입력의
  두 조각 byte split 868건.
- Unicode: 검사 884건, 두 조각 byte split 3,634건.
- Pronunciation provenance: native protocol 147건, 정책 검사 102건,
  두 조각 byte split 746건. User/Customer override와 fallback layer 초기화를 포함한다.
- 웹: 기존 normalize/inspect/callback/chunk검증·사전 override·코퍼스·모바일 검사와
  신규 28건 코퍼스 및 Unicode 예제 통과. VM port 8000의 service는 v0.15.0이다.
- Reference audit: 20개 파일 831건, 구조/기계적 결함 0, 독립 사람 검수 완료 0.
- 최초 acceptance의 `1st`, Apple, GPU, WiFi CLI 출력을 다시 확인했다.

## 남은 한계와 다음 작업

한국어 cue는 유한한 policy이며 모든 조사·어미·복합 문맥의 정답을 보장하지 않는다.
문맥 없는 `번`은 Sino 기본값, 100 이상 서수는 프로젝트 정책이다. 전체 CLDR locale,
모든 Unicode numeral과 접두 서수 범위의 발화는 지원 범위에 포함하지 않는다.
부적합 형식의 전체 보존은 의미 훼손을 막는 정책이며 읽기 coverage의 완성을 뜻하지 않는다.

이번 provenance는 source layer다. target mapper의 파일/행·layer, 복수 발음의
variant 선택과 compound 단어별 상세 경로는 아직 남아 있다. 잘못된 일부 resource
정책은 normalize 시점에 거부되며 setup 단계 검증은 다음 우선순위다.

[남은 작업](../remaining-work.md)을 유지한다. 실제 금융/콘텐츠 corpus의 독립 검수,
사전 관용형 검토, held-out 평가도 필요하며 현재의 통과율을 production 전반의
무결성이나 합성음 품질로 해석하지 않는다.
