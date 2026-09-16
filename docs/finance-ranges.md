# Numeric, quantity and time ranges (v0.9)

A separator between supported compatible endpoints is a semantic interval. It
is not read using the symbol's name. Korean defaults use `에서` and repeat the
unit at both endpoints. A standalone `~` uses the symbol resource `물결`.

| Input | Korean text | RANGE value / secondary / unit |
|---|---|---|
| `1~2` | `일에서 이` | `1 / 2 / (empty)` |
| `1~2개` | `한 개에서 두 개` | `1 / 2 / 개` |
| `1시~2시` | `한 시에서 두 시` | `1 / 2 / 시` |
| `09:00~10:30` | `아홉 시 영 분에서 열 시 삼십 분` | `09:00 / 10:30 / time` |
| `1~2만 원` | `만 원에서 이만 원` | `10000 / 20000 / KRW` |
| `1만원~2만원` | `만 원에서 이만 원` | `10000 / 20000 / KRW` |
| `1억 원 - 2억 원` | `일억 원에서 이억 원` | `100000000 / 200000000 / KRW` |
| `$10-$20` | `십 달러에서 이십 달러` | `10 / 20 / USD` |
| `1.5%~2.5%` | `일 점 오 퍼센트에서 이 점 오 퍼센트` | `1.5 / 2.5 / percent` |
| `25bp~50bp` | `이십오 베이시스포인트에서 오십 베이시스포인트` | `25 / 50 / basis_point` |
| `1kg~2kg` | `일 킬로그램에서 이 킬로그램` | `1 / 2 / kg` |

Number-unit spacing is an explicit project text policy. Existing Korean TIME
rendering retains zero minutes (`영 분`); this is not a claim that every natural
utterance should state zero minutes. Terminal particles in the input are retained,
for example `1시~2시에` ends in `두 시에`.

## Syntax and values

- Default ASCII delimiters are `-` and `~`; horizontal spaces around them are allowed.
- Bare numbers, a shared trailing unit, and repeated matching units are recognized.
- A single shared Korean scale is inherited: `1.5~2.5만원` has endpoints 15000 and
  25000. `1~2천만원` inherits the product 천만. Additive mixed scales such as
  `1~2억3천만원` are ambiguous for this grammar and are preserved as TEXT.
  Shared scientific-coefficient scale expressions such as `1~2e3만원` are also
  preserved; partial conversion must not change the first endpoint meaning.
- Flat/scaled explicit currency endpoints can mix: `1만원~20,000원`.
- Native-counter policy applies independently to both endpoints (`한`, `두`, `세`).
- TIME ranges hold canonical `HH:MM[:SS]` strings in value/secondary, unit `time`.
  Meridiem-bearing interval syntax is not supported by this initial contract.
- Grouping, signed decimals and bounded scientific notation use exact decimal
  strings. No currency conversion, endpoint sorting, or floating point is involved.
- A hyphen immediately followed by another sign is guarded as ambiguous. Use
  `1% - -2%` or `1%~-2%`, not `1%--2%`.
- Repeated mismatched units (`1kg~2km`, `1%~2%p`, `$10~₩20`) and chained intervals
  remain TEXT; they do not inherit an incompatible unit.

A range is one Semantic IR span. Public ABI v1 is unchanged. Generic grammar has
no Korean scale spellings; the Korean pack constructs exact scaled endpoints.

## Resource editing

See the [Korean editing guide](resource-editing-guide.ko.md). All columns below
use real TAB delimiters; spaces in the final column are meaningful.

```python
from pathlib import Path
Path('customer.tsv').write_text(
    'policy\tko-KR\tquantity_range_separator\t부터 \n'
    'policy\tko-KR\tquantity_range_suffix\t까지\n', encoding='utf-8')
```

```sh
build/ttf normalize --customer customer.tsv '1~2만원'
# 만 원부터 이만 원까지
```

`quantity_range_repeat_unit=1` is the default. The generic delimiter inventory
`quantity_range_separator / * / ~ / 1` enables syntax; this is distinct from
`policy / ko-KR / quantity_range_separator / 에서 `, which chooses spoken text.
Disabling a delimiter does not erase the symbol: standalone Korean `~` reads
`물결` through its symbol resource. Protected code/path spans retain literal text.

Bare hyphens can be identifier separators depending on context. The historical
finance/customer `range_mode=digits` examples remain explicit broad overrides;
they are not recommended financial defaults. Use label-based identifier rules
for accounts/contracts. Explicit tilde ranges retain quantity readings even with
that legacy overlay.

## Streaming and limits

The segmenter shares the tokenizer/classifier and retains unfinished numeric
units, scales and interval delimiters. Tiny phrase targets do not split a
supported range's meaning. Tests compare text, endpoints, metadata and callback
sequences across arbitrary byte chunks. A pending limit still applies.

The grammar remains bounded (512-byte numeric lookback/compound search). It does
not cover all mathematical expressions, inequalities, prose `from ... to ...`,
Unicode dash/wave variants, meridiem inheritance or locale decimal commas.

## Corrected references

The v0.7/v0.8 range references mistakenly used `다시`. They were corrected in
v0.9 alongside their developer-authored expected text; prior pass percentages measured
agreement with those flawed references. They do not establish linguistic quality.
See the [change ledger](audit/reference-corrections-0.9.0.json) and
[reference inventory](../tests/evaluation/reference_reviews.json). No synthesized
audio or independently human-approved production accuracy is claimed.
