# 영어 TN: VoxCPM2 내장 wetext와 비교 (2026-10-06)

VoxCPM2(github.com/OpenBMB/VoxCPM)는 영어 문장을 `src/voxcpm/utils/text_normalize.py`에서 정규화합니다. 순서는 `clean_text`(마크다운·이모지·줄바꿈·둥근 따옴표), `wetext.Normalizer(lang="en", operator="tn")`(WeTextProcessing FST 런타임), 남은 숫자를 `inflect`로 풀어 쓰기입니다. `benchmark/evaluators/compare_wetext.py`가 이 순서를 그대로 재현하고, 우리 엔진과 같은 문장을 같은 채점(`english_sentences.level`, variant)과 같은 영어 범위로 채점합니다.

```sh
python3 -m venv ~/venv-wetext && ~/venv-wetext/bin/pip install wetext==0.1.8 inflect regex
~/venv-wetext/bin/python benchmark/evaluators/compare_wetext.py \
  --corpus benchmark/generated/en-eval/nemo-eval-acacc21.jsonl \
  --corpus benchmark/generated/en-eval/sproat-sample-shard58.jsonl \
  --corpus benchmark/generated/en-eval/sproat-heldout-shard58.jsonl \
  --library build/libtts_text_frontend.so --resources resources --out /tmp/wetext-cmp
```

## 별도 평가셋

`sproat-heldout-shard58`(11,135문장)은 개발에 쓴 적이 없는 문장입니다. 기존 `sproat-sample-shard58`과 같은 1MB 범위 요청에서, 그 표본이 쓴 49,998줄 뒤(49,999~199,995줄)만 가져왔습니다. 앞 916,200바이트가 기존 표본과 같은 해시라 출처가 이어지고, 기존 표본과 같은 문장 217개는 뺐습니다. 데이터는 `benchmark/generated/`(저장소 밖)에만 있습니다.

## 결과 (speech, variant 일치율)

| 평가셋 | 문장 | 우리 엔진 | wetext (VoxCPM2) |
|---|---|---|---|
| 전체 | 30,308 | **84.91%** | 68.38% |
| NeMo GoogleTN | 7,521 | **85.85%** | 74.07% |
| NeMo LibriTTS | 7,677 | **80.70%** | 53.69% |
| NeMo EngConf | 231 | 26.84% | **58.44%** |
| Sproat 개발 표본 | 3,779 | **88.07%** | 74.01% |
| **Sproat 별도 평가셋** | 11,100 | **87.32%** | 72.97% |

- 별도 평가셋의 차이(14.4%p)는 개발 표본의 차이(14.1%p)와 비슷하고, 우리 점수도 개발 표본보다 0.75%p 낮을 뿐입니다. 평가셋에 맞춰 고친 효과가 크지 않다는 뜻입니다.
- 숫자 속 `and`(one hundred and twenty eight)를 양쪽에서 접어도 결과는 같습니다(84.76% : 68.28%). wetext의 `and`는 점수 차이의 원인이 아닙니다.
- 참조 일치율이라 참조의 표기 습관이 섞입니다. 날짜(`the second of october` / `october second`)와 대문자 약어(`HLA`를 그대로 둘지 글자로 읽을지)는 둘 다 말할 수 있는 읽기인데 한쪽만 맞습니다.

## 어디서 갈리는가 (입력의 첫 번째 형태 기준)

| 형태 | 문장 | 우리 | wetext | 우리만 맞음 | wetext만 맞음 |
|---|---|---|---|---|---|
| 연도 | 8,328 | 78.1% | 58.6% | 2,361 | 737 |
| 수 | 4,299 | 84.4% | 81.4% | 479 | 350 |
| 대문자 약어 | 1,973 | 69.6% | 26.8% | 963 | 119 |
| 로마 숫자 | 1,868 | 73.9% | 1.1% | 1,368 | 8 |
| 날짜 | 526 | 81.6% | 5.3% | 406 | 5 |
| 금액 | 164 | 89.6% | 54.3% | 62 | 4 |
| 단위 | 104 | 86.5% | 50.0% | 39 | 1 |
| 소수 | 167 | 64.1% | 41.9% | 46 | 9 |

