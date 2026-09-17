# Architecture

The engine is a deterministic text frontend for multilingual TTS. Language data,
context signals and reading policy belong to packs and resources. The core owns
UTF-8 scanning, allocation, token boundaries, routing dispatch, rule selection,
metadata assembly and streaming lifecycle.

```mermaid
flowchart TD
    Input[UTF-8 text / incremental writes] --> Pending[Pending buffer]
    Pending --> Tokens[Universal tokenizer]
    Tokens --> Literal[Resource-driven literal recognition / neutral token view]
    Literal --> Planner[Shared stable boundary planner]
    Literal --> Preserved[TEXT IR / identity rendering]
    Preserved --> Spans
    Planner --> Router[Source and context router]
    Router --> Classifier[Universal and pack classifiers]
    Classifier --> IR[Universal Semantic IR]
    IR --> Verbalizer[Pack verbalizer]
    Verbalizer --> Foreign[Foreign pronunciation processor]
    Foreign --> Lexicon[User / Customer / Domain / System lexicon]
    Foreign --> Source[Source pronunciation dictionary / G2P]
    Source --> Phones[Pronunciation IR]
    Phones --> Target[Target phonetic renderer]
    Target --> Spans[Normalized text and spans]
    Lexicon --> Spans
    Foreign --> Spans
    Spans --> Callback[Committed segment callback]
    Callback --> TTS[TTS engine integration]
```

## Decisions

1. **C11, small runtime.** There is no runtime YAML, regex, ML or Python dependency.
   The typed API uses opaque request handles, public versioned descriptors, and
   explicit result ownership. Python is an optional resource build tool.
2. **Semantic IR before text.** Universal recognition creates semantic objects;
   classifiers do not emit spoken text. Pack verbalizers receive the IR. Semantic
   value, unit, source/context/render language and rule provenance survive in spans.
3. **Three language identities.** Source is inferred from lexical script or
   semantic syntax. Context uses pack scoring of the stable local unit and attached
   scripts/particles. Render language is selected by policy. For `1st는`, source is
   English, context/render Korean; the source ordinal `first` is rendered `퍼스트`.
   Complete quotations use an independent pack-scoring window. The shared boundary
   planner retains that scope across incremental writes.
4. **Resource precedence is explicit.** Customer > Domain > System > Common;
   pronunciation additionally has User above Customer. TN rules and pronunciation
   lexicons use different resource kinds. Same-priority rules use longest match,
   specificity and declaration order; exact customer rules outrank patterns.
5. **Pronunciation dispatch uses an intermediate representation.** Source packs
   produce a shared phone inventory; target packs consume it. The core does not
   enumerate English→Korean or other language pairs. Direct foreign lexicons
   remain a useful fast path.
6. **Frozen shared engine.** All configuration is mutable only during setup.
   `ttf_engine_freeze` publishes immutable resources. Each normalization call and
   stream owns its mutable state. Resource replacement requires a new engine;
   application code can swap engines after draining old requests.
7. **One boundary planner.** Batch and streaming process the same stable units.
   Targets use source character counts; pending input never changes an already
   committed unit. Segmentation configuration therefore also defines the context
   horizon. Context scoring cannot revise an earlier callback.
8. **Conservative commits.** Incomplete UTF-8 and open lexical tokens are held.
   Sentence punctuation is checked against lexical patterns and abbreviation data.
   Phrase segmentation waits for safe token boundaries. Maximum segment length is
   a soft bound for indivisible tokens; pending byte limits provide the hard cap.

## Components

| Component | Responsibility |
| --- | --- |
| `src/tokenizer` | Valid UTF-8, category boundaries, reusable byte offsets |
| `src/protected` | Shared literal grammar, neutral token view, pending lookahead |
| `src/semantic` | Universal structured token recognition and semantic values |
| `src/core` | Engine lifecycle, pack routing, result assembly |
| `src/rule`, `src/resource` | Exact/pattern rules, layered lookup, transactional load |
| `src/pronunciation` | Lexicon priority, source/target pronunciation dispatch |
| `src/segmenter`, `src/stream` | Deterministic safe boundary and callbacks |
| `languages/*` | Context scoring, classification hooks, verbalizers, G2P/renderers |
| `resources/*` | Reading policy, numeric/unit data, pronunciation data, overrides |
| `tools/cli` | Normalize, inspect and JSONL streaming transport |
| `tools/evaluation` | Independent text references, diffs, metadata and streaming equivalence checks |
| `tools/benchmark` | Warm-engine frontend commitment timing through public C API |
| `tools/resource_compiler` | Validated source resources to versioned runtime envelope |

