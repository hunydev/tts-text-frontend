# Semantic and pronunciation IR

`ttf_semantic_t` is the language-neutral interface between classification,
verbalization and inspection. It contains:

| Field | Meaning |
| --- | --- |
| `struct_size`, `type` | ABI layout marker and semantic category |
| `lexical_class` | Word, proper noun, initialism, acronym, alphanumeric or unknown Latin |
| `input_begin`, `input_end` | Original input UTF-8 byte interval |
| `value`, `secondary`, `unit` | Canonical string values, auxiliary value and unit/currency |
| `source_lang` | Surface/source language tag |
| `context_lang` | Speech context language tag |
| `render_lang` | Output rendering language tag |
| `rule`, `layer` | Recognition/override provenance |

The type vocabulary reserves TEXT, WORD, NUMBER, CARDINAL, ORDINAL,
DIGIT_SEQUENCE, COUNTER, DATE, TIME, DATETIME, PHONE, CURRENCY, PERCENT, MEASURE,
FRACTION, RANGE, VERSION, IP_ADDRESS, URL, EMAIL, ABBREVIATION, INITIALISM, ACRONYM,
ALNUM_TERM, SYMBOL and CUSTOM. An enum value does not imply complete grammar or
verbalization support. The implemented golden corpus documents supported forms. DATETIME remains reserved;
a combined date/time expression currently produces separate DATE and TIME spans.

```text
1st는 → ORDINAL(value="1", source="en-US", context="ko-KR", render="ko-KR")
1st floor → ORDINAL(value="1", source="en-US", context="en-US", render="en-US")
$12 → CURRENCY(value="12", unit="USD")
3kg / 3 kg → MEASURE(value="3", unit="kg")
1 / 2 → FRACTION(value="1", secondary="2")
14:30:05 → TIME(value="14:30:05")
9:05 AM → TIME(value="09:05", secondary="AM")
0.25%p → MEASURE(value="0.25", unit="percentage_point")
25bp → MEASURE(value="25", unit="basis_point")
1.5-2.5% → RANGE(value="1.5", secondary="2.5", unit="percent")
1.2e6원 → CURRENCY(value="1200000", unit="KRW")
$2.75million → CURRENCY(value="2750000", secondary="6", unit="USD")
$2M~$3M → RANGE(value="2000000", secondary="3000000", unit="USD")
계좌는 001-204 → DIGIT_SEQUENCE(value="001-204", secondary="계좌는", unit="financial_identifier")
2026년 6월 1일 → DATE(value="2026-06-01")
3.5만원 → CURRENCY(value="35000", unit="KRW")
1억2천3백만원 → CURRENCY(value="123000000", unit="KRW")
C++ → ALNUM_TERM(source="en-US", lexical_class=ALNUM_TERM)
11st → TEXT(rule="syntax.invalid-ordinal")
```

Numeric values are strings so the interface does not lose digits to floating
point conversion. The Korean compound parser performs exact decimal array
arithmetic and emits one span for the complete amount. Tests assert the semantic
value and source extent as well as the final string; several separate incorrect
semantic values can accidentally concatenate to the expected text. Fixed-size fields bound the initial ABI; very long numbers and
unsupported structures are conservatively handled rather than claiming full
arbitrary precision support. English ordinals exceeding `uint64_t` retain their
surface form instead of saturating to a different number. Output buffers grow independently of IR fields.

`ttf_span_t` associates the semantic object with output byte offsets, original
input offsets, script, lexicon provenance, independent `pronunciation_layer`,
actual semantic verbalizer tag and `context_trace`
with deterministic per-pack routing scores. A foreign ordinal can record an
English verbalizer, a Korean render language, and different semantic/pronunciation
resource layers. Results own their
span array. Segment span output offsets are local to the callback text; input
offsets refer to the original request.

`ttf_pronunciation_t` is a separate representation containing an alphabet name,
space-separated phone symbols and provenance. This keeps future phoneme output
separate from normalized text. Current API exposes pronunciation callbacks for
packs; a final phoneme-sequence request/output API remains future work. Korean has a
text-level pronunciation output instead (`pronunciation=hangul`, a Hangul
pronunciation string in the normalized text; see korean-phoneme.ko.md), which is not
a phone-symbol API.

