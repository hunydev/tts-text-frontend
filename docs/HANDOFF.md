# 현재 작업 인수인계

갱신: 2026-10-11. Release **0.20.0**, public ABI **v1**.

## 최근 인계: 태국어 운영 전환 (2026-10-15)

- 프로젝트 소유자 결정으로 th-TH를 개발에서 **운영**으로 바꿨다(`tools/web/language_support.json`). 이제 여섯 언어가 모두 운영이다.
- **근거:**
  - 실제 문장 v2 96.1%, held-out v1 96.0%, 소유자 셋 test 94.7%.
  - 실제 원문 오류 0.
  - TTS→STT에서 원문 45.3% → TN 73.3%, 새 TN 문제 없음.
- **남은 확인:** 사람 원어민 검수, VoxCPM2 청취 비교. 상세: [thai-pack.ko.md](thai-pack.ko.md) "운영 전환".

## 최근 인계: 태국어 청취 비교 (2026-10-15)

- 운영 전환 판단용 TTS→STT A/B. 소유자 실제 문장 v2 371문장으로 했다.
- 결과: 원문 45.3% → TN 73.3%(구간 62.9% → 83.0%). 새로 드러난 TN 문제는 없다.
- TN 악화 23건은 STT의 숫자·약어 표기와 잘못 들음이다. 숫자 주변 공백을 붙인 실험도 차이가 없었다(잡음 수준).
- 상태는 아직 **개발**이다. 운영 전환은 소유자 결정을 기다린다. 상세: [thai-pack.ko.md](thai-pack.ko.md) 끝 절.

## 최근 인계: v2 소유자 결정 반영 (2026-10-16)

- **한국어 영문+숫자:** 두 자리 이상 숫자는 한국어(웰컴 이천이십육, 아이피 육십팔), 긴 코드는 한국어로 한 자리씩(케이이 공팔일), 한 자리는 영어(케이세븐). 코드 안 하이픈은 마이너스가 아님.
- **일본어 kWh:** キロワットアワー.
- **주소 단어:** 사전 확장만(ko·ja 메일함 단어, vi 성조 없는 단어). 사람 이름은 철자.
- **상세:** [owner-v2-four.ko.md](owner-v2-four.ko.md)의 "소유자 결정" 절.

## 최근 인계: 4개 언어 합성 v2 (2026-10-15)

- **자료:** 소유자 합성 v2(ko/ja/zh/vi 각 400문장). `benchmark/generated/owner-v2-src`(로컬)에 있고 `owner_v2_eval.py --lang`으로 잰다. 이 수정 뒤로는 dev 셋이다.
- **점수(구간):** ko 92.0→94.0, ja 90.7→94.7, zh 96.7→98.6, vi 94.5→96.9. 엔진 오류 0.
- **고친 것:** 언어별 팩 규칙과 단위·신호어, 코어 3곳(전각 통화기호 토큰, 수식 피연산자의 천 단위 쉼표, 수식 연산자 뒤 공백에서 분절 금지).
- **소유자 결정 대기:** 주소 계정명 단어 읽기 vs 철자, 한국어 영문+숫자 라벨, 14팀 고유어.
- **상세:** [owner-v2-four.ko.md](owner-v2-four.ko.md).

## 최근 인계: 영어 실제 문장 (2026-10-15)

- **자료:** 소유자 v2(실제 문장 398개)와 실제 원문 1만 3,105문장. `benchmark/generated/en-owner`(로컬)에 있고 `owner_v2_eval.py`로 잰다.
- **점수:** v2 88.2% → 95.8%. 소유자 셋 test 88.8%. 실제 원문 오류 0, 숫자가 남은 문장 0.2%.
- **고친 것:**
  - 영어 팩: 인치, 규격 x, 날짜 범위 서수, 빗금 per, 금액 단위, 모델번호.
  - 코어: 코드 뒤 하이픈은 마이너스가 아님, 북미 띄어 쓴 전화번호, camelCase 도메인.
- **상세:** [english-real-sentences.ko.md](english-real-sentences.ko.md).

## 최근 인계: 태국어 팩 4단계 (2026-10-15)

- **소유자 데이터:** `benchmark/generated/th-owner`에 둔다(로컬 전용).
  - 실제 문장 정답 셋 v2: `--set owner2`.
  - 실제 원문 1만 4,927문장: 정답 없음. 오류와 남은 숫자를 세는 데 쓴다.
  - 고유명사 목록: CC0. `tools/thai/proper_names.py`로 `names.tsv`를 만든다.
- **점수:**
  - v2: 87.5% → 96.1%(한국어 요청 95.9%).
  - 실제 원문: 오류 12 → 0, 숫자가 남은 문장 1.7% → 0.6%.
  - 소유자 셋 test 94.7%.
- **소유자 결정:** 홀로 쓰인 대문자 약어는 태국 글자 이름으로 읽는다. 2G~6G·4K·8K는 영어 숫자로 읽는다.
- **주의:** `names.tsv`처럼 모든 엔진이 싣는 리소스는 행 수가 엔진 생성 시간에 바로 반영된다. 4만 6천 행에서 context_cache_byte_splits가 30 → 83초가 됐다.
- **다음:** v2도 이제 개발용이다. 다음 측정에는 새 실제 문장 셋이 필요하다. 상세: [thai-pack.ko.md](thai-pack.ko.md).

## 최근 인계: 베트남어·태국어·중국어 제어 구문 (2026-10-15)

- **문제:** 세 팩은 제어를 받지 못해 원문을 그대로 냈다(`1234{@digits}` → `1234`).
- **수정:** 공용 제어 문법이 만든 객체(`layer=USER`)를 팩이 자기 표기로 다시 써서 자기 분류기로 읽는다(`ttf_control_reread`). `@email`·`@url`은 전자 주소 렌더러를 쓴다. 한국어 전용 예약어는 무시하고 평소대로 읽는다.
- **모든 언어:** `v1.2.3{@version}`·`15%{@percent}`가 묶지 않아도 붙는다.
- **계약:** `control_packs_v92`. 게이트: The owner-set outputs of all six languages are identical; Korean quick, English, JSUT, ja-tn-eval, Chinese and Vietnamese gates are unchanged; all contracts pass except the three known failures.
- **상세:** [explicit-pattern-controls.ko.md](explicit-pattern-controls.ko.md) 끝 절.

## 최근 인계: 태국어 팩 3단계 (2026-10-15)

- **held-out v1:** `benchmark/datasets/th-tn-eval`. 첫 측정 81.5%(한국어 요청 71.4%) → 94.6%(두 요청 같음). 이제 개발용이다.
- **팩:** 수식, 시간 길이, 요일·띄운 범위, 계좌·우편·항공편 번호, ทับ(บ้านเลขที่, 혈압), ~ ประมาณ.
- **코어:**
  - 태국 숫자는 모든 요청에서 숫자로 읽는다.
  - 태국 글자 뒤 마침표는 문장 끝이 아니다.
  - 잘린 긴 문장은 앞 단위의 태국 글자로 판정한다.
  - 태국 문장에서는 태국 팩만 분류한다.
- **점수:** 소유자 셋 dev 99.2%, test 93.9%. 계약 `th_pack3_v91`.
- **게이트:** The other five languages on the owner sets (outputs identical), Korean quick, English, JSUT, ja-tn-eval, Chinese and Vietnamese gates are unchanged; all contracts pass except the three known failures.
- **다음:** 일반화를 다시 재려면 새 held-out v2가 필요하다. 남은 오답은 정책(홀로 쓰인 영어 약어)과 사전(태국 고유어 로마자) 쪽이다. 상세: [thai-pack.ko.md](thai-pack.ko.md).

## 최근 인계: 태국어 팩 2단계 (2026-10-14)

- **영어 낱말 태국어 표기:** `electronic_word` 139개. 이메일·URL·@핸들·파일 이름과 수에 붙은 낱말(SAVE50, COVID-19)에 쓴다. 홀로 쓰인 영어 낱말은 그대로.
- **그 밖:** 문서·차량 번호(นร 0505/ว 212, กข 1234), 1 1/2 + 분류사, 08/2027·11/11.
- **점수:** dev 95.6% → 98.9%, test 91.6% → 92.5%. 계약 `th_pack2_v90`.
- **게이트:** The other five languages on the owner sets (outputs identical), Korean quick, English, JSUT, ja-tn-eval, Chinese and Vietnamese gates are unchanged; all contracts pass except the three known failures.
- **다음:** 남은 dev 오답은 태국 고유어 로마자(somchai), 날짜 범위의 ถึง(코어 목록 표시), microSD·USB-C. 상세: [thai-pack.ko.md](thai-pack.ko.md).

## 최근 인계: 태국어 팩 1단계 (2026-10-14)

- **시작:** 프로젝트 소유자 요청. 소유자 평가셋 th-TH(1,000문장)를 `benchmark/datasets/user-tn-eval`에 넣었다(dev 700 / test 300).
- **팩:** `languages/th-TH`. 숫자·날짜·시각·돈·단위·범위·번호·코드·약어. 영어 낱말은 그대로 둔다.
- **코어:** 태국 문자 문장은 어느 요청에서든 th-TH로 간다. 태국 숫자 ๐-๙를 숫자로 본다.
- **점수:** dev 0.4% → 95.6%, test 91.6%. 계약 `th_pack1_v89`.
- **게이트:** the other five languages unchanged on the owner sets, Korean quick, English, Japanese, Chinese and Vietnamese gates unchanged.
- **다음:** 영어 낱말의 태국어 표기(이메일·URL 라벨, 제품명)가 남은 오답의 대부분이다. 상세: [thai-pack.ko.md](thai-pack.ko.md).

## 최근 인계: 소유자 평가셋 7단계 (2026-10-14)

- **범위:** 남은 dev 오답 가운데 일반화되는 형식만(소유자 승인). 기존 참조와 충돌하거나 문맥 없이는 판단할 수 없는 것은 남겼다.
- **점수(구간, dev/test):** ko 96.0 / 91.8, en 95.9 / 88.5, ja 96.7 / 92.2, zh 98.7 / 96.1, vi 98.4 / 94.8. dev 손실 0. 계약 `user_eval7_v88`.
- **게이트:** English bench speech +11 and text +11 against the references, ja-tn-eval direct +22 (ours unchanged), zh held-out 3 +2 and 5 +1, Korean quick +2, Vietnamese unchanged.
- **고정 계약 갱신:** `ja_pack_v36`·`ja_rest_v50`·`ja_rules_v43`의 조수사 뒤 目(Was: いっかい目).
- **main 버그 수정:** 베트남어 `Mỗi ngày 2-3 lần`이 날짜(ngày hai tháng ba)로 읽히던 문제.
- **다음:** test가 거의 움직이지 않는다. 코드를 고정하고 새 held-out 셋으로 일반화를 확인하는 단계가 맞다. 상세: [benchmark/datasets/user-tn-eval/README.md](../benchmark/datasets/user-tn-eval/README.md).

## 최근 인계: 베트남어 영어 수준 맞추기 (2026-10-14)

- **계기:** 지원 표에서 베트남어가 영어보다 범주가 적다는 소유자 질문. 영어 범주로 직접 돌려 보니 실제 빈 곳과 버그가 있었다.
- **버그(main에도 있음):** 한글·한자·가나가 섞인 베트남어 문장이 영어 약어처럼 철자로 읽히고 성조 글자가 사라졌다. 고쳤다.
- **메운 것:** 베트남어 기호·이모지 이름(fallback 파일 누락), 약어 46개, 원문자, a.m./SA/CH, 비율, 문장 끝 `$15 triệu.`
- **함께 고친 것:** 한국어·일본어 문장 속 악센트 낱말(Café, Pokémon, phở)이 글자를 잃던 문제.
- **그대로:** 대문자 이니셜(WHO, GDP)은 그대로 둔다.
- 계약 `vi_parity_v87`. 게이트: user eval, Korean quick, English, Japanese, Chinese and Vietnamese gates unchanged. 상세: [vietnamese-pack.ko.md](vietnamese-pack.ko.md) "영어 팩 수준 맞추기".

## 최근 인계: 소유자 평가셋 1단계 (2026-10-14)

- **평가셋:** 소유자가 만든 언어별 1,000문장(ko·en·ja·zh·vi)을 `benchmark/datasets/user-tn-eval`에 넣었다.
  - 원본은 그대로 둔다. dev 700 / test 300으로 나눴고 수정은 dev 오답에서만 고른다.
  - 채점기는 `benchmark/evaluators/user_tn_eval.py`다. 정책 동치와 줄별 보완(`amend.tsv`)은 README에 있다.
- **1단계 수정:**
  - 공통: U+2212 마이너스, ja/zh 전각 표기, en dash 범위
  - 소유자 정책 4건: 한국어 영하, 중국어 등식의 乘以, 베트남어 phần tư, 영어 범위 to
  - 계약 `user_eval1_v81`(19)
- **점수(구간, dev/test):** ko 91.0/90.1, en 84.0/85.0, ja 84.9/83.3, zh 93.6/95.6, vi 86.3/87.3이다. 1단계 전보다 0.5~2.6%p 올랐다.
- **다른 게이트:** 영어·일본어·중국어·베트남어 기존 셋 변화 없음.
  - 한국어 quick은 1건이 정답과 달라졌다. 참조는 `마이너스 십삼도`, 출력은 `영하 십삼도`로 소유자 정책에 따른 변화다.
- **2단계(같은 날):** 언어별 코드·번호·단위·수식을 고쳤다. 계약 `user_eval2_v82`(27).
  - 점수(구간, dev/test): ko 91.3/90.4, en 90.4/87.6, ja 87.5/84.6, zh 94.5/95.6, vi 90.5/90.1
  - 영어 벤치는 speech·text 각 1건이 참조와 달라졌다. ZIP 26278과 PIN 503165를 한 자리씩 읽는 소유자 정책 때문이다.
  - 나머지 게이트는 변화가 없다(일본어 1308/1344, 중국어 9셋, 베트남어 held-out3·6 각 100%·99.7%).
- **고정 계약 갱신 1건:** `ko_list_break_v24`의 `-3도`를 영하 정책에 따라 `영하 삼도`로 바꿨다(Was: 마이너스 삼도).
- **3단계(같은 날):** 날짜·촉음·부호·베트남어 표기. 계약 `user_eval3_v83`(19).
  - 소유자 결정: 101번째 백한 번째(`ordinal_policy large mixed`, 끝자리 1~9만 고유어). test_korean_counters 기대값을 바꿨다(Was: 백일 번째, 천일 번째).
  - 점수(구간, dev/test): ko 92.3/90.4, en 91.5/87.9, ja 92.3/89.1, zh 95.4/95.8, vi 92.9/90.6
  - 일본어 채점 규칙 보정(ー를 히라가나 표기대로)으로 모든 단계를 다시 쟀다(README 표).
  - 기존 게이트는 main과 같다. 한국어 quick은 참조 일치 +7이다.
- **4단계(같은 날):** 단위 읽기 버그, 경과 시간, 부호, 베트남어 번호. 계약 `user_eval4_v84`(22), 테스트 `shared_unit_korean_readings`.
  - 점수(구간, dev/test): ko 93.6/90.7, en 92.6/88.2, ja 93.2/89.3, zh 96.2/96.1, vi 97.4/94.2
  - 기존 게이트는 3단계와 같다. 한국어 통화 소수 끝 0 생략은 한국어 참조 61건과 충돌해 되돌렸다.
- **5단계(같은 날):** 코드·시간 범위·연호·요일·수식. 계약 `user_eval5_v85`(28).
  - 점수(구간, dev/test): ko 94.2/90.7, en 94.7/88.5, ja 94.5/90.9, zh 97.3/96.1, vi 97.5/94.5. dev 손실 0, 획득 51.
  - 공통 수정: 단서 없는 1-800 번호, 0으로 시작하는 4-3-3 번호(0120-456-789)를 전화번호로 본다.
  - 고정 계약 갱신: `slash_date_v21` 날짜 뒤 요일 괄호 2건(Was: 구 월 이십오 일(목)까지).
  - 게이트: 한국어 quick 참조 일치 +6/−1(0으로 시작하는 번 번호), 중국어 held-out5 +2, 나머지는 4단계와 같다.
  - 소유자 결정 필요: ja 범위의 첫 조수사(평가셋 정답이 섞여 있고 기존 계약은 첫 수를 조수사 없이 읽는다).
- **6단계(같은 날):** 주소 낱말, 날짜 범위, 혈압, 수식 기호. 계약 `user_eval6_v86`(18).
  - 소유자 결정: ja 범위의 첫 조수사는 현행 유지. 채점에서 첫 조수사 유무를 둘 다 받는다.
  - 점수(구간, dev/test): ko 95.4 / 91.5, en 95.6 / 88.5, ja 96.1 / 92.2, zh 98.0 / 96.1, vi 97.6 / 94.5. 5단계를 같은 채점 규칙으로 다시 잰 것과 비교해 dev 손실 0.
  - 고정 계약 갱신: `electronic_v21`(help·support), `handle_v21`(user·team), `zh_pack3_v66`(10w+了 十万加).
  - 게이트: English bench speech +2, text +1 against the references; Korean quick, Japanese, Chinese and Vietnamese gates unchanged.
- **다음 후보:** 이름·로마자 주소 라벨(minsu, hana), ko 0으로 시작하는 주소 숫자(myhome2026), en IP 옥텟과 Boeing 계열, vi 단위 뒤 /. 남은 dev 오답은 언어당 10~38건이다.


- **결정:** 프로젝트 소유자 결정으로 vi-VN을 **운영**으로 바꿨다.
- **근거**
  - held-out 새 셋 점수가 2단계부터 97% 안팎이다(v6 96.8%).
  - OpenAI TTS→STT 257건에서 원문 63.8% → TN 93.8%였다.
- **전환 전 수정:** 비교에서 드러난 TN 문제 6건(đ/USD, euro, +금액, MM/YY 유효기간, 띄어 쓴 계좌번호, 12.12)을 고쳤다. 계약 `vi_prod_v80`.
- **남은 확인:** 사람 원어민 검수, VoxCPM2 청취 비교. 상세: [vietnamese-pack.ko.md](vietnamese-pack.ko.md) "운영 전환과 남은 확인".

## 최근 인계: 위치 표시 제어 {@mark} (2026-10-13, main 반영)

- **구문:** annotated 입력의 `{@mark}`·`{@mark:cough}`는 읽기를 바꾸지 않는 위치 표시다(프로젝트 소유자 요청, 기본안).
  - 출력 글자는 없다. 앞뒤 공백은 하나로 합친다.
  - 앞 낱말에 붙지 않는다. `[[…]]{@mark}`, 빈 이름·대문자 이름은 잘못된 제어다.
- **콜백:** 표시 자리에 길이 0 span 하나가 생긴다(`rule=control.mark`, `unit=@mark`, `value=이름`).
  - 콜백은 `segment->spans`에서 `unit == "@mark"`를 찾아 `begin` 위치에 효과음을 넣는다.
  - 세그먼트 경계는 바꾸지 않는다.
- **테스트:** `tests/integration/test_mark_control.py`(29건)
  - 배치와 스트림 3가지 분할(4096·7·1바이트)에서 같은 텍스트·span·위치
  - 잘못된 표시 8종, plain 입력
- 문서: [input-format.md](input-format.md) "Mark control".

## 최근 인계: 베트남어 팩 6단계 (2026-10-13, main 반영)

- **고친 것:** held-out v5 실패 유형($3,500, tr.đ, lúc 22g, hạn 뒤 d/m, 월/년 범위).
- **측정:** held-out v6(코드 고정 뒤 처음 연 257문장)에서 **96.8%**다. 세 요청 언어가 같다.
  - 새 셋 점수는 93.2 → 97.1 → 95.6 → 97.1 → 97.7 → 96.8이다. 2단계부터 97% 안팎에서 멈췄다.
- **측정 뒤 버그 수정:** 단서는 온전한 낱말이어야 한다(khuyến mãi ≠ mã). "앞 절 ngày" 규칙은 점 날짜에만 쓴다.
- **다음 제안:** held-out 따라가기는 이득이 작다. 운영 전환 판단(사람 원어민 검수, VoxCPM2 또는 OpenAI TTS→STT 청취 비교)으로 넘어가는 편이 낫다. 상세: [vietnamese-pack.ko.md](vietnamese-pack.ko.md) "6단계".

## 최근 인계: 베트남어 팩 5단계 (2026-10-13, main 반영)

- **고친 것:** held-out v4 실패 유형(1tr5, k 범위, 앞 절 ngày의 점 날짜, mã nguồn, số·cổng 식별자, µg·kN).
- **측정:** held-out v5(코드 고정 뒤 처음 연 256문장)에서 **97.7%**다(영어·한국어 요청 97.1%).
  - 새 셋 점수는 v1 93.2 → v2 97.1 → v3 95.6 → v4 97.1 → v5 97.7이다.
- **측정 뒤 버그 수정:** 10억 단위 금액(`1.250.000.000`)이 IP로 읽히던 2단계 버그를 고쳤다.
- **6단계 후보:** `$3,500`, `125 tr.đ`, `22g`, 단서 없는 `5/11`, 월/년 범위. 상세: [vietnamese-pack.ko.md](vietnamese-pack.ko.md) "5단계".

## 최근 인계: 베트남어 팩 4단계 (2026-10-13, main 반영)

- **넓힌 범주**
  - 과학·IT 단위 전반
  - 치수 nhân, ÷ 범위
  - ISO·점 날짜, 기념일 단서
  - QL1A, 4N3Đ, 짧은 번호, I·V·X 로마 숫자
- **측정:** held-out v4(코드 고정 뒤 처음 연 260문장)에서 **97.1%**다.
  - 새 셋 점수는 v1 93.2 → v2 97.1 → v3 95.6 → v4 97.1이다.
  - 세 요청 언어가 같고, 다른 언어 게이트는 그대로다.
- **5단계 후보:** 1tr5, k 금액 범위, Mã nguồn 오인, µg·kN, 단서 없는 코드. 상세: [vietnamese-pack.ko.md](vietnamese-pack.ko.md) "4단계".

## 최근 인계: 베트남어 팩 3단계 (2026-10-13, main 반영)

- **고친 것:** held-out v2 실패 유형.
  - mã 단서는 절 안에서만 본다.
  - 1m05, en dash 음수, 35 tr, nm, 4.0, triệu ha
- **정책:** 금액 뒤 /낱말은 một으로 읽는다(단위/단위는 trên).
- **측정:** held-out v3(코드 고정 뒤 처음 연 258문장)에서 **95.6%**다.
  - 새 셋 점수는 v1 93.2 → v2 97.1 → v3 95.6이다.
  - 남은 실패는 흩어져 있다. 다음은 held-out 하나씩 따라가기보다 단위·날짜 형식을 범주 단위로 넓히고, 사람 원어민 검수와 VoxCPM2 청취로 확인하는 편이 낫다.
- 상세: [vietnamese-pack.ko.md](vietnamese-pack.ko.md) "3단계".

## 최근 인계: 베트남어 팩 2단계 (2026-10-13, main 반영)

- **고친 것:** held-out v1 실패 유형.
  - 날짜 범위·하이픈 날짜
  - 1m78, P./Q., CN, IP, 긴 코드
  - 금액의 단위어·trên·단위/단위, mmHg, 의정 번호
  - 분절기는 수의 처음부터 분류한다.
- **측정:** held-out v2(코드 고정 뒤 처음 연 257문장)에서 **97.1%**다. 세 요청 언어가 같다. 다른 언어 게이트는 그대로다.
- **3단계 후보:** mã(종목) 오인, 1m05, en dash 음수, 35 tr, nm, 4.0, đồng/tháng의 một·mỗi. 상세: [vietnamese-pack.ko.md](vietnamese-pack.ko.md) "2단계".

## 최근 인계: 베트남어 팩 1단계 (2026-10-13, main 반영)

- **팩:** `languages/vi-VN`, 북부 표준 읽기.
  - 숫자: nghìn·linh·mốt·lăm, 점은 천 단위, 쉼표는 소수점
  - 날짜·시각·돈·단위·서수·로마 숫자·번호
- **코어:** `latin_sentence`가 베트남어 글자가 있는 라틴 문장을 vi-VN으로 보낸다. 분절기는 U+1E00–U+1EFF와 조합형 부호를 라틴 문자로 본다.
- **측정:** held-out v1(코드 고정 뒤 처음 연 240문장)에서 vi-VN 93.2%다.
  - 영어·한국어 요청은 단위 경계 수정 뒤 같은 수치다.
  - 다른 언어 게이트는 모두 그대로다.
- **검수:** Claude 원어민 검수와 OpenAI gpt-5.4-mini 교차 확인(불일치만 본다). 비용은 1달러 미만이었다.
- 상세: [vietnamese-pack.ko.md](vietnamese-pack.ko.md).

## 최근 인계: 혼합 문장 2차 검토 (2026-10-13)

- **중국어 문장 속 이메일·URL:** 요청 전체가 `invalid resource`로 실패하던 문제를 고쳤다.
  - 새 kind `address_symbol`로 `abc 艾特 qq 点 com`처럼 읽는다.
  - 읽을 수 없는 주소는 원문을 남긴다.
- **작은따옴표 인용:** 한글 없는 문장에서 공백이 든 `'…'`을 문맥 창으로 본다.
- **코드 포인트:** `U+20A9`를 철자로 읽는다(ko 유 플러스 이공에이구 / en / ja).
- **영어:** 1M requests, requests/day, 단독 ≥≤, 24/7, v2.1 version.
- **한국어 낱말:** 잉글리시·테크·세트·레젝스.
- **유지한 것:** 조사는 바꾸지 않는다(좌등는). regex는 레젝스이고 정규식으로 옮기지 않는다.
- **확인:**
  - 계약 `mixed_lang2_v73`(32)
  - 한국어 quick: changed 0 (142,255문장)
  - 영어·JSUT·ja-tn-eval·중국어 9개 세트: 그대로
- 상세: [cjk-korean-reading.ko.md](cjk-korean-reading.ko.md) 6.8.

## 최근 인계: 중국어 운영 전환 (2026-10-13)

- 프로젝트 소유자 결정으로 zh-CN을 베타에서 **운영**으로. 근거: held-out v8 97.0%, OpenAI TTS→STT 200건에서 원문 72.5% → TN 96.0%(새로 드러난 TN 문제 없음).
- 남은 확인: 사람 원어민 검수, VoxCPM2 청취 비교. 상세: [chinese-pack.ko.md](chinese-pack.ko.md) "운영 전환과 남은 확인".

## 최근 인계: 한국어 벤치마크 마지막 단계 Decisions (2026-10-12)

- 추가: 같은 문장을 Jev로도 채점(`tools/web/ko_jev.py`). 둘 다 틀림 2,388문장(1.68%). `/ko-quality`에서 "Jev·Decisions 모두 틀림" + 점수 정렬(평균·각 판정기, 오름·내림). 평균 오름차순이 실제 오류부터 보여 준다(주문번호를 금액으로, `P`를 `포인트`로).