Pack directories are discovered by CMake, which generates registration code. A
third language can also register a descriptor through the public API. The test
suite registers an independent `xx-Test` pack that recognizes a new script context
and verbalizes a cardinal without editing core code.

## Milestones and current state

| Milestone | Implemented initial scope | Next work |
| --- | --- | --- |
| 1 — Universal Core | UTF-8 tokens, Semantic IR, resource/rule manager, pack API | Richer grammar DSL, indexed compiled resources |
| 2 — KO / EN | Basic cardinal, ordinal, date/time, currency, percent, units; Korean counters/phone | Further locales, ambiguity and numeric grammars |
| 3 — Code Mixed | Pack context scores, local lexical runs, particle scope, English clause signals, `1st` case | Embedded quotations, longer mixed-language passages |
| 4 — Foreign Pronunciation | Layered lexicons, initialism/acronym data, source dictionary/G2P, phone IR, fallback | Broader reviewed vocabulary and text pronunciation accuracy |
| 5 — Streaming | Pending/lookahead, shared boundaries, phrase targets, callbacks, frontend benchmarks | More boundary/context cases and reduced repeated scanning |
| 6 — Extensibility Validation | Synthetic third-pack test, generated build registry | Real third language pack and platform matrix |

These are initial implementation milestones, not claims of complete linguistic
coverage. Production priorities are text corpus coverage, grammar and context
accuracy, reviewed pronunciation resources, and ABI compatibility automation.

## Text quality checkpoint (0.3.0)

The C ABI v1 and JSONL v1 layouts are retained. The evaluation contract is exact
normalized text plus semantic and language metadata. Each reference string is an
explicit reviewed policy choice, not a snapshot generated from current output.
Streaming checks compare multiple byte chunk sizes and must retain that reference.

The audio bridge and listening evaluation were removed from the current scope.
Runtime callbacks and JSONL remain available for downstream consumers.

## Finance/content checkpoint (0.4.0)

Finance has first priority and content narration is a second reference domain;
these priorities do not introduce domain names into the core. Numeric policies,
context roles and pronunciation entries remain resource driven.

Korean compound quantities are parsed as descending groups and major scales. Exact
decimal arithmetic produces a single canonical Semantic IR value before rendering.
Malformed compounds and IR overflow are preserved, and ordinary-word boundaries
are checked separately from numeric morphology.

The KO/EN packs share a language-local lexical-run helper. Pack-specific resources
supply predicates, particle evidence and weights; the core's pack-scoring interface
is unchanged. [Routing policy](context-routing.md) documents the clause/noun-phrase
heuristics and bounded streaming context.

[Scenario evaluation](domain-corpus.md) compares identical references on the frozen
previous version and current version. Strict regression/development gates and the
advisory challenge report are kept distinct. This produces evidence of improvement
and visible limitations without changing expected text to match runtime output.

## Financial/context checkpoint (0.5.0)

Universal syntax now parses exact scientific decimal shifts and quantity ranges;
unit inventories and readout labels remain resources. Explicit pack identifier
cues outrank equal-extent generic numeric/date syntax, preserving zeros in the IR.
The core does not branch on a language name. The KO pack supplies label semantics
and rendering. Existing ABI v1 layouts and callback ownership are unchanged.

Freeze builds an immutable index of effective boundary cues. The segmenter and
language packs use the same cue resources to keep label/number and title context
across stream writes. Title retention has a bounded horizon so a long English
run cannot postpone all output. [Streaming](streaming.md) records the tradeoff.

The 30-case strict advanced suite promotes 13 previously observed challenges and
adds 17 explicit references. The old 15-case file remains unchanged for historical
comparison, but is no longer an unseen holdout. HTML/code and paths remain visible
failures. [Financial contracts](financial-text.md) and [validation](validation.md)
state implemented policy and limitations separately from future production work.

## Literal-source checkpoint (0.6.0)

