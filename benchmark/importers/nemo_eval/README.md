# NeMo English evaluation-set importer

An original, standard-library importer for the three English evaluation sets in
[NVIDIA/NeMo-text-processing `data/`](https://github.com/NVIDIA/NeMo-text-processing/tree/acacc21b1cb7916b10558855bc4f85957a0b2fde/data)
(CC BY 4.0 per its `data/README.md`). No NeMo code or data is vendored; tests use
original synthetic fixtures.

```sh
python3 benchmark/importers/nemo_eval/import_nemo_eval.py \
  --data-dir /path/NeMo-text-processing/data \
  --revision acacc21b1cb7916b10558855bc4f85957a0b2fde \
  --out benchmark/generated/en-eval/nemo-eval-acacc21.jsonl
```

| File | dataset id | Mapping |
| --- | --- | --- |
| `GoogleTN.json` | `nemo-googletn` | `text` → text, `gt_normalized` → expected |
| `LibriTTS.json` | `nemo-libritts` | same; audio paths and ASR transcripts are dropped |
| `EngConf.txt` | `nemo-engconf` | `TEXT~~RAW` block: first `~~1` → expected, all `~~1` → `accepted_references`, `~~0` → `rejected_references` |

- Schema v1 records, `split: evaluation`, `language: en-US`,
  `reference_status: dataset-reference`, `usage: benchmark-only`, no spans.
- Provenance carries the pinned revision, file SHA-256 and source line. A
  `.manifest.json` next to the output records per-file hashes and counts.
- Every sentence is imported. English-only scoping happens in
  `benchmark/evaluators/english_sentences.py` so excluded sentences stay visible.
- A malformed EngConf line or a block without `~~RAW` is an error; the output is
  replaced atomically and an existing output is never overwritten.

See [the English reference benchmark](../../../docs/english-reference-benchmark.ko.md)
for scope, match levels and results.
