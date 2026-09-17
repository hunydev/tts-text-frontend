# Scaled currency normalization

The v0.12 currency contract keeps the monetary amount and currency identity in
Semantic IR. The output language chooses a reading of that amount. Arithmetic is
performed with decimal strings; parsing does not pass through binary floating
point and does not round the input.

## Examples and scope

| Input | Context | Output |
| --- | --- | --- |
| `$2.75million` | en-US | `two point seven five million dollars` |
| `예산은 $2.75million입니다.` | ko-KR | `예산은 이백칠십오만 달러입니다.` |
| `$2.7bn` | en-US | `two point seven billion dollars` |
| `2M USD` | en-US | `two million dollars` |
| `A$3M` | en-US | `three million Australian dollars` |
| `$2M~$3M` | en-US | `two million to three million dollars` |
| `예산은 $2M~$3M입니다.` | ko-KR | `예산은 이백만 달러에서 삼백만 달러입니다.` |
| `1.25JPY` | en-US | `one point two five yen` |
| `£1.01` | en-US | `one pound and one penny` |
| `길이는 2m입니다.` | ko-KR | `길이는 이 미터입니다.` |
| `표기는 US$2MEUR입니다.` | ko-KR | `표기는 US$2MEUR입니다.` |

A scale has monetary meaning only when an explicit currency prefix or suffix
establishes currency identity. A bare `m` remains the measurement unit where
that grammar applies. Conflicting explicit currencies are preserved, rather
than silently changing the monetary unit or discarding an identifier.

Resource entries enumerate supported spellings. For example, `M`, `m`, `mn`,
`million`, `Million`, and `MILLION` are separate entries. This does not imply
unrestricted case folding, recognition of every financial abbreviation, or
currency conversion. The project retains its existing `$` → USD policy;
`A$` and `C$` identify AUD and CAD explicitly.

## Semantic contract

For `$2.75million`:

```text
type      = CURRENCY
value     = 2750000
secondary = 6
unit      = USD
```

`value` is the expanded exact decimal amount. For an individual scaled currency,
`secondary` records the decimal exponent of the recognized written scale.
English reconstructs the coefficient and renders the named scale; Korean reads
the expanded value in its own number system. Leading integral zeros and trailing
fractional zeros in the reconstructed coefficient are insignificant and removed.
Signs are retained. An English scale phrase uses a plural currency noun, even
when a tiny coefficient expands to exactly one monetary unit:
`$0.000001 million` becomes `zero point zero zero zero zero zero one million dollars`.
Ordinary unscaled currency readings retain their major/minor singular rules.

For `$2M~$3M`, the type is `RANGE`, `value` is `2000000`, `secondary` is
`3000000`, and `unit` is `USD`. `secondary` is the second endpoint for ranges;
it is never interpreted as a scale hint in that semantic class. Both endpoints
retain their monetary value. Korean repeats the currency at both endpoints and
uses `에서`; English uses `to` and a shared final currency label.
If only one English endpoint reaches a thousand, the currency labels are
repeated to make their scopes clear: `$2~$3 million` becomes
`two dollars to three million dollars`, preserving the explicit two-dollar
left endpoint. No scale is inferred across an explicit second currency prefix.

The English renderer accepts named scale exponents 3, 6, 9, 12, 15, and 18.
The initial resource aliases cover thousand, million, billion, and trillion.
Larger named scales require explicit resource aliases and the corresponding
number labels. The fixed-size IR still bounds representable input length.

### Shared trailing scales and signs

`$2~3 million` and `2~3 million USD` share their trailing scale: the endpoints
are exactly `2000000` and `3000000` USD. In contrast, the two explicit currency
prefixes in `$2~$3 million` establish separate endpoint amounts (`2`, `3000000`).
`$2million~3`, with only a leading scale and no second currency, is preserved as
an ambiguous expression. A range with conflicting explicit currency identities
is also preserved; the engine never performs an exchange conversion.

An adjacent sign before a prefix belongs to the amount: `-US$2M` records
`-2000000`, and `-$2만` records `-20000` USD. Duplicated signs such as `-$-2M`
or `-$-2만` are preserved as invalid text. The existing Korean compound-number
policy removes an explicit positive sign; English scaled amounts retain it.

After whitespace, a scale alias must cover a complete marker or end immediately
before an explicit currency suffix. Ordinary following words such as `to`,
`monthly`, `boxes`, and `total` are not scale abbreviations. Attached malformed
tails such as `$2M+3` and `$1million_foo` are preserved as complete text. These
boundary guards are tested with input-span metadata as well as output strings.

## Editable resource policy

See [currency resource sources and policy](currency-resources.md) and the
[Korean resource editing guide](resource-editing-guide.ko.md). The relevant
editable files are:

- `resources/languages/en-US/rules/currencies.tsv`: shared recognition aliases,
  English labels, and English minor-unit reading policy.
- `resources/languages/ko-KR/rules/currencies.tsv`: Korean currency labels.
- Existing `numeric.tsv` and `measurements.tsv` retain previously registered
  currency and measurement entries.

These are tab-separated fields, not spaces:

```text
currency_scale	*	million	6
numeric_suffix	*	USD	CURRENCY:USD
currency_minor_digits	en-US	JPY	0
currency_minor_digits	en-US	USD	2
```

For English, `currency_minor_digits=2` enables a major/minor reading for one or
two fractional digits. More precise decimals remain decimal major-unit readings.
`0` always uses the exact major-unit decimal reading and does not invent cents.
Missing policy retains the legacy value of `2`; other values are rejected as
invalid resources. This is a **text reading policy**, not payment validation or
rounding. Fractional JPY/KRW inputs remain exact even if an application would not
accept that precision for a payment.

## Web and repeatable verification

In the web workbench, select **금융 금액 배수·통화·범위**. Its registered key is
`scaled-currency-v12`. The four opening examples are also in the sample catalog.
The corpus contains sixteen original English/Korean inputs with expected text,
semantic class, expanded value, unit, and context language assertions.

```sh
build/ttf inspect --language en-US 'The budget is $2.75million.'
python3 tools/evaluation/evaluate.py \
  --ttf build/ttf --resources resources \
  --corpus tests/evaluation/scaled_currency_v12.jsonl \
  --output-dir benchmark/generated/scaled-currency-v12
```

The native regression tests additionally check the `secondary` field and
arbitrary input chunk boundaries. The web corpus checker currently asserts the
supported span fields, including `value` and `unit`; it does not independently
assert `secondary`.

These references were authored for the documented contract, independently of
external dataset text. They remain **pending-independent-review** in
`tests/evaluation/reference_reviews.json`. Passing verifies implementation
agreement with those explicit policies; it does not provide independent human
linguistic approval or demonstrate general financial-text coverage. Evaluation
compares text and metadata only.

## Known coverage gaps

This grammar recognizes declared prefixes and ISO/Korean suffixes. Spelled-out
English currency names are not yet consistent quantity suffixes: an input such
as `$2 million dollars` currently repeats the word `dollars`, and `$2m euros`
does not detect the conflicting written-out currency. Treat those forms as
unsupported for deployment. Adding global word aliases would need context rules
for ambiguous `pounds`, `won`, and generic `dollars` used with AUD/CAD. This
limitation also exists for ordinary unscaled prefix-plus-name inputs.

The [external measurement](research/scaled-currency-v12-results.md) records the
supported improvements and the much broader remaining TN gaps.