Explicit code/markup/path syntax is recognized before routing. The public scanner
still exposes normal lexical categories; its internal protected view coalesces
literal spans into neutral tokens. These feed both the commit planner and the
normalizer, keeping their byte boundaries consistent. Literal items become TEXT
Semantic IR with a typed unit and then identity output, without language-pair
logic or lossy copies into numeric value fields.

An active preservation policy takes precedence over TN/lexicon transformation.
Prose classifiers and rules cannot consume across a literal; pack results are
validated against this bound. Resource flags and opaque-tag inventories select
policy without rebuilding. See [protected text](protected-text.md) for exact
syntax and the distinction from a document renderer or speech policy.


## Workbench and interval checkpoint (0.7.0)

The native core classifies repeated explicit quantity endpoints as RANGE, with
exact decimal endpoint strings and one common unit. Written Korean scale amounts
stay in the Korean pack. Delimiters, reading separators, title cues and lexicon
entries remain resource policies. ABI v1 layouts are unchanged.

The optional `tools/web` transport uses Python stdlib HTTP and a disposable process
per operation. A ctypes adapter calls the native API, copies callback-owned text
and spans within each callback, and emits NDJSON events. The browser implements
inspection and test controls; it contains no duplicate normalization algorithm.
[Workbench contracts](web-workbench.md) document isolation, lifetimes and limits.

## Multiword pronunciation checkpoint (0.8.0)

At freeze, effective multiword pronunciation entries are indexed by their first
lexical word. The core matches bounded whole-token sequences without knowing the
source/target language pair. Language packs continue to decide context. The matched
surface becomes one WORD IR span before its pronunciation value is rendered.
Layer priority precedes match length, including single-word overrides. Protected
syntax and TN rules at the start retain their priority.

The shared batch/stream planner protects candidate prefixes using the same resource
inventory. Future bytes cannot retroactively invalidate an earlier protected cut;
this preserves segmentation parity when a later word mismatches. This is bounded
lexical lookahead, not sentence-level context recovery.

Public ABI v1 layouts and ownership are unchanged. Span granularity can change as
resource coverage grows: `Apple Watch` is now one pronunciation span instead of two
word spans and a space. The original 60-case `domain_cases.jsonl` is preserved as
historical evidence; `domain_cases_v8.jsonl` keeps every input/output reference and
updates only that case's metadata. Consumers must use byte offsets and semantic
fields rather than assume one span per word. See [multiword contracts](multiword-lexicon.md).


## Reference integrity and interval semantics (0.9.0)

Ranges carry both exact endpoints in Semantic IR before rendering. Shared units
and Korean scales are resolved by the generic classifier and language pack,
respectively. Counter/clock lookahead uses the same classification so a phrase
callback cannot prematurely commit the first endpoint. Clock ranges use unit
`time` with canonical values in the existing ABI v1 string fields.

Korean endpoints independently select native/Sino/unit/prefix resources and then
join using the configured interval connector. Symbol names are only used outside
recognized semantic ranges. Legacy bare-hyphen identifier overrides remain explicit;
resource-driven tilde ranges are not affected by that digit policy.

Expected-text fixtures have a separate audit inventory. Structural checks and
known-defect checks run without invoking the frontend, and the web exposes each
reference's review class. Changes to incorrect expectations are recorded with
before/after text and reasons. Historical output agreement is retained as evidence,
not promoted to linguistic truth. See [v0.9 validation](validation.md) and
[human resource editing](resource-editing-guide.ko.md).


## ADR: managed vocabulary and frozen TN index (v0.10)

Pronunciation authoring uses validated JSONL plus source catalogs. A deterministic
Python build tool generates the existing C-runtime TSV format; native consumers
have no JSON dependency. Records distinguish spelling-source verification from
independent human review, and scopes prevent ambiguous financial initialisms from
entering the default system manifest. This compiler is an authoring profile for
en-US→ko-KR; it does not restrict the public language-pack or pronunciation IR ABI.

The engine freeze step indexes only `exact`/`pattern` TN rules separately from
pronunciation records. Boundary cue indexing also filters kinds before effective
resource lookup. All indexes are engine-owned and immutable after freeze, preserving
shared-engine request concurrency and the existing deterministic priority ordering.
The public ABI is unchanged. This removes a whole-dictionary scan per token;
fixed hash bucket storage still needs growth/compiled storage work for large data.
See [source workflow](lexicon-maintenance.ko.md) and [scale experiment](lexicon-scale-v10.md).

