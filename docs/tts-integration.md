# TTS engine integration and audio evaluation

The C frontend commits normalized text and span metadata. An acoustic TTS engine
receives that text through a callback or the optional JSONL CLI transport.
The reference Python adapter turns committed segments into real PCM WAV files.
It has an eSpeak NG backend and a configurable external-command backend.
Neither Python nor a speech engine is a dependency of the C library.

## Offline example

Install the optional backend using your system package manager. On Ubuntu:

```sh
sudo apt-get install espeak-ng
espeak-ng --voices
cmake -S . -B build
cmake --build build --parallel
build/ttf stream-json --chunk-bytes 1 input.txt > segments.jsonl
python3 tools/tts_bridge/bridge.py --input segments.jsonl --output-dir artifacts/audio
```

For a live pipeline, the adapter consumes one committed segment at a time:

```sh
set -o pipefail
build/ttf stream-json --chunk-bytes 64 input.txt |
  python3 tools/tts_bridge/bridge.py --output-dir artifacts/live-audio
```

The output directory must be new. It contains `audio-00000.wav`, subsequent audio
runs, `combined.wav`, and `manifest.json`. A failed or truncated frontend stream
produces a failed manifest; previously produced audio cannot be retracted. Consumers
must check `status == "complete"` before treating the complete request as successful.
Individual WAVs become available before the whole input completes. The combined
file is assembled at the end without resampling or inserting additional silence.

The default voice mapping is `ko-KR → ko`, `en-US → en-us`. A JSON object supplied
through `--voice-map voices.json` can replace it. Unmapped languages fail explicitly.
The adapter selects **render language**, so `Apple → 애플` is sent to the Korean
voice despite its English source language. Adjacent spans with the same render
language are combined within a committed segment. Whitespace-only runs generate
no audio. Pronunciation boundaries can sound unnatural when an engine is restarted
for each run; this adapter is a functional reference, not a production prosody model.

Upstream documentation: [eSpeak NG command-line guide](https://github.com/espeak-ng/espeak-ng/blob/master/docs/guide.md)
and [supported languages](https://github.com/espeak-ng/espeak-ng/blob/master/docs/languages.md).
Use your installed `--voices` output to confirm available voices.

## Another engine

An external adapter process receives literal normalized UTF-8 text on stdin. It
must write an uncompressed PCM WAV to the requested path and exit nonzero on failure.
Create a JSON configuration using an argument array, for example:

```json
{"argv":["/absolute/path/to/my-tts-adapter","--language","{language}","--output","{output}"]}
```

```sh
python3 tools/tts_bridge/bridge.py --input segments.jsonl --output-dir artifacts/custom-audio \
  --backend command --command-config adapter.json
```

Only `{language}` and `{output}` are substituted. No shell is invoked and text is
never assembled into a command line. The default per-call timeout is 30 seconds
(`--timeout` overrides it). All runs must return the same sample rate, sample width
and channel count; an engine adapter is responsible for resampling when necessary.
The bridge checks WAV headers, complete frame payloads and partial-frame tails
before accepting audio.

For direct C integration, copy callback text and metadata into an owned request if
synthesis continues after the callback. Use `semantic.render_lang` for acoustic
voice selection. Preserve `source_lang` and `context_lang` for diagnostics. Engine
and stream lifetimes follow the existing [C API contract](api.md).

## Timing and review

`manifest.json` records each synthesis duration, the time from bridge start to the
first received job, and the time until the first complete nonempty WAV is available.
These include acoustic backend subprocess startup and waiting for upstream data;
Python startup and bridge argument parsing occur before the timer starts. They are **not** the
first PCM byte from a streaming synthesis API. `ttf_bench` separately measures C
frontend callback latency with a warm frozen engine.

The adapter is synchronous: it stops consuming JSONL while synthesizing, so the
pipe supplies backpressure without an unbounded pending work queue. The reference
adapter retains completed job metadata and text until it writes the manifest, so
report memory grows with request size; it is not a constant-memory service. Backend
stderr is captured in memory. Production systems
should add cancellation propagation, bounded asynchronous queues, persistent
engine sessions, audio format conversion and voice/domain listening evaluation.
No perceptual quality score is inferred from a successful WAV or text comparison.
See [evaluation](evaluation.md) for corpus reports and listening checklists.