- wetext의 확실한 약점: 로마 숫자를 읽지 않음(`CHAPTER XI`), 금액 어순(`$ 25,000` dollar twenty five thousand), 날짜 표기.
- 우리 엔진의 확실한 약점: **연도**. wetext만 맞은 연도 문장 747개 중 477개는 우리가 연도를 일반 수로 읽은 경우입니다(`Civil Rights Act of 1964` one thousand nine hundred sixty four, `1894.` one thousand eight hundred ninety four). 다음 개선 대상입니다.
- EngConf(231문장, 일부러 어려운 문장)는 wetext가 앞섭니다. 우리 엔진은 32문장을 그대로 두었고(`10/02/2017`처럼 월/일 순서가 모호한 날짜), `3/4 of an hour`를 three over four로 읽습니다.

## 참조 표기 습관을 뺀 비교: Jev (2026-10-07)

두 시스템 출력이 다른 15,241문장 중 600문장을 무작위로 골라, 양쪽 출력을 같은 Jev 질문(`spoken-reading-correct-v3`, 영어)으로 판정했습니다. 참조를 보지 않으므로 날짜 어순이나 약어 표기 같은 참조의 습관이 빠집니다.

| | 우리 | wetext |
|---|---|---|
| Jev 0.7 이상 | 81.5% | 79.2% |
| 평균 | 0.82 | 0.80 |
| 상대보다 0.1 넘게 높음 | 109 | 70 |
| 결과에 숫자가 남음 | 12 | 0 |

- 참조 일치로는 16.5%p 차이였는데, 참조를 빼면 2.3%p로 줄어듭니다. 차이의 대부분은 표기 습관이고, 그래도 우리가 앞섭니다.
- 참조가 우리만 맞다고 본 문장에서는 Jev도 우리를 더 높게 봤고(86.1% : 77.9%), 참조가 wetext만 맞다고 본 문장에서는 Jev도 wetext를 높게 봤습니다(80.0% : 96.7%).
- 한계: Jev는 영어 보정을 하지 않았고 숫자를 자리별로 대조하지 못합니다. 우리 결과에 숫자가 남은 12문장(모호한 날짜 등)은 wetext에 없는 약점입니다.

## 연도 읽기 개선 뒤 (2026-10-07)

영어 문장에서 단서 없이 쓴 4자리 수(1100~2099)를 연도로 읽게 했습니다(`date_policy uncued_min_year 1100`). 참조는 이런 수를 연도로 6,537번, 일반 수로 88번 읽었고, 일반 수는 뒤에 단위나 세는 말이 올 때였습니다. 그래서 다음은 일반 수로 둡니다.

- 뒤에 단위·세는 말: `1146 miles`, `1500 years`, `1996 degrees`, `1900 hrs`, `1500 soldiers` (`date_year_quantity`)
- 앞에 번호 이름: `page 1965`, `chapter 1965`, `room 1204` (`date_year_label_before`)
- 앞에 수량 말(나열이면 나열 앞의 말): `rose to 1965`, `paid 1865`, `The count is 2026`, `scores were 1200, 1400 and 1600` (`date_year_quantity_before`)
- 영어 문장이 아닐 때: 숫자 앞 64바이트에 한글·한자·가나가 있거나, 같은 줄 앞이나 바로 뒤에 영어 낱말이 없을 때(`2026` 단독, 줄에 숫자만 있을 때, 한국어 문장). 스트리밍과 배치가 같은 결과를 내도록 기존 연도 단서 규칙과 같은 범위만 봅니다.

| 평가셋 | 우리 (전) | 우리 (후) | wetext |
|---|---|---|---|
| 전체 30,308 | 84.91% | **86.84%** | 68.38% |
| NeMo GoogleTN | 85.85% | **87.33%** | 74.07% |
| NeMo LibriTTS | 80.70% | **83.26%** | 53.69% |
| Sproat 개발 표본 | 88.07% | **90.05%** | 74.01% |
| Sproat 별도 평가셋 | 87.32% | **89.13%** | 72.97% |

wetext만 맞은 문장은 1,365개에서 904개로 줄었습니다. 영어 벤치마크(speech)는 +400, -0(83.52% → 85.51%), text는 +374, -0이고 한국어는 변화가 없습니다. 남은 한계: 문장 끝 마침표 뒤의 `1894.`는 목록 번호 규칙이 먼저 잡아 일반 수로 읽습니다.

## 숫자가 남은 출력 줄이기 (2026-10-07)

