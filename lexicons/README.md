# Editable pronunciation sources

See the [Korean maintenance guide](../docs/lexicon-maintenance.ko.md) for the
record schema, review states, scope selection, browser workflow and deployment.

```sh
python3 tools/lexicon/manage.py validate
python3 tools/lexicon/manage.py build
python3 tools/lexicon/manage.py build --check
```

Commit JSONL/source-catalog changes together with generated runtime TSV files.
`pending` and `source-checked` are explicit project choices, not independent human
pronunciation approval. Imported external dictionaries need documented distribution
rights; this seed does not contain a bulk external dictionary import.
