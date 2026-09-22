# v0.20 입력 제어와 최종 발화 텍스트 검증

작성일: 2026-09-17. 평가는 출력 텍스트, Semantic IR, resource trace와
스트리밍 콜백이다. 합성음 생성·음성 비교는 수행하지 않았다.

## 참조와 비교의 범위

사용자가 기록한 사례를 현재 v0.19에서 먼저 재현했다. 당시 기록이 최신
버전의 측정값이라는 가정은 하지 않았다. `ABC마트`는 이미 올바르게 읽혔고,
그 동작도 회귀 조건으로 유지했다.

`tests/evaluation/user_patterns_v20.jsonl`의 **51건**은 후보 실행 전에 작성한
개발 계약이다. SHA256:
`ff1a6df945fb6501dc40c217c168cb259c759df0986f08c133acdb1ecad93d96`.
참조는 후보 출력으로 다시 생성하거나 수정하지 않았다. 이들은 프로젝트
읽기 정책에 따른 pending 참조이며 독립 언어 검수 완료를 주장하지 않는다.

v0.19에는 새 input/output 옵션이 없으므로 새 51건을 v0.19에서 옵션 없이
실행한 결과를 같은 설정의 정확도 비교로 포장하지 않는다. 새 기능 계약의
현재 통과율과, 기존 text 설정에서의 회귀 비교를 별도로 기록한다.

## 구현과 실제 출력

| 입력 (`speech`, KO 기본 문맥) | 출력 |
|---|---|
| `www.는 월드와이드웹의 줄임말이다` | `더블유더블유더블유는 월드와이드웹의 줄임말이다` |
| `1~2만 원대면 괜찮은 가격이지` | `만 원에서 이만 원대면 괜찮은 가격이지` |
| `k7 타고 다닌다` | `케이칠 타고 다닌다` |
| `오늘의 top100` | `오늘의 탑백` |
| `대륭포스트타워 20F 으로 오세요` | `대륭포스트타워 이십 층 으로 오세요` |
| `유&미` | `유앤미` |
| `1+1은 귀요미` | `일 더하기 일은 귀요미` |
| `이메일 abc@example.com` | `이메일 에이비씨 앳 이엑스에이엠피엘이 닷 씨오엠` |

추가 `annotated` 모드에서 `1{첫}번째 → 첫번째`, `1{@native} → 하나`,
`1{@sino} → 일`, `ABC{@alphabet}마트 → 에이비씨마트`를 확인했다.
직접 alias, 그룹, 이스케이프, 잘못된 제어문법, 처리 중 오류, 원문·출력
오프셋과 resource 우선순위를 함께 검사한다.

- 미지원 한자·이모지 기본값은 **보존**이다. 명시적 drop은 해당 문자와
  ZWJ/수식자 잔여 문자를 제거한다. 숫자 키캡은 숫자만 읽지 않도록 묶는다.
- 등록한 한자와 키캡은 `speech_character`를 먼저 적용한다.
- 원문 보존·제거 이유, 위치, 실제 소비한 spelling resource 행이 trace에 남는다.
- 사용자·고객 발음, 명시적 alias, 보호 코드와 모호한 의미 객체는 각각의
  보존/우선순위 계약을 유지한다.
- `K7`, 선행 0, word+number, 영어 문맥을 함께 확인한다. 기존 uppercase
  initialism 정책 `GPT-4 → 지피티포`도 유지한다.
- 열린 제어는 임의 UTF-8 분할에서 대기하며, raw byte 제한 초과를 닫는 괄호
  또는 flush까지 미루지 않는다. 앞서 확정한 콜백은 나중 오류에도 유지된다.

## 검증 실행

새 통합 스크립트에서 **813 CLI 계약**, **2,164 native 분할·오류·ownership
검사**가 통과했다. 별도 코퍼스 평가에서 **51/51**이며 기본/짧은 분절,
1·2·7·4096바이트 및 모든 두 조각 경계와 난수 스케줄을 검사한다.
검사 개수는 스크립트가 세는 검증 항목이며 독립적인 2,977개 언어 문장이라는
뜻은 아니다. 전체 테스트와 외부 회귀 결과는 이 문서의 아래 실행 요약을 따른다.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
python3 tests/integration/test_user_patterns.py build/ttf resources \
  --library build/libtts_text_frontend.so