- 한국어 참조 벤치마크의 `공백 외 차이` 문장에만 OpenAI Decisions를 붙임(`tools/web/ko_decisions.py RUN` → `decisions-final.json`). `/ko-quality`에 검정(Decisions 인정)·회색(Decisions까지 틀림) 막대, 카드·열·필터.
- 20261012-mixed: 공백 외 차이 21,687 중 인정 18,595, Decisions까지 틀림 3,092문장(2.17%). 판정은 양쪽으로 틀리므로 거친 지표.
- 표본에서 본 엔진 쪽 의심: `15시` → `열다섯 시`, `50개입` → `쉰 개입`, 번호 끝자리 조사 단서(`7395으로` 칠삼구다섯, `9831를` 구팔삼하나).
- OpenAI 키는 `~/.config/ttf/openai_api_key`. 상세: [korean-benchmark-representative.ko.md](korean-benchmark-representative.ko.md) 5절.

## 최근 인계: 벤치마크 마지막 단계 Jev, 영어부터 (2026-10-12)

- `/jev-eval`(모든 문장을 Jev로 채점) 제거. 대신 참조 벤치마크의 불일치 문장에만 Jev를 붙여 "Jev까지 틀림"을 본다.
- 영어: `tools/web/en_jev.py RUN` → `jev-final.json`. `/en-quality`에 색 막대(초록·노랑·주황·빨강=벤치마크 최종, 검정=Jev 인정, 회색=Jev까지 틀림)와 카드·필터.
- 측정(20261012-mixed): speech Jev까지 틀림 568문장(2.96%), text 860문장(4.48%). 영어 Jev는 보정 안 됨 → 하한값.
- 한국어는 불일치가 수만 문장이라 아직 적용하지 않음. 상세: [english-vs-wetext.ko.md](english-vs-wetext.ko.md)

## 최근 인계: 한·영·중·일 혼합 문장 (2026-10-12)

프로젝트 소유자가 준 혼합 문장으로 찾은 빈틈을 고쳤다. 계약 `mixed_lang_v72`(41건).
- 한자·가나 독음 옵션이 온전한 중국어·일본어 문장까지 글자 단위로 바꾸던 것: 이제 한국어 문장 안과 낱말에만 적용([cjk-korean-reading.ko.md](cjk-korean-reading.ko.md) §6.7).
- 간체자·신자체 627자에 정자 독음, 한글이 있는 문장 안에서만(中国チーム에도 중국). 손으로 쓴 대응표, 자주 쓰는 글자만. 한글 없는 `中国 日本`은 고정 계약대로 `중国 일본`.
- zh-CN·ja-JP 요청에서 한국어·영어 문장의 숫자·영문이 요청 언어로 읽히던 것(오전 九시, エーアイ)([matrix-language.ko.md](matrix-language.ko.md)).
- 한국어 문장 속 `I'll`이 `I'l l`로 깨지던 것.
- 영어 요일·월·이름 87개를 한국어 사전에 추가(Friday 프라이데이, John 존, Alex 알렉스. Was: 프리데이, 조흔, 앨렉스). 이메일·URL 안의 이름은 계정명이므로 철자 그대로(`address_spelled`, john.doe@… 제이오에이치엔).
- 한국어·영어·JSUT·ja-tn-eval·중국어 평가셋 변화 없음.
- 남은 결정: 한국어 문장 속 일본 인명(佐藤)은 독음 옵션에서 한국 한자음(좌등)이 된다. 일본어 읽기(사토)는 인명 사전이 필요하다.

## 최근 인계: 중국어(간체) 팩 1단계 (2026-10-11)

- 0단계: 자체 평가셋 `benchmark/datasets/zh-tn-eval`(322문장)로 wetext 85.5%, NeMo 66.8% 측정. [chinese-phase0.ko.md](chinese-phase0.ko.md)
- 1단계: `languages/zh-CN` 팩(베타)과 한자 문장 라우팅(`han_sentence`, `src/core/normalize.c`). 계약 `zh_pack_v64`(54건).
  - 평가셋: 요청 언어 zh-CN 100%, 한국어·영어 요청 속 한자 문장 98.0%. 같은 작성자라 과적합 포함.
  - held-out 평가셋(496문장, 팩을 보지 않은 작성자, 측정 전 정답 고정): 우리 87.5%, wetext 75.5%, NeMo 60.1%. 실제 성능은 이쪽.
- 2단계: held-out v1 실패 유형과 엔진 버그 2개(로마자 많은 문장의 표식 누출, 전각 쉼표)를 고침. 계약 `zh_pack2_v65`(52건).
  - 최종 측정 held-out v2(500문장, 코드 고정 뒤 검수): 우리 90.2%(1단계 81.3%), wetext 72.3%, NeMo 53.9%. v1은 이제 개발용.
  - 남은 유형과 원문보다 나빠지는 버그 5건은 [chinese-pack.ko.md](chinese-pack.ko.md)에 있다.
- 3단계: held-out v2 남은 유형('/' 복합 단위, 분·초 기호, 양사, 날짜, 번호, 아래첨자)과 원문보다 나빠지던 버그. 계약 `zh_pack3_v66`(39건).
  - 최종 측정 held-out v3(496문장, 코드 고정 뒤 검수): 우리 92.2%(2단계 89.9%), wetext 72.8%, NeMo 55.4%. v2는 이제 개발용.
  - 남은 56구간과 새로 드러난 나빠지는 버그 11건은 [chinese-pack.ko.md](chinese-pack.ko.md)에 있다(4단계 후보).
- 4단계: held-out v3 남은 유형과 나빠지던 버그 11건. 계약 `zh_pack4_v67`(35건).
  - 최종 측정 held-out v4(500문장): 우리 90.6%(3단계 90.1%), wetext 70.9%, NeMo 54.9%. 사례별 수정의 일반화 이득이 거의 없어짐(+0.5%p).
  - 다음은 범주 단위(IT 표기, 로마·그리스 기호, SI 단위 전체, 번호 문맥)로. 상세는 [chinese-pack.ko.md](chinese-pack.ko.md).
- 5단계(범주 단위): IT 표기, 로마·그리스 기호, SI 단위, 번호 문맥. 계약 `zh_pack5_v68`(35건).
  - 최종 측정 held-out v5(487문장): 우리 92.4%(4단계 88.1%, +4.3%p), wetext 66.2%, NeMo 49.9%. 범주 단위가 사례별보다 잘 일반화됨.
  - 남은 유형(식 속 단위, 표준번호, 금액, 전화 단서 범위)은 [chinese-pack.ko.md](chinese-pack.ko.md).
- 6단계(v5의 실패 범주): 식 속 단위, 표준·인증 번호, 버전, 금액 표기, 전화 단서 축소. 계약 `zh_pack6_v69`(32건).
  - 최종 측정 held-out v6(491문장): 우리 92.5%(5단계 91.4%, +1.1%p), wetext 71.6%, NeMo 53.8%. 새로 틀린 구간 없음.
  - 남은 유형(공백 뒤 로마자 단위, 二/两, 콜론, 날짜)은 [chinese-pack.ko.md](chinese-pack.ko.md).
- 7단계(v6의 실패 범주): 공백 뒤 단위, 새 단위, 约, 二/两 양사, 콜론, 날짜 표기, 서비스 번호·16진수·공사 구간. 계약 `zh_pack7_v70`(42건).
  - 최종 측정 held-out v7(491문장): 우리 96.1%(6단계 93.2%, +2.9%p), wetext 67.5%, NeMo 49.9%. 새로 틀린 구간 없음. 작성 지시에 공백 단위를 넣어 단위 이득은 과대일 수 있음.
  - 남은 유형(단서 없는 점 월-일, 하이픈 점수, 문맥 번호, 식 뒤 공백 단위)은 [chinese-pack.ko.md](chinese-pack.ko.md).
- 8단계(v7의 실패 범주): 점 월-일, 하이픈 점수, 문맥 번호, 단위·기호, 두 자리 연도. 계약 `zh_pack8_v71`(27건).
  - 최종 측정 held-out v8(495문장, 중립 지시문): 우리 97.0%(7단계 96.9%, +0.1%p), wetext 73.6%, NeMo 55.5%. 새로 틀린 구간 없음.
  - 남은 오류가 범주마다 1~2건으로 흩어짐. 다음은 큰 실제 텍스트 표본에서 빈도로 고르는 편이 낫다([chinese-pack.ko.md](chinese-pack.ko.md)).
- 자원 해시 버킷 1021 → 8191(`TTF_RESOURCE_BUCKETS`, 엔진은 힙에 둠). 조회 한 번에 평균 24행을 훑던 것이 줄어 숫자 3만 개 처리 45초 → 12초. 출력 불변.
  - 한국어·영어·JSUT·일본어 평가셋: unchanged (Korean quick 0 changed sentences, English 90.83%/86.80%, JSUT 203/332, ja-tn-eval 97.3%).
  - 남은 것: held-out 평가셋, 원어민 검수, VoxCPM2 청취 비교. 상세: [chinese-pack.ko.md](chinese-pack.ko.md)

## 이전 인계: 일본어 평가셋과 한국어 발음열 출력 (2026-10-08~10)

- **일본어:** 자체 평가셋 `benchmark/datasets/ja-tn-eval`(1,250개 항목·채점 span 1,343개, 작성자 원어민 검토)을 만들고, 찾은 빈틈을 계약 `ja_range_mark_v44`~`ja_rest_v50`으로 고쳤다.
  - 평가셋 점수: 우리 97.3%, wetext 76.9%, NeMo 73.6%
  - 2026-10-11 프로젝트 소유자 결정으로 **운영** 전환. 원어민 검수와 VoxCPM2 청취 비교가 남아 있다(OpenAI TTS→STT 비교는 원문 70.0%, TN 85.0%).
  - 상세: [japanese-benchmark.ko.md](japanese-benchmark.ko.md)
- **한국어 발음열 출력(G2P):** 옵션 `pronunciation=hangul`, 제어 `{@pronounce}`. 구어 기준 한글 발음열이다.
  - 표준발음법 규칙, 표준국어대사전 예외(`pronunciation/stdict_phonemes.tsv`, CC BY-SA 2.0 KR), 활용형, 동형어 문맥 단서, 합성어를 처리한다.
  - 말뭉치 200문장 원어민 검토: 문장 94.5%, 어절 99.4%. 계약 `ko_phoneme_v51`~`v59`(211건). 한 자리씩 읽은 숫자(112 일릴리)와 원자 제어 `X{@pronounce}`도 지원한다.
  - 사용자 결정으로 여기서 마무리했다. 상세와 남은 한계는 [korean-phoneme.ko.md](korean-phoneme.ko.md)에 있다.
- **사전 재생성:** `python3 -I tools/korean/stdict_phonemes.py --library build/libtts_text_frontend.so --resources resources --data benchmark/generated/stdict-20260906/data/pronunciations.tsv --archive benchmark/generated/stdict-20260906/stdict.zip`
  - 원본은 로컬 전용이다.
  - 출력 파일을 손으로 고치지 않는다. 고칠 때는 생성기 목록(`MAIN_READINGS`, `VERB_FORMS`, `NOUN_ONLY`, `RULE_HOMOGRAPHS`)을 바꾼다.
- **옵션을 끈 출력:** 한국어·영어·JSUT 벤치마크가 모두 변하지 않았다.

## 이번 인계: 엔진 2차 개선 — 한국어 명백한 오류·영어 형식 (2026-09-30)

브랜치 `claude/engine-round2`(작업 트리 `~/ttf-r2`)를 `tools/ci/run.sh full`(gcc·clang·sanitizers·
text-evaluation) 통과 후 `main`에 fast-forward 병합하고 `build-web`과 서비스를 갱신했다.
항목마다 계약(`tests/evaluation/*_v23.jsonl`)을 구현 전에 작성했고, 한국어·영어 전 자료 회귀를
확인한 뒤 커밋했다. 전날(09-29)에는 variant 지표 v2, 읽기 단위 띄어쓰기 옵션(`spacing`),
`X{@break:1|2|3}` 재설계를 반영했다.

### 측정 (같은 import·options·evaluator·review 파일, comparison_key 동일)

| 지표 | 20260929-default | 20260930-round2 | 변화 |
|---|---:|---:|---:|
| 한국어 보정 variant | 101,081 / 142,178 (71.09%) | 107,183 (75.39%) | +6,102 |
| 한국어 보정 compact | 88,205 (62.04%) | 91,905 (64.64%) | +3,700 |
| 한국어 읽기 단위 옵션 exact | 57,923 (40.74%) | 59,310 (41.72%) | +1,387 |
| 영어 speech variant (19,208) | 16,025 (83.43%) | 16,042 (83.52%) | +17 |
| 영어 text variant | 15,457 (80.47%) | 15,475 (80.57%) | +18 |

보고서: `ko-release/reports/20260930-round2`, `20260930-round2-reading-unit`,
`en-eval/reports/20260930-round2`, `20260930-round2-textmode`. 엔진 스냅샷
`ko-release/engine-20260930-round2`.

### 항목별 결과 (한국어 새로 맞음 / 새로 틀림, variant 기준)

| 커밋 | 항목 | 효과 |
|---|---|---|
| `fb738a6`, `71ec02f` | 하이픈 식별번호: 선행 0·6자리 이상·길이 불균형 쌍은 범위가 아닌 식별자로 읽음, 하이픈 옆 공백 허용 | +3,331 / −0 |
| `bbafc39` | 차량 번호판 `12가 3456` | +1,163 / −3 |
| `d9bd006` | speech 원문 숫자 누출 3종(미등록 접미사 앞 복합수, `제`+수+명사, 전화 모양 예외 축소) | +524 / −0 |
| `b015ec8` | 한국어 문맥의 숫자+라틴 단위 문자(`5t`, `3P`, `50cc`, `10Km`) speech 한정 | +351 / −0 |
| `f2d7b60` | 옷 사이즈 `44사이즈` → 사사 | +740 / −4 |
| `00c0f53` | 영어 오전/오후 `5pm`, `7 a.m.`, `10.30pm` (붙여 쓴 대문자 `2PM`은 이름으로 유지) | 영어 +6 / +7, 한국어 0 |
| `5eb56d5` | 영어 `1899--seventeen` 이중 하이픈 앞 수 | 영어 +11 / +11, 한국어 0 |

### 못 한 것 / 판단

- 숫자+대문자 단위(`4Km`, `20 KM`)를 공통 접미사로 올리는 일은 보류했다. 영어 자료 1건뿐이고
  한국어 text 모드의 `KG` 회사명과 충돌할 위험이 있다.
- 일반 숫자+문자 읽기(`20F`, `4K`, `45A`)는 사용자가 결정한 라벨 정책과 충돌해서 넣지 않았다.
- 영어 `1861--1865`(수-수 이중 하이픈), `15th March`, `Main St.`는 원문 그대로 둔다.
  `Main St.`는 분할기 설계가 필요하다.
- 새로 발견한 문제(미착수): 영어 `Call Kim at 555-1234.`가 범위로 읽힘, 한국어 음차
  `Terms of Service` → 텀스 **오프**, `Dynamite` → 드내밋.

### 다음: 문장 언어(matrix language) 라우팅 — 설계 초안, 사용자 검토 대기

영어 문장에 한글 단어가 하나만 들어가도 문장 전체가 한국어로 음차된다
(`My friend 민수 bought 3 books yesterday.` → 미 프린드 민수 바우그흐트 …). 기존 벤치마크는
이 경우를 보지 못한다. 설계 초안과 미등록 계약 22건(현재 10/22 일치)은 병합하지 않고 작업 트리에만
있다. 검토 후 들어간다.

## 이번 인계: 야간 작업 — 참조 정리, 영어 평가, 영어·한국어 TN 개선 (2026-09-29)

브랜치 `claude/night-20260928` (작업 트리 `~/ttf-ref`)를 `main`에 fast-forward 병합,
origin push (Actions 워크플로는 꺼져 있음). 모든 항목은 계약(`tests/evaluation/*_v23.jsonl`)을
구현 전에 작성했고, 항목마다 한국어·영어 전 자료 회귀를 확인한 뒤 커밋했다.

### 측정 (자료: AI Hub 159 Training / NeMo 평가셋 + Sproat 표본, held-out 아님)

| 지표 | 시작 (main `1d22c2f`) | 끝 | 변화 |
|---|---:|---:|---:|
| 한국어 원본 공백무시 일치 | 81,991 / 142,248 (57.64%) | 88,205 (62.01%) | +6,214 |
| 한국어 보정 (참조 오류 70문장 제외) | 81,991 / 142,178 (57.67%) | 88,205 (62.04%) | +4.37%p |
| 영어 speech `variant` (19,208문장) | 13,352 (69.51%) | 16,025 (83.43%) | +13.92%p |
| 영어 text `variant` | 13,535 (70.47%) | 15,457 (80.47%) | +10.00%p |

영어 자료별 speech / text `variant`:

| 자료 | speech 전 → 후 | text 전 → 후 |
|---|---|---|
| GoogleTN (7,521) | 84.91 → 85.83 | 80.80 → 81.05 |
| LibriTTS (7,677) | 47.41 → 80.54 | 55.13 → 79.80 |
| EngConf (231) | 23.38 → 26.41 | 44.59 → 45.89 |
| Sproat 표본 (3,779) | 86.58 → 88.01 | 82.64 → 82.80 |

보고서: 한국어 `ko-release/reports/20260928-night` (비교 대상
`20260928-claude-identifier-cleaned`, 같은 검토 파일 `reviews/reference-errors-v1.jsonl`),
영어 `en-eval/reports/20260928-night`, `20260928-night-textmode` (비교 대상
`20260928-claude-identifier`, `-textmode`). 엔진 스냅샷 `ko-release/engine-night-20260928`.

### 항목별 결과 (새로 맞음 / 새로 틀림)

| 커밋 | 항목 | 효과 |
|---|---|---|
| `4515278` | 참조 오류 제거·보정 지표 `adjusted_sentences`, 엔진 무관 검출기 | 70문장 제외(0.05%) |
| `8ec8d90` | 영어 평가: 영어 범위, NeMo importer, `english_sentences.py` | 기준선 측정 |
| `be1e9e4` | a) 렌더러가 못 읽는 토큰은 원문 유지(문장 전체 실패 금지), `'00s` = two thousands | 영어 엔진 오류 1 → 0 |
| `8ec84f1` | b) 영어 speech의 단어 안 하이픈·대시를 "hyphen"으로 읽지 않음 | 영어 speech +709 / −0 |
| `04c0293` | f) 한국어 영문+숫자 코드 (K37PA1798 → 케이 삼칠 피에이 일칠구팔) | 한국어 +3,338 / −46 |
| `8ce9d0c` | c) 로마 숫자: 번호어 뒤 기수, 왕·교황 이름 뒤 the + 서수 | speech +1,522 / −1, text +1,570 / −1 |
| `9af412b` | d) `Month,` 뒤 네 자리 수 = 연도 | speech·text 각 +83 / −0 |
| `0902546` | g1) 카드번호 4-4-4-4 한 자리씩 | 한국어 +1,068 / −0 |
| `4c1dd0f` | g2) 비밀번호·PIN 라벨 뒤 숫자 한 자리씩 | 한국어 +562 / −0 |
| `6bbe282` | g3) 코로나19 → 코로나 일구 (참조 1,591/1,591) | 한국어 +1,292 / −0 |
| `a83583f` | e1) 대문자 일반 단어를 철자로 읽지 않음 (IMPRESSIONS) | 영어 speech +89 / −0 |
| `21d2927` | e2) St./Mt./Ft. + 이름 → Saint/Mount/Fort | speech +270, text +269 / −0 |

새로 틀린 문장의 내용:
- f) 46문장 모두 참조가 소수 정책을 따른 경우다: 버스 노선을 값으로 읽음 31
  (`M4108번` → 엠사천백팔 번; 데이터 전체는 한 자리씩 661 : 값 49), 식별번호 0을 `영`으로 읽음 9,
  모델명을 값으로 읽음 6 (`E300` 이삼백, `K311`).
- c) `Baldwin I of Constantinople`: GoogleTN 참조가 `I`를 그대로 둔다. 우리 출력 the first.
- 한국어 출력이 바뀐 문장은 전부 해당 항목의 모양을 포함함을 확인했다
  (f 4,841, g1 8,413, g2 873, g3 1,589; b는 한국어 문장 안 `GTX-A` 3문장, 판정 변화 없음).

### 못 한 것 / 되돌린 것

- e2의 도로 유형 (`Main St.` → Street, `Oak Dr.` → Drive): 구현했으나 스트림 세그먼트가
  `Oak | Dr.`로 나뉘면 앞 도로명이 보이지 않아 batch/stream 결과가 달라져 되돌렸다.
  계획기(`src/segmenter/segmenter.c` `pending_word`)가 뒤 토큰을 보지 않는 설계이므로,
  도로명 뒤 약어를 지원하려면 planner 쪽 설계가 필요하다. 기존 `Oak Dr.` 보존 계약 유지.
- f에서 두 자리 코드(KF94, TMA-12)는 참조가 구십사/구사로 갈려 정책 결정 전 제외.
- 정책 차이로 남긴 한국어 유형 (고치지 않음): 식별번호 0의 영/공, 번호 구분자 에/다시/무음,
  `혼합` 카테고리의 고유어 숫자(하나·둘), 서른/삼십, 버스 노선 값/자리 읽기,
  `10번` 열 번/십 번 (횟수·번호 문맥).

### 검증

- `tools/ci/run.sh full` (gcc, clang, sanitizers, text-evaluation) 통과. 계약 파일
  9개 추가(`*_v23.jsonl`), C fixture `tests/unit/test_verbalize_fallback.c`.
- 회귀 확인 도구는 scratch (`/tmp/quick_ko.py`, `/tmp/quick_en.sh`)로만 두었다.
  정식 보고서는 위 경로.

## 이번 인계: 식별번호 대체 읽기와 하이픈 `다시` (2026-09-28)

브랜치 `claude/identifier-fallback` (작업 트리 `~/ttf-work`), 기준 `3b6db7b`.
**미커밋·미push.** 사용자 검토 후 커밋한다. 자세한 정책·측정은
[identifier-fallback.ko.md](identifier-fallback.ko.md).

- speech 출력에서 전화·범위 인식기가 거부한 순수 숫자·하이픈 열을 식별번호로 읽는다
  (`src/core/normalize.c` `identifier_fallback`/`read_identifier`). IR은 `TEXT` 유지.
  text 출력, 전화번호 모양(`0X`/`0XX`), 영숫자 코드는 대상 아님.
- `identifier_render / ko-KR / separator` → ` 다시 `. 기존 계약 10행 갱신
  ([원장](audit/identifier-separator-policy-20260928.json)). `korean-range-again`
  검사에 명시적 식별번호 선언 예외를 추가했다 (self-test 5건).
- 검증: `identifier_fallback_v22.jsonl` 19/19, GCC Debug 전체 CTest 195/195.
  sanitizer 전체와 clang은 이번에 돌리지 않았다.
- AI Hub 159 참조 (Training, held-out 아님): 문장 공백무시 일치 80,523 → 81,991
  (+1,468; 새로 맞음 1,722 / 새로 틀림 254). 대체 읽기 +1,005 (손실 0), 하이픈 `다시`
  +463 (손실 254 — 참조가 하이픈을 읽지 않은 문장). 보고서
  `20260928-claude-{base,separator,identifier}`.
- 재현:
  ```sh
  cd ~/ttf-work && cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j2
  ctest --test-dir build -R 'identifier_fallback|reference_integrity|phone|identif' --output-on-failure
  ```
- 다음: 영숫자 식별번호 (`26-6573557-L97`, `32124a`) 약 700문장.

## Git 체크포인트: 누적 작업 일괄 커밋 (2026-09-28)

사용자가 현재까지의 전체 워크트리 커밋과 `main` push를 승인했다. 이 체크포인트는
아래 각 작업에서 남긴 미커밋 코드·리소스·테스트·문서를 함께 보존한다.
아래의 “미커밋”, “commit/push 없음”은 각 검증 당시 상태를 기록한 과거 설명이다.

- 한국어/영어 리소스 보강: 조사·단위 경계, 횟수, 전화번호·선행 0, 전각 숫자,
  점 포함 약어·문자 읽기, 공통 발음 사전과 선택형 고객 읽기.
- 구간 제어와 의미 유형 지정: 한국어 주소, 이메일·URL, 추가 명시 패턴,
  `{@break:1|2|3}` 위치 끊기와 스트리밍·길이 제한 계약.
- 한국어 참조 importer/evaluator, 사전 후보 수집, 공백 차이 진단,
  카테고리별 웹 리포트·전체 사례 모달, 정책 변경 원장과 후속 작업 큐.
- 마지막 엔진 검증: GCC 전체 최초193/194, 실제 회귀 수정 후 영향15/15;
  Clang ASan+UBSan 영향12/12; 새 계약62건·1,740 byte 분할 및 웹 검사 통과.
  상세 실행 기록과 남은 한계는 다음 절에 있다. 이 커밋 정리에서 엔진 코드는
  추가 변경하지 않았다.

한국어 참조 원본·가공 데이터·문장별 결과·실행 캡처는 기존 정책대로
`benchmark/generated/`, `artifacts/`에 로컬 보존하며 Git에 추가하지 않는다.
새 clone에는 이 파일들이 없으므로 [한국어 참조 평가 가이드](korean-reference-benchmark.ko.md)의
가져오기·재평가 절차를 따른다. 기존 리포트의 `git_head`/dirty 표시는 측정 당시
provenance이므로 커밋 후에도 다시 쓰지 않는다. 이 VM의 웹 배포 상태는 유지한다.

## 이번 인계: 나이·차수·학년 경계 / 위치 끊기 제어 — 완료 (미커밋)

사용자 요청: 나이·생년월일의 큰 공백 격차와 제시한 `15세인 … 1차 예약`을
일반 패턴으로 보강. 추가로 끊기 강도1=공백,2=쉼표+공백,3=마침표+공백을 구현.
기존 dirty 작업 보존, ABI v1 유지, 음성 평가/commit/push 없음.

### 구현과 편집 위치

- KO `counters.c`에서 기존 숫자 스캐너와 COUNTER/RANGE IR을 재사용한다.
  `15세인 → 십오 세인`, `1차 예약 → 일 차 예약`, `3학년에 → 삼 학년에`.
  `counter_noun_unit`으로 세/차/학년, `counter_noun_tail`로 정확한 조사·어미를 선언한다.
  `counter_noun_attached / 차=1`은 공백 뒤의 일반 명사 ‘차’를 차수로 오인하지 않는다.
  `counter` 읽기 table과 Customer 우선순위 유지. Core의 numeric_suffix는 변경하지 않았다.
- 정수/선행0 수량/전각/범위 지원. `세대/세탁기/차원/학년제`, 원래 한글, 소수·음수
  확대는 이번 범위 밖. 부정 예와 명시 Customer 비활성화 검사가 있다.
  [단위 경계와 읽기 정책](korean-spacing.ko.md).
- annotated에서 위치 구문 `{@break:1|2|3}` 추가. 정확히 ` ` / `, ` / `. ` 삽입.
  앞 단어는 정상 TN, 기존 공백·부호는 보존한다. 앞 단어 길이는 marker10바이트 한도에
  포함되지 않는다. plain/보호코드/리터럴 별칭에서는 재해석하지 않는다.
