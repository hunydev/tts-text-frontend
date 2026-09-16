# Changelog

## 0.6.0 — Literal code, markup and path preservation

- Resource-driven preservation of opaque HTML-style elements, comments, backtick
  code, explicit Unix/Windows paths and quoted paths with spaces/UTF-8.
- Shared protected token view isolates literal contents from language routing,
  classifiers, rule overrides and sentence/phrase boundary interpretation.
- Emit inspectable neutral `TEXT` spans; preserve unfinished literals on flush,
  reject invalid UTF-8, and honor pending caps. Native ABI v1 remains unchanged.
- Cache safe incomplete-literal body continuations with incremental UTF-8
  validation; keep closing-delimiter and early callback behavior observable.
- Propagate planner allocation/resource errors through the streaming API.
- Add 40 frozen synthetic text references, parser, stream-limit and overlay tests.
  Same references improve 4/40 to 40/40 across text/metadata/parity; historical
  challenge cases improve 13/15 to 15/15. These are development regression results.

## 0.5.0 — Financial identifiers, exact quantities and content cues

- Explicit resource labels select identifier digit reading with leading zeros and
  group boundaries; customer labels, zero spelling and separators are configurable.
- Added distinct `bp`/`%p` measures, decimal/signed ranges with units, exact bounded
  scientific notation, and uppercase AM/PM. Semantic values remain inspectable.
- Added resource title cues and generic hyphenated foreign words; curated spellings
  for `real-time`, `dashboard` and `quasar`. Unknown-word G2P remains a prototype.
- Preserve cue/value context across arbitrary stream chunks and tiny targets;
  bound title retention and avoid repeatedly rescanning prior words at each cut.
- Add optional `secondary` to JSONL v1 span records and `secondary`/`unit` to inspect.
  Native ABI v1 signatures, public structs and shared SONAME are unchanged.
- Promote known challenges into a strict 30-case text suite. Compare frozen v0.4
  output: 8/30 to 30/30; unchanged challenge references: 3/15 to 13/15. HTML/code and
  file-path handling still fail their desired references. No speech evaluation.

## 0.4.0 — Finance and content text quality

* Parse compound Korean amounts into a single canonical semantic value with exact
  decimal arithmetic; preserve invalid scales, grouping and overflow.
* Score complete local foreign phrases and English clauses, retaining language
  identity at clause endings and propagating Korean particle context across nouns.
* Add finance/content scenario references written before baseline execution,
  immutable baseline comparisons and a separate advisory challenge report.
* Add independent amount-value/metadata checks, resource-only routing overrides,
  and arbitrary byte splits through spaced compound amounts.
* Keep C ABI v1. Korean scale resources now use atomic exponents; see the migration
  note in `docs/ko-coverage.md` for old compound scale aliases.

## 0.3.0 — Text correctness and broader grammar

* Replace synthesis/listening evaluation with independent normalized-text references,
  exact matches, character edit counts, metadata assertions and byte-chunk parity.
* Remove the optional audio bridge, synthesis test dependency and audio CI artifacts.
* Recognize spaced quantities, fractions, seconds in times, Korean calendar forms,
  scaled Korean currency and resource-declared technical terms.
* Preserve invalid dates/times, grouped numbers, ordinal spellings and unsupported
  numeric atoms without partially converting their prefixes.
* Scope context inside complete quotations and preserve them during commitment.
* Resolve compound foreign verbalizations through the layered word pipeline.
* Expand KO/EN/foreign corpora and curated category lexicons. C ABI v1 is unchanged.

## 0.2.0 — TTS integration and evaluation

* Versioned JSONL streaming transport with language metadata and early pipe output.
* Optional reference TTS bridge using eSpeak NG or a configurable external adapter.
* PCM WAV validation, per-language synthesis jobs and listening manifests.
* Evaluation corpus, text comparisons, fallback statistics and HTML listening reports.
* C frontend latency/throughput benchmark with warm-engine timing and batch parity.
* Additional protocol, adapter and benchmark tests; CI publishes evaluation audio.
* Existing C ABI v1 and library behavior are retained.

## 0.1.0 — Initial multilingual frontend

* C11 static/shared libraries, C ABI v1, KO/EN language packs and resource layering.
* Semantic and pronunciation IR, context-sensitive ordinals and foreign pronunciation.
* Deterministic batch/stream normalization, metadata inspection, golden corpus and CI.
