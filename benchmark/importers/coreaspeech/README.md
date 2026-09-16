# CoreaSpeech optional local importer

Read [the source and license research](../../../docs/research/korean-sources.md)
first: the code's MIT claim and the dataset's terms are different. The official
dataset card's restrictions conflict with the general CC BY-NC claim. Local
processing requires independently established rights; neither CLI opt-in grants
permission. No data is downloaded or included in the shipped frontend.

```sh
python3 benchmark/importers/coreaspeech/import_coreaspeech.py /local/numeric.jsonl \
  --split numeric --allow-noncommercial --acknowledge-data-rights \
  --output benchmark/generated/corea.jsonl
```

Input is a local UTF-8 JSONL export with the official Testset's `text1` and
`text2` fields. These are two different utterances, so two separate unlabeled
records are emitted. `expected` and every span's `spoken` are null. Lexical span
classes support gap inspection and are not semantic-class ground truth. Audio
fields are ignored, and audio/Parquet packages are not required.

For an authorized training-set export, `--dataset-part training --split train`
reads `text`. `tr`, `N2gk` and `N2gkPlus` are never used as references. Use
`--revision` if the export comes from a different revision than the documented
default pin. A declared revision is the operator's statement; SHA256 independently
identifies the local export bytes.

## Optional references

First import to inspect stable IDs, then supply `--references /local/refs.jsonl`.
Each reference object requires its imported `id` and exact `text`, plus either
an `expected` string or nonempty `spans`. A span needs UTF-8 byte `begin`/`end`,
matching `written`, a `spoken` string, and `semiotic_class`. Source/reference
files are separately hashed. IDs absent from the export and mismatched texts or
offsets fail. This cannot pair `text1` with `text2` or silently accept prompt/target
fields from an undocumented mirror.

References are labeled `provided-reference`, without claiming human review or
correctness. The operator must prepare them independently from candidate engine
output to obtain a meaningful accuracy comparison. Both the output and any
reference file remain local and subject to the dataset's terms. No product or
golden-corpus path is accepted as an output; use `benchmark/generated/` or an
external local directory. Writes are atomic and cannot overwrite the input.
