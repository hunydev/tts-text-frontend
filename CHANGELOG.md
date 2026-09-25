# Changelog

## Unreleased — speech final gate

- Improve the guess for an undeclared English word in Korean text: the grapheme
  prototype now understands word-start and word-end context and a silent marker,
  and the Korean mapper drops coda `r` the way loanword spelling does
  (`report.txt` → `리포트`, `market` → `마케트`, `center` → `센터`). On an authored
  set of 28 out-of-vocabulary words the exact match rises 0 → 14; the remaining
  failures stay recorded rather than being added to a lexicon.
- Take the guess further with three declared Korean spelling rules: a voiceless
  stop closes the preceding syllable only after a short vowel and never before a
  nasal or liquid (`market` → `마켓`, but `update` → `업데이트`, `partner` →
  `파트너`), an `l` after a vowel is written twice (`platform` → `플랫폼`,
  `delivery` → `딜리버리`, `daily` → `데일리`), and the r of `ER` becomes the onset
  of a following vowel instead of disappearing (`battery` → `배터리`). A palatal
  takes `이` as its inserted vowel and palatalises a following vowel (`package` →
  `패키지`, `shopping` → `쇼핑`), and a diphthong's tail syllable carries the coda
  (`point` → `포인트`). The 28-word set now matches exactly, 0 → 28.
- Add `phone_vowel_length` (`short|long`) and the `phone_mapping` keys `onset_l`
  (`double|single`) and `linking_r` (`link|drop`).
- Widen the guess to consonant clusters, longer words and derived forms, measured
  on two further authored sets (30 words, then 18 neighbour words written to catch
  rules overfitted to one word). A dropped coda `r` lengthens its vowel so the next
  stop keeps its own vowel (`smart` → `스마트`), a word-final sonorant after a
  consonant is the coda of the inserted vowel (`simple` → `심플`), a consonant
  before a glide is an onset rather than a coda (`security` → `시큐리티`), and a
  palatal takes `ㅖ` (`sharing` → `셰어링`). `W` now carries to `ㅞ` and `ㅝ`
  (`west` → `웨스트`, `water` → `워터`).
- Let a g2p mapping carry `.` as a morpheme boundary, which closes the syllable
  whichever way the phonology would fall: `^up` → `AH P .` reads `uploading` as
  `업로딩` rather than `어플로딩`. Generalising the same position phonologically
  would misread `application`, so the boundary stays data, not a rule.
- Add the reduced vowel `AX` (ㅡ) to the Korean phone map: `person` → `퍼슨`,
  `lesson` → `레슨`.
- Add three gates: 30 wide words, 17 neighbour words, and the earlier 28. The
  open case is `national` (the prototype reads `네이셔널`; the standard `내셔널`
  needs stress-driven vowel reduction) and it stays recorded as a failure.
- Read a `w` after a consonant as its own syllable, which is what Korean writes
  (`sweet` → `스위트`, `twin` → `트윈`), while a velar keeps it (`quick` → `퀵`).
  The choice is the new `phone_glide` resource (`merge|split`, default `split`);
  `y` always merges (`review` → `리뷰`) and does not consult it.
- Read `o` as `오` by default, and an `s` between vowels as `z`: `logging` →
  `로깅`, `boston` → `보스턴`, `season` → `시즌`, `music` → `뮤직`, `easy` → `이지`.
- Raise the g2p key bound from four letters to five, so an English suffix or stem
  fits in one row (`^reaso`, `ator$`, `^music`). Longer keys also overfit less: a
  four-letter `^chic` for `chicago` would have misread `chicken`.
- Add two more measured sets, again authored before the rules: 27 words for verb
  forms, `w` clusters, voiced `s`, proper nouns and long words (5/27 → 25/27), and
  24 neighbour words for the rules that set motivated (15/24 → 24/24). The
  measurement is now 127 words with 124 exact and 124 gated. The three open cases
  are `national` (stress-driven vowel reduction), `michael` (a proper noun's
  vowels are lexicon knowledge) and `visit` (its expectation is contested: the
  rule behind `market` `마켓` gives `비짓`).
