# Resources and deterministic priority

The runtime reads UTF-8 TSV with four nonempty fields per record:

```text
kind<TAB>language<TAB>key<TAB>value
```

`#` starts a comment only at the beginning of a line. Blank lines are ignored.
Values preserve spaces, including leading/trailing spaces used by separators.
Literal tabs and newlines are not supported within fields. `*` is a language
fallback. The root `manifest.ttfm` lists relative resource filenames, one per line,
in declaration order. Explicit domain/customer resource loading does not need a
new manifest or rebuild.

## Data kinds

| Kind | Example key | Purpose |
| --- | --- | --- |
| `exact` | `123-456` | Exact TN replacement, represented as CUSTOM IR |
| `pattern` | `ID{digits}` | Limited declarative digit-span TN pattern |
| `policy` | `range_mode`, `range_separator` | Pack reading choices |
| `lexicon` | `en-US:apple` | Target-language foreign pronunciation |
| `pronunciation` | `apple` | Source-language shared phone sequence |
| Pack-defined kinds | number/unit/context/grapheme/phone data | Reading and rendering tables |

TN exact/pattern rules are independent of pronunciation lexicons. A TN replacement
can change semantics; a foreign lexicon supplies how the same word is pronounced.

General resource precedence is Customer > Domain > Language System > Common.
User pronunciation overrides sit above Customer. For TN matching the comparison
is, in order:

1. Higher resource layer.
2. Exact over pattern within that layer.
3. Longer consumed input.
4. Greater literal specificity.
5. Earlier declaration.

Thus a customer pattern can outrank a domain exact rule; a customer exact match
outranks a customer pattern. Identical same-layer rows retain the first declaration.
Resource lookup prefers language-specific data over `*` at the same layer. Explicit
`ttf_lexicon_add` replaces an entry in its selected layer.

## Domain/customer example

Use actual tab delimiters; the following Python example preserves separator spaces:

```python
from pathlib import Path
Path("finance.tsv").write_text(
    "policy\tko-KR\trange_mode\tdigits\n"
    "policy\tko-KR\trange_separator\t \n", encoding="utf-8")
Path("customer.tsv").write_text(
    "policy\tko-KR\trange_mode\tdigits\n"
    "policy\tko-KR\trange_separator\t 번에 \n", encoding="utf-8")
```

System `123-456` is `백이십삼에서 사백오십육`; finance gives `일이삼 사오육`;
customer gives `일이삼 번에 사오육`. `ttf_resource_load` is transactional: malformed
input does not partially mutate the engine. Loads after freeze return `TTF_ESTATE`.

## Compiler/runtime boundary

```sh
python3 tools/resource_compiler/compile_resources.py finance.tsv finance.ttfb --ttf build/ttf --resources resources
```

Since v0.16 the compiler invokes the same C policy validators as runtime loading.
Build the CLI first or pass `--ttf`; `--syntax-only` is an explicit unvalidated
bootstrap option. `ttf validate-resource` reports file/line/key diagnostics, and
`--require-complete-validation` rejects unhandled namespaces. See the
[policy validation and editing workflow](resource-validation.md).

The initial compiled envelope contains `TTFB`, little-endian version 1, payload
length, FNV-1a integrity checksum, then validated UTF-8 TSV. The C runtime accepts
both source and compiled files with the same `ttf_resource_load` API. Invalid
version, length, checksum and malformed payload are rejected. The checksum detects
accidental corruption; it is not authentication.

This first binary format establishes the build/runtime boundary; it still parses
TSV at startup and is not a memory-mapped indexed lexicon. A future format can store
preindexed tries/FSTs without changing normalization or pack APIs. The existing
runtime hash lookup is isolated behind `ttf_resource_get` and pronunciation lookup.
Exact/pattern rule selection currently scans loaded candidate rules, so large rule
sets need indexing and profiling.

Resources are capped by the runtime loader and IR fields have fixed limits. Treat
resource syntax/version as an initial format until the first stable release.

## Resource-controlled recognition

`numeric_suffix` entries select semantic type/unit for attached or horizontally
spaced quantities. `ordinal_tail` constrains suffix spellings in the source pack
(e.g. English `11` → `th`, `1` → `st`, longest decimal tail first). The core knows
neither English ordinal exceptions nor foreign technical vocabulary.

`semantic_term` entries may span punctuation and script boundaries, for example:

```text
semantic_term<TAB>*<TAB>C++<TAB>ALNUM_TERM:en-US
semantic_term<TAB>*<TAB>USB-C<TAB>ALNUM_TERM:en-US
```

Use actual tab characters in a source resource. The value carries the semantic
class and source language. Matching requires the complete term, so `C++x` does
not match `C++`. Rendering remains in the pack and separate pronunciation lexicon;
this table contains recognition metadata only.

## Identifier and context cue resources (v0.5)