python3 tools/evaluation/evaluate.py --ttf build/ttf --resources resources \
  --corpus tests/evaluation/user_patterns_v20.jsonl \
  --output-dir artifacts/evaluation/user-patterns-v20
```

외부 데이터는 기존 라이선스·로컬 다운로드 정책을 유지한다. 대용량 원본이나
새 외부 사전은 이번 변경으로 vendoring하지 않았다.

## 남은 범위

이 버전이 production 전체 품질을 인증하지는 않는다. 일반 G2P, 실제 업무
코퍼스의 사전 coverage와 독립적인 정답 검수가 계속 필요하다. `top → 탑`도
새로 작성한 pending 정책이다. 모든 Unicode grapheme/emoji 조합을 판별하는
완전한 segmenter는 아니다. `speech_character`는 현재 한 코드 포인트 또는
명시적으로 묶은 키캡 단위로 적용한다. 알 수 없는 문자열 보존과 의미가
모호한 숫자·단위 보호는 의도된 동작이다.

`@native`는 0..99 독립형, `@sino`는 명시된 십진수 범위, `@alphabet`은
ASCII 문자·숫자로 제한된다. 다른 문법은 오류로 알린다. 전체 Unicode,
잘못된 제어문의 세부 진단 위치 API, 최악 입력에서의 TTFB, 자동 binary ABI
비교와 실제 배포 운영 검증은 후속 범위다.

편집 방법: [입력 문법](../input-format.md), [읽기 리소스](../speech-resources.md),
[API](../api.md), [전체 남은 작업](../remaining-work.md).

## 외부 회귀 측정 (기존 plain/text 설정)

Sproat 개발 corpus의 3,785문장, 46,213 span, 16 class를 같은 현재 Python
adapter로 v0.19 snapshot과 v0.20에 다시 실행했다. 새 adapter 해시가 달라
예전 report를 재사용하지 않았다. strict comparator가 동일 참조·실행 설정을
확인했고, 결과 행 파일 전체가 동일했다 (잘못 읽은 결과도 그대로 포함).

| 측정 | v0.19 | v0.20 |
|---|---:|---:|
| 모든 span exact | 34,978 / 46,213 | 34,978 / 46,213 |
| TN 대상 span exact | 1,118 / 3,293 | 1,118 / 3,293 |
| literal sentence exact | 4 / 3,785 | 4 / 3,785 |
| spoken projection sentence exact | 2,197 / 3,785 | 2,197 / 3,785 |
| 엔진 오류 | 0 | 0 |
| chunk 비교 / 불일치 | 11,355 / 0 | 11,355 / 0 |

수정/회귀 span은 모두 0이다. **새 speech 모드의 외부 정확도 개선 수치가
아니며 기존 동작 회귀 증거다.** 특히 TN 대상 exact가 낮으므로 새로운
51건 통과를 일반 영어 TN의 production 품질로 해석하면 안 된다.

Corpus SHA256:
`6b0a0a56a4955166ca1786dc6778ab744e372219cc4d8ab6bcf4d51943c2dd51`.
양쪽 results SHA256:
`2de855736e266e6c2e29e33d99de013b6d1bb85a425ba43b39aa5ee95b7f60ef`.
로컬 report는 `benchmark/generated/sproat-v20-baseline-v19-current-adapter`,
`sproat-v20-final`, `sproat-v20-final-comparison.json`이다.
재현 명령은 v0.19 실행 기록의 evaluator와 comparator에 이 경로를 지정한다.

## 2026-09-22 마무리 및 인수인계

GCC 전체96개 중95개, ASan/UBSan84개 중83개가 첫 실행에 통과했다.
두 suite를 2-core VM에서 동시에 돌릴 때 30k-number context gate가60초 제한을
넘었다. 수정 없이 각각 단독 재실행하여 GCC19.92초, sanitizer48.50초에 통과했다.
시간 제한은 변경하지 않았다. 같은 유형의 불필요한 재실행을 피하도록 AGENTS와
인수인계 가이드에 VM의 실행 제약을 기록했다.

새 `tools/quality/tn_cycle.py`와 실제 CLI 계약 검사는 참조 덮어쓰기/캡처 변조를
거부하고 fixed/regressed/remaining_failure를 구분한다. 외부 원문을 포함하지 않는
class별 진단과 작업 큐, 다음 모델용 요청 템플릿은 [HANDOFF](../HANDOFF.md)에서
시작한다. 이번 작업 후 무기한 goal을 자동 재개하지 않는다.