- 별도 CUSTOM IR, `rule=control.break`, `unit=@break`, `value=1|2|3`, 원문 marker byte 범위,
  명시 입력 trace. 3단계는 자동 sentence_split 설정과 별개로 해당 위치에서 callback을
  확정한다. 1·2단계는 문자 삽입이며 강제 callback이 아니다. [규격](input-format.md).
- 웹 예제6개/새 corpus32+30개 등록. 공백 진단의 `categories`별 집계를 추가해
  나이·생년월일 등 범주를 고르고 전체 사례 모달로 확인할 수 있다. 이전 진단은 전체만
  표시한다. COUNTER의 일 단위 경계를 숫자 ‘일’의 내부 간격과 구별하도록 진단도 보강했다.

### 재평가 / 참고값과의 차이

`reports/20260928-spacing-v1` → `reports/20260928-counted-noun-v3`。
142,255문장 중142,248채점 /272,325구간, 같은 comparison_key.

| 지표 | 이전 | 이후 |
|---|---:|---:|
| 전체 문장 정확 일치 | 18,182 (12.78%) | 18,977 (13.34%) |
| 전체 문장 공백 제거 일치 | 80,669 (56.71%) | 80,523 (56.61%) |
| 나이·생년월일 정확 일치 /5,439 | 119 (2.19%) | 511 (9.40%) |
| 나이·생년월일 공백 제거 일치 | 5,054 (92.92%) | 4,999 (91.91%) |
| 전체 구간 정확 일치 | 80,457 | 83,189 |
| 전체 구간 공백 제거 일치 | 174,610 | 176,414 |

변경4,831문장 중4,606은 공백만 변경. 정확 일치 신규796/상실1,
공백 제거 신규2/상실148. 전체 구간 정렬 실패39,771→37,750.
기존 참조와 입력은 그대로이며 엔진 오류0/미채점7/구조 경고1,349 유지.

참조·입력·평가 프로토콜 그대로, Training 참조 일치도이며 held-out 정확도가 아니다.
원본/실제 문장은 ignored 경로에만 둔다. 값/단위의 의미 정확도를 참조 일치율과 혼동하지 않는다.

- 이전 age_birthdate 공백 차이4,935문장 중 진단상4,461문장(중복 범주)이 수사 내부
  간격을 포함했다. `십오`와 `십 오`를 무조건 같게 채점하거나 기본 수사를 분해하지 않는다.
- 새 수량 범위는 기존 정책에 따라 `15~19세 → 십오 세에서 십구 세`로 읽는다.
  참조가 끝 단위만 쓰면 공백 제거 일치에서도 달라진다. compact 상실148문장 전부
  이 범위 계열을 포함한다. 평가에서 제외하지 않았다. `range_repeat_unit / 세=0`
  등의 명시 고객 정책으로 끝 단위만 선택 가능하며 기본 점수와 섞지 않는다.
- 정확 일치 상실1건은 아파트 ‘6차’의 육차/육 차 표기 차이(traffic/003432).
  숫자값은 같고 공백 제거 일치는 유지한다. 고유명 관용 붙임은 별도 검토 대상이다.
- 사용자 사례 age_birthdate/000043은 나이/차수 경계를 고쳤지만 `코로나19`의
  십구/일구는 여전히 별도 관용명 읽기 문제다. `[[코로나19]]{코로나 일구}`는 사용할 수
  있다. 모든 한글+숫자에 자리 읽기를 적용하지 않았으며 미해결을 완료로 세지 않는다.

### 검증과 변경 이력

- 새 계약32+30 **62건**,2분절 설정 **1,740 byte schedules**의 text/span/trace 동일.
  Customer 비활성화·native 읽기와 3단계 write 중 callback을 확인했다.
  malformed18종 batch/1byte stream, marker정확한10byte한도(긴 앞 단어 포함),
  인접/리터럴 구문과 invalid resource3종 검사 통과.
- age cycle:17→32, fixed15/regressed0. break의 이전 엔진은 새 문법을 거부하여
  tn_cycle가 before 단계에서 exit2로 종료했다(`cycle-break/before.log`). 이를 통과한
  전후 cycle로 보고하지 않는다. 새 문법은 고정30건과 strict/stream/기존 corpus로 검증했다.
- 기존2,656건 비교: 정책 갱신한 `2학년 → 이 학년`1건 외 회귀0. 선택 행·같은 layer
  상대 순서 유지. 이전 기대와 이유는 `docs/audit/counted-noun-policy-20260928.json`.
- v1 외부 평가에서 `0987 차는`의 차량 번호를 차수로 오인한 실제 결함을 발견했다.
  진행 중 GCC를 중단하고 v1 로그/report/snapshot을 보존했다. v2 수정 전에 독립 부정
  계약2개를 추가했으며 차수는 숫자에 붙은 형태만 허용한다. v1/v2는 웹에 발행하지 않았다.
- GCC 전체194개 중193개 최초 통과(819.84초). `user_pattern_byte_splits`의 기존 길이 한도 계약1개가 실제 회귀를 찾았다. 불완전한 break prefix의 캐시를 해제해 별칭으로 재판별하도록 수정하고, 영향15개를 **15/15** 재검증(53.06초)했다. 기대값/제한시간 변경 없음. `gcc-ctest-v2.log`, `gcc-recheck-v3.log`, `gcc-final-status.json`을 함께 본다.
- Clang ASan+UBSan 영향 범위 **12/12** 통과(161.49초). GCC 후 순차 실행, GCC/Clang compiler warning 0. 전체 sanitizer suite 재실행을 뜻하지 않는다.
- 한글/숫자 혼합 두 workload 교대7라운드×3회 처리 시간비 **0.979× / 1.003×**, 1.25× 게이트 통과. 평가/빌드와 동시에 측정하지 않았다.
- 후보 CLI/library hash 확인 후 원자적 교체·service 재시작. 웹 API 예제11개 text/parity, 웹 corpus **62/62** 통과. Playwright 메인/품질 페이지 검사 모두 통과: 범주별 공백 진단, 전체 사례 모달, 키보드/모바일/XSS, streaming callback 포함.
- 참조 구조 감사61파일2,019건 결함0, 독립 언어 검수 완료라는 뜻은 아니다.

### 산출물과 재현

`artifacts/tn/age-break/`: `baseline`, `candidate-v3` 불변 snapshot.
`candidate`는 v1, `candidate-v2`는 길이 한도 수정 전, `cycle-age-v3`, `existing-corpora-v3.json`, `external-comparison-v3.json`,
`audit-baseline`, `audit-candidate-v3`, `streaming-v2.log`, `errors-v2.log`,
`gcc-ctest-v2.log`, `gcc-recheck-v3.log`, `asan-ctest.log`, `throughput-v3.json`, `deployment.json`, `previous-web/`.
최초60건 고정본과 v2 수정 전 부정2건을 더한 고정본, `pre-change-tests/`도 보존했다.
완료 보고서는 `reports/20260928-counted-noun-v3`, 공백 진단은 sibling `spacing/`에 있다.

```sh
python3 tools/quality/tn_cycle.py cycle --baseline artifacts/tn/age-break/baseline \
  --candidate artifacts/tn/age-break/candidate-v3 \
  --corpus tests/evaluation/counted_noun_v22.jsonl --out artifacts/tn/NEXT/cycle
python3 tests/integration/test_age_break.py build/libtts_text_frontend.so resources
python3 tests/integration/test_break_errors.py build/ttf resources
python3 benchmark/evaluators/korean_release.py \
  --import-dir benchmark/generated/ko-release/import-v1 \
  --library artifacts/tn/age-break/candidate-v3/libtts_text_frontend.so \
  --resources artifacts/tn/age-break/candidate-v3/resources \
  --out benchmark/generated/ko-release/draft-reports/NEXT --label '변경 설명'
python3 benchmark/evaluators/spacing_audit.py \
  --report benchmark/generated/ko-release/reports/REPORT_ID \
  --out benchmark/generated/ko-release/spacing/REPORT_ID
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -j1
# GCC가 끝난 뒤에만 실행. 정확한 12개 -R 목록은 보존된 script에 있다.
bash artifacts/tn/age-break/run-sanitizer.sh
TTF_PLAYWRIGHT_MODULE=/tmp/ttf-web-browser/node_modules/playwright \
  /tmp/ttf-web-browser/node_modules/node/bin/node tests/web/korean-quality.cjs http://127.0.0.1:8000
TTF_PLAYWRIGHT_MODULE=/tmp/ttf-web-browser/node_modules/playwright \
  /tmp/ttf-web-browser/node_modules/node/bin/node tests/web/browser.cjs http://127.0.0.1:8000
```

최종 웹 engine/report identity:
- library SHA256 `17173a71247c1d3418d57e4603c596599de2b085f547fb7d88b602d6c3765db2`
- resources SHA256 `4bafb21cf61325616769b939ccb028d387766ba7724f90db06ad4826c1d674af`
- comparison_key `59c137e84281cd26929f3c1dabe2ff29a07c44c99b44ca6f8021fcb7c39eba6b`

다음 요청에서는 날짜의 수사 내부 간격을 맞춤법/발화 선호로 분리 검토하거나,
코로나19 같은 한글+숫자 관용명에 사용할 사전 경로를 별도 범위로 설계한다.
참조 전체를 맞추기 위해 날짜의 숫자 내부를 일괄 분리하거나 고유 명칭을 일반 숫자
규칙으로 바꾸지 않는다. 기존 `range_repeat_unit` 선택형 평가도 자동 기본 점수와 구분한다.

---

## 이전 인계: 한국어 수 묶음 띄어쓰기 / 공백 차이 진단 — 완료 (미커밋)

사용자 요청: 공백만 다른 비율을 먼저 줄이고, 필수 표기와 참조의 발화 간격·허용
표기 차이를 구분한다. 첫 반복은 **만 단위 수 묶음**으로 제한했다. 기존 dirty 작업
보존, C ABI v1 유지. [정책·리소스 가이드](korean-spacing.ko.md).

### 구현과 경계

- KO `cardinal()`이 0이 아닌 만·억·조 등의 묶음 사이에 공백을 생성한다.
  `12,500원 → 만 이천오백 원`, `1억2천3백만원 → 일억 이천삼백만 원`.
- `numeric.tsv`의 `number_spacing / ko-KR / group = space`. 값은 `space|compact`만
  허용하며 고객 overlay로 기존 붙임 정책을 선택한다. 행 없는 과거 root는 compact.
  Core/API 변경 없이 pack scalar renderer를 재사용하는 금액·단위·범위에도 적용한다.
- 수사 내부 `십이/열두`, `유월/시월`, 전화·선행 0 자리 읽기, 영어, 리터럴 별칭,
  입력 한글 문장은 일괄 교정하지 않는다. 음성 생성/청취 평가는 하지 않는다.
- 새 `benchmark/evaluators/spacing_audit.py`는 완료 보고서/DB hash를 확인하여
  공백만 다른 문장의 경계를 Unicode 문자 좌표와 엔진 UTF-8 byte span으로 연결한다.
  수 묶음/수사 내부/번호 자리/단어 내부/단위·접사/서식/나머지 경계로 검토 후보를
  구분한다. 자동 언어적 오류 판정이나 점수 보정기가 아니며 유형별 문장 수는 중복된다.
- `/ko-quality`에 **공백 차이의 종류** 표와 전체 사례 모달을 추가했다.
  `/api/ko-spacing`은 별도 sidecar를 읽으며 보고서 hash 불일치/경로 이탈을 거부한다.
  원본 보고서와 참조는 그대로이며 진단 없는 과거 보고서에는 미생성 안내를 보인다.

### 동일 외부 자료 재평가

`reports/20260928-phone-zero-v2` → `reports/20260928-spacing-v1`.
142,255문장 중142,248채점 /272,325구간, 같은 comparison_key.
Training 참조 일치도이며 독립 held-out 정확도가 아니다. 참조 수정·제외 없음.

| 지표 | 이전 | 이후 |
|---|---:|---:|
| 문장 정확 일치 | 16,967 (11.93%) | 18,182 (12.78%) |
| 문장 공백 제거 일치 | 80,669 (56.71%) | 80,669 (56.71%) |
| 공백만 다른 문장 | 63,702 | 62,487 |
| 구간 정확 일치 | 77,113 | 80,457 |
| 구간 공백 제거 일치 | 174,610 | 174,610 |

변경16,459문장 전부 공백 제거 문자열·참조·입력·의미 IR 동일.
문장 정확 일치 신규1,215 / 상실0. 구간 정렬 실패39,771 / 구조 경고1,349 /
미채점7 / 엔진 오류0 유지. 공백 진단 대상에서 수 묶음 경계 차이6,342→0,
전체 공백 경계175,599→169,257. 공백 외 불일치가 있는 문장은 이 진단에서 제외되므로
전체 수 묶음의 언어적 완전성이나 전체 corpus 정확도를 뜻하지 않는다.

### 계약·감사·게이트

- 구현 전 고정한 새30건 통과. cycle:15→30 / fixed15 / regressed0.
  2분절 설정의608 byte schedules text/span/trace 동일, 고객 overlay2개 통과.
- 기존2,626건 비교: 독립 수 묶음 정책 변경 이외 회귀0. 숫자 값·단위·입력좌표·
  언어·규칙/trace 선택 행과 상대 순서 동일. 공백 때문에 출력 byte 좌표만 이동한다.
  trace 좌표도 공백 제거 prefix가 동일한 위치인지 확인. 행1개 추가로 순번 이동785건.
- 기존 참조74건과 고정 literal18줄의 변경 전후는
  `docs/audit/ko-spacing-policy-20260928.json`. 자동69건은 이전 엔진 IR과 독립
  수 묶음 계산으로 작성했으며 쉼표/명시 통화5건은 같은 규칙으로 별도 검토했다.
  후보 출력으로 정답 생성하지 않았다. 새30건은 구현 전 고정본과 같다.
- GCC 전체190개 중188개 최초 통과(794.06초). 기존 붙임 기대값이 남은
  `korean_pack_edges`, `typed_controls_evaluation`2개는 정책 원장을 갱신하고
  unit fixture만 재빌드하여 재실행 **2/2** 통과(5.55초). 엔진/리소스 재변경 없음.
  최초/재실행 로그와 `gcc-final-status.json`을 함께 보아야 한다.
- Clang ASan+UBSan 영향 범위 **10/10** 통과(93.50초), GCC와 순차 실행.
  compiler warning 없음. 전체 sanitizer suite를 다시 돌렸다는 뜻은 아니다.
- 한글/숫자 혼합 두 workload 교대7라운드×3회 처리 시간비 **0.990× / 0.999×**,
  1.25× 게이트 통과. 평가/빌드와 동시에 재지 않았다.
- 참조 구조 감사59파일1,957건 결함0. 독립 사람 검수 완료를 뜻하지 않는다.
- 후보 CLI/library의 hash를 확인해 원자적 교체, 서비스 재시작 후 보고서/공백
  sidecar2개 발행. API 사례9개 text/parity와 웹 corpus **30/30** 통과.
  Playwright 품질 페이지의 공백 분류/사례 모달·전체 내용·키보드·모바일·XSS, 기존 메인
  UI의 제어/사전/콜백/byte parity 검사 통과. `deployment.json`, `live-web.json`,
  `browser-quality.log`, `browser-main.log`. 이전 바이너리 백업은 `previous-web/`.
- main은 기존 `0012911`; commit/push 없음. 이전 작업까지 포함한 dirty 상태 유지.

### 재현 / 산출물

루트 `artifacts/tn/ko-spacing/`: `baseline`, `candidate` 불변 snapshot,
`references-before-candidate.jsonl` 새 계약 고정본, `pre-policy-tests/` 이전 참조 사본,
`cycle/`, `existing-corpora.json`, `external-comparison.json`, `audit-baseline/`,
`audit-candidate/`, `gcc-ctest.log`, `gcc-rerun.log`, `asan-ctest.log`, `throughput.json`.
대용량 입력/실제 문장과 sidecar는 ignored 경로에만 둔다.

```sh
python3 tools/quality/tn_cycle.py cycle --baseline artifacts/tn/ko-spacing/baseline \
  --candidate artifacts/tn/ko-spacing/candidate \
  --corpus tests/evaluation/ko_spacing_v22.jsonl --out artifacts/tn/NEXT/cycle
python3 benchmark/evaluators/korean_release.py \
  --import-dir benchmark/generated/ko-release/import-v1 \
  --library artifacts/tn/ko-spacing/candidate/libtts_text_frontend.so \
  --resources artifacts/tn/ko-spacing/candidate/resources \
  --out benchmark/generated/ko-release/draft-reports/NEXT --label '변경 설명'
python3 benchmark/evaluators/spacing_audit.py \
  --report benchmark/generated/ko-release/reports/REPORT_ID \
  --out benchmark/generated/ko-release/spacing/REPORT_ID
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -j1
# GCC 종료 후 sanitizer를 단독 실행
cmake -S . -B build-sanitize -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug \
  -DTTF_ENABLE_ASAN=ON -DTTF_ENABLE_UBSAN=ON
cmake --build build-sanitize -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitize --output-on-failure -j1 \
  -R '^(ko_spacing_evaluation|ko_spacing_audit|resource_semantic_validation|korean_pack_edges|semantic_boundary_streaming|numeric_width_evaluation|typed_controls_evaluation|finance_ranges_v7_evaluation|scaled_currency_v12_evaluation|financial_quantity_values)$'
```

최종 library SHA `35289fff69c9a0ad72a6c6eaf2c410c67aadbfdbb8a2c36994a65aa9f9282ca9`,
resources SHA `65dfe40cc9bad52f92eca0bda5b32c1af09c2f68968449d3ed2650cab7e8833e`.
웹 `https://tts-text-frontend.exe.xyz:8000/ko-quality`에서 **만·억·조 수 묶음 띄어쓰기**
보고서를 선택한다. corpus에는 `ko-spacing-v22`가 있다.

다음은 `korean-quality-work-queue.json`의 KO-SPACING을 우선한다. 단위·접사와 나머지
경계의 실제 사례를 검토한 뒤 한 규칙만 고르고, 순서/합성어/조사의 부정 계약을
먼저 작성한다. `십 이/열 두/유 월` 또는 식별자 자리 간격을 단순 점수 향상 때문에
일괄 도입하지 않는다. 발화 간격 프로필, 자동 전역 띄어쓰기 교정, 사람의 판정이
완료된 공백 평가 subset은 후속이다. 남은62,487문장을 해결 완료로 취급하지 않는다.

---

## 이전 인계: 전화번호 꼬리 문맥 / 선행 0 자리 읽기 — 완료 (미커밋)

사용자 요청: `015`에 국한하지 않고 `02`와 영어 전화 형태를 고려하며, 맨 `0046`은
0을 버린 사십육 대신 **공공사육**으로 읽는다. 이번에는 이 두 문법 계열만 변경한다.
이전 작업의 dirty 파일은 그대로 유지한다. public ABI v1 유지, 외부 참조 수정 없음.

### 변경과 정책

- 실제 bypass 원인은 앞자리 whitelist가 아니라 전화 뒤 `번/번호`를 수량 접미사로
  취급한 `src/semantic/phone.c::quantity_tail`이었다. 한국어 `phone_number_tail / 번=1`
  리소스로 기존 전화 구조 뒤의 번호 꼬리를 허용한다. kg 등 수량 방어는 유지한다.
- 공통 `numeric_policy / leading_zero_min_digits=2`, 한국어 `number / leading_zero_zero=공`.
  맨 부호 없는 숫자열은 `DIGIT_SEQUENCE`, `unit=leading_zero`, value는 모든 자릿수 보존.
  영어는 기존 zero 읽기를 사용한다. 임계값7 또는 영 읽기는 Customer overlay로 선택한다.
- 완성 전화번호는 뒤 `만` 조사/`조` 이름 때문에 공유 배율 범위로 해석하지 않는다.
  반면 `01~02만 원`의 01은 수량 계수로 유지한다. 중간 v1의 계수 해석 문제는 v2에서
  수정했으며 v1 snapshot/report는 실패 이력으로 보관하고 발행하지 않는다.
- 날짜/시간/소수/금액/단위/부호/그룹 수량/명시 제어의 의미를 유지한다. 기존 금융 cue와
  일반 선행 0의 IR을 구분한다. `0046{@cardinal}` 또는 `{@sino}`는 annotated에서 사십육.
- [편집·정책·한계](phone-zero-policy.ko.md), [전화 리소스](phone-resources.md).
  웹 `phone-zero-v22` corpus와 전화·숫자 예제4개를 등록했다.

### 재평가 결과 (전체 게이트 후 웹 발행 완료)

`reports/20260928-lexicon-v2` → `reports/20260928-phone-zero-v2`.
같은 원본142,255문장/채점142,248문장/272,325구간, 같은 comparison_key.
Training 탐색용 참조 일치도이며 독립 held-out 정확도가 아니다.

| 지표 | 이전 | 이후 |
|---|---:|---:|
| 문장 엄격 일치 | 16,967 (11.93%) | 16,967 (11.93%) |
| 문장 공백 제거 일치 | 80,318 (56.46%) | 80,669 (56.71%) |
| 구간 엄격 일치 | 77,113 (28.32%) | 77,113 (28.32%) |
| 구간 공백 제거 일치 | 174,388 (64.04%) | 174,610 (64.12%) |
| 전화번호 범주 공백 제거 / 9,110 | 1,404 | 1,661 |

문장 출력 변화10,975: 새 일치361 / 상실10 / 여전히 불일치10,604(미채점1 포함).
기존에 일치하던10문장/13구간의 상실은 모두 영→공 정책 차이이며 원본 그대로 집계한다.
정렬 구간 새 일치235 / 상실13 / 순증222. 구조 경고1,349 / 미채점7 / 엔진 오류0,
구간 정렬 실패39,782→39,771. 평가 소요176.922초는 보고서 작성 시간이다.
전화 그룹의 ‘다시’ 연결과 숫자 읽기 선호 등은 남은 불일치로 보존한다.

변경된 구간도 조사: 선행 0 읽기11,618구간, 이전 invalid-phone에서 복구1,957구간,
전화 뒤 배율 동음 문자로 생긴 잘못된 범위/경계13사례를 별도 확인했다.
일부 식별자와 전화 모양의 모호성 전체를 해결한 것은 아니다. 개인 번호의 화자 선호는
명시 제어/독립 검토 후속으로 두고 자동 점수를 위해 참조를 바꾸지 않는다.

### 계약과 감사

- 새 자체 계약53/53, 2분절 설정에서1,760 byte schedules text/span/trace 동일,
  Customer override4개 통과. `cycle-v2`: 기존25→53, fixed28 / regressed0.
- 기존2,573건 비교: 명시 정책 변화12건 외 회귀0. trace declaration_order 이동3,627,
  선택 행·비순번 필드·같은 layer 안 상대 순서/동률 보존.
- 기존 기대 문자열 변경11건은 `docs/audit/phone-zero-policy-20260928.json`에 전후 기록.
  나머지1건은 영어 긴 숫자열의 IR unit 추가이며 출력은 그대로다. 과거 ID는 유지한다.
- 자체 새 range 부정 참조1건의 ‘다시’는 기존부터 ‘에서’였던 저작 오류다.
  `reference-authoring-correction.json`에 수정 전·후와 baseline 증거를 보관한다.
  v2 공유 배율/전화 꼬리6건은 해당 보완 전에 별도 고정했다.
- 참조 구조 감사58파일1,927건 결함0. 독립 사람 검수 완료라는 뜻은 아니다.

### 재현과 산출물

루트 `artifacts/tn/phone-zero/`. `baseline`과 `candidate-v2`는 불변 snapshot.
`candidate`, `cycle`, `draft-reports/20260928-phone-zero-v1`은 중간안이다.
`pre-policy-tests/`는 기존 참조의 변경 전 사본, `references-before-*`는 새 계약 고정본이다.

```sh
python3 tools/quality/tn_cycle.py cycle --baseline artifacts/tn/phone-zero/baseline \
  --candidate artifacts/tn/phone-zero/candidate-v2 \
  --corpus tests/evaluation/phone_zero_v22.jsonl --out artifacts/tn/NEXT/cycle
python3 benchmark/evaluators/korean_release.py \
  --import-dir benchmark/generated/ko-release/import-v1 \
  --library artifacts/tn/phone-zero/candidate-v2/libtts_text_frontend.so \
  --resources artifacts/tn/phone-zero/candidate-v2/resources \
  --out benchmark/generated/ko-release/draft-reports/NEXT --label '변경 설명'
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -j1
```

비교: `existing-corpora-v2.json`, `external-comparison-v2.json`,
`external-span-audit-v2.json`, `external-review-v2.json`과 실행 스크립트.
원본/실제 문장은 ignored 경로에만 있고 제품 리소스에 옮기지 않았다.

### 완료 게이트

- GCC 전체187개 중184개가 최초 통과(769.82초). 영어 비활성 식별자 cue의 이전 기수
  기대값과 전각 corpus 고정 해시가 남아3개 실패했다. 사용자 정책 변경을 전후 원장에
  기록하고 `unit=leading_zero` 검사와 정확한 해시 pin을 유지했다. C/리소스 변경 없이
  `ctest --test-dir build --rerun-failed --output-on-failure -j1` **3/3 통과**,81.93초.
  최종 모든187개 통과 근거는 `gcc-final-status.json`, 최초/재실행 로그를 함께 본다.
- Clang ASan+UBSan 영향 범위 **13/13**,133.11초. `asan-build.log`/`asan-ctest.log`.
  GCC 전체/재실행 종료 후 단독 실행했고 compiler warning 없음.
- 불변 baseline/candidate-v2의 한글/숫자 혼합 두 workload, 교대7라운드×3회:
  처리 시간비 **1.010× / 1.005×**,1.25× 게이트 통과. `throughput.json`.
- `build-web` CLI/공유 라이브러리 원자적 교체 후 service 재시작. 새 보고서만 발행했다.
  `deployment.json`의 이전 바이너리 백업은 `previous-web/`. 리소스 hash와 snapshot 일치 확인.
- 웹 새 예제4개 및 추가6개(API의 KO/EN/고객 영 읽기/명시 기수/금액 범위) text·parity 통과.
  웹 corpus **53/53**, 최신 보고서의 전체/전화 범주 지표 확인. `live-web.json`.
- Playwright 한국어 품질 페이지의 지표·필터·사례 전체 모달·키보드·모바일·내보내기·XSS
  및 기존 메인 UI의 제어·trace·사전·callback·byte parity·corpus 검사 통과.
  `browser-quality.log`, `browser-main.log`.
- git diff --check 통과. main은 기존 `0012911`; commit/push 없음.

Sanitizer 재현 (GCC와 동시에 실행하지 않는다):

```sh
cmake -S . -B build-sanitize -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug \
  -DTTF_ENABLE_ASAN=ON -DTTF_ENABLE_UBSAN=ON
cmake --build build-sanitize -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitize --output-on-failure -j1 \
  -R '^(phone_scanner_bounds|resource_semantic_validation|korean_pack_edges|semantic_boundary_streaming|leading_zero_v21_evaluation|numeric_width_evaluation|phone_zero_evaluation|phone_v18_evaluation|labeled_financial_identifiers|financial_quantity_values|finance_ranges_v7_evaluation|typed_controls_evaluation|numeric_width_native)$'
```