- Add a 28-word measurement set and promote all 28 into the regression gate.
- Update three earlier references whose expectation recorded the prototype's own
  output, now that the guess is the standard spelling: `snake` 스내크 → 스네이크,
  `daily` 데이리 → 데일리, and the `IT` word policy 이트 → 잇. Each case records
  the reason in its notes; the policy each case measures is unchanged.
- Read a single digit after a letter label with the source numeral: `k7` →
  `케이세븐`, `A4` → `에이포`, `F1` → `에프원`, `i5` → `아이파이브`, `B2` → `비투`.
  Two or more digits keep the target numeral (`G80` → `지팔십`, `S23` → `에스이십삼`),
  a leading zero keeps its digits (`k007` → `케이영영칠`), and two-letter uppercase
  labels keep the initialism policy (`MP3`, `GPT-4`). Ten contracts across the
  v0.20 and v0.21 corpora record the new readings.
- Add 15 contracts; external English results are identical.
- Read a label whose letters have a declared word reading as that word plus the
  source-language numeral, spaced: `top10` → `탑 텐`, `best5` → `베스트 파이브`,
  `SAVE20` → `세이브 트웬티`. An idiomatic spelling is declared whole instead:
  `top100` → `탑백`. Spelled letter labels (`k7`, `MP3`, `GPT-4`, `A4`) keep the
  target numeral attached. Two v0.20 contracts record the new readings.
- Read an uppercase promotion code whose letters have a declared word reading as
  a word label (previously `에스에이브이이투지로`). An uppercase run with no declared
  reading keeps the initialism digits policy, so `GPT-4` → `지피티포`, `MP3` and `KBS`
  are unchanged. Add seven pending code words.
- Add 10 contracts; external English results are identical.
- Add an initialism policy: `spell` (default) reads an all-uppercase run of two
  or more letters as an abbreviation even when the same spelling is an ordinary
  word (`IT 업계` → `아이티 업계`, previously `이트`), while `word` keeps the word
  reading. A declared acronym (`NASA` → `나사`), proper noun or lexicon entry wins
  under both. Set through `ttf_engine_set_option(engine, "initialism", …)`,
  `--initialism` or the workbench selector.
- Add 15 contracts; external English output is unchanged (ten spans now carry
  INITIALISM instead of WORD).
- Add a decorative policy: `follow` (default) keeps stars, hearts, arrows and
  emoji under the unreadable policy, while `read` speaks a declared name
  (`★ 별점` → `별 별점`, `삼성™` → `삼성 상표`). Undeclared characters, including
  emoji, still follow preserve/drop, and `speech_decorative` holds the names.
  Set through `ttf_engine_set_option(engine, "decorative", …)`, `--decorative` or
  the workbench selector.
- Add 13 contracts; external English results are identical.
- Add an unknown-foreign-word policy: `guess` (default, the existing
  pronunciation guess), `spell` (`daily 업데이트` → `디에이아이엘와이 업데이트`) or
  `preserve` (`daily 업데이트`). A word with a lexicon or dictionary entry keeps its
  declared reading under every policy, and initialisms, alphanumeric labels,
  addresses and identifiers are unaffected. Set through
  `ttf_engine_set_option(engine, "unknown_foreign", …)`, `--unknown-foreign` or the
  workbench selector.
- Add 15 contracts; external English results are identical.
- Add a bracket policy: `read` (default), `skip` or `preserve` decides whether a
  bracketed aside is read, left out of the reading, or kept exactly as written
  (`설명(참고: 3kg)입니다` → `설명입니다` / `설명(참고: 3kg)입니다`). Declared pairs live
  in `bracket_pair`; quotation marks keep their own scope and an unclosed bracket
  is read as before. The option arrives through the new
  `ttf_engine_set_option(engine, "brackets", …)`, the CLI `--brackets` flag and the
  workbench selector, so no published structure changes.
