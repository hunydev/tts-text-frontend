# Streaming and stable commitment

An engine is immutable after freeze. Every stream owns a pending byte buffer,
commit cursor and first/subsequent segment state. `write` accepts arbitrary bytes,
including incomplete UTF-8 code points. `flush` validates and commits the remaining
input, then finishes the request; do not use a flushed stream for a new request.
Destroy does not implicitly flush.

For `가격은 12` followed by `,500원입니다.`, the first write cannot commit `12`.
The pending lexical unit is held until a stable boundary or final flush. UTF-8
validation rejects malformed sequences and a final incomplete sequence.

Batch normalization and streaming share the lexical boundary planner and the same
normalization path. Therefore:

```text
batch(input, configuration, resources)
    == concatenate(callback.text for any byte chunking of the same input)
```

The invariant includes spaces and newlines. Tests cover every single two-chunk byte
split for representative inputs, one-byte feeds, randomized chunk sizes, randomized
mixed-token corpora and alternate segment targets. Splits inside multibyte Hangul,
`12,500`, URLs, decimal points, versions and dotted dates are exercised.

## Segmentation configuration

| Option | Purpose |
| --- | --- |
| `first_segment_target` | Default 48; early phrase target to reduce first callback latency |
| `normal_segment_target` | Default 112; larger target for later throughput |
| `max_segment_length` | Default 200; character target for forcing a safe phrase boundary |
| `sentence_split` | Emit safe sentence boundaries |
| `phrase_split` | Emit safe phrase boundaries before a sentence ends |
| `max_pending_bytes` | Default 1 MiB; hard safety limit for an unbreakable pending unit |

A target is measured in source Unicode characters, not UTF-8 bytes. Atomic tokens
can exceed a segment target; the maximum is a soft boundary preference rather than
permission to split a numeric token or UTF-8 character. The pending byte cap returns
`TTF_ELIMIT` when safe progress is impossible within the configured limit.

Sentence detection uses the shared lexical scan and structural checks. Periods in
numbers, dates, versions, URLs and configured abbreviations do not automatically
commit. Sufficient lookahead is required: the end of a write is not evidence of a
sentence end. `flush` explicitly supplies final-input evidence.

Context detection is local to stable planned units. Shorter phrase targets can
change contextual readings compared with longer targets; batch and stream remain
equal for the same configuration. This bounded context is the initial tradeoff for
incremental commitment. [Frontend benchmarks](benchmark.md) measure actual callback
timing; no absolute latency guarantee is claimed.

## Telephone and identifier commitment (v0.18)

The classifier and boundary planner share a language-neutral phone scanner. At a
possible cut the planner examines only the available prefix; later bytes in a
larger write cannot retroactively justify a different earlier cut. Telephone
groups, parentheses and configured extension markers remain pending together,
including the period in `ext.`. A newline ends extension scope. Incomplete or
malformed phone-shaped input is preserved instead of reading its first digits as
a quantity and its remainder as another semantic value.

Resource-defined phone and identifier labels retain their scope across phrase
targets. Disabled longer labels also retain scope so a cut cannot expose and
reactivate an enabled shorter suffix. A label being retained does not enable it.
Ordinary parentheses around quantities or dates do not establish a phone number.
See [phone policies](phone-resources.md) for the supported forms and editable
reading labels. These atomic spans can exceed a soft segment target; the pending
byte limit still applies.

Callbacks run synchronously during `write`/`flush`; the caller controls pacing by
returning from the callback. A nonzero callback return cancels the request with
`TTF_ECALLBACK`. A real TTS queue should copy segment data if synthesis continues
after the callback. The library does not supply asynchronous queues or backpressure
scheduling. Do not reenter, write, flush or destroy the same stream from its callback; the
engine and stream must outlive callback execution.

Segment text and span arrays are valid only during the callback. Output offsets
are local to the segment; input offsets refer to the original stream. An error after
prior callbacks cannot retract already delivered text. Treat a failed stream as a
failed request and destroy it.

## Semantic spaces and quotation scopes

Horizontal whitespace following numbers, fraction separators, signs or configured
currency prefixes is retained with lookahead. This allows `3 kg`, `1 / 2` and
`₩ 12,500` to classify as one semantic span even with a one-character phrase target.
The scanner does not commit a numeric prefix because a write happened to end there.
Pack `numeric_continuation` resources also retain calendar/scale markers following
a number, so even invalid `2026년 2월 29일` remains a complete candidate under tiny
phrase targets.

Paired ASCII double quotes, curly double quotes, guillemets and corner quotes
create independent context windows. Their content is retained until the matching
closer so trailing Korean text cannot change the last English word inside a quote.
Up to 32 levels of nesting are tracked. ASCII apostrophes remain lexical punctuation
for English contractions; they do not open a quotation scope. Unclosed quotations
wait until final flush or the pending-byte limit. This improves text correctness
but may delay the first callback for long quoted passages; maximum segment length
is a soft target for these indivisible scopes too.

## Resource cues and bounded title retention (v0.5)

The boundary planner shares `identifier_label` and `context_cue` resources with
classification/routing. It retains partial multiword cue prefixes and cue/value
boundaries without requiring future bytes. A reference label and leading zeros
therefore remain in one context window even with targets of one/three codepoints.
Cue state is advanced within the existing forward scan rather than searching the
entire preceding lexical run separately at every whitespace.

Title cues may retain at most `max(max_segment_length, 256)` codepoints of the
current unit before ordinary phrase cuts resume; this is a soft limit at a safe
whitespace boundary. It trades scope on exceptionally long titles for bounded
first-callback delay. Identifier cues still follow their exact numeric scope and
IR limits. The pending-byte hard cap remains authoritative for indivisible text.

## Protected source spans and incremental validation (v0.6)

The shared protected token view prevents sentence/phrase cuts inside recognized
code, comments, tag attributes and paths. The same lexical recognizers produce
identity TEXT spans during normalization. Open literals wait for a closing boundary
or final flush; raw tails survive finalization. They remain subject to the hard
pending cap, and may exceed soft segment targets.

Streams retain a UTF-8 validation cursor and a private incomplete-literal hint.
Safe ordinary body bytes do not trigger another full lexical scan; possible
closers or syntax changes do. Undecided prefixes use conservative scanning.
Normal prose still plans on each write, so the optimization does not batch its
sentence callbacks. UTF-8 errors are validated before any cache skip. Hints are
cleared after commitment and belong to a single stream, never a shared engine.

Long opaque content can still delay output until its delimiter and require the
pending buffer. Inputs dense with delimiter candidates may still cause repeated
scans; the cache is not a general linear-time document parser. Public callback,
error and ownership contracts are unchanged. See [protected text](protected-text.md).