최종 library SHA `6ac635d13adf031f45f9d6a4f2b2149d76236913b4d6821d6141a12cdc9ecb75`,
resources SHA `1dd2bbdeafe223355c4729182e58a002b520b495cf8f86e8f2606b05909fa507`.
웹 `https://tts-text-frontend.exe.xyz:8000/ko-quality`에서 **전화번호 꼬리 문맥 · 선행 0 자리 읽기**
보고서를 고르고 직전 사전 보고서와 비교한다. 첫 화면은 전화·숫자 예제4개와 corpus53건을 제공한다.

다음 반복은 별도 요청에서 한 계열만 고른다. 전화 연결어/개인 번호의 화자 선호는
참조 일치율만으로 자동 결정하지 말고 사용자 리소스/명시 제어 평가로 분리한다.
남은 한국어 counter family 또는 사전/문맥 충돌은 `korean-quality-work-queue.json`의
근거를 먼저 검토한다. 이번에 전체 전화 numbering plan이나 개인 식별번호 문법을 확장하지 않았다.
초기 v1 GCC 실행은 계수 합성 문제를 확인해 중단한 로그(`gcc-ctest-interrupted-v1.log`)다.
성공한 전체 실행으로 세지 않는다. 커밋/푸시는 하지 않는다.

---

## 이전 인계: 공통 발음 사전 / 선택형 사용자 사전 — 완료 (미커밋)

사용자 결정: **공통은 기본 사전, 선택형은 사용자 사전**. 한 번의 사전 coverage 반복만
진행했다. 엔진 C/ABI/숫자 문법은 변경하지 않았다. 기존 dirty 작업은 그대로 유지했다.
제공 Training 참조의 다수 읽기를 언어적 정답으로 자동 채택하지 않는다.

### 변경과 편집 위치

- `benchmark/evaluators/lexicon_candidates.py`: 완료 보고서/DB hash를 확인하고 lexicon
  후보4,290키/13,869구간을 수집. 미일치5,434와 복수 읽기/정렬/구조/단위 분류 신호를
  보존한다. 원본·사전·gold를 수정하지 않고 산출물을 덮어쓰지 않는다. 외부 후보는 ignored.
- `lexicons/ko-KR/usage-additions-v22.jsonl`: 공통23항목, 별칭 포함24키. PIN, GS25,
  KIA, PASS, IELTS, AXA, FedEx, COOV, CJONE, Hmall, YES24, S-OIL, ITZY, GOT7,
  KOVO, GS SHOP/GSSHOP, PRO, SKY, ECO, FULL, LOUD, GO, YES.
  출처10항목 대조 / 편집 선택13항목 대기. 원본 JSONL → `manage.py build`로 기존5개 TSV 생성.
- `lexicons/sources/usage-v22.json`: 확인일·공식 URL·편집 정책. 외부 웹 원문/이미지/코드와
  제공 corpus 문장을 제품 리소스에 복사하지 않았다. THIRD_PARTY에 범위를 기록했다.
- 선택형9키: `resources/customers/reading-preferences/ko-KR/lexicon.tsv`.
  TV/CCTV/OLED/K3/SM3/XM3/QM3/S-OIL/MY. system manifest에 없으며 customer 로드나
  User API로 명시 적용한다. 기본 TV→티브이는 유지, 선택형은 티비.
- 웹 sample `reading-preferences`, corpus `lexical-coverage-v22` 등록. 선택형 파일은
  웹 고객 TSV 칸에서 편집 가능. 공통 사전은 `/lexicon` 검색·출처·수정안 시험에서 확인한다.
- 편집/적용법: [사전 보강 가이드](lexicon-coverage-v22.ko.md), [관리 가이드](lexicon-maintenance.ko.md).
  전체 관리 사전453항목/456키(system407/finance46), source-checked95/pending358/human-reviewed0.

### 같은 한국어 참조로 재측정

`reports/20260928-frequency-v3` → `reports/20260928-lexicon-v2`.
전체 게이트 후 최종 보고서를 웹에 발행했다. 원본/참조/옵션/평가기 동일, 제어 삽입과 선택형
사용자 프로필은 **적용하지 않았다**. Training 탐색용 일치도이며 held-out 정확도가 아니다.

| 지표 | 이전 | 이후 |
|---|---:|---:|
| 문장 정확 일치 / 142,248 | 16,963 (11.92%) | 16,967 (11.93%) |
| 문장 공백 제거 일치 | 80,242 (56.41%) | 80,318 (56.46%) |
| 구간 정확 일치 / 272,325 | 76,878 (28.23%) | 77,113 (28.32%) |
| 구간 공백 제거 일치 | 174,100 (63.93%) | 174,388 (64.04%) |

문장 출력 변화301: 새 일치77 / 상실1 / 여전히 불일치223. 정렬된 구간 변화309:
새 일치289 / 상실1 / 여전히 불일치19. 미채점 문장7 / 구조 경고1,349 /
구간 정렬 실패39,782 / 엔진 오류0 유지. 평가 보고서 작성182.774초(처리량/TTFB 아님).

상실1은 `ko-release/statistics/014536`: IELTS의 참조 `아이이엘티에스`와 공식
표기 `아이엘츠`의 차이. 참조 수정/제외 없이 불일치로 집계하며 독립 검토 대기다.
S-OIL(에쓰/에스), GS SHOP(샵/숍)도 raw 차이를 숨기지 않는다.
모든 변경301건의 부분 출력도 점검했다. PRO 사전 근거는 기존 ALNUM_TERM의
붙은 숫자 읽기도 바꾼다: `pro5549` 1건과 `PRO360` 2건은 전후 모두 불일치다.
값 손실은 없지만 원하는 식별번호/제품 코드 읽기는 명시 제어/전체 키 사전의 후속이다.

### 검증과 수정 기록

- 새 자체 계약36건, 기대 문자열은 최초 저작 이후 변경 없음. `cycle-final`: 이전12→이후36,
  fixed24 / regressed0. 기본/짧은 분절에서2,110 byte schedules의 text/span/trace 일치.
  User > Customer > System 및 영어 문맥 검사 통과. 짧은 분절 설정 자체는 기존 context
  창을 바꾸므로 서로 다른 설정 간 동일 출력은 보장하지 않는다(가이드 참고).
- 최초 English fixture는 text 보존 의도인데 speech로 작성해 모드만 text로 정정했다.
  `reference-mode-correction.json`에 원본/사유 기록; 기존 EN speech 출력은 전후 동일.
- 최초 MY를 전역 추가하자 기존 email local-part의 철자 읽기가 바뀌었다. MY를 선택형으로
  제한해 해소했다. 중간 실패 candidate/cycle/report는 보존하고 웹에는 발행하지 않는다.
- `existing-corpora-final.json`: 기존2,537건 text/span/비순번 trace 회귀0.
  trace 순번1149건 이동: 정렬 TSV 여러 위치 삽입으로 상수 오프셋은 아니며, 선택한 행과
  같은 layer 안의 상대 순서/tie는 모두 유지한다. 기존 corpus 참조 수정 없음.
- 후보 수집기 자체3검사 및 관련 CTest6/6 통과. 참조 audit57파일1,874건 구조 결함0,
  독립 사람 승인0. 전체 GCC/웹 결과는 아래 완료 검증에 기록했다.

### 재현과 산출물

루트 `artifacts/tn/ko-lexicon/`. 원래 baseline은 불변이고, `baseline-final`은 같은
기본 리소스에 명시 overlay 검사용 선택형 파일만 둔 비교 snapshot이다.
`candidate-v2`가 최종23개 기본 사전, `candidate-v1`은 이메일 회귀가 있던 중간안이다.

```sh
python3 benchmark/evaluators/lexicon_candidates.py \
  --report benchmark/generated/ko-release/reports/20260928-frequency-v3 \
  --out artifacts/tn/NEXT/candidates.json
python3 tools/lexicon/manage.py build --check
python3 tools/quality/tn_cycle.py cycle --baseline artifacts/tn/ko-lexicon/baseline-final \
  --candidate artifacts/tn/ko-lexicon/candidate-v2 \
  --corpus tests/evaluation/lexical_coverage_v22.jsonl --out artifacts/tn/NEXT/cycle
python3 benchmark/evaluators/korean_release.py \
  --import-dir benchmark/generated/ko-release/import-v1 \
  --library artifacts/tn/ko-lexicon/candidate-v2/libtts_text_frontend.so \
  --resources artifacts/tn/ko-lexicon/candidate-v2/resources \
  --out benchmark/generated/ko-release/draft-reports/NEXT \
  --label '사전 변경 설명'
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -j1
```

- `cycle-final/`, `native-contracts.log`, `existing-corpora-final.json`과 실행 스크립트.
- `external-comparison-final.json`, `external-changes-final.log`, `external-span-changes-final.json`:
  전체 변경/상실/부분 차이. 비교 스크립트는 발행된 reports 경로를 사용한다.
- `candidates-v1.json`, `inventory-final.json`, `reference-audit.json`: 검토 큐/사전/참조 원장.
- library SHA는 직전과 같은 `57fa7e8f40899ecf9de0ab6608d71cd49fa7d6cc4b8f06e901dd167efc5f2093`.
  resources SHA `d9c73f569661655e867c90fa36a9a633d513e2a513b9acb9f602c53eb09ccbee`.

### 완료 검증 / 다음 순서

- GCC Debug C11 build, **CTest185/185**, 790.18초. `gcc-ctest.log`.
- 이번 반복은 C 코드 변경이 없고 library SHA가 직전과 같으므로 sanitizer 전체를 다시
  실행하지 않았다. 직전 native 변경의 sanitizer10/10 기록은 아래 이전 인계에 있으며,
  이를 이번 리소스의 새 sanitizer 실행으로 주장하지 않는다.
- 웹 service 재시작, native library는 교체하지 않았다. `deployment.json`에 실제
  library/resources/summary 해시를 기록했다. 실패 중간안은 draft에만 남긴다.
- `/api/lexicon`:453항목, 새23항목 확인. 선택형9키 모두 요청별 text/byte parity 통과,
  기본 TV→티브이 유지, 웹 corpus36/36. `live-web.json`, `live-web.log`.
- Playwright: 선택형 예제의 고객 TSV·최종 text·분할 검증과 KIA 사전 근거 확인 통과.
  한국어 지표/필터/사례 전체 모달/키보드/모바일/내보내기/XSS gate, 기존 메인 UI의
  callback/제어/trace/코퍼스/모바일 gate도 모두 통과.
  `browser-preferences.log`, `browser-quality.log`, `browser-main.log`.
- 참조 구조 audit57파일1,874건 결함0, `git diff --check` 통과. main HEAD는 기존
  `0012911`이며 기존 작업과 이번 변경은 아직 미커밋이다.

웹: `https://tts-text-frontend.exe.xyz:8000/ko-quality`에서 최신 **공통 발음 사전
23항목 보강**과 직전 횟수 보고서를 비교한다. 첫 화면의 **사용자 사전 ·
티비/올레드/차종 읽기** 예제를 선택하면 편집 가능한 고객 TSV가 채워진다.

```sh
TTF_PLAYWRIGHT_MODULE=/tmp/ttf-web-browser/node_modules/playwright \
  /tmp/ttf-web-browser/node_modules/node/bin/node \
  tests/web/korean-quality.cjs http://127.0.0.1:8000
TTF_PLAYWRIGHT_MODULE=/tmp/ttf-web-browser/node_modules/playwright \
  /tmp/ttf-web-browser/node_modules/node/bin/node \
  tests/web/browser.cjs http://127.0.0.1:8000
```

운영 적용은 `artifacts/tn/ko-lexicon/publish_web.py`에 절차를 남겼지만 기존 output을
다시 덮어쓰는 명령은 아니다. 다음 변경은 새 snapshot/report/artifact 이름을 사용한다.

다음 반복은 KB/MG가 브랜드 문맥에서 단위로 분류되는 문제를 독립 계약으로 검토하거나,
이번 pending 읽기를 검수하는 한 계열만 선택한다. 개인 식별번호의 자리별 native 읽기와
assisted score는 사용자 의도 검토 후 별도 작업이다. 현재 선택형 테스트 통과를 외부자료
제어 적용 정확도로 표시하지 않는다. 미커밋 상태이며 커밋/푸시는 수행하지 않았다.

## 이전 인계: 한국어 횟수 `N번` 개선 — 완료 (미커밋)

사용자 요청에 따라 개인고유번호는 발화 의도/명시 제어의 후속 과제로 남기고,
제공 한국어 자료에서 **고유어기수의 횟수** 한 계열을 개선했다. 자동 TN 결과에
제어 구문을 주입하지 않았다. 제어 적용 정확도는 아직 측정하지 않는다.
`@native`는 정수 전체 읽기이며 자리별 `하나/둘` 읽기가 아니라는 점과 후속 평가의
부분집합·원문·주석·참조·검토자·literal/pattern 구분을 `remaining-work.md`에 기록했다.

### 변경

- `counter-frequency.tsv`: 58행. 앞 횟수 단서8 + streaming boundary8,
  뒤 횟수 동사19 + 번호 명사16, 조사 lookahead6, 붙은 `씩`1.
- KO pack `counters.c`에 `counter_role_tail` 추가: `3번씩`의 직접 횟수 단서.
  번호 단서 우선, 단서 없는 `3번`은 한자어, 나머지 기존 IR/전후 문맥 알고리즘 유지.
- `validate.c`: role enum/키 검사, enabled tail의 `particle|quantity_tail` 및
  `quantity_lookahead_tail` 의존 검사. 고객 `0` override 가능. Core/ABI 변경 없음.
- 자체 계약33건: 문맥·조사·범위·전각·문항/노선 부정·번호 우선·줄바꿈·고객 override.
  후보 전에 저작했고 기존 기대값을 바꾸지 않았다. 실제 외부 문장은 Git에 복사하지 않았다.
- CMake/web corpus/reference inventory 등록, `korean-counters.md`·`resources.md` 편집법,
  `korean-reference-benchmark.ko.md` 측정 이력, Korean work queue 갱신.

### 같은 외부 참조로 재측정

142,255문장 중 채점142,248, 미채점7. 272,325구간. Training 탐색용 참조이며
독립 held-out 정답률이 아니다. 원본/리뷰 overlay/평가기/옵션을 바꾸지 않았다.

| 지표 | 기준선 | 최종 |
|---|---:|---:|
| 전체 문장 정확 일치 | 16,896 (11.88%) | 16,963 (11.92%) |
| 전체 문장 공백 제거 일치 | 80,114 (56.32%) | 80,242 (56.41%) |
| 고유어기수 정확 일치 / 13,267 | 5,630 (42.44%) | 5,689 (42.88%) |
| 고유어기수 공백 제거 일치 | 11,040 (83.21%) | 11,145 (84.01%) |
| 구간 공백 제거 일치 | 173,929 | 174,100 |

출력 변화171문장; 새 공백 제거 일치129, 상실1, 순증128. 상실1은
`ko-release/currency_amount/015183`의 `26번의 우승`에서 참조 `이십육` / 정책
`스물여섯` 차이다. 독립 검토 대기이며 원본 불일치로 그대로 집계했다.
나머지41문장은 아직 전체 불일치지만 부분 변화도 검토했다. 엔진 오류0,
구간 정렬 실패39,782와 구조 경고1,349는 동일하다.

초기 후보는 `매월`이라는 노선명과 `확인` 앞 식별번호를 횟수로 오인했다.
**점수의 gain/loss만 보면 이미 불일치였던 번호 문장의 악화가 숨는다.**
전체 변경171건의 부분 출력도 확인하는 것이 이 반복의 필수 절차다.
최종안은 `매월`과 넓은 `확인/검사/수정` 단서를 빼고 직접 `씩`을 읽는다.
중간 실패 보고서는 `draft-reports/`에만 남겼으며 웹에 발행하지 않았다.

### 고정 결과·재현

- 작업 artifact: `artifacts/tn/ko-frequency/`. `baseline/`, `candidate/`, `candidate-v2/`,
  **`candidate-v3/`**에 CLI·library·resources·snapshot hash 보존. 최종 외부 평가175.413초
  (보고서 생성 전체 시간; 처리량/TTFB로 인용하지 않는다).
- 최종 보고서: `benchmark/generated/ko-release/reports/20260928-frequency-v3/`.
  기준선은 `reports/20260928-baseline/`; 같은 comparison_key. 원문/보고서 Git ignored.
- 최종 library SHA: `57fa7e8f40899ecf9de0ab6608d71cd49fa7d6cc4b8f06e901dd167efc5f2093`.
  resources SHA: `5d130bfa6428d70740415310fabb98fdfe9102b51f881eb8df0122d45e57bcd2`.
- `cycle-v3/cycle.json`: 33/33, baseline 통과13 → 후보33, fixed20, regressed0.
  `existing-corpora-v3.json`: 기존2,504건 출력/span/비순번 trace 회귀0.
  trace 순번217건은 +58 균일, 같은 layer 상대 순서 동일(엔진 상대 일련번호).
- `external-comparison-v3.json`과 `external-changes-v3.log`: 모든 실제 변경/상실/부분 변화.
  비교 스크립트를 재실행할 때 `new`를 발행한 `reports/` 경로로 바꾼다.
- 새 native 테스트: 33개 참조, 기본/짧은 분절에서1,718개 UTF-8 byte 분할의
  text/span/trace 동일, 고객 override·번호 우선·이른 callback 확인.
- 참조 audit: 56파일1,838건, 구조 결함0, 독립 사람 승인0.
- 교대 측정7회×3의 처리 시간 비율: 한글0.989배, 숫자/영어 혼합1.034배.
  기존 1.25배 제한 유지, baseline/candidate 출력 동일.

검증 명령은 현재 저장소 루트에서 실행한다(기존 artifact 경로 재사용 금지):

```sh
python3 tools/quality/tn_cycle.py snapshot --build build-ko-report --resources resources \
  --out artifacts/tn/ko-frequency/candidate-v3
python3 tools/quality/tn_cycle.py cycle --baseline artifacts/tn/ko-frequency/baseline \
  --candidate artifacts/tn/ko-frequency/candidate-v3 \
  --corpus tests/evaluation/counter_frequency_v22.jsonl --out artifacts/tn/ko-frequency/cycle-v3
python3 benchmark/evaluators/korean_release.py \
  --import-dir benchmark/generated/ko-release/import-v1 \
  --library artifacts/tn/ko-frequency/candidate-v3/libtts_text_frontend.so \
  --resources artifacts/tn/ko-frequency/candidate-v3/resources \
  --out benchmark/generated/ko-release/draft-reports/20260928-frequency-v3 \
  --label '횟수·번 문맥 단서 보강'
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -j1
```

### 완료 검증·웹 반영

- GCC CTest **182/182**, 782.82초. `gcc-ctest.log`.
- GCC 종료 뒤 Clang ASan+UBSan 관련 **10/10**, 21.26초. leak/error halt 활성.
  전체 sanitizer suite를 다시 돌린 것은 아니다. 새 횟수33건 CLI, KO 경계,
  리소스 의미 검증, 우선순위, 임의/무작위/의미 스트리밍, 동시 엔진 검증 포함.
  `asan-configure.log`, `asan-build.log`, `asan-ctest.log`.
- `check_web.py`: 실제 서비스의 이전/이후 지표, 횟수/번호4예제의 text/span/trace/segment
  분할 동일성, 새 웹 corpus33/33. `live-web.json`/`live-web.log`.
- `tests/web/korean-quality.cjs`, `tests/web/browser.cjs`: 실제 Chromium에서 보고서,
  상세 모달·전체 내용·모바일·XSS 처리 및 기존 workbench/콜백 통과.
  `browser-report.log`, `browser-workbench.log`.
- `publish_web.py`는 위 gate와 library/resources/report DB hash를 확인하고,
  기존 실행 파일을 `previous-web/`에 백업한 뒤 원자적 교체·서비스 재시작했다.
  `deployment.json`에 전후 SHA 기록. 운영 `build-web/`은 최종 스냅샷과 동일하다.
  https://tts-text-frontend.exe.xyz:8000/ko-quality 에서 **횟수·번 문맥 단서 보강** 선택,
  이전 기록 **20260928-baseline**과 비교할 수 있다. 표의 사례는 모달로 열린다.
- HEAD **0012911** 및 이전 미커밋 작업을 보존했다. 이번 커밋/푸시 없음.

재현한 sanitizer 명령(전체 GCC 종료 후 단독):

```sh
cmake -S . -B build-v20-asan -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug \
  -DTTF_ENABLE_ASAN=ON -DTTF_ENABLE_UBSAN=ON
cmake --build build-v20-asan -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
ctest --test-dir build-v20-asan --output-on-failure -j1 \
  -R '^(golden_ko|api_and_tokenizer|resource_and_lexicon_priority|resource_semantic_validation|arbitrary_chunk_streaming|korean_pack_edges|randomized_streaming|semantic_boundary_streaming|concurrent_shared_engine|counter_frequency_evaluation)$'
```

### 다음 한 작업

1. 현재 고유어기수의 남은 미일치2,122건에서 직접적인 셈단위 누락을 한 계열만 선별한다.
   `분`(사람/시간), 30개 등의 한자어/고유어 허용형, `두세` vs `두 번에서 세 번`은
   정책 차이일 수 있으므로 참조만 따라 고치지 않는다. 명확한 변형·부정 계약부터 만든다.
2. 개인고유번호는 문장별 읽기 의도를 검토한 뒤 제어의 자리별 native digit 규격과
   assisted 부분집합 지표를 설계한다. 자동 점수와 합산하지 않는다.
3. held-out 분리는 여전히 별도 후속. 이번 자료를 이미 튜닝에 썼으므로 일반화 성능 주장 금지.


## 이번 인계: 한국어 제공 참조 평가·웹 보고서 — 완료 (미커밋)

### 후속 UI 수정: 사례 상세 모달 (2026-09-28)

사례 상세가 긴 표 아래에서 열려 전체 내용을 찾기 어렵다는 사용자 제보를 반영했다.
`ko-quality.html/js/css`에서 native dialog 모달로 전환했다. 행 어느 칸에서나 열 수 있고,
원문/주석 참조/비교 참조/출력 전체, 선택 구간과 문장, 펼칠 수 있는 IR을 제공한다.
긴 텍스트 줄바꿈·내부 스크롤, 모바일 크기, 닫기/Esc/배경 클릭과 초점 복원을 지원한다.
빠른 재열기 때 지연된 close 이벤트가 새 요청을 무효화하지 않도록 동기 취소를 적용했다.
운영 8000 포트의 정적 파일에 반영했고 새로고침하면 적용된다. 엔진/자료/점수 변경 없음.
`korean_release_web` CTest1/1 및 `tests/web/korean-quality.cjs` 실서비스 브라우저 통과.
브라우저는 실제 필드 전체 일치, 7,500자 이상 내용 끝까지 표시, 행 클릭, 키보드 재열기,
모바일, XSS 문자 처리와 기존 보고서 필터·다운로드를 확인한다.
로그: `artifacts/tn/ko-release/browser-modal.log`. 전체 C suite 재실행은 하지 않았다.

2026-09-28. `/tmp/tn_release/summary.json`부터 21개 JSON을 조사해 **142,255문장 /
272,325참조 구간**의 첫 전체 기준선을 만들었다. 이번에는 엔진 C/rule/lexicon을 수정하지
않았다. HEAD **0012911** 및 기존 미커밋 약어·문자·그룹/의미 제어·NUM-WIDTH 작업을
보존했다. 커밋/푸시는 하지 않았다. 사용자가 요청한 웹 보고서 반영은 아래에 기록했다.

### 구현과 객관성 계약

- `benchmark/importers/korean_release/import_release.py`: summary/파일/행 개수·원본 SHA,
  bracket 참조 해제, 문장 전체의 유일한 source↔spoken 정렬, 원본 UTF-8 offset,
  중복·충돌·구조 경고. 원본 theme은 주제이며 semantic class 정답으로 취급하지 않는다.
- `benchmark/evaluators/korean_release.py`: frozen native engine 1개를 재사용하는 전체 문장
  평가, strict/Unicode 공백 제거 지표, 구간 투영·정렬 실패, 카테고리/주제/IR/수정 방향별
  집계. 정렬 실패는 구간 분모에서 빼지 않는다. 값·기호·대소문자·NFC/NFKC는 완화하지 않는다.
- 수정 방향 `pattern / lexicon / context-control / review`는 입력 형태와 주제에서 얻은
  **제안**이다. 자동 TN에 사전/제어를 몰래 삽입하지 않고 확정된 원인처럼 표시하지 않는다.
- `--reviews`는 source fingerprint·검토자·사유·날짜가 있는 accepted/corrected/excluded/
  needs-review overlay. 원본 headline은 고정하고 검토 부분집합만 별도 집계한다.
- import/report output 재사용 거부, 시작/종료 corpus/library/resource/harness hash 검사,
  6개 평가 코드 사본, 압축 SQLite 사례/IR 저장. 완료 summary를 마지막에 발행한다.
- `tools/web/ko_reports.py`, `/ko-quality`: 완료된 로컬 보고서만 읽는 GET API·50개 페이지,
  카테고리/주제/수정 방향/공백 차이/구조 경고/문장·구간 필터, 실제 IR, 검토 양식·집계
  다운로드, 동일 protocol 기록만 증감 비교. 서버에서 평가나 참조 쓰기를 수행하지 않는다.
- 제공 자료는 **Training**이고 Validation이 동일하여 제외했다는 원본 설명을 보존했다.
  독립 held-out/production 정답률로 부르지 않는다. 사용 권리는 미확인, 자동 다운로드·
  vendoring 없음. 외부 원문과 보고서는 Git ignored 아래에만 둔다.

### 실제 첫 기준선

| 지표 | 문장 | 참조 구간 |
|---|---:|---:|
| 전체 | 142,255 | 272,325 |
| 채점 분모 | 142,248 | 272,325 |
| 정확 일치 | 16,896 (11.88%) | 76,757 (28.19%) |
| 공백 제거 일치 | 80,114 (56.32%) | 173,929 (63.87%) |
| 공백만 다름 | 63,218 | 97,172 |

엔진 오류0, 문장 미채점7, 구간 정렬 실패39,782. 구조 경고 문장1,349
(주석/metadata 차이21, 주석 밖 변경1,321, 불완전 bracket7). 중복 입력 행233,
입력별 참조 충돌 그룹0. 구조 검사 통과 부분집합·중복 제거·검토 부분집합도 별도 표시한다.
사람 승인 참조0. 기록된177.036초는 보고서 생성 전체 시간이지 엔진 처리량/TTFB가 아니다.
초기 저장 형식의 측정과 최종 압축 DB 측정은 모든 지표·카테고리·주제·수정 방향 집계 동일.

### 재현 파일과 다음 작업

- 원본 보관: `benchmark/generated/ko-release/source/` (업로드와 동일).
- import: `benchmark/generated/ko-release/import-v1/`.
- immutable Release snapshot: `benchmark/generated/ko-release/engine-20260928/`.
- 최종 보고서: `benchmark/generated/ko-release/reports/20260928-baseline/`.
  `summary.json`, `report.sqlite`(약671MiB), `harness/`. 웹에는 이 완료 기록만 공개한다.
