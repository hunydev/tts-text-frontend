# en-ja-dev v1 (2026-10-17)

Development sets for the English and Japanese packs, built after the open-tn comparison
(open-tn is scoring-only and is never used for fixes).

- Authors: eight fresh subagents (4 per language, 250 sentences each) with no repository or
  open-tn access, from the generation spec in the owner's open-benchmark prompt (same 17
  categories and quotas, same markup).
- Deduplicated: sentences 80% or more similar to an open-tn sentence or to each other were
  dropped (52).
- Review: four independent reviewer subagents (42 fixes, 3 drops), then Claude's own
  native-speaker sample review of 60 lines per language.
- Format: `id<TAB>lang<TAB>domain<TAB>category<TAB>difficulty<TAB>markup<TAB>note`;
  `{written|reading1/reading2}`, `{written|=}` stays as written. Japanese readings list the
  kanji form and the kana reading.
- Sizes: en-dev1 964, ja-dev1 981.
- Scoring: `benchmark/evaluators` runner with the open-tn score.py metric (and the pyopenjtalk
  reading view for Japanese). Fixes come from these sets; they are dev sets from the start.

## amend.tsv (2026-10-22)

Dev lines whose reading conflicts with a later project-owner decision are widened in `amend.tsv`
(`lang`, `line`, `was`, `now`, `why`); the source tsv files are never edited and the scorer reads an
amended copy. The first rows widen 15 English file names kept as written (`{report.pdf|=}`) to
also accept the full reading, after the decision that file names with a known extension are read.

