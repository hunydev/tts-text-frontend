# Plain and annotated input

Input format is an explicit per-engine option. **Plain input does not interpret
pronunciation controls or escapes.** Select annotated input when the application
intentionally supplies the grammar described here. The existing ABI v1 structures
and the original `ttf_engine_create` behavior remain available.

Include `tts_text_frontend/ttf_frontend_options.h`, initialize
`ttf_frontend_options_t`, set `input_format = TTF_INPUT_ANNOTATED`, and apply it with
`ttf_engine_set_frontend_options` before freezing the engine. Set `output_mode` and
`unreadable_policy` explicitly for the application: they are separate from input
syntax. Input and all offsets use UTF-8 bytes.

## Choosing output settings

Input syntax and output purpose are independent:

| Setting | Meaning |
|---|---|
| `input_format=plain` | Ordinary text; braces and backslashes do not request overrides |
| `input_format=annotated` | Enable the strict control grammar below |
| `output_mode=text` | Existing text normalization, preserving electronic address spelling |
| `output_mode=speech` | Also render electronic addresses and configured characters for speech |
| `unreadable=preserve` | Keep unsupported characters in speech output |
| `unreadable=drop` | Remove unsupported characters from speech output |
| `brackets=read` | Read a bracketed aside (default) |
| `brackets=skip` | Leave a bracketed aside out of the reading |
| `brackets=preserve` | Keep a bracketed aside exactly as written |
| `unknown_foreign=guess` | Guess a target-language pronunciation for an undeclared Latin word (default) |
| `unknown_foreign=spell` | Spell such a word letter by letter |
| `unknown_foreign=preserve` | Keep such a word as written |

For example, Korean speech output reads `a1@b2.com` as
`에이일 앳 비이 닷 컴`; text output preserves `a1@b2.com`. Speech mode does
not synthesize audio. The unreadable policy is used only by speech output;
protected code and explicit literal aliases retain their supplied text.

The web workbench starts with **plain / speech / preserve**, control limit 4096
bytes. Select **발음 제어 표기 사용** before trying the alias and pattern examples.
The example selector sets the necessary options, and **청크 검증** compares batch
output, spans, trace and callback boundaries across several input divisions.
The **읽기 제어·발화용 출력** corpus has 51 independently authored test references;
these are developer policy expectations pending independent linguistic review.
Historical corpora explicitly use text mode unless a row supplies its own options.

The original C engine constructor retains plain/text/preserve behavior. Calling
`ttf_frontend_options_init` selects plain/speech/preserve; set all three fields
explicitly when porting an application. Web requests may set `max_control_bytes`
between 1 and 16384, within the workbench's 16384-byte input cap.

## Literal pronunciation controls

An immediately adjacent brace payload replaces one source atom with the literal
text inside the braces:

| Annotated input | Literal replacement and remaining text |
|---|---|
| `1{첫}번째` | `첫번째` |
| `K7{케이세븐}` | `케이세븐` |
| `신조어{지정 발음}` | `지정 발음` |
| `ABC{우리 상점}마트` | `우리 상점마트` |

Spaces in the payload are significant. The control does not insert a space before
an attached suffix. Literal payloads are pronunciation overrides, not translation
instructions. They are emitted once; an alias containing decoded braces is never
interpreted as another control. Output policies remain a separate stage.

An atom is one Hangul/Other Unicode script word, or adjacent Latin letters and
digits such as `K7`. Script changes between Hangul and Latin/digits delimit atoms.
For numeric atoms, an optional leading ASCII sign and dots or commas between
ASCII digits stay in the source atom. A pattern renderer, not this scanner,
decides whether the resulting numeric spelling is valid. A hyphenated word,
punctuation-containing name or multiple words requires the grouped form below.
Whitespace between an atom and `{` does not bind them into a control.

### Explicit source groups

Use `[[source phrase]]{literal pronunciation}` to replace an explicit group:

```text
[[Bank of America]]{뱅크 오브 아메리카}
[[a hyphenated-name]]{지정한 이름}
```

The source group may contain spaces and punctuation. `[[` and `]]` are delimiters,
and `{` must immediately follow the closing `]]`. Empty or ASCII-whitespace-only
groups are invalid. Groups do not nest. Escape literal brackets or braces in a
source group. Grouped controls currently accept literal payloads only; a grouped
`{@pattern}` returns `TTF_EINVAL`.

## Language-pack patterns

An unescaped `@` at the beginning of a payload selects a named pattern:

```text
1{@native}
123{@sino}
ABC{@alphabet}마트
```