- library SHA256 `98ad7a767971fe3b46548fa1c28b81fed83b85fc7e3c8e7f12a0513939a6f413`.
- resources SHA256 `b4a341b00b61b5673affcac878d4e2c5e02cc918db6fc76cec22eff9b4a7e865`.
- 조사/빌드/평가/CTest/브라우저 근거: `artifacts/tn/ko-release/`.
- 재실행 명령과 review 계약: [한국어 참조 가이드](korean-reference-benchmark.ko.md).
- 다음 순서: [한국어 작업 큐](../benchmark/manifests/korean-quality-work-queue.json).
  먼저 참조 경고와 금융 번호의 공/영·묶음·마스킹·문맥 정책을 검토하고 한 규칙군만 선정한다.
  관용 이름은 사전, 불명확한 의도는 명시 제어 검토로 구분한다. 띄어쓰기63,218건은
  값/단어 개선과 별도로 다룬다. EVAL-03 held-out 분리는 여전히 미완료다.

### 검증·웹 반영

- GCC configure/build 성공. 새 importer/evaluator/HTTP/native 검사와 기존 연결 검사
  `ctest --test-dir build --output-on-failure -R 'korean_release_|external_benchmark_evaluator|evaluation_comparison|web_workbench'`
  **6/6** (15.58초), 전체 `ctest --test-dir build --output-on-failure -j1`
  **180/180** (774.56초). 시간 제한 변경 없음. Python importer/evaluator 단위5개,
  HTTP 계약4개와 native 합성 참조2건을 새 게이트에 포함했다.
- 이번 작업은 C 엔진을 수정하지 않아 sanitizer를 다시 실행하지 않았다. 이전 NUM-WIDTH
  관련 ASan+UBSan30/30은 아래 과거 인계의 결과이며 이번 전체 검사로 세지 않는다.
- 실제 전체 자료142,255건 평가 완료·엔진 오류0. source/엔진/resource/harness 고정 검사를
  통과했다. synthetic CI fixture만 Git 관리하며 외부 원문은 CI 입력/artifact로 올리지 않는다.
- 운영 브라우저 검사 두 개 모두 성공:
  `TTF_PLAYWRIGHT_MODULE=/tmp/ttf-web-browser/node_modules/playwright /tmp/ttf-web-browser/node_modules/node/bin/node tests/web/korean-quality.cjs http://127.0.0.1:8000`
  및 같은 환경의 `tests/web/browser.cjs http://127.0.0.1:8000`.
  새 보고서 필터/상세/다운로드/동일·상이 protocol 비교/모바일/XSS 문자 처리와 기존
  제어·오류·사전·임의 분할·실시간 callback·corpus UI까지 통과했다.
- 사용자 요청에 따라 **8000 포트 반영 완료**:
  https://tts-text-frontend.exe.xyz:8000/ko-quality (메인 화면 링크도 추가).
  기존 `ttf-workbench.service`를 재시작했다. Release snapshot의 shared library·CLI를
  `build-web`에 atomic replace했고 service/library/resource 경로와 접근 정책은 유지했다.
  웹 library SHA는 보고서와 동일하다. static archive는 서비스가 쓰지 않아 교체하지 않았다.
  교체 전 파일/서비스 설정은 `artifacts/tn/ko-release/previous-web/`, 전후 hash는
  `deployment.json`, HTTP 검증은 `live-http.json`에 있다. 8001 임시 검증 서버는 종료했다.
- 실서비스 `/api/health`, 보고서 catalog, `１２３원 → 백이십삼 원`,
  `[[0210]]{@digits} → 영이일영`, `Apple은 사과입니다. → 애플은 사과입니다.` 확인.
  git diff --check 통과, 외부 원문·DB가 ignored임을 확인했다. HEAD/원격 변경 없음.

## 이전 인계: NUM-WIDTH 전각 숫자 — 완료 (미커밋)

2026-09-28. 제어 예약어 확장 전의 KO/EN 작업 순서로 복귀해 **전각 숫자 U+FF10–FF19**
한 규칙군을 구현했다. HEAD **0012911**, 기존 미커밋 약어·전각 영문자·그룹 제어·20개
의미 지정 제어를 유지했다. 커밋/푸시/운영 웹 배포·재시작은 하지 않았다.

### 구현 범위

- `src/semantic/numeric_width.c`가 보호 구간 밖 숫자의 scanner view와 양방향 byte map을
  제공한다. universal numeric/phone/EN date/KO counters는 기존 ASCII 값 스캐너를 재사용한다.
  각 스캐너에 UTF-8 폭 처리를 복제하지 않았다. core 언어 분기·새 public ABI 없음.
- tokenizer 전각 숫자→DIGIT, segmenter/normalizer가 공통 view 사용. input offset은 raw UTF-8,
  IR value는 canonical ASCII, numeric verbalizer의 surface는 canonical. 상세 trace는
  numeric-width 결정을 기록한다. exact/pattern/원문 보존은 raw 기준이다.
- 코드·경로·명시적 제어 보호. 경로 검사와 incremental path 대기에 전각 숫자를 허용했다.
  annotated 숫자 예약어의 ASCII-only 계약은 유지한다.
- 미지원 전각 숫자 구두점의 일부 읽기를 방지하는 `syntax.width-punctuation` 보호.
  `１２，５００원`을 `십이，오백 원`으로 잘못 나누지 않는다. 전각 구두점 자체를
  숫자 문법으로 지원하지는 않는다. 위/아래 첨자는 기존 보존 정책 그대로다.
- [정책·설계·참조 가이드](numeric-width.ko.md), README/language-pack/remaining-work,
  CTest3개, 웹 corpus `numeric-width-v22`·예문3개와 reference inventory를 갱신했다.

### 확정된 근거

- 구현 전 자체 참조30건, **IR 포함 계약6→30 (개선24, 회귀0), 텍스트 exact7→30**.
  tn_cycle batch/1·7·64byte streaming과
  IR 검사 통과. 개발자 정책 참조이며 외부 실데이터 정확도·독립 언어 감수로 세지 않는다.
- 최초 참조 중 `width-invalid-group`의 span만 기존 ASCII baseline을 근거로 정정했다.
  TEXT가 숫자 부분을 소유하고 원은 다음 WORD다. 기대 출력 텍스트는 그대로다.
  original/current SHA와 before/after는 가이드와 artifacts/reference-correction.json 참조.
- 기존 **2,474건 중 의도한 변경2건, 예상 밖 회귀0**. 과거 미지원 보존 참조
  `char-width-digits-not-folded`·`numeral-char-v21-fullwidth-open`은 구현 전에 승격했다.
  나머지 text/span/trace가 모두 동일하고 declaration_order 변경도0이다.
- 두 분할 profile에서 **1,244회** 임의 byte 분할 text/span/trace 일치. 추가 ASCII↔전각·혼합
  숫자64조합의 값/IR 동등성. invalid/preserved 원문은 원래 폭을 유지하는 별도 계약.
  canonical로 원문 override 열쇠를 바꾸지 않는지, 앞부분 폴딩 후 제어 offset, early callback도 검사.
- native C gate가 UTF-8 byte 경계, 전각 구두점8부정, code/control/path, 9,009byte 경로의
  1byte 입력을 검증해 sanitizer에서도 실행된다.
- 처리 시간 교대7회·3반복 중앙값: 한국어 기존 입력 **80.19→83.43ms (1.040배)**,
  KO/EN·ASCII 숫자 입력 **134.16→138.32ms (1.031배)**. 고정1.25배 한도 통과.
  전각 workload **124.63ms**는 정보값이다. Debug/VM에서 측정, 숫자 폭에 따른 경로 차이가
  있어 성능 우위를 주장하지 않는다. 생성/로딩/trace 비용은 이 처리량 측정에서 제외했다.
- reference audit **55파일·1,805건**, 구조결함0, 사람승인0.
- GCC 전체 **177/177** (761.74초), Clang **ASan+UBSan 관련30/30** (125.71초).
  sanitizer evaluation_reports도 기존 제한 그대로 **31.86초**에 통과. compiler warning 및
  메모리·누수·UB 진단0. 두 suite는 순차 실행했다. sanitizer 전체 suite를 재실행한 것은 아니다.
- 최종 build CLI/shared library의 SHA256은 candidate snapshot과 동일하다. CLI 예문5개를
  직접 실행해 문서와 대조했다(cli-examples.json). git diff --check 통과. HEAD/원격 변경 없음.

### 재현·남은 순서

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -j1
python3 tools/quality/tn_cycle.py cycle --baseline artifacts/tn/numeric-width/baseline --candidate artifacts/tn/numeric-width/candidate --corpus tests/evaluation/numeric_width_v22.jsonl --out artifacts/tn/numeric-width/NEW-cycle
```

근거: `artifacts/tn/numeric-width/`의 immutable snapshots, cycle/, existing-corpora.json,
compare_existing.py, intentional-changes.json, 참조 변경 기록, throughput.py/json,
GCC/ASan 로그 및 reference-audit.json. sanitizer는 GCC 전체 suite 종료 후
`bash artifacts/tn/numeric-width/run-sanitizer.sh`로 순차 실행한다. 제한을 늘리지 않는다.

다음 후보는 **EVAL-03 held-out 평가 분리** 또는 새로 제공된 실입력 한 규칙군이다.
맨 연도/짧은 선행0/로마 숫자/위·아래 첨자는 해석 정책 결정을 먼저 해야 한다.
이번 작업에서 자동으로 변경하지 않았다. 최신 웹 바이너리 적용은 별도 승인 후 진행한다.

## 이전 인계: 의미 지정 제어 예약어 20개 — 완료 (미커밋)

2026-09-28. 사용자가 address/email/url 외 여러 의미를 명시하는 예약어 확장을 요청했다.
HEAD **0012911**와 이전 미커밋 약어·전각 문자·그룹 제어를 보존했다. 커밋/푸시/서비스 배포나
재시작은 하지 않았다. 이번 범위는 다음20개이며 자동 TN 개선 goal을 재개하지 않는다.

- 숫자: `@cardinal`, `@digits`, `@decimal`, `@ordinal`.
- 달력: `@year`, `@date`, `@date-mdy`, `@date-dmy`, `@time`.
- 수량: `@currency`, `@percent`, `@measure`, `@counter`(KO), `@fraction`, `@range`.
- 기술: `@phone`, `@version`, `@ip`(IPv4).
- 출력 의도: `@preserve`, `@skip`.

### 구현·계약

- 기존 예약어6개와 총26개. 문법은 기존 `[[source]]{@name}`이며 single atom도 유지한다.
  annotated에서만 실행하고 명시 제어는 text/speech 모두 적용한다.
- 공유 `src/control/semantic.c`가 엄격한 유효성 검사 → 내부 Semantic IR → 기존 pack
  verbalizer를 연결한다. pack이 CUSTOM dispatch에서 선택하며 core의 언어 분기/새 ABI/
  재귀 normalize는 없다. 외부 span은 CUSTOM/control.pattern, 내부 타입·값·unit·secondary·
  grammar는 control-semantic trace, 전체 decoded 원문은 input-control trace에 남는다.
- 단위형은 요청한 타입으로 전체 매칭될 때만 사용한다. 부분 매칭/TEXT-preserve/다른 타입은
  EINVAL. 정수부18자리, 일반 semantic 원문95byte, digits95자리. preserve/skip은 전체
  max_control_bytes 한도. 기존 sino44자리/주소 정책은 변경하지 않았다.
- `[[9/25]]{@fraction}` → `이십오 분의 구`, `[[0210]]{@digits}` → `영이일영`.
  `[[03/04/2026]]{@date-dmy}` → `이천이십육 년 사 월 삼 일`.
  영어 `[[1984]]{@year}` → `nineteen eighty four`.
- 날짜 순서 directive는 입력 순서만 지정한다. 달력 유효성 검증(윤년 포함). phone은
  선택적+와 숫자/공백/하이픈, version은 선택적v/V와 숫자 성분, IPv4는4개 0..255성분이다.
  IPv6·SemVer 전체·phone 내선·중첩·국제화 전자주소·영문주소는 지원하지 않는다.
- `@preserve`는 문자 drop도 건너뛰고 원문 유지. `@skip`은 완전 생략이며 주변 공백을
  추가/정리하지 않는다. 고객 phone/electronic/range 리소스 override를 그대로 따른다.
- 웹 corpus `typed-controls-v22`, 예문10개·도움말, README/input-format/language-pack/
  remaining-work와 [한국어 가이드](explicit-pattern-controls.ko.md)를 갱신했다.

### 검증 근거

- 구현 전 참조67건, 최초66/67. `typed-ip-en`은 작성자가 기존 version의 point를 IP에도
  잘못 쓴 참조 오류였다. **기존0012911 speech.tsv의 ip=dot / version=point**를 근거로
  참조만 정정했다. 엔진/리소스 정책 변경 없음. 원본·정정SHA와 사유는 가이드/리뷰 목록,
  artifacts의 references-original.jsonl/reference-correction.json에 남겼다. 사람승인0.
- 정정본67/67, SHA `37c928066f5d65fd9916d9a3331fd3d90b0bcfef487289c5b9da2f91cdf5586f`.
  개발자 정책 참조이며 외부 corpus/독립 언어 평가의 개선 점수로 해석하지 않는다.
- 기존 **2407건 회귀0**: text/span/trace 완전 동일, declaration_order 변화도0.
  `tn_cycle`은 기존 grouped_controls30건으로 **30→30, 회귀0**. 새 이름은 이전 엔진이
  거부하므로 도구 오류를 텍스트 불일치 점수로 바꾸지 않는다.
- 새67건×2분할 profile, 모든2조각/1byte/random **2858회** text/span/trace 일치.
  고객 override, 긴 그룹, preserve/drop, escape, code/plain 보호, 앞 문장의 write callback과
  잘못된 뒤 구간 비노출 검사. 오류 **200가지 언어/형식 조합** batch/byte stream 거부,
  크기초과10건, EN counter 거부.
- GCC 전체 **174/174** (755.50초). 이후 내부 IR의 struct_size/상대 입력 범위 초기화를
  명시했고, 최종 제어·웹 검사 **7/7** (32.16초)와 기존2407건 비교를 재실행해 모두 통과했다.
  최종 candidate-final 스냅샷의 CLI/shared library hash는 build와 동일하다.
- Clang **ASan+UBSan 관련11/11** (120.06초), compiler warning/메모리·누수·UB 진단0.
  GCC suite와 순차 실행했다. sanitizer 전체 suite를 재실행한 것으로 주장하지 않는다.
  신규/기존 제어, 오류·한도, arbitrary/semantic streaming, pack 확장, CLI callback을 검사했다.
- 최종 평가 after-final/report.json은 **67/67**, metadata/stream 실패0. tn_cycle legacy-final은
  기존30건30→30. 이전 바이너리는 새67건 모두 EINVAL로 거부(baseline-availability.json).
- reference audit **54파일·1775건**, 구조결함0/사람승인0. 기준 기대값은 독립 검토 대기다.

### 재현과 다음 작업

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -R '^typed_controls_'
python3 tools/evaluation/evaluate.py --ttf build/ttf --resources resources --corpus tests/evaluation/typed_controls_v22.jsonl --output-dir artifacts/tn/typed-controls/NEW-evaluation
python3 tools/quality/tn_cycle.py cycle --baseline artifacts/tn/typed-controls/baseline --candidate artifacts/tn/typed-controls/candidate-final --corpus tests/evaluation/grouped_controls_v22.jsonl --out artifacts/tn/typed-controls/NEW-legacy-cycle
```

증거: `artifacts/tn/typed-controls/` (immutable snapshots, 기존2407 비교, legacy-cycle,
참조 정정 근거, GCC/ASan 로그, reference audit, status.json). 운영 웹 엔진은 배포하지 않았다.
Sanitizer 재현은 GCC suite 종료 후 실행한다:

```sh
cmake -S . -B build-v20-asan -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug -DTTF_ENABLE_ASAN=ON -DTTF_ENABLE_UBSAN=ON
cmake --build build-v20-asan -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-v20-asan --output-on-failure --stop-on-failure -R '^typed_controls_|^grouped_controls_|^user_pattern_contracts$|^user_patterns_v20_evaluation$|^semantic_boundary_streaming$|^protected_stream_limits$|^arbitrary_chunk_streaming$|^language_pack_extension$|^cli_stream_json$'
```
다음 변경은 구체적 실입력/요청을 받아 같은 가이드의 타입·오류·byte 분할 계약부터 추가한다.
NUM-WIDTH/자동 연도·leading-zero/held-out은 별도 미착수 항목이다.

## 이전 인계: 명시적 구간 패턴 제어 — 완료 (미커밋)

2026-09-28. 사용자가 NUM-WIDTH보다 먼저 구간 지정 패턴을 요청했고, 주소는 **한국어부터**
지원하는 범위를 확인했다. HEAD `0012911`; 앞선 점 약어·전각 문자 변경을 그대로 보존했다.
현재 작업도 commit/push/웹 엔진 배포·재시작을 하지 않았다.

### 구현과 사용법

- 기존 `[[source]]{literal}`을 `[[source]]{@pattern}`까지 확장했다. 새 괄호 문법 없이
  기존 parser/stream pending/UTF-8 escape와 한 구간 callback 계약을 재사용한다.
- KO `@address`: 한글 지명 그대로, ASCII 숫자 한자어 수사, 선행0 자릿수 보존,
  숫자 사이 하이픈은 `다시`, ASCII 글자는 글자 이름. `address_policy` 3행과 validator.
  주소 숫자는 기존 CARDINAL/DIGIT_SEQUENCE IR/verbalizer를 재사용하며 재귀 TN은 없다.
- KO/EN `@email`·`@url`: 전체 구간을 기존 electronic renderer/사전/기호 읽기로 처리.
  자동 TLD/URL 인식에 의존하지 않는다. 내부 이메일 호스트와 상대 경로도 지원한다.
  ASCII 형식 검증과 명시적 오류를 제공하며 DNS/우편 검증이나 네트워크 요청은 없다.
- `--input-format annotated` 필요. 새 제어는 기존 @native처럼 **text/speech 모두 읽기**.
  `[[서울시 12-3]]{@address}` → `서울시 십이 다시 삼`;
  `[[a1@b2.com]]{@email}` → `에이일 앳 비이 닷 컴`;
  `[[q1/a2]]{@url}` → `큐일 슬래시 에이이`.
- public ABI v1 변경 없음. CUSTOM `unit=@name`, `secondary=control-surface`와 기존
  callback surface/length로 완전한 decoded group을 전달한다. 95byte를 넘으면 value는
  빈 값, 전체 원문은 callback과 input-control trace에 유지한다. 원문을 자르지 않는다.
  단일 atom의 기존95byte 한도와 제어 전체 max_control_bytes는 유지한다.
- 긴 전자 주소의 알파벳 run도 같은 borrowed-surface 전달로 읽는다. scalar native/sino
  범위는 유지. `@address`의 숫자 run은 cardinal44자리/digits95자리 제한.
- 영어 주소/국제화 이메일·URL/모든 지명 관용 독음/그룹 중첩은 미지원. 예외 발음은
  literal 제어를 사용한다. 한국어 주소 계약은 사용자 선택이지만 자체 30사례의 독립
  언어 감수나 우편 주소 유효성 인증을 주장하지 않는다.

### 검증과 근거

- `grouped_controls_v22.jsonl` **30/30**, 구현 전 고정, 기대값 변경 없음.
  SHA256 `aed54be33c8879afdfc8c3105e06a4091427a1cc40561a4cac4628a850362395`.
  기준선은 새 구문27건을 EINVAL로 거부하며 legacy3건만 지원한다. evaluator의 정상
  프로토콜 오류를 텍스트 점수로 바꾸지 않았고 `baseline-availability.json`에 따로 기록했다.
- `tn_cycle`은 기존 user_patterns51건에 적용: **51→51, 회귀0**. baseline/candidate는
  `tools/quality/tn_cycle.py snapshot`으로 고정했다.
- 기존 **2,377건 회귀0**(앞선 미커밋 약어30·문자30 포함): text/span, 선택 리소스/trace
  비순번 필드 모두 동일. 늦게 로드되는 CJK/overlay trace217개만+3, 같은 layer 상대순서/동률 불변.
- 새30건×2profile, 모든2조각/1byte/seed random **2,246회 text/span/trace 일치**.
  앞 문장의 write callback, 미완성/잘못된 그룹 비노출, 이미 commit한 callback 유지 확인.
- 잘못된 구문/메일/URL/주소 **32종**을 batch·1byte streaming 양쪽에서 거부.
  제어 전체 byte limit 경계, 영어 주소 거부, 기존 atom 길이, 잘못된 고객 정책도 검사했다.
- GCC 신규3/3 및 관련 **39/39** (64.23초). Clang ASan+UBSan 관련 **9/9** (82.42초),
  메모리·UB 진단0, compiler warning0. 두 suite는 순차 실행. release 전체 suite 재실행은 아니다.
- reference audit **53파일·1,708건**, 구조결함0/사람승인0. 주소 단독 annotated 구간만
  하이픈 `다시` 검사의 주소 예외다. plain, 명시 RANGE, 물결, 주소 밖 범위는 계속 검출하며
  부정 self-test로 고정했다. 기존 금액 범위 게이트를 느슨하게 만들지 않았다.
- CTest/웹 corpus `grouped-controls-v22`/웹 예문3개/입력 도움말 등록.
  [한국어 사용·편집 가이드](explicit-pattern-controls.ko.md), [입력 계약](input-format.md),
  language-pack ABI 전달 규약과 README 갱신. 일반행3,575, CJK both11,997.

### 재현

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -R '^grouped_controls_'
python3 tools/quality/tn_cycle.py cycle --baseline artifacts/tn/grouped-controls/baseline --candidate artifacts/tn/grouped-controls/candidate --corpus tests/evaluation/user_patterns_v20.jsonl --out artifacts/tn/grouped-controls/NEW-legacy-cycle
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-v20-asan --output-on-failure -R '^grouped_controls_'
```

증거는 `artifacts/tn/grouped-controls/`: immutable snapshots, baseline availability,
신규 평가 report, 기존 전수 비교 script/JSON, legacy-cycle, GCC/ASan 로그, audit.
이 요청은 완료. 다음 TN 후보 NUM-WIDTH는 아직 미착수이며 자동 진행하지 않는다.

## 이전 인계: speech_character 전각 영문자 — 완료 (미커밋)

2026-09-27 시작, 09-28 완료. HEAD **`0012911`**. 직전 점 약어 working tree를 기준선으로
고정하고 한 문자군만 보강했다. 이전 변경을 유지했으며 이번에도 **commit/push/배포/서비스
재시작은 하지 않았다**. 웹 코퍼스 등록은 소스에 반영됐지만 운영 중인 웹 서비스는 갱신하지 않았다.

### 변경·계약·한계

- `resources/languages/{ko-KR,en-US}/rules/speech-characters.tsv` 각52행을 manifest 끝에 추가.
  KO는 기존 알파벳 이름, EN은 대소문자 유지 ASCII로 최종 출력한다. 이번 작업의 runtime
  C 코드·ABI·tokenizer 변경은 없다. 숫자·단어의 의미 분석 전에 입력을 folding하지 않는다.
- `계좌 ＡＢＣ 확인` → `계좌 에이비씨 확인`, `Ｂ등급 채권` → `비등급 채권`;
  EN speech `Ｆｉｌｅ` → `File`. KO `Ｆｉｌｅ`는 **글자 이름**이며 `파일` 복원은 범위 밖이다.
- source=und/script=OTHER 및 원문 offset 유지. 선택 key/value/layer/부분 byte 범위는
  policy trace에 남는다. 붙임/조사 locality, 보호 구간, text 원문, unknown 기본 보존 유지.
- Customer > Domain > System, 같은 layer는 먼저 선언한 행. 자기 자신 치환으로 문자별
  보존 가능. 전각 단어는 EN 발음 사전 검색 대상이 아니므로 관용 읽기가 필요하면
  annotated literal을 쓴다. 이 한계와 실제 탭 편집 예시는
  [문자표 가이드](speech-resources.md#전각-영문자-최종-읽기-v021-continuation)에 기록했다.
- 고정 계약 `speech_character_width_v21.jsonl` **30건**, 구현 전 작성, 기대값 수정 없음.
  SHA256 `b507c23f08e290a8e225e53bfae7aa4d747b7da6e3392401e8533944cbc86a97`.
  `tn_cycle`: **8/30→30/30**, fixed22 / regressed0 / remaining0. 자체 정책 검증이며
  외부 실제 자료/held-out/독립 언어 감수로 해석하지 않는다.

### 검증 결과

- GCC 신규3/3, 관련 **31/31** (30.36초). CJK86계약, 기존 漢/keycap 문자 locality,
  점 약어, compiled resources, 웹, reference integrity 포함.
- Clang ASan+UBSan 관련 **7/7** (30.47초), 메모리/누수/UB 진단0. GCC와 동시 실행 없음.
  이번에는 release 전체 suite를 다시 실행하지 않았다.
- 새30건×2분할 profile, 모든2조각/1byte/seed random **834회** text/span/trace 일치.
- 기존 corpus **2,347건 회귀0**: `0012911`의 corpus2,317 + 미커밋 점 약어30건.
  출력·span 및 선언번호 외 trace 모든 필드 동일. 추가104행 뒤의 CJK/overlay 이벤트217개
  선언번호만+104; 같은 layer 상대순서/동률 불변.
- audit **52파일·1,678건**, 구조 결함0, 사람 승인0. 새로운 corpus는 pending review.
- 일반 공용 행3,572, hanja만11,693 / kana만3,878 / both11,994. 공개 가이드 현재 수치 갱신;
  CJK 설계의 역사적 측정값은 유지했다. 새 TSV는 생성 CJK 파일이 아닌 자체 편집 정책표다.
- snapshot candidate와 최종 build의 CLI/shared library SHA 동일. 기존 corpus 비교 스크립트와
  결과, 기준선/후보, cycle, GCC/ASan 로그, audit는 `artifacts/tn/speech-character-width/`.

### 재현 명령

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -R '^speech_character_'
python3 tools/quality/tn_cycle.py cycle --baseline artifacts/tn/speech-character-width/baseline --candidate artifacts/tn/speech-character-width/candidate --corpus tests/evaluation/speech_character_width_v21.jsonl --out artifacts/tn/speech-character-width/NEW-comparison
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-v20-asan --output-on-failure -R '^speech_character_'
```

다음 한 규칙군 후보: **NUM-WIDTH**(전각 숫자, 숫자/날짜/전화/셈단위 스캐너와 source offset).
문자표에 숫자를 넣어 의미 해석을 우회하지 않는다. 맨 연도·NUM-ZERO·로마숫자·EVAL-03은
정책/별도 범위를 유지한다. 이번 한 규칙군은 완료했으므로 자동으로 계속하지 않는다.

## 이전 인계: 점 표기 영문 약어 — 구현·검증 완료 (미커밋)

사용자가 승인한 순서에 따라 CJK 전체를 **`0012911`**로 커밋한 뒤 KO/EN 개선을 재개했다.
현재 HEAD는 `0012911`. 이번 TN 변경은 검증된 working tree에 남겼다. push·배포·서비스
재시작은 수행하지 않았다. **한 규칙군 완료 후 종료**하며 다음 항목을 자동 진행하지 않는다.