- Add 11 contracts.
- Let the streaming planner read a declared year cue the way it reads a floor
  cue, so a cue word keeps its number in the same segment without becoming a
  general boundary word. Determiners (`the`, `a`, `an`, `this`, `that`) are cues
  again, and `from 1965 to 1970` / `between 1914 and 1918` work through a new
  `date_year_interval` rule that requires the first endpoint to be a cued year,
  so `rose to 1965` and `( 2003 and 2005 )` stay cardinals. On the external
  English sample DATE exact rises 343 → 394 with no regression and no CARDINAL
  change. Four v0.18 compatibility references record the new segment scores.
- Recognize more English year contexts (`from`, `between`, `around`, `about`,
  `circa`, `late`, `early`, `mid`, `through`, `till`, `post`, `pre`, `this`,
  `versus`): `from 1965 to 1970` → `from nineteen sixty five to …`. The existing
  quantity guard still keeps `page 1965` and `the 1965 people` cardinal. On the
  external English development sample DATE exact rises 319 → 343 with no
  regression; determiners and conjunctions are left out (see remaining-work).
- Add 13 contracts.
- Transmit an identifier or handle the way an address is transmitted:
  `abc_123` → `에이비씨 언더스코어 일이삼`, `@daily_log`, `user_name`. Declared
  lexicon words still read as words (`test_case` → `테스트 언더스코어 …`), text
  output preserves the identifier, and the identifier's own letters are not
  English context evidence. A word without an underscore or handle mark, an
  email address and a digit-leading form such as `2millionUSD_foo` are unchanged.
- Add 11 contracts; external English results are identical.
- Read a range written across several tokens: `3월 1일~3월 5일` →
  `삼 월 일 일에서 삼 월 오 일`, `10시 30분~11시` → `열 시 삼십 분에서 열한 시`.
  When neighbouring endpoints carry different units of one declared
  `range_unit_family` and the preceding word is a unit of that family, each side
  is read on its own. `1시~2분`, `1kg~2m` and same-unit intervals are unchanged.
- Add 11 contracts; external English results are identical.
- Speak an attached `~` between two readings as the range word instead of the
  symbol name: `서울~부산` → `서울에서 부산`, `9/25~9/30`, `오전 10시~오후 9시`,
  `월~금`. A spaced `~`, a repeated run (`진짜~~~`), a word-final lengthening mark
  and a customer-disabled separator keep their previous reading.
- Add 15 contracts; external English results are identical.
- Read an unambiguous slash date instead of a fraction: `9/25` → `구 월 이십오 일`
  (`September twenty fifth` in English), `2026/9/25`, and `5/8(수)` through the
  bracketed weekday cue. `1/2 컵`, `3/4`, `16/9`, `10/10` and the locale-ambiguous
  `9/25/2026` keep their previous reading.
- Add 16 contracts; external English results are identical.
- Speak an exact hour without its zero minutes in Korean: `09:00` → `아홉 시`,
  `09:00부터 18:00까지` → `아홉 시부터 열여덟 시까지`. A zero second is dropped the
  same way, `13:30` and `09:00:01` are unchanged, and `clock zero_minutes read`
  restores the old wording. Three fixtures recorded the earlier `영 분` text.
- Add 13 clock contracts; external English results are identical.
- Spell single letters joined by a slash as one sequence, like the hyphen form:
  `A/S 문의` → `에이에스 문의`, `I/O 포트` → `아이오 포트`. Fractions, units
  (`km/h`), URLs, dates and Hangul pairs keep their grammar.
- Add 13 contracts; external English results are identical.
- Keep the 30,000-number streaming gate inside its time limit: the Korean pack
  only re-runs the shared classifier when a 대 can follow, and particle
  agreement checks the span shape before any resource lookup.
- Read Korean service numbers with a declared four-digit prefix as telephones
  (`1588-0000` → `일오팔팔 공공공공`, previously the range `천오백팔십팔에서 영`).
  `phone_service_prefix` lists the prefixes; an undeclared pair such as
  `1500-1600` stays a range. The v0.18 contract for `1588-0000` is updated.
