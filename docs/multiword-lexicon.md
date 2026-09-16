# Multiword foreign pronunciation (v0.8)

A foreign name can require one pronunciation entry covering several words. In a
Korean speech context, the frontend now matches an explicit phrase lexicon entry
before falling back to individual words. It preserves English sentences and
explicit English title contexts under the existing context-routing policy.

```text
Bank of America는 은행입니다.
→ 뱅크 오브 아메리카는 은행입니다.

Adobe Premiere Pro를 사용합니다.
→ 어도비 프리미어 프로를 사용합니다.

Please check Adobe Premiere Pro.
→ Please check Adobe Premiere Pro.

제목은 Adobe Premiere Pro입니다.
→ 제목은 Adobe Premiere Pro입니다.
```

These are pronunciation renderings, not translations. The feature is active for
foreign spans under `TTF_PHONETIC_LOCALIZE`. It does not replace text globally or
change literal code/path preservation.

## Resource and API format

The existing pronunciation lexicon schema accepts a phrase in its key:

```text
lexicon  ko-KR  en-US:bank of america  뱅크 오브 아메리카
lexicon  ko-KR  en-US:after effects    애프터 이펙츠
```

Actual files use **tabs between the four columns**. Phrase words in a resource
key use spaces; an unescaped tab is a TSV column delimiter. ASCII case is folded
and repeated internal horizontal spaces are canonicalized. Input text may use
ASCII spaces or tabs between words. Output uses the exact dictionary reading,
including the reading's own spaces.

The 12 initial phrase choices live in three category files under
`resources/languages/ko-KR/lexicon/multiword/`:

| Category | Entries |
|---|---|
| Finance | Bank of America, Goldman Sachs, Morgan Stanley |
| Places/media | New York, Wall Street, New York Times |
| Content/products | After Effects, Adobe Premiere, Adobe Premiere Pro, Final Cut Pro, DaVinci Resolve, Apple Watch |

These are original project seed choices. No external dictionary was imported,
and the choices have not received an independent pronunciation review.

User overrides use the existing API before freezing the engine:

```c
ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR",
                "After Effects", "커스텀이펙트");
/* Canonical case/spacing identifies the same key. */
ttf_lexicon_remove(engine, TTF_LAYER_USER, "en-US", "ko-KR",
                   "AFTER\tEFFECTS");
```

API keys accept spaces or tabs between words. Leading/trailing horizontal
whitespace is rejected. Engine setup copies the strings; callers need not retain
those buffers. Add/remove after freeze returns `TTF_ESTATE`. Removing a layer's
entry reveals any lower-priority pronunciation.

## Matching and precedence

At a lexical start, selection is deterministic:

1. User > Customer > Domain > System > Common.
2. At the same layer, select the longest complete phrase.
3. Existing resource rules resolve duplicate canonical keys.

Layer priority also applies when a **shorter phrase or a single word** competes
with a longer lower-layer phrase. A user `Apple → 사용자애플` therefore beats the
system `Apple Watch` phrase and leaves `Watch` to normal per-word processing.
A customer `Adobe Premiere` beats a system `Adobe Premiere Pro`; a customer
`Adobe Premiere Pro` beats the shorter customer entry.

This concerns pronunciation lexicons. TN rules remain a separate subsystem.
Phrase entries are indexed by their first word when the engine freezes. Both
phrase lookup and the streaming planner consult the immutable engine resources;
no language-pair converter or global mutable state is introduced.

## Bounded grammar

The initial matcher accepts:

- Two through eight whole lexical words from one script category.
- Canonical phrase keys up to 95 UTF-8 bytes, excluding the source-language prefix.
- Original matched input extents up to 255 bytes, including internal whitespace.
- ASCII case folding and ASCII spaces/tabs between words.

Digits, punctuation, line breaks, other Unicode whitespace and protected literal
tokens form barriers. There are no partial-word matches: `Alpha Beta` does not
match `Alpha BetaMax` or `XAlpha Beta`. Hyphens/apostrophes inside multiword keys
are outside this prototype. Single-token dictionary behavior remains available
for existing spellings with punctuation.

Malformed multiword API keys return `TTF_EINVAL`; invalid resource rows return
`TTF_ERESOURCE`. Full Unicode case folding, morphological inflection, fuzzy
matching, arbitrary whitespace and open-ended named-entity recognition are not
implemented. Pronunciation data must name the phrases that should coalesce.

## Semantic metadata and compatibility

A matched foreign phrase is one output span:

```text
surface        = Bank of America
semantic_type  = WORD
value          = bank of america
source_lang    = en-US
context_lang   = ko-KR
render_lang    = ko-KR
rule           = lexicon.phrase
lexicon        = lexicon:system
resource_layer = system
pronunciation_layer = system
```

Source and output offsets remain UTF-8 byte offsets. The full original source
extent includes the phrase's spaces/tabs; its normalized text is the dictionary
reading. English/title contexts preserve the original text and keep their
ordinary per-word metadata.

The public ABI and JSON protocol remain at version 1. That compatibility does
not promise that every source word is always a separate output span. In
particular, the new system `Apple Watch` entry intentionally changes two old
word spans into one phrase span while keeping `애플 워치` text unchanged. The
historical `domain_cases.jsonl` is retained unchanged; `domain_cases_v8.jsonl`
records the explicit metadata migration. This change must not be counted as a
text-normalization improvement or silently hidden by rewriting historical data.

## Streaming and callback lifetime

A known first-word prefix can keep whitespace pending until the bounded phrase
candidate succeeds or fails. This prevents `After ` from being committed before
`Effects를` arrives, including with tiny phrase targets. The 255-byte source
bound prevents unlimited phrase lookahead; the existing overall pending-byte
limit still applies. A completed safe sentence/phrase emits a callback before
EOF, and flush commits the remainder.

Dictionary phrase spans preserve exact source offsets across arbitrary chunk
boundaries, including boundaries inside UTF-8 characters. Callback pointers
remain borrowed for the callback duration. Consumers must copy text and metadata
before queuing asynchronous work. The native test copies both, destroys the
stream and engine, then compares the retained copies.

## Evidence

`tests/evaluation/multiword_v8.jsonl` contains 30 synthetic development references,
frozen before either baseline or candidate execution:

```text
SHA-256 5e8b9d4082e33cfbb7e94f4e79546626424a9587a7e35a95b25e9c9d86787544
```

| Check | v0.7 baseline | v0.8 candidate |
|---|---:|---:|
| Text + asserted metadata | 15/30 | 30/30 |
| Output text | 17/30 | 30/30 |
| Character edit distance / 643 reference characters | 71 | 0 |
| Batch/stream mismatches | 0 | 0 |

The baseline uses the frozen v0.7 binary and resources, including the absence of
these new phrase entries. Additional CLI tests cover metadata, both endpoints
of the 255-byte limit, case/spacing, barriers, layer selection and tiny targets.
Native tests exercise user add/remove, all two-chunk byte splits for representative
sentences, preservation policy, callback lifetime and pre-flush delivery. Tests
invoke the CLI/native executable and can run under ASan/UBSan.

This measures a bounded feature on development examples. It does not estimate
real customer coverage, independently reviewed pronunciation accuracy, or
synthesized speech quality. Evaluation remains output text and metadata only.