The KO pack recognizes `identifier_label ko-KR <label> digits`; `disabled` turns off
an inherited label. `identifier_render` controls `zero` and `separator`. Labels
may contain spaces (horizontal input whitespace is normalized). See
[financial text contracts](financial-text.md) for copyable customer examples.
`context_cue en-US <label> <0..1000>` adds an explicit title-context score; zero
turns it off. Neither kind belongs in the pronunciation lexicon.

On engine freeze the core indexes effective cue records for the shared boundary
planner. Customer/domain precedence applies before indexing. This avoids scanning
the complete pronunciation dictionary at every potential boundary. This compact
index and its links are engine-private; public C ABI v1 is unchanged.

## Literal preservation (v0.6)

`common/protected.tsv` declares `protection` switches for `markup`, `code` and
`path`, plus `protected_tag` names whose value `opaque` preserves complete
contents. These Common records follow the existing layer hierarchy; a customer
can disable a switch or change an element to `transparent` without core changes.
They remain separate from TN replacement rules and pronunciation lexicons. Active
preservation prevents rules from consuming or modifying a literal. See
[protected text contracts](protected-text.md) for runnable overlays.

## Human editing

See the [Korean resource editing guide](resource-editing-guide.ko.md) for range
connectors, counter policies, pronunciation data and reference review. Legacy
`range_mode=digits` is a broad hyphen customization example, not a recommended
financial default: use labeled identifier resources for actual account identifiers.


## Pronunciation authoring boundary (v0.10)

Managed system/finance pronunciation data are edited as `lexicons/` JSONL and
source catalogs. `tools/lexicon/manage.py build` validates and deterministically
emits the existing runtime TSV format; `build --check` is a CI gate. The existing
resource compiler can then compile those TSV files to TTFB. Neither authoring
metadata nor Python is required by the C runtime. TN rules remain separate TSV
resources. Domain pronunciation files are explicitly loaded and prohibited from
the default manifest by the authoring gate. See [maintenance](lexicon-maintenance.ko.md).

## Unicode grouping and Korean counter policy (v0.15)

[Unicode numeric grouping](unicode-numbers.md) defines editable grouping separators
and whole-object preservation. [Korean counter policy](korean-counters.md) documents
`번째`, count/identifier `번`, `제N`, context cues and customer examples. The resource
kind `native_ordinal_suffix` is separate from universal English `ordinal_suffix`.

`classifier_priority <language> <matched-rule> <integer>` adds specificity for
identical extents. Default is 0; only -1000..1000 is accepted. It does not beat a
longer candidate or the separate exact/pattern TN rule layer. This is a classifier
policy resource; it is not a pronunciation dictionary entry.

When adding count context, pair a new left cue with `boundary_word`. A particle
which may require right context also needs `quantity_lookahead_tail` so a short
phrase target cannot commit its number early. Match preservation, exact values,
IR secondary fields and byte-split invariance after any such change.

`counter_role_tail ko-KR <attached-tail> frequency|identifier|0` supplies a role
from the complete suffix immediately after `번` (system default: `씩`). An enabled
tail requires a `particle` or `quantity_tail` entry and a `quantity_lookahead_tail`
entry; freeze validates both dependencies. Identifier cues still win over count
cues. Customer `0` disables that tail without changing the other cues. The bounded
system cue expansion is in `languages/ko-KR/rules/counter-frequency.tsv`; see the
[counter guide](korean-counters.md) for ambiguous verbs and route names excluded
from the defaults.

## CJK Korean readings

The manifest lists `languages/ko-KR/cjk/{hanja_readings,kana_readings,cjk_words}.tsv`.
Creation skips these three system tables; freeze opens only the tables selected
by the final `hanja`/`kana` options. Both passthrough (including legacy callers)
opens none; either Korean mode loads the word table, plus its own reading table.
The same reserved basenames with `.bin` or `.ttfb` extensions support compiled
manifests. Other manifest paths retain their ordinary eager loading behavior.
Offline inventory, auditing and compilation still see all three paths.

Explicit domain/customer/user loads remain eager, even for disabled scripts.
At freeze, the selected system files use the existing SYSTEM-layer loader and
validators. Any load, final validation or index-allocation failure rolls back
all rows and declaration numbers added by that freeze, retaining explicit
overrides and leaving the engine mutable for a clean retry. Same-layer earlier
declarations still win; deferred system rows follow pre-freeze overrides.
`resource_root=NULL` uses the same resolved default root for both phases.

Resource validation before freeze reports only currently loaded rows. For the
shipped manifest without overlays (including the nine dotted-abbreviation rows), counts are:

| Options | Before freeze | After freeze |
|---|---:|---:|
| Both passthrough / legacy | 3,575 | 3,575 |
| Hanja Korean only | 3,575 | 11,696 |
| Kana Korean only | 3,575 | 3,881 |
| Both Korean | 3,575 | 11,997 |