- Read a phone number written inside brackets (`연락처(010-1234-5678)`); the
  brackets stay as text and `(02) 1234-5678` keeps its area-code grammar.
- Add 17 contracts; external English results are identical.
- Add managed lexicon readings for mixed-case product names (`iOS` → `아이오에스`,
  `macOS`, `iPad`, `iPadOS`, `watchOS`, `iMac`, `AirPods`, `TikTok`, `eSIM`,
  `eBook`, `eSports`) and declare digit-leading labels `5G`, `4G`, `3G`, `2D` as
  alphanumeric terms with their own readings (`5G 요금제` → `오지 요금제`).
  `3D`, `K7`, `GPT-4` and undeclared labels such as `4K` are unchanged.
- Add 20 contracts; external English results are identical.
- Agree a Korean particle written against a reading with the spoken form:
  `5kg로` → `오 킬로그램으로`, `1+1는` → `일 더하기 일은`, `7월는` → `칠 월은`,
  `B과` → `비와`. The `particle_form` table lists obligatory allomorphs only
  (은/는, 이/가, 을/를, 과/와, 로/으로 with the ㄹ exception); optional copula
  contractions keep their text. A particle after a space is a separate word.
- Add 20 particle contracts. Four fixtures that recorded the earlier
  ungrammatical output (`고객는`, `유에스커스텀를`) now record the agreed form.
- Korean letter name for `Z` is `제트`, keeping it distinct from `G` = `지`
  (`Z세대` → `제트세대`). English `zee` is unchanged.
- A number sign before digits names a position through a `number_sign` template:
  `#1` → `일 번` / `number one`. Hashtags (`#해시태그`), `C#` and text output are
  unchanged, and the reading is not padded before punctuation.
- Add 17 contracts; external English results are identical.
- Read a lone lowercase letter in Korean text as its letter name (`x 값` → `엑스 값`,
  `n번째` → `엔번째`) and hyphen-joined single letters as one spelling
  (`x-y 좌표` → `엑스와이 좌표`, `A-B 테스트` → `에이비 테스트`).
- `<` and `>` outside an expression are silent separators in speech
  (`설정>일반` → `설정 일반`); `3>2` is still read as a comparison.
- Add 20 contracts; external English results are identical.
- Read basement floors `B1층`/`B1F` as `지하 일 층` and ranges `B1~B3층` as
  `지하 일 층에서 지하 삼 층`; add `지하`/`주차장` floor cues (`지하 2F` → `지하 이 층`).
  Bare `B2`, `B2B` and vitamin `B1` stay identifiers.
- Add 19 floor contracts; external English results are identical.
- Read Korean `대` after a decade or a large scaled amount as a band: `20대 남성` →
  `이십 대 남성` (was `스무 대`), `1~2억대` → `일억에서 이억 대`. Adjacent cues
  (`차량 20대`, `20대 차량`) keep the machine count `스무 대`.
- Read `1억~2억 원` as an ascending interval when both endpoints are scaled; the
  descending `1억-2천만원` stays verbatim.
- Add 25 band/count contracts; external English results are identical.
- Read arithmetic expressions with a relation or an unambiguous operator:
  `3-1=2` → `삼 빼기 일은 이`, `3>2` → `삼은 이보다 크다`, `a+b는` → `에이 더하기 비는`.
  `math_relation` templates pick 은/는 from the left side; bare `5-3` ranges,
  `2+1`, dates and phone numbers are unchanged. The expression's own letters are
  not language evidence.
- Korean `#` reads `샵` (`#해시태그` → `샵 해시태그`, `C#` → `씨샵`); English uses
  `speech_symbol_prefix` for hashtags (`#hashtag` → `hashtag hashtag`).
