# Streaming CLI protocol, version 1

`ttf stream-json` exposes the existing streaming callback as newline-delimited
UTF-8 JSON. An external TTS adapter can consume each segment immediately:

```sh
ttf stream-json --resources ./resources --chunk-bytes 4096 input.txt
cat input.txt | ttf stream-json --resources ./resources -
```

Standard output contains only protocol events. Diagnostics go to standard error.
Each event is one JSON object followed by LF, and stdout is flushed after every
segment. JSON string escaping preserves quotes, backslashes and controls without
creating extra protocol lines. Unicode text remains UTF-8.

## Segment event

```json
{"version":1,"event":"segment","sequence":0,"input_begin":0,"input_end":5,"text":"애플","spans":[{"begin":0,"end":6,"input_begin":0,"input_end":5,"script":"LATIN","source_lang":"en-US","context_lang":"ko-KR","render_lang":"ko-KR","semantic_type":"WORD","value":"","secondary":"","unit":"","rule":"identity","resource_layer":"common","pronunciation_layer":"system","lexicon":"lexicon:system","verbalizer":"en-US","context_trace":"en-US=8 ko-KR=260"}]}
```

The example illustrates the schema; actual context scores depend on the surrounding
input and resources. `sequence` starts at zero and increases for each segment.
`text` is ready for a text-input TTS backend. The normalized output is exactly the
concatenation of segment text, including spaces and newlines.

All offsets are **UTF-8 byte offsets**, with an exclusive end:

| Field | Meaning |
| --- | --- |
| Event `input_begin`, `input_end` | Original input range covered by this segment |
| Span `begin`, `end` | Range within this event's normalized `text` |
| Span `input_begin`, `input_end` | Absolute range within the original input stream |
| `script` | Scanner category, such as `LATIN`, `HANGUL`, or `DIGIT` |
| `source_lang` | Source language tag, or `und` for a neutral token |
| `context_lang` | Speech context chosen by deterministic routing |
| `render_lang` | Language of the rendered output |
| `semantic_type`, `value`, `unit` | Classification and primary semantic value |
| `secondary` | Optional since v0.5: range/fraction endpoint, AM/PM, or matched identifier cue |
| `rule`, `resource_layer` | Selected TN rule and its layer |
| `pronunciation_layer`, `lexicon` | Foreign pronunciation provenance and path |
| `verbalizer` | Selected verbalizer language tag |
| `context_trace` | Diagnostic context scores |

Readers must tolerate unknown additive fields and absent `secondary` from older
JSONL v1 producers. Native ABI layouts have not changed.

Consumers should use the dedicated language and semantic fields for dispatch;
`context_trace`, `rule`, and `lexicon` are diagnostic strings whose contents can
expand as resources and language packs improve. A segment can contain several
render languages. A multilingual backend may consume spans directly; a backend
with one voice per request should split on render-language changes while preserving
all text. The CLI serializes callback data synchronously, within their lifetime.

## Completion and failure

After successful EOF and stream flush, the CLI emits exactly one final event:

```json
{"version":1,"event":"end","segment_count":1,"input_bytes":5}
```

An empty input emits only this event with both counts zero. Consumers must observe
the `end` event **and** a zero process exit status before treating the request as
complete. An invalid or truncated UTF-8 sequence, unreadable input, resource error,
or output error produces a nonzero exit. Failure before completion produces no
`end` event; segments already delivered cannot be retracted. A broken output pipe
can also terminate the process through the operating system's pipe behavior. Do
not synthesize an error message from stderr as though it were normalized text.

## Input pacing and segmentation

| CLI option | Default | Purpose |
| --- | --- | --- |
| `--chunk-bytes N` | 4096 | Maximum bytes per input read and stream write |
| `--first-target N` | 48 | First phrase target, in source Unicode characters |
| `--normal-target N` | 112 | Subsequent phrase target |
| `--max-length N` | 200 | Soft maximum segment target; atomic tokens stay intact |

Every size must be a positive decimal integer representable by `size_t`; signs,
fractions, zero and overflow are rejected. The input buffer caps reads at 65536
bytes even if a larger chunk size is requested. `--chunk-bytes` applies only to
`stream` and `stream-json`; segmentation options also apply to `normalize` and
`inspect`. Batch and streaming comparisons must use the same segmentation settings.

For stdin, the CLI uses POSIX `read` or Windows `_read` so a pipe can deliver
available bytes without filling the requested chunk first. The producer must flush
its own output to make those bytes available. Regular files use buffered reads.
The native library remains portable C11 and does not depend on this CLI I/O layer.
Windows stdin and stdout are placed in binary mode for streaming.

Transport flushing does not force a frontend commit: incomplete tokens and
ambiguous boundaries remain pending until enough lookahead or EOF arrives.
Callbacks apply backpressure synchronously through the pipe when the consumer is
slow. The transport does not add an unbounded queue. Existing `ttf stream` keeps
its plain text format and now also flushes each callback to support pipes.

## Validation

`tests/integration/test_cli_json.py CLI RESOURCE_ROOT` checks JSON decoding and
control escapes, byte offsets and required metadata, source/context differences,
batch equivalence across read sizes and segment settings, empty input, malformed
UTF-8, invalid options, and a first callback arriving before stdin closes. It uses
only the Python standard library and the built CLI.

## Literal-source metadata (v0.6)

Protected spans are `TEXT` with `unit` equal to `code`, `markup` or `path`.
Source/context/render tags are `und`, `verbalizer` is `identity`, and the diagnostic
rule starts with `protected.`. Text and delimiters are byte-identical to the
source interval. Consumers can apply a downstream policy for these literal spans;
the frontend does not define spoken-code/path rendering. No JSONL schema or native
ABI version change is required. See [protected text](protected-text.md).

## Optional detailed pronunciation events (v0.17)

`ttf stream-json --trace` adds `trace_version: 1` and a `trace` array to each segment.
Omitting the flag preserves the original protocol fields. Events contain consumed
source/target resource rows, selected/rejected disposition, and generated decisions
with explicit unknown provenance. Input offsets are absolute; output offsets are
relative to the segment and cover the whole normalized span, not individual phones.

`--trace-max-events N` and `--trace-max-bytes N` require `--trace` and positive sizes.
They bound cumulative request evidence, including rejected attempts and previously
committed segments. Overflow fails without an end event; earlier callbacks remain
committed. The byte limit measures logical event/string payload, not peak allocation.
See [the trace schema and ownership guide](detailed-pronunciation-trace.md).