## ADR: external evidence without replacing the C frontend (v0.11)

The [NeMo review](research/nemo-sproat.md) supports keeping class-specific
classification, verbalization and authoring-time compilation separate. We retain
numeric semantic values in Universal IR: NeMo's English tagger can already contain
English number words, which is not our language-neutral contract. Weighted FST
selection is not substituted for the existing explicit layer/longest/specificity/
declaration order. FAR/OpenFst/Sparrowhawk are not introduced as C11 dependencies.
Current TTFB remains a checked TSV envelope rather than a compiled FST or DAWG.

```mermaid
flowchart LR
    A[External source with revision and rights] --> B[Independent importer]
    B --> C[Local benchmark with reference status]
    C --> D[Frozen C engine and resources]
    D --> E[Contextual span and class failures]
    E --> F[Human review and independent contracts]
    F --> G[Deterministic grammar or resource change]
    G --> H[Same benchmark and chunk parity]
    C --> H
    H --> I[Fixed and regressed counts]
```

The Python research layer is outside the runtime. Its schema distinguishes source
annotations from candidates and unknown references. Evaluators validate complete
input/output byte coverage, include unaligned references as failures, record
content hashes, and reject incomparable runs. Corpus and failure text retain source
restrictions; no automatic feedback path copies them into shipped dictionaries.
[Corea Numeric](research/korean-sources.md) is an optional local restricted benchmark,
not product vocabulary or independently labeled written/spoken pairs.

[CMU import](research/pronunciation-sources.md) preserves every pronunciation and
stress marker offline. An explicit base-variant adapter can build the existing
native pronunciation resource with license/provenance sidecars. The source pack
and target renderer still meet at Pronunciation IR. Direct target-language
lexicons take priority; the native renderer's simplified phonetic output is not
approved automatically by a dictionary hit. Variant selection, complete phonetic
features and detailed source/layer trace remain future versioned API work.

The first feedback iteration adds generic unit/currency resources and makes Latin
currency prefixes use the universal numeric grammar. Single currency amounts now
use the same prefix/suffix consistency check as interval endpoints. No language-pair
branch, copied corpus-specific rule, or public ABI change is introduced.

## ADR: exact currency scale arithmetic and reading policy (v0.12)

Currency scale recognition is shared syntax whose vocabulary is supplied by
`currency_scale`, `currency_prefix` and `numeric_suffix` resources. A recognized
currency establishes monetary meaning before scale expansion; an unqualified
measurement such as `3m` keeps its existing unit. Explicit prefix/suffix currencies
must agree. Adjacent signs belong to the amount, and invalid sign combinations,
conflicting identities and values exceeding the bounded IR are preserved.

The classifier shifts decimal digits without binary floating point. An individual
`CURRENCY` stores the expanded amount in `value`, its ISO-style currency identity
in `unit`, and the recognized written scale exponent in `secondary`. English
reconstructs a coefficient and named scale for text output; Korean reads the
expanded value with its own numeral system. A currency `RANGE` continues to use
`value` and `secondary` for its two exact endpoints. Consumers dispatch on semantic
type, rather than treating every `secondary` field as a second number or scale.

Currency labels and English fractional-reading precision are editable resources.
`currency_minor_digits` distinguishes minor-unit rendering from an exact decimal
major-unit reading; it never authorizes rounding or validates a payment's legal
precision. Parser code does not contain a list of currency names. Korean written
scales remain pack grammar, while the generic parser handles resource-defined
English financial scale spellings in either context language.

The batch and stream boundary planner use the same classifications and scale
continuation inventory. They retain incomplete monetary expressions until the
required input is available. Original references, exact Decimal arithmetic checks
and arbitrary chunk tests validate text, metadata and commit behavior. External
benchmark re-evaluation uses the unchanged source references rather than fitting
currency identities to reference wording. No audio evaluation is introduced.

Public ABI v1 signatures, struct layouts and ownership remain unchanged. This
extends supported semantics in existing fields. See [currency contracts](scaled-currency.md),
[resource policy and sources](currency-resources.md) and [IR field meanings](semantic-ir.md).