- Add 37 expression/hashtag contracts; external English results are identical.
- Read email/URL labels as words when a resource supports it (`electronic_word`,
  then the pronunciation lexicon) and spell the rest: `help@naver.co.kr` →
  `에이치이엘피 앳 네이버 닷 씨오 닷 케이알`, `.com` → `닷 컴`. This replaces the
  v0.20 all-letter spelling contract. Add 11 pending mail/portal lexicon entries.
- Recognize bare hosts ending in a declared lowercase `url_tld` (`naver.com에서`);
  text output preserves them.
- Read versions and IP addresses in speech output (`2.1.3` → `이 점 일 점 삼`,
  `v1.10.2`, `192.168.0.1` → `일구이 점 일육팔 점 영 점 일`); text keeps the surface.
- Add 34 electronic contracts; external English text output is unchanged.
- Spell short capital runs joined by an unspaced ampersand as one initialism
  (`R&D` → `알앤디`, `AT&T` → `에이티앤티`); speech reads the joiner from
  `speech_symbol &`, text keeps the written `&`. Lowercase words keep word grammar.
- Read a lone capital letter inside a Korean sentence as its letter name
  (`A 학점` → `에이 학점`, `K팝` → `케이팝`, `X세대` → `엑스세대`).
- Add 26 letter-name contracts. External English outputs are unchanged; three
  `A&M`/`DP&I`/`D&D` spans now carry INITIALISM IR.
- Read registered symbol and number-form characters as their own words in speech
  output: circled numbers, Roman numerals, vulgar fractions, `±`, `×`, `÷`, `㈜`.
  `speech_symbol` now accepts one non-ASCII symbol code point; `speech_join
  symbol|symbol.<char>` sets separation per language (KO `&` stays joined).
- Treat `~`/`～` after a word and before a pause as a lengthening mark
  (`speech_lengthening`): speech drops it, text keeps the written mark instead of
  the former `물결`. Standalone tildes and numeric ranges keep their readings.
- Map compatibility unit signs (`℃`, `㎏`, `㎞`, `㎝`, `㎜`, `㎎`, `㎖`, `ℓ`, `㎾`, `㎓`,
  `㎒`, `㎐`) to the existing canonical units; keep middle dot `·` as a pause mark.
- Speech output no longer leaves a doubled or leading space where an unreadable
  span was removed. Removal between two words now leaves one space (`가🙂나` → `가 나`), replacing the v0.20 joining contract by project-owner decision.
- Add 40 independent final-gate contracts; the external English development
  sample remains byte-identical to v0.20.

## 0.19.0

- Reuse request-local lexical-run boundaries and KO/EN context evidence while
  preserving scores, resource priority, quotations and full output metadata.
- Invalidate cached evidence after partial token consumption; isolate nested and
  concurrent requests; propagate factory failures with deterministic cleanup.
- Add native score/ownership checks, v0.18 metadata compatibility cases, arbitrary
  streaming splits and whole-unit 3,000/30,000-number regression gates.
- Add a reproducible context-scaling comparison tool and document borrowed pack
  engine views without changing public ABI v1 layouts.
- Retain identical results on the existing external English development sample.
  Repeated token mutations, pending scans and linguistic coverage remain open work.

## 0.7.0 — Financial intervals, content vocabulary and web workbench

- Normalize repeated currency/rate/measure endpoints into exact RANGE IR; retain
  mismatched units and malformed/chained intervals as source text.
- Extend explicit content title cues and add 40 category-separated pronunciation seeds.
- Add TTF Lab: actual C ABI integration, normalized text/IR inspection, live output
  callbacks, chunk parity verification, user/customer resource experiments and corpus reports.
- Add web API/ABI and Chromium browser gates; keep native ABI v1 and SONAME 1.
- Freeze 24 interval and 40 content references before baseline execution; document
  bounded syntax, small-context behavior and synthetic-data limitations.

## 0.6.0 — Literal code, markup and path preservation

- Resource-driven preservation of opaque HTML-style elements, comments, backtick
  code, explicit Unix/Windows paths and quoted paths with spaces/UTF-8.
- Shared protected token view isolates literal contents from language routing,
  classifiers, rule overrides and sentence/phrase boundary interpretation.
