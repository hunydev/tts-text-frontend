# English abbreviations, initials and written currency (v0.14)

This iteration adds written currency identity and explicit abbreviation reading
policy. It also resolves pronunciation aliases before choosing a resource layer.
All examples are normalized text; no synthesized audio is evaluated.

## Named currencies

See [written currency guide](written-currencies.md) for aliases, exact decimal
values, range syntax, ambiguity guards and customer edits. Recognition is data in
`resources/languages/en-US/rules/currency-names.tsv`. Core grammar consumes the
shared semantic form; KO and EN retain their own currency verbalizers.

| Input | Default normalized text |
| --- | --- |
| `The budget is 2.5 million dollars.` | `The budget is two point five million dollars.` |
| `제작비는 2.5 million dollars입니다.` | `제작비는 이백오십만 달러입니다.` |
| `The fee is 3 British pounds.` | `The fee is three pounds.` |
| `The box weighs 2 pounds.` | `The box weighs two pounds.` |

The last row is a cardinal and ordinary word, not a GBP amount. Identical English
text alone cannot prove the currency class or value; inspect the Semantic IR.

## Abbreviation policy

The EN pack uses `abbreviation.c`, with resources in
`resources/languages/en-US/rules/abbreviations.tsv`.

- Dotted `Dr.`, `Mr.`, `Mrs.`, `Prof.`, `St.`, `Mt.`, `Ft.` expand before an
  ASCII-capitalized name. The default labels are `Doctor`, `Mister`, `Missus`,
  `Professor`, `Saint`, `Mount`, `Fort`.
- Terminal `Dr.` and `Oak Dr.` retain their spelling. Ambiguous `St.` and `Ms.`
  do not receive an invented expansion. This finite name guard is not address or
  person-name recognition.
- Uppercase dotted initials such as `U.S.` and `J. R.` use the `INITIALISM` IR,
  canonical value `US` or `JR`, and secondary `dotted`. A single `J.` requires a
  following capitalized name.
- Dotted initials produce separated lowercase English letters by default.
  A final sentence period stays a separate punctuation span. The punctuation
  can therefore survive both English text and Korean pronunciation rendering.
- Known undotted initialisms and inferred uppercase tokens retain English text
  by default. `NASA`, ordinary uppercase prose and protected literals keep
  their established policies. Uppercase alone does not justify spelling out
  every English word.
- Dotted single-letter tokens supply no English context vote by themselves.
  Surrounding words or the configured default determine speech language.

| Input | Default normalized text |
| --- | --- |
| `Dr. Rowan spoke.` | `Doctor Rowan spoke.` |
| `The U.S. market opened.` | `The u s market opened.` |
| `J. R. Rowan spoke.` | `j r Rowan spoke.` |
| `U.S.를 확인합니다.` | `유에스를 확인합니다.` |
| `NASA released the image.` | `NASA released the image.` |

