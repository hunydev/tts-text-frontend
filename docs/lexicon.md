# Pronunciation lexicons

Pronunciation overrides are separate from TN rules. `3개 → 세 개` is semantic
verbalization; `Apple → 애플` is a pronunciation lookup.

## Authoring and review (v0.10)

`lexicons/ko-KR/*.jsonl` is the managed source of the system/finance foreign
lexicons. `lexicons/sources/*.json` records evidence and its limits. Run
`python3 tools/lexicon/manage.py build` to generate deterministic runtime TSV;
CI uses `build --check` to reject drift, canonical collisions, missing evidence
and accidental system activation of the scoped finance dictionary. Customer
overrides can still be authored directly as TSV or loaded through the public API.

The inventory has 379 entries / 381 keys: 333 system entries and 46 optional
finance entries. The 85 source-checked entries have documented source comparisons;
294 are pending and none is independently human-reviewed. Source checking and
test conformance do not approve pronunciation quality. The authoring tool currently
supports the bounded en-US → ko-KR key grammar; the native language-pack ABI remains
extensible. [Maintenance guide](lexicon-maintenance.ko.md) covers editing, domain
selection, overrides, review states and a complete test/deployment workflow.

`tools/lexicon/coverage.py` and the web `/lexicon` page use actual span traces to
report dictionary hits and fallback candidates. They do not promote generated
fallback spellings into the source dictionary. All 381 managed keys are exercised
through the native engine in addition to 40 hand-authored contextual references.

The new immutable TN-rule index prevents pronunciation entries from being scanned
on every rule lookup. Hash storage still uses a fixed bucket count; see
[measured scale limits](lexicon-scale-v10.md).

| Priority | Typical owner |
| --- | --- |
| User | Per-engine request/profile pronunciation customization |
| Customer | Organization-specific names and policies |
| Domain | Finance, telecom or product vocabulary |
| System | Curated language-pack vocabulary |
| Fallback | Source dictionary, G2P, phonetic renderer, grapheme renderer |

`ttf_lexicon_add(engine, layer, source_lang, render_lang, word, pronunciation)`
adds/replaces an entry at one layer before freeze. Word keys use deterministic ASCII
case folding; source and render language tags are explicit. `ttf_lexicon_remove`
removes that layer's entry, revealing lower-priority entries. To evaluate emergency
fallback, remove the system lexicon entry and any matching source pronunciation
resource in a separate test resource tree, or invoke the target renderer directly.

Foreign lexicon TSV rows have kind `lexicon`, language equal to the target tag and
key `<source-tag>:<lowercase-word>`:

```text
lexicon<TAB>ko-KR<TAB>en-US:apple<TAB>애플
```

System data are grouped into `common`, `brands`, `technology` and `acronyms` under
`resources/languages/ko-KR/lexicon/`. New categories such as proper nouns and customer
terms can be listed in the manifest without editing C code. Entries include Apple,
Microsoft, Google, YouTube, WiFi, OpenAI and ChatGPT, plus initialism/acronym defaults.
SQL and GIF readings are policy choices that callers can override.

The seed vocabulary also covers common computing vocabulary (API, SDK, JSON, Docker,
Chrome, Unicode, C++, USB-C), ordinary loanwords (coffee, computer, server, client),
and cardinal/ordinal words needed after English verbalization. These are hand-authored
project defaults, not an imported comprehensive dictionary. Ambiguous product names,
regional variants, and alternative initialism readings still need customer policies.
Golden references exercise these entries in Korean context; English context must
continue to preserve the original English surface.

Runtime records live in an engine-owned hash table. Lookup is independent of the
storage representation; changing to a trie, double-array trie, DAWG, FST or compiled
binary index can happen behind that interface. This initial table is suitable for
small to moderate seed data; large dictionaries need memory and collision profiling.

Inspect output records hit provenance and layer. The callback/result text is owned
by the request, so an application can replace entire frozen engines without pointers
from a completed result depending on lexicon record lifetimes.

The v0.4 seed contained 160 direct foreign-pronunciation entries. Five additions
support the reviewed phrase cases (`watch`, `and`, `the`, `database`, `backup`).
The v0.5 seed has 163 entries, adding `real-time`, `dashboard` and `quasar`.
Adjacent Latin words connected by single hyphens are now one lexical WORD (bounded
to 95 bytes), allowing user/customer whole-term overrides before G2P fallback.
English context preserves the original spelling. These three curated entries fix
known vocabulary cases; they do not establish general unknown-word accuracy.

## Multiword names (v0.8)

The same lexicon resource and public add/remove APIs support bounded multiword
names such as `Bank of America` and `After Effects`. Matching is restricted to
whole lexical tokens, with ASCII case folding and horizontal whitespace folding.
Layer priority is evaluated before length, including a higher-layer single-word
entry competing with a lower-layer phrase. At the same layer, the longest match
wins. Phrase data stay separate from TN rules.

The segment planner retains matching prefixes, so an input write ending after
`After ` cannot cause premature single-word rendering. A matched phrase carries
one semantic span and a pronunciation layer; English context and explicit English
title cues still preserve the source. See [full contracts and limits](multiword-lexicon.md).