### 변경과 정책

- 기존 `abbreviation_bare`가 정한 vol/ltd/vs/etc/jr 읽기를 **점 표기**에도 명시적으로
  적용했다. `Read Vol. 3.` → `Read volume three.`, `Acme Ltd.` → `Acme limited.`.
- `abbreviations-dotted.tsv`: 읽기5행, boundary marker3행, vs의 continuation1행.
  새 일반 경로는 EN pack의 `ABBREVIATION(value=key, secondary=dotted)`이며
  `en.abbreviation.dotted`와 실제 읽기/마침표 리소스를 trace한다. ABI/Core 변경 없음.
- EOF/개행/닫는 괄호·따옴표/다중점 앞의 점을 보존한다. 뒤의 대문자 낱말은 문장
  시작일 수 있어 기본 보존한다. vs는 `continuation`으로 `vs. Beta`의 점을 소비한다.
  고객이 `sentence|continuation`을 바꿀 수 있다. 일반 문장 경계 추론을 완성한 것은 아니다.
- 식별자·URL·email·literal은 기존 경로. 마지막 검사에서 `Vol.é`→`volumeé` 오인식을
  찾아 확장 Latin도 오른쪽 경계에서 거부했다. 고정30건 기대값은 수정하지 않았다.
- KO 문맥은 기존 발음 파이프라인을 사용한다. customer `limited→리미티드` 및
  user `Ltd.→고객명` 우선순위를 검사했다. 새로운 system 발음 사전을 넣지 않았으며
  기본 unknown-word 한글 발음 품질의 개선/검수를 주장하지 않는다.
- CJK lazy 테스트가 일반 행 수3,459를 고정하고 있어 일반 자원9행 추가에 깨지는 것을
  방지했다. baseline record 수를 읽되 CJK 사전 미로드·옵션별 증가량·rollback·우선순위
  검사는 유지한다. 일반공용3,468행, 둘 다 CJK Korean일 때 총11,890행. 공개 리소스
  가이드의 현재 수치도 갱신했다. 설계§18의 과거 측정 수치는 바꾸지 않았다.

### 고정 계약과 검증

- 자체 작성 `abbreviation_dotted_v21.jsonl` **30건**, 구현 전 고정.
  SHA256 `b7c1d5a34ec18816b996cdb0d189cd1fb2344cf27878cd1026cc029cca521382`.
  `tn_cycle` **9/30→30/30, fixed21 / regressed0 / remaining0**.
- 두 phrase profile, text/speech, 모든2조각/단일byte/seed random **1,738회**
  text/span/trace 일치. 의미값·원문 byte offset·문장 마침표 별도span도 검사한다.
- 기준선0012911의 기존 corpus **2,317건 회귀0** (CJK86 포함).
  text/span 및 trace의 선택행 kind/key/value/layer 등은 모두 동일. 새9행 뒤에 로드되는
  CJK system 및 domain/customer trace217개의 선언번호만+9 이동한다. 같은 layer 상대순서/동률 불변.
  이 비교는 이번 일반 자원 추가의 상대순서를 검사하며 기존 CJK migration gate를 수정하지 않았다.
- GCC 관련 **6/6 + 27/27**. 최종 Unicode 경계 수정 이후 해당3개를 다시 실행해 **3/3**.
  Clang ASan+UBSan 관련 **9/9**(46.10초), 최종 경계 수정 후 **2/2**(9.04초), 진단0.
  이번 턴에 GCC/ASan 전체 suite를 다시 돌렸다고 표시하지 않는다.
- 외부 Sproat 개발 표본 **3,785문장 /46,213span**, 전후exact35,313 동일,
  streaming11,355회 불일치0. 두 results.jsonl SHA256 모두
  `3f2d4d69e9a7c5466bd884c41a5623d6e2e50d9273e75915b47befe44fa4bf79`.
  held-out/독립 정답이 아니며 이번 합성표본21건을 외부 정확도 향상으로 세지 않는다.
- reference audit **51파일·1,648건**, 구조 결함0, 사람승인0; 새 corpus는 pending review.
  웹 `abbreviation-dotted-v21`/CTest/audit 등록. 기존 gold/외부 데이터 변경·반입 없음.

### 재현과 인계 파일

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure -R '^abbreviation_dotted_'
python3 tools/quality/tn_cycle.py cycle --baseline artifacts/tn/abbreviation-dotted/baseline --candidate artifacts/tn/abbreviation-dotted/final-candidate --corpus tests/evaluation/abbreviation_dotted_v21.jsonl --out artifacts/tn/abbreviation-dotted/NEW-comparison
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-v20-asan --output-on-failure -R '^abbreviation_dotted_'
```

`artifacts/tn/abbreviation-dotted/`: baseline/초기candidate/최종final-candidate snapshot,
최종cycle·기존전수비교JSON/스크립트, GCC/ASan 로그, reference audit를 보관한다.
`benchmark/generated/sproat-v21-dotted-{before,after}`와 `sproat-v21-dotted-comparison.json`은
로컬 측정 증거다. 원본 경로는 `/tmp/ttf-research-v11/sproat-bounded/sentences.jsonl`,
입력 SHA는 기존 `6b0a0a56a4955166ca1786dc6778ab744e372219cc4d8ab6bcf4d51943c2dd51`.
[약어 편집 가이드](english-forms.md#dotted-word-abbreviations-v021-continuation)에 표/옵션/한계가 있다.

다음은 이번 TN 변경의 커밋·배포 범위 확인이다. 이후 자원표는 **speech_character**를
한 규칙군으로 다룬다. 다의적 Co./Inc./No./St., e.g./i.e., 소유격은 별도이며 맨 연도·
NUM-ZERO·NUM-WIDTH·로마숫자·EVAL-03의 기존 우선순위/정책 보류를 유지한다.

## 이전 인계: CJK 정규화 — 1~4단계 완료, 기준선 `0012911`

권위 문서: [cjk-korean-reading.ko.md](cjk-korean-reading.ko.md), 특히 §5.4/§5.5/§9/§18.
Base `4386841`, HEAD `d9d3342` (이전 승인된 trace 비교 게이트 커밋).
사용자가 CJK 기준선 커밋 후 KO/EN 품질 개선 재개를 승인했다.
이 커밋은 사용자 준비 설계·생성기·데이터·86계약 및 검증된 CJK 구현을 함께 고정한다.
push·배포·서비스 재시작은 수행하지 않았다. 다음은 KO/EN 자원표 한 규칙군이다.

### 확정된 동작과 범위

- 한자·가나 기본값은 `passthrough`; CJK source는 `und` 유지. 두 옵션 독립.
  `--hanja korean`: `내 이름은 張勳입니다.` → `내 이름은 장훈입니다.`.
  `--kana korean`: `ソウル うどん ボウル` → `서울 우동 보우루`.
  둘 다 켜면 `東京とうきょう` → `동경도쿄`.
- ABI v1 유지, frontend options는 v0.20 prefix32B부터 current40B까지 받는다.
  부족한 필드 기본값, 과대 크기 거부. legacy initializer는32B만 기록한다.
- Han/Kana는 script enum 끝에 추가; 기존 토큰 병합/문맥 점수는 legacy-equivalence 유지.
  unified scanner, runtime 두음/어두 변이, preferred첫 독음, 전용 word index,
  촉음/발음의 받침 결합 및 미결합 원문 보존, halfwidth 최장 fold를 구현했다.
- 한자8,116 / 가나301 / 단어5행 고정. 기대값/TSV 수동 변경 없음.
  계약86 SHA256 `721c212c3ab21c156578725b917de8c6b0c8e9dc8968ce75af85805d6a7e558e`.
  가나27규칙 + 이전pending3 =30/30, 전체86/86, pending0.
- 미등록/간체/신자체는 원문 보존. speech drop은 낱말 간격 유지(`山𠮷水`→`산 수`).
  가타카나 모음 장음 흡수 없음(`トウキョウ`→`도우쿄우`); 히라가나만 흡수한다.
  형태소 분석/원어 복원/독립 언어 감수/외부 한국어 TN gold는 범위 밖이다.
- 스트리밍은 기존 phrase 보호를 최대6cp CJK word 길이만큼 확장했다.
  별도 전체 문장 버퍼 없음. 86건×2profile, 모든 2조각 byte경계/1byte/random
  **2,488회 text/span/trace 일치**, flush 전 문장/구 callback 확인.
- `generate.py build --check`는 세 TSV를 임시 재생성해서 LF 포함 바이트 비교한다.
  자동 외부 다운로드 없음. 사용자 contract는 simulator로 재생성하지 않는다.

### 마지막 수정: §5.5 조건부 로드 + §5.4 전용 힙 인덱스

공용 해시에 CJK 행을 남기지 않는 것만으로 warm throughput은 회복됐지만,
사용자가 기동마다 미사용8,422행을 파싱하는 비용을 확인했다(§9.2).
manifest를 유지하는 (b)를 채택했다. create는 CJK 세 경로를 건너뛰고,
freeze에서 최종 옵션에 맞는 reading 파일과 공통 word 파일을 SYSTEM으로 읽는다.
둘 다 passthrough 및 setter 없는 legacy는 CJK 파일을 **열지 않는다**.

- 원래 manifest/validator/레이어를 유지. `.tsv`, `.bin`, `.ttfb` 예약 basename 지원.
  실패 시 이번 freeze의 행과 declaration 번호를 모두 되돌리고 mutable 상태로 남긴다.
  마지막 파일 오류, final validator 오류, 수정 후 재시도, 중복 freeze를 검증했다.
  CJK index 할당은 마지막 실패 가능 작업; 성공 후 원래 엔트리만 옮긴다.
- 먼저 로드한 동일 SYSTEM 행은 계속 우선, customer/user override 불변.
  공용 해시는3,459행. 전용 인덱스는 heap, 엔진은포인터1개/17,320B.
  `e->rules` 미사용. word bucket은 원래 행을 빌리며 destroy가 owning 행을 한 번씩 해제.
- `resource_root=NULL`도 create에서 확정된 같은root를 사용한다.
  공개 freeze 이후load는 `TTF_ESTATE`. validation/get은 **현재 로드된 모든 행**을 본다.

| 옵션 (추가 override 없음) | freeze 전 record | freeze 후 record |
|---|---:|---:|
| 둘 다 passthrough / legacy | 3,459 | 3,459 |
| hanja만 korean | 3,459 | 11,580 |
| kana만 korean | 3,459 | 3,765 |
| 둘 다 korean | 3,459 | 11,881 |

### 검증 결과

- 이전 GCC 전체 **161/161 통과**는 그대로 보존하고 전체를 다시 실행하지 않았다.
  이번 수정 영향 범위 **CJK9/9 + 리소스·사전·trace·golden 등23/23** 통과.
- passthrough **2,235건 출력 회귀0**; 허용script9곳 외 메타데이터 차이0.
  기존 declaration offset7건도 이제0. 상대순서/선택행 불변.
- GCC CLI 기동25표본 중앙값: **17.972ms /16.5ms =1.089×**, 한도1.25 통과.
  참고용 Korean: hanja43.889ms, kana19.065ms, both44.704ms.
- 13,776B 순수한글 warm throughput: 기준71.910ms → **74.632ms (1.038×)**,
  +18,000단어74.910ms (**1.042×**), 한도1.25 통과.
  두 옵션 Korean/비CJK입력 참고75.0ms. 모든 성능 측정은 suite와 겹치지 않았다.
- Clang ASan+UBSan `evaluation_reports` **30.37초 통과**, 기존60초 제한 불변.
  corpus109건 및 harness의 후속 검사를 통과. 평가 러너/기동 횟수/코퍼스 변경 없음.
- 새 로드/롤백/해제·동시읽기 sanitizer **7/7**, 메모리·누수·UB 진단0.
- 이전에 못 실행한 sanitizer **24/24 통과 (276.04초)**. 새 전체테스트 수는 GCC162/ASan145;
  이번에 전체를 처음부터145/145 실행했다고 보고하지 않는다. 이름 목록으로 이전24개를 재개했다.
- compiler manifest3파일 네 옵션의 출력/trace일치; lexicon inventory/references/
  generation--check 통과. `git diff --check` 통과. 최종 snapshot도 보존했다.

### 재현 명령과 증거

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure --stop-on-failure -R '^cjk_'
python3 tests/integration/test_cjk_passthrough.py --before artifacts/tn/cjk-v22/baseline/libtts_text_frontend.so --before-resources artifacts/tn/cjk-v22/baseline/resources --after build/libtts_text_frontend.so --after-resources resources --report artifacts/tn/cjk-v22/NEW-passthrough.json
python3 tools/cjk/check_startup.py --ttf build/ttf --resources resources --report artifacts/tn/cjk-v22/NEW-startup.json
python3 tools/cjk/check_throughput.py --before artifacts/tn/cjk-v22/baseline/libtts_text_frontend.so --before-resources artifacts/tn/cjk-v22/baseline/resources --after build/libtts_text_frontend.so --after-resources resources --report artifacts/tn/cjk-v22/NEW-throughput.json
cmake -S . -B build-v20-asan
cmake --build build-v20-asan -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --test-dir build-v20-asan --output-on-failure --stop-on-failure -R '^evaluation_reports$'
# 별도 실행: 같은 환경 변수로 asan-resume-regex.txt의 이름24개 선택
```

`artifacts/tn/cjk-v22/stage4-lazy/`에 gcc/asan 빌드·검증 로그, 이전144개 목록,
재개24개 이름/regex/선택검증JSON, frozen계약보고서, passthrough/startup/throughput.json,
status.json, CJK소스조사표가 있다. `stage4-lazy-candidate/`는 최신 immutable snapshot.
`stage4/`의 이전 전체GCC와 sanitizer실패 로그는 덮거나 삭제하지 않았다.
과거수치/원인변경은 설계§18에 남겼다. `stage4-cycle/`의 stage1→stage4 비교는
8/86→86/86, fixed78/regressed0. 이번 로드 변경은 정확도 개선으로 다시 세지 않는다.

### 다음 작업 (별도 승인/범위)

1. CJK 기준선 커밋 이후 KO/EN 자원표 한 규칙군을 진행한다. push/배포는 별도다.
2. 배포/웹 CJK 선택기는 별도 진행. live 서비스는 이전버전 그대로다.
3. 공용 hash 전체 힙화/동적 크기 및 layer별 counter 변경은 별도 핵심저장소 작업.
4. 독립 감수·실제 입력 검토는 가능하나 간체사전/단어 coverage확장은 이번 범위 밖이다.

## 이전 인계: 날짜 뒤 영문 요일 / 자원표 점검 (base `842dbfd`, 2026-09-27)

이전 조사표 커밋 `842dbfd`의 GitHub Actions 전체 성공을 확인한 뒤, 5번의
month/weekday alias 규칙군 하나를 진행했다. 다음 작업을 자동으로 시작하지 않는다.

- 기존 월 alias 24개는 모두 정상. 기존 한글 weekday cue 14개도 점검했다.
  영문 alias 14개 + KO/EN 읽기 14개를 추가했다. `마감은 5/8(Fri)입니다.` →
  `마감은 오 월 팔 일(금요일)입니다.`.
- `languages/en-US/date.c`가 날짜에 바로 붙은 괄호 안 alias만 분류한다.
  공통 IR은 `DATE(value=1..7, unit=weekday, secondary=접힌 alias)`이며 KO/EN
  verbalizer가 각자의 `weekday` 표로 렌더링한다. Core는 단서의 대소문자·점 처리와
  layer 전달만 일반화했다. 공개 구조체/symbol/ABI 변경 없음.
- 상세 trace에 실제 `weekday_label`/`weekday` 행을 기록한다. 괄호는 독립 span이며
  read/preserve/skip을 유지한다. 사용자 파일의 alias 추가·0 비활성·읽기 override를 검증한다.
- 신규 `calendar_alias_v21.jsonl` 72건을 구현 전에 직접 작성했다. 최초 precheck의
  분수 negative 3건은 `five eighths`로 작성돼 기존 `fraction_over=over`와 달랐다.
  **구현 전** 정책 검토에서 정정하고 원본을 artifacts에 보관했다. 최종 같은 corpus의
  baseline 33/72 → candidate 72/72, fixed 39 / regressed 0. 정정 3건을 개선으로 세지 않았다.
  기존 활성 corpus 기대값 수정은 없다. 독립 사람 검수 및 외부 KO gold는 아직 없다.
- corpus SHA256: `37f7d7d908e12e959195130576917362d489e3e70e5f0b2aa28372a7498d673c`.
  `artifacts/tn/calendar-alias/`에 baseline, 최초 reference/precheck, 개발 결과,
  candidate/comparison을 보존한다. 최종 경계값 수정 후 캡처·검증 위치는 아래에 기록한다.
- CTest·reference audit·웹 `calendar-alias-v21` 등록.
  [달력 별칭 편집 가이드](calendar-alias-resources.md), Semantic IR/validation/THIRD_PARTY 갱신.
- 외부 영어 표본 3,785문장·46,213span 결과는 이전과 동일. exact 35,313 및 chunk
  11,355회 불일치 0. 결과 SHA256 `3f2d4d69e9a7c5466bd884c41a5623d6e2e50d9273e75915b47befe44fa4bf79`.
  이번 합성 표본에서 확인한 개선을 외부 정확도 향상으로 주장하지 않는다.
- 남은 범위: prefix 요일(`Mon., May 8`), 날짜/괄호 사이 공백, 한자 요일 및 한글 약자
  확장, 날짜-요일 불일치 진단. 날짜 lookbehind는 96바이트로 제한한다.
  다음 자원표는 **abbreviation** 또는 **speech_character** 중 하나만 선택한다.
  맨 연도·NUM-ZERO·NUM-WIDTH·로마 숫자·EVAL-03은 별도 범위다.

재현 명령:

```sh
ctest --test-dir build --output-on-failure -R 'calendar_alias|resource_semantic_validation'
python3 tools/quality/tn_cycle.py cycle \
  --baseline artifacts/tn/calendar-alias/baseline \
  --candidate artifacts/tn/calendar-alias/final-candidate \
  --corpus tests/evaluation/calendar_alias_v21.jsonl --out artifacts/tn/calendar-alias/NEW-comparison
```

검증·배포:

- GCC Debug 전체 **152/152 통과**, 692.12초 (`/tmp/ttf-calendar-all-tests.log`).
  이 전체 실행은 `candidate` snapshot의 바이너리다. 이후 허용 최대 31자 alias에
  끝 점을 붙였을 때 공통 단서 버퍼가 부족한 경계를 수정했다(32 → 33바이트).
  최종 바이너리는 관련 **4/4 통과** (`/tmp/ttf-calendar-final-targeted.log`):
  신규 corpus/native byte split, resource semantic validation, compiled resources.
  이 최종 한 바이트 경계 수정 후 로컬 전체 152개를 재실행했다고 표시하지 않는다.
- Native 비교 **2,122회**: 두 분절 profile에서 모든 두 조각 UTF-8 분할의
  text/span/trace 일치. 고객 alias 추가·비활성·읽기 교체, 31자+점, KO context/source,
  bracket read/preserve/skip 및 부정 사례를 별도 검사했다.
- 최종 Clang ASan/UBSan 관련 **6/6 통과**, 73.07초
  (`/tmp/ttf-calendar-sanitize.log`): context score cache, resource validator, 신규 corpus,
  bracket corpus, English date value contracts, compiled resources. sanitizer 전체는
  로컬 재실행하지 않았으며 push CI가 전체 GCC/Clang/ASan/UBSan을 실행한다.
- 최종 `artifacts/tn/calendar-alias/{final-candidate,final-comparison}`: fixed 39,
  unchanged-pass 33, regressed 0. 기준문 SHA는 위와 동일하다.
  외부 재평가 `benchmark/generated/sproat-v21-calendar-alias-final` 및
  `sproat-v21-calendar-alias-final-comparison.json` 역시 기존 결과 SHA와 완전히 같다.
- Reference audit: 50파일·1,618건, 구조 결함 0, 사람 승인 0 / 전체 독립 검수 대기.
  `artifacts/tn/calendar-alias/final-reference-audit.json`.
- Release `build-web` 재빌드, `ttf-workbench.service` 재시작 완료. 브라우저 gate 통과
  (`/tmp/ttf-calendar-browser.log`), live API 신규 corpus **72/72**, 상세 trace와
  chunk parity 확인 (`artifacts/tn/calendar-alias/live-web.json`).
  `/api/health`는 `ok=true, version=0.20.0`; 이번에도 release 번호는 변경하지 않았다.
- 편집 가이드의 TSV 예제를 `validate-resource --require-complete-validation`으로 검사하고
  `마감은 오 월 팔 일(금)입니다.` 출력도 확인했다. 기존 동작을 바꾸는 고객 정책은
  시스템 기본 사전에 합치지 않았다.
- `gh run list --branch main --limit 1`로 이번 push의 전체 CI를 확인한다.
  위는 로컬 실행 증거이며 pending CI를 성공으로 기록하지 않는다.


---

## 이전 인계: 조사 자원표 한 규칙군 (base `aa562a7`, 2026-09-27)

사용자가 제시한 여섯 후보 중 **5번의 조사표**를 선택했다. 다음 작업을 자동으로 시작하지 않는다.

- `resources/languages/ko-KR/rules/context.tsv`에 의무적 조사 쌍 8개를
  `particle_form` 16행 + `particle` 16행으로 선언했다. 공개 ABI 변경 없음.
- 전체 회귀 검사에서 기존 고객 문자표 계약의 오인식이 드러났다: `가漢나`가 `가한이나`,
  `가1️⃣나`가 `가키캡일이나`가 됐다. 기대값은 그대로 두고 `normalize.c`에서 미분류 OTHER
  identity / unreadable-cluster 이름 치환 뒤에는 조사 교정을 생략하도록 좁혔다.
  문자 이름 출력이 명사 경계라는 근거는 없으므로 자기 span만 바꾼다. 이는 해당 문자 뒤의
  실제 조사도 자동 교정하지 않는 보수적 경계이며, 별도의 의미 분류가 필요한 후속 범위다.
- `3랑` → `삼이랑`, `GPU이랑` → `지피유랑`, `1만원라도` → `만 원이라도`.
  마지막 사례는 금액 경계까지 복구하며 CURRENCY(value=10000, unit=KRW)를 유지한다.
- 변경 **전에** 고정한 `particle_table_v21.jsonl` 72건: baseline 35/72 → candidate 72/72,
  fixed 37 / regressed 0. corpus SHA256
  `346d129829bf4d9a942cdfb92fd606ed40902697b86283c2b621fd52bb65d457`.
  기존 기대값은 바꾸지 않았다. 독립적인 사람 검수 및 외부 KO gold는 여전히 없다.
- `tests/integration/test_particle_resources.py`: 두 분절 설정의 모든 두 조각 UTF-8 분할
  1,390회에서 text/span/trace 동일. 금액 값, 선택 resource의 입력 offset·layer,
  customer 조사표 override 및 user 발음에 따른 교정 검사도 통과.
- `tn_cycle` snapshot/cycle: `artifacts/tn/particle-table/{baseline,final-candidate,final-comparison}`.
  최초 resource-only candidate/comparison은 발견 과정의 기록으로 별도 보존했다.
  외부 영어 `sproat-v21-leadzero` 대 `sproat-v21-particle-table-final`: 3,785문장·46,213span,
  exact 35,313 그대로, chunk 11,355회 불일치 0. 결과 파일 SHA256 양쪽 동일:
  `3f2d4d69e9a7c5466bd884c41a5623d6e2e50d9273e75915b47befe44fa4bf79`.
  비교는 `benchmark/generated/sproat-v21-particle-table-final-comparison.json`.
- corpus는 CTest·reference audit·웹 `particle-table-v21`에 등록했다.
  사람용 [조사표 편집 가이드](particle-resources.md)와 THIRD_PARTY 참고 내역을 추가했다.
- 남은 범위: 조사 연쇄(`이랑은`), 서술격 선택 축약, 호격, 문맥 prefix 오인식의 형태소 분석.
  다음에는 5번의 **month/weekday alias** 표를 한 규칙군으로 훑는 작업을 권한다.
  맨 연도·NUM-ZERO·NUM-WIDTH·로마 숫자·EVAL-03은 이번에 다루지 않았다.

재현 명령:

```sh
python3 tools/quality/tn_cycle.py cycle \
  --baseline artifacts/tn/particle-table/baseline \
  --candidate artifacts/tn/particle-table/final-candidate \
  --corpus tests/evaluation/particle_table_v21.jsonl --out artifacts/tn/particle-table/NEW-comparison
ctest --test-dir build --output-on-failure -R 'particle|compiled_resources'
```

검증·배포:

- Debug 전체 150개 실행에서 149개 통과, `user_pattern_contracts` 1개 실패로 위 문자 치환
  회귀를 찾았다 (`/tmp/ttf-particle-table-ctest.log`). 경계 수정 뒤 관련 **9/9 통과**
  (`/tmp/ttf-particle-table-guard2-check.log`). 최종 바이너리로 전체 150개를 다시 실행했다고
  표기하지 않는다. push CI의 GCC/Clang 전체 실행은 해당 main commit의 Actions에서 확인한다.
- 최종 ASan/UBSan 관련 **6/6 통과**: user-pattern 계약, 기존/신규 particle corpus,
  resource validation/compiler, context cache. 30k-number는 **58.67초**로 기존 60초 제한 유지.
  `/tmp/ttf-particle-table-final-sanitize.log` (이번에 sanitizer 전체 suite를 재실행하지 않음).
- 기준선부터 작성한 참조 SHA는 그대로이며, 최종 snapshot/cycle fixed 37 / regressed 0.
  추가한 참조 audit는 결함 0, 72건 전부 독립 검수 대기.
- 브라우저 gate 통과 (`/tmp/ttf-particle-table-browser.log`). 문자 경계 수정 후에는
  Release `build-web` 재빌드·서비스 재시작, live API로 금액·약어 조사 및 한자/키캡
  문자 치환 4건을 다시 확인했다. 웹 `particle-table-v21`은 72/72 통과.
  `/api/health` = `{"ok":true,"version":"0.20.0"}`. 릴리스 번호는 이번에 변경하지 않았다.
- `gh run list --branch main --limit 1`로 push CI 결과를 확인한다. 이 절은 로컬 증거이며
  pending CI를 성공으로 표시하지 않는다. 로그와 raw 외부 corpus는 Git에 넣지 않는다.

이후의 과거 기록에서 "커밋·미배포"는 각 작성 당시 상태이므로 현재 상태는 `git log`와
서비스 설정으로 확인한다.

## 최근 작업: speech 최종 관문 (2026-09-22, 커밋·미배포)

사용자 사례(www., 가격대, k7, top100, 20F, 유&미, 1+1, 이메일, 제어 문법)는 v0.20에서
이미 기대대로 나옴을 재현했다. 변형 50여 건에서 찾은 약점 중 **C. 최종 관문**을 처리했다.

- 변경: `src/core/speech_output.c`(기호 이름 띄어쓰기, 늘임표, 문맥 인자),
  `src/core/normalize.c`(TN 단계 늘임표, drop 뒤 공백), ko/en validator,
  `resources/{common,languages/*/rules}/speech.tsv`, en `numeric.tsv` 호환 단위 alias.
