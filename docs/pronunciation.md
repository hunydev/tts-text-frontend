# Foreign pronunciation

For a foreign word selected for phonetic localization, the pipeline is:

```mermaid
flowchart TD
    Word[Foreign surface token] --> User[User lexicon]
    User --> Customer[Customer lexicon]
    Customer --> Domain[Domain lexicon]
    Domain --> System[System foreign lexicon]
    System --> Dictionary[Source pronunciation dictionary]
    Dictionary --> G2P[Source deterministic G2P]
    G2P --> IR[Shared Pronunciation IR]
    IR --> Mapper[Target phone mapper]
    Mapper --> Fallback[Emergency target grapheme renderer on failure]
```

A successful lexicon hit ends lookup. Otherwise the source pack returns pronunciation
phones and the target pack renders them. English source data use a documented ASCII
ARPABET subset (`AE P AH L`, `W AY F AY`), stored with an alphabet identifier and
provenance. The Korean target maps vowels/onsets/codas into Hangul syllables. Phone
stress variants can be normalized by the mapper. This is a prototype, not a complete
English pronunciation model or Korean loanword spelling standard.

The source English pack distinguishes initialisms/acronyms through resource data
and lexical signals. `USB`, `CPU`, `GPU`, `AI` use letter readings; `NASA` uses its
lexical pronunciation. Mixed alphanumeric terms are retained as structured lexical
units where supported. User/customer entries determine ambiguous readings.

Source dictionary misses use a greedy, resource-driven grapheme-to-phone prototype.
Target failures use an emergency grapheme renderer. The emergency fallback emits
Hangul for Latin letters and common sequences; directly testing `wifi` gives `위피`,
while dictionary/lexicon lookup gives `와이파이`. Unknown words remain speakable but
may be unnatural or wrong. The golden and unit tests separately cover system hits,
removed lexicon entries, unknown words and direct emergency rendering.

The core dispatches source `pronounce` and target `render_phones`; it contains no
English/Korean pair table. A Japanese target can consume the same pronunciation IR.
The small phone inventory, fixed pronunciation-buffer capacity and prototype
syllabification require expansion for more source languages and long lexical items.

`TTF_PRESERVE_LANGUAGE`, `TTF_PHONETIC_LOCALIZE` and `TTF_SEMANTIC_LOCALIZE` make
render intent explicit. The default is phonetic localization in a foreign context.
English context preserves ordinary English words and uses the English verbalizer
for semantic numbers/ordinals. A target pack without phonetic rendering callbacks
preserves source spoken text, sets render language to the source, and records
`preserved:no-renderer` in inspection. A phoneme-only output API can build on pronunciation
callbacks later; the current top-level result is normalized text with semantic spans.
