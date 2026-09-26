# 영어 DATE 불일치 진단과 괄호 연도 단서 (2026-09-25)

## 왜 지표부터 고쳤나

외부 영어 표본(3,785문장)에서 DATE는 1,463 span 중 394개만 정확했다. 그런데 이 숫자는
"무엇을 고쳐야 하는지"를 말해 주지 않았다. `remaining-work`의 PRON/측정 항목이 오래 지적한
대로, 불일치의 절반 이상이 **대소문자만 다른 건**이었기 때문이다.

그래서 평가기에 진단 두 가지를 넣었다. 정확도 자체는 그대로 byte 비교이고 아무것도 접지
않는다(`benchmark/evaluators/evaluate.py`의 `metric_policy` 문구도 그렇게 적었다).

- `mismatch_kinds`: 실패한 span마다 이유를 이름 붙인다 —
  `case-only`, `space-only`, `case-and-space`, `alignment`, `no-output`, `different`.
- `diagnostic_case_insensitive`: 클래스별로 대소문자를 접었을 때의 정확 수를 따로 센다.

## 진단이 보여 준 것

| 클래스 | 정확 | 대소문자 무시 | 종류별 |
|---|---:|---:|---|
| DATE | 394 / 1,463 | 1,092 | case-only 698, different 344, alignment 27 |
| LETTERS | 174 / 712 | 174 | different 538 |
| PUNCT | 0 / 8,919 | 0 | different 8,909, alignment 10 |

DATE의 698건은 데이터셋이 발화 텍스트를 전부 소문자로 적는 관행이다(`January` vs
`january`). 프로젝트는 입력의 대소문자를 보존하는 쪽을 유지한다 — TTS 뒤쪽에서 고유명사나
강조에 쓸 수 있는 정보를 지표 점수를 위해 버리지 않는다. 진단이 이 관행을 따로 세어 주므로
이제 실제 오류만 보고 일할 수 있다.

## 실제 오류 344건의 갈래

| 갈래 | 건수 | 예 |
|---|---:|---|
| 맨 네 자리 연도 | 258 | `1965` → 기대 `nineteen sixty five`, 출력 `one thousand nine hundred sixty five` |
| ISO 날짜 어순 | 46 | `2009-09-17` → 기대 `the seventeenth of september ...` |
| 연대 표기 | 33 | `1960s` → 기대 `nineteen sixties`, 출력 `1960s`(미정규화) |
| `oh` vs `o` | 3 | `1908` → 기대 `nineteen o eight` |
| 슬래시·점 날짜 | 4 | `6/17/13`, `8.21.2014` |

맨 연도 258건의 앞 문맥을 세어 보니 121건이 괄호 안이었고, 그중 **108건이 괄호 안에 숫자만**
있는 인용 형태(`( 1965 )`)였다.

## 괄호 연도 단서

`date_year_cue`가 단어뿐 아니라 여는 괄호 `(`, `[`, `{`도 key로 받는다. 다만 팩은
**괄호가 숫자만 감싸고 바로 닫힐 때만** 단서로 본다. 그래서 `( 2003 and 2005 )`와
`(1 2 3 4 5 6 7)`은 그대로 cardinal이고, 수량 명사(`( 1965 copies )`)와 연도 범위
(`( 2100 )`)도 기존 규칙이 그대로 막는다. `born`도 단서에 추가했다.

계약 10건을 구현 전에 작성했다(`tests/evaluation/year_cue_v21.jsonl`, 14 → 24건).

### 뒤집은 기존 계약 2건

`tests/integration/test_phone_contracts.py`의 "ordinary parenthesis" 목록에 있던
`( 2024 )`와 `1385 ( 2006 )`은 CARDINAL을 기대하고 있었다. 이 두 줄은 이제 DATE를 기대한다.
두 계약의 원래 목적(괄호 숫자는 전화번호가 아니다)은 그대로다. 같은 목록의
`( 2003 and 2005 )`는 손대지 않았고 여전히 cardinal이다. 이것은 **정책 변경**이며, 근거는
외부 표본의 108건과 인용 관행이다. 표 안의 수량을 괄호로 적는 문서에서는 이 읽기가 틀릴 수
있다 — 그 반대 근거가 나오면 되돌릴 수 있도록 여기에 남긴다.