An explicit pack identifier classification outranks generic numeric/date/phone
syntax only when both cover the same token count and byte extent. Exact/pattern
resource overrides still run afterward. Identifier labels are reading hints,
not verification that a number is a valid account. See [financial contracts](financial-text.md).

## Literal spans (v0.6)

Protected code, markup and paths use existing `TEXT`, with `unit=code|markup|path`,
empty numeric values, neutral source/context/render tags (`und`) and identity
rendering. Complete source/output offsets carry the literal, including paths
longer than a numeric value field. Diagnostic `protected.*` rules and layers
make the policy observable. These spans are opaque to context routing and further
TN rules; they do not claim a spoken pronunciation. Public ABI layouts are unchanged.

## Multiword pronunciation spans (v0.8)

An explicit foreign phrase can coalesce several lexical tokens into one `WORD`
span. `value` contains the canonical phrase key, `rule` is `lexicon.phrase`, and
`source_lang`, `context_lang`, `render_lang` remain independent. The dictionary
layer is present in both semantic resource provenance and pronunciation provenance.
The source extent includes original internal whitespace; the output extent covers
its exact pronunciation value. This does not change the public struct layout.
Consumers must not assume a fixed word-to-span count as lexicon resources expand.

## Named and partial dates (v0.13)

`DATE.value` may hold a full `YYYY-MM-DD`, partial `YYYY-MM` (`unit=year-month`),
`--MM-DD` (`unit=month-day`), or a contextual `YYYY` (`unit=year`). The partial
forms are this API's documented canonical strings; no missing field is inferred.
`secondary=mdy|dmy` records named-date input order. Existing numeric dates retain
empty unit/secondary. Consumers must examine `unit` before assuming three fields.
See [the complete date contract](english-dates.md).

The v0.21 work also uses `DATE(unit=weekday, value=1..7)` for an English
weekday in an adjacent `date(label)` expression; Monday is 1. `secondary`
holds the folded input alias (`fri` for `Fri.`), not a date order. The preceding
date and parentheses retain separate spans. Rendering uses the context pack's
`weekday` table. See [calendar alias resources](calendar-alias-resources.md).

Named months establish an English source language. A contextual year whose
source span contains only digits has source `und`; context and render language
remain independent. Calendar values render through the selected language pack,
so Korean date speech uses Korean year/month/day readings directly. The C ABI
layout is unchanged; this extends the accepted semantic values.

## Currency scale metadata (v0.12)

For an individual `CURRENCY` span, `value` always contains the exact amount in
major currency units. A nonempty `secondary` is the base-ten exponent of a
recognized written currency scale, not another amount: `$2.75million` has value
`2750000` and scale hint `6`. The English renderer reconstructs the coefficient
from these exact strings; the Korean renderer reads the expanded value. Ordinary
unscaled currencies keep `secondary` empty.

Interpret `secondary` by semantic type. In a `RANGE` it remains the exact second
endpoint, including currency ranges; it is never a scale hint. Fraction and time
uses are likewise unchanged. The signed amount and explicit currency code are
independent of the selected spoken form. Invalid conflicting currencies and
unrepresentable scale operations remain literal `TEXT` instead of exposing a
truncated financial value. Existing public ABI v1 fields and layouts are unchanged.
See [the complete contract](scaled-currency.md).

## Telephone and extension values (v0.18)

`PHONE.value` contains the written digits, optional leading `+`, and canonical `-`
between explicit groups. Parentheses and horizontal ASCII whitespace become group
boundaries; no country code or implicit grouping is guessed for compact input.
`secondary` holds extension digits including all leading zeros. New phone forms use
`unit=international` or `national`; established three-group hyphen phones retain the
legacy empty unit when no extension is present. Labels lie outside the phone span;
an accepted extension marker and its digits lie inside it. Numbers are never parsed
as integers for this representation.

```text
+1 (212) 555-0100 ext. 0042
  → PHONE(value="+1-212-555-0100", secondary="0042", unit="international")
Account: 212-555-0100
  → DIGIT_SEQUENCE(value="212-555-0100", secondary="account", unit="financial_identifier")
```

An explicit active identifier cue suppresses phone inference before the generic
phone candidate can outgrow the identifier. Existing pack specificity still governs
other numeric candidates. Compact signed numbers need an explicit telephone cue or
a declared strong extension hint; `+1250000` remains a cardinal and multiplication
`x` does not supply a strong phone cue by default. See [reading policies](phone-resources.md).
