# CMUdict pronunciation importer

This tool imports the **pinned English pronunciation dictionary**, not a Korean
normalization gold corpus. It preserves ARPABET stress, all alternatives, source
line numbers and inline comments. It does not use upstream `make_ps_dict.py`, which
removes stress and deduplicates unstressed variants.

Requires Python 3.10+ and no third-party Python packages. An existing pinned clone
or unpacked snapshot is sufficient; the importer itself does not use the network.

## Reproducible import

Run from the repository root. Use a new output directory: existing outputs are
rejected, so an earlier provenance bundle cannot be silently replaced.

```sh
git init /tmp/cmudict-source
git -C /tmp/cmudict-source fetch --depth 1 https://github.com/cmusphinx/cmudict \
  74790861f652b15e4ac49015a90074ad62a27690
git -C /tmp/cmudict-source checkout --detach FETCH_HEAD
python3 benchmark/importers/cmudict/import_cmudict.py \
  --source /tmp/cmudict-source \
  --output benchmark/generated/cmudict-archive
```

The tool verifies SHA-256 of `cmudict.dict`, `cmudict.phones`, `cmudict.symbols`
and `LICENSE`. In a Git checkout it also verifies HEAD. A changed source is rejected
until the explicit pin/hash constants and source review are updated.

For a small subset, create an allowlist with one exact CMUdict headword per line
(for example `read`, `lead`, `abacus`), then pass `--words /tmp/cmudict-words.txt`.
An absent requested word is an error. Every variant of a selected word is retained.

Output bundle:

| File | Contract |
| --- | --- |
| `pronunciations.jsonl` | One headword per line; every source pronunciation retained |
| `manifest.json` | Pin, input/output hashes, record counts, runtime policy if requested |
| `COPYING` | Exact upstream license, to accompany redistribution |
| `NOTICE.txt` | CMU attribution, source revision, interpretation limits |

The JSONL schema is `ttf-cmudict-pronunciation` version 1, independent of the TN
sentence benchmark schema. Each record has `word`, `language`, `alphabet`,
`reference_status: dictionary-pronunciation`, and provenance fields
`source_url`, `revision`, `source_file`, `source_sha256`, `license`.
Each item in `pronunciations` contains:

```json
{
  "source_entry": "read(2)",
  "variant_label": "2",
  "source_line": 98826,
  "arpabet": ["R", "IY1", "D"],
  "phones": [
    {"symbol": "R", "stress": null},
    {"symbol": "IY", "stress": 1},
    {"symbol": "D", "stress": null}
  ],
  "comment": null
}
```

`variant_label: null` denotes the unnumbered entry. Labels and order have no
frequency, part-of-speech or contextual preference guarantee. Identical phoneme
sequences under distinct source labels remain distinct. Stress `0`, `1`, `2` and
an absent stress marker remain distinct.

## Opt-in native resource export

Both switches are required. No default resource manifest or existing lexicon file
is changed. Loading the resulting resource at a chosen layer is a separate action.

```sh
python3 benchmark/importers/cmudict/import_cmudict.py \
  --source /tmp/cmudict-source \
  --output benchmark/generated/cmudict-runtime \
  --words /tmp/cmudict-words.txt \
  --export-runtime --variant-policy base
build/ttf inspect --resources resources \
  --customer benchmark/generated/cmudict-runtime/pronunciation.bin \
  'abacus의 결과입니다.'
```

This adds `pronunciation.tsv` and its existing TTFB v1 compiled container
`pronunciation.bin`. The current container embeds checked TSV; it is not a compact
indexed dictionary. Rows use `pronunciation`, `en-US`, headword and spaced ARPABET.
Stress tokens remain intact. The C English pack returns them in Pronunciation IR;
the Korean renderer currently ignores stress when rendering.

`base` selects the **unnumbered source entry**, not a semantically preferred
variant. Alternatives remain in JSONL, and the manifest counts unselected runtime
alternatives. The conservative adapter exports only lowercase ASCII letter words
shorter than 96 bytes, with at most 128 phones and less than 1024 pronunciation
bytes. Apostrophes, hyphens, digits and other forms remain in JSONL; skip reasons
are reported. This filter claims no reachability for unsupported token shapes.

Direct User/Customer/Domain/System target-language lexicons still take precedence
over source pronunciation lookup. A loaded customer pronunciation resource may
override a built-in source pronunciation with the same key, according to the
existing layer policy. English context remains English.

CMU lookup coverage and Korean rendering accuracy must be measured separately.
The current mapper lacks complete English syllable/length/context rules; a
dictionary hit is not a reviewed Korean spelling. No synthesized audio is used.

## Tests and helper API

```sh
python3 tests/benchmark/test_cmudict.py --ttf build/ttf --resources resources
python3 tests/benchmark/test_cmudict.py --ttf build/ttf --resources resources \
  --source /tmp/cmudict-source
```

The first command is offline and uses hand-authored parser fixtures. It verifies
stress, alternatives without deduplication, validation failures, reproducibility,
license/provenance packaging, opt-in behavior, native dictionary routing, direct
lexicon precedence and English preservation. The second additionally imports real
pinned source data and loads a compiled subset into the native CLI. Neither test
generates a Korean expected spelling from the candidate mapper output.

Reusable functions: `verify_source`, `inventory`, `parse_dictionary`,
`select_words`, `runtime_payload`, `write_bundle`. The production CLI verifies
pin/hashes before parsing. Tests may call parser/bundle helpers on explicit
synthetic fixtures without representing them as pinned CMU content.

See [source, license and mapper research](../../../docs/research/pronunciation-sources.md).
