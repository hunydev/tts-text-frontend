# Named English dates and contextual years

The English language pack recognizes named calendar dates before its lexical
classification step. Recognition produces the shared `DATE` semantic IR;
rendering remains the responsibility of the selected speech language pack.
Neither the core classifier nor the segmenter contains an English month list.

## Reading policy

These are project defaults, with single spaces in normalized text:

| Input | English output |
| --- | --- |
| `September 17, 2026` | `September seventeenth twenty twenty six` |
| `17 September 2026` | `the seventeenth of September twenty twenty six` |
| `September 17` | `September seventeenth` |
| `17 September` | `the seventeenth of September` |
| `September 2026` | `September twenty twenty six` |
| `in 1905` | `in nineteen oh five` |
| `in 1900` | `in nineteen hundred` |
| `in 2000` | `in two thousand` |
| `in 2005` | `in two thousand five` |
| `2026-09-17` | `September seventeenth two thousand twenty six` |

The ISO example deliberately retains the existing numeric-date year style.
The engine does not alter ISO output merely to match one external benchmark.
Named dates accept case-insensitive month aliases. Output month capitalization
comes from the existing `month` labels (`September`, not a forced lowercase
benchmark spelling). Aliases with an enabled `boundary_period` resource may
have a following period; that period belongs to the date candidate and does not
force a sentence callback. The source table enables this for short aliases;
acceptance is controlled by the resource, rather than alias length.

For a `calendar` year style, years before 1000 and multiples of 1000 use the
ordinary cardinal renderer. Years 2000 through 2009 also use cardinal reading.
Other four-digit years use century and remainder, with `hundred` for a zero
remainder and `oh` before a single-digit remainder. This is one deterministic
service policy; acceptable spoken English has alternatives. The initial pack
does not select between variants probabilistically.

## Semantic representation

Public ABI version 1 and its fixed-size fields remain unchanged:

| Meaning | `type` | `value` | `unit` | `secondary` |
| --- | --- | --- | --- | --- |
| Full named date, month first | `DATE` | `2026-09-17` | empty | `mdy` |
| Full named date, day first | `DATE` | `2026-09-17` | empty | `dmy` |
| Full numeric date | `DATE` | `2026-09-17` | empty | empty |
| Month and day | `DATE` | `--09-17` | `month-day` | `mdy` or `dmy` |
| Month and year | `DATE` | `2026-09` | `year-month` | `mdy` |
| Contextual year | `DATE` | `1905` | `year` | empty |

A partial date never invents the missing year or day. The named-date classifier
validates actual month lengths and Gregorian leap years; a month/day without a
year can represent February 29. Named full dates can represent years 0001–9999.
The narrower contextual-year limits below do not constrain an explicit named
date. The trace retains input offsets, source/context/render languages, rule,
and resource layer. Named dates have English source language (`en-US`). A bare
contextual year keeps neutral source language (`und`); its English contextual
cue selects a year interpretation without claiming that the digit spelling is
English. Speech language remains a separate router decision.

The Korean renderer uses the same canonical date fields and its existing
year/month/day resources, regardless of the source order. For example:

```text
마감은 September 17, 2026입니다.
→ 마감은 이천이십육 년 구 월 십칠 일입니다.
```

The established Korean spacing is retained: most months use the quantity
spacing (`구 월`), while June and October retain `유월` and `시월`. Month/day and
year/month values omit only their absent component. No English-to-Korean
date conversion is implemented in the core.

## Editable resources

The source table is
[`resources/languages/en-US/rules/dates.tsv`](../resources/languages/en-US/rules/dates.tsv).
It is loaded through the normal resource manifest and compiler. Each data line
has four tab-separated columns: `kind`, `language`, `key`, `value`.

