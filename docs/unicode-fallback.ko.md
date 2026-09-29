# Unicode 발화 fallback (v0.24)

어떤 규칙도 읽지 못한 기호·이모지를 TTS에 원문 그대로 넘기지 않고, 그 언어의 이름으로 읽는다.
언어마다 **인덱스 파일과 데이터 파일을 한 쌍**으로 둔다. 코드포인트로 인덱스 위치를 바로 찾고,
거기 적힌 offset으로 데이터 파일의 이름을 읽는다. 여러 코드포인트로 된 이모지는 정렬된
시퀀스 파일에서 찾는다. 파일은 메모리에 올리지 않고 읽기 전용으로 매핑한다.

## 1. 파일 (`resources/unicode-fallback/`)

| 파일 | 내용 |
|---|---|
| `<tag>.idx` | 헤더 16바이트(`TTFUFIX1`, build id, 범위) + U+0000..U+1FFFF 각 코드포인트의 u32 offset(LE). 0은 이름 없음 |
| `<tag>.dat` | 헤더 16바이트(`TTFUFDT1`, build id, pool 크기) + NUL 종료 UTF-8 이름. 같은 이름은 한 번만 저장 |
| `<tag>.seq` | 헤더 16바이트(`TTFUFSQ1`, build id, 개수) + 64바이트 레코드(60바이트 UTF-8 키, 0으로 채움 + u32 offset). 키 바이트순 정렬, 이진 탐색 |
| `<tag>.tsv` | 같은 내용을 사람이 검토하는 표: 종류, 키, 문자, 이름, 출처 |
| `manifest.json`, `coverage.tsv` | 원본 hash, 변환 규칙, 언어별 개수와 build id |
| `LICENSE-UNICODE.txt` | Unicode License v3 (UCD·CLDR 데이터) |

- 인덱스 범위를 plane 0~1로 둔 이유: 기호와 이모지 기본 문자가 모두 이 범위에 있다. 언어당 512KB다.
- 세 파일의 build id는 같아야 한다.
- 엔진은 파일을 열 때 헤더, 크기, pool 끝의 NUL을 한 번만 검사한다. 조회할 때는 offset 범위만 확인한다.
- 파일이 없으면 fallback 없이 동작한다.
- 파일이 있는데 손상됐거나 짝이 맞지 않으면 엔진 준비(freeze)가 `TTF_ERESOURCE`로 실패한다. 손상된 파일을 추측해서 읽지 않는다.
- `ko-KR` 팩은 `ko-KR.*` 파일이 없으면 `ko.*`를 쓴다.

저장소에는 엔진이 쓰는 `ko`, `en`만 넣는다. 전체 언어(CLDR annotations가 있는 170개)는
`benchmark/generated/unicode-fallback/cldr-48-2/`에 빌드해 둔다(git 무시, 189MB). 언어 팩을 추가할 때
그 언어의 세 파일을 옮기면 된다.

## 2. 빌드 (`tools/unicode_fallback/build.py`)

입력은 고정된 릴리스다. Unicode 17.0.0 UCD(`UnicodeData.txt`)와 emoji 파일(`emoji-sequences.txt`,
`emoji-zwj-sequences.txt`, `emoji-data.txt`), 그리고 CLDR `release-48-2`(`annotations`,
`annotationsDerived`, `rbnf`, `supplementalData.xml`)를 쓴다. 같은 입력이면 모든 파일이 바이트 단위로
같다(재빌드 비교 확인).

```bash
python3 tools/unicode_fallback/build.py --ucd <ucd-17.0.0> --cldr <cldr-release-48-2> --out <dir>
```

이름을 고르는 순서:

1. CLDR `type="tts"` 이름. 키워드 목록은 쓰지 않는다. 부모 locale 사슬(`parentLocales`, 없으면 태그를
   자름, `root` 제외)을 따라 상속을 해석하고, `↑↑↑` 상속 표식과 `unconfirmed`/`provisional` draft는 버린다.
   `annotations`가 `annotationsDerived`보다 우선한다.
2. 영어만: CLDR 이름이 없는 기호(S*)는 UCD 이름을 소문자로 바꾼 것(`SPHERICAL ANGLE` → `spherical angle`).
3. 숫자 키캡 `0️⃣`~`9️⃣`: 그 숫자 하나만 읽는다. 단어는 그 언어의 RBNF `spellout-numbering`에서 온다
   (ko `공 일 이 …`, en `zero one two …`, ja `〇 一 二 …`). RBNF가 없는 언어 약 70개는 숫자 키캡 항목이 없다.
   `#️⃣`, `*️⃣`는 기본 기호의 이름을 쓴다(`해시 기호`, `별표`).

대상과 제외:

- **넣는 것:** 기호(Sm, Sc, Sk, So), 이름 있는 Po, 이모지 기본 문자, RGI 시퀀스(키캡·국기·태그·피부색·ZWJ).
- **빼는 것:**
  - ASCII(엔진의 `speech_symbol` 등이 담당)
  - 글자·결합 부호·숫자·제어·서식 문자
  - 단독 피부색 수식자와 지역 표시 기호
  - 쉼을 나타내는 문장부호(`、。，：；！？…¿¡` 등), 괄호·따옴표·대시
  - 단어를 가르는 가운뎃점·글머리 기호(`유•초•중등`, `자택∙직장`, `10⋅26`의 `· • ∙ ⋅ ・` 등)
  - 범위 표시(`5∼6`의 `∼ ～ 〜`). 이름으로 읽으면 "물결표 연산자"가 된다.
