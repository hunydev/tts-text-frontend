# 영어 문장을 한글로 읽기 (`foreign_sentence`)

갱신: 2026-10-28. 프로젝트 소유자 요청.

한국어 요청(`--language ko-KR`)에서도 완결된 영어 문장은 영어로 판정해 영어로 읽는다
([context-routing.md](context-routing.md)). 영어를 읽지 못하는 한국어 전용 TTS에 보낼 때는
그 문장도 한글이어야 하므로 옵션을 둔다.

| 옵션 | 값 | 동작 |
|---|---|---|
| `foreign_sentence` (CLI `--foreign-sentence`, 웹 "영어 문장") | `keep` (기본) | 영어 문장은 영어로 읽는다 |
| | `localize` | 영어 팩의 읽기를 한글로 옮긴다 |

## 동작

1. **판정은 그대로:** 문장 언어와 낱말별 문맥 점수는 바꾸지 않는다. 한국어 문장, 한글 조사가 붙은
   영어 낱말(`Apple은`)은 원래대로 한국어 경로로 읽는다.
2. **영어로 먼저 읽기:** 영어로 판정된 토큰은 영어 팩이 읽는다. 숫자·서수·시각·날짜·통화·약어의
   영어 규칙이 그대로 적용된다(3 three, 1st first, 3pm three p m, 2019 twenty nineteen, $500 five hundred dollars).
3. **한글로 옮기기:** 그 읽기의 라틴 낱말을 한국어 쪽 외래어 읽기 순서로 옮긴다. 순서는 사용자·고객 사전,
   프로젝트 렉시콘, 표준국어대사전 외래어, CMU(켜졌을 때), 글자 이름, 철자 규칙이다
   ([english-words.ko.md](english-words.ko.md)). 다섯 글자 이하 대문자 낱말은 글자 이름으로 읽는다(GPU 지피유).
4. **구문 사전:** 여러 낱말 렉시콘(`lexicon ko-KR en-US:thank you 땡큐`, Bank of America)이 영어 문장 안에서도 맞는다.
5. **범위:** speech 출력에서만, 기본 언어 팩이 영어 팩이 아닐 때만 적용한다. text 출력과 영어 요청은 바뀌지 않는다.

| 입력 (ko-KR, speech) | `keep` | `localize` (CMU 없음) | `localize` (CMU) |
|---|---|---|---|
| `I have 3 apples.` | I have three apples. | 아이 헤이브 쓰리 애플스. | 아이 해브 쓰리 애플스. |
| `Meeting at 3pm on Oct 5, 2019.` | Meeting at three p m on October fifth twenty nineteen. | 미팅 앳 쓰리 피 엠 온 옥토버 피프스 트웬티 나인틴. | 같음 |
| `Please go to the 1st floor.` | Please go to the first floor. | 플리스 고 투 더 퍼스트 플로어. | 플리즈 고 투 더 퍼스트 플로어. |
| `The GPU costs $500.` | The GPU costs five hundred dollars. | 더 지피유 카스트스 파이브 헌드레드 달러스. | 같음 |
| `오늘 Thank you for coming 3명` | 오늘 Thank you for coming 세 명 | 오늘 땡큐 포 카밍 세 명 | 오늘 땡큐 포 커밍 세 명 |

## 문장 언어 `declared`와 함께 켤 때 (2026-10-29)

`matrix=declared`는 한글이 있는 문장을 한국어 문장으로 고정하고, 한글 없는 한자·가나 문장도 한국어로 읽는다.
둘 다 켜면 영어 낱말이 다수인 문장(문장 언어 투표가 영어)은 `localize`가 정한다. 문장 안에 한글 이름이
끼어 있어도 영어 문장으로 읽고 한글로 옮긴다. 그 밖의 문장은 `declared` 그대로다. 결과는 영어 문장에서
`localize`만 켰을 때와 같다. 소유자 요청에 따른 동작이며, 전에는 `declared`가 이겨 숫자가 한국어 수(삼)였다.

| 입력 (ko-KR, speech, `--hanja korean --kana korean`, CMU) | `declared`만 | `localize`만 | 둘 다 |
|---|---|---|---|
| `My friend 민수 bought 3 books.` | 마이 프렌드 민수 밧 삼 북스. | 마이 프렌드 민수 밧 쓰리 북스. | 마이 프렌드 민수 밧 쓰리 북스. |
| `오늘 Thank you for coming 3명` | 오늘 Thank you for coming 세 명 | 오늘 땡큐 포 커밍 세 명 | 오늘 땡큐 포 커밍 세 명 |
| `我们明天见` | 아문명천견 | 아们명천见 | 아문명천견 |
| `東京で会いましょう` | 동경데회이마쇼 | 동경데会이마쇼 | 동경데회이마쇼 |

한국어 전용 TTS로 보낼 때는 둘 다 켜는 조합이 빈틈이 가장 적다.

## 숫자

영어 문장 속 숫자는 영어 수를 한글로 옮긴다(쓰리 애플스). 한국 사람이 영어 문장을 소리 내 읽는 방식과 같고,
연도를 둘씩 끊기·서수·시각 같은 영어 규칙을 다시 만들 필요가 없다. 한글 단위나 조사가 붙은 숫자(3명, 5개,
3시에, 1,000원, 1990년)는 한국어 문법에 속하므로 한국어 수로 읽는다. 이것은 옵션과 무관한 기본 동작이다
(2026-10-28; 전에는 앞의 영어 절을 따라 five 개, three 명).

## CMU

CMU가 없으면 사전에 없는 낱말은 철자 규칙으로 읽어 거칠다(have 헤이브, please 플리스). 웹 워크벤치의 "영어 발음
사전" 기본값은 **자동**으로, `localize`일 때 CMU 내보내기 파일이 있으면 함께 켠다. C API·CLI는 호스트가 같은
파일을 도메인 층으로 불러온다(`--domain benchmark/generated/cmudict-runtime-full/pronunciation.tsv`). 파일은
라이선스 고지(BSD 형식)와 크기 때문에 저장소에 없다.

## 복수형

사전에 없는 복수형은 단수형 읽기에 `스`를 붙인다(apples 애플스, dollars 달러스, games 게임스). 단수형이
렉시콘이나 표준국어대사전에 있을 때만 쓰고(CMU보다 먼저), 어미는 `context ko-KR loanword_plural`(값 `스`)이다. 기본 한국어
문장의 영어 낱말에도 적용된다(`apples를 샀다` 애플스를). 한국어 릴리스 벤치마크 142,248구간에서 바뀐 출력은
8seconds 6건(에이트세콘드스 → 에이트세컨드스)이고 판정 변화는 없다.

## 남은 한계

- 철자 규칙 읽기의 품질(friend 프린드, today 토데이)은 CMU를 켜거나 렉시콘에 넣어야 나아진다.
- 관례 표기와 CMU 미국식 발음이 다를 수 있다(costs 카스트스). 자주 쓰는 낱말은 렉시콘으로 고정한다.
- 관용 표현은 구문 사전 행으로 읽는다(thank you 땡큐는 사전 v28에 있다).

계약: `foreign_sentence_v128`(19).
