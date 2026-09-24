# 미등록 영어 단어 추측 발음 측정 (2026-09-24)

## 측정 대상

한국어 문맥에서 자주 나오지만 발음 사전에 **없는** 영어 단어 28개다. 기대 표기는 일반적인
외래어 표기를 독립적으로 작성했고 사람 검수는 대기 상태다. 이 단어들을 사전에 추가하는 것은
개선으로 세지 않는다 (remaining-work의 PRON-01 조건).

- 측정 세트: `tests/evaluation/oov_guess_v21.jsonl` (28건)
- 회귀 게이트: `tests/evaluation/oov_guess_regression_v21.jsonl` (28건 전부 승격)

## 결과

| 시점 | 정확 일치 |
|---|---|
| 변경 전 | 0 / 28 |
| 1차 (문맥 g2p + coda_r) | 14 / 28 |
| 2차 (구개음·이중모음 꼬리) | 20 / 28 |
| 3차 (받침 조건 + l 겹침 + 연결 r) | 28 / 28 |

## 1차: 문맥 있는 자소 규칙

영어 G2P 프로토타입이 단어 시작(`^`)과 끝(`$`) 문맥, 묵음 표시(`-`)를 지원하게 하고
`ss`, `ce`, `sion`, `age$`, `ing$` 같은 규칙을 데이터로 추가했다. 한국어 매퍼는 모음 뒤 `r`를
외래어 표기 관행대로 지우게 했다 (`phone_mapping ko-KR coda_r drop`).

## 2차: 구개음과 이중모음

- 구개음(`JH CH SH ZH`) 뒤의 삽입 모음은 `으`가 아니라 `이`다 (package 패키지, message 메시지).
- `SH`는 뒤따르는 모음을 구개음화한다 (shopping 쇼핑, session 세션).
- 이중모음의 꼬리 음절이 받침을 가져간다 (point 포인트, design 디자인).

## 3차: 받침 조건, `l` 겹침, 연결 `r`

세 규칙 모두 자원으로 선언하고 매퍼가 조건을 해석한다.

- **어말 파열음의 받침**: `P T K`는 **짧은 모음 뒤에서만** 앞 음절을 닫는다
  (market 마켓, 하지만 update 업데이트·report 리포트). 짧은 모음 목록은
  `phone_vowel_length ko-KR AA|AE|AH|EH|IH|UH short`다. 비음·유음 앞에서도 닫지 않는다
  (partner 파트너).
- **`l` 겹침** (`phone_mapping ko-KR onset_l double`): 모음 뒤의 `l`은 앞 음절의 받침 ㄹ과
  자신의 초성 ㄹ로 두 번 적힌다 (platform 플랫폼, delivery 딜리버리, daily 데일리).
- **연결 `r`** (`phone_mapping ko-KR linking_r link`): `ER` 뒤에 모음이 오면 r는 사라지지 않고
  다음 음절의 초성이 된다 (delivery 딜리버리, battery 배터리). 자음 앞·어말에서 지우는
  `coda_r drop`의 반대쪽 규칙이다.

자소 규칙은 `^con`, `nnel`, `^lice`, `nse$`, `ersi`, `on$`, `ake$`를 추가했다.

## 기대값을 고친 기존 참조 3건

프로토타입의 당시 출력을 그대로 적어 둔 참조였고, 새 출력이 표준 외래어 표기에 더 가깝다.
각 케이스의 `notes`에 변경 사유를 남겼다. 사람 검수는 여전히 대기 상태다.

| 케이스 | 이전 | 이후 |
|---|---|---|
| `handle-v21-neg-no-underscore` (snake) | 스내크 | 스네이크 |
| `foreign-v21-guess-default` / `-text-mode-guess` (daily) | 데이리 | 데일리 |
| `initialism-v21-word-policy-it` (IT, word 정책) | 이트 | 잇 |

## 확인 범위

Debug CTest 122건 통과, sanitizer 단독 실행, 외부 영어 표본
(`benchmark/generated/sproat-v21-oovguess`)은 직전 실행과 수치가 같다 — 한국어 전용 변경이라
영어 출력은 바뀌지 않는다. 사전 항목을 늘려 이 목록을 통과시키지 않았다.
