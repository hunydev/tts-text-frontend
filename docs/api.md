# Public C API

Include `tts_text_frontend/ttf.h`. Pack integrations additionally include
`tts_text_frontend/ttf_language_pack.h`. C++ callers use the provided `extern "C"`
guards. All public symbols begin with `ttf_`; internal helpers are hidden in shared
builds. ABI version and descriptor structure sizes are checked at setup.

## Lifecycle

1. `ttf_config_init(&config)` sets defaults and ABI markers.
2. `ttf_engine_create(&config, &engine)` loads builtin packs and system resources.
3. Optionally register packs, load resources, and add/remove lexicon entries.
4. `ttf_engine_freeze(engine)` finalizes immutable setup.
5. Call `ttf_normalize` concurrently, or create request-local streams.
6. Free results and destroy all streams, then destroy the engine.

Configuration copies used internally belong to the engine. Set a resource root for
an installed/relocated package. Configuration and packs should be fully initialized
with current structure sizes; this initial release requires matching ABI versions.
ABI v1 freezes existing public function signatures and structure layouts after this
initial release. Incompatible changes require a new major ABI, API version and
shared-library SOVERSION; they must not silently alter v1. The project release
version is declared in `CMakeLists.txt`; the shared-library SOVERSION is 1. New extensible capabilities
should use additional APIs or explicitly versioned descriptors. Automated ABI
comparison in release CI remains future work.

| API | Ownership / result |
| --- | --- |
| `ttf_engine_create`, `ttf_engine_destroy` | Caller owns opaque engine lifetime |
| `ttf_engine_freeze` | Ends all engine mutations |
| `ttf_normalize` | Input caller-owned for the call; output result owned by caller |
| `ttf_result_free` | Frees text/spans; use for every successful result |
| `ttf_resource_load` | Reads a file; engine owns parsed records |
| `ttf_lexicon_add`, `ttf_lexicon_remove` | Setup-only mutation; strings are copied |
| `ttf_stream_create`, `ttf_stream_destroy` | Caller owns request; engine must outlive it |
| `ttf_stream_write` | Copies pending input; caller can release bytes after return |
| `ttf_stream_flush` | Final commit; closes logical request |
| `ttf_tokenize`, `ttf_tokens_free` | Caller frees allocated token array |
| `ttf_engine_register_pack` | Descriptor/callbacks/userdata caller-owned, outlive engine |
| `ttf_resource_get` | Underlying engine owns returned strings; setup mutations may invalidate pointers; after freeze valid until that engine is destroyed |
| `ttf_buffer_append`, `ttf_buffer_puts`, `ttf_buffer_free` | Helpers for pack output |

`ttf_result_t` should be zero-initialized. Free a result before reusing its storage.
Normalize reports status separately from the result. Byte lengths are explicit;
text is UTF-8. Span boundaries are byte offsets, never character indexes. Output
text is NUL-terminated in addition to carrying its byte length.

## Frontend input and output options

Include `tts_text_frontend/ttf_frontend_options.h`. This is an additive API;
`ttf_config_t`, result, span, trace and language-pack ABI v1 layouts are unchanged.
Initialize `ttf_frontend_options_t` with `ttf_frontend_options_init`, edit the
fields and call `ttf_engine_set_frontend_options` **before freeze**. The setter
copies the fields; no caller pointers are retained. Setup requires caller
synchronization. Frozen engines reject changes with `TTF_ESTATE`.

The CJK options extension accepts the v0.20 prefix size through the current size,
with ABI version 1. Missing or incomplete trailing fields use their defaults;
larger sizes are rejected. The historical `ttf_frontend_options_init` symbol writes
only the v0.20 prefix, so an already-compiled caller's allocation remains safe.
New headers route initializer calls to `ttf_frontend_options_init_sized(options,
sizeof(ttf_frontend_options_t))`; FFI callers should use that sized entry point.
The sized initializer ignores null pointers and sizes outside the accepted range.

| Field | Initializer default | Other choice |
|---|---|---|
| `input_format` | `TTF_INPUT_PLAIN` | `TTF_INPUT_ANNOTATED` |
| `output_mode` | `TTF_OUTPUT_SPEECH` | `TTF_OUTPUT_TEXT` |
| `unreadable_policy` | `TTF_UNREADABLE_PRESERVE` | `TTF_UNREADABLE_DROP` |
| `max_control_bytes` | 4096 | Positive total bytes per annotated control |
| `hanja` | `TTF_HANJA_PASSTHROUGH` | `TTF_HANJA_KOREAN` |
| `kana` | `TTF_KANA_PASSTHROUGH` | `TTF_KANA_KOREAN` |

Han/Kana Korean rendering is opt-in, independently per script, in Korean context.
Both flags default to passthrough. Source language remains `und`; successful
readings use existing WORD IR, Korean render language and resource/initial-variant
trace events. No semantic enum or language-pack ABI was added.