## ADR: calendar precision and deterministic continuation (v0.13)

English named-date grammar lives in `languages/en-US/date.c`. Month aliases,
contextual year cues, following quantity guards, and year reading styles are
resource data. Classification emits Gregorian values and precision (full date,
month/day, year/month or year) before either language pack renders the output.
Named input order is retained separately from its numeric value. Missing fields
are never filled with today's year. Numeric-only contextual years retain neutral
source language; a month name establishes English source language.

For equal source extents, an explicitly classified pack DATE outranks a generic
CARDINAL/NUMBER. Longer semantic objects and exact/pattern resource overrides
retain their existing precedence. This rule is language independent and does not
grant dates priority over currency, identifiers or protected literals.

The segmenter uses pack-declared `boundary_word` and `boundary_period` hints.
It retains possible continuations after month aliases, year cues, ordinal suffixes
and numeric commas, using only the prefix before a candidate cut. It does not
decide a cut from future bytes that might arrive in a later input write. These
hints can lengthen a segment; configured length targets remain soft at semantic
boundaries. The hard pending-byte limit still bounds request memory.

Existing ISO date readings remain stable, while named dates and explicit year
cues use a documented calendar-year policy. An external reference's lowercase
month or different date order does not override the product policy. Diagnostic
casefold and alignment counts are reported separately from exact-match accuracy.

## Written currency and abbreviation checkpoint (0.14.0)

Written currency names use a separate word-alias resource namespace. Recognition
folds ASCII case and horizontal whitespace, checks complete boundaries, resolves
unit identity before verbalization and reuses the exact decimal/range parser.
Ambiguous aliases require explicit syntax or resource context cues. A contradictory
or malformed suffix is retained as one TEXT object. Source language metadata is
carried by the alias while speech language remains the router's decision.

Dotted initial and title grammar resides in the EN pack. Initialism IR stores a
canonical letter sequence and a resource-selected reading policy; a terminal
period can remain independent punctuation. Generic boundary syntax retains an
incomplete initial period without relying on unavailable future chunks. Ordinary
word pronunciations and explicit initialism pronunciations use separate source
resource namespaces, preventing a dictionary pronoun from overriding a letter
sequence. Four Korean title readings pass through managed target lexicons.

Foreign pronunciation selects among original and canonical spelling aliases by
resource layer first and exact spelling second. This retains user overrides for
ordinals and dotted initials after source verbalization. No public ABI layout or
source-target language-pair branch is introduced. Token, semantic and output
byte offsets remain tied to the original text.

The source resource compiler now uses exactly the runtime's LF/CRLF record
boundaries. Benchmark comparison independently reconstructs summary metrics from
result rows, checks reference fields/order and exact flags, and hashes report
files before and after use. [Remaining work](remaining-work.md) separates verified
contracts from unclosed production gaps, including source pronunciation layer
trace, Unicode numeric grouping, Korean counter roles and independent references.

### v0.15 classifier specificity and shared numeric grammar

Universal syntax and language-pack candidates compete over the same token view.
Greater token/byte extent wins. At identical extent a pack may declare
`classifier_priority <language> <rule> <integer>` (default 0, supported -1000..1000).
Higher specificity wins; remaining ties retain established pack registration order.
The existing explicit identifier/date refinements and lexical WORD refinement remain.
This lets Korean ordinal/count morphology refine a generic numeric suffix without
adding language IDs to the core. Exact/pattern TN replacements retain their later,
separate resource-layer priority. Invalid priority values fail with `TTF_ERESOURCE`.

The built-in packs share the exact decimal scanner through an internal interface;
public language-pack ABI v1 is unchanged. Unicode group separators are validated
without rewriting source text. KO ordinals use the same scanner so `제1 250호`
cannot become two independent numbers. Unsupported forms preserve their complete
candidate text. The prefix-stable segment planner keeps numeric grouping spaces,
registered ordinal/count cues and declared quantity tails pending. Both batch and
streaming therefore classify the same committed text.

Pronunciation source provenance now carries an optional validated envelope through
the existing ABI. The core reports the consumed source resource layer, and retains
the prior path strings. The target mapper layer is still a separate, unreported
concern; see [trace contract](pronunciation-trace.md).
