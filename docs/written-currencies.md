# Written currency names

Written names are quantity syntax backed by resources. They use the existing
exact decimal currency representation and KO/EN verbalizers. They are separate
from pronunciation lexicons: `12 US dollars` is an amount, while an isolated
word such as `dollars` remains ordinary lexical text.

## Supported forms

| Written input | English text | Semantic identity |
| --- | --- | --- |
| `12 dollars` | `twelve dollars` | `CURRENCY`, value `12`, unit `USD` |
| `1 dollar` | `one dollar` | `CURRENCY`, value `1`, unit `USD` |
| `2 euros` | `two euros` | `CURRENCY`, value `2`, unit `EUR` |
| `3 British pounds` | `three pounds` | `CURRENCY`, value `3`, unit `GBP` |
| `2.5 million dollars` | `two point five million dollars` | `CURRENCY`, value `2500000`, secondary `6`, unit `USD` |
| `2 ~ 3 Australian dollars` | `two to three Australian dollars` | `RANGE`, value `2`, secondary `3`, unit `AUD` |

In Korean context, `금액은 12 US dollars입니다.` becomes
`금액은 십이 달러입니다.`. The quantity retains `source_lang=en-US`; routing
still chooses the context and rendering language independently. Written-name
ranges retain their recognized source language too.

The inventory lives in
[`currency-names.tsv`](../resources/languages/en-US/rules/currency-names.tsv):

- `dollar(s)`, `US dollar(s)`, `United States dollar(s)`, `American dollar(s)` → USD
- `euro(s)` → EUR
- `Australian dollar(s)` → AUD; `Canadian dollar(s)` → CAD
- `British pound(s)`, `pound(s) sterling` → GBP
- `Swiss franc(s)` → CHF; `Norwegian krone/kroner` → NOK
- `yen`, `Japanese yen` → JPY
- `Korean won`, `South Korean won` → KRW

Bare `dollar(s)` follows the existing service policy for `$`: USD. This is a
configurable policy, not a claim that every occurrence of “dollar” worldwide is
USD. Bare `won` is excluded because it is also an English verb.

## Matching and precedence

A `currency_name` value is `ISO_CODE:source-language-tag`. Keys use lowercase
ASCII words with one space between them. Matching folds ASCII letter case and
runs of spaces/tabs, so `12 UNITED\tSTATES dollars` and `12 United States dollars`
have the same identity. At least one space or tab must separate the number or
scale from the written name. Matching does not cross a line break. An attached
Korean suffix is allowed: `12 dollars입니다`.

The longest complete declared name wins. Resource layer precedence applies to
identical `(kind, language, key)` records: Customer > Domain > System > Common;
the established first-declaration tie rule remains in effect. Written names
are checked before generic adjacent `numeric_suffix` recognition. Override a
written alias with `currency_name`, rather than adding an unrelated
`numeric_suffix` record for the same spelling.

A prefix and written suffix must agree. `US$2 dollars` is one USD amount.
`£2 dollars`, `$2 British pounds`, and `€2 million dollars` are preserved as a
single `TEXT` object. A range may share a suffix or scale, but its explicit
currencies must agree. Mixed `2 Canadian dollars ~ 3 US dollars` is preserved;
no currency conversion is inferred.

The parser never converts decimal input through binary floating point. Scale
expansion shares the bounded decimal arithmetic used for symbolic currencies.
The existing render policies choose major/minor units and labels; see
[currency resources](currency-resources.md).

## Ambiguous pounds

A bare `3 pounds` does not establish whether the quantity is money or mass. It
therefore remains a cardinal plus an ordinary word. It can still produce
`three pounds`; inspect the semantic metadata when testing the distinction.

The resource flag `currency_name_ambiguous` requires stronger evidence for
`pound` and `pounds`:

1. An explicit currency prefix, such as `£3 pounds`, establishes money. A
   conflicting prefix still causes whole-object preservation.
2. An immediately preceding monetary cue declared by `currency_name_cue`, such
   as `paid`, `costs`, or `spent`, can establish GBP: `I paid 3 pounds.`.
3. A following mass complement declared by `currency_name_mass_tail`, such as
   `of` or `weight`, vetoes the weak cue: `It costs 3 pounds of apples.` does
   **not** receive a `CURRENCY` span.

These are deterministic local rules. They do not perform general English word
sense disambiguation. A qualified spelling such as `British pounds` or `pounds
sterling` supplies its identity directly. The rule does not infer currency from
a later endpoint to reinterpret an otherwise ambiguous earlier `pounds` phrase.

The ambiguity itself is supported by the publisher's separate money and weight
entries in [Cambridge Dictionary: pound](https://dictionary.cambridge.org/dictionary/english/pound),
checked 2026-09-17. Only this lexical fact informed the guard design. No definition,
example sentence, pronunciation table or third-party dataset was copied into the
runtime resource or test corpus. The publisher's dictionary text is copyrighted;
this citation is not a data redistribution license.

## Editing customer policy

Create a TSV file with actual tabs. For example, choose AUD for an unqualified
plural dollar and add a customer-specific CAD alias:

```sh
python3 - <<'PY'
from pathlib import Path
Path('/tmp/currency-policy.tsv').write_text(
    'currency_name\t*\tdollars\tAUD:en-US\n'
    'currency_name\t*\tcustom dollars\tCAD:en-US\n'
    'boundary_word\t*\tcustom\t1\n',
    encoding='utf-8',
)
PY
./build/ttf inspect --resources resources --language en-US \
  --customer /tmp/currency-policy.tsv '2 custom dollars'
```

The result is `two Canadian dollars` with `unit=CAD` and
`resource_layer=customer`. Apply a corresponding override for the singular
`dollar` if both singular and plural should follow the same customer policy.
Use the exact source language tag of a registered source pack.

Every nonfinal word in a new multiword alias needs a `boundary_word` continuation
hint, so tiny streaming phrase targets cannot split the quantity early. The
bundled table includes these hints for qualifiers, monetary cues and the
`pounds sterling` spelling. A custom word alias is not automatically a new
currency: its ISO code must have labels/policies in each target language pack.

After changing an alias, inspect value, unit, source, context and layer; verify
both intended matches and nearby nonmatches. Run the independent currency checks:

```sh
python3 tests/integration/test_written_currency.py ./build/ttf resources \
  --library ./build/libtts_text_frontend.so
```

## Guards and tested scope

The original test cases cover singular/plural names, case and tab folding,
qualified currencies, signed fractional amounts, scaled amounts, repeated and
shared-unit ranges, Korean attachments, resource overrides and ambiguous mass.
They run chunk sizes 1/7/64 under normal and tiny segmentation settings and every
two-way UTF-8 byte split through the native library. Expected text is authored
independently; the tests also assert exact value and currency identity.

- `12dollars` is not a spaced currency name.
- `12 dollars100`, `12 dollars_foo`, `12 dollarsé` and `12 dollars€` must not
  become currency spans.
- A recognizable malformed written suffix after an explicit prefix stays whole:
  `£2 pounds_foo` and `$2 dollars100` remain text, avoiding a duplicated currency
  from prefix-only normalization.
- `12,34 dollars`, mixed-currency ranges and unsupported repeated scale chains
  do not produce a valid monetary interpretation.
- Newline-separated names and text protected as code remain outside this grammar.

The inventory does not yet parse spelled-out numeric coefficients, dotted `U.S.`
qualifiers, free-standing minor units such as `50 cents`, or arbitrary currency
names outside the declared codes. Local monetary cues are a finite policy and
require independent corpus review for new domains. These limits remain visible
instead of inferring unsupported financial meanings.
