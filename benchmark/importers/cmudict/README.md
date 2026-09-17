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
indexed dictionary. Base rows use `pronunciation`, `en-US`, headword and spaced ARPABET.
Stress tokens remain intact. The C English pack returns them in Pronunciation IR;
the Korean renderer currently ignores stress when rendering.

`base` selects the **unnumbered source entry**, not a semantically preferred
variant. Alternatives remain in JSONL, and the manifest counts unselected runtime
alternatives. The conservative adapter exports only lowercase ASCII letter words
shorter than 96 bytes, with at most 128 phones and less than 1024 pronunciation
bytes. Apostrophes, hyphens, digits and other forms remain in JSONL; skip reasons
are reported. This filter claims no reachability for unsupported token shapes.

### Explicit alternate export and selection (v0.17)

Use `--export-runtime --variant-policy all` to include bounded numbered alternatives
as `pronunciation_variant` rows alongside every eligible base:

```text
pronunciation         en-US  read    R EH1 D
pronunciation_variant en-US  read#2  R IY1 D
```

The displayed columns represent **tabs**, not spaces. No choice policy is installed
by the importer: loading this resource preserves the existing base default. A
separate deployment overlay can explicitly select the variant:

```text
pronunciation_choice  en-US  read    2
```

`base` restores base/default fallback selection. Variant IDs are opaque 1..31-byte
lowercase ASCII labels (`[a-z0-9][a-z0-9._-]*`); `base` is reserved. This importer
preserves CMUdict's numbered source IDs without renumbering or merging identical
phone sequences. IDs carry no POS, frequency or context ranking.

A choice applies only when its layer is at least the effective base pronunciation's
layer. For example, a domain choice cannot redirect an explicit customer base. An
eligible customer choice can select a system/domain variant; the trace reports the
choice policy and selected source row separately. At freeze, a missing selected
variant is an error. A lower choice shadowed by a higher base does not create an
unreachable dependency. Choices affect ordinary `pronunciation`, not the separate
`initialism_pronunciation` namespace; direct target-language lexicons still win.

`exported_runtime_alternatives` counts written numbered rows.
`unselected_runtime_alternatives` counts alternatives excluded by the `base` export
policy, not an automatic runtime ranking. Per-alternate size failures are reported
in `skipped_runtime_alternatives_by_reason` and do not remove an eligible base or
sibling alternate. All original alternatives remain in archival JSONL, including
those excluded from native export. `runtime_choice_installed` remains false.

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
license/provenance packaging, opt-in behavior, bounded numbered runtime export,
explicit selection/reset, higher-layer base protection, native dictionary routing,
direct lexicon precedence and English preservation. The second additionally imports real
pinned source data and loads a compiled subset into the native CLI. Neither test
generates a Korean expected spelling from the candidate mapper output.

Reusable functions: `verify_source`, `inventory`, `parse_dictionary`,
`select_words`, `runtime_payload`, `write_bundle`. The production CLI verifies
pin/hashes before parsing. Tests may call parser/bundle helpers on explicit
synthetic fixtures without representing them as pinned CMU content.

See [source, license and mapper research](../../../docs/research/pronunciation-sources.md).

## Native validation dependency (v0.16)

Native TTFB export now uses the same C resource validators as runtime loading.
Build `ttf` first (`cmake -S . -B build && cmake --build build`) or set `TTF_CLI` to
the current executable. The standalone compiler accepts `--resources`, `--language`
and ordered `--domain`/`--customer` overlays to match deployment. Importing the
pronunciation archive itself does not require the native executable. See
[resource validation](../../../docs/resource-validation.md).
