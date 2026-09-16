# Korean coverage and reading policy

The initial Korean pack is usable for text input to a Korean TTS engine. The
following outputs were verified with the built CLI and corpus tests.

| Category | Input | Output |
| --- | --- | --- |
| Sino-Korean | `0 1 10 11 100 101 1000 10000 100000000` | `영 일 십 십일 백 백일 천 만 일억` |
| Native counters | `3개 3명 3살 3시` | `세 개 세 명 세 살 세 시` |
| Native contraction | `20개 21개 99명` | `스무 개 스물한 개 아흔아홉 명` |
| Wider counters | `2마리 4권 3시간 3개월 3달` | `두 마리 네 권 세 시간 삼 개월 세 달` |
| Sino-Korean units | `3분 3초 3원` | `삼 분 삼 초 삼 원` |
| Dates | `2026-09-16`, `2026.09.16` | `이천이십육 년 구 월 십육 일` |
| Times | `14:30`, `09:05` | `열네 시 삼십 분`, `아홉 시 오 분` |
| Currency | `12,500원`, `₩12,500` | `만이천오백 원` |
| Spaced quantity | `3 개`, `3 kg`, `12,500 원` | `세 개`, `삼 킬로그램`, `만이천오백 원` |
| Scaled currency | `3.5만원`, `1.25억원` | `삼만오천 원`, `일억이천오백만 원` |
| Compound currency | `1억2천3백만원`, `1억 2,500만원` | `일억이천삼백만 원`, `일억이천오백만 원` |
| Multiple magnitudes | `2조3억4만원` | `이조삼억사만 원` |
| Korean date notation | `2026년 6월 1일` | `이천이십육 년 유 월 일 일` |
| Seconds | `14:30:05` | `열네 시 삼십 분 오 초` |
| Fraction | `3/4`, `-1/2` | `사 분의 삼`, `마이너스 이 분의 일` |
| Percent | `12%`, `12.5%` | `십이 퍼센트`, `십이 점 오 퍼센트` |
| Measures | `3kg 10km 32GB 2GHz` | `삼 킬로그램 십 킬로미터 삼십이 기가바이트 이 기가헤르츠` |
| Telephone | `010-1234-5678` | `공일공 일이삼사 오육칠팔` |
| Telephone | `02-123-4567` | `공이 일이삼 사오육칠` |
| Ordinal in Korean | `1st는 첫 번째입니다.` | `퍼스트는 첫 번째입니다.` |
| Compound ordinal | `21st는 순서입니다.` | `트웬티 퍼스트는 순서입니다.` |
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
value limit fall back to individual digits. Decimal readings preserve trailing zeros.
Fraction IR holds the numerator and denominator separately; Korean rendering puts
the denominator first. Signed fractions place the sign before the entire reading.

The Korean classifier recognizes complete `YYYY년 M월 D일` forms, including adjacent
markers and spaces. Invalid calendar forms such as `2026년2월29일` remain unchanged.
June and October use `유 월` and `시 월` under the existing spaced-unit policy. This
is an explicit output-text convention; applications wanting orthographic `유월`
and `시월` need to choose that policy before adopting their reference corpus.

## Compound amount grammar

Scale records now represent **atomic powers**: `십=1`, `백=2`, `천=3`, `만=4`,
`억=8`, `조=12`, and `경=16`. A minor group uses decreasing `천/백/십` terms;
a major scale multiplies the entire minor group. Major groups also decrease.
Coefficients are Arabic digits with an optional decimal fraction and correctly
grouped commas. Spaces may separate a coefficient, its scale and the final unit.
A single leading sign applies to the entire amount. A currency prefix is read
from `currency_prefix` resources and must agree with an explicit currency suffix.

