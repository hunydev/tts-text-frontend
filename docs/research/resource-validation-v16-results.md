# v0.16: 리소스 적용 전 정책 검증

## 변경 범위

C runtime의 공통 검증기와 KO/EN pack의 정책 검증기를 추가했다. 기존 ABI v1
구조체를 바꾸지 않고 별도 versioned descriptor와 `_ex` 진단 API를 사용한다.
기본 manifest 1,697행은 모두 담당 schema에서 검사되며 미등록 행은 0이다.

잘못된 행은 임시 파일 파싱 중 거부하여 기존 리소스를 보존한다. 모든 overlay를
로드한 뒤 날짜 범위, 필수 영어 읽기 label, 통화 minor 정책, 문맥 cue의 streaming
경계 hint, title abbreviation의 companion을 검사한다. 실패한 freeze는 수정 후
재시도할 수 있다. 미등록 namespace는 통과시키면서 수를 보고하고, CLI/compiler의
strict mode에서는 거부한다. 새 언어팩은 자체 검증기를 등록할 수 있다.

검토 중 발견한 결함도 회귀 검사에 넣었다: manifest의 NUL/잘못된 UTF-8,
0으로 비활성화한 숫자 경계 flag, 지원하지 않는 CLI default language,
소비자의 용량을 넘는 phone 수, 조회되지 않는 대문자/선행 0 key,
잘못된 compiler 응답 protocol, 배포 domain/customer 문맥 누락.

웹은 고객 TSV의 파일·행·키별 오류를 출력하고, 성공 결과에 검사 행 수를 포함한다.
기본 리소스의 reading label이나 corpus 기대값을 이번 검증에 맞춰 바꾸지 않았다.
[편집·API 가이드](../resource-validation.md)에 적용 순서와 제약을 기록했다.

## 검사 결과

| 검사 | 결과 |
|---|---:|
| GCC 전체 CTest | 84 / 84 |
| Native validator API | 102,808 assertions / 실패 0 |
| CLI / compiler policy 검사 | 318 assertions / 실패 0 |
| Compiler protocol / 배포 문맥 검사 | 297 assertions / 실패 0 |
| Clang 변경 경로 CTest | 5 / 5 |
| ASan + UBSan 변경 경로 CTest | 5 / 5 |
| Playwright 웹 검사 | 통과 |
| 기본 리소스 schema coverage | 1,697 / 1,697, 미등록 0 |
| Reference 구조 감사 | 20개 파일, 831건, 기계적 결함 0 |
| 독립 사람 reference 승인 | 0 / 831 |

Clang 및 sanitizer의 변경 경로 5개는 native API, CLI 정책, compiler protocol,
기존 compiler roundtrip, CMU importer다. 전체 CTest도 GitHub Actions의 GCC/Clang/
sanitizer job에 등록되어 있다. Sanitizer 설정은 비계측 Python이 계측 library를
직접 적재하는 ctypes 검사 9개를 제외하고 75개를 실행한다. 일반 build는 84개다.
새로운 잘못된 리소스 사례는 setup 단계에서 거부되는지를 검증하며, 정상 출력의
기대값을 느슨하게 바꾸지 않았다.

대표 CLI 출력도 다시 확인했다.

| 입력 | 출력 |
|---|---|
| `1st는 첫 번째라는 뜻입니다.` | `퍼스트는 첫 번째라는 뜻입니다.` |
| `Please go to the 1st floor` | `Please go to the first floor` |
| `Apple은 사과입니다.` | `애플은 사과입니다.` |
| `GPU를 사용합니다.` | `지피유를 사용합니다.` |
| `WiFi를 사용합니다.` | `와이파이를 사용합니다.` |
| `1~2만 원` | `만 원에서 이만 원` |
| `1~2개` | `한 개에서 두 개` |
| `1~2시` | `한 시에서 두 시` |

## 외부 영어 개발 benchmark 회귀

