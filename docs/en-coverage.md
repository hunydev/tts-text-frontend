# English and shared numeric grammar

This document describes normalized **text** policy. It does not define a speech
synthesis score or claim complete English linguistic coverage.

## Reading policy

| Input | Text with `en-US` context | Semantic IR |
| --- | --- | --- |
| `1,234.50` | `one thousand two hundred thirty four point five zero` | CARDINAL, value `1234.50` |
| `.5` | `zero point five` | CARDINAL, value `0.5` |
| `21st` | `twenty first` | ORDINAL, value `21`, source `en-US` |
| `2024-02-29` | `February twenty ninth two thousand twenty four` | DATE, value `2024-02-29` |
| `2026. 9. 16.` | `September sixteenth two thousand twenty six` | DATE, value `2026-09-16` |
| `14:30:05` | `fourteen thirty and five seconds` | TIME, value `14:30:05` |
| `09:00:01` | `nine o'clock and one second` | TIME, value `09:00:01` |
| `$ 12.50` | `twelve dollars and fifty cents` | CURRENCY, value `12.50`, unit `USD` |
| `12.5 %` | `twelve point five percent` | PERCENT, value `12.5` |
| `1.0 kg` | `one point zero kilogram` | MEASURE, value `1.0`, unit `kg` |
| `-3 / 4` | `minus three over four` | FRACTION, value `-3`, secondary `4` |
| `32 °C` | `thirty two degrees Celsius` | MEASURE, value `32`, unit `°C` |

Horizontal space is optional between a numeric quantity and its unit or percent
sign, after a currency prefix, and around the fraction slash. Newlines are not
numeric-grammar whitespace. Negative and positive decimal quantities and leading
zero omitted decimals are accepted. Quantity agreement uses the absolute numeric
value (`-1 kg`, `1.0 kg` use singular). Integer values through `UINT64_MAX` use
cardinal words; larger cardinals use digit sequences. Oversized ordinals retain
the surface because inventing an ordinal from a truncated integer is unsafe.

Time policy retains 24-hour values rather than converting them to AM/PM. Optional
seconds are explicit. The generic classifier validates hour, minute and second
ranges; leap seconds are not supported. Fractions use the unambiguous `over`
policy, not irregular `half` / `quarter` readings. A zero denominator is invalid.
Dates have a Gregorian month/day/leap-year check. Numeric ranges retain the
existing integer `to` policy.

## Recognition data and language independence

The core contains numeric scanning, grammatical boundary validation, and semantic
classification algorithms. Written unit inventories and labels remain resources:
`numeric_suffix`, `currency_prefix`, `ordinal_suffix`, and `ordinal_tail`.

`ordinal_tail` is a source-pack policy. Lookup tries the longest suffix of the
numeric value first: tails `11`, `12`, `13` require `th`; the final decimal digits
supply the remaining endings. The core does not hardcode English suffix strings.
This prevents `11st`, `12nd`, `13rd`, and `21th` from becoming valid ordinals.

`semantic_term` entries declare technical forms that cross tokenizer boundaries:

```text
semantic_term<TAB>*<TAB>C++<TAB>ALNUM_TERM:en-US
semantic_term<TAB>*<TAB>USB-C<TAB>ALNUM_TERM:en-US
semantic_term<TAB>*<TAB>3D<TAB>ALNUM_TERM:en-US
```

Supported values begin with `ALNUM_TERM:`, `WORD:`, `ACRONYM:`, or `INITIALISM:` and
end with a registered source language tag. Keys are case sensitive; the entire
term must end before another ASCII letter, digit, underscore, plus, or hyphen.
For example, `C++x` does not match the `C++` record. These are classification
records; their pronunciations belong in a separate pronunciation lexicon.

The English context scorer treats known unit markers as neutral even when
separated from the quantity (`3 kg`) or prefixed by a symbol (`32 °C`). A unit
marker alone must not turn an otherwise Korean quantity into English speech.

## Conservative handling and limits

Malformed structured objects are preserved as TEXT, with an inspectable
`syntax.invalid-*` or `syntax.unsupported-numeric` rule:

- Bad grouping: `12,34`, `1234,567`, `1,234,56`, `1,,234`.
- Invalid dates: `2026-02-29`, `1900-02-29`, `2026-13-01`, `2026-04-31`.
- Invalid times: `24:00`, `12:60`, `12:3`, `12:30:60`.
- Invalid fractions and suffixes: `1/0`, `1/2/3`, `1.2nd`, `10kmh`.
- Unsupported attached identifiers: `123abc` and ISO datetimes such as
  `2026-09-16T14:30`.

Preservation prevents a false normalized prefix from corrupting the whole object;
it is not a claim that the TTS engine can pronounce the preserved string well.
URLs, email addresses, versions and IP addresses also retain their original
text. Bounded scientific notation, decimal/signed ranges with one trailing unit,
`bp`/`%p`, and uppercase AM/PM are supported as documented in
[financial text contracts](financial-text.md). Time zones, natural-language dates,
locale-dependent currency formats, repeated-unit ranges and DATETIME rendering
remain outside this iteration. Group separators are commas and the decimal separator is
a period. English pronunciation fallback remains a prototype.

The English golden corpus contains independent positive and negative expected
text. Shared streaming tests separately verify that the new spaced constructions
survive arbitrary UTF-8 byte chunk boundaries and short phrase targets.

The v0.6 shared literal recognizer protects explicit code/markup/path spans before
English classification; see [literal contracts](protected-text.md). This does not
extend English G2P or define spoken path/code pronunciation.