| Kind | Purpose and valid values |
| --- | --- |
| `month_alias` | Lowercase input alias to month integer `1`–`12`; do not put the period in the key |
| `date_policy` / `named_year_style` | `calendar` by default; `cardinal` also accepted |
| `date_policy` / `numeric_year_style` | `cardinal` by default; `calendar` also accepted |
| `date_policy` / `year_style` | `calendar` by default; `cardinal` also accepted |
| `date_policy` / `min_year`, `max_year` | Default contextual-year recognition window `1000`–`2099` |
| `date_word` | DMY connective labels `the` and `of` |
| `date_year_cue` | Lowercase preceding contextual cue, enabled with `1`. An opening bracket (`(`, `[`, `{`) is also a valid key; the pack reads it as a cue only when the bracket holds the number and nothing else, so `( 1965 )` is a year while `( 2003 and 2005 )` keeps its cardinals |
| `date_year_cue_after` | One ASCII mark that follows a four-digit number and introduces a year, enabled with `1`. `:` is declared, for the timeline shape `1830 : The first Constitution` |
| `decade_plural` | Plural decade word for a tens value `10`–`90`, or `100` and `1000` for a round decade (`nineteen hundreds`, `two thousands`) |
| `date_year_quantity` | Lowercase following quantity noun that prevents a bare-year match, enabled with `1` |
| `boundary_word` | Keep a possible continuation after this word, enabled with `1` |
| `boundary_period` | Permit and retain the period after a date alias inside its candidate, enabled with `1` |

An invalid year style returns `TTF_ERESOURCE` when that style is used. Do not put
spoken month labels into `month_alias`; change the existing `month` resources
for output labels instead. A new input month alias should have the corresponding
`boundary_word` record and, when abbreviated with a period, a `boundary_period`
record. These hints are necessary for batch/stream agreement with short segment
targets. They can retain additional pending input; they do not turn a word into
a date by themselves.

Example customer table, using actual tabs:

```text
date_policy	en-US	named_year_style	cardinal
date_policy	en-US	year_style	cardinal
```

Load this TSV at the customer resource layer, or put it in a customer manifest.
Customer resources override domain and language-system resources. Rebuild the
resource artifact when using compiled resources; rebuilding the C library is
unnecessary. With this override, `September 17, 2026` ends in
`two thousand twenty six` while keeping the same semantic value. See
[the resource editing guide](resource-editing-guide.ko.md) for loading and
validation commands.

## Guards and current limits

A four-digit number alone remains a cardinal. Contextual-year recognition needs
a configured preceding cue, a value inside the configured year window, and no
following quantity guard. For example, the initial resources distinguish
`in 1998` from `in 1998 copies`. The guard list includes finance and content
terms such as shares, accounts, transactions, pages, views and subscribers,
duration/measurement nouns, and frames, samples, tracks, episodes, videos and
clips. Singular and plural forms are separate resource keys.
`May I continue?` does not become a date because a month alias must participate
in a date-shaped candidate.

These heuristics do not understand every possible noun. Customers should add
their domain's quantity nouns to `date_year_quantity` and test the whole
sentence. Bare contextual years remain an explicitly conservative prototype;
catalog numbers, document identifiers and prose ellipses require additional
coverage. Invalid or unsupported date-shaped input is retained rather than
silently repairing a day or changing its year.

This work does not add BC/AD eras, relative dates, date ranges, holidays,
or ambiguous locale-dependent numeric forms. A plural decade (`the 1960s`, `the
80s`, `the '90s`) was added on 2026-09-25: the digits must end in a zero and
carry nothing after the `s`, so `the 1965s`, `787s` and `MP3s` keep their text
and a model-number plural is still out of scope. Existing universal numeric-date
grammar remains in place. Text exact match can also differ because a reference
uses a valid alternative year reading, different capitalization, or a different
date order. Report these differences separately from calendar/value errors.
External development samples are a coverage instrument, not proof of full
production accuracy; held-out domain examples and independent review remain
necessary. No audio synthesis or audio scoring is part of this verification.

## References and provenance

Reviewed 2026-09-17:

- [Cambridge Dictionary, English Grammar Today: Dates](https://dictionary.cambridge.org/grammar/british-grammar/dates)
  describes written MDY/DMY forms, ordinal days, optional spoken connectives,
  and several accepted year readings. It supports treating these choices as
  policy instead of declaring a single spelling universally correct.
- [Cambridge Dictionary blog: How to say years](https://dictionaryblog.cambridge.org/2017/10/04/1066-and-all-that-how-to-say-years/)
  discusses paired year readings, century endings, and US/UK alternatives after
  2000. The project chooses a stable subset and documents its limits above.

Only factual conventions and links were used. No article text, upstream code,
dataset examples, or third-party tables are vendored by this feature. These
resources and synthetic test sentences were written for this repository.
