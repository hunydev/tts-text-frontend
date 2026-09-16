# Korean coverage and reading policy

The initial Korean pack is usable for text input to a Korean TTS engine. The
following outputs were verified with the built CLI and corpus tests.

| Category | Input | Output |
| --- | --- | --- |
| Sino-Korean | `0 1 10 11 100 101 1000 10000 100000000` | `영 일 십 십일 백 백일 천 만 일억` |
| Native counters | `3개 3명 3살 3시` | `세 개 세 명 세 살 세 시` |
| Native contraction | `20개 21개` | `스무 개 스물한 개` |
| Sino-Korean units | `3분 3초 3원` | `삼 분 삼 초 삼 원` |
| Dates | `2026-09-16`, `2026.09.16` | `이천이십육 년 구 월 십육 일` |
| Times | `14:30`, `09:05` | `열네 시 삼십 분`, `아홉 시 오 분` |
| Currency | `12,500원`, `₩12,500` | `만이천오백 원` |
| Percent | `12%`, `12.5%` | `십이 퍼센트`, `십이 점 오 퍼센트` |
| Measures | `3kg 10km 32GB 2GHz` | `삼 킬로그램 십 킬로미터 삼십이 기가바이트 이 기가헤르츠` |
| Telephone | `010-1234-5678` | `공일공 일이삼사 오육칠팔` |
| Telephone | `02-123-4567` | `공이 일이삼 사오육칠` |
| Ordinal in Korean | `1st는 첫 번째입니다.` | `퍼스트는 첫 번째입니다.` |
| Ordinal in English | `Please go to the 1st floor.` | `Please go to the first floor.` |
| Foreign word | `Apple은 사과입니다.` | `애플은 사과입니다.` |
| Initialism | `GPU를 사용합니다.` | `지피유를 사용합니다.` |
| Brand | `OpenAI가 발표했습니다.` | `오픈에이아이가 발표했습니다.` |
| Unknown-word phones | `foobar를 사용합니다.` | `푸바를 사용합니다.` |

Core recognition creates a semantic value before any of these readings. Inspect
shows `source=en-US context=ko-KR render=ko-KR semantic=ORDINAL value="1"` for the
first ordinal; the English example has the same source and semantic value with
English context and rendering. English source text in an English sentence is
preserved.

Native counters cover 1–99, including contractions before a counter. Zero and
100 or greater use Sino-Korean readings. Times preserve the 24-hour number;
AM/PM conversion is not enabled. Sino-Korean composition supports four-digit
magnitude groups through `정` (10^40). Larger values within the 95-byte semantic
value limit fall back to individual digits. Fractions preserve trailing zeros.

Korean context uses three external settings: a Hangul presence floor, a strong
score for a Hangul token itself, and an attached-particle score. The particle
inventory also lives in resources. The local English function-word detector
can preserve English stretches in mixed text. Ambiguous quotations, unusual
punctuation and very long code switches still need a broader evaluation corpus.

The foreign pronunciation stack distinguishes production extension interfaces
from prototype linguistic quality. The lexicon layers, dictionary lookup,
shared phone interface, and fallback dispatch all execute in the initial
library. The default pronunciation dataset is deliberately small. The mapper
supports onset/vowel composition, nasals and lateral codas, glides, and selected
diphthongs. It does not implement the complete Korean loanword spelling rules
or a full English phonology model. Its final `R` omission is a simple adaptation
rule, not a general dialect model. Unknown pronunciations must be reviewed for
quality before deployment in a customer voice.

A dictionary-free emergency renderer produces Korean text even when the source
G2P cannot produce phones. The direct mapper tests verify `wifi → 위피` and a
nonempty Hangul result for every ASCII letter. This renderer prioritizes a
speakable result; it does not promise a linguistically correct pronunciation.

Resource and customer policies can replace number labels, counter readings,
context weights, foreign lexicon entries, and range separators without a core
rebuild. See [the Korean pack guide](../languages/ko-KR/README.md) for exact files
and [resource documentation](resources.md) for priority and load rules.