The title-expansion facts were checked against publisher references:
[Cambridge Dr](https://dictionary.cambridge.org/dictionary/english/dr) and
[Dictionary.com titles](https://www.dictionary.com/articles/mr-mrs).
The exact output casing, name guard and spelling policy are project choices.
No dictionary definitions, example sentences or pronunciation tables are copied.
Four Korean title pronunciations live in the managed lexicon and remain pending
independent pronunciation review; they are not automatically approved by these
English references.

## Resource editing without rebuilding

Create a customer overlay with real tabs:

```sh
python3 - <<'PY'
from pathlib import Path
Path('/tmp/ttf-initials.tsv').write_text(
    'initialism_policy\ten-US\tdotted\tnames\n'
    'initialism_text\ten-US\tgpu\tletters\n'
    'abbreviation_expansion\ten-US\tdr\tDoc\n', encoding='utf-8')
PY
build/ttf normalize --resources resources --language en-US \
  --customer /tmp/ttf-initials.tsv 'Dr. Rowan uses GPU in the U.S. market.'
```

Expected: `Doc Rowan uses g p u in the you ess market.`

`initialism_policy` keys are `dotted`, `known`, `inferred`, and the speech-mode
variants `dotted_speech`, `known_speech`, `inferred_speech` and
`pronounceable_speech`. A speech key is consulted only in speech output mode and
only when `initialism_text` says nothing; `pronounceable_speech` covers an
uppercase run that reads as an English word (`NASA`, `FIFA`), which the
`syllable_onset` and `syllable_coda` cluster tables decide. The system defaults
spell an inferred initialism in speech (`FBI` → `f b i`) and keep a pronounceable
run as written. Values are `preserve`, `letters` or `names`. `initialism_text` supplies the same policy for an individual
lowercase canonical key. `letter_name` supplies the names used by `names`.
`abbreviation_expansion` can use `preserve` to disable a title expansion.
`initial_continuation` lists the marks that carry a sentence on after a single
initial (`,`, `;`, `(`, `"`, `&`, `:`). Before one of them the initial is read as
a letter and its own period is dropped (`Cossette , M. , Lecomte` →
`Cossette , m , Lecomte`); a sentence-ending period is kept, and a key that is
`.`, `!` or `?` is rejected.
`abbreviation_bare` expands a token that carries no period at all (`vol 1` →
`volume one`, `Records ltd` → `Records limited`); it is only for abbreviations
with a single reading, and `preserve` disables one. `st`, `dr`, `sr`, `no` and
`pp` are deliberately absent, because a bare token there could be saint or
street, doctor or drive.
Invalid initialism policy values fail explicitly when used. Semantic validation
of every resource during setup remains tracked in [remaining work](remaining-work.md).

For a new title key, also declare an `abbreviation` entry with value `preserve`
in the same overlay: the boundary planner must know that the period can continue.
The expansion table and boundary table have distinct responsibilities.
`boundary_syntax en-US dotted_initials 1` opts into prefix-safe initial periods.
`context_neutral en-US dotted_initials 1` prevents an isolated dotted spelling
from choosing English speech context by itself.

## Pronunciation aliases and precedence

Before phonetic localization, the core checks the original surface and source
verbalizer's canonical text. The order across aliases is **User > Customer > Domain
> System > Common**; the original spelling wins ties. A sentence-ending period is
also considered when looking up a complete dotted abbreviation, while it remains
an independent output punctuation span. Thus a user `U.S.` entry applies both to
`U.S.를` and to a terminal `U.S.`. A user `US` canonical entry can override a system
`U.S.` entry. An exact TN rule still precedes pronunciation processing.

This fixes raw `1st` pronunciation overrides that previously disappeared after
verbalization to `first`. Semantic type/value/source/context stay unchanged; the
selected lexicon layer is visible in inspect. Lexicon pointers remain engine-owned,
result buffers request-owned, and public ABI v1 layouts are unchanged.

Source pronunciation dictionaries use `pronunciation` for ordinary words and
`initialism_pronunciation` for an explicit letter-sequence override. Thus a CMU
`us` word entry cannot make `U.S.` read as the pronoun. Without an initialism entry,
the source pack constructs letter phones. Target-language User/Customer/Domain/
System pronunciation lexicons still precede either source dictionary.

## Verification and remaining coverage

`english_forms_v14.jsonl` provides 24 independent original text/IR contracts,
available as **영어 통화명·약어·이니셜** in the web workbench. The integration tests
cover policies, guards, aliases and every two-way byte split of selected inputs,
including tiny phrase targets. Existing reference strings are not regenerated
from candidate output.

The grammar is finite. Undotted titles, lowercase dotted acronyms, general name
recognition, possessives, arbitrary sentence-final abbreviation disambiguation
and broad currency-name inflections require more work. Phrase target changes can
still change the context horizon in other constructions. Neither benchmark
agreement nor these tests establish complete production language accuracy.

## Dotted word abbreviations (v0.21 continuation)

`resources/languages/en-US/rules/abbreviations-dotted.tsv` explicitly opts five
period-bearing forms into the **already established bare readings**. Titles keep
an independent following-name guard. No outside dictionary/code was imported.

| Input | Output |
|---|---|
| `Acme Ltd. paid 12 dollars.` | `Acme limited paid twelve dollars.` |
| `Read Vol. 3.` | `Read volume three.` |
| `Alpha vs. Beta` | `Alpha versus Beta` |
| `Use paper, ink, etc.` | `Use paper, ink, etcetera.` |
| `Rowan Jr.` | `Rowan junior.` |

The English pack produces `ABBREVIATION`, lowercase key in `value`, `dotted` in
`secondary`, and `en.abbreviation.dotted` in `rule`. The source language is
`en-US`; context and render language retain normal routing. Text and speech use
the same semantic expansion. In Korean context the expanded English word enters
the existing pronunciation pipeline; this change does not certify the naturalness
of its unknown-word Hangul fallback. User/customer lexicons may set that reading.

### Editing the resources

Author a separate UTF-8 TSV with real tabs and load it with `--customer FILE`:

```tsv
abbreviation_dotted	en-US	inc	incorporated
abbreviation	en-US	inc	preserve
abbreviation_dotted	en-US	vol	part
abbreviation_dotted_period	en-US	vs	sentence
```

- `abbreviation_dotted`: 1..31 lowercase ASCII letters; nonempty reading below
  64 bytes, or `preserve` to disable expansion. A new enabled key also needs the
  `abbreviation` marker so sentence/phrase streaming retains enough input.
  Final resource validation rejects a missing marker. Ordinary bare words are
  not changed by this table.
- `abbreviation_dotted_period`: `sentence` (default) or `continuation`.
  `sentence` retains a period before a following capitalized ASCII word, since
  it may start a new sentence. `continuation` consumes it there (`vs. Beta`).
  Both retain the dot at EOF, newline, a closing quote/bracket, or before further
  dots. A following `!`/`?` or comma keeps that punctuation and consumes the
  abbreviation's own dot. This is a deterministic finite policy, not general
  sentence-final abbreviation disambiguation.
- Customer > Domain > System; same-layer earlier declarations win. `inspect
  --trace` records the selected expansion and any explicit period policy.
- An ASCII letter/digit, extended Latin letter, `_`, `-`, slash, backslash or `@`
  immediately after the dot is not this grammar. `Vol.3`, `Vol.é`, hosts, paths,
  email and protected literals retain their existing interpretation.

For a Korean conventional reading, use the separate pronunciation namespace:

```tsv
lexicon	ko-KR	en-US:limited	리미티드
```

`표기는 Ltd.입니다.` then yields `표기는 리미티드입니다.`. A user `Ltd.` pronunciation
entry still takes precedence over that customer verbalized alias. This does not
translate `limited` into a Korean company type. Pronunciation and TN rows remain
separate. `Dr.`, `St.`, `No.` and undeclared ambiguous abbreviations are unchanged.

### Evidence and limits

The 30 original references were frozen before candidate changes (SHA256
`b7c1d5a34ec18816b996cdb0d189cd1fb2344cf27878cd1026cc029cca521382`).
`tn_cycle`: **9/30 → 30/30**, fixed21, regressed0; no expected strings changed.
Two segment profiles, text/speech, every two-part byte split, single bytes and
seeded random schedules give **1,738** exact text/span/trace comparisons. Native
and CLI checks cover semantic offsets, period ownership, disabling, custom keys,
validation bounds, layer ties, code-mixed aliases and extended Latin boundaries.
These are project policy contracts, pending independent linguistic review.

Possessives, general sentence inference, multi-dot abbreviations such as `e.g.` /
`i.e.`, and context-dependent `Co.` / `Inc.` / `No.` remain separate work. Those
forms were not silently added to the system table. The web workbench corpus is
`abbreviation-dotted-v21`; serving it requires a separately authorized deployment.