The parser accepts a lowercase ASCII name beginning with a letter, followed by
letters, digits, `_` or `-`, up to 30 name characters. It does not contain a list
of languages or pattern meanings. The selected language pack receives
`CUSTOM` Semantic IR with the original atom in `value` and `@name` in `unit`.
An unsupported pattern or an invalid source value returns `TTF_EINVAL`.

The initial Korean pack supplies these bounded policies:

| Pattern | Source contract | Reading policy |
|---|---|---|
| `@native` | One or two ASCII digits, value 0–99 | Standalone native Korean number forms; zero uses `영` |
| `@sino` | Signed or unsigned decimal, without grouping or exponent syntax | Sino-Korean numbers |
| `@alphabet` | ASCII letters and digits | Per-character names |

Pattern values must fit the 95-byte Semantic IR value field. For example,
`1.5{@sino}` is one decimal source; `1,000{@sino}` remains one source but is
rejected by the initial pattern policy. Literal controls do not have this IR-value
restriction; their total control length still has the configured limit below.

## Escaping and protected text

Annotated input recognizes exactly these escapes, both outside controls and in
literal payloads or source groups:

| Written bytes | Literal output |
|---|---|
| `\\` | `\` |
| `\{` | `{` |
| `\}` | `}` |
| `\[` | `[` |
| `\]` | `]` |

For example, `A{\{이름\}}` supplies the literal alias `{이름}`. An unknown escape,
such as `\q`, is invalid. A payload beginning with `@` is reserved for a pattern;
there is no `\@` escape. Unescaped stray braces, empty payloads and nested raw
braces are invalid. Ordinary single brackets are text; `[[` starts an explicit
group and must complete the grouped-control grammar.

Existing protected code, markup and paths take precedence over control parsing.
For example, a protected backtick span containing `1{@native}` remains literal.
An unprotected prose body inside ordinary markup can still contain controls.
Protection follows the existing resource-configured lexical grammar; it is not
a general HTML, Markdown or filesystem parser. See [protected text](protected-text.md).

## Streaming, limits and metadata

Batch and streaming use the same parser. A source atom at a write boundary waits
for the following bytes; a complete control is one indivisible region. A write
may split a UTF-8 character, delimiter or escape. An unfinished payload or explicit
group remains pending until its closing syntax arrives. `flush` on unfinished
syntax returns `TTF_EINVAL`. Invalid UTF-8 uses the existing `TTF_EUTF8` status.

`max_control_bytes` defaults to 4096 and counts the entire original control,
including its source and delimiters. Exceeding it returns `TTF_ELIMIT` during
`write`, even while waiting for a closing delimiter or the rest of a UTF-8 character. The existing
`max_pending_bytes` limit also applies to the stream. These are bounds, not silent
truncation. A malformed control cannot expose its directive text as ordinary
pronunciation. If a later unit fails, segments already delivered by earlier
callbacks remain committed; the failing unit is not delivered.

Control payloads do not vote in language/context detection. The complete control
is a neutral internal region; source-language metadata can be derived from the
source atom while surrounding prose supplies speech context. Group contents are
likewise excluded from neighboring context votes. Consequently a directive alone
uses the configured default context when no other evidence is present.

Metadata input offsets cover the original source and control syntax. The internal
parser separately retains source and payload ranges; output offsets cover the
rendered replacement. Applications should preserve the original input alongside
these offsets. A literal alias is not re-tokenized to manufacture additional
source spans.

## Bracket policy

`bracket_pair` in the common resources declares the pairs this policy covers:
`()`, `[]`, `{}`, `（）`, `〈〉` and `《》`. Quotation marks are a separate scope and are
not affected. `skip` removes the whole aside, including its brackets, and keeps one
separating space; `preserve` copies the source bytes without normalizing them. An
unclosed bracket has no region, so the text is read as before. The policy is set
with `ttf_engine_set_option(engine, "brackets", "read|skip|preserve")` before the
engine is frozen, by the CLI flag `--brackets`, or by the workbench selector. Named
options are functions rather than new structure fields, so a consumer built against
an older header keeps its layout and its defaults.

## Unknown foreign words

A Latin word reaches the target language through the pronunciation lexicon, then
the source pronunciation dictionary, and otherwise through a guess. `spell` and
`preserve` replace that last step only: a declared reading, an initialism
(`R&D`), an alphanumeric label (`k7`), an address and an identifier keep their own
contracts. `preserve` also leaves the word out of the final character pass, so the
source spelling survives in speech output. Customers who prefer a word reading add
the term to a lexicon layer instead of changing this policy.
