# 미등록 영어 단어 추측 발음 측정 (2026-09-24)

## 측정 대상

한국어 문맥에서 자주 나오지만 발음 사전에 **없는** 영어 단어 28개다. 기대 표기는 일반적인
외래어 표기를 독립적으로 작성했고 사람 검수는 대기 상태다. 이 단어들을 사전에 추가하는 것은
개선으로 세지 않는다 (remaining-work의 PRON-01 조건).

- 측정 세트: `tests/evaluation/oov_guess_v21.jsonl` (28건, 게이트 아님)
- 회귀 게이트: `tests/evaluation/oov_guess_regression_v21.jsonl` (현재 맞는 14건)

## 결과

| 시점 | 정확 일치 |
|---|---|
| 변경 전 | 0 / 28 |
| 변경 후 | 14 / 28 |

변경 내용은 두 가지다. 영어 G2P 프로토타입이 단어 시작(`^`)과 끝(`$`) 문맥, 묵음 표시(`-`)를
지원하게 하고 `ss`, `ce`, `sion`, `age$`, `ing$` 같은 규칙을 데이터로 추가했다. 한국어 매퍼는
모음 뒤 `r`를 외래어 표기 관행대로 지우게 했다 (`phone_mapping ko-KR coda_r drop`).

## 남은 실패 14건

```
update: expected 업데이트, got 어프데이트
content: expected 콘텐트, got 칸텐트
design: expected 디자인, got 디잔이
shopping: expected 쇼핑, got 소핑
delivery: expected 딜리버리, got 디리버이
market: expected 마켓, got 마케트
license: expected 라이선스, got 라이스느스
version: expected 버전, got 버선
platform: expected 플랫폼, got 프래트폼
channel: expected 채널, got 채넬
point: expected 포인트, got 폰이트
session: expected 세션, got 세선
message: expected 메시지, got 메시즈
package: expected 패키지, got 패키즈
```

원인은 크게 모음 질(`o`가 항상 AA), 어말 파열음의 받침 처리(`마케트` vs `마켓`), 그리고
`-tion`/`-sion` 외의 접미사다. 사전 항목을 늘려 이 목록을 통과시키지 않는다. 외부 영어 표본의
결과 파일은 이 변경으로 바뀌지 않았다.