- 계약: `tests/evaluation/speech_final_gate_v21.jsonl` 40건. 구현 전에 작성했다.
  단어 사이 제거는 사용자 결정(2026-09-22)으로 한 칸을 남긴다 (`가🙂나` → `가 나`).
  v0.20 corpus 4건과 통합·웹·browser 기대값을 같은 결정으로 갱신하고 notes에 기록했다.
- 결과: `tn_cycle cycle` fixed 31 / unchanged-pass 9 / regressed 0
  (`artifacts/tn/final-gate/comparison/cycle.json`, 40건). 단어 사이 제거 결정 반영 후
  Debug CTest 98/98, ASan/UBSan 86/86(단독), Release text-evaluation 33/33,
  browser gate 통과(임시 port 8123, `/tmp/ttf-web-browser`의 Node 22·Playwright).
  외부 영어 3,785문장 results SHA `2de85573…` v0.20과 동일
  (`benchmark/generated/sproat-v21-final-gate`). `build-web` 배포 library는 바꾸지 않았다.

### A. `&` 이니셜·대문자 한 글자 (2026-09-22, 커밋·미배포)

- 변경: `languages/en-US/pack.c`(`ampersand_initialism` 분류), `src/pronunciation/foreign.c`
  (`&` 분해 렌더링, 대문자 한 글자 이니셜). 계약 `tests/evaluation/letters_v21.jsonl` 26건.
- 결과: cycle fixed 20 / unchanged-pass 6 / regressed 0
  (`artifacts/tn/amp-initialism/comparison/cycle.json`). Debug 99/99, ASan/UBSan 86/86(단독).
  외부 영어 출력·exact 동일, `A&M`/`DP&I`/`D&D` IR만 INITIALISM
  (`benchmark/generated/sproat-v21-letters-comparison.json`).
- 남음: `Johnson&Johnson` → `Johnson앤Johnson` 혼합 렌더, 단독 2글자 대문자 단어 충돌.

### B. 이메일·URL 단어·버전·IP (2026-09-22, 커밋·미배포)

- 변경: `src/pronunciation/electronic.c`(주소 렌더러), `src/semantic/classifier.c`
  (`bare_domain_end`, `v` 접두 버전), ko/en `dotted_numbers`, validator 4종
  (`url_tld`, `electronic_word|number|separator`), 사전 11항목(pending).
- 계약 `tests/evaluation/electronic_v21.jsonl` 34건. `test@`는 기존 사전 항목이라
  `테스트`가 맞다는 작성 오류를 커밋 전에 바로잡고 notes에 기록했다.
  v0.20 이메일 계약 2건은 정책 변경으로 갱신했다(`닷 씨오엠` → `닷 컴`).
- 결과: cycle fixed 26 / unchanged-pass 8 / regressed 0
  (`artifacts/tn/electronic/comparison/cycle.json`). Debug 99/99(+신규 gate 통과),
  ASan/UBSan 88/88(단독), Release text-evaluation 35/35, browser gate 통과. 외부 영어 출력 동일
  (`benchmark/generated/sproat-v21-electronic-comparison.json`).

### D. 수식·비교·해시태그 (2026-09-22, 커밋·미배포)

- 변경: `src/semantic/classifier.c`(`math_expression_end`), ko/en `expression` 렌더러,
  `src/core/normalize.c`(식의 문맥 중립), `src/core/speech_output.c`(`speech_symbol_prefix`),
  `C#` semantic_term·사전, `tools/lexicon/manage.py`(`c#` 키 허용).
- 계약 `tests/evaluation/math_hashtag_v21.jsonl` 37건. 한국어 `#`=샵은 사용자 결정이며
  final-gate 계약 1건(`해시 해시태그`)을 같은 결정으로 갱신했다.
- 결과: cycle fixed 28 / unchanged-pass 9 / regressed 0
  (`artifacts/tn/math-hashtag/comparison/cycle.json`). Debug 101/101, ASan/UBSan 89/89(단독),
  Release text-evaluation 36/36, browser gate 통과. 외부 영어 결과 파일 동일
  (`benchmark/generated/sproat-v21-math-comparison.json`).

### E. `대` 구간·대수, 양끝 배수 범위 (2026-09-22, 커밋·미배포)

- 변경: `languages/ko-KR/pack.c`(`classify` → `band_role` 후처리, `ascending_integers` 범위 상속,
  `range_repeat_unit`), `counters.tsv` cue·band 리소스, validator.
- 재현 중 `20대 남성` → `스무 대`(연령이 대수로 읽힘) 값 오류를 발견해 같은 규칙군으로 처리했다.
  기존 golden `1억-2천만원` 보존 계약을 지키도록 오름차순일 때만 범위로 만든다.
- 계약 `tests/evaluation/counter_band_v21.jsonl` 25건. cycle fixed 14 / unchanged-pass 11 /
  regressed 0 (`artifacts/tn/band/comparison2/cycle.json`). Debug 101/101, ASan/UBSan 89/89(단독),
  외부 영어 결과 파일 동일(`benchmark/generated/sproat-v21-band`).

### F. 층·지하층 (2026-09-22, 커밋·미배포)

- 변경: `languages/ko-KR/patterns.c`(`ttf_ko_classify_basement`), patterns.tsv cue·리소스, validator.
- 계약 `tests/evaluation/floor_v21.jsonl` 19건. cycle fixed 11 / unchanged-pass 8 / regressed 0
  (`artifacts/tn/floor/comparison/cycle.json`). Debug 102/102, ASan/UBSan 90/90(단독),
  외부 영어 결과 파일 동일(`benchmark/generated/sproat-v21-floor`).
- 새로 발견: 조사 교정 규칙군(`3F로` → `삼 층로`, `5kg로`, `1+1는`). remaining-work에 기록.

### D 잔여: 소문자 한 글자·메뉴 경로 (2026-09-22, 커밋·미배포)

- 변경: `src/pronunciation/foreign.c`(소문자 한 글자), `languages/en-US/pack.c`(`hyphen_letters`,
  `joined_letters` 문맥 중립), `src/core/speech_output.c`(`speech_silent`), 리소스·validator.
- 계약 `tests/evaluation/letters_paths_v21.jsonl` 20건. cycle fixed 14 / unchanged-pass 6 /
  regressed 0 (`artifacts/tn/letters-paths/comparison/cycle.json`). Debug 103/103,
  ASan/UBSan 91/91(단독), 외부 영어 결과 파일 동일.
- 결정 필요: `Z` 한국어 이름 `지` vs `제트`(`Z세대`).

### Z 읽기와 `#`숫자 (2026-09-22, 커밋·미배포)

- 사용자 결정: `Z`=제트(G=지와 구분), `#1`=일 번 / number one.
- 변경: `phones.tsv` letter.Z, `src/semantic/classifier.c`(`syntax.number-sign`), ko/en
  `number_sign` 템플릿, 구두점 앞 공백을 넣지 않도록 `@math`/`@number-sign` 간격 규칙 보완.
- 계약 `tests/evaluation/letter_number_sign_v21.jsonl` 17건. D의 `번호#123` 기대값을 같은
  결정으로 갱신했다. cycle fixed 10 / unchanged-pass 7 / regressed 0. Debug 104/104,
  ASan/UBSan 92/92(단독), 외부 영어 결과 파일 동일.

### 조사 교정 (2026-09-22, 커밋·미배포)

- 변경: `src/core/normalize.c`(`particle_agreement`, `following_particle`),
  `src/semantic/classifier.c`(`#N` 뒤 조사 허용), `context.tsv`의 `particle_form`, validator.
- 공백 없이 붙은 조사 span만 대상으로 한다. `이며/며` 같은 선택적 축약은 표에서 제외했다.
- 계약 `tests/evaluation/particle_v21.jsonl` 20건. cycle fixed 8 / unchanged-pass 12 /
  regressed 0. Debug 105/105, ASan/UBSan 93/93(단독), 외부 영어 결과 파일 동일.
- 기존 fixture 4곳(`고객는`, `정확는`, `패턴는`, `관용형는`, `유에스커스텀를`)은 비문이라
  교정형으로 갱신했고, alias 테스트는 조사 규칙을 테스트 안에서 따로 구현해 비교한다.

### 혼합 대소문자·숫자+문자 (2026-09-23, 커밋·미배포)

- 데이터만 변경: 사전 15항목(pending), `semantic_term` 5G/4G/3G/2D, `tools/lexicon/manage.py`의
  숫자 선행 라벨 키 허용. 코드 변경 없음.
- 계약 `tests/evaluation/mixed_case_v21.jsonl` 20건. cycle fixed 13 / unchanged-pass 7 /
  regressed 0. Debug 106/106, ASan/UBSan 94/94(단독), 외부 영어 결과 파일 동일.

### 대표번호·괄호 전화번호 (2026-09-23, 커밋·미배포)

- 실제 문장 훑기에서 `1588-0000` → `천오백팔십팔에서 영`(값 오독), `연락처(010-...)` 미읽기를 발견.
- 변경: `src/semantic/phone.c`(`service_number`, `bracketed_number`, 닫는 괄호 tail 허용),
  ko `phone_service_prefix` 16개, validator.
- 계약 `tests/evaluation/phone_service_v21.jsonl` 17건. v0.18 `1588-0000` 범위 계약을 전화로
  갱신하고 사유를 기록했다. cycle fixed 9 / unchanged-pass 8 / regressed 0.
  Debug 107/107, ASan/UBSan 95/95(단독), 외부 영어 결과 파일 동일.
- 같은 훑기에서 발견한 남은 문제: `A/S` → `A slash S`, `SAVE20을` → `에스에이브이이투지로를`,
  `09:00` → `아홉 시 영 분`, `13.6인치` 띄어쓰기.

### 성능 회귀 수정 (2026-09-23)

push한 `30b6b98`의 CI sanitizer 작업에서 `context_cache_compatibility`의 30k-number 게이트가
60초 제한을 넘겼다(80초). 원인은 두 가지 per-span 비용이었다: ko `classify` 래퍼가 숫자
토큰마다 공통 분류기를 다시 호출한 점, 조사 교정이 모든 span에서 리소스를 조회한 점.
`band_unit_ahead`로 `대`가 뒤따를 때만 재분류하고, 조사 교정은 리소스 조회 전에 비ASCII·
한글 받침 검사를 먼저 한다. 제한은 바꾸지 않았다. 로컬 ASan 단독 실행 80초→50.9초
(기존 기록 48.5초), Debug 108/108, ASan/UBSan 96/96, 외부 영어 결과 파일 동일.

### 슬래시 약어 (2026-09-23, 커밋·미배포)

- 변경: `languages/en-US/pack.c`의 `hyphen_letters`/`joined_letters` 중립 판정에 `/` 추가.
- 계약 `tests/evaluation/slash_letters_v21.jsonl` 13건. cycle fixed 7 / unchanged-pass 6 /
  regressed 0. Debug 108/108, ASan/UBSan 96/96(단독), 외부 영어 결과 파일 동일.
- `SAVE20` → `에스에이브이이투지로`는 영어 발음 경로에서 여러 자리 숫자를 자릿수 영어
  이름으로 읽기 때문이며, 읽기 정책 결정이 필요해 보류했다.

### 정시 읽기 (2026-09-23, 커밋·미배포)

- 변경: ko `TTF_SEM_TIME` 렌더러에서 0분·0초 생략, `clock zero_minutes` 리소스, validator.
- 계약 `tests/evaluation/clock_v21.jsonl` 13건. 기존 fixture 3곳(`golden_ko`, `range_semantics`,
  `semantic_boundary_streaming`)의 `영 분`/`영 초` 기대값을 사유와 함께 갱신했다.
  cycle fixed 10 / unchanged-pass 3 / regressed 0. Debug 109/109, ASan/UBSan 97/97(단독),
  외부 영어 결과 파일 동일. push한 `9767c5f`의 CI는 성공이다.

### 슬래시 날짜 (2026-09-23, 커밋·미배포)

- 실제 문장 훑기에서 `9/25(목)까지` → `이십오 분의 구`(분수 오독)를 발견했다.
- 변경: `src/semantic/classifier.c`의 `slash_date`(연도 우선, 일>12, 괄호 요일 단서),
  ko `weekday_label` 리소스, validator. 영어도 `9/25` → `September twenty fifth`가 된다.
- 계약 `tests/evaluation/slash_date_v21.jsonl` 16건. cycle fixed 9 / unchanged-pass 7 /
  regressed 0. Debug 110/110, ASan/UBSan 98/98(단독), 외부 영어 결과 파일 동일.
  슬래시 약어 corpus의 `2026/09/23` 부정 예를 이 변경에 맞춰 갱신했다.

### 붙여 쓴 범위 표시 (2026-09-23, 커밋·미배포)

- 변경: `src/core/speech_output.c`의 `ttf_speech_range_mark`, `src/core/normalize.c` TN 단계 적용,
  ko/en `speech_range_mark` 리소스, validator. 반복 마크·공백·고객이 끈 구분자는 제외한다.
- 계약 `tests/evaluation/range_mark_v21.jsonl` 15건. cycle fixed 8 / unchanged-pass 7 /
  regressed 0. Debug 111/111, ASan/UBSan 99/99(단독), 외부 영어 결과 파일 동일.
- `1~2`·`1%~2%`의 기존 고객 계약은 `quantity_range_separator ~ 0`을 존중해 그대로 유지된다.

### 여러 토큰 구간 (2026-09-23, 커밋·미배포)

- 변경: `src/semantic/classifier.c`의 `same_unit_family`/`preceding_family_unit`,
  ko `range_unit_family` 리소스, validator. 앞 단어가 같은 계열 단위일 때만 각 끝을 읽는다.
- 계약 `tests/evaluation/multi_range_v21.jsonl` 11건. cycle fixed 4 / unchanged-pass 7 /
  regressed 0. Debug 112/112, ASan/UBSan 100/100(단독), 외부 영어 결과 파일 동일.
  기존 `1시~2분` 보존 계약은 앞 단어 근거 요구로 유지된다.

### 아이디·핸들 (2026-09-23, 커밋·미배포)

- 변경: `src/semantic/classifier.c`의 `@handle` 분류(글자/밑줄 시작, 밑줄 필요),
  `src/core/normalize.c`에서 주소 렌더러 사용·text 보존·문맥 중립.
- 계약 `tests/evaluation/handle_v21.jsonl` 11건. cycle fixed 8 / unchanged-pass 3 /
  regressed 0. Debug 113/113, ASan/UBSan 101/101(단독), 외부 영어 결과 파일 동일.
  숫자로 시작하는 `2millionUSD_foo`의 기존 보존 계약은 글자 시작 조건으로 유지된다.

### 영어 연도 문맥 (2026-09-23, 커밋·미배포)

- 프로젝트의 측정 데이터(외부 영어 표본)에서 가장 큰 실제 오류가 연도 읽기였다
  (`1965` → `one thousand nine hundred sixty five`).
- 변경: `date_year_cue`/`boundary_word`에 전치사·구간어 15개 추가(리소스만).
  DATE exact 319 → 343, 회귀 0. 계약 13건, Debug 114/114, ASan/UBSan 102/102(단독).
- 흔한 기능어(the/a/and/to/by/on)를 넣으면 DATE 401까지 오르지만 boundary_word 등록이
  chunk 크기에 따른 span 불일치를 만든다. remaining-work에 측정치와 함께 기록했다.

### planner의 연도 단서 처리와 구간 연속 (2026-09-23, 커밋·미배포)

- 앞선 "막힌 지점"을 해결했다. `src/segmenter/segmenter.c`가 `date_year_cue`를 floor cue처럼
  직접 읽는다. en validator는 한 단어 연도 단서에 한해 `boundary_word` 요구를 해제하고,
  다른 단서와 여러 단어 단서에는 그대로 요구한다.
- `languages/en-US/date.c`에 `interval_continuation`을 추가했다. `from X to Y`/`between X and Y`는
  **앞 끝이 단서로 확인된 연도**일 때만 뒤 끝을 연도로 본다.
- 결과: DATE exact 343 → 394(기준 319 대비 +75), CARDINAL 변화 없음, chunk 불일치 0.
  Debug 115/115, ASan/UBSan 103/103(단독). 계약 14건.
- v0.18 호환 참조(`context_cache_v18_reference.json`, `..._partial_...`) 4+1건의 `context_trace`만
  갱신하고 사유를 파일 purpose에 적었다. 텍스트·offset·의미·render 언어는 동일하다.

### 사용자 옵션 1: 괄호 안 처리 (2026-09-23, 커밋·미배포)

- 사용자가 요청한 옵션 네 가지(괄호 처리, 미등록 외국어 단어, 장식 문자, 대문자 약어) 중 첫 번째.
- 공개 구조체 `ttf_frontend_options_t`에 필드를 더하면 예전 헤더로 빌드한 소비자가 깨진다
  (`ttf_frontend_options_init`이 더 큰 구조체를 기존 버퍼에 쓴다; 실제로 native 테스트가 SEGFAULT).
  그래서 **새 함수** `ttf_engine_set_option(engine, name, value)`로 전달한다.
- 변경: `src/core/frontend_options.c`, `src/core/internal.h`(엔진 내부 필드),
  `src/core/normalize.c`(괄호 구간 처리), CLI `--brackets`, 웹 선택기, `bracket_pair` 리소스.
- 계약 `tests/evaluation/bracket_v21.jsonl` 11건. Debug 116/116, ASan/UBSan 104/104(단독),
  외부 영어 결과 파일 동일, browser gate 통과.

### 사용자 옵션 2: 미등록 외국어 단어 (2026-09-23, 커밋·미배포)

- `ttf_engine_set_option(engine, "unknown_foreign", "guess|spell|preserve")`, CLI `--unknown-foreign`,
  웹 선택기. 기본 `guess`는 기존 동작이다.
- 변경: `src/pronunciation/foreign.c`(`declared_pronunciation` 확인 후 정책 적용),
  `src/core/normalize.c`(보존된 단어는 최종 문자 패스를 건너뜀), 옵션 배선.
- 사전·발음 사전에 있는 단어(`Apple`, `Google`, `Hello`), 이니셜(`R&D`), 라벨(`k7`), 주소·식별자는
  정책과 무관하게 기존 계약을 유지한다.
- 계약 `tests/evaluation/unknown_foreign_v21.jsonl` 15건. Debug 117/117, ASan/UBSan 104/104(단독),
  외부 영어 결과 파일 동일, browser gate 통과. 기본 정책 사례는 cycle에서 회귀 0.

### 사용자 옵션 3: 장식 문자 이름 읽기 (2026-09-23, 커밋·미배포)

- `ttf_engine_set_option(engine, "decorative", "follow|read")`, CLI `--decorative`, 웹 선택기.
- 변경: `src/core/speech_output.c`(선언된 이름 읽기), `src/core/normalize.c`(이모지 cluster 경로),
  `speech_decorative` 리소스 ko/en 21개, validator.
- 계약 `tests/evaluation/decorative_v21.jsonl` 13건. Debug 117/117, ASan/UBSan 105/105(단독),
  외부 영어 결과 파일 동일.

### 사용자 옵션 4: 대문자 약어 읽기 방식 (2026-09-23, 커밋·미배포)

- `ttf_engine_set_option(engine, "initialism", "spell|word")`, CLI `--initialism`, 웹 선택기.
- 변경: `languages/en-US/pack.c`의 lexical 분류에서 전부 대문자 2글자 이상이면 약어로 본다.
  선언된 ACRONYM/PROPER_NOUN/INITIALISM과 사전 항목은 제외한다(NASA → 나사 유지).
- 계약 `tests/evaluation/initialism_v21.jsonl` 15건. Debug 118/118, ASan/UBSan 106/106(단독),
  외부 영어 출력 동일(IR 10건만 INITIALISM). 사용자 옵션 네 가지를 모두 마쳤다.

### 대문자 코드 단어 (2026-09-24, 커밋·미배포)

- 사용자 결정: `SAVE20` = `세이브 이십`.
- 변경: `src/pronunciation/foreign.c`의 ALNUM 경로에 `declared_word` 판정 추가. 사전 근거가 있으면
  코드 단어로 보고 대상 언어 수사와 띄어쓰기를 쓴다. 사전 7항목(save/sale/best/event/summer/winter)
  추가, `coupon`은 기존 금융 항목과 충돌해 제외했다.
- 사용자 결정(2026-09-24, 2차): 글자 라벨 뒤 **한 자리** 숫자도 원어 수사다 (`k7` → `케이세븐`,
  `A4` → `에이포`, `B2` → `비투`). 두 자리 이상은 대상 언어 수사(`G80` → `지팔십`), 선행 0은
  자릿수 유지(`k007`). 기존 계약 10건(v0.20 `k7`/`K7` 포함)을 같은 사유로 갱신했다.
- 사용자 결정(2026-09-24): 사전 단어 라벨은 **원어 수사**를 띄어 읽는다 (`top10` → `탑 텐`,
  `best5` → `베스트 파이브`, `SAVE20` → `세이브 트웬티`). `top100`은 관용 표기라 전체를 사전에
  등록해 `탑백`으로 둔다. 철자 라벨(`k7`, `MP3`, `GPT-4`)은 대상 언어 수사를 붙여 읽는다.
  v0.20 계약 2건(`top100`, `top10`)을 같은 사유로 갱신했다.
- 계약 `tests/evaluation/code_word_v21.jsonl` 10건. cycle fixed 3 / regressed 0.
  Debug 119/119, ASan/UBSan 107/107(단독), 외부 영어 결과 파일 동일.

### 미등록 단어 추측 발음 (2026-09-24, 커밋·미배포)

- 측정 우선. 사전에 없는 28단어의 기대 표기를 독립 작성하고 0/28을 기록한 뒤 고쳤다.
- 변경: `languages/en-US/pack.c`(g2p 앵커 `^`/`$`, 묵음 `-`), `graphemes.tsv` 규칙 추가,
  `languages/ko-KR/pack.c`(모음 뒤 `r` 삭제)와 `phone_mapping coda_r` 리소스, validator.
- 결과 0/28 → 14/28. 남은 14건과 원인은 `docs/research/oov-guess-v21-results.md`.
  사전 추가로 통과시키지 않았다. Debug 121/121, 외부 영어 결과 파일 동일.
- 기존 corpus 5건의 추측 발음 기대값(`레포르트`→`리포트`, `델이`→`데이리` 등)을 갱신했다.

### 미등록 단어 추측 발음 2차 (2026-09-24, 커밋·미배포)

- 남은 14건을 표기 규칙 세 가지로 처리해 **28/28**이 됐다. 규칙은 모두 리소스로 선언한다.
  - 어말 `P T K`는 짧은 모음 뒤에서만 받침이 된다(`phone_vowel_length`, market 마켓 /
    update 업데이트). 비음·유음 앞에서도 닫지 않는다(partner 파트너).
  - 모음 뒤 `l`은 겹쳐 적는다(`phone_mapping onset_l double`, platform 플랫폼).
  - `ER` 뒤 모음이 오면 r가 다음 음절 초성이 된다(`phone_mapping linking_r link`, 배터리).
  - 구개음 뒤 삽입 모음은 `이`, `SH`는 뒤 모음을 구개음화, 이중모음 꼬리가 받침을 가져간다.
- 변경: `languages/ko-KR/pack.c`, `validate.c`, `phones.tsv`, `graphemes.tsv`
  (`^con`, `nnel`, `^lice`, `nse$`, `ersi`, `on$`, `ake$`), 회귀 게이트 14 → 28건.
- 기존 참조 3건(`snake` 스내크→스네이크, `daily` 데이리→데일리, `IT` word 정책 이트→잇)의
  기대값을 갱신하고 사유를 각 notes에 남겼다. 프로토타입 출력을 적어 둔 값이었다.
- 결과: Debug CTest 122/122, 외부 영어 표본 수치 동일(`sproat-v21-oovguess`, DATE 394).
  자세한 기록은 `docs/research/oov-guess-v21-results.md`.

### 미등록 단어 추측 발음 3차 — 세트 확대와 과적합 점검 (2026-09-24, 커밋·미배포)

- 28단어 세트가 2~3음절 명사에 치우쳐 있어, 자음군·4음절 이상·파생형 30단어를 규칙 변경
  **전에** 작성했다(기준선 11/30 → 30/30).
- 이어서 새 규칙의 **이웃 단어** 18개를 다시 규칙 변경 전에 작성했다(기준선 9/18 → 17/18).
  이 세트가 `cate`(cater 카테), `^pers`(person 퍼스언), `nati`(nation 내션) 같은 한 단어짜리
  규칙을 잡아냈고, 공유 부분(`ateg`, `^per`+`son$`, `atio`)으로 바꿨다.
- 매퍼: 지운 `r`의 모음 길이, 어말 성절 자음, 활음 앞 자음, 구개음 뒤 `ㅖ`, `W`의 `ㅞ`/`ㅝ`.
- g2p 데이터에 형태소 경계 token `.`을 추가했다(`^up` → `AH P .`, uploading 업로딩).
  음운 규칙으로 일반화하면 application 애플리케이션이 깨지므로 데이터로 남겼다.
- 남은 실패 1건 national(네이셔널)은 강세 모음 축소 문제라 사전에 넣지 않고 기록했다.
- 게이트 75건(28+30+17), 측정 76건. Debug CTest 125/125, 외부 영어 수치 동일.

### 미등록 단어 추측 발음 4·5차 (2026-09-25, 커밋·미배포)

- 4차 27단어(동사 활용, 자음 뒤 `w`, 모음 사이 유성 `s`, 고유명사, 6음절 이상):
  기준선 5/27 → 25/27. 5차 24단어(4차 규칙의 이웃): 15/24 → 24/24.
- 새 자원 `phone_glide`(`merge|split`)로 자음 뒤 `w`를 끊는다(스위트·트윈, 퀵은 유지).
  `Y`는 항상 붙으므로 이 표를 보지 않는다 — 처음에 `Y`까지 끊었다가 리뷰가 리브유가 됐다.
- 자소 기본값 `o` → `OW`, 모음 사이 `s` → `Z`(자소 규칙).
- **자소 key 상한을 4자에서 5자로 올렸다**(`languages/en-US/pack.c` matcher와 validator,
  `docs/resource-validation.md`). 접미사를 한 행에 적을 수 있고 과적합도 줄어든다
  (`^chica`는 chicken을 건드리지 않지만 `^chic`였다면 시킨이 됐다).
- 측정 127단어 / 정확 124 / 게이트 124건. 남은 3건은 national(강세 모음 축소),
  michael(고유명사는 사전 대상), visit(기대값 다툼)이며 사전에 넣지 않고 기록했다.
- Debug CTest 126/126. 자세한 기록은 `docs/research/oov-guess-v21-results.md`.

### DATE 진단 지표와 괄호 연도 단서 (2026-09-25, 커밋·미배포)

- 평가기에 `mismatch_kinds`와 `diagnostic_case_insensitive`를 넣었다. 정확도는 그대로
  byte 비교이고 이 둘은 진단이다. DATE 불일치 1,144건 중 698건이 월 이름 대소문자 관행임이
  드러났고, 프로젝트는 입력 대소문자를 계속 보존한다.