같은 Sproat training prefix 3,785문장 / 46,213 span / 16 class를 v0.15와 v0.16에
적용했다. 이전에 실패를 검토한 개발 표본이며 held-out 성능이 아니다. 새 native
adapter로 baseline도 다시 실행하여 evaluator/adapter hash를 동일하게 맞췄다.

| 지표 | v0.15 | v0.16 |
|---|---:|---:|
| 전체 span exact | 34,978 / 46,213 | 34,978 / 46,213 |
| TN-only span exact | 1,118 / 3,293 | 1,118 / 3,293 |
| literal sentence exact | 4 / 3,785 | 4 / 3,785 |
| 보조 spoken-projection exact | 2,197 / 3,785 | 2,197 / 3,785 |
| 엔진 오류 | 0 | 0 |
| batch/stream 비교 | 11,355 / 불일치 0 | 11,355 / 불일치 0 |

class별 fixed/regressed는 모두 0이다. 이번 변경으로 TN 정확도가 개선됐다고
주장하지 않는다. 낮은 literal exact는 projection 점수로 대체하지 않는다.
[집계 및 hash](resource-validation-v16-summary.json)에는 원문이나 실패 예문을
포함하지 않았다. 외부 대용량 데이터는 계속 Git 밖에 둔다.

```sh
# baseline: f527506의 library 및 resources를 별도 build/snapshot에서 준비한다.
python3 benchmark/evaluators/evaluate.py \
  --corpus /tmp/ttf-research-v11/sproat-bounded/sentences.jsonl \
  --library benchmark/generated/baseline-0.15.0/libtts_text_frontend.so \
  --resources benchmark/generated/baseline-0.15.0/resources \
  --limit 0 --chunk-bytes 1 7 64 \
  --output-dir benchmark/generated/sproat-v16-baseline-v15
python3 benchmark/evaluators/evaluate.py \
  --corpus /tmp/ttf-research-v11/sproat-bounded/sentences.jsonl \
  --library build/libtts_text_frontend.so --resources resources \
  --limit 0 --chunk-bytes 1 7 64 \
  --output-dir benchmark/generated/sproat-v16-release-verified
python3 benchmark/evaluators/compare.py \
  benchmark/generated/sproat-v16-baseline-v15 \
  benchmark/generated/sproat-v16-release-verified \
  --output benchmark/generated/sproat-v16-release-comparison.json
```

다른 환경에서는 고정 source revision과 importer로 같은 prefix를 재생성한다.
검증한 corpus SHA256은
`6b0a0a56a4955166ca1786dc6778ab744e372219cc4d8ab6bcf4d51943c2dd51`이다.

## 큰 사전 setup smoke

임시 생성한 100,000행(약 3.7 MB) source pronunciation 및 target lexicon에
GCC Debug library를 사용했다. 각각 기본 manifest를 합친 101,697행이 모두
검증됐다. 3회씩 측정한 process CPU time은 load 약 0.23~0.27초,
explicit validate 0.16~0.20초, freeze 0.21~0.23초였으며 peak RSS는 약 27.5 MB다.
다른 검사가 동시에 실행된 VM에서 wall time은 약 4.3~5.6초였다. 격리된 성능
benchmark나 production latency 목표 달성 근거로 쓰지 않는다. 임시 대량 사전은
삭제했으며 저장소에 포함하지 않았다.

## 남은 범위

리소스의 enum·범위·의존성 검사는 일반 발음 문자열의 언어학적 정답을 보장하지
않는다. 미등록 namespace는 별도 validator가 필요하고, 최종 관계 오류의 원본
파일/행 이력은 현재 보존하지 않는다. G2P 조각 합성에는 기존 IR 용량 한계가 남는다.
Target mapper/variant 상세 trace, 외부 benchmark의 DATE/DIGIT/전자 주소 등
실패 class, 금융·콘텐츠 실제 corpus와 독립 검수가 다음 우선순위다.
[전체 남은 작업](../remaining-work.md)을 유지한다. 합성음 비교는 수행하지 않았다.
