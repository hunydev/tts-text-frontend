# vi-th-dev v1 (2026-10-21)

Development sets for the Vietnamese and Thai packs, built after the open-tn comparison
(open-tn is scoring-only and is never used for fixes).

- Authors: eight fresh subagents (4 per language, 250 sentences each) with no repository or
  open-tn access, from the generation spec in the owner's open-benchmark prompt (same 17
  categories and quotas, same markup).
- Deduplicated: sentences 80% or more similar to an open-tn sentence or to each other were
  dropped (54).
- Review: four independent reviewer subagents (118 fixes, 4 drops), then Claude's own
  native-speaker sample review of 60 lines per language.
- Format: `id<TAB>lang<TAB>domain<TAB>category<TAB>difficulty<TAB>markup<TAB>note`;
  `{written|reading1/reading2}`, `{written|=}` stays as written. Japanese readings list the
  kanji form and the kana reading.
- Sizes: vi-dev1 965, th-dev1 977.
- Scoring: `benchmark/evaluators` runner with the open-tn score.py metric (and the pyopenjtalk
  reading view for Japanese). Fixes come from these sets; they are dev sets from the start.
