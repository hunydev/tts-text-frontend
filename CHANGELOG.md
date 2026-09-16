# Changelog

## 0.2.0 — TTS integration and evaluation

* Versioned JSONL streaming transport with language metadata and early pipe output.
* Optional reference TTS bridge using eSpeak NG or a configurable external adapter.
* PCM WAV validation, per-language synthesis jobs and listening manifests.
* Evaluation corpus, text comparisons, fallback statistics and HTML listening reports.
* C frontend latency/throughput benchmark with warm-engine timing and batch parity.
* Additional protocol, adapter and benchmark tests; CI publishes evaluation audio.
* Existing C ABI v1 and library behavior are retained.

## 0.1.0 — Initial multilingual frontend

* C11 static/shared libraries, C ABI v1, KO/EN language packs and resource layering.
* Semantic and pronunciation IR, context-sensitive ordinals and foreign pronunciation.
* Deterministic batch/stream normalization, metadata inspection, golden corpus and CI.
