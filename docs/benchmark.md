# Frontend commitment benchmark

`ttf_bench` measures the synchronous text frontend through the public C API. The
first callback means that a normalized segment is available for TTS. It is
**frontend commit latency, not audio TTFB**: there is no synthesis, device, network,
audio queue, or input-arrival pacing in this tool.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/ttf_bench --resources resources --iterations 20 --chunk-bytes 64 \
  --text 'Apple은 사과입니다. Please go to the 1st floor.'
./build/ttf_bench --resources resources --iterations 100 --chunk-bytes 16 \
  --first-target 32 --normal-target 96 --max-length 200 input.txt
```

Supply exactly one UTF-8 file or `--text TEXT`; `-` reads standard input. Files are
loaded before timing. Empty input, malformed/incomplete UTF-8, embedded NUL,
invalid options, and resource/stream errors return failure and a diagnostic on
stderr, without a success JSON report. Numeric options require positive integers.
Defaults are 20 iterations, 64-byte writes, and the normal frontend segmentation
defaults: first target 48, subsequent target 112, maximum target 200 characters.
Targets count source Unicode characters; write sizes count bytes and can split
UTF-8 code points. `--resources DIR` selects the resource manifest root.

## Work performed

1. Load input and allocate timing arrays outside measurements.
2. Measure engine creation, resource loading and freeze once as `engine_setup_ms`.
3. Normalize the full input once as an untimed reference. Feed one untimed stream
   with the requested chunk size and compare each callback's bytes directly with
   the matching part of the reference. Also verify total length and FNV-1a hash.
4. Free the reference result before timed iterations. Each iteration creates a new
   stream, starts the timer immediately before feeding, sends the entire in-memory
   input without delays, and flushes. Stop timing after flush returns. Stream
   creation/destruction, reference generation and result reporting are excluded.
5. For every timed run, compare output length, incremental 64-bit FNV-1a hash,
   callback count, and first-callback feed/commit position with the preflight.
   Fail on a mismatch. Timed callbacks do not retain or concatenate output.

Every timed run includes the callback's byte hashing and accounting overhead, and
one clock read at first callback entry. First callback timing is captured before
hashing that segment. Hash comparison alone has a theoretical collision risk; the
separate untimed preflight uses exact byte comparison. This is a repeated warm
workload: the engine is frozen once and the preflight already exercised the text
and resources. It does not represent a cold request or per-request engine setup.

The complete input and, temporarily, the batch reference text/span metadata are in
memory. Timed callback storage is constant size, but this tool is not an unbounded
input reader or a peak-memory benchmark. Streams retain their usual pending buffers.

## JSON fields

The output is a single JSON object with `schema_version: 1`.

| Field | Definition |
| --- | --- |
| `metric` | `frontend_commit_latency_not_audio_ttfb` |
| `clock` | Platform clock used for all measurements |
| `input_bytes`, `output_bytes` | UTF-8 lengths of the source and normalized output |
| `iterations`, `chunk_bytes` | Repetition count and maximum bytes supplied per write |
| `first_segment_target`, `normal_segment_target`, `max_segment_length` | Segmentation configuration in source characters |
| `engine_setup_ms` | One engine create + resource load + freeze measurement |
| `callbacks` | Validated, deterministic callback count per iteration |
| `total_callbacks` | `callbacks * iterations` |
| `first_callback_input_bytes` | End byte offset of the write being submitted when the first callback fires; full input length if it fires in flush |
| `first_callback_committed_input_bytes` | First segment's committed source end offset from callback metadata |
| `first_callback_before_final_input_runs` | Runs whose first callback fires while the caller still has bytes it has not passed to `write` |
| `first_callback_before_flush_runs` | Runs whose first callback fires during `write`, before final flush |
| `first_callback_ms` | Elapsed feed-start to first callback entry: min, p50, p95, max |
| `total_ms` | Elapsed feed-start to flush-return: min, p50, p95, max |
| `throughput_mib_s` | `(input_bytes * iterations / 1048576) / sum(total_seconds)`; null if measured total is zero at clock resolution |
| `validation` | `exact_preflight_and_fnv1a64_each_iteration` |
| `output_fnv1a64` | Sixteen lowercase hexadecimal digits for the complete normalized output hash |

Percentiles use the nearest-rank convention: sort N values and select
`ceil(percentile / 100 * N)`, numbered from one. No interpolation or minimum
hardware speed is assumed. The full JSON configuration should accompany results.
Also record compiler/build type, revision, resource revision, host/CPU, operating
system, workload, background load and power settings when comparing runs.

`first_callback_input_bytes` describes the caller's submitted write extent. The
frontend can internally process a large write in smaller pieces; that field does
not claim how many bytes the implementation had consumed when it called back.
An early callback before unsubmitted input is positive evidence of incremental
delivery. Zero early callbacks is normal for short inputs, a large single write,
or text lacking a safe boundary. It is not itself an error or a speed measurement.

POSIX uses `clock_gettime(CLOCK_MONOTONIC)`. Windows uses
`QueryPerformanceCounter`/`QueryPerformanceFrequency`. Other C11 platforms fall
back to `timespec_get(TIME_UTC)`, which is a wall clock and can be adjusted by the
system; the emitted clock name identifies this weaker basis. Detected backward
movement fails the run. Small runs can be below clock resolution; larger text and
more repetitions make comparisons more useful.

The smoke test verifies JSON accounting, known normalized output hashes, actual
callbacks before the last input chunk, one-byte UTF-8 feeds, and invalid input.
It sets no hardware latency thresholds. Benchmark results establish neither audio
quality nor a production latency SLA. End-to-end audio TTFB needs a real TTS engine
and the actual request transport and playback pipeline.