```c
ttf_frontend_options_t options;
ttf_frontend_options_init(&options);
options.hanja = TTF_HANJA_KOREAN;
options.kana = TTF_KANA_KOREAN;
/* Apply before ttf_engine_freeze(). */
ttf_engine_set_frontend_options(engine, &options);
```

Freeze first loads only the enabled CJK system tables, then moves all six CJK
resource kinds (including explicit pre-freeze overlays) into one engine-owned
heap index. Both passthrough and legacy callers never open the system CJK files.
A failed freeze rolls back deferred rows and declaration numbers; callers may
correct the resource/options and retry. Public loading after freeze is unchanged
(`TTF_ESTATE`). `ttf_resource_get` and resource validation include all currently
loaded rows: before freeze this excludes deferred system tables, afterwards it
includes the enabled tables. See the counts in [resources](resources.md#cjk-korean-readings).
Returned values remain engine-owned until destroy. Immutable request
views borrow the index. No per-request copy of the index or mutable global state
is introduced. See the [CJK design](cjk-korean-reading.ko.md),
[resource authoring](resources.md#cjk-korean-readings) and [handoff](HANDOFF.md).

An engine without the setter retains legacy plain/text/preserve behavior. CLI
also defaults to text; the web workbench selects speech explicitly. Unknown Han
and emoji preservation is the default in every mode. Drop applies only to the
speech character pass. Protected literals and explicit aliases retain their
literal contract; malformed/ambiguous semantic objects remain visible with a
trace reason instead of being reinterpreted digit by digit.

Annotated controls produce `CUSTOM` spans over the complete original syntax,
including braces. Literal aliases are decoded once and are not normalized again.
Pattern source values fit the existing 95-byte IR value; literal aliases may
be longer within the configured control and pending-buffer limits. Dropped
characters retain source spans with zero output length. Trace `final-*` policy
part offsets refer to the pre-filter rendered text within that span; original
`input_begin/end` and final `output_begin/end` remain the span boundaries.

See [input format](input-format.md) and [speech resources](speech-resources.md).

## Errors

`TTF_OK` indicates success. Other statuses describe invalid arguments, allocation
failure, invalid UTF-8, I/O failure, malformed resources, lifecycle misuse,
configured limits and callback cancellation. Check every fallible API call.
`ttf_status_string` provides a stable human-readable diagnostic category.
A failed setup call does not authorize use of an uninitialized output handle.

## Thread safety

Frozen engines contain immutable resources and can serve concurrent normalizations
and separate streams. The test suite runs eight threads issuing 800 requests against
one engine. This verifies the exercised paths, not a formal proof of race freedom.
Setup mutation, freeze and destruction require caller synchronization. Each stream
is single-owner and must not receive concurrent writes; callback reentry into the
same stream is unsupported. Pack callbacks must meet the same reentrancy contract.

No global mutable language state is needed. To update resources during service,
create and freeze a replacement engine, route new requests to it, and retain the
old engine until its streams/requests finish. Callback data must be copied before
queueing asynchronous TTS work.

## Language-pack callback ownership

The engine pointer supplied to a pack callback is borrowed. Ordinary
`context_score` callbacks and callbacks during traced normalization may receive a
request-local view rather than the application's original handle. Do not retain
or destroy the callback's engine pointer, or use its address as a persistent
identity. The application must keep the underlying engine alive for every active
request and stream.

This view does not own the resource strings returned by `ttf_resource_get`. After
freeze, those strings remain valid until the underlying engine is destroyed,
including after the callback returns. Input text and token pointers remain borrowed
callback data. Public signatures and ABI v1 layouts are unchanged; request caches
are internal and do not mutate the shared frozen engine.

## Resource validation extension (v0.16)

Include `tts_text_frontend/ttf_resource_validation.h` for additive `ttf_engine_create_ex`,
`ttf_resource_load_ex`, `ttf_engine_validate_resources`, and
`ttf_engine_register_resource_validator`. Existing ABI v1 layouts are unchanged.
File loads check row schemas transactionally; freeze checks effective dependencies
and constraints. Initialize optional caller-owned diagnostic/report outputs using
the corresponding init helpers. See [contracts and C example](resource-validation.md).

## Detailed pronunciation trace extension (v0.17)

Include `tts_text_frontend/ttf_trace.h`. `ttf_normalize_traced` returns owned text,
spans and copied events; `ttf_traced_result_free` releases all of them together.
`ttf_stream_create_traced` uses the existing write/flush/destroy API and passes
borrowed text/spans/events to the callback. Copy these before returning if retained.
The original engine must remain frozen and alive for the request/stream lifetime.

Options default to 65,536 events and 16 MiB logical event/string payload per request,
including rejected attempts and already committed segments. Limits fail explicitly
without a partially described current segment. Earlier callbacks remain committed.
These limits do not cap peak allocator memory. Existing ABI v1 layouts are unchanged.
Source/target resource events and generated decisions have distinct provenance;
output offsets identify whole spans, not syllable alignments. See the
[detailed contract and runnable examples](detailed-pronunciation-trace.md).