평가셋 30,308문장 중 우리 출력에 숫자가 남은 문장이 346개였습니다(wetext 0). 원인별로 고쳐 144개로 줄였습니다. 계약 `en_digits_left_v25.jsonl` 19건은 구현 전에 작성했습니다.

| 원인 | 예 | 지금 |
|---|---|---|
| TeX 따옴표를 코드로 봄 (225문장) | `` `` Nepal Census 2001 `` `` | `` `` Nepal Census two thousand one `` `` |
| 숫자 뒤 이중 하이픈 | `CHAPTER 16 --` | CHAPTER sixteen |
| 범위 뒤 대시 | `In 1957 - 1974 - Bryantsev` | nineteen fifty seven to nineteen seventy four Bryantsev |
| 구분자 뒤 이중 하이픈 | `Scene 2:--` | Scene two: |
| 세 자리 연도 날짜 | `July 18, 453` | July eighteenth four fifty three |
| 분수 | `1/4`, `3/4`, `2/3`, `3/8` | one quarter, three quarters, two thirds, three eighths |
| 대분수 | `2 1/2`, `1 -1/2` | two and one half, one and one half |

- TeX 따옴표: 백틱 두 개 뒤에 공백과 두 글자 이상 낱말과 공백, 또는 공백 없이 대문자 낱말이 오면 따옴표로 봅니다. 여는 부분만 보고 정하므로 스트리밍(1바이트 조각까지)과 배치가 같고, 긴 인용도 코드로 잘리지 않습니다. 같은 줄 앞에 짝이 안 맞는 ``가 있으면 닫는 따옴표입니다. 진짜 코드(`` `` x = 2; `` ``, `` `ls -la 2` ``)는 그대로 보존합니다.
- 분수: 참조는 진분수를 분모 이름으로 51번, `over`로 0번 읽었습니다. 분모 16까지만 이름으로 읽고(half, quarter는 자원 `fraction_denominator`, 나머지는 서수+s), 가분수(`24/7`)와 음수(`-3/4`)는 `over` 그대로입니다. `over`를 고정한 이전 계약 9곳은 notes에 이전 값을 남기고 바꿨습니다.
- 대분수는 영어 문장에서만(앞 64바이트·뒤 16바이트에 한글·한자·가나가 없고, 기본 언어가 영어이거나 앞에 영어 낱말이 있을 때) 읽습니다. 한국어 `2 1/2개`는 그대로입니다.
- 결과: 영어 벤치마크 speech 85.51% → 86.52% (+632, -39), text 82.42% → 83.16% (+525, -9). 한국어 변화 없음. wetext 비교 86.84% → 87.48% (wetext 68.38%). 새로 틀린 39건은 대부분 따옴표 안 제목의 `VI`, `ABS`, `/`를 참조는 원문 그대로 두고 우리는 v i, a b s, slash로 읽는 표기 차이입니다.
- 남은 144문장: ISBN·DOI·URL 속 숫자, `2:16 -17` 같은 장절, `10/02/2017` 같은 월/일 순서가 모호한 날짜 등입니다.

## wetext만 읽는 것과 그대로 남는 숫자·기호 (2026-10-07)

질문: wetext가 읽는데 우리가 못 읽는 것이 있는가, 숫자나 기호가 그대로 남는가. 30,308문장(중복 제외 기준)을 다시 셌습니다.

| | 전 | 후 |
|---|---|---|
| wetext만 맞은 문장 | 771 | 693 |
| 우리 출력에 참조에 없는 숫자가 남은 문장 | 145 | 75 |
| `/`가 남은 문장 | 30 | 10 |
| 전체 일치율 (wetext 68.38%) | 87.48% | 87.96% |

wetext(VoxCPM2)는 마지막에 남은 숫자를 모두 inflect로 읽어 버리므로 숫자가 남는 일이 없습니다. 우리는 코드·식별자를 보존하는 정책이라 0이 되지는 않습니다.

고친 원인 (계약 `en_wetext_gaps_v26.jsonl` 35건, 구현 전에 대부분 작성):

