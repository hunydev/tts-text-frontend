# 영어 실제 문장 (소유자 v2, 2026-10-15)

프로젝트 소유자가 준 `tn-eval/en-US-v2.zip`으로 영어를 실제 문장에서 쟀다.

- **자료:** scb-mt-en-th-2020 영어 쪽(CC BY-SA 4.0)이다. 라이선스 때문에 저장소에는 넣지 않고 `benchmark/generated/en-owner/`에만 둔다.
  - `en-US-v2`: 실제 문장 398개, 채점 구간 861개, 정답 있음.
  - `en-real`: 실제 원문 1만 3,105문장, 정답 없음.
- **평가기:** `benchmark/evaluators/owner_v2_eval.py run|score --lang en-US`.
  - 소유자 셋 채점기(`user_tn_eval.py`)의 접기·정책 변형·글자 이름 변형을 그대로 쓴다.
  - 태국어 v2(`--lang th-TH`)도 같은 방식으로 잰다. 결과는 `thai_tn_eval.py --set owner2`와 같다(96.1%).

## 결과

| | 첫 측정 (고정 코드 b39e1f0) | 이번 작업 후 |
|---|---|---|
| v2 구간 | 88.2% | 95.8% |
| v2 문장 | 81.4% | 92.2% |
| 소유자 셋 dev / test | 95.9% / 88.5% | 95.9% / 88.8% |
| 실제 원문: 오류, 숫자가 남은 문장 | 0, 0.3% | 0, 0.2% |

v2는 이제 개발용이다.

## 고친 것 (실제 문장에서 여러 번 나온 꼴)

- **인치 표기 (상품 규격):**
  - 8-3/8"Dia. → eight and three eighths inch diameter, 36"L → thirty six inch long, 8"x 7" → eight inch by seven inch, 3/8" → three eighths inch.
  - 따옴표가 닫는 인용 부호인 경우(`"I have 5" and left`)는 앞쪽 따옴표 수로 가려 그대로 둔다.
- **규격의 x:**
  - 한쪽만 띄운 x(97.6x 57.1x 112.8 cm)는 by로 읽는다. 마지막 항은 단위와 함께 읽는다.
  - 대문자 한 글자 사이(W x H)도 by다.
- **날짜 범위:**
  - 15 to 30 June 2013 → the fifteenth to the thirtieth of June. 전에는 첫날만 기수였다.
  - March 4 to 15 → March fourth to the fifteenth.
  - July-September → July to September.
- **범위·분수:** 띄운 en dash(3.0 – 4.0 percent)는 to로 읽는다. 대분수 2 1/2는 two and a half다.
- **빗금:** 25 sheets/pack, 10/Pack(ten per pack), 30 pins / pack, cm./each는 per로 읽는다(`per_word`에 묶음 단위 낱말 추가).
- **모델번호 (코어):** 글자가 든 코드 바로 뒤 하이픈(EP-001-1, LC6-40mm)은 마이너스가 아니다.
  - 전에는 "sixminus forty"였다. 하이픈은 그대로 두고 뒤의 40mm는 단위로 읽는다.
  - -5, x-3=7은 그대로 마이너스다.
- **전화 (코어):** 띄어 쓴 북미 3-3-4 꼴(888 555 3232)은 단서 없이도 전화번호로 읽는다(1-800 꼴과 같은 예외).
- **도메인 (코어, 영어):** 라벨 안에서 대소문자가 바뀌면 나눠서 각각 읽는다. BeautyExpert.com → Beauty Expert, TBDress.com → T B Dress. 전에는 통째로 철자했다.
- **금액:** $53.7-million → fifty three point seven million dollars. a $1-billion takeover → one billion dollar(꾸밈말). $US6 million → six million u s dollars, $A → australian dollars.
- **그 밖:** Q4/2010 → Q four twenty ten, .SPX → dot S P X, Pkg·Qty·Hwy·sqm, 384,400Km.

## 남은 것

- 대문자만 이어 쓴 도메인(MICHAELKORS.com, RIVERISLAND.com, KLOOK.com)은 낱말 경계를 몰라 철자한다.
- 문자 은어(2da, 4U), 오타 시각(7:5pm), 승패 기록(46-15 forty six and fifteen), 미국 정치 약어(R-Colo.).
- v2는 이제 개발용이다. 다음 측정에는 새 실제 문장 셋이 필요하다.

계약 `en_real_v94`(23). 게이트: The other five languages on the owner sets (outputs identical), Korean quick, JSUT, ja-tn-eval, Chinese and Vietnamese gates are unchanged; the English benchmark gains 12 sentences; all contracts pass except the three known failures.