## 결과

| 지표 | 이전 | 이후 |
|---|---:|---:|
| span 정확 | 35,053 | 35,163 |
| DATE 정확 | 394 | 504 |
| DATE `different` | 344 | 234 |

CARDINAL·PLAIN 등 다른 클래스의 수치는 바뀌지 않았다. Debug CTest 126/126, chunk 불일치 0.

## 연대 표기 (2026-09-25)

`1960s`는 아예 정규화되지 않아 숫자가 그대로 TTS에 흘러갔다. 표본에 실제로 나오는 표기는
`1960s`, `80s`, `'90s`, `1900s`, `1910s`이고 읽기는 복수형 연도다.

- 분류: `en.date.decade`. 숫자가 0으로 끝나는 네 자리 또는 두 자리 뒤에 `s`가 붙고, 뒤에
  글자·숫자가 이어지지 않을 때만 잡는다. 그래서 `1965s`, `787s`, `MP3s`는 그대로 둔다.
  앞의 작은따옴표(`'90s`)는 span에 포함한다.
- 읽기: 복수형 낱말은 자원이다(`decade_plural`: 10 tens … 90 nineties, 100 hundreds,
  1000 thousands). 세 자리 이상은 세기 + 복수형(`nineteen sixties`), 00으로 끝나면
  `nineteen hundreds` / `two thousands`, 두 자리는 복수형만(`eighties`).
- `classifier_priority en.date.decade 10`: 같은 byte 범위를 "꼬리 붙은 숫자"로 보존하던
  범용 분류보다 구체적인 규칙이 이긴다. 기존 층·서수 규칙이 쓰던 방식과 같다.

계약 13건을 구현 전에 작성했다(`tests/evaluation/decade_v21.jsonl`).

## 결과 (누적)

| 지표 | 진단 전 | 괄호 단서 | 연대 표기 |
|---|---:|---:|---:|
| span 정확 | 35,053 | 35,163 | 35,191 |
| DATE 정확 | 394 | 504 | 532 |
| DATE `different` | 344 | 234 | 206 |

CARDINAL·PLAIN·LETTERS·VERBATIM은 세 단계 모두 수치가 같다. Debug CTest 127/127.

## 합쳐진 span을 alignment 실패와 구분 (2026-09-26)

작업 큐의 1순위(FIN-VALUES)가 "값 오류와 alignment를 먼저 갈라 보라"고 지목한 자리다.
CARDINAL의 alignment 실패 79건을 열어 보니 이런 모양이었다.

```
text  : The Expos win , 6 - 5 , on Hunt's ...
참조  : '6' → 'six' | '-' → 'to' | '5' → 'five'   (세 span)
엔진  : '6 - 5' → 'six to five'                    (한 span)
```

읽기는 **똑같고** span 경계만 다르다. 그래서 평가기에 `merged-agreement` 진단을 넣었다.
정렬에 실패한 참조 span들이 하나의 엔진 span 안에 들어 있고, 그 엔진 span의 텍스트가 참조
읽기들을 이어 붙인 것과 같으면 그렇게 이름 붙인다. 정확도는 그대로다 — 경계가 다르므로
참조 span은 여전히 실패로 센다.

| 클래스 | alignment (이전) | alignment | merged-agreement |
|---|---:|---:|---:|
| CARDINAL | 79 | 35 | 44 |
| PLAIN | 40 | 14 | 26 |
| MEASURE | 3 | 0 | 3 |

남은 35건은 진짜 확인 대상이다: `1988 - 93`처럼 연도 스타일이 다른 것, `p . 57 - 58`처럼
구두점이 끼어든 것, 그리고 데이터셋 쪽 토큰화(`19 67,`처럼 연도가 쪼개진 것)가 섞여 있다.

## 다음

남은 DATE `different` 206건 중 괄호 밖 맨 연도가 150건으로 가장 크고, 그다음이 ISO 날짜
어순 46건이다. 어순과 `oh`/`o` 3건은 데이터셋 관행이라 점수를 위해 따라가지 않는다.
대소문자 698건도 마찬가지이며 진단 지표로 따로 센다.
