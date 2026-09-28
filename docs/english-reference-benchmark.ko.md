# 영어 참조 문장 benchmark

영어 TN을 **순수 영어·숫자·기호 문장**으로 평가한다. 한국어·한자·가나·키릴 문자 등이 섞인
문장은 영어 TN의 채점 대상이 아니다(그 읽기는 문장 주 언어의 결정이다). 이런 문장은
분모에서 빼고 `out-of-scope.jsonl`에 따로 남긴다. 결과는 제공 참조와의 일치율이며
사람이 검수한 정답률이 아니다.

## 자료

| dataset id | 원본 | 문장 | 성격 |
|---|---|---:|---|
| `nemo-googletn` | NeMo `data/GoogleTN.json` | 7,551 | Sproat/Google TN test에서 파생. 구두점 토큰(`` `` `` `.`) 유지 |
| `nemo-libritts` | NeMo `data/LibriTTS.json` | 7,677 | 오디오북 원문과 읽기가 다른 문장만 모음. 대소문자 유지(`CHAPTER nine.`) |
| `nemo-engconf` | NeMo `data/EngConf.txt` | 231 | 문맥에 따라 읽기가 갈리는 수작업 자료. 입력마다 허용 읽기(`~~1`) 여러 개와 오답(`~~0`) |
| `sproat` | Sproat 학습 shard 58 앞부분 | 3,785 | 기존 표본. 구두점은 `sil`로 지움 |

NeMo 자료는 revision `acacc21b1cb7916b10558855bc4f85957a0b2fde`, CC BY 4.0
(`data/README.md`). 권리 기록은 [THIRD_PARTY](THIRD_PARTY.md)와
`benchmark/manifests/sources.json`의 `nemo-evaluation-data`에 있다. 가져온 자료와 보고서는
`benchmark/generated/en-eval/`(Git ignored)에만 둔다.

## 영어 범위 (`benchmark/english_scope.py`, `english-scope/1`)

라틴 문자가 아닌 문자(Unicode letter)가 하나라도 있으면 범위 밖이다.

- 범위 안: ASCII, 라틴 확장·발음 구별 부호(café, Nguyễn), 기호(`$ % & ℓ ℃`),
  수식 기호로 쓰인 **한 글자** 그리스 문자(`Δ ( x )`, `5 μm`).
- 범위 밖: 한글, 한자, 가나, 키릴, 히브리, 아랍, 두 글자 이상 이어진 그리스 문자 등.

현재 자료에서 범위 밖 문장은 36개(GoogleTN 30, Sproat 6)다.
한자 16, 키릴 15, 가나 8, 한글 2, 히브리 1, 아랍 1(한 문장에 여러 문자가 있을 수 있다).

## 일치 단계

뒤 단계는 앞 단계를 포함한다. 허용 읽기가 여러 개면(EngConf) 어느 하나와 맞아도 된다.

