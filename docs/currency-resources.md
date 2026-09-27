# Currency identity, scales and reading resources

The currency grammar stores the exact decimal amount and currency code in
Semantic IR. The language pack chooses the reading. No exchange-rate conversion
is performed. These tables define a service reading policy; they do not validate
legal payment amounts, cash rounding or settlement precision.

The new records are in:

- `resources/languages/en-US/rules/currencies.tsv`: shared recognition aliases,
  English labels and English minor-unit reading policy.
- `resources/languages/ko-KR/rules/currencies.tsv`: additional Korean labels.

On 2026-09-26 a sweep of 23 symbols and 30 ISO codes found 17 symbols and 21
codes undeclared, so 24 currencies were added (CNY, INR, RUB, BRL, MXN, ZAR, NZD,
SGD, HKD, TWD, THB, VND, IDR, MYR, PHP, SEK, DKK, PLN, CZK, HUF, TRY, ILS, UAH,
NGN). Three decisions are worth repeating: `¥` maps to JPY, the convention for
that symbol, and the yuan is written `CN¥`; `¢` is a minor unit rather than a
currency, so it stays undeclared and is left unread; and the new set declares
`currency_minor_digits 0`, so a fractional amount reads in major units until a
customer overlay adds minor labels. A symbol carrying Latin letters (`HK$`,
`NZ$`) still chooses English context when it stands alone, which is the ordinary
context scoring, not a currency rule.

Existing USD, EUR, JPY and KRW records remain in `numeric.tsv` or
`measurements.tsv`. Keeping each key in one place avoids first-declaration
precedence silently hiding a later edit. Recognition records use language `*`,
so their meaning is shared by both packs.

## Currency identity

| IR code | Additional prefix | Uppercase suffix | English major-unit reading | Korean major-unit reading |
| --- | --- | --- | --- | --- |
| USD | Existing `$`, `US$` | `USD` | dollar / dollars | 달러 |
| EUR | Existing `€` | `EUR` | euro / euros | 유로 |
| AUD | `A$`, `AU$` | `AUD` | Australian dollar / Australian dollars | 호주 달러 |
| CAD | `C$`, `CA$` | `CAD` | Canadian dollar / Canadian dollars | 캐나다 달러 |
| GBP | `£` | `GBP` | pound / pounds | 파운드 |
| CHF | None | `CHF` | Swiss franc / Swiss francs | 스위스 프랑 |
| NOK | None | `NOK` | Norwegian krone / Norwegian kroner | 노르웨이 크로네 |
| JPY | None | `JPY` | yen | 엔 |
| KRW | Existing `₩` | `KRW` | won | 원 |

These are deliberate recognizer aliases. Plain ISO prefixes and the ambiguous
`¥` symbol are not added by this file. Bare `$` retains the existing USD policy;
a customer serving other dollar currencies should configure explicit aliases.
`A$` must retain AUD even when a benchmark reference abbreviates its spoken
result to “dollars.” A reference mismatch is preferable to changing the currency.

An ISO suffix is recognized as a whole quantity suffix, not as a prefix of a
longer identifier. A conflicting explicit prefix and suffix must not create a
different amount or currency. Inspect `semantic_type`, `value` and `unit` as well
as the output text when reviewing a new alias.

## Decimal scale aliases

`currency_scale` maps each spelling to a base-ten exponent:

| Exponent | Recognized spellings |
| --- | --- |
| 3 | `thousand`, `Thousand`, `THOUSAND`, `k`, `K` |
| 6 | `million`, `Million`, `MILLION`, `m`, `M`, `mn` |
| 9 | `billion`, `Billion`, `BILLION`, `b`, `B`, `bn` |
| 12 | `trillion`, `Trillion`, `TRILLION`, `t`, `T`, `tn` |

These spellings implement the contemporary English short-scale reading policy.
Matching is case-sensitive, with the listed variants declared explicitly. A scale
is applied within an explicit currency expression: `$1.25m` represents the exact
amount `1250000` with `unit=USD`. It does not reinterpret bare `3m`, whose existing
unit is meters. The decimal expansion is arithmetic on digits, without binary
floating-point rounding. A scale that exceeds the grammar's supported numeric
capacity must be preserved rather than truncated or partly applied.

Do not add customer aliases such as `MM` without first checking possible conflicts
with identifiers and units in the intended domain. Scale recognition belongs in
the TN resource table, separate from the foreign pronunciation lexicon.