- 실제 오류 344건의 갈래: 맨 연도 258, ISO 어순 46, 연대 표기 33, 기타 7.
  맨 연도 중 108건이 `( 1965 )` 인용 형태였다.
- `date_year_cue`가 여는 괄호도 key로 받는다. 단 **괄호가 숫자만 감싸고 바로 닫힐 때만**
  단서다(`( 2003 and 2005 )`는 그대로 cardinal). `born`도 추가했다. 계약 10건을 구현 전에
  작성했다(`year_cue_v21.jsonl` 14 → 24건).
- 기존 phone-negative 계약 2건(`( 2024 )`, `1385 ( 2006 )`)의 기대를 CARDINAL → DATE로
  바꿨다. **정책 변경**이며 근거와 되돌릴 조건을 `docs/research/date-diagnostic-v21-results.md`에
  남겼다.
- 외부 영어: DATE 394 → 504, span 35,053 → 35,163, 다른 클래스 동일. Debug 126/126.

### 영어 연대 표기 (2026-09-25, 커밋·미배포)

- `1960s`는 미정규화 상태였다. `en.date.decade` 분류 + `decade_plural` 자원 + 
  `classifier_priority en.date.decade 10`으로 처리한다. 계약 13건을 구현 전에 작성했다
  (`tests/evaluation/decade_v21.jsonl`).
- 0으로 끝나는 네 자리/두 자리 + `s`만 잡으므로 `1965s`·`787s`·`MP3s`는 그대로다.
- 외부 영어: DATE 504 → 532, span 35,163 → 35,191, 다른 클래스 동일. Debug 127/127.

### speech 모드 측정과 영어 이니셜 읽기 (2026-09-25, 커밋·미배포)

- 평가기에 `--output-mode speech`를 넣었다. 리포트는 `provenance.frontend_options`와
  `metric_policy`(`...-speech`)로 자기 정체를 밝히고, text baseline과 섞이지 않는다.
  speech baseline은 span 35,236이다(text 35,191과 비교할 수치가 아니다).
- LETTERS 538건 실패가 전부 `unchanged`였다. speech 모드에서 `FBI`를 그대로 내보내고 있었다.
- 전부 철자로 읽으면 PLAIN이 124건 퇴행한다(`EXCEL` → `e x c e l`). 판별식 5개를 결과
  파일로 오프라인 평가해 "2자 이하 또는 발음 불가"(+297/−36)를 골랐다. 음소배열 표
  `syllable_onset`/`syllable_coda`는 자원이다.
- `initialism_policy`에 `inferred_speech`(letters), `pronounceable_speech`(preserve) 추가.
  글자만이 아닌 값(`R&D`)은 제외 — 이 규칙이 없으면 `R and D`가 `r d`가 된다(기존 계약이 잡음).
- speech span 35,236 → 35,501, LETTERS 174 → 471, PLAIN −33. **text 모드는 35,191로 동일.**
  Debug 128/128. 기록은 `docs/research/initialism-speech-v21-results.md`.

### 한국어 대문자 약어 (2026-09-25, 커밋·미배포)

- `FIFA` → 에프아이에프에이처럼 단어로 읽어야 할 약어를 글자로 읽고 있었다. 계약 20건을
  먼저 작성했다(`tests/evaluation/ko_acronym_v21.jsonl`, 기준선 13/20).
- 영어 speech에서 쓴 음소배열 판별을 한국어 분류에 올리는 안을 이 세트로 먼저 채점했다:
  18/20이지만 틀리는 `CEO`·`LED`의 손해가 크다(영어는 보존일 뿐이지만 한국어는 `시오` 같은
  오독이 된다). 그래서 **분류를 건드리지 않았다**.
- 런타임은 이미 사전이 글자 읽기를 덮어쓴다(`RAM` → 램). 빠져 있던 것은 항목이었다.
  `lexicons/ko-KR/acronym-additions.jsonl`에 7건(피파·유네스코·나토·오펙·우에파·엑셀·심)을
  넣고 `tools/lexicon/manage.py build`로 다시 빌드했다. 출처는 `project-authored-v21-acronyms`,
  `review_status`는 pending이다. 20/20. 기록은 `docs/research/ko-acronym-v21-results.md`.

### 마침표 없는 약어 (2026-09-25, 커밋·미배포)

- PLAIN 실패 101건을 갈래로 나누니 약 40건이 마침표 없는 약어(`vol 1`, `Records ltd`)였고
  나머지는 영국식→미국식 철자 변환(데이터셋 관행, 따르지 않는다)이었다.
- 후보 key마다 표본의 참조 읽기를 세어 **한 가지로만 읽히는 것**만 declare했다:
  vol·ltd·vs·etc·jr·mr. `st`(saint 8 / street 1), `dr`(doctor 3 / drive 1), `sr`, `no`, `pp`는
  제외했다. 새 자원은 `abbreviation_bare`이고 `preserve`로 끌 수 있다.
- 계약 13건을 구현 전에 작성했다(`tests/evaluation/bare_abbreviation_v21.jsonl`).
- 외부 영어 text: PLAIN 33,860 → 33,881, span 35,191 → 35,212. Debug 130/130.

### per 단위와 누락 단위 (2026-09-25, 커밋·미배포)

- 작은 클래스의 실패를 훑다가 `193.5/km²`가 **기호 그대로** 나가는 것을 발견했다(7건).
  `20 degrees C`, `-2011 kl`도 미정규화였다.
- 원인: `/`로 시작하는 토큰은 경로 보호가 가져가고, 붙여 쓴 형태는 분수 분기가
  `syntax.invalid-fraction`으로 보존했다. **분모가 숫자가 아닐 때만** 단위 접미사를 시도하게
  좁혔다(`5/6`은 그대로 분수, 띄어 쓴 `53.5 /km²`는 그대로 경로 보호).
- 자원만으로 표현: `numeric_suffix * /km²`, `kl`, `degrees C`, `degrees F` + `unit` label.
  표본에 없는 `/kg` 같은 형태는 선언하지 않았다.
- 계약 9건을 구현 전에 작성했다. 외부 영어: MEASURE 76 → 83, span 35,212 → 35,219.
  Debug 131/131.

### 단독 이니셜 (2026-09-25, 커밋·미배포)

- 로마 숫자 후보를 조사하다 같은 목록에서 이니셜 마침표가 더 큰 덩어리임을 발견했다.
  `U.S. Army` → `u s Army`는 되는데 `Cossette , M. , Lecomte`는 그대로였다. 단독 이니셜은
  **이름이 뒤따를 때만** 인식했기 때문이다.
- 표본의 85건을 뒤따르는 문자로 세었다: `(` 35, `;` 16, `,` 14, `"` 5, `&` 3. 문장을 잇는
  문자 뒤에서만 이니셜로 보고 자기 마침표를 지운다. 새 자원 `initial_continuation`.
- 문장 끝 마침표는 그대로 둔다 — 지우면 문장 경계가 같이 사라진다. 닫는 따옴표 앞도
  기존 판단(`terminal_period`)대로 남긴다(`Smith , b. " title "`).
- 계약 9건을 구현 전에 작성했다. 외부 영어 text: LETTERS 174 → 242, span 35,219 → 35,287.
  Debug 132/132.

### 한국어 불변식 스윕 (2026-09-25, 커밋·미배포)

- 한국어는 외부 참조가 없어 정답 대조 대신 **불변식**을 걸었다: speech 모드 한국어 출력에
  라틴 문자가 남으면 안 된다. 조합 1,080건 중 위반 1건(`제 10kg`)이었고, 따라가니 가족이 셋이었다.
- `제`는 서수 접두사이자 보통 낱말이다. 서수 단위를 만나지 못한 후보가 숫자·단위까지 보존해
  원문을 그대로 내보내고 있었다. 단위를 만난 후보만 보존하도록 고쳤다(`0번째` 등은 그대로).
- 단위 약어 단독이 추측 발음이 됐다(`kg` → 크그). 사전 10건을 관리 파이프라인으로 추가했다
  (`lexicons/ko-KR/unit-additions.jsonl`, 출처 `project-authored-v21-units`, pending).
- 소문자 `ml`이 선언되어 있지 않았다. 기존 `mL` 단위 id로 매핑했다.
- 스윕을 `tests/integration/test_ko_speakable.py` 게이트로 만들고 입력을 1,770건으로 넓히자
  같은 성격의 결함이 더 나왔다: 전화 단서 뒤의 짧은 숫자·범위·수식(`문의 12`, `문의 3-5`,
  `문의 1+1`)과 `제` 접두사가 삼킨 범위·소수(`제 3-5`, `제 0.5`).
- 면제 방식이 중요했다. 처음엔 규칙 이름(`*invalid*`)으로 면제했는데 그러면 방금 고친
  `제 10kg`도 통과한다. **문서화된 보존 입력만** 이름으로 면제하도록 바꿨다.
- 프로즌 계약 `제0`이 경계를 잡아 줬다: 붙여 쓴 `제`는 서수 접두사, 띄어 쓴 `제`는 보통 낱말.
- 계약 16 + 10건, 스윕 위반 0. Debug 135/135.
  기록은 `docs/research/ko-unit-word-v21-results.md`.

### 숫자+기호 잔여물 (2026-09-25, 커밋·미배포)

- 같은 속성 검사를 영어 speech에도 돌리고, 숫자와 기호 조합을 양쪽 언어로 훑었다.
  `20°`, `5m²`, `5㎡`, `5㎢`, `5kcal`, `5㎉`, `5‰`가 기호를 그대로 내보내고 있었다.
- 자원만으로 처리했다(`numeric_suffix` + `unit`). 눈금이 붙은 `°C`/`°F`는 가장 긴 접미사가
  먼저 맞으므로 그대로 이긴다.
- 모호한 것은 손대지 않고 계약의 음성 사례로 남겼다: `′`/`″`(피트·인치 대 분·초),
  `¢`(보조 통화 단위), 위첨자 `²`(각주 표시일 수 있음).
- 계약 16건을 구현 전에 작성했다. Debug 136/136.

### 국가 코드 도메인 (2026-09-26, 커밋·미배포)

- speech 기준선을 다시 재니 ELECTRONIC이 0/22였다. 대부분은 데이터셋 표기법(`b_letter`) 차이라
  맞출 수 없지만, `Vecernji.hr`·`Amazon.co.uk`는 **인식 자체가 안 돼** 점이 그대로 나갔다.
- ISO 3166 코드 전체를 overlay로 씌워 외부 표본을 돌렸더니 수치가 그대로였다(회귀 0). 그런데
  표본이 못 보는 실패가 있다: `See fig.no 3`이 도메인이 된다.
- 그래서 **영어 낱말과 겹치는 코드는 빼고** 200개만 넣었다. 계약 9건(음성 사례 3건 포함)을
  구현 전에 작성했다. Debug 138/138.

### 콜론 앞 숫자와 연표 연도 (2026-09-26, 커밋·미배포)

- 맨 연도 148건 중 안전한 부분집합을 찾다가 콜론 형태(7건)를 골랐고, 구현하다 **더 큰 것**을
  발견했다: `Chapter 12: Introduction`의 숫자가 아예 안 읽히고 있었다. 콜론이 뒤에 숫자가 없어도
  구조로 취급돼 `syntax.unsupported-numeric`으로 보존됐기 때문이다.
- 콜론은 **뒤에 숫자가 있을 때만** 구조다(`3:4`, `09:30` 그대로). 숫자 없는 콜론은 시간 후보도
  만들지 않는다(`1912:`가 invalid-time으로 보존되던 것도 해소).
- 연표 형태는 새 자원 `date_year_cue_after`로 읽는다. 연도 범위·수량 명사 규칙은 그대로다.
- 계약 7건을 추가로 작성했다(음성 사례 4건 포함, 총 31건). 외부 영어 DATE 532 → 538,
  span 35,287 → 35,293. Debug 138/138.

### 인용 형태 연도 (2026-09-26, 커밋·미배포)

- 쉼표와 문장 끝 사이의 4자리 숫자는 표본에서 **전부 연도**였다(25건, 반례 0). 다만 표본이
  인용 중심이라 규칙을 좁게 뒀다: 앞뒤 표시가 **둘 다** 맞아야 한다.
- 괄호 전용이던 코드를 `date_year_enclosure` 자원으로 일반화했다. 여는 표시가 자기 닫는 표시
  집합을 선언하고 `$`는 문장 끝을 뜻한다. 쉼표는 `.;$`를 닫는 표시로 갖는다.
- planner도 같은 자원을 읽어야 배치와 스트리밍이 갈리지 않는다 — 옮기자마자 기존 계약
  `( 2024 )`가 스트리밍에서 어긋나며 이 점을 잡아냈다.
- 계약 7건 추가(음성 사례 3건, 총 38건). DATE 538 → 558, span 35,293 → 35,313. Debug 138/138.

### 하이픈 연도 범위 (2026-09-26, 커밋·미배포)

- `merged-agreement` 진단으로 남은 alignment 35건을 열어 보다 발견했다: `from 1965 to 1970`은
  두 끝점을 연도로 읽는데 `the 1988 - 1993 period`는 기수로 읽고 있었다.
- 단서 규칙은 그대로다(첫 끝점이 단서로 확인된 연도여야 한다). 그래서 단서 없는 쌍과
  `p . 57 - 58`은 기수를 유지하고 ISO 날짜도 그대로다.
- `number_at`이 붙은 하이픈을 malformed 필드로 보기 때문에 첫 끝점만 직접 읽는다.
  읽기는 `unit=year`를 단 RANGE 한 개이고 verbalizer가 끝점별로 읽는다.
- 계약 7건 추가(음성 사례 3건, 총 45건). 정확 수치는 35,313 그대로지만 진단이 개선을 보여 준다:
  merged-agreement DATE 0 → 5, PLAIN 26 → 29, alignment DATE 27 → 22. Debug 138/138.

### 한국어 고유어 셈단위 19개 (2026-09-26, 커밋·미배포)

- 작업 큐 2순위(KO-REAL-INPUT)의 취지대로 실제 한국어 상거래 문구를 읽어 보다 발견했다.
  `최대 5회 → 오회`와 `재고 3개 → 세 개`의 띄어쓰기 차이를 따라가니 원인은 **미등록 셈단위**였다:
  선언이 없으면 한자어 수사로 떨어져 `2쌍`이 `이쌍`이 된다.
- 고유어 수사를 쓰는 19개(쌍 통 판 척 알 조각 묶음 상자 봉지 갑 채 그루 그릇 접시 군데 가지
  줄 칸 끼)를 선언했다. 한자어 수사 셈단위(회 배 박 층 호 인분)는 그대로다.
- 셈단위가 더 긴 낱말의 첫 음절이면 기존과 같은 방식으로 잡힌다(`2장미` → `두 장미`).
  새 유형의 오류는 아니지만 노출이 넓어지므로 `docs/ko-coverage.md`에 적었다.
- 계약 29건을 구현 전에 작성했다(한자어 유지·조사·범위·20/100 경계 포함). Debug 140/140.
- **2차 25개 (같은 날):** 처방·리뷰·공공 문구를 더 읽어 `1정 → 일정`(한 정), `8자 → 팔자`
  (여덟 자)를 찾았다. 정 자 곡 편 컷 방 캡슐 포 첩 술 모금 발 자리 바퀴 걸음 쪽 캔 팩 세트
  다발 근 관 짝 사발 공기를 선언했다. 계약 32건. 선언 수는 26개 → 70개가 됐다.
  접두 충돌은 기존과 같은 양상이고 일부는 더 자연스러워진다(`5정도` → 다섯 정도).

### 한국어 식별 번호 라벨 (2026-09-26, 커밋·미배포)

- 실제 고지·주문 문구를 계속 읽다가 발견했다. `우편번호 06236`이 육천이백삼십육(선행 0 소실),
  `주문번호 20260926-0001`이 범위, `송장번호 123456789012`가 거대 기수로 읽혔다.
- 메커니즘(`ko:labeled-identifier`)은 이미 있었고 **라벨 자원이 금융 14행뿐**이었다. 66행을
  더했다(우편·주문·송장·운송장·회원·사업자·예약·인증·승인·카드·등록·문서·고객·접수, 학번·사번·군번).
- 식별 숫자의 0은 `공`이다(전화·계좌와 같은 정책). 작성한 기대값이 `영`이어서 엔진 쪽으로 고쳤다.
- 프로즌 계약 `접수 번호는 123-456` 충돌: 그 계약은 범위 읽기(백이십삼에서 사백오십육)를 담고
  있었다. 접수번호에는 틀린 읽기라 근거와 함께 갱신했고, 고객 범위 구분자는 `range-customer`
  케이스가 계속 검증한다.
- 계약 12건을 구현 전에 작성했다(라벨 없는 금액·연도·셈단위 음성 사례 포함). Debug 141/141.

### 단위 약어 일괄 훑기 (2026-09-26, 커밋·미배포)

- 셈단위에서 통한 "표를 한 번에 훑는" 방식을 단위에도 적용했다. 77개 중 **54개가 미정규화**였다.
- 모호하지 않은 33개를 선언했다(ms sec min hr kHz GW Wh mAh kV mA PB dpi px pt cal kJ Pa hPa
  kPa atm psi mol lux dB rpm Ω inch yd mile lb oz gal ha).
- **한 글자 약어(A V B t s l)는 일부러 제외했다**: `5A`는 `20F`·`4K`와 같은 모호 식별자
  형태다. bps 계열도 한국어 읽기가 정착되지 않아 제외했다. 둘 다 음성 사례로 고정했다.
- 계약 43건을 구현 전에 작성했다.

### 통화 일괄 훑기 (2026-09-26, 커밋·미배포)

- 기호 23개 중 17개, ISO 코드 30개 중 21개가 미정규화였다. 한국어 문장에서 미선언 코드는
  금액까지 영어로 끌고 갔다(`5 CNY` → five c n y).
- 24개 통화를 코드·기호·영/한 라벨까지 선언했다. `¥`는 JPY(그 기호의 관행), 위안은 `CN¥`.
  `¢`는 통화가 아니라 보조 단위라 제외했다. 새 통화에는 보조 단위를 선언하지 않아 소수는
  주 단위로 읽는다(고객 overlay로 추가 가능).
- 문자가 섞인 기호(`HK$`·`NZ$`)는 단독일 때 영어 문맥을 고르는 기존 동작이라 계약을 한국어
  문장 안에 넣었다(`가격은 오 홍콩 달러입니다`).
- 계약 45건을 구현 전에 작성했다. Debug 145/145.

### 기호 일괄 훑기 (2026-09-27, 커밋·미배포)

- 기호 96개 중 76개가 원문 그대로였는데 **대부분은 의도된 보존**이었다: 구두점은 운율용
  `speech_preserve`, 장식 기호는 `--decorative read`로 이름이 붙는다(`★` → 별).
- 진짜 공백 셋: ① 그리스 문자 24개(장식도 구두점도 아니고 이름이 확립됨) ② `∞ √ § №`
  ③ `≠ ≈ ≡`가 `math_relation`에서 누락(`≤`·`≥`는 동작).
- ③은 네 곳을 함께 고쳐야 했다: `src/semantic/classifier.c`의 연산자 표, ko/en pack의
  관계 목록, 그리고 **양쪽 validator**(새 key는 validator를 같은 커밋에 — 메모리 규칙).
- 계약 39건을 구현 전에 작성했다(장식·구두점 불변 음성 사례 포함). Debug 146/146.

### 긴 선행 0 숫자 (2026-09-27, 커밋·미배포)

- 외부 표본의 TELEPHONE/DIGIT 실패를 열어 보니 `0774858583`이 "칠억칠천사백…"으로 읽히고 있었다.
- 처음엔 "선행 0이면 자리열"로 넓게 바꿨는데 프로즌 계약 7건이 걸렸고, 그중 둘은 프로젝트가
  **의도적으로** 정한 위치였다: `advanced-bare-identifier-ambiguous`(id가 "모호"라고 말한다)와
  golden `00010 → 십`. 그래서 뒤집지 않고 **7자리 이상**으로 좁히고 7건을 되돌렸다.
- 자리 구분이 있는 수(`0,123,456`)는 서식 잔재이므로 제외한다(원문이 순수 숫자 열일 때만).
- 짧은 열에 대한 불일치(외부 표본은 `09`도 자리별)와 `공`/`영` 0 읽기 선택은 remaining-work의
  **NUM-ZERO**에 근거와 닫는 증거까지 적었다.
- 구현 중 라벨 경로가 덮이는 문제도 고쳤다: 선택 로직에 DIGIT_SEQUENCE를 더해
  `계좌는 0012345` → `공공일이삼사오`가 유지된다. 계약 15건. Debug 148/148.

이후 후보: 남은 맨 연도 약 120건(문맥 이해 필요), 로마 숫자(이득 8건 남짓),
한국어 외부 참조 확보(EVAL-03), 코드 단어 사전 검수. ISO 어순 46건과 대소문자 698건은 관행
차이로 남긴다.

## v0.20에서 마무리한 범위

- `speech` 텍스트: www. initialism, 가격대 범위, letter/word+number,
  층수 문맥, 이항 더하기, & 및 이메일·URL 철자 읽기.
- `annotated` 입력: `1{첫}번째`, `1{@native}`, `1{@sino}`,
  `ABC{@alphabet}마트`, `[[1+1]]{원플러스원}`, escape/error/byte cap.
- 미지원 한자·이모지는 **원문 보존 기본**. drop은 명시적 선택.
  literal override/보호 코드/의도적 semantic guard의 별도 계약 유지.
- 웹 옵션·예제·51건 corpus, trace의 보존/제거 이유 및 리소스 출처.
- 모델 독립적인 사례 접수·snapshot·수정 전후 비교 도구.

일반 모델이 한 규칙군을 마칠 수 있는 절차는
[tn-improvement-playbook.ko.md](tn-improvement-playbook.ko.md)에 있다.
기존 무기한 goal은 자동 재개하지 않는다. 다음 요청을 받으면 한 이슈/한 규칙군만
선택하고, 결과와 다음 순서를 남긴 뒤 종료한다.

## 배포 상태 (2026-09-26)

`build-web` library와 저장소 `resources`를 각 커밋마다 다시 빌드하고 `ttf-workbench.service`를
재시작했다. `/api/health`는 `{"ok": true, "version": "0.20.0"}`이고 **CMake release 번호는 아직
0.20.0이다**. Unreleased 항목이 v0.20 이후로 상당히 쌓였으므로 번호 상향을 검토할 시점이지만,
릴리스 번호는 여전히 별도 결정이다.

배포마다 서비스 API에서 그 변경의 대표 출력을 확인했다. 최근 것만 적으면
`the f b i and NASA and R and D at UNESCO`(영어 이니셜 speech),
`피파 / 씨이오`(한국어 약어), `킬로그램 단위 / 제 십 킬로그램 / 문의 십이`(말할 수 있음 스윕),
`이십 도 / 오 제곱미터 / 오 퍼밀`(단위 기호),
`20F 안내`가 정책별로 `20F 안내`(preserve) · `20F 안내`(drop) · `이영에프 안내`(read).

## 진실의 기준

| 항목 | 위치/계약 |
|---|---|
| 현재 release/빌드 | CMakeLists.txt; main commit은 `git log -1 --oneline` 확인 |
| 런타임/API | include/tts_text_frontend; 기존 C engine/CLI 기본 plain/text |
| 웹 | speech/plain/preserve를 명시적으로 선택, port8000 |
| 제어 문법/리소스 | input-format.md, speech-resources.md |
| 신규 참조/계약 | tests/evaluation/user_patterns_v20.jsonl, tests/integration/test_user_patterns.py |
| 검증 기록 | research/speech-controls-v20-results.md |
| 데이터 기반 다음 순서 | ../benchmark/manifests/quality-work-queue.json |
| 구조·운영의 잔여 범위 | remaining-work.md (전체 완료로 표시하지 않음) |
| 실행 하네스 | ../tools/quality/tn_cycle.py --help |

## 검증과 아직 모르는 것

신규 51/51 text 계약, 813 CLI 계약, 2,164 native split/error/ownership 검사 통과.
웹 API/ABI 133 검사 및 browser gate 통과. 기존 외부 plain/text 개발 corpus는
3,785문장/46,213 span에서 v0.19와 결과 전체 동일, stream11,355회 불일치0.
이는 speech 모드의 일반 정확도 인증이 아니다. TN 대상 exact는1,118/3,293이고,
937개 로컬 참조가 아직 독립 검수 대기다. 사전379항목/381키 중294 pending,
85 source-checked, independently human-reviewed0이다.

GCC96개/sanitizer84개 기존 suite에서 각95/83개가 병렬 실행 통과했고,
남은 30k-number timeout gate는 **단독 재실행 통과**했다 (GCC19.92초,
ASan/UBSan48.50초). 시간 제한을 늘리거나 실패를 지우지 않았다.
Release build-web의 신규 기능·harness·web/CLI 선택 gate **7/7**도 통과했다.
새 harness gate는 실제 CLI로 fixed/regressed/trace/snapshot 훼손/참조 덮어쓰기
방지 검사에 통과했다. 릴리스 CI는 GitHub Actions의 해당 main commit에서 확인한다.

## 이 VM의 운영

- 저장소: `/home/exedev/tts-text-frontend`
- 웹: `https://tts-text-frontend.exe.xyz:8000/`
- 사용자 서비스: `ttf-workbench.service`
- 배포 library: `build-web/libtts_text_frontend.so`, resources는 저장소의 `resources/`
- 빌드/데이터는 ignored `build*`, `artifacts/`, `benchmark/generated/`에 둔다.
- GCC와 sanitizer 전체 suite를 이 2-core VM에서 동시에 실행하지 않는다.
- 배포 시 library와 resource revision을 함께 갱신하고 health/대표 출력 확인.
  환경별 HTTPS 프록시는 exe.dev 공식 문서만 사용한다.

```sh
git status --short
git log -1 --oneline
curl --fail --silent http://127.0.0.1:8000/api/health
systemctl --user status ttf-workbench.service --no-pager
```

## 다음 AI에게 줄 요청 템플릿

> AGENTS.md와 docs/HANDOFF.md를 읽고, 아래 TN 사례 하나의 원인을 최신 버전에서
> 재현해라. docs/tn-improvement-playbook.ko.md의 snapshot/cycle을 사용하고
> expected를 엔진에 맞춰 바꾸지 마라. 같은 규칙군의 변형·부정 예를 포함하고,
> resource/pack 수정 후 관련 gate와 동일 참조 비교 결과를 기록해라.
> 이번 규칙군을 마치면 중단하고 남은 항목을 인계해라.
>
> 원문: … / 기대 출력: … / 언어·문맥: …
> output_mode: speech|text / input_format: plain|annotated
> unreadable: preserve|drop / domain·customer: … / 읽기 근거: …

로컬 마무리 로그: `/tmp/ttf-v20-context-isolated.log`,
`/tmp/ttf-v20-asan-context-isolated.log`, `/tmp/ttf-v20-web-gates.log`.
하네스 실제 snapshot/cycle smoke 결과:
`artifacts/tn/v20-handoff/comparison/cycle.json` (candidate_failed0, regressed0).
이 smoke는 같은 capture의 재현성 검사이며 품질 개선 수치가 아니다.
