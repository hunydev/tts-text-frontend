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
| `unreadable=read` | Say every segment: an object the engine refused to interpret (`20F`, `11st`) is spelled by the final gate, a character with no name is removed, punctuation stays |

Two options can ask for opposite things. `unknown_foreign=preserve` keeps an
unregistered foreign word exactly as written, which is what a caller does when a
bilingual voice will read it, so that word stays Latin even under
`unreadable=read`; `read` still says everything else in the segment. The pairing
is pinned by contract and walked by the option-matrix gate.
| `brackets=read` | Read a bracketed aside (default) |
| `brackets=skip` | Leave a bracketed aside out of the reading |
| `brackets=preserve` | Keep a bracketed aside exactly as written |
| `unknown_foreign=guess` | Guess a target-language pronunciation for an undeclared Latin word (default) |
| `unknown_foreign=spell` | Spell such a word letter by letter |
| `unknown_foreign=preserve` | Keep such a word as written |
| `decorative=follow` | Stars, hearts, arrows and emoji follow the unreadable policy (default) |
| `decorative=read` | Speak the declared name of such a character |
| `initialism=spell` | An all-uppercase run of two or more letters spells its letters (default) |
| `initialism=word` | Such a run keeps its ordinary word reading |

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
source group. Grouped controls accept both literal payloads and named patterns. For example,
`[[서울시 12-3]]{@address}`, `[[a1@b2.com]]{@email}` and `[[q1/a2]]{@url}`
apply a reading to the entire group. This extends the existing grouped syntax;
unmarked whitespace or punctuation never enlarges the atom before a control.

## Break controls (v0.23)

In annotated input, `X{@break:1}`, `X{@break:2}` and `X{@break:3}` read the atom or
`[[group]]` X immediately before the marker **one character at a time**, with a
separator between the characters:

| Input | Output (ko-KR) | Output (en-US) |
|---|---|---|
| `1234{@break:1}` | `일 이 삼 사` | `one two three four` |
| `1234{@break:2}` | `일, 이, 삼, 사` | `one, two, three, four` |
| `1234{@break:3}` | `일. 이. 삼. 사.` | `one. two. three. four.` |
| `K7{@break:2}` | `케이, 칠` | |
| `[[12-34]]{@break:1}` | `일 이 삼 사` | |
| `번호는 1234{@break:2}입니다.` | `번호는 일, 이, 삼, 사입니다.` | |

Strength 1 separates with one space, 2 with a comma and a space, 3 with a period
and a space and also ends with a period. Digits read as `{@digits}` reads them
(Korean zero 영) and Latin letters by their names, both through the render pack;
other characters (Hangul, Han, ...) are kept as written; spaces and ASCII
punctuation inside a group are dropped. Text around the control is untouched,
so a suffix written against the marker attaches to the last item.

The marker binds exactly like other attached controls: an atom is a Hangul/Other
script word or adjacent Latin letters and digits; use `[[...]]` for anything else.
A marker with nothing before it (`{@break:1}`, `앞 {@break:1}`), a group with no
letter or digit, and strengths other than exactly 1, 2 or 3 are `TTF_EINVAL`.
`max_control_bytes` covers the atom and the marker. The directive works in text
and speech output and in every language pack; plain input and protected code do
not interpret it. Inspect reports one `CUSTOM` span with `rule=control.pattern`,
`unit=@break:N` covering the atom and marker; the trace records `BREAK` and the
strength. It does not force a stream callback: the separators are output text,
not an audio duration or a guaranteed pause length of a downstream voice.

This replaces the v0.22 position marker (`앞{@break:1}뒤` inserted a separator at
that position without reading the preceding word differently).

## Join control (2026-10-03)

`X{@join}` is the opposite of `{@break}`: the atom or `[[group]]` X is read as
usual and the spaces between the readings are dropped (project-owner request).

| Input | Output (ko-KR) |
|---|---|
| `[[너 나 안 본 지 두 달 다 되 감]]{@join}` | `너나안본지두달다되감` |
| `오늘 [[3시 반에 만나]]{@join} 알겠지?` | `오늘 세시반에만나 알겠지?` |
| `[[GV70 사러 간다]]{@join}` | `지브이칠공사러간다` |
| `[[너 나]]{@join}는 친구` | `너나는 친구` |

The group is normalized on its own with the engine's options, then spaces, tabs
and line breaks are removed from the reading; text around the control keeps its
spacing. Inspect reports one `CUSTOM` span with `rule=control.pattern`,
`unit=@join`; the trace records `JOIN` (the inner read adds no events). An
unknown name such as `{@joinx}` is ignored like any invalid control.

## Pronounce control (2026-10-09)

`[[group]]{@pronounce}` reads the group as usual and then writes its Korean as it
is pronounced, like the engine option `pronunciation=hangul` for that group only
(colloquial Hangul, no vowel length).

