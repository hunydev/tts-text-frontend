# NUM-WIDTH: 전각 숫자의 값 해석

2026-09-28. public ABI v1, 기본 옵션 변경 없음. 이번 범위는 **U+FF10–U+FF19
(`０`–`９`)**다. 숫자를 한 글자씩 발음 사전에 넣지 않고 기존 숫자 의미 해석에 연결한다.

## 사용자에게 보이는 동작

| 입력 | KO 출력 |
|---|---|
| `１２３원` / `1２3원` | `백이십삼 원` |
| `₩１２,５００` | `만이천오백 원` |
| `３개` | `세 개` |
| `２０２６-０９-１６` | `이천이십육 년 구 월 십육 일` |
| `０１０-１２３４-５６７８` | `공일공 일이삼사 오육칠팔` |
| `１~２만 원` | `만 원에서 이만 원` |

EN 문맥도 동일한 기존 정책을 쓴다. `１st` → `first`, `$１２.５０` →
`twelve dollars and fifty cents`. 별도 옵션 없이 text/speech 모두 숫자 의미 해석을 한다.
선행 0·연도·서수·범위 읽기의 기존 선택은 변경하지 않는다. 전각 문자 폭과 의미를 분리한다.

```sh
build/ttf normalize --output-mode speech '１２３원'
build/ttf inspect --output-mode speech --trace '０１０-１２３４-５６７８'
```

웹 도구의 **전각 숫자 · 금액 / 날짜 / 전화 / 셈단위** 코퍼스와 예문에서 검증할 수 있다.
이는 저장소의 웹 도구를 갱신한 것이며 실행 중인 서비스 배포·재시작과는 별개다.

## 공통 scanner view

기존에는 universal numeric, phone, EN date, KO counters가 각각 ASCII 숫자를 스캔했다.
네 구현마다 UTF-8 폭 처리를 복제하는 대신 `src/semantic/numeric_width.c`에서 요청별
scanner view를 만든다. classifier와 segmenter가 같은 뷰를 사용한다.

1. 원문에서 코드·경로·태그·annotated 제어 보호 구간을 먼저 찾는다.
2. 보호 구간 밖의 전각 숫자만 ASCII로 폴딩한다. 전체 NFKC는 적용하지 않는다.
3. tokenizer는 전각 숫자를 `DIGIT`으로 분류하고 인접 ASCII 숫자와 병합한다.
4. 기존 universal/pack classifier가 뷰를 읽어 기존 Semantic IR을 만든다.
5. 숫자를 surface에서 읽는 verbalizer에는 canonical surface를 전달한다.
6. raw↔scan byte map으로 소비한 범위와 streaming 경계를 원문 위치로 되돌린다.

원문 문자열을 덮어쓰지 않는다. context detection은 원문 토큰을 사용하고 숫자는 계속
중립적이다. exact/pattern 리소스의 열쇠도 원문이다. `exact ko-KR １２３원 고객금액`과
`exact ko-KR 123원 일반금액`은 서로 다른 override다. 기존 `{digits}` DSL은 여전히
ASCII 패턴이며 이 변경으로 자동 확장하지 않는다.

전각 숫자가 없는 처리 단위는 뷰·byte map을 할당하지 않는다. 문장 전체를 추가 버퍼링하지
않고 기존 확정 단위·lookahead·callback을 유지한다. 엔진 전역 가변 상태나 새 public 필드는
없다. 라이브러리/팩 ABI v1은 유지한다.

## 원문·IR·trace 계약

- 예: `１２３원`의 span.surface는 원문 그대로, semantic.type=CURRENCY, value=`123`.
- span 및 trace의 `input_begin/input_end`는 **원본 UTF-8 byte offset**이다. scanner의
  ASCII byte offset을 외부 입력 위치로 노출하지 않는다.
- `decision / numeric-width` trace는 폴딩한 규칙과 원문을 기록한다.
- trace의 `part_*`는 기존 [상세 trace 계약](detailed-pronunciation-trace.md)에 따라
  실제 발음 입력에 상대적이다. canonical surface를 읽는 verbalizer에서는 canonical
  문자열 기준이고, final gate에서는 렌더링된 문자열 기준이다. 원문 offset과 더하지 않는다.
- 원문 보존/TEXT 경로는 원래 전각 표기를 반환한다. 숫자 문법이 거부한 값을 ASCII로
  치환한 채 보존하는 동작은 하지 않는다.

## 보존과 이번 범위 밖

- `` `１２３원` ``, `/tmp/１２３.txt`, 태그 속성, annotated 원문은 폴딩하지 않는다.
  경로 보호와 streaming의 path 대기 검사는 전각 숫자 구성요소를 인지한다.
