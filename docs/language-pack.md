# Language packs

The public contract is `include/tts_text_frontend/ttf_language_pack.h`.
`ttf_language_pack_t` has a structure size, ABI version, language tag, caller-owned
userdata, and callbacks. The descriptor, callbacks and userdata must outlive the
engine. Callbacks must be reentrant once the engine is frozen.

| Hook | Meaning |
| --- | --- |
| `script_score` | Source-language evidence for a universal script category |
| `context_score` | Deterministic context evidence for a stable local unit |
| `classify` | Optional semantic recognition; returns consumed tokens, emits IR |
| `verbalize` | Render one semantic object using language reading policy |
| `pronounce` | Source text to shared Pronunciation IR |
| `render_phones` | Shared phones to this language's phonetic text |
| `render_graphemes` | Emergency target-language rendering |

An optional classifier extends universal recognition, never writes output text.
The `ttf_pack_input_t` view provides input bytes, lexical tokens, current index and
immutable engine resources. Output goes through dynamically growing
`ttf_buffer_append`/`ttf_buffer_puts`, returning allocation errors to the caller.

## Add a pack

1. Add `languages/ja-JP/pack.c`, exporting
   `const ttf_language_pack_t ttf_pack_ja_JP`.
2. Implement the callbacks needed by the language. Source pronunciation and target
   phone rendering are separate so the core requires no pair-specific branches.
3. Add resource files under `resources/languages/ja-JP/` and list them in
   `resources/manifest.ttfm`.
4. Reconfigure/build. CMake discovers `languages/*/pack.c` and generates registration.
5. Add independent golden and mixed-language tests.

Alternatively, an application can compile the pack separately and call
`ttf_engine_register_pack` before freezing. This requires no core rebuild. Shared
object discovery/loading is not implemented; the application owns loading and
unloading if it uses plugins. Resource-only changes need no recompilation.

The test `language_pack_extension` uses the public API to register `xx-Test`, routes
an Other Unicode script context, and verbalizes a universal number. It proves the
extension mechanism without claiming a complete Japanese pack.

Each pack exports its descriptor from `pack.c`. English date and abbreviation
grammars are separate modules, discovered by the same recursive CMake source glob. Split
classifier/verbalizer/G2P code into subdirectories as they grow; CMake already
includes pack source files recursively. Pronunciation and TN data are separate
under `resources/languages/<tag>/`.

## Optional policy validators (v0.16)

A pack may add `validate.c` exporting `ttf_resource_validator_<tag_with_underscores>`.
CMake registers this descriptor after its language pack. The separate additive
validation API preserves the original pack ABI. Row validators own language policy
schemas, final callbacks check effective dependencies, and unknown namespaces remain
explicitly unvalidated. See [resource validation](resource-validation.md) for ownership,
callback purity, priority and extension tests.

## Optional pronunciation instrumentation (v0.17)

`ttf_trace.h` adds `ttf_trace_record_resource` for actually consumed effective rows
and `ttf_trace_record` for generated decisions. Instrument `pronounce`,
`render_phones` and `render_graphemes` callbacks at consumption sites; do not record
speculative lookups as selected rows. Core supplies span/stage/part scope. A traced
callback receives a borrowed request-local engine view: never retain or destroy it.
Ordinary normalization records no events. Instrumentation errors remain sticky and
must not be hidden by a successful fallback. Existing pack descriptors need no change.
See [trace contracts](detailed-pronunciation-trace.md) for ownership and limits.
