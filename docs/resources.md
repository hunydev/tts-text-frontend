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

System `123-456` is `백이십삼 다시 사백오십육`; finance gives `일이삼 사오육`;
customer gives `일이삼 번에 사오육`. `ttf_resource_load` is transactional: malformed
input does not partially mutate the engine. Loads after freeze return `TTF_ESTATE`.

## Compiler/runtime boundary

```sh
python3 tools/resource_compiler/compile_resources.py finance.tsv finance.ttfb
```

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