- `[[１２３]]{@preserve}`는 원문 유지, `[[１２３]]{별칭}`은 명시 별칭을 쓴다.
  `@digits`·`@address` 등의 **명시 숫자 입력은 기존 ASCII-only 계약**을 유지한다.
  예를 들어 `[[１２]]{@digits}`는 EINVAL이다. 이 변경은 자동 TN의 숫자 스캐너 범위다.
- `２０２３-０２-２９`, `１２,３４원` 같은 기존 숫자 문법의 오류도 원문 보존한다.
- 전각 구두점·전각 단위 문자는 이번에 해석하지 않는다. 숫자 필드에 붙은 전각
  `＋－，．／：％～`는 `syntax.width-punctuation`으로 보호하여 금액 일부를 잘못 읽지 않는다.
  예: `１２，５００원`, `$12，500`, `０９：０５`, `１２３％`는 그대로다. 전각 쉼표를 쓰려면
  ASCII 쉼표로 교체한다. 이것은 임의 Unicode 숫자 문법을 모두 검사한다는 뜻이 아니다.
- 위 첨자 `¹²³`, 아래 첨자 `₁₂₃`은 원문 보존한다. 수량·지수·각주를 구별할 정책이 필요하다.
- 식별자로 거부되는 `２０F 타워`처럼 ASCII 입력도 자동 읽지 않는 문장은 원래 전각 표기를
  유지한다. 자동 TN이 인식하지 않는 모든 숫자 표기를 읽게 만드는 변경은 아니다.

## 참조의 출처와 변경 이력

`tests/evaluation/numeric_width_v22.jsonl` 30건은 구현 전에 작성한 프로젝트 정책 참조다.
외부 benchmark나 독립 언어 검수 정답으로 주장하지 않는다. text/IR, 변형·부정 입력,
코드 보호, KO/EN, 임의 바이트 분할을 검사한다.

- 최초 파일 SHA256: `36fdb7ea36831007e80c666353c0269091ce36f0437e27d9323c889ae47dafca`.
- 현재 SHA256: `b74babf8fd0e070994d7e950d2085334d1147da93a21648242e515db62ff57b0`.
- `width-invalid-group`의 expected_spans.surface를 `１２,３４원`에서 `１２,３４`로 정정했다.
  **변경 전 ASCII baseline**도 TEXT `12,34` + WORD `원`으로 분리한다는 근거다.
  기대 출력 텍스트는 변경하지 않았다. 최초 파일은 artifacts의 references-initial.jsonl에 보존했다.
- 이전 미지원 한계 참조 2건을 구현 전에 명시적으로 승격했다:
  `char-width-digits-not-folded`: `１２３` → `백이십삼`,
  `numeral-char-v21-fullwidth-open`: `１２３원` → `백이십삼 원`.
  기존 참조와 사유는 artifacts의 prior-*.jsonl, intentional-changes.json 및 reference_reviews에 기록했다.
- 전각 구두점 부정 8건은 방어 구현 전에 고정했고 native 검사에도 포함했다.
  ASCII와 전각의 의미 동등성 비교 64건은 metamorphic 검사이며 독립 언어 정답 수에 더하지 않는다.

## 재현

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -R '^numeric_width_'
python3 tools/quality/tn_cycle.py cycle \
  --baseline artifacts/tn/numeric-width/baseline \
  --candidate artifacts/tn/numeric-width/candidate \
  --corpus tests/evaluation/numeric_width_v22.jsonl \
  --out artifacts/tn/numeric-width/NEW-cycle
```

비교 근거는 `artifacts/tn/numeric-width/`에 있다. 고정 snapshot은 덮어쓰지 않는다.
참조30건의 텍스트 exact는 **7→30**, IR 포함 계약은 **6→30**, 계약 개선24/회귀0; 기존 2,474건 중 위2건을 제외하면
text/span/trace 및 declaration_order가 모두 같다. 임의 byte 분할 1,244회와 64개
숫자 폭 변형 검사, 원문 override, 긴 경로 streaming을 검증한다.

동일 Debug binary 조건의 교대7회·각3반복, 생성/로딩/trace 제외 처리 시간 중앙값:

| 기존 입력 workload | baseline | candidate | 비율 |
|---|---:|---:|---:|
| 한국어 안내문 13,776byte | 80.19ms | 83.43ms | 1.040 |
| KO/EN·ASCII 숫자 9,000byte | 134.16ms | 138.32ms | 1.031 |

측정 전 고정한 1.25배 한도 이내다. 전각 숫자 변형 12,000byte는 정보용으로 124.63ms이며
입력 처리 경로가 다르므로 ASCII와의 속도 우위를 주장하지 않는다. 전체 CTest 및 sanitizer
최종 결과는 [HANDOFF](HANDOFF.md)에 기록한다.
