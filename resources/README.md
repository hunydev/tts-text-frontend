# Resource tree

`manifest.ttfm` declares the deterministic Common and Language System load order.
Domain and customer overlays are intentionally opt-in (`ttf_resource_load` or CLI
`--domain` / `--customer`). Do not list them in the system manifest.

* `common/`: shared segmentation data and future generic syntax policies.
* `languages/ko-KR/rules/`: Korean reading, units, counters and context policy.
* `languages/ko-KR/lexicon/{common,brands,technology,acronyms}/`: foreign spellings
  and Hangul pronunciation; independent of TN rules.
* `languages/ko-KR/dictionaries/`: shared-phone and emergency grapheme rendering.
* `languages/en-US/rules/`: English reading and context policy.
* `languages/en-US/dictionaries/`: small original pronunciation seed dictionary.
* `languages/en-US/g2p/`: deterministic prototype grapheme-to-phone data.
* `domains/finance/ko-KR/`, `customers/example/ko-KR/`: runnable reading overrides.

The seed dictionaries are original project data. No third-party pronunciation
corpus is redistributed. See `docs/resources.md` for syntax, compilation and
precedence, and `docs/lexicon.md` for lookup details.