- Emit inspectable neutral `TEXT` spans; preserve unfinished literals on flush,
  reject invalid UTF-8, and honor pending caps. Native ABI v1 remains unchanged.
- Cache safe incomplete-literal body continuations with incremental UTF-8
  validation; keep closing-delimiter and early callback behavior observable.
- Propagate planner allocation/resource errors through the streaming API.
- Add 40 frozen synthetic text references, parser, stream-limit and overlay tests.
  Same references improve 4/40 to 40/40 across text/metadata/parity; historical
  challenge cases improve 13/15 to 15/15. These are development regression results.

## 0.5.0 — Financial identifiers, exact quantities and content cues

- Explicit resource labels select identifier digit reading with leading zeros and
  group boundaries; customer labels, zero spelling and separators are configurable.
- Added distinct `bp`/`%p` measures, decimal/signed ranges with units, exact bounded
  scientific notation, and uppercase AM/PM. Semantic values remain inspectable.
- Added resource title cues and generic hyphenated foreign words; curated spellings
  for `real-time`, `dashboard` and `quasar`. Unknown-word G2P remains a prototype.
- Preserve cue/value context across arbitrary stream chunks and tiny targets;
  bound title retention and avoid repeatedly rescanning prior words at each cut.
- Add optional `secondary` to JSONL v1 span records and `secondary`/`unit` to inspect.
  Native ABI v1 signatures, public structs and shared SONAME are unchanged.
- Promote known challenges into a strict 30-case text suite. Compare frozen v0.4
  output: 8/30 to 30/30; unchanged challenge references: 3/15 to 13/15. HTML/code and
  file-path handling still fail their desired references. No speech evaluation.

## 0.4.0 — Finance and content text quality

* Parse compound Korean amounts into a single canonical semantic value with exact
  decimal arithmetic; preserve invalid scales, grouping and overflow.
* Score complete local foreign phrases and English clauses, retaining language
  identity at clause endings and propagating Korean particle context across nouns.
* Add finance/content scenario references written before baseline execution,
  immutable baseline comparisons and a separate advisory challenge report.
* Add independent amount-value/metadata checks, resource-only routing overrides,
  and arbitrary byte splits through spaced compound amounts.
* Keep C ABI v1. Korean scale resources now use atomic exponents; see the migration
  note in `docs/ko-coverage.md` for old compound scale aliases.

## 0.3.0 — Text correctness and broader grammar

* Replace synthesis/listening evaluation with independent normalized-text references,
  exact matches, character edit counts, metadata assertions and byte-chunk parity.
* Remove the optional audio bridge, synthesis test dependency and audio CI artifacts.
* Recognize spaced quantities, fractions, seconds in times, Korean calendar forms,
  scaled Korean currency and resource-declared technical terms.
* Preserve invalid dates/times, grouped numbers, ordinal spellings and unsupported
  numeric atoms without partially converting their prefixes.
* Scope context inside complete quotations and preserve them during commitment.
* Resolve compound foreign verbalizations through the layered word pipeline.
* Expand KO/EN/foreign corpora and curated category lexicons. C ABI v1 is unchanged.

## 0.2.0 — TTS integration and evaluation

* Versioned JSONL streaming transport with language metadata and early pipe output.
* Optional reference TTS bridge using eSpeak NG or a configurable external adapter.
* PCM WAV validation, per-language synthesis jobs and listening manifests.
* Evaluation corpus, text comparisons, fallback statistics and HTML listening reports.
* C frontend latency/throughput benchmark with warm-engine timing and batch parity.
* Additional protocol, adapter and benchmark tests; CI publishes evaluation audio.
* Existing C ABI v1 and library behavior are retained.

## 0.1.0 — Initial multilingual frontend

* C11 static/shared libraries, C ABI v1, KO/EN language packs and resource layering.
* Semantic and pronunciation IR, context-sensitive ordinals and foreign pronunciation.
* Deterministic batch/stream normalization, metadata inspection, golden corpus and CI.
