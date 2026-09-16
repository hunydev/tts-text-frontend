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
packs; a final phoneme-sequence request/output API remains future work.
