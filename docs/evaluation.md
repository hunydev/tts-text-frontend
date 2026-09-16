# Text and listening evaluation

This integration checkpoint produces normalized text, inspectable language routes,
and optional speech audio from the streaming callbacks. It does not assign an
automatic pronunciation score. Text equality, successful WAV generation, and a
human assessment of a voice answer different questions.

## Reproduce a run

Build the CLI, then run the Python standard-library evaluator from the repository
root. The output directory must not exist; use a different directory for each run.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
python3 tools/evaluation/evaluate.py --ttf build/ttf --resources resources \
  --corpus tests/evaluation/cases.jsonl --output-dir artifacts/evaluation-text
```

To create audio, install the optional eSpeak backend described in the TTS bridge
documentation and add `--synthesize`:

```sh
python3 tools/evaluation/evaluate.py --ttf build/ttf --resources resources \
  --corpus tests/evaluation/cases.jsonl --output-dir artifacts/evaluation-audio \
  --synthesize
```

The evaluator invokes `ttf stream-json` with one input byte per write, including
splits inside Hangul UTF-8 sequences. It checks that the event sequence completes,
that byte offsets cover the input and output, and that concatenated normalized
segments equal each independently specified expected string. The separate native
streaming tests exercise batch parity across arbitrary and random chunk sizes.

The corpus contains the original mixed ordinal acceptance case, Korean and English
Apple contexts, initialisms, brands, an unknown word, embedded English, counters,
dates, phone numbers, currency, percent, and measures. Expected strings define
current deterministic behavior; they are not evidence that a particular acoustic
model pronounces the result correctly. Dictionary-removal and emergency grapheme
fallback checks remain in the native API tests because the CLI does not mutate
the lexicon.

## Artifacts

- `report.json`: exact text matches, per-case source/normalized/expected strings,
  pronunciation paths, fallback span counts, source/context/render routes, and
  pending human assessments. It also records the CLI, corpus, and resource SHA-256
  hashes so input data can be identified later.
- `report.html`: escaped input text and metadata, audio players when requested,
  and blank listening score controls. The page contains no executable script.
- `review.csv`: a blank score sheet to edit and save. Scores in the HTML page are
  scratch notes and are not automatically persisted.
- `<case-id>/input.txt` and `segments.jsonl`: exact inputs and protocol events.
- `<case-id>/audio/`: optional bridge manifest, per-run audio, and `combined.wav`.

The CLI prints counts as JSON. Exit status is 0 for all text matches, 1 for text
mismatches (reports are still written), and 2 for invalid input, failed subprocesses,
or incomplete protocol/audio artifacts. Existing output directories are refused.
A failed run may leave partial evidence; it is never presented as a completed report.

`fallback_spans` counts paths containing `prototype` or `fallback`. Lexicon hits,
pronunciation dictionary hits, initialism paths and exact path names remain visible
in each case. Source/context/render route counts include neutral spans; they are
not counts of acoustic voice switches. Empty pronunciation paths mean no foreign
pronunciation transformation was needed. No content is sent to an external API.

`frontend_process_seconds` measures the entire CLI subprocess, including startup
and deliberately tiny input writes. It is not a first-audio or production throughput
benchmark. A real service must measure source arrival, first committed callback,
backend request, first audio byte, playback, and queue backpressure separately.

## Human review

Listen with the intended Korean and English voices. For each case record reviewer,
pronunciation correctness, intelligibility and naturalness on a 1–5 scale, plus
the exact word or boundary that needs work. Leave scores empty until somebody
has listened. Pay particular attention to the `1st` context change, brand names,
initialisms, prototype G2P, unit readings, and pauses at language/segment changes.

eSpeak is a reproducible local integration backend. Its output is useful for
checking the text/audio handoff and voice routing. Production readiness depends
on evaluation with the deployment voices and domain corpus; no such quality
approval is inferred from a passing text test or the existence of generated audio.

Next work should select deployment voices, review this corpus, add real domain
failure cases, improve pronunciation resources from those findings, and measure
first-audio latency under sustained input. Preserve the corpus and resource hashes
with each listening decision so comparisons remain traceable.