| Input | Output (ko-KR) |
|---|---|
| `오늘 [[민주주의의 의의]]{@pronounce}를 배웠다.` | `오늘 민주주이으 으이를 배웠다.` |
| `[[국물이 맛있다]]{@pronounce}` | `궁무리 마싣따` |

A word next to the group still decides liaison inside it; text outside the group
is not rewritten. The group is normalized on its own, like `{@join}`.

`X{@pronounce}` on one atom (2026-10-10) reads the atom in its sentence, with up
to two words on each side, and rewrites only the atom's reading:
`112{@pronounce}에 신고해주세요.` → `일릴리에 신고해주세요.` (112 is read digit by
digit because of 신고). See [Korean pronunciation output](korean-phoneme.ko.md).

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
`CUSTOM` Semantic IR with `@name` in `unit`. Short source text is stored in
`value`. Grouped patterns also receive `secondary="control-surface"` and the
complete decoded group through the existing `surface/length` callback arguments.
Those bytes are borrowed for the call, not engine-owned.
An unsupported pattern or an invalid source value returns `TTF_EINVAL`.

The first six directives supply these bounded policies; the twenty additional
[semantic directives](#explicit-semantic-types-v022-continuation) are listed below:

| Pattern | Source contract | Reading policy |
|---|---|---|
| `@native` | One or two ASCII digits, value 0–99 | Standalone native Korean number forms; zero uses `영` |
| `@sino` | Signed or unsigned decimal, without grouping or exponent syntax | Sino-Korean numbers |
| `@alphabet` | ASCII letters and digits (no spaces) | Per-character names; KO and EN |
| `@address` | Explicit Korean address group | Sino-Korean numbers, numeric hyphen `다시`; KO only |
| `@email` | ASCII unquoted mailbox + hostname, including internal hosts | Existing electronic spelling and layered address lexicons; KO/EN |
| `@url` | Printable ASCII reference, including relative paths | Existing electronic spelling; KO/EN |

Ungrouped pattern values retain the 95-byte Semantic IR limit. For example,
`1.5{@sino}` is one decimal source; `1,000{@sino}` remains one source but is
rejected by its reading policy. A grouped source may exceed 95 bytes, up to
`max_control_bytes` for the entire control. Its `value` is empty when the complete
decoded source does not fit, never a truncated prefix. `@address`, `@email`, `@url`
and grouped `@alphabet` consume the borrowed source. `@native`/`@sino` retain their
numeric limits; an oversized scalar group returns `TTF_ELIMIT`.

### Explicit semantic readings

```text
[[서울시 강남구 테헤란로 123-4 101동 1203호]]{@address}
→ 서울시 강남구 테헤란로 백이십삼 다시 사 백일동 천이백삼호
[[a1@b2.com]]{@email}
→ 에이일 앳 비이 닷 컴
[[q1/a2]]{@url}
→ 큐일 슬래시 에이이
```

These are explicit reading requests, so they produce spoken text in **both text
and speech output modes**, just like `@native`. Automatic EMAIL/URL recognition
still preserves spelling in text mode. Use groups around punctuation: in
`abc@example.com{@email}`, the atom grammar cannot bind the whole email. The
invalid trailing atom is rejected; use `[[abc@example.com]]{@email}`.

`@address` does not establish that an address exists. It preserves written Hangul
names, spacing and `. , ( )`; reads ASCII letter runs as letter names; renders
unsigned ASCII integers as Sino-Korean numbers, with leading-zero runs spoken as
digits; and reads a hyphen **only between numbers** as `다시`. No ordinal, date,
phone, range or counter classifier is applied inside this region. Nonzero postal
codes are cardinal unless a customer chooses the digits policy. A numeric run is
limited to 44 digits in cardinal mode or 95 in digits mode. Unsupported characters
(including fullwidth digits), isolated hyphens and tilde ranges return `TTF_EINVAL`.
English address conventions and exceptional place-name pronunciations are outside
this first policy; use a literal alias for an exceptional reading.

`@email` accepts one `@`, nonempty dot-atom local/host parts, no whitespace, empty
labels, edge dots or edge hostname hyphens. A host need not be on the TLD list or
even contain a dot. Quoted local parts, internationalized domains/mailboxes and
address literals are not supported. `@url` accepts a printable ASCII reference
with well-formed percent escapes; spaces and raw quotes, braces, backslashes,
angle brackets and backticks are rejected. This is a reading contract, not a
complete RFC validator. Neither control performs network access, percent decoding,
DNS lookup or mailbox delivery checks.

The explicit control is one CUSTOM span (`rule=control.pattern`, `unit=@name`).
Input offsets include group delimiters and directive; output offsets cover the
whole reading. For grouped controls, resource trace parts refer to the decoded
source or the number/letter sub-input being rendered, and `input-control.text`
retains the full decoded source even when `value` is empty. Plain input, literal
aliases, protected code and unknown-pattern errors retain their previous behavior.

Korean customer policies live separately from general numeric ranges:

```tsv
address_policy	ko-KR	number	cardinal
address_policy	ko-KR	leading_zero	digits
address_policy	ko-KR	hyphen	다시
```

`number` and `leading_zero` accept `cardinal|digits`; `hyphen` accepts a nonempty
printable label up to 95 UTF-8 bytes. Resource layers and same-layer declaration
priority are unchanged. `electronic_word`, pronunciation lexicons and
`speech_symbol` customize `@email`/`@url` exactly as for automatic electronic
reading. An explicit typed control chooses a reading algorithm; a literal payload
chooses the final output. These requests do not add persistent rules or entries.

See [Korean usage guide](explicit-pattern-controls.ko.md) for examples and limits.

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

## Decorative characters

`speech_decorative` holds the names this policy may speak: `★ 별`, `♥ 하트`,
`※ 참고표`, `→ 화살표`, `™ 상표` and similar, with English equivalents. The default
`follow` speaks none of them, so a script keeps reading as before. `read` speaks a
declared name like any other symbol name, separated from neighbouring words, and
leaves an undeclared character, emoji included, to the unreadable policy. Text
output is unaffected; add a customer row to name more characters.

## Uppercase abbreviations

`IT`, `AT`, `BE` and `IN` spell ordinary English words, so they used to reach the
word path while `KBS` or `OECD` were spelled. Under the default `spell` policy an
all-uppercase run of two or more letters is an abbreviation regardless, and under
`word` it keeps the word reading. A term the resources declare as an acronym
(`NASA`), a proper noun, or an explicit pronunciation lexicon entry outranks both,
and single letters, mixed-case names (`iPhone`) and English-context text are
unaffected.

## Explicit semantic types (v0.22 continuation)

See the complete [Korean directive guide](explicit-pattern-controls.ko.md#의미를-지정하는-예약어-20개-2026-09-28-추가).
In addition to native/sino/alphabet/address/email/url, built-in packs now support:

- Numbers: `@cardinal`, `@digits`, `@decimal`, `@ordinal`.
- Calendar: `@year`, `@date` (YMD), `@date-mdy`, `@date-dmy`, `@time`.
- Quantities: `@currency`, `@percent`, `@measure`, `@counter` (KO only), `@fraction`, `@range`.
- Digit groups: `@digits:N` reads digits N at a time (1-9; `1234{@digits:1}` 일 이 삼 사); several digits repeat in turn (`123123{@digits:12}` 일 이삼 일 이삼, `01012341234{@digits:344}`).
- Units: `@unit` (`3M{@unit}` 삼 미터): `@measure` with the unit looked up as written, then lowercase, then uppercase.
- Pairs and formulas: `@score` (`10:10{@score}` 십 대 십, ten to ten) and `@math` (`4-3{@math}` 사 빼기 삼, four minus three). A bare digit atom includes one `- + * / = :` between digits; group anything else with `[[...]]`.
- Technical strings: `@phone`, `@version`, `@ip` (IPv4 only).
- Invalid controls: an unknown name or a value the pattern rejects is ignored and the text is read as if the control were absent; malformed syntax is plain text. Option `invalid_control=error` (CLI `--invalid-control error`) makes the request fail as before.
- Output intent: `@preserve`, `@skip`.

`[[9/25]]{@fraction}` forces a fraction, `[[1984]]{@year}` forces a year without a cue.
Date order directives describe the **input** order; the pack's date output policy stays in force.
Numbers must be ASCII; general integers may have signs and correctly grouped commas, decimals
require a point, digits/ordinals accept only digits (ordinal > 0). Year requires four digits > 0;
numeric dates require a four-digit year and Gregorian validity. Phone accepts an optional `+`
and digit groups separated by a hyphen or spaces; no parentheses/extensions. Version accepts
optional v/V and at least two dot-separated numeric components. IPv4 requires four 0..255 octets.
Types needing a unit/currency require it in the source and a complete match of that type's grammar.
No guessed currency, partial success or raw fallback is permitted for an invalid directive.

New semantic sources are at most 95 decoded bytes; common numeric integral parts at most 18 digits.
Digits allow 95 digits. Preserve/skip may use the whole group byte limit. Existing native/sino and
address limits are unchanged. `@preserve` bypasses further normalization and character dropping.
`@skip` emits nothing, including no inserted space: surrounding whitespace belongs to the caller.
All explicit controls apply in both text and speech output and are inactive in plain/protected input.

The shared internal semantic helper is called by opting-in packs, not by core name dispatch.
It makes inner Semantic IR, then calls the pack verbalizer without recursive normalization.
The public span remains CUSTOM/control.pattern; `control-semantic` trace records the inner type,
value, grammar, unit and secondary value. `input-control` records full decoded source.
See `typed_controls_v22.jsonl` and the integration tests for examples and rejection contracts.
