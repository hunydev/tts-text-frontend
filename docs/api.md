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
| `ttf_resource_get` | Engine-owned; setup mutations may invalidate pointers; after freeze valid until engine destruction |
| `ttf_buffer_append`, `ttf_buffer_puts`, `ttf_buffer_free` | Helpers for pack output |

`ttf_result_t` should be zero-initialized. Free a result before reusing its storage.
Normalize reports status separately from the result. Byte lengths are explicit;
text is UTF-8. Span boundaries are byte offsets, never character indexes. Output
text is NUL-terminated in addition to carrying its byte length.

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
