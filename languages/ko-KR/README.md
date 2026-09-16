# Korean language pack

`pack.c` exports `ttf_pack_ko_KR` through the versioned language-pack ABI. It
contains Korean number composition, deterministic context scoring, and phonetic
realization. The core does not dispatch on Korean language IDs.

Reading data live in `resources/languages/ko-KR/`:

| Resource | Purpose |
| --- | --- |
| `rules/numeric.tsv` | Digits, magnitudes, native numbers, counters, units, currency labels, range policy |
| `rules/context.tsv` | Particle inventory and context weights |
| `lexicon/common/default.tsv` | Common foreign words and ordinal pronunciations |
| `lexicon/brands/default.tsv` | Product and organization pronunciations |
| `lexicon/technology/default.tsv` | Technology pronunciations |
| `lexicon/acronyms/default.tsv` | Initialism and acronym pronunciation defaults |
| `dictionaries/phones.tsv` | Shared phone inventory to Hangul onset, vowel, coda and letter-name mappings |
| `dictionaries/graphemes.tsv` | Emergency Latin grapheme pronunciations |

The resource manager loads these files through `resources/manifest.ttfm`.
Customer or domain resource files can override individual keys without compiling
the language pack again. Resource values preserve spaces, including the leading
and trailing spaces of `range_separator` and `number/decimal`.

## Reading conventions

- Ordinary numbers use Sino-Korean units: `10000 → 만`, `100000000 → 일억`.
- Decimal places preserve every digit: `12.50 → 십이 점 오영`.
- Counters `개`, `명`, `살`, `시` use native numbers from 1 through 99; `20개 → 스무 개`
  and `21개 → 스물한 개`. Zero and values of 100 or more use Sino-Korean readings.
- Times preserve the 24-hour value: `14:30 → 열네 시 삼십 분`.
- Dates separate number and unit: `2026-09-16 → 이천이십육 년 구 월 십육 일`.
  June and October use irregular stems `유 월`, `시 월`.
- Telephone zero uses `공`; hyphens become spaces.
- `policy/range_mode=digits` changes range components from cardinals to digit
  sequences; `policy/range_separator` changes their connector.

## Foreign pronunciation

Pronunciation lexicon lookup belongs to the frontend pipeline and precedes the
phone renderer. This pack consumes source-independent `ttf-phones-v1` phones and
composes Hangul syllables. For example, `W AY F AY → 와이파이` and
`F UW B AA R → 푸바`. Letter phones such as `letter.C` support initialisms.

The phonetic mapper is deliberately a prototype: consonant adaptation, vowel
mapping, nasal/lateral codas, glides, and selected diphthongs are supported;
comprehensive loanword orthography and stress-conditioned realization are not.
If a source cannot produce usable phones, longest-prefix emergency grapheme
resources still produce speakable Korean, for example `wifi → 위피`.
`tests/unit/test_ko_pack.c` verifies that every ASCII letter has a nonempty Korean
fallback.

Language context uses resource weights and the same script tokens as semantic
classification. Attached particles strongly favor Korean speech context even
when the source token is English. Longer mixed sentences remain heuristic;
explicit foreign quotations without clear local signals may need customer
resources or future router improvements.
