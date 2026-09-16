# Architecture

The engine is a deterministic text frontend for multilingual TTS. Language data,
context signals and reading policy belong to packs and resources. The core owns
UTF-8 scanning, allocation, token boundaries, routing dispatch, rule selection,
metadata assembly and streaming lifecycle.

```mermaid
flowchart TD
    Input[UTF-8 text / incremental writes] --> Pending[Pending buffer]
    Pending --> Tokens[Universal tokenizer]
    Tokens --> Planner[Shared stable boundary planner]
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