| Surface | Canonical Semantic IR value | Rendered text |
| --- | --- | --- |
| `1억2천3백만원` | `123000000` | `일억이천삼백만 원` |
| `1억 2,500만원` | `125000000` | `일억이천오백만 원` |
| `2조3억4만원` | `2000300040000` | `이조삼억사만 원` |
| `-1억2천만원` | `-120000000` | `마이너스 일억이천만 원` |
| `1억2.5천만원` | `125000000` | `일억이천오백만 원` |
| `1억0.00001만원` | `100000000.1` | `일억 점 일 원` |
| `₩1억2천만` | `120000000` | `일억이천만 원` |

A valid amount produces one `CURRENCY` span with the entire input byte range,
`unit=KRW` and `rule=ko:compound-currency`. Its value represents the sum, not just
the first written component. Recognized counters and measures use the same
arithmetic with their own semantic type. Bare scale expressions are `CARDINAL`.
Text evaluation must check this metadata because concatenated independent
readings can accidentally resemble a correct amount while holding incorrect IR.

All addition and multiplication use decimal digit arrays; neither floating point
nor fixed-width integer money arithmetic is used. A coefficient is limited to
96 written digits, and the final canonical value to 95 bytes including a sign
and decimal point. Values exceeding these bounds are preserved as text. Scale
values may be configured through exponent 40 (minor 1–3, then multiples of 4).
The default input scale inventory stops at `경` to avoid collisions with ordinary
words; the Sino-Korean output magnitude inventory still extends through `정`.
Fractional won are preserved exactly and read with `점`; there is no monetary
rounding or currency settlement policy. Expanded values omit insignificant
trailing fractional zeros.

Repeated, ascending, or overlapping components are rejected as a whole numeric
candidate: `1억2억원`, `2만3억원`, `1천23백원`, `1억10000만원`, malformed grouping
`1억2,50만원`, and an internal sign `1억-2천만원` stay unchanged. A later component
must be smaller than the preceding scale unit. Contradictory currencies such as
`$1억원` also stay unchanged. Inspect identifies rejected candidates with
`ko:invalid-compound-number`; their numeric value is empty. This is a conservative
normalization policy, not a claim that every unusual written expression lacks a
possible human interpretation.

A scale embedded in an ordinary Hangul word must not be treated as multiplication:
`1.5천국` retains a separate `1.5` number, and `3백과사전` retains a separate `3`.
Attached particles are accepted only when the complete remainder matches a
`particle` or `quantity_tail` resource. Common predicate forms (`이며`, `이고`),
compound particles (`으로는`, `에도`) and financial qualifiers (`이상`, `미만`)
are provided in `numeric.tsv`. Extend `quantity_tail` with domain/customer records
for further morphology; arbitrary word prefixes are deliberately not enough.
All-Hangul numerals (`일억원`), omitted coefficients (`1천백원`), and arithmetic
operators inside an amount are outside this grammar.

**Resource migration from 0.3:** scale aliases `십만=5`, `백만=6`, `천만=7` have
been replaced by atomic `십/백/천` followed by `만`. Existing source aliases must
be converted to atomic records; arbitrary exponent aliases are not accepted by
the compound parser. The former combined names remain `numeric_continuation`
entries for segmentation. This is a resource schema change; the public C ABI and
Semantic IR layout are unchanged.

Counter policy distinguishes native `시간`, `마리`, `권`, `잔`, `병`, `장`, `벌`,
`켤레`, `송이`, `자루`, `달` from Sino-Korean `개월`, `년`, `월`, `일`, `주`, `번`,
`층`, `호`. These are defaults; homographic units need contextual rules for uses such
as chapter numbers versus sheets of paper. No claim of unrestricted Korean TN is made.

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
normalized-text quality before adopting an application's expected-output policy.

A dictionary-free emergency renderer produces Korean text even when the source
G2P cannot produce phones. The direct mapper tests verify `wifi → 위피` and a
nonempty Hangul result for every ASCII letter. This renderer prioritizes a
speakable result; it does not promise a linguistically correct pronunciation.

Resource and customer policies can replace number labels, counter readings,
context weights, foreign lexicon entries, and range separators without a core
rebuild. See [the Korean pack guide](../languages/ko-KR/README.md) for exact files
and [resource documentation](resources.md) for priority and load rules.
