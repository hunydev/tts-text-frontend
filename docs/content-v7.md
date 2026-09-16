# Content titles and pronunciation seeds (v0.7)

For the subsequent bounded multiword name support, see
[v0.8 multiword lexicons](multiword-lexicon.md). The scope below describes v0.7.

This increment adds data for content production and finance through the existing
language-pack interfaces. No new public API or C ABI is required. All validation
uses normalized text, span metadata and batch/stream parity; there is no speech
synthesis or listening evaluation.

## Explicit content labels

The English context resource now recognizes these additional **exact Hangul token
spellings**, each with a cue weight of 800:

| Label | Typical use |
| --- | --- |
| `영상제목은`, `영상제목` | Video title |
| `작품명은`, `작품명` | Work title |
| `에피소드명은`, `에피소드명` | Episode title |
| `프로젝트명은` | Project name |
| `영문타이틀은` | Explicit English title |

Horizontal whitespace and an optional ASCII colon may separate the label and its
following English lexical run. This reuses the bounded title-cue mechanism from
v0.5. A run must contain a lexical word; an ordinal alone does not count. The cue
does not cross a newline, quotation boundary or intervening clause punctuation.

```text
영상제목은 The 1st day입니다.
→ 영상제목은 The first day입니다.

작품명: The 4th day입니다.
→ 작품명: The fourth day입니다.

영상제목은 1st입니다.
→ 영상제목은 퍼스트입니다.

작품명은 Canva Story입니다. Canva를 사용합니다.
→ 작품명은 Canva Story입니다. 캔바를 사용합니다.
```

These labels express a project reading policy, not automatic title discovery.
`새영상제목은` is a different token and receives no cue. Spaced labels such as
`영상 제목은`, unknown labels and very long titles are outside this added contract.
The existing `제목은` cue may still apply independently inside a spaced label.

A customer can disable or add a label without recompiling. The file contains
actual tab characters; the Python example below creates a valid overlay:

```python
from pathlib import Path
Path("content-customer.tsv").write_text(
    "context_cue\ten-US\t영상제목은\t0\n"
    "context_cue\ten-US\t전시명은\t800\n",
    encoding="utf-8",
)
```

Load it using `ttf ... --customer content-customer.tsv ...` or
`ttf_resource_load(engine, path, TTF_LAYER_CUSTOMER)` before freezing the engine.
See [context-routing.md](context-routing.md) for scoring and retention bounds.

## Category-separated pronunciation data

Forty original project seed entries are loaded through `resources/manifest.ttfm`:

| New category | Entries | Examples |
| --- | ---: | --- |
| `lexicon/finance/default.tsv` | 12 | portfolio → 포트폴리오, hedge → 헤지, spread → 스프레드 |
| `lexicon/content/default.tsv` | 12 | podcast → 팟캐스트, thumbnail → 섬네일, storyboard → 스토리보드 |
| `lexicon/proper_nouns/default.tsv` | 16 | Canva → 캔바, Figma → 피그마, Nasdaq → 나스닥, Reuters → 로이터 |

The paths are relative to `resources/languages/ko-KR/`. Corresponding source
lexical classes live separately in the English context resource. For example,
`OBS` is an initialism, while product names carry the proper-noun lexical class
and remain `WORD` in the public Semantic IR.

These entries are authored project defaults, with no imported pronunciation
dictionary. They are not independently reviewed authoritative brand readings.
Spelling variants and alternatives should use customer or user overrides.
Single-token `AfterEffects` is included; this does not implement arbitrary
multiword dictionary matching for `After Effects`.

All records are phonetic renderings, not translations. The `finance` directory
is a vocabulary category in the **system** layer; it does not implicitly enable
financial TN policies. Pronunciation lookup still follows:

```text
User > Customer > Domain > System > source dictionary / G2P / emergency fallback
```

Keys use deterministic ASCII case folding. English speech context preserves the
original spelling even if a Korean pronunciation override exists. Code and path
literals remain protected. No adjustment of a following Korean particle is
performed: source particles are retained after pronunciation replacement.

```text
Canva를 사용합니다.             → 캔바를 사용합니다.
Canva released the 2nd trailer. → Canva released the second trailer.
상품은 swap 및 spread입니다.   → 상품은 스와프 및 스프레드입니다.
```

The existing resource APIs and lexicon APIs can change these choices before
engine freeze. Category storage remains independent of runtime hash lookup.

## Evidence and limits

`tests/evaluation/content_v7.jsonl` contains 40 synthetic references with 59 exact
span assertions. The references were authored and frozen before either baseline
or candidate evaluation. SHA-256:

```text
f1e5618349ce00534eeba9097ebfc7b5ff179641bc3f211bdb3abbf1063109c0
```

| Same 40 references, default configuration | v0.6 baseline | v0.7 resources |
| --- | ---: | ---: |
| Text and metadata pass | 3/40 | 40/40 |
| Exact output text | 5/40 | 40/40 |
| Character edits / 686 reference characters | 182 | 0 |
| Batch/stream mismatches | 0 | 0 |

This is a development regression comparison on known, synthetic project policies.
It is not held-out evaluation, customer data, general English G2P accuracy or
measured TTS voice quality.

`tests/integration/test_content_v7.py` additionally performs 312 checks, covering
1/7/4096-byte input chunks, default/tiny segment targets, span coverage, domain and
customer precedence, case folding, new and disabled labels, English preservation,
and isolation between separately created engines. Explicit title references must
hold under tiny targets as well. Other tiny-target cases check batch/stream parity
against the batch result produced with that same configuration.

Segment targets can change an ordinary unquoted context window. For example,
`first=1, normal=3, max=12` may split `상품은 swap 및 spread입니다.` before `swap`,
leaving that word in English; the default targets produce the reference above.
An English ordinal can likewise lose its English context after an artificially
short cut. This pre-existing routing limit is not hidden by the parity tests.
Use the same segmentation configuration when comparing outputs. Longer-range
context inheritance and more explicit rendering controls remain future work.

The next quality steps are independently reviewed finance/content examples,
multiword names, alternative pronunciation policies and wider unknown-word data.