- **세그먼트 경계**: 첫 세그먼트(48자)가 숫자 바로 앞에서 잘리면 숫자가 앞 낱말을 못 봐서 `from 1918 to 1923`의 1923, `the # 7 seed`를 일반 수로 읽었습니다. 낱말 뒤 공백에서 다음 토큰이 숫자이면 자르지 않고(다음 토큰이 아직 없으면 입력을 더 기다려 스트림과 배치가 같음), 숫자가 붙은 낱말(`112에`, `2-1로`) 바로 뒤에서도 자르지 않습니다. 줄바꿈과 코드 뒤에서는 예전처럼 바로 자릅니다. 아주 짧은 세그먼트 설정(1/3/12)에서 `is 2026.`처럼 앞 낱말이 계사 하나뿐이면 결정하는 낱말이 앞 세그먼트에 있으므로 일반 수로 둡니다. TeX 따옴표(``…``) 안에서도 자르지 않습니다(닫는 ``가 다음 세그먼트에서 코드로 열리던 문제).
- `# 7` number seven(참조 48 : 1), `No. 5` number five(숫자가 올 때만), `Rev.` Reverend, `Hon.` Honorable.
- `km2`, `m2`, `/km2`, `mi²`, `/mi²` 제곱 단위(참조 19 : 19), `lbs` pounds(29 : 29).
- 2000년 전 ISO 날짜의 연도를 달력식으로(`1997-04-10` April tenth nineteen ninety seven). 2000년부터는 기존 계약대로 two thousand twenty six입니다(참조는 `2011-08-18`을 twenty eleven으로 읽어 이 부분은 표기 차이로 남김).
- `6/25/1940` June twenty fifth nineteen forty: 영어 문장의 월/일/년은 월이 먼저(참조와 같음), 첫 칸이 12보다 크면 일(`25/12/1999` the twenty fifth of December), 두 자리 연도는 쓴 대로(`13` thirteen, `00` oh oh).
- 대문자 서수 `24TH`, `22ND`; `THURSDAY`를 철자로 읽지 않음(낱말 안 coda 뒤 s).
- `21 January 19 44` 세기와 나머지로 나뉜 연도, `ISBN` 뒤 숫자를 한 자리씩(끝의 X 포함; 이전 phone 계약의 ISBN 보존 행은 notes에 이전 값을 남기고 바꿈).
- `INGREDIENTS.--To`에서 대시를 지울 때 낱말이 붙지 않게 띄어 씀.

결과: 영어 벤치마크 speech 86.52% → 87.11%, text 83.16% → 83.54%. 한국어 +28, -0(세그먼트 경계 덕분에 차량 번호 `57후 4597`, 점수 `2-0` 등). 새로 틀린 영어 12건은 LibriTTS 참조가 `No. 13`을 No. thirteen으로 둔 표기 차이입니다(EngConf 참조는 number).

남은 75문장: URL·DOI 속 숫자, `6:14-16` 장절, `1.1.3` 버전 표기 일부, `75F`, `12pm-2pm`, `99.9FM`, `787s` 같은 붙은 단위·모델명, `23d` 같은 옛 서수.

## 남은 숫자를 끝까지 읽기 (2026-10-07)

프로젝트 소유자 결정: 영어 speech도 한국어 written fallback(2026-10-02 결정)처럼, 분류기가 거절한 숫자 덩어리를 쓴 대로 읽습니다. wetext/VoxCPM2가 마지막에 남은 숫자를 모두 읽는 것과 같은 목적입니다. 코드(백틱, `<code>`)와 경로처럼 보호된 부분은 한국어와 같이 보존합니다. URL·이메일 속 숫자는 원래부터 읽고 있었습니다. text 출력은 쓴 대로 둡니다.

| 쓴 형태 | 읽기 |
|---|---|
| `97X`, `6a`, `32mo` | ninety seven X, six a, thirty two mo (글자는 그대로) |
| `3771A` | three seven seven one A (네 자리 이상·0으로 시작하면 한 자리씩) |
| `787s`, `690s`, `18s` | seven eighty sevens, six nineties, eighteens (세 자리 복수는 둘로 나눠 읽음, 참조와 같음) |
| `12pm-2pm`, `1861--1865` | twelve p m to two p m, eighteen sixty one to eighteen sixty five (숫자 사이 대시 하나는 범위) |
| `99.9FM`, `1/7th` | ninety nine point nine FM, one seventh |
| `23d` | twenty three d (참조와 같음) |

