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