## English fractional currency policy

`currency_minor_digits en-US CODE VALUE` has two supported service settings:

| Value | Rendering policy |
| --- | --- |
| `2` | Up to two fractional digits may be spoken as hundredths with the currency's minor label. Greater precision remains a decimal major-unit amount. |
| `0` | Preserve fractional input as a decimal major-unit amount; do not invent minor units. |

The defaults are `2` for USD, EUR, AUD, CAD, GBP, CHF and NOK, and `0` for JPY and
KRW. Thus a fractional yen or won value can still be read exactly without claiming
it is a valid settlement amount. No setting rounds or discards fractional digits.
The KO pack continues to read decimal currency values in major units.

New English minor labels are cent/cents for AUD and CAD, penny/pence for GBP,
centime/centimes for CHF, and `øre` for NOK. `øre` retains the name used in English
by Norges Bank; downstream TTS pronunciation of that word remains an engine or
pronunciation-resource concern. Minor-unit precision does not describe the set of
coins currently in circulation.

## Customer editing example

To make the USD nationality explicit without changing its semantic code, write a
customer overlay with actual tab separators:

```sh
python3 - <<'PY'
from pathlib import Path
Path('/tmp/currency-customer.tsv').write_text(
    'currency\ten-US\tUSD.one\tUS dollar\n'
    'currency\ten-US\tUSD.many\tUS dollars\n'
    'currency\tko-KR\tUSD\t미국 달러\n',
    encoding='utf-8',
)
PY

build/ttf normalize --resources resources --language en-US \
  --customer /tmp/currency-customer.tsv 'US$2'
build/ttf inspect --resources resources --language ko-KR \
  --customer /tmp/currency-customer.tsv '금액은 US$2입니다.'
```

Expected English text: `two US dollars`. Expected Korean text:
`금액은 이 미국 달러입니다.`. `--language` supplies a default; detected sentence
context still controls the output language.
The IR unit remains USD. Customer records override System records without a C
rebuild. Restart or recreate an engine after loading the changed resource, because
the frozen engine's resources are immutable.

Source TSV can also be compiled with the existing tool:

```sh
python3 tools/resource_compiler/compile_resources.py \
  /tmp/currency-customer.tsv /tmp/currency-customer.ttfb
```

Use `--customer /tmp/currency-customer.ttfb` in the same commands. Review singular,
plural, decimal, scale, range, conflicting-code and arbitrary-chunk cases before
deploying a reading change. See [the editing guide](resource-editing-guide.ko.md)
and [external benchmark workflow](../benchmark/README.md).

## Source verification

Checked on **2026-09-17**. The nine currency code/name associations and minor-unit
digits were checked against SIX, the ISO 4217 maintenance agency, and its
[List One XML](https://www.six-group.com/dam/download/financial-information/data-center/iso-currrency/lists/list-one.xml).
The fetched list declares publication date `2026-01-01` and SHA-256
`838dfb991648cf36df939edd5fe3811737962b75a32252847d239cedd1e291c9`.
Its currency names are US Dollar, Euro, Australian Dollar, Canadian Dollar, Pound
Sterling, Swiss Franc, Norwegian Krone, Yen and Won. SIX provides the authoritative
code-list context on its [financial data standards page](https://www.six-group.com/en/products-services/financial-information/market-reference-data/data-standards.html).

Additional naming references:

- The [Swiss National Bank coin information](https://www.snb.ch/en/the-snb/mandates-goals/cash/coins)
  uses the English term centime.
- The [Royal Mint decimalisation history](https://www.royalmint.com/collect/decimalisation/the-story-of-decimalisation/)
  describes the pound's decimal subdivision; penny/pence is the selected reading.
- [Norges Bank's English monetary policy report](https://www.norges-bank.no/en/news-events/publications/Monetary-Policy-Report/2026/mpr-22026/web-report-mpr-22026/)
  uses `Øre` for fractional currency prices.

The source XML and website text are not vendored. The small original runtime
tables contain selected code mappings and project-authored reading policies,
including Korean labels and scale aliases. This is not a redistributed ISO
standard or a complete licensed financial reference database. Recheck the primary
source when extending the supported currencies. Benchmark data licenses remain
separate; see [THIRD_PARTY.md](THIRD_PARTY.md).
