# Deterministic context routing

The source script identifies a candidate source pack. Each registered pack then
scores the speech context independently. The core selects the highest positive
score and records every pack score in `context_trace`; with no positive score it
uses `default_language`. The default is a fallback, not a forced output language.
The existing C ABI is unchanged.

## Lexical runs and clauses

A lexical run contains Latin words, digits and intervening spaces. A script
transition, newline or clause punctuation ends the run. Attached apostrophes,
hyphens and decimal/version separators stay inside it. Resource-defined currency
prefixes, numeric signs and leading decimal points inherit the quantity context
instead of falling back to the configured language. Digit grouping commas and
resource `context_connector` symbols (`%`, `°`) also keep quantities connected to
the words that follow, so `$12,500 is the total.` uses English with the Korean
default. The language-local shared
helper `languages/context_run.h` handles only token boundaries; vocabulary and
weights live in pack resources.

The English pack uses the whole run inside the committed context window. Every
word contributes `latin_weight`; resource `context_word` entries add function-word
evidence. Numeric unit markers and ordinal suffixes are neutral. This avoids the
old fixed token radius losing the clause evidence at its last word.

`context_role` provides finite predicates, imperative verbs, subjects and polite
markers. A finite predicate with at least one other word adds `clause_weight`.
An imperative requires three words, or two with a subject/polite marker. These
are explicit heuristics, not a general part-of-speech parser. Non-clause evidence
is capped by `phrase_ceiling`, so a long noun list cannot defeat a Korean particle.

The Korean pack supplies its Hangul context floor, a strong signal for a Hangul
token itself, and one `particle_weight` across the complete preceding foreign
noun phrase. This includes spaces and conjunctions. A stronger English clause
signal may still win for a clause that is followed by Korean quotation grammar.

| Input | Speech routing | Normalized text |
|---|---|---|
| `Apple은 사과입니다.` | `Apple`: Korean | `애플은 사과입니다.` |
| `고객은 Apple Watch를 구매했습니다.` | Both foreign nouns: Korean | `고객은 애플 워치를 구매했습니다.` |
| `GPU and CPU를 사용합니다.` | Nouns and conjunction: Korean | `지피유 앤드 씨피유를 사용합니다.` |
| `GPU and CPU are ready라고 표시합니다.` | Complete English clause: English | Preserved |
| `한국어로 Please go to the 1st floor라고 말합니다.` | Through `floor`: English | `한국어로 Please go to the first floor라고 말합니다.` |

With default resources, `GPU and CPU` scores English 69 and Korean 260 when
followed by `를`. `Please go to the 1st floor` scores English 660 and Korean 260
before `라고`. Inspect prints these scores alongside source/context/render tags,
semantic class and pronunciation provenance. Numbers describe the policy; they
are not probabilities.

## Resource customization

English settings are in `resources/languages/en-US/rules/context.tsv`; Korean
settings and particles are in the matching Korean resource directory. A customer
resource can introduce another predicate without rebuilding the core:

```text
context_role\ten-US\tapproved\tfinite
```

Use actual tab characters in the file. Load it with `--customer FILE` or
`ttf_resource_load(..., TTF_LAYER_CUSTOMER)` before freezing the engine. A noun/verb
homograph can be deliberately changed with the same mechanism. Pronunciation
choices such as `watch -> 워치` remain in the separate pronunciation lexicon.

## Boundaries and current limits

A complete quotation remains a separate context scope. Batch and streaming use
the same deterministic commit planner, so equal text/config/resources produce the
same normalized text for arbitrary byte chunks. The scorer sees only its committed
context window; segment targets may therefore change context for long unquoted
clauses. It does not infer intent from unlimited future input.

Unknown predicates, noun/verb homographs, interrupted English clauses, comma
clauses, product slogans and two-word imperatives remain ambiguous. English
function words can preserve an English continuation even without a predicate;
strong attached Korean particles still win for noun phrases. Korean particle
matching uses resource prefixes and is not a morphological analyzer. These
limitations need independent domain references and resource changes; passing the
current corpus is not an estimate of general language accuracy.

`tests/integration/test_context_routes.py` checks source/context/render metadata
and exact text, including the final word of unquoted clauses, arbitrary input
chunk sizes, a data-only predicate override and an English-default engine with
explicit Korean grammar. No speech synthesis or audio comparison is involved.

## Explicit title cues (v0.5)

`context_cue en-US 제목은 800` (tab-separated) adds a score to the immediately
following English lexical run. `타이틀은` and `영문제목은` are also supplied. Labels
match complete tokens; horizontal spaces and an optional ASCII colon are allowed.
The run must include a real lexical word: `제목은 The 1st day입니다.` stays English,
while the established `오늘의 제목은 21st입니다.` reading remains Korean. At least
one word is a heuristic, not automatic detection of titles in arbitrary prose.

Scores are integer values in 0..1000; zero disables a cue through customer resources.
New labels use the same resource kind. The cue ends at script/clause boundaries,
and does not cross a newline, quotation boundary or intervening punctuation.
Pronunciation lexicons remain independent: `Apple Watch를` still localizes.

The shared segmenter retains cue prefixes and their following run so arbitrary
byte chunks and tiny targets do not discard the hint. Title retention is capped
at `max(max_segment_length, 256)` codepoints per committed unit; after that,
ordinary safe phrase boundaries apply. Very long titles can lose their cue after
the first committed window. This bounds commitment latency without changing ABI.
