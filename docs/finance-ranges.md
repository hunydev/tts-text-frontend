# Repeated financial quantity intervals (v0.7)

The text frontend recognizes two explicit, matching quantity endpoints. This is
an exact text/semantic operation: it does not calculate returns, infer missing
currency, convert units, or assess synthesized speech.

| Source | Korean text | Canonical RANGE |
|---|---|---|
| `1만원~2만원` | `만 다시 이만 원` | `value=10000`, `secondary=20000`, `unit=KRW` |
| `1억 원 - 2억 원` | `일억 다시 이억 원` | `100000000`, `200000000`, `KRW` |
| `$10-$20` | `십 다시 이십 달러` | `10`, `20`, `USD` |
| `1.5% ~ 2.5%` | `일 점 오 다시 이 점 오 퍼센트` | `1.5`, `2.5`, `percent` |
| `25bp~50bp` | `이십오 다시 오십 베이시스포인트` | `25`, `50`, `basis_point` |
| `1kg~2kg` | `일 다시 이 킬로그램` | `1`, `2`, `kg` |

In English context `$10-$20` renders `ten to twenty dollars`, and
`1.5%~2.5%` renders `one point five to two point five percent`. The existing RANGE
renderer states the unit once after the second number. Currency interval decimal
endpoints use decimal readings: `$1.25~$2.50` becomes
`one point two five to two point five zero dollars`. Scalar currency's cents
policy does not apply to RANGE.

## Syntax and exact values

- Default delimiters: ASCII `-` and `~`, with optional spaces or tabs around them.
- Both endpoints explicitly carry currency, percent, or measure units. Existing
  numeric suffix/currency prefix inventories determine recognized units.
- Korean written-scale currency endpoints reuse the Korean pack's decimal
  compound parser. Scaled and flat endpoints can mix: `1만원~20,000원`.
- Signs, grouped thousands, and bounded decimal exponents retain the scalar
  grammar's exact base-ten representation. No binary floating point is used.
- Signed endpoints are accepted. A `-` delimiter followed immediately by another
  sign is ambiguous and preserved: `1%--2%`. Use `1% - -2%` or `1%~-2%`.
- A range records `value`, `secondary`, `unit`, input byte extent and rule/layer
  provenance in the existing Semantic IR. Public ABI v1 is unchanged.

Mismatched units/types remain TEXT over the source interval, including
`1kg~2km`, `1%~2%p`, `$10~₩20`, and `1만원~2kg`. They are not converted or silently
assigned the second unit. Malformed numeric second endpoints and chained repeated
quantity expressions also have preservation guards. No interval ordering check
is performed; written descending endpoints retain their order.

## Resources and architecture

The generic syntax parser owns delimiter/endpoint recognition and unit identity.
The Korean pack constructs canonical exact quantities for written scale forms;
it reuses the builtin generic scalar classifier for a flat opposite endpoint.
The core contains no Korean scale spellings or currency-specific branches.

The default global delimiter inventory is shipped alongside the existing global
numeric suffix inventory in `resources/languages/en-US/rules/numeric.tsv`:

```text
quantity_range_separator  *  -  1
quantity_range_separator  *  ~  1
```

These rows are **tab separated** in resource files. `1` enables a spelling; `0`
disables it. Customer/domain layering applies. The language-specific reading
separator is a different resource: `policy / ko-KR / quantity_range_separator`.
For example, this customer resource adds `|` and renders Korean intervals with
`에서` without rebuilding C:

```python
from pathlib import Path
Path("customer.tsv").write_text(
    "quantity_range_separator\t*\t|\t1\n"
    "policy\tko-KR\tquantity_range_separator\t 에서 \n",
    encoding="utf-8",
)
```

```sh
./build/ttf normalize --customer customer.tsv '1만원|2만원'
# 만 에서 이만 원
```

Delimiter spellings must be compatible with token and sentence syntax; the
shipped contract uses the two single-character delimiters above. Existing bare
hyphen ranges, dates, identifiers and domain range policies retain their previous
behavior. Disabling a repeated-quantity delimiter does not disable the older
bare-number `-` grammar.

## Streaming

A quantity followed by whitespace can still acquire a range delimiter. The
segmenter conservatively retains that whitespace using bounded classification
of the preceding quantity and the same resource delimiter inventory. It does
not depend on whether future bytes arrived in the current write. This also
applies to very small phrase targets. Complete intervals remain one semantic
span across arbitrary input chunks. Ordinary following prose provides a later
safe boundary; a quantity at EOF commits on flush.

Keeping quantity tails pending may move a phrase callback later than its target.
The existing pending-byte cap still applies. The numeric lookback and first
Korean endpoint search are bounded to 512 input bytes; this is an initial grammar
bound, not a claim to cover arbitrary mathematical expressions.

## Verification and limits

`tests/evaluation/finance_ranges_v7.jsonl` contains 24 synthetic development
references, frozen before baseline or candidate execution. SHA-256:
`844d5ba7eab0d542eb017cbb6523a128718605702b603ca53893a273eec36e0a`.
The v0.6 baseline matched 0/24; the implementation matched 24/24, including explicit
unit/value/type/context assertions. These are development regressions, not
independently annotated customer data or a production accuracy estimate.

`tests/integration/test_finance_ranges_v7.py` additionally checks the exact second
endpoint, offsets, entire callback event equality across chunk sizes, tiny phrase
targets, invalid/mismatched intervals, chained forms and resource overrides.
Native exhaustive two-chunk tests cover representative intervals as well.

Still outside this iteration: inferred/missing first units, arbitrary counter
interval policies, general interval algebra, en/em dash variants, natural-language
`from ... to ...` grammar, unit/currency conversion, locale-specific decimal comma,
and currencies or units absent from resources. The unchanged scalar grammar also
sets limits on malformed/oversized numeric values.
