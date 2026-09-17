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

- Dotted `Dr.`, `Mr.`, `Mrs.`, `Prof.` expand before an ASCII-capitalized name.
  The default labels are `Doctor`, `Mister`, `Missus`, `Professor`.
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

`initialism_policy` keys are `dotted`, `known`, `inferred`. Values are `preserve`,
`letters` or `names`. `initialism_text` supplies the same policy for an individual
lowercase canonical key. `letter_name` supplies the names used by `names`.
`abbreviation_expansion` can use `preserve` to disable a title expansion.
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
