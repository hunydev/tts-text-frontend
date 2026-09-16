# Financial text contracts in v0.5

The frontend produces normalized text and semantic spans. This document describes
the implemented reading policies for financial inputs; it does not define account
validation, banking rules or financial calculations. Examples and test fixtures
are synthetic. Evaluation compares output text and metadata, without speech
synthesis or listening tests.

## Explicit account and reference labels

The Korean pack recognizes a digit string immediately after an enabled
`identifier_label` resource. System labels include `계좌는`, `계좌번호는`,
`계좌 번호는`, `참조번호는`, `참조 번호는`, `거래번호는` and `거래 번호는`,
plus forms without `는`. A colon is optional between the label and value.
Internal spaces/tabs in a label are normalized to one space for lookup; newlines
end the scope. Recognition starts at a tokenizer boundary, so `계좌는` does not
match inside `내계좌는`.

| Input | Normalized text |
| --- | --- |
| `입금 계좌는 110-123-456789입니다.` | `입금 계좌는 일일공 일이삼 사오육칠팔구입니다.` |
| `거래 참조 번호는 001204입니다.` | `거래 참조 번호는 공공일이공사입니다.` |
| `참조 번호: 0000` | `참조 번호: 공공공공` |
| `참조 번호는 2026-09-16입니다.` | `참조 번호는 이공이육 공구 일육입니다.` |

The value consists of ASCII digits, optionally divided by single hyphens into
nonempty groups. It occupies at most 95 bytes, including hyphens. The 96-byte IR
field reserves one byte for its terminator. Leading zeros and groups survive
classification unchanged:

```text
type       = DIGIT_SEQUENCE
value      = "110-123-456789"
secondary  = "계좌는"
unit       = "financial_identifier"
rule       = "ko:labeled-identifier"
source     = und
context    = ko-KR
```

The matching label's resource layer is recorded in the span. The most specific
complete label is selected by longest match; normal resource layering resolves
records with the same key. An explicit identifier classification takes precedence
over generic cardinal, phone, range or date syntax covering the same bytes. The
date-shaped example above therefore preserves every digit. There is no account
length, bank, institution or checksum validation.

System identifier pronunciation uses `공` for zero and one space for a hyphen.
Other digit-sequence policies retain their existing zero reading. Ordinary
quantities remain quantities: `계좌는 3개입니다.` produces `계좌는 세 개입니다.`,
and `계좌는 12,500원입니다.` produces `계좌는 만이천오백 원입니다.`.
The weak label `번호는` is deliberately absent. Unlabeled `001204` still receives
ordinary cardinal processing; this is not automatic account discovery.

### Resource customization

Load a customer TSV with `--customer customer-identifiers.tsv`, or load it through
`ttf_resource_load` before freezing the engine. This Python snippet writes the actual tab-separated records and preserves the
significant separator spaces:

```python
from pathlib import Path
Path("customer-identifiers.tsv").write_text(
    "identifier_label\tko-KR\t정산코드는\tdigits\n"
    "identifier_render\tko-KR\tzero\t영\n"
    "identifier_render\tko-KR\tseparator\t 번에 \n",
    encoding="utf-8",
)
```

This makes `정산코드는 001-204입니다.` produce
`정산코드는 영영일 번에 이영사입니다.`. The rendering settings apply to all
financial identifiers handled by that engine. A record such as
`identifier_label<TAB>ko-KR<TAB>계좌는<TAB>disabled` disables that exact system cue.
The literal spaces around the separator value are significant.

The segmenter builds a cue index when the engine freezes. It retains partial
multiword labels and the following identifier across phrase boundaries, including
very small segment targets. This preserves the batch/stream invariant for the
same configuration. Protected units may exceed a segment target; the pending
buffer limit still applies. Identifier cues currently support an ASCII colon,
not a general punctuation or markup grammar.

## Percent, percentage points and basis points

Resource suffixes give `%p` and `bp` distinct `MEASURE` units:

| Surface | Semantic IR | Korean text | English text |
| --- | --- | --- | --- |
| `0.25%p` | `MEASURE(value="0.25", unit="percentage_point")` | `영 점 이오 퍼센트포인트` | `zero point two five percentage points` |
| `25bp` | `MEASURE(value="25", unit="basis_point")` | `이십오 베이시스포인트` | `twenty five basis points` |
| `12%` | `PERCENT(value="12")` | `십이 퍼센트` | `twelve percent` |

Values are read as supplied; no conversion between these units is performed.
The system spellings `%p` and `bp` are case-sensitive resource keys. English unit
labels choose singular/plural forms using the semantic value.

## Decimal and signed ranges

The common classifier emits one `RANGE` span with the first endpoint in `value`,
the second in `secondary`, and an optional shared trailing `unit`. Examples:

```text
1.5-2.5%       → RANGE("1.5", "2.5", unit="percent")
              → 일 점 오 다시 이 점 오 퍼센트
-1.5 - -0.5%  → 마이너스 일 점 오 다시 마이너스 영 점 오 퍼센트
```

Use an ASCII hyphen as the separator. A signed second endpoint requires whitespace
after that separator: `1--2` remains invalid rather than becoming a signed range.
This grammar supports one shared trailing unit; it does not infer arbitrary
prose intervals, inequalities or repeated units on both endpoints.

The legacy `range_mode`/`range_separator` policies apply to bare unsigned integer
ranges. Thus `123-456` is `백이십삼 다시 사백오십육` by default and
`일이삼 사오육` with `resources/domains/finance/ko-KR/rules.tsv`. The example
customer overlay joins the digit groups with ` 번에 `.

Decimal, signed and unit-bearing ranges use quantity reading even when the
finance domain is loaded. Their separator is controlled separately by
`policy / ko-KR / quantity_range_separator`, whose system value is ` 다시 `.
This prevents the identifier-oriented legacy overlay from changing a rate range
such as `1.5-2.5%` into an identifier reading.

## Exact, bounded scientific notation

The common number grammar expands `e`/`E` notation using decimal digit arithmetic;
binary floating point is not involved. For example:

```text
표시 금액은 1.2e6원입니다.
→ 표시 금액은 백이십만 원입니다.
→ CURRENCY(value="1200000", unit="KRW")
```

The exponent magnitude is bounded by 96. The parsed mantissa and expanded value
must fit the existing 95-byte numeric string payload, including a sign and
decimal point. An exponent within the bound can still produce an oversized value
and be rejected. Expansion may remove insignificant decimal zeros; it preserves
the exact numeric value, not the notation's original formatting or precision
annotation.

Malformed or oversized numeric atoms are preserved as text, rather than rounded
or saturated. For example, `1e+` and `1e97` remain unchanged. This is a bounded
normalizer, not an arbitrary-precision calculation API.

## AM/PM text

The time grammar accepts resource suffixes `AM` and `PM`, requires an hour from
1 through 12 when a suffix is present, and stores the suffix in `TIME.secondary`.
The canonical time retains its twelve-hour value; no timezone or twenty-four-hour
conversion is implied. System resources render `AM` as `a m` in English and
`오전` in Korean, and `PM` as `p m` / `오후`.

```text
The webinar starts at 9:05 AM.
→ The webinar starts at nine oh five a m.
```

Only the configured suffix spellings are recognized; dotted or lowercase
variants are not automatically equivalent.

## Verification and remaining work

Focused tests cover identifier text and IR, leading zeros, group boundaries,
customer overlays, false positives, and default/tiny streaming targets.
Financial grammar tests also check semantic endpoints/units and exact exponent
expansion. Promoted scenarios live in `tests/evaluation/advanced_cases.jsonl`;
historical challenge references remain available for comparison.

The current scope is plain text with explicit reading policies. Production work
still includes customer-approved identifier vocabularies, broader real input
coverage, ambiguity review, locale-specific amount policies and operational
resource versioning. Synthetic passing cases do not establish production
accuracy. Selected code/markup/path forms now have a [literal-preservation
policy](protected-text.md); unrestricted financial expressions and general document
interpretation remain outside the contract.