| 단계 | 비교 방법 | 왜 필요한가 |
|---|---|---|
| `exact` | 바이트 동일 | 가장 엄격 |
| `compact` | 공백 제거 후 동일 | 띄어쓰기만 다름 |
| `loose` | 소문자화, 구두점(와 `` ` ``)을 공백으로, 공백 정리 | 자료마다 구두점·대소문자 정책이 다름. 숫자 읽기는 접지 않음 |
| `variant` | `loose` + 영국/미국 철자를 양쪽 모두 같은 형태로 | GoogleTN·Sproat 참조는 centre→center, organisation→organization으로 **철자까지 바꾼다**. TN이 할 일이 아니므로 어느 철자도 감점하지 않는다 |

`rejected`는 출력이 자료가 오답으로 표시한 읽기(EngConf `~~0`)와 `loose`로 같은 경우다.
기본 출력 모드는 제품 모드인 `speech`다. `text` 보고서와는 `comparison_key`가 달라 서로
비교하지 않는다.

## 실행

```sh
E=benchmark/generated/en-eval
python3 benchmark/importers/nemo_eval/import_nemo_eval.py \
  --data-dir /path/NeMo-text-processing/data \
  --revision acacc21b1cb7916b10558855bc4f85957a0b2fde --out $E/nemo-eval-acacc21.jsonl
python3 benchmark/evaluators/english_sentences.py \
  --corpus $E/nemo-eval-acacc21.jsonl --corpus $E/sproat-sample-shard58.jsonl \
  --library ENGINE/libtts_text_frontend.so --resources ENGINE/resources \
  --out $E/reports/NAME
```

보고서는 `summary.json`(전체·자료별 지표, 범위 밖 문자 통계, 입력·엔진·평가기 hash),
`results.jsonl`(채점한 문장별 출력과 단계), `out-of-scope.jsonl`로 이루어진다. 기존
output 디렉터리는 덮어쓰지 않고, 평가 중 입력·리소스·평가 코드가 바뀌면 보고서를 쓰지
않는다. 다른 엔진(legacy TN 등)은 `run(..., backend=...)`에 `.run(text, lang)`이
`{text, spans}`를 돌려주는 adapter를 넘겨 같은 기준으로 잰다.

## 첫 측정 (2026-09-28)

엔진: `ko-release/engine-claude-identifier`(main `1d22c2f`와 같은 엔진·리소스).
채점 19,208문장, 엔진 오류 1.

| 자료 | 채점 | speech `loose` | speech `variant` | text `loose` | text `variant` |
|---|---:|---:|---:|---:|---:|
| 전체 | 19,208 | 68.7% | 69.5% | 69.7% | 70.5% |
| GoogleTN | 7,521 | 83.3% | 84.9% | 79.3% | 80.8% |
| LibriTTS | 7,677 | 47.4% | 47.4% | 55.1% | 55.1% |
| EngConf | 231 | 23.4% | 23.4% | 44.6% | 44.6% |
| Sproat 표본 | 3,779 | 85.7% | 86.6% | 81.8% | 82.6% |

`exact`는 GoogleTN 62.8%, Sproat 0.2%다. Sproat 참조는 구두점을 지우므로 문장 `exact`는
의미가 없고 `loose` 이상을 본다.

### speech 모드에서 드러난 엔진 문제

speech 불일치 5,855문장 중 반복 유형(한 문장이 여러 유형에 걸릴 수 있다).

| 유형 | 문장 | 예 |
|---|---:|---|
| 로마 숫자 장 번호 | 1,344 | `CHAPTER IX.` → `CHAPTER i x.` (참조 `CHAPTER nine.`) |
| 단어 안 하이픈을 "hyphen"으로 읽음 | 1,190 | `well-known` → `well hyphen known`, `twenty-first` → `twenty hyphen first` |
| 월·일 뒤 연도를 기수로 읽음 | 509 | `April, 1865` → `one thousand eight hundred sixty five` |
| 대문자 단어를 한 글자씩 읽음 | 376 | `FIRST IMPRESSIONS` → `FIRST i m p r e s s i o n s` |
| St./Dr. 등 약어 미전개 | 292 | `St. Louis`, `Circle Dr.` |

하이픈 문제는 speech 모드에서만 생긴다(text 모드는 `well-known` 유지). LibriTTS·EngConf에서
speech가 text보다 낮은 주원인이다. 또 `00s`(2000년대)를 포함한 문장은 두 모드 모두
`invalid resource` 오류로 **문장 전체가 실패**한다(`the 10s`, `the 90s`는 정상).

## 야간 개선 후 (2026-09-29)

엔진 `ko-release/engine-night-20260928` (브랜치 `claude/night-20260928`, `21d2927`).
보고서 `en-eval/reports/20260928-night`, `20260928-night-textmode`. 엔진 오류 1 → 0.

| 자료 | speech `variant` 전 → 후 | text `variant` 전 → 후 |
|---|---|---|
| 전체 (19,208) | 69.51% → 83.43% | 70.47% → 80.47% |
| GoogleTN | 84.91 → 85.83 | 80.80 → 81.05 |
| LibriTTS | 47.41 → 80.54 | 55.13 → 79.80 |
| EngConf | 23.38 → 26.41 | 44.59 → 45.89 |
| Sproat 표본 | 86.58 → 88.01 | 82.64 → 82.80 |

위 표의 문제 중 하이픈, 로마 숫자 장 번호·왕 이름, 월 뒤 연도, 대문자 일반 단어, `00s`
오류, `St./Mt./Ft.` + 이름을 고쳤다. 남은 것: 이름 없는 도로 약어(`Main St.`; 스트림
세그먼트 경계 문제로 보류), `1/4`·`5pm` 같은 문맥 의존 형식, EngConf의 문맥 모호 사례.

## 한계

- 참조는 자료 제작 시스템(Kestrel 등)이나 작성자의 판단이다. 정답의 전부가 아니다.
- `variant` 철자 규칙(-our/-or, -re/-er, -ise/-ize, -yse/-yze, -mme/-m, -ogue/-og,
  -ence/-ense, -ll-/-l-, ae/oe)은 양쪽에 똑같이 적용하므로 출력과 참조가 철자만 다를 때만
  결과가 바뀐다. 목록에 없는 변형(grey/gray 등)은 여전히 불일치로 센다.
- Sproat 표본은 학습 shard의 앞부분이며 무작위·held-out 표본이 아니다.
- 문장 단위 지표다. Sproat의 semiotic class별 span 지표는 기존
  `benchmark/evaluators/evaluate.py`로 따로 본다.