Explicit overlays add their row counts in both phases. After freeze, validation
and public resource lookup include every loaded CJK row in the detached index.
Before freeze, deferred system values are absent from lookup; explicit values
remain available. Validate a system CJK file explicitly to check it offline
without enabling its script. Public loading after freeze still returns `TTF_ESTATE`.

These generated files are not edited by hand. Reproduce all three offline:

```sh
python3 tools/cjk/generate.py build --unihan data/cjk --out artifacts/cjk/build
python3 tools/cjk/generate.py build --unihan data/cjk --out resources/languages/ko-KR/cjk --check
```

The first command writes tables, provenance and coverage reports to ignored artifacts.
The second generates in a temporary directory and compares TSV bytes (including LF)
without writing into the resource directory. CI runs it in CTest. No automatic
external downloads occur. Inputs are the committed licensed Unihan subset,
`tools/cjk/kana_table.py` and `tools/cjk/word_table.py`; their scope is fixed by the
[CJK design](cjk-korean-reading.ko.md). Review generated differences before copying
TSV files into the resource tree; keep provenance/report JSON in artifacts.

For a customer-approved reading, author a separate UTF-8 TSV (actual tabs):

```tsv
cjk_word	ko-KR	張勳	장훈
hanja_reading	ko-KR	樂	악|락
```

Load it with `--customer FILE` or `ttf_resource_load` before freeze, and enable
`--hanja korean` (and/or `--kana korean`). Reading choices are deterministic: the
first `|` item is preferred; initial variants are derived at runtime. A `cjk_word`
key is one to six Han/Kana codepoints with no spaces; its output is Hangul. All
scripts in the key must be enabled. Literal annotated aliases take precedence.
For the C user pronunciation API use `source_lang="und", render_lang="ko-KR"`.
The workbench's existing user-lexicon form is English-only; use its customer TSV
field or API for CJK rows. The five system word rows are not a coverage collection.

Higher layer beats a longer word; within a layer the longest match wins, then
language specificity and earlier declaration. Single-character resource lookups
retain the existing `ttf_resource_get` precedence. Trace exposes the selected kind,
key, value, layer and declaration order. Misses preserve original characters;
only the optional final speech policy may remove them, preserving word separation.

Freeze detaches all six kinds (`hanja_reading`, `kana_reading`, `kana_mark`,
`kana_fold`, `kana_long_after`, `cjk_word`) into a heap index. The engine stores one
pointer; the owning buckets hold the original entries and the word buckets borrow
them. Common resource/rule lookups do not traverse these rows. Public lookups and
validation still include every detached row, with unchanged layer/order and value
lifetime. Destroy frees each owning row and the index once.

## Bare leading-zero policy and telephone tails (2026-09-28)

`resources/common/numeric-policy.tsv` declares `numeric_policy / * /
leading_zero_min_digits = 2` (integer 2–95; absent legacy root defaults to 7).
Only plain unsigned digit runs qualify after structured grammars. Korean
`number / leading_zero_zero = 공` controls their zero rendering independently
of phone/identifier/digit controls. Missing values retain the legacy digit table.

`phone_number_tail / ko-KR / 번 = 1` permits number-designating tails after an
established telephone shape; value `0` disables the exception. Keys are printable
UTF-8 without spaces, shorter than 96 bytes; values are exactly `0` or `1`.
The shared phone classifier and Korean pack validators preserve layer precedence.
No prefix allocation list is added. See [policy and overlay examples](phone-zero-policy.ko.md).

Row-count tables above describe their dated snapshots; newly added system rows
change absolute counts and engine-relative declaration ordinals. Verify selected
rows and same-layer relative order rather than treating historical counts as
the current manifest total.

## Korean numeral spacing (2026-09-28)

`number_spacing / ko-KR / group = space` separates nonzero myriad groups in the
cardinal verbalizer: `12500 → 만 이천오백`. Only key `group` and values `space`
or `compact` are valid. Customer/domain overlays keep the normal layer precedence;
an older resource root without this row defaults to `compact`.
This does not split tens/hundreds, phone digits, native numerals or literal aliases.
It applies when other grammars reuse the cardinal renderer, including currency,
measure and range quantities. See [policy, examples and reference audit](korean-spacing.ko.md).

`counter_noun_unit / ko-KR / 세|차|학년 = 1` declares bounded Korean counted
nouns, with the existing `counter` reading table. `counter_noun_tail` declares
exact attached grammatical endings; `counter_noun_attached / 차 = 1` requires
adjacency to the numeric field to avoid reading a vehicle identifier as a stage.
These three kinds take bounded keys under 32 bytes and boolean `0|1` flags.
Higher-layer zero disables the corresponding entry. This does not change the
universal `numeric_suffix` inventory or add broad Hangul prefix matches.