같이 고친 것: `Micah 6:6-8` chapter six verses six to eight(책 이름 `scripture_book` 뒤의 장:절-절만; `John 3:16` 단일 절과 `9:00-17:00` 시각은 그대로), `2007 Apr 24` two thousand seven April twenty fourth(연-월-일 순서 그대로), 띄어 쓴 ISBN(`0 - 486 - 43365- X`), 마침표가 붙은 4자리 숫자만 있는 줄(`1716.` seventeen sixteen; 참조 33줄 모두 연도, 소유자가 권장 방향으로 결정을 맡김. `2026`처럼 마침표가 없으면 일반 수 그대로). 계약 `en_fallback_v27.jsonl` 23건; 원문 보존을 고정했던 계약 7건(`1255.`, `at 13pm`, `1861--1865`, `3XL` 2건, `75t`, `20F`·`11st`)은 notes에 이전 값을 남겼습니다.

| | 전 | 후 |
|---|---|---|
| 숫자가 남은 문장 | 75 | 7 |
| wetext만 맞은 문장 | 693 | 615 |
| 전체 일치율 (wetext 68.38%) | 87.96% | 88.27% |
| 영어 벤치마크 speech | 87.11% | 87.58% (+91, -0) |

한국어는 변화가 없습니다. 남은 7문장은 코드로 잘못 잡힌 TeX 따옴표 안(`` `` 91 `` ``)과 윗첨자 `²`입니다.

## 세그먼트 앞 문맥 (2026-10-07)

문장이나 구 경계에서 세그먼트가 잘리면, 다음 세그먼트 첫머리의 숫자는 앞 낱말을 보지 못해 연도를 일반 수로 읽었습니다(`Douglas Haig born. 1880.`, `( b . 1952 )`, `E. G . 1998 .`). 이제 스트림은 앞 세그먼트 원문의 마지막 64바이트를 들고 있다가, 다음 세그먼트를 분류할 때만 앞에 붙여 보여 줍니다. 분류기들이 원래 숫자 앞 64바이트 창을 읽도록 만들어져 있으므로 같은 창이 세그먼트를 넘어 이어질 뿐입니다. 언어 판정(토큰 기준), 출력, 스팬 위치는 바뀌지 않고, 배치도 같은 스트림 경로를 쓰므로 스트림과 배치 결과가 같습니다.

- 같이 지킨 것: `May. 2026`처럼 문장을 끝낸 월 이름 바로 뒤의 수는 그 수의 단서가 아니므로 일반 수(v13 가드).
- 계약 `en_segment_context_v28.jsonl` 7건.
- 결과: 영어 speech 87.58% → 87.67% (+16, -0), text 83.92% → 84.01%, 한국어 변화 없음, wetext 대비 88.34% : 68.38%.

## 영어 Jev 재평가와 낮은 점수 문장 처리 (2026-10-07)

아침의 600문장(두 시스템 출력이 다른 문장 무작위)을 다시 판정했습니다. Jev가 항상 맞지는 않으므로 wetext가 0.1 넘게 앞선 문장을 직접 보고 실제 오류만 고쳤습니다.

| | 아침 | 지금 | wetext |
|---|---|---|---|
| Jev 0.7 이상 | 81.5% | **84.5%** | 79.2% |
| 평균 | 0.816 | 0.831 | 0.798 |
| wetext가 0.1 넘게 앞섬 | 70 | 46 | |
| 숫자가 남은 결과 | 12 | 0 | 0 |

고친 것 (계약 `en_jev_fixes_v29.jsonl` 12건): `the 9th October`의 관사 중복(the the ninth), `Route NO 128` number, `STORY XXIII` twenty three(`roman_cue story`), 띄어 쓴 ISBN 묶음(`ISBN 968 5005 59 1`), 영어에서 발음 가능한 호스트 이름은 낱말로(`Scrum.com` Scrum dot com, `reasonradionetwork.com`; 두 글자·네 자음 연속·짧은 소문자 계정·대문자는 그대로 철자. `spiegel.de` 계약은 notes에 이전 값을 남기고 바꿈).

그대로 둔 것(판단): 대문자 약어를 철자로 읽는 정책(`DNA` d n a; TTS에서 같은 소리, Jev는 쓴 형태를 선호), 마침표가 붙은 단독 4자리 줄의 연도 읽기(`1231.` twelve thirty one; Jev는 일반 수를 더 높게 봤지만 책의 연표 머리글이 대부분이고 참조 33줄 모두 연도).

영어 벤치마크 speech 87.67% → 88.03% (+69, -0), 한국어 변화 없음.

