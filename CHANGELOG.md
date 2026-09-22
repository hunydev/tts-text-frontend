# Changelog

## Unreleased — speech final gate

- Read basement floors `B1층`/`B1F` as `지하 일 층` and ranges `B1~B3층` as
  `지하 일 층에서 지하 삼 층`; add `지하`/`주차장` floor cues (`지하 2F` → `지하 이 층`).
  Bare `B2`, `B2B` and vitamin `B1` stay identifiers.
- Add 19 floor contracts; external English results are identical.
- Read Korean `대` after a decade or a large scaled amount as a band: `20대 남성` →
  `이십 대 남성` (was `스무 대`), `1~2억대` → `일억에서 이억 대`. Adjacent cues
  (`차량 20대`, `20대 차량`) keep the machine count `스무 대`.
- Read `1억~2억 원` as an ascending interval when both endpoints are scaled; the
  descending `1억-2천만원` stays verbatim.
- Add 25 band/count contracts; external English results are identical.
- Read arithmetic expressions with a relation or an unambiguous operator:
  `3-1=2` → `삼 빼기 일은 이`, `3>2` → `삼은 이보다 크다`, `a+b는` → `에이 더하기 비는`.
  `math_relation` templates pick 은/는 from the left side; bare `5-3` ranges,
  `2+1`, dates and phone numbers are unchanged. The expression's own letters are
  not language evidence.
- Korean `#` reads `샵` (`#해시태그` → `샵 해시태그`, `C#` → `씨샵`); English uses
  `speech_symbol_prefix` for hashtags (`#hashtag` → `hashtag hashtag`).
- Add 37 expression/hashtag contracts; external English results are identical.
- Read email/URL labels as words when a resource supports it (`electronic_word`,
  then the pronunciation lexicon) and spell the rest: `help@naver.co.kr` →
  `에이치이엘피 앳 네이버 닷 씨오 닷 케이알`, `.com` → `닷 컴`. This replaces the
  v0.20 all-letter spelling contract. Add 11 pending mail/portal lexicon entries.
- Recognize bare hosts ending in a declared lowercase `url_tld` (`naver.com에서`);
  text output preserves them.
- Read versions and IP addresses in speech output (`2.1.3` → `이 점 일 점 삼`,
  `v1.10.2`, `192.168.0.1` → `일구이 점 일육팔 점 영 점 일`); text keeps the surface.
- Add 34 electronic contracts; external English text output is unchanged.
- Spell short capital runs joined by an unspaced ampersand as one initialism
  (`R&D` → `알앤디`, `AT&T` → `에이티앤티`); speech reads the joiner from
  `speech_symbol &`, text keeps the written `&`. Lowercase words keep word grammar.
- Read a lone capital letter inside a Korean sentence as its letter name
  (`A 학점` → `에이 학점`, `K팝` → `케이팝`, `X세대` → `엑스세대`).
- Add 26 letter-name contracts. External English outputs are unchanged; three
  `A&M`/`DP&I`/`D&D` spans now carry INITIALISM IR.
- Read registered symbol and number-form characters as their own words in speech
  output: circled numbers, Roman numerals, vulgar fractions, `±`, `×`, `÷`, `㈜`.
  `speech_symbol` now accepts one non-ASCII symbol code point; `speech_join
  symbol|symbol.<char>` sets separation per language (KO `&` stays joined).
- Treat `~`/`～` after a word and before a pause as a lengthening mark
  (`speech_lengthening`): speech drops it, text keeps the written mark instead of
  the former `물결`. Standalone tildes and numeric ranges keep their readings.
- Map compatibility unit signs (`℃`, `㎏`, `㎞`, `㎝`, `㎜`, `㎎`, `㎖`, `ℓ`, `㎾`, `㎓`,
  `㎒`, `㎐`) to the existing canonical units; keep middle dot `·` as a pause mark.
- Speech output no longer leaves a doubled or leading space where an unreadable
  span was removed. Removal between two words now leaves one space (`가🙂나` → `가 나`), replacing the v0.20 joining contract by project-owner decision.
- Add 40 independent final-gate contracts; the external English development
  sample remains byte-identical to v0.20.

## 0.19.0

- Reuse request-local lexical-run boundaries and KO/EN context evidence while
  preserving scores, resource priority, quotations and full output metadata.
- Invalidate cached evidence after partial token consumption; isolate nested and
  concurrent requests; propagate factory failures with deterministic cleanup.
- Add native score/ownership checks, v0.18 metadata compatibility cases, arbitrary
  streaming splits and whole-unit 3,000/30,000-number regression gates.
- Add a reproducible context-scaling comparison tool and document borrowed pack
  engine views without changing public ABI v1 layouts.
- Retain identical results on the existing external English development sample.
  Repeated token mutations, pending scans and linguistic coverage remain open work.

## 0.7.0 — Financial intervals, content vocabulary and web workbench

- Normalize repeated currency/rate/measure endpoints into exact RANGE IR; retain
  mismatched units and malformed/chained intervals as source text.
- Extend explicit content title cues and add 40 category-separated pronunciation seeds.
- Add TTF Lab: actual C ABI integration, normalized text/IR inspection, live output
  callbacks, chunk parity verification, user/customer resource experiments and corpus reports.
- Add web API/ABI and Chromium browser gates; keep native ABI v1 and SONAME 1.
- Freeze 24 interval and 40 content references before baseline execution; document
  bounded syntax, small-context behavior and synthetic-data limitations.

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