- **UCD 영어 이름을 쓰지 않는 것:** 호환 분해가 있는 문자(`㎡` SQUARE M SQUARED, `①` 등). 이름이 글자
  모양을 설명할 뿐이고, 읽기는 분해 결과(단위·숫자)를 다루는 TN의 몫이다. CLDR 이름이 있으면 그 이름은 쓴다.

이 제외 목록은 한국어 말뭉치 회귀에서 나왔다. 제외하기 전에는 `∼` 197문장, `•∙⋅` 9문장, `㎡ ㏊ ㎿` 등
29문장이 이름으로 읽혀 오히려 나빠졌다(`5∼6교시` → 오 물결표 연산자 육교시, `6.5㏊` → 육 점 오 스콰 해).
제외한 뒤 한국어·영어 벤치마크 변화는 0이다. 이 두 묶음은 TN이 직접 읽어야 할 대상이다(후속 과제).
- **키 변환:** 키에서 U+FE0E/U+FE0F를 뺀다(CLDR 키와 같은 방식).
- **이름 변환:** `분류: 세부`의 콜론을 쉼표로 바꾼다(`깃발: 대한민국` → `깃발, 대한민국`).

## 3. 엔진 연결

적용 위치는 speech 출력에서 **다른 모든 규칙이 읽지 못한 곳 두 군데**뿐이다.

1. **이모지 시퀀스 묶음:** 공통 분류기가 국기 쌍, 피부색·표시 선택자·키캡·태그·ZWJ로 이어진 이모지를
   한 객체(`@unreadable-cluster`)로 묶는다. 시퀀스 이름이 있으면 전체를 한 번에 읽는다. 없으면 기존
   unreadable 정책을 적용한다. **일부만 읽지 않는다**(`🍔‍🍟` → 그대로).
2. **최종 관문의 `unknown` 분기:** `speech_character`, `speech_symbol`, 장식 문자 등 어떤 표에도 없는
   문자만 조회한다.

그 밖의 규칙:

- 장식 문자(`speech_decorative`: `★ ※ © ® ™ →` 등)는 `decorative` 정책을 따른다. `follow`에서는
  fallback도 읽지 않는다.
- 이름에 숫자가 있으면(`1st place medal`, `키 캡, 10`) fallback을 끈 채로 한 번 더 정규화한다
  (`first place medal`, `키 캡, 십`).
- 해당 언어에 이름이 없고 `symbol_fallback=on`이면 영어 파일의 이름을 쓴다. 영어가 아닌 문장에서는 그
  이름을 외래어 렌더러로 읽는다(`∢` → 스퍼리컬 애얼). 음차가 어색할 수 있어서 `native`를 두었다.
- 이름의 언어는 그 구간의 render 언어다. 문장 언어 라우팅(`docs/matrix-language.ko.md`)을 따르므로
  영어 문장의 이모지는 영어 이름으로 읽는다.
- text 모드는 바꾸지 않는다.

named option `symbol_fallback`(CLI `--symbol-fallback`, 웹 "기호·이모지 이름"):

| 값 | 동작 |
|---|---|
| `on` (기본) | 그 언어의 이름, 없으면 영어 이름 |
| `native` | 그 언어의 이름만 |
| `off` | 파일을 열지 않음. 이전 동작(unreadable 정책) |

## 4. 예

| 입력 (speech) | ko | en |
|---|---|---|
| `🍔` | 햄버거 | hamburger |
| `1️⃣` / `0️⃣` | 일 / 공 | one / zero |
| `🇰🇷` | 깃발, 대한민국 | flag, South Korea |
| `👨‍👩‍👧` | 가족, 남자 여자 여자 아이 | family, man, woman, girl |
| `👍🏽` | 올린 엄지, 갈색 피부 | thumbs up, medium skin tone |
| `❤️` | 빨간색 하트 | red heart |
| `🥇` | 금메달 | first place medal |
| `∢` | 스퍼리컬 애얼 (`native`면 그대로) | spherical angle |
| `🍔‍🍟` (등록 안 된 ZWJ) | 그대로 | 그대로 |

## 5. 검증과 한계

- 계약 `tests/evaluation/unicode_fallback_v24.jsonl` 28건. 이름은 고정한 CLDR/UCD에서 가져왔다.
- `tests/integration/test_unicode_fallback.py`는 다음을 확인한다.
  - 저장소의 ko/en 바이너리가 검토 TSV와 같은지
  - 헤더, 정렬, NUL 종료
  - 파일이 없을 때의 동작, 손상·짝 불일치 거부, `off`에서 파일을 열지 않는지
- 한국어·영어 벤치마크에는 이모지가 한 건도 없다. 효과는 계약으로만 확인한다. 두 벤치마크의 변화는 0이다.
- 기존 계약 16행은 이모지를 "이름 없는 문자"의 예로 써서 unreadable 정책을 검사한다(`5′`, `5¢` 음성 계약
  포함). 이 행들의 기대 텍스트는 그대로 두고 `symbol_fallback: "off"`로 조건만 고정했다.
- 조사 교정은 목표가 아니다. 이름 뒤에 붙은 조사가 받침과 맞지 않을 수 있다.
- 반복을 줄이지 않는다(`😂😂😂`는 이름 세 번).
- CLDR 이름은 설명형이라 길 수 있다(`기쁨의 눈물을 흘리는 얼굴`).
- 업데이트 절차: 새 CLDR·Unicode 릴리스를 받아 다시 빌드하고, TSV diff를 검토한 뒤 ko/en 파일을 교체한다.
