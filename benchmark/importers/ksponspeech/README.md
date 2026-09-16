# KsponSpeech local importer

See [the source research](../../../docs/research/korean-sources.md) for annotation,
license, and attribution boundaries. This importer contains original code;
the reference preprocessor's MIT license does not license the corpus.

```sh
python3 benchmark/importers/ksponspeech/import_ksponspeech.py /local/transcripts \
  --split eval-clean --encoding cp949 --acknowledge-data-rights \
  --output benchmark/generated/kspon.jsonl
```

Input is a `.txt` file or a directory recursively containing `.txt` files.
`--split` records the actual source split; it does not re-split data. Output is
schema-1 benchmark JSONL. Written sides reconstruct the full input; selected TN
pairs become candidate spans, while sentence `expected` stays null. Their spoken
strings preserve the dataset's spacing. Excluded dual annotations and reasons
remain in metadata. `--include-unlabeled` also keeps files with no TN candidates.

No network, audio, automatic rights grant, production resource writes, or
automatic gold/reference approval. Local rights must already cover the intended
use and processing location. In-repository outputs must use `benchmark/generated/`.
Strict malformed/encoding failures leave a previous output intact. Summary counts
go to stderr; no source text is printed there. Tests use only synthetic fixtures.
