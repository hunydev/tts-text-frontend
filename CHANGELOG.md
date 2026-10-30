# Changelog

## Unreleased — speech output, measurement and the final gate

- xvoice5 comparison rerun (2026-10-30, owner request): the owner supplied an xvoice5 TN built
  from the internal engine source (TN1.14.16; the copy bundled with the engine was TN1.14.5 and
  lacked a 2026-09-23 fix). On the v3 Korean test spans it scores 76.5% (83.4% with the
  spelling adjustment; was 75.5% / 80.8%) against TTF 95.0% on main 710f10c. Held-out address
  and CJK sets are unchanged for xvoice5 (24 wrong spans; 82 of 210 sentences). Details in
  docs/xvoice5-comparison.ko.md. Documentation only; no engine change.
- Arabic email and URL controls (2026-10-27, owner report: the language comparison listed
  them as unsupported): `[[a1@b2.com]]{@email}` and `{@url}` failed with invalid resource,
  as the shared reader found no Arabic mark names. They now pass the shared shape check and
  read as an address in Arabic plain text does (إيه واحد آت بي اثنين دوت كوم); 13 Arabic
  `address_symbol` rows also read a URL query (? = &). Contract ar_controls_v123 (5).
- Startup gate relative (2026-10-27, owner decision): `tools/cjk/check_startup.py` starts a
  before and an after build in alternating order and gates the passthrough median ratio at
  1.25 (`--before`, `--before-resources`, `--after`, `--after-resources`). The fixed 16.5 ms
  reference predated the language packs, and the shared VM measured the same build at 55 ms
  and 100 ms an hour apart.
- Small fixes (2026-10-27). A particle written for 에프 after a basement floor takes the
  form 층 needs: B3F로 지하 삼 층으로 (was 층로), B2F를 층을, B1F와 층과, B1F로부터 층으로부터;
  B1F로비 is not touched. 啡 reads 배 (咖啡 가배; it had no reading) and 滑 reads 활 first
  (滑雪 활설; was 골설); words the dictionary writes with a compatibility ideograph are
  listed under the unified form too (滑稽 골계, 頻數 빈삭; cjk_words_stdict.tsv 4,890 rows,
  80 滑 words now read by the character rule). Web workbench: a named-dictionary field
  (`[NAME]` sections, option `named_dict_tsv`) for `{@dictionary:NAME}`, help rows and a
  sample with `{@nodict}`. Contract ko_floor_cjk_v122 (15); 38 evaluation sets, the Korean
  release benchmark and v3 are unchanged.
- Foreign cents and 쪽 (2026-10-27, decisions the owner delegated): a foreign amount with
  two decimals drops their trailing zeros (€89.90 팔십구 점 구 유로, $15.00 십오 달러; was
  팔십구 점 구영 유로); a won amount and a rate with more decimals keep every digit
  (1,182.60원 육영 원, 1.13890달러). 쪽 counts pages with Sino numbers (설명서 4쪽 사 쪽,
  12~15쪽 십이에서 십오 쪽; was 네 쪽), and a piece of food stays native (사과 2쪽 두 쪽,
  하루 10쪽씩 열 쪽씩). The owner sets and v3 accept both; the Korean release references read
  쪽 9 Sino : 3 native. Contracts ko_counter_native2_v21 and ko_both_weak_v118 (Was notes).
  Korean release benchmark +8 (쪽), -27 (26 cents read 영 in the references, 1쪽 한 쪽);
  v3 ko spans dev 95.7% → 96.0%, test 94.8% → 95.0%.
- Dictionary controls (2026-10-27, owner request): `[[group]]{@dictionary:NAME}` and
  `{@exact:NAME}` (NAME or NAME.tsv) read a group with a named dictionary of the engine, in
  place of the request dictionary; `[[group]]{@nodict}` reads it with no dictionary of any
  layer (exact, lexicon, lexicon_case and loanword rows, the request dictionary too). The group
  is read in its sentence. `ttf_engine_add_dictionary` registers a dictionary before the
  freeze (the engine owns it); CLI `--dictionary NAME=FILE`. An unknown name reads as usual,
  or fails under invalid_control=error. Tests `dictionary_controls`, `request_dictionary`.
  Output without these controls is unchanged.
- Korean addresses (2026-10-27, owner decision: plain-text rules stay general and must not
  touch other rules; a caller marks `[[…]]{@address}` for address reading). Plain text: B1층에
  reads 지하 일 층에 (a basement floor may carry its particle; was 비원층에) and 지하 B2층 지하 이
  층 (was 지하 지하 이 층); an address lot number keeps the identifier reading (역삼동 123-45
  일이삼 다시 사오, also correct; an address-word heuristic tried the same day was withdrawn).
  `{@address}`: B1층 지하 일 층 (was 비일층), and a unit letter or syllable before the hyphen
  (B-101호 비 다시 백일호, 가-101호 가 다시 백일호). Held-out address comparison (150 sentences,
  323 spans, digits or numbers both accepted for lot numbers): TTF plain text 312 (305 before),
  with the 90 address spans marked 319; xvoice5 297 (299 with its pronunciation spellings
  standardised). Contract ko_address_v121 (14); the Korean release benchmark is unchanged.
- Chinese and Japanese read in Korean under matrix=declared (2026-10-26, owner request:
  compare with xvoice5 with the hanja and kana readings on). With `--matrix declared`
  every sentence is read in Korean, so simplified Chinese and Japanese new forms take
  their traditional reading there too (a bare 中国 in auto routing still keeps it);
  hanja_rules.tsv gains readings for a character whose own reading differs from the
  form it stands for, Chinese in a sentence with neither Hangul nor kana (几 기, 价 가,
  广 광, 台 대) and Japanese in a sentence with kana (予 예, 体 체). In a Japanese
  sentence, hiragana after a read kanji goes on with its word (待って 댓테, 行きます
  행키마스, 家族と 가족토); a katakana word after hiragana starts a word (のチケット
  노지켓토). hanja_readings_variants.tsv grows from 627 to 1,582 rows: more simplified,
  Japanese and glyph-variant pairs (青 靑, 教 敎, 税 稅), project readings for
  frequent characters Unihan has no Korean reading for (你 니, 吗 마, 吧 파, 哪 나),
  and the Unihan kHangul of characters outside K0/K1. Held-out comparison (110
  sentences, Korean with hanja or kana, Chinese, Japanese): TTF 108 sentences right,
  xvoice5 22. Contract cjk_declared_v120 (16). Output without the reading options,
  and the Korean release benchmark, are unchanged.
- Korean forms both engines missed in the v3 comparison (2026-10-26): lengths of time
  M:SS and H:MM:SS by a length word before or after (재생 길이 3:45 삼 분 사십오 초, 0:45 길이
  사십오 초, 2:58:30 만에 두 시간 오십팔 분 삼십 초, 4:25의 기록 사 분 이십오 초) and H:MM before
  소요 (비행 2:10 정도 소요 두 시간 십 분); a clock stays a clock after 오전/오후, before
  에/부터/까지 and after 시각/시점/일시. A ratio word before a pair (축척 1:25 일 대 이십오).
  Month and day without a year after a date word (오는 11. 18. 십일 월 십팔 일, 매월 09.15 구 월
  십오 일); a list 1. 2. 3. and a full date keep their reading. Anniversaries before 연휴, 추모,
  혁명 and other event words (8.15 연휴 팔일오). 정도 apart from its number is about, not the
  counter 정 with 도 (€89.90 정도 no longer fails; 3정도 stays 세 정도; numeric_suffix_word value
  `spaced`). Foreign amounts keep reading every written digit (€89.90 팔십구 점 구영 유로):
  dropping the zeros won two v3 dev sentences and lost 33 Korean release sentences. Contract
  ko_both_weak_v118 (26); Korean release benchmark +7, -0; v3 ko spans dev 94.4% → 95.7%,
  test 94.6% → 94.8%.
- Superscript powers across languages (2026-10-26): an English sentence routed from Korean
  reads the Korean pack's powers (If 3²=9 three squared equals nine; was three ² equals nine,
  16 one six); en.power writes a power as ^N so a Korean reading says 제곱 ((x+1)²=0 (엑스
  더하기 일) 제곱, E=mc² mc 제곱; was the English word spelled 에스큐유에이알이디); a superscript
  minus reads (10⁻³ ten to the minus three, 십의 마이너스 삼제곱). Contract
  superscript_power_v119 (7).
- Styled characters, chemical formulas and accented words (2026-10-26,
  docs/styled-accents.ko.md): mathematical digits 𝟏𝟐𝟑 read as numbers (numeric width view);
  mathematical letters 𝐀𝐁𝐂 and letterlike ℝ read like full-width letters (speech-math-letters.tsv);
  Korean chemical formulas spell the elements and read counts as English numerals (owner decision:
  H₂O 에이치투오, H2SO4 에이치투에스오포, C6H12O6 씨식스에이치트웰브오식스; x₁ 엑스원); the stdict
  loanword build takes accented originals (café 카페, München 뮌헨, 404 words) and phrases
  (São Paulo 상파울루, Les Misérables 레 미제라블; stdict_loanword_phrases.tsv, CC BY-SA 2.0 KR);
  ten project entries (Pokémon 포켓몬, Beyoncé 비욘세); for unknown words the accents guide the
  guess (final é, ï/ë, ñ, ç). Contract ko_styled_accent_v117 (30). The 26,456 evaluation sentences
  and the Korean release benchmark keep their output.
- Request dictionaries (2026-10-26): `ttf_dict_t` holds exact rules and lexicon rows for one
  request (`ttf_normalize_with`) or stream (`ttf_stream_set_dict`) of a frozen engine, above
  every engine layer (`TTF_LAYER_REQUEST`, trace name `request`); CLI `--request-dict FILE`.
  Rows match as written (no particle or case folding, owner decision). Among engine exact
  rules of one layer and length, a rule of the sentence language now beats a `*` rule (file
  order decided it). Without a dictionary 26,456 sentences of the seven languages keep
  byte-identical output and spans. Test `request_dictionary`.
- Request dictionaries in the web workbench and traces (2026-10-26): a **요청 사전 TSV** field
  (option `request_dict_tsv`, 16 KiB) for normalize, detailed trace, stream and chunk runs, and
  a sample; `ttf_normalize_traced_with`; CLI `--request-dict` now works with `--trace`.
- Dictionary entries win inside grammar spans (2026-10-26): when a classification would take
  a later token where an exact rule above the system layer (domain, customer, user or request
  dictionary) starts, it is classified again up to that token (제 1231987호 now reads the user's
  entry, 제 일이삼일구팔칠호). No shipped resource has such a rule; output without dictionaries is
  unchanged (26,456 sentences) and a million-entry dictionary keeps its speed.
- en-US: a digit at the start of the text no longer reads two bytes before it in the
  scripture verse check (AddressSanitizer).
- The owner's TN evaluation sets (benchmark/datasets/user-tn-eval): 1,000 sentences each for
  ko-KR, en-US, ja-JP, zh-CN and vi-VN, split once into dev 700 / test 300 (fixes come from
  dev only). Scorer: benchmark/evaluators/user_tn_eval.py.
- User eval phase 1 (common forms and the owner's four reading policies, 2026-10-14):
  - U+2212 MINUS SIGN before a number or currency is the minus (no more "minus sign").
  - In ja-JP and zh-CN requests, full-width digits, Latin letters and . : / , + - % next to a
    digit read like ASCII (２８．４ｍ, ２３：５９, ５，０００); the Chinese clause comma ， is
    never a thousands separator.
  - An en dash between numbers is a range (9–12 nine to twelve, $20–$35).
  - Policies: Korean temperatures below zero read 영하 and drop 섭씨 (-2℃ 영하 이 도, -3도 영하
    삼도); Chinese × in an equation reads 乘以 (4×4 and 7.9×10³ keep 乘); Vietnamese 1/4 một
    phần tư.
  - Span accuracy, dev / test: ko 90.4→91.0 / 89.0→90.1, en 81.6→84.0 / 82.1→85.0, ja
    83.3→84.9 / 80.7→83.3, zh 93.1→93.6 / 94.7→95.6, vi 85.7→86.3 / 87.1→87.3. No dev span lost.
    Contract user_eval1_v81 (19).
- User eval phase 2 (codes, labeled numbers, units and maths, 2026-10-14):
  - English: code numbers (B214 b two fourteen, CR2032 c r twenty thirty two, KB5029331 one by
    one), capital words before a number (SAVE20 save twenty), digits after PIN, ZIP, code,
    passcode, extension, ext. and tracking number (spaced groups too), N11 numbers in English
    text (dial 911), toll-free 1-800 (one eight hundred), short English words in addresses
    (five letters: example.com/track), hrs, fl oz, sq ft, mpg, tbsp, tsp, L/min, MB/s, GB/s, mg/dL, mg/kg,
    pct, and a month/day after a date word (ships 12/5 December fifth).
  - Japanese: hyphenated codes (SK-0392 エスケーのぜろさんきゅうにー) and 24 counters (戸, 巻,
    色, 条, 章, 戦, 項, 版, 子, 室, 年生 ...; 40肩 しじゅうかた).
  - Chinese: code cues 号码, 序号, 行号, 信用代码; a listed service number before the word a
    call takes (96110的来电); a route of three digits or more (818路 八一八路).
  - Vietnamese: hyphenated codes (SP-20481 SP hai không bốn tám một) and maths expressions
    (7 × 8 = 56, x² - 5x + 6 = 0, 12:4 bằng mấy, 4x100m, x2); 7÷14 ngày stays a range.
  - Korean: COVID-19 코비드 일구, Wi-Fi 와이파이.
  - Span accuracy, dev / test: ko 91.3 / 90.4, en 90.4 / 87.6, ja 87.5 / 84.6, zh 94.5 / 95.6,
    vi 90.5 / 90.1. No dev span lost. Contract user_eval2_v82 (27).
  - English benchmark: one sentence each in speech and text now differs from its reference
    (ZIP code 26278 and PIN 503165 read digit by digit, the owner's reading).
- User eval phase 3 (dates, sound changes, signs, Vietnamese forms, 2026-10-14):
  - Korean: '25.11.3 and 26.12.24 dates, 1/4분기 일 사분기, 큰술/작은술 in native numbers, and
    101번째 백한 번째 (project-owner decision: a last digit 1-9 after hundreds is native; 176번째
    stays 백칠십육 번째; test_korean_counters updated, was 백일 번째).
  - English: 6'2" six foot two, a 65" TV sixty five inch, 1:10 one to ten after a ratio word,
    4 x 8 ft four by eight feet, Flight UA 2314 u a twenty three fourteen, Room 1408 fourteen oh
    eight (speech).
  - Japanese: じゅってん before a decimal point (8.5 stays はちてんご), じゅっ before キロ units, 銭.
  - Chinese: an amount with a plus sign reads 正 (+0.15% 正百分之零点一五), 2站 两站.
  - Vietnamese: ℃ độ xê, CN chủ nhật after vào, T4 thứ tư, 7g sáng giờ, 48h giờ, a display's 4K
    bốn K, 4x100m and VX-2026-031 in longer sentences.
  - The ja scorer now reads a long-vowel mark as hiragana writes it (ジョー = じょう); all phases
    were rescored.
  - Span accuracy, dev / test: ko 92.3 / 90.4, en 91.5 / 87.9, ja 92.3 / 89.1, zh 95.4 / 95.8,
    vi 92.9 / 90.6. No dev span lost. Contract user_eval3_v83 (19). Other gates unchanged, Korean
    quick +7 reference matches.
- User eval phase 4 (units, durations, signs and Vietnamese numbers, 2026-10-14):
  - Bug fix: shared unit suffixes added for English (MB/s, mg/dL, tbsp ...) and six older ones
    (degrees C, /km², kl ...) had no Korean reading, so a Korean request spelled the resource key
    (엠비 언더스코어 피이알 ...). tests/integration/test_unit_readings.py keeps them covered.
  - Korean: +9시간 플러스 아홉 시간, durations (3:05짜리 삼 분 오 초, 기록
    2:58:30 두 시간 오십팔 분 삼십 초), 6/6 현충일 유월 육 일, 1:25,000 일 대 이만 오천, 5Mbps.
    Currency decimals keep their written zeros (the Korean references read 1,182.60원 천백팔십이
    점 육영 원).
  - English: 67¢ cents (English lines only), 4.5/5 out of five, +/- plus or minus, Wed, Oct 15 Wednesday,
    Monday–Friday Monday to Friday, Expiration: 08/2027, 4.6 in. inches.
  - Japanese: +1.2kg プラスいってんにキログラム, 21:15に終了 a time (not a score), 1:10に希釈.
  - Chinese: zero cents are not read (￥86.40 八十六点四元), 20-30% 百分之二十到百分之三十,
    2-3次 两到三次, 1-2年级 一到二年级.
  - Vietnamese: per-word after k/tr and units (150k/ngày một ngày), ±, GMT+7, SPF 50+, 8/10
    trên mười, 4,9/5, 1/1.000, E04, scores after hòa, giải nhất, v2.3.1, AM/PM, Km 92, ĐT 743,
    rpm, IP octets, extension/emergency/build numbers, five-group and +84 phones, Phòng P.1205.
  - Span accuracy, dev / test: ko 93.6 / 90.7, en 92.6 / 88.2, ja 93.2 / 89.3, zh 96.2 / 96.1,
    vi 97.4 / 94.2. No dev span lost. Contract user_eval4_v84 (22). Other gates unchanged.
  - Contracts updated with Was notes (owner-set evidence): vi_prod_v80 12/28 phiếu trên,
    vi_pack2_v75 P.308 phòng ba không tám and IP 192.168.1.1 digit by digit.
- Thai is in production (th-TH, project-owner decision, 2026-10-15): owner real-sentence v2 96.1,
  held-out v1 96.0, owner set test 94.7, no engine errors in 14,927 real sentences; TTS-to-STT on
  the 371 v2 sentences: raw text 45.3% to TN 73.3% of sentences (spans 62.9% to 83.0%), no new TN
  problem. Human native review and VoxCPM2 listening remain, as for the other languages.
- Arabic pack phase 1 (ar, Modern Standard Arabic; project-owner request 2026-10-22):
  - benchmark/datasets/user-tn-eval: the owner ar set (1,000 sentences, dev 700 / test 300).
  - languages/ar: numbers agreeing with the counted noun (3-10 the opposite gender and the plural,
    11-99 the accusative singular, 2 the dual, the last two digits decide), the oblique and
    nominative cases paired as the set accepts them, construct forms (مئتي, ألفي), dates with an
    ordinal day and Hijri months, times with the feminine ordinal hour (التاسعة والنصف, إلا ربعا),
    money with its minor unit, units, ordinals after cue nouns and الـ, ranges (من … إلى, the dual
    for 2), fractions, ratios, scores, phone numbers and codes digit by digit with Latin letters by
    their Arabic names, addresses by known words or letter names, maths and the shared expression.
  - Core: Arabic-Indic digits ٠-٩ and ۰-۹ are digits, and ٫ ٬ ٪ next to digits read like . , %
    in every request; an en dash after one is the range hyphen; a sentence with Arabic letters
    routes to ar in any request (arabic_sentence); CLDR Arabic symbol names
    (resources/unicode-fallback/ar.*), the builder treats ، ؛ ؟ ۔ as pauses (ar and en rebuilt; en
    loses only those three names).
  - Owner set ar span accuracy: 0 to dev 97.7%, test 86.1%. Contract ar_pack1_v101 (27). The other
    six languages on the owner sets, Korean quick, English, JSUT, ja-tn-eval, Chinese and
    Vietnamese gates are unchanged; all contracts pass except the three known failures.
- Arabic pack phase 2 (2026-10-22): the owner held-out ar-v2 (400 sentences, kept local) was
  opened after the phase 1 freeze at 78.8% of spans and is now a dev set.
  - Plurals in ى and the ات plural of a تفعيل noun are masculine; one of a time noun is the noun
    alone; the minor amount after an abbreviation takes the oblique for 11-19; a unit after a
    scale word, the dual scale (مليوني نسمة); 15-17 o'clock read مساء; a version says الإصدار
    unless written and its dots are فاصلة; digits before a currency abbreviation are an amount;
    an Arabic letter variable starts an expression; a teen multiplier before lower groups takes
    ألفا; a Latin unit symbol takes the case of any unit; Hijri years read هجرية; more ordinal and
    code cue nouns and address words.
  - Owner set ar: dev 97.7%, test 86.1 to 88.6%; ar-v2 78.8 to 93.5%. Where ar-v2 and the
    ar set disagree (the case pairs) the ar set is followed.
  - The address and code hyphen (owner decision 2026-10-23): both sets accept شرطة or داش
    (amend.tsv; the Arabic brief allows both, the two authors each wrote one) and the engine reads
    شرطة, the standard word; a hyphen inside a phone number stays silent.
    Contract ar_pack2_v110 (16); ar_pack1_v101 three cases Was (هجرية, مساء at 15-17 o'clock, owner
    set evidence; the code hyphen شرطة, owner decision). The other languages and gates are unchanged.
- Large user dictionaries (project-owner request 2026-10-25), measured with a million exact rules
  (1000000..2000000) at the user layer: before, a sentence took 3.2 s (every exact rule walked at
  each position), freeze 3.1 s and 286 MB.
  - exact rules are found by hash at each key length that exists (the list keeps patterns and
    keys of 63 bytes or more); exact and pattern rows live in their own table from their first
    insert (one sparse calloc of 1048573 heads, grown at freeze past two rows a bucket); the main
    table grows at freeze to about one bucket per row (the bucket count is the engine's own);
    a row and its four strings are one allocation.
  - With the million rules: a sentence with dictionary hits 0.11-0.13 ms, other v3 sentences
    0.28-0.31 ms (0.25-0.29 ms without the dictionary); load 0.6 s, freeze 0.35 s, 223 MB
    (14 MB without). xvoice5 TN with the same million entries: 222 ms a sentence (it reads its
    dictionary on every call). Outputs unchanged on 23,686 sentences in seven languages.
  - Test large_dictionary: 300,000 rules in three customer files, 200 sentences under 30 s
    (0.46 s now).
- Speed (project-owner request 2026-10-24): resource lookups were 93% of the run time (gprof).
  The table has 65521 buckets (was 8191 for about 43k rows) and each row keeps its hash, compared
  before the strings; at freeze the key byte lengths of every kind and language are recorded,
  so longest-prefix scans (core resource_prefix, Korean quantity_resource_at) try only lengths
  that exist; a loaded file merges from its staged row list instead of walking every bucket.
  Korean v3 sentences: 2.4-2.9 ms to 0.28-0.38 ms each (xvoice5 TN 1.1-1.4 ms); the run-to-run
  spread is gone; engine load unchanged within noise. Outputs are byte for byte the same on
  23,686 sentences in seven languages.
- Korean: a unit symbol after an approximate number reads as its unit (6천여 m² 육천여 제곱미터,
  200여 m 이백여 미터; was 엠 제곱, 엠). Contract ko_approx_unit_v116 (4); Korean quick 5 sentences
  better.
- Korean superscript powers (project-owner request 2026-10-23; the superscript was passed through):
  3²=9 삼의 제곱은 구, 10³=1000 십의 세제곱은 천, 2¹⁰ 이의 십제곱, 10⁻⁶ 십의 마이너스 육제곱,
  x²+y²=z² 엑스 제곱 더하기 와이 제곱은 제트 제곱; a power left after a Latin letter or digit
  reads 제곱 (mc² 엠씨 제곱); superscripts alone (¹²³, 각주¹) stay. Korean requests or Hangul
  sentences only. Contract ko_superscript_v115 (7); unit_symbol_v21 one case Was (1² 일의 제곱).
  v3 ko-KR dev 94.4% (math 17 to 23 of 25), owner set ko-KR dev 96.9%.
- Speed against xvoice5 TN (docs/xvoice5-comparison.ko.md): engine load 45 ms against 161 ms;
  per sentence 2.4-2.9 ms against 1.0-1.4 ms on the v3 sentences (TTF varies 1.2-3.2 ms by run).
- Korean project-owner decisions after the xvoice5 comparison (2026-10-23):
  - Ranges: a native first number keeps its unit (2~4시간 두 시간에서 네 시간; was 둘에서 네 시간),
    a Sino one stays bare (2~4층 이에서 사 층); a prefixed unit keeps its prefix at both ends and
    the unit once (60~70℃ 섭씨 육십에서 섭씨 칠십 도, 2~3 mph 시속 이에서 시속 삼 마일). Adjacent
    contraction stays (40~50분 사오십 분).
  - 석 (up to ten) and 문항 count natively (2석 두 석, 15문항 열다섯 문항); 평 stays Sino.
  - A decimal before one scale word is read as written (1.25만 일 점 이오만, 3.5억 원 삼 점 오억 원,
    -4.5조); won in 만 (3.5만 원) and three decimals or more (1.001만 원) keep the sum; ranges
    keep the sum (1~1.5만 원). Port numbers stay digit by digit.
  - An IP address reads its zero 공; go and or before .kr are spelled (지오, 오알); weather 웨더;
    court case numbers read the year and the number (2025타경10492 이천이십오 타경 만 사백구십이).
  - Contracts Was: cases, counter_frequency_v22, identifier_fallback_v22, ko_control_ignore_v24,
    ko_identifier_shapes_v23, ko_range_reading_v24 (one each), electronic_v21 (two), typed_controls_v22
    (one); 24:00 is no longer the invalid-time example (25:00 in help.json and the typed-control
    test). New contract ko_owner_dec_v114 (10). v3 widened for 50번째 (쉰/오십 번째).
  - v3 ko-KR: dev 94.0%, test 94.6%; owner set ko-KR dev 96.6%, test 92.1%; Korean quick loses
    one sentence whose reference reads 12~15살 열둘에서 열다섯 살. Other languages unchanged.
- Korean fixes found against xvoice5 TN on the open set v3 (2026-10-23):
  - 24:00 and 24:00:00 read 이십사 시 (the end of the day; the 2026-10-02 guard that kept a
    colon that looks like a time preserved is lifted on the owner set v3, two contracts Was);
    PM2.5 피엠 이 점 오 (capital letters before a decimal number); 2:2:1 이 대 이 대 일 (three
    terms or more that cannot be a clock). Korean requests or Hangul sentences only.
  - v3 ko-KR spans: dev 92.2 to 93.1%, test 93.7 to 94.3%; owner set ko-KR dev 96.2 to 96.5%.
    xvoice5 TN on v3: dev 74.8%, test 75.5% (80.4 / 80.8% with its sound spellings read as
    spelled; our 22 lexicon words as its user dictionary do not raise it). docs/xvoice5-comparison.ko.md.
    Contract ko_v3_fixes_v113 (6).
- Arabic variants (owner decision 2026-10-23): where the Arabic brief accepts two readings and an
  author listed one, both owner sets are widened by rule in amend.tsv (the original reading kept as
  `was`, the v1 test split by the same rule): case (عشرون/عشرين, اثنان/اثنين, مئتان/مئتين, خمسا/خمس,
  the accusative alif of the noun after a number), مئة/مائة, unit spelling (غرام/جرام, غيغا/جيجا),
  دوت/نقطة in addresses and فاصلة in versions; v1 884 spans, ar-v2 348. One v1 reference error
  (masculine numbers with سنة) gets the agreeing reading.
  - Engine: المخرج is an ordinal cue (المخرج 4A a code), psi, a top-level domain written alone
    (.sa, و.org), Arabic file names (تقرير_2026.pdf). Contract ar_pack3_v111 (6).
  - Owner set ar: dev 99.2%, test 89.8%; ar-v2 99.1%. The other languages are unchanged.
- Project-owner decisions on English readings (2026-10-22):
  - Two-digit years read in full with 00-49 in the 2000s and 50-99 in the 1900s: m/d/yy (7/4/58
    nineteen fifty eight), card expiry after exp, expires or thru and a day that cannot be one
    (exp 09/31 September twenty thirty one), and an apostrophe year after a month (Aug '19). A bare
    apostrophe year stays as written ('58 fifty eight, back in '07 oh seven): old books mean 1858
    and speakers say oh eight; full reading lost 31 LibriTTS sentences.
  - File names with a known extension read in full (report.pdf report dot p d f, kickoff-deck.pptx,
    tax_return_2025.pdf): the body in words, letter runs without a vowel spelled, separators by
    name, the extension by letter names or its set word (zip, jpeg, json). New resource namespace
    file_extension. Paths (/tiles/y48.png) stay protected.
  - Address words: common English words and given names of three letters or more read as words in
    URLs and e-mail addresses (transit.org/live, jess.m@, tom@); single letters, runs without a
    vowel and two-letter codes stay spelled; a capitalized word keeps its capital (GreenLeaf.com).
    resources/languages/en-US/rules/electronic-words.tsv (self-authored).
  - Letter names stay (pee ess five); open-tn gets an adjusted English score with letter names
    folded next to the official one: 92.5 (official 88.2; WeTextProcessing 69.8, NeMo 75.7).
  - The en-ja-dev amend.tsv widens 15 dev lines that kept file names as written (the source file
    is never edited). Frozen contract en_speech_hyphen_v23 (2) changed with Was notes.
  - Gates: owner sets en dev 789 to 791, test 317 to 321, others unchanged; English real sentences
    with no errors. Thai dots checked: จุด in decimals, versions and IP addresses, ดอท in URLs,
    e-mail addresses and file names. Contract en_owner_dec_v100 (13).
- Listening check and the open-tn comparison (2026-10-22):
  - TTS-to-STT on 250 sentences each of the owner set dev split: Korean 55.2% of sentences raw to
    78.8% after TN, English 83.2% to 94.8% (92.4% before the fixes below), Japanese 71.2% to 82.4%.
    The Korean and Japanese sentences worse after TN are STT digits or mishearing.
  - English fixes from it: month/day ranges (09/01 – 09/30, 03/15-04/14), a leading-zero month
    (08/04), ½" half inch, °F and °C without a number, 2×4s two by fours and 4 × 8 ft dimensions,
    train number 171 one seventy one, C-3PO and B-1B, the extension of an underscored file name.
  - m/d/yy reads the year in full (5/20/16 twenty sixteen; 00-30 the 2000s, 31-99 the 1900s): the
    owner set user-tn-en-US reads it so in all four cases. Frozen contract en_wetext_gaps_v26 (2)
    changed with Was notes; the English benchmark loses one sentence whose reference keeps thirteen.
  - open-tn v1.1 (scoring only): ours ko 93.4, en 88.1, ja 78.7 (83.3 by reading), zh 96.1, vi 91.4,
    th 90.7; WeTextProcessing en 69.4, ja 74.3, zh 80.6; NeMo ko 53.1, en 75.4, ja 34.7, zh 63.9,
    vi 65.8. See docs/tn-comparison.ko.md.
  - Gates: owner sets en dev 780 to 789 and test 315 to 317, others unchanged; English real sentences
    13,105 with no errors. Contract en_listen_v99 (13).
- Vietnamese and Thai improvement from the dev sets v1 (2026-10-21):
  - Dev sets v1 (benchmark/datasets/vi-th-dev): vi 88.4 to 90.8, th 91.4 to 94.3. open-tn v1.1,
    scored once at the end and never used for fixes: vi 90.9 to 91.4, th 89.8 to 90.7.
  - Vietnamese vi_dev_forms ("say" readings): #2 số, A$ and other dollar prefixes, PM2.5, Km42+150,
    Roman century ranges, Thứ 2-Thứ 7, leading-zero counts, house numbers 25/4 trên, dates after a
    weekday, before âm lịch or after vào, 9g giờ, 120' phút, race times, room 2.05 chấm, emergency
    numbers and hotlines digit by digit, MB/s and Mbps. One digit after a Latin letter reads in
    Vietnamese (B3 B ba; alnum_number vi-VN single_digit target).
  - Thai th_dev_forms ("say" readings): hashtags, weekday abbreviations and ranges, licence plates
    after ทะเบียน, abbreviation plus number (ปอ.29, ทร.14, LV.50), a unit abbreviation before a size
    (ซม.100), currency with a scale word (€60 ล้าน), ฿350/ตร.ม., scores, house numbers ทับ, quarters,
    month and year, race times, H:MM ชม., powers, 12.12 shopping days, four-digit hotlines; units MW,
    hPa, Wh, ft, MP, GW, ft.lb. The shared expression does not take a zero-led key code (กด * 005 * 2 *).
  - Gates: owner sets vi dev 840 to 842 and test 343 to 344, Thai unchanged, other languages
    unchanged; Thai real sentences 14,927 with no errors and digits left unchanged.
    Contract vi_th_dev_v98 (38).
- Real-sentence check and the Japanese prefix decision (2026-10-21):
  - Real sentences after the English and Japanese work: English 13,105 and Thai 14,927 sentences, no
    errors, sentences with digits left unchanged (0.19%, 0.62%); speed within run-to-run noise.
  - Fixes from that check: H2O and G2A keep their digit (B2B, B2C, P2P, D2D only), 5-in-1, Mar.19,
    6/21-24 June twenty first to twenty fourth, K-2SO, V1.4 version; a comparison word list without
    show, of or units; the Thai pack reads the shared maths expression (10.8 / 1.2 = 9).
  - Project-owner decision: a Japanese kanji prefix of a number reads in kana with it (第2位 だいにい,
    午後3時 ごごさんじ, 令和8年 れいわはちねん, 平成元年 へいせいがんねん). Frozen contracts changed with
    Was notes: ja_gaps78_v49, ja_pack_v36 (2), ja_range_mark_v44 (2), ja_rest_v50, user_eval2_v82.
  - Gates: owner sets en dev 96.3, others unchanged; ja-tn-eval and JSUT unchanged.
- English and Japanese improvement (project-owner request, 2026-10-17):
  - Dev sets v1: benchmark/datasets/en-ja-dev (en 964, ja 981 sentences; eight fresh subagents from
    the open-benchmark spec, no repository or open-tn access, deduplicated against open-tn, reviewed).
    open-tn is scoring-only and was not used for fixes.
  - Dev spans: en 84.4 → 92.1 (95.7 with letter names), ja 72.3 → 75.8 (85.7 by reading).
    open-tn v1.1 (scored once at the end): en 84.8 → 87.7, ja 76.5 → 77.0 (reading 82.9 → 83.3); the
    other four languages unchanged.
  - English (speech in an English request only): an amount or measure before a noun is singular
    (a $25 deposit, the 200 m butterfly, 40-ft, a 3.2 lb package, the 65W charger); $0.99 ninety nine
    cents; $2.99/mo a month; 3/$1; label numbers in pairs after Room, Form, Boeing, Error, port,
    Route, course names and before a street (3015 Broadway thirty fifteen, 1600 sixteen hundred);
    card ending in 4417; M/D dates after the twelfth or a date word (9/12, 10/27), fractions near a
    comparison (7/16 is less than 1/2), 9/11; '08 oh eight, '58-'59; weekday and month abbreviations
    (Thurs., Mon–Fri, Sept.); en dash ranges between dates, grades and labels (Dec. 24–Jan. 1, K–5,
    A1–A12, K-12); race times (2:31:08, 1:18.442, 2h 40m); dotted phones, x4417, 988; units kt, inHg,
    nmi, bbl, yds, ppm, ppb, ng/mL, sq ft, yrs, wks, bpm, 128/82 mmHg, 14 in of; superscripts (x²,
    2¹⁰ to the tenth); fractions inside equations; √ the square root of; PM2.5; Super Bowl LIX; Python
    3.12 three point twelve; 401(k), 529 plan, 1099-NEC, B2B, 4x100, AAA batteries, Agent 007, ½ the;
    PO Box, Apt., RR; ZIP after a state; .312 batting three twelve; 4th St fourth Street.
  - Japanese: superscripts (エックスのにじょう, にのじゅうじょう), fractions after の and in equations
    (ぶんの), M/D before a date word (1/5出発), mixed numbers (2と3/4), 2丁目14-3 の, 受付番号15番 じゅうご
    ばん (内線 stays digits), 12.50ドル drops the zero, £5.60 pence, 1ドル＝152円, μSv, km/L, 38万km,
    86°F 華氏, B1階 ちかいっかい, 4分間隔, pt, #hashtag, Python 3.12 さんてんじゅうに, latin_phrase
    HbA1c, 401k, M-1, 1on1, 2in1, 3x3, e-Tax, USB-C, 1080p, and 50 address words (dictionary only).
  - Not changed (frozen contracts, form only): kanji prefixes 第・午前・午後・令和 stay kanji before a
    kana number; 〒 alone reads ゆうびん; COVID-19 コビッドじゅうきゅう.
  - Gates: owner sets dev en 96.0, ja 97.3 (test en 90.5, ja 92.7), others unchanged; ja-tn-eval and
    JSUT unchanged; Chinese and Vietnamese unchanged; the English benchmark loses 9 sentences whose
    references read plural or cardinal forms (a four kilometers extension, Boeing seven hundred
    thirty seven, yds kept). Contract en_ja_dev_v97 (39).
- Owner decisions on the v2 questions (2026-10-16):
  - Korean letters and digits: a number of two digits or more reads in Korean (WELCOME2026 웰컴
    이천이십육, IP68 아이피 육십팔, SAVE20 세이브 이십, iPhone15 아이폰 십오); long codes read digit by
    digit in Korean with 공 (KE081 케이이 공팔일, ABC123 에이비씨일이삼, RTX4090 알티엑스사공구공,
    R-20261015 알이공이육일공일오); one digit keeps the English numeral (K7 케이세븐, best5, GPT-4
    지피티포); top10 탑 텐 and covid19 코비드 나인틴 stay set phrases; a hyphen inside a code is no
    minus sign (context ko-KR code_hyphen 1: KS-2026-0418); a lowercase run without a vowel is
    letters (rm1207 알엠). Resource: alnum_number ko-KR initialism target-digits (Was:
    source-digits) and word_label single-digit.
  - Japanese kWh キロワットアワー (Was: キロワットじ); the scorer accepts both.
  - Address words from the dictionary only: common mailbox and site words in ko and ja (recruit,
    receipt, feedback, soudan そうだん, renraku, yoyaku ...) and Vietnamese words written without marks
    (tracuu tra cứu, hotro hỗ trợ, lienhe liên hệ, hanoi Hà Nội); given names stay spelled.
  - v2 spans: ko 94.0 → 95.2, ja 94.7 → 96.0, zh 98.6, vi 96.9 → 96.5 (the v2 gold keeps hotro and
    hanoi as written but reads tracuu). Owner sets dev: ja 97.1, vi 98.7, others unchanged; Korean
    quick, JSUT, ja-tn-eval, Chinese and Vietnamese gates unchanged.
  - Frozen contracts changed with Was notes on these decisions: code_word_v21 (4), letter_label_v21,
    ko_alnum_identifier_v23, ko_alnum_models_v24 (2), handle_v21, control_packs_v92. Contract
    owner_dec_v96 (15).
- The owner's synthetic v2 sets for ko, ja, zh and vi (project-owner request, 2026-10-15; 400
  sentences each, kept local under benchmark/generated/owner-v2-src; owner_v2_eval.py):
  - Spans on the frozen code 41f1190 → now: ko 92.0 → 94.0, ja 90.7 → 94.7, zh 96.7 → 98.6,
    vi 94.5 → 96.9 (sentences 90.2 → 92.7, 87.8 → 93.0, 95.8 → 98.2, 93.0 → 96.2). No engine errors.
  - Korean: an explicit + before an amount (+320억 원 플러스), 2×23,000원 (a grouped operand),
    ㎞ ㎏ ㎝ ㎜ ㎖ ㎎ ㎡ ㎥ ㎾ ㎞/h, g/dL, nit(s), 12.14(월) a dotted date with a weekday, 1:1.2 일 대
    일 점 이, a slash date range with a spaced mark or at the end of the text (11/7 ~ 11/9).
  - Japanese: codes read every digit group digit by digit (KS-2026-0418), letters and digits that
    alternate three times are a code (R7K2M9, 7G3K2), 毎月1日 ついたち, 2026/12 年月, 〒100-0005,
    (月・祝) keeps its marks, ￥ after kana, Japanese volt cues, X/1 is a date (4/1入社), +2,350歩 keeps
    the sound change, 号車 ごうしゃ, 滴 てき, g/dL, nit.
  - Chinese: +800 after hanzi 正八百 (not after 区号 or a phone word, not +1), 今天-1200 负,
    ¥428×3 乘以, ￥980元 reads 元 once, 10/20 18:30, bj.12306.cn, A7Q3B91KD, 按/稀释 ratios, 10.1当天,
    10.18的演唱会, 10/20前, nit.
  - Vietnamese: 31.10, before a comma a day after the twelfth is a date (phiên bản 14.2 stays), a
    spaced x after a quantity or a counted word (8 x 2 + 7, 1 viên x 2 lần), an X inside a code
    (K7X2QL), > 10.000 lớn hơn, + between counted words, đêm 13.10, dL, µmol.
  - Core: U+FFE0-FFE6 are symbols (￥980 after kana); a math operand keeps three-digit comma groups;
    the segmenter keeps the space after = + x < > × that follows a spaced number (a long expression
    early in a text was cut before =).
  - Owner sets: dev ko 96.0 → 96.2, ja 96.7 → 97.0, zh 98.7 → 98.7, vi 98.4 → 98.6, en and th
    unchanged; no dev span lost. Korean quick, JSUT, ja-tn-eval, Chinese (eval 346/346, held-outs
    equal or better) and Vietnamese gates unchanged; en and th v2 outputs identical.
  - Frozen contracts changed on the owner v2 evidence (its sign category reads every written plus):
    golden ko +1억2천만원 플러스 일억 이천만 원 (Was: 일억 이천만 원); test_scaled_currency +A$2만
    플러스 이만 호주 달러, value +20000 (Was: 이만 호주 달러, 20000).
  - Contract v2_four_v95 (42). Owner questions left: account names in addresses read as words in the
    v2 gold (recruit, jiho.park, soudan, tracuu) against the spelled policy; Korean English-digit
    labels (WELCOME2026, IP68) against the v2 gold; 14팀 stays Sino (개발 1팀 is a name).
- English on real sentences (en-US, project-owner request, 2026-10-15):
  - The owner's v2 of 398 real sentences (scb-mt-en-th-2020, CC BY-SA 4.0, kept local;
    owner_v2_eval.py): 88.2 on the frozen code b39e1f0, 95.8 now. 13,105 real sentences: no
    errors before or after, sentences with digits left 0.3% to 0.2%. Owner set dev 95.9, test 88.8.
  - Inch marks (8-3/8"Dia. inch diameter, 36"L inch long, 8"x 7"), an x spaced on one side
    (97.6x 57.1x), W x H, day ranges as ordinals (15 to 30 June, March 4 to 15), July-September,
    a spaced en dash between numbers, two and a half, 25 sheets/pack and 10/Pack per, $53.7-million,
    $US6 million, Q4/2010, .SPX, Pkg, Qty, Hwy, sqm, Km.
  - Core: a hyphen after a code with a letter is no minus sign (EP-001-1, LC6-40mm); a spaced
    North American 3-3-4 number is a phone number without a cue; an English address label splits
    where its case changes (BeautyExpert.com, TBDress.com).
  - Frozen contracts changed on the owner v2 evidence: en_digits_left_v25 mixed numbers read
    "and a half" (Was: and one half); phone_cases_v18 en-unlabeled-spaces is a phone number.
  - Contract en_real_v94 (23). The other five languages on the owner sets (outputs identical), Korean quick, JSUT, ja-tn-eval, Chinese and Vietnamese gates are unchanged; the English benchmark gains 12 sentences; all contracts pass except the three known failures.
- Thai pack phase 4 (th-TH, project-owner request and decisions, 2026-10-15):
  - The owner's v2 of real sentences (scb-mt-en-th-2020, CC BY-SA 4.0, kept local): 87.5 on the frozen
    code (a Korean request 86.5, one engine error), 96.1 after this phase (95.9 in a Korean request).
    14,927 real sentences: errors 12 to 0, sentences with digits left 1.7% to 0.6%.
  - Owner decisions: capitals alone in a Thai sentence are spelled (OTP โอทีพี); 2G-6G, 4K and 8K
    read English digits (ไฟว์จี). th_pack1_v89 otp now expects โอทีพี (Was: OTP as written).
  - Fixes: a long Thai run before an abbreviation was cut inside a character (invalid UTF-8); a
    list number before a Thai sentence read in the request language.
  - Real-text forms: space-grouped thousands, สรอ., มอก., product sizes with decimals and units, a
    slash after a counted word (ต่อ), x between Thai words, inch marks, falling model numbers, phone
    numbers after a cue or with uneven groups, one-sided dashes, a decimal before แสน/ล้าน, CNY,
    +/-, &, a lone %, the split sara am (ดํา ดำ).
  - names.tsv: 13,699 romanized Thai names for address labels (Wikidata CC0, pythainlp royin
    Apache-2.0; tools/thai/proper_names.py), +7 ms engine creation.
  - Owner set: dev 99.4, test 94.7; held-out v1 96.0. Contract th_pack4_v93 (28). The other five languages on the owner sets (outputs identical), Korean quick, English, JSUT, ja-tn-eval, Chinese and Vietnamese gates are unchanged; all contracts pass except the three known failures.
- Explicit controls in the Vietnamese, Thai and Chinese packs (project-owner request, 2026-10-15):
  - The packs passed every control through as written (1234{@digits} stayed 1234). Controls now
    reach the shared control grammar and @email/@url the electronic renderer; a control object is
    marked layer USER and the pack writes it back as text it reads (15/10/2025, 2025年10月15日,
    $12.50, thứ 4) and classifies that text with its own grammar (ttf_control_reread).
  - The Korean-only @native, @sino, @counter and @address are invalid there and the text reads
    as usual; @alphabet keeps Latin letters in vi-VN and zh-CN.
  - Every language: v1.2.3{@version} and 15%{@percent} bind without a group.
  - Contract control_packs_v92 (35). The owner-set outputs of all six languages are identical; Korean quick, English, JSUT, ja-tn-eval, Chinese and Vietnamese gates are unchanged; all contracts pass except the three known failures.
- Thai pack phase 3 (th-TH, project-owner request, 2026-10-15):
  - Held-out set th-tn-eval v1 (250 sentences, 297 spans, two authors without the pack): 81.5
    at first (a Korean request 71.4), 94.6 after this phase (both requests).
  - Maths (12 + 8 = 20, x², √, ÷, > <), durations 2:05:37, weekday and spaced-dash ranges,
    accounts 123-4-56789-0, postal codes, flight codes TG 628, บ้านเลขที่ 47/12 and 150/90 มม.ปรอท
    with ทับ, ~ ประมาณ, ฯพณฯ, ลบ., short unknown address labels spelled, 67-0412 a code.
  - Core: Thai digits read as digits in every request; a period after a Thai letter ends no
    sentence (routing and segmenter); a unit cut from a long Thai sentence counts the previous
    unit's Thai letters; the Thai pack classifies alone in its sentences (thai_sentence, like
    han_sentence and latin_sentence); electronic labels carry the rule electronic.label.
  - Owner set: dev 98.9 to 99.2, test 92.5 to 93.9. Contract th_pack3_v91 (20);
    th_pack1_v89 date-abbr joins the year to the date span (Was: a space before it). The other five languages on the owner sets (outputs identical), Korean quick, English, JSUT, ja-tn-eval, Chinese and Vietnamese gates are unchanged; all contracts pass except the three known failures.
- Thai pack phase 2 (th-TH, project-owner request, 2026-10-14):
  - Thai spellings of 139 English words (electronic_word: iphone ไอโฟน, save เซฟ, report รีพอร์ต) in
    email, URL, handle and file name spans and in a known word with a number (SAVE50, COVID-19,
    Boeing 787); unknown capitals spell with Thai letter names.
  - Thai document numbers and licence plates (นร 0505/ว 212, กข 1234), 1 1/2 + classifier
    (หนึ่งเม็ดครึ่ง), month/year 08/2027 and equal pairs 11/11 with ทับ.
  - th_pack1_v89 latin-word now expects ไอโฟนสิบสาม (Was: iPhone สิบสาม), as the owner's set reads
    iPhone 13.
  - Span accuracy on the owner's set: dev 95.6 to 98.9, test 91.6 to 92.5. Contract th_pack2_v90
    (13). The other five languages on the owner sets (outputs identical), Korean quick, English, JSUT, ja-tn-eval, Chinese and Vietnamese gates are unchanged; all contracts pass except the three known failures.
- Thai pack phase 1 (th-TH, project-owner request, 2026-10-14; the owner's set user-tn-eval th-TH):
  - Numbers (เอ็ด, ยี่สิบ, ล้าน groups), decimals, dates with month abbreviations and the year as
    written (พ.ศ. พอสอ), times (นาฬิกา), money (บาท, ฿, THB, 65.-), units, ranges, scores and
    ratios (ต่อ), fractions, phone numbers, IDs and codes digit by digit, IP, versions, Thai
    letter abbreviations (ภ.ง.ด.91, ม.3/5). English words stay as written.
  - Core: a sentence with Thai letters routes to th-TH in any request (thai_sentence); Thai digits
    are digits and fold to ASCII in a th-TH request; the CLDR Thai symbol names.
  - Span accuracy on the owner's set: dev 95.6, test 91.6 (from 0.4). Contract th_pack1_v89
    (24). the other five languages unchanged on the owner sets, Korean quick, English, Japanese, Chinese and Vietnamese gates unchanged.
- User eval phase 7 (generalizable forms from the remaining dev misses, 2026-10-14):
  - English: an IP octet of one or two digits reads as a number (10.0.4.17 ten dot zero dot four
    dot seventeen; 192 stays one nine two), codes whose letters and digits change more than once
    (WDT750SAKZ, 7KXJ442) read one by one, two years in order read as years without a cue
    (built 1998-2004).
  - Japanese: mAh ミリアンペアアワー, mg/dL, mg/L, N·m, 128/82mmHg with の, AC100V ボルト, ２杯目 め.
  - Chinese: × after 计算 reads 乘以, grouped numbers inside an expression, digit domains
    (12306.cn 幺二三零六点cn, www 三W), 2026.10.20-10.31.
  - Vietnamese: 5! = 120 giai thừa (a factorial before an operator no longer ends the sentence),
    ₫1.590.000, Sáng 2-1 and Ngày Nhà giáo … 20-11 dates, Mỗi ngày 2-3 lần stays a range (it read
    as a date on main), khung hình/giây một, l/100km trên.
  - Korean: 010.3304.6671, spaced digit groups after a number label, WELCOME10 (welcome added to
    the code words); scorer Z 제트 = 지.
  - Span accuracy, dev / test: ko 96.0 / 91.8, en 95.9 / 88.5, ja 96.7 / 92.2, zh 98.7 / 96.1, vi 98.4 / 94.8. No dev span lost.
    Contract user_eval7_v88 (20). English bench speech +11 and text +11 against the references, ja-tn-eval direct +22 (ours unchanged), zh held-out 3 +2 and 5 +1, Korean quick +2, Vietnamese unchanged.
  - Contracts updated with Was notes (owner-set evidence): ja_pack_v36, ja_rest_v50, ja_rules_v43
    read 目 after a counter as め.
- Vietnamese parity with the English pack (project-owner question, 2026-10-14):
  - Bug (also on main): a Vietnamese sentence with a Hangul, kana or hanzi word (Khách hàng 김민수
    đã đặt phòng) lost its sentence language, and its words were spelled as English letters
    with the Vietnamese letters dropped. Such a sentence stays Vietnamese when the Vietnamese
    weight is at least two and twice the number of other-script words.
  - The English pack no longer infers letters from a word with non-ASCII letters (Văn, phở),
    and Korean and Japanese read accented Latin words without their accents (Pokémon 포케먼,
    phở 포) instead of dropping the letters.
  - The CLDR Vietnamese symbol names (resources/unicode-fallback/vi.*) were never copied when the
    pack was added, so ® read "registered"; they are in now, with ★ ✓ and five others added.
  - Vietnamese abbreviations (TP.HCM, TS., PGS.TS, THPT, UBND, v.v. … 46), enclosed numbers and
    letters (① ⑴ ❶ Ⓐ), 10 a.m. / 9:00 SA / 5:00 CH, tỷ lệ 3:2 ba trên hai, $15 triệu. at a
    sentence end. Contract vi_parity_v87. user eval, Korean quick, English, Japanese, Chinese and Vietnamese gates unchanged.
- User eval phase 6 (address words, date ranges, blood pressure, expression marks, 2026-10-14):
  - Owner decision: a Japanese range keeps the plain first number (10〜15分 じゅうからじゅうごふん); the
    scorer accepts the first number with or without its counter. Also scorer variants the sets
    write both ways: ja example イグザンプル = エグザンプル, アットマーク = アット; ko A부터 B까지 = A에서 B.
  - Addresses: common address words read as words (Korean help 헬프, news 뉴스, support 서포트 ...;
    Japanese status ステータス ...), a label of two address words reads both (fastpay 패스트페이,
    oncall 온콜), and an English label of seven letters or more that splits into two English-looking
    words reads as a word (fieldscope, lincolnschools). Given names (john) stay spelled.
  - Korean: 9/28~10/1 a date at both ends, 혈압 150/95 백오십에 구십오 (and 140/90mmHg).
  - English: 1½ hours an hour and a half, 2½ two and a half, 7¾ in seven and three quarters
    inches, *67 star six seven, localhost:8080, Oct 24-26 October twenty fourth to twenty sixth.
  - Chinese: 10万+ and 10w+ at the end of an amount 十万加 (500+条 keeps 五百多条), 摄入<消耗 小于,
    12V×2A=24W 乘以 and 等于, 203诊室 二零三, @小鱼爱做饭 艾特.
  - Vietnamese: a test result (-) âm tính, (+) dương tính.
  - Span accuracy, dev / test: ko 95.4 / 91.5, en 95.6 / 88.5, ja 96.1 / 92.2, zh 98.0 / 96.1, vi 97.6 / 94.5. No dev span lost
    against phase 5 rescored with the same scorer. Contract user_eval6_v86 (18). English bench speech +2, text +1 against the references; Korean quick, Japanese, Chinese and Vietnamese gates unchanged.
  - Contracts updated with Was notes (owner-set evidence): electronic_v21 (help, support),
    handle_v21 (user, team), zh_pack3_v66 (10w+了 十万加).
- User eval phase 5 (codes, time ranges, era dates, weekday marks and expressions, 2026-10-14):
  - Shared: an unlabelled 1-800-555-0132 (the 1-3-3-4 layout) is a phone number, and a 4-3-3
    number starting with 0 (0120-456-789) is a phone number rather than a malformed date.
  - Chinese: 第4.2节 第四点二节, 3h45m 三小时四十五分钟, UTC+8 UTC加八, A7-21-33 A七二一三三,
    0755-12320 digit by digit, 转8021, RTX 4070 (index values such as AQI 165 keep the number),
    spaced digit groups after a code label.
  - English: 8am–1pm to, (x + 2)(x − 3) times and minus, x² squared (units such as km² keep their
    reading), 1.2M million, 4 min 12 s and a 90 s target (only with a time cue), 737 MAX seven
    thirty seven, CAD 64.30 and CHF 40.
  - Japanese: 前年比＋2.8％ プラス, R7.6.30 れいわななねん…, 座席は23A, 24:00 にじゅうよじ,
    2^10 にのじゅうじょう, spaced phone and ID groups with の, digits in 「」 after 下4桁,
    クーポンコード and ホットライン.
  - Korean: 10/13(월) 시월 십삼 일 월요일, 0427번 공사이칠번, 1080p 천팔십 피.
  - Vietnamese: ¾ ba phần tư, × nhân in a shared expression, x = 5 bằng.
  - Span accuracy, dev / test: ko 94.2 / 90.7, en 94.7 / 88.5, ja 94.5 / 90.9, zh 97.3 / 96.1,
    vi 97.5 / 94.5. No dev span lost (51 gained). Contract user_eval5_v85 (28). Korean quick
    +6 / -1 against the references (leading-zero 번 numbers), zh held-out 5 +2, other gates unchanged.
  - Contract updated with Was notes (owner-set evidence): slash_date_v21 reads a weekday mark
    after a date as the weekday (구 월 이십오 일 목요일).
- Vietnamese (vi-VN) is in production (project-owner decision, 2026-10-13).
  - TTS -> STT comparison: OpenAI, all 257 held-out v6 sentences. 63.8% were read right from the
    raw text and 93.8% from our output.
  - The six TN issues it found are fixed: đ/USD, the word euro, +amounts, a card expiry 12/28,
    spaced account numbers, double days 12.12 (contract vi_prod_v80, 9 cases).
  - Still to do: a human native review of the evaluation sets and a listening comparison with
    the target TTS (VoxCPM2).
- Mark control: in annotated input, `{@mark}` and `{@mark:name}` mark a position without
  changing the reading (no output text; a space on both sides collapses to one).
  - Each marker is one zero-width span (rule control.mark, unit @mark, value = name), so a
    stream callback can insert a non-speech sound such as a cough at that offset.
  - A marker never binds the atom before it, does not force a segment boundary, and gives the
    same result in every chunk division (tests/integration/test_mark_control.py).
- Vietnamese (vi-VN) phase 6:
  - Fixes: $3,500, tr.đ, lúc 22g, d/m after hạn, month/year ranges.
  - Bug fixes: a cue must be a whole word (khuyến mãi is not mã), and a dash pair after ngày
    stays a range.
  - Results: held-out v6 (257 sentences, opened after the code freeze) scores 96.8% of spans. The
    new-set scores have held near 97% since phase 2. Contract vi_pack6_v79 (9).
- Vietnamese (vi-VN) phase 5:
  - Fixes: 1tr5, k ranges, dot dates with ngày earlier in the clause, mã nguồn, identifiers
    after số and cổng, µg and kN.
  - Bug fix: a billion amount with dot groups (1.250.000.000 đồng) is no longer read as an IP
    address.
  - Results: held-out v5 (256 sentences, opened after the code freeze) scores 97.7% of spans (en/ko
    requests 97.1%). Contract vi_pack5_v78 (15).
- Vietnamese (vi-VN) phase 4, category-level work:
  - Science and IT units.
  - Formats: dimensions (nhân), ÷ ranges, ISO and dot dates, holiday date cues.
  - Codes: road codes (QL1A), tour lengths (4N3Đ), short numbers, bare I/V/X Roman numerals.
  - Results: held-out v4 (260 sentences, opened after the code freeze) scores 97.1% of spans. The
    new-set scores by phase are 93.2, 97.1, 95.6 and 97.1. Contract vi_pack4_v77 (22).
- Vietnamese (vi-VN) phase 3, fixing the failure types of held-out v2:
  - A code cue stays in its clause.
  - Notations: 1m05, an en dash minus, 35 tr, nm, 4.0, triệu ha.
  - Policy: an amount over a word reads một (đồng một tháng); a unit over a unit keeps trên.
  - Results: held-out v3 (258 sentences, opened after the code freeze) scores 95.6% of spans. The
    new-set scores by phase are 93.2, 97.1 and 95.6, and the misses are scattered. Contract
    vi_pack3_v76 (11).
- Vietnamese (vi-VN) phase 2, fixing the failure types of held-out v1:
  - Dates: date ranges and dash dates.
  - Notations: 1m78, P./Q. (phòng, phường, quận), CN, IP addresses, long codes.
  - Amounts: scale words, trên with a unit, unit/unit, mmHg and decree numbers.
  - Segmenter: a cut decision classifies a number from its start (85,5 triệu stays whole).
  - Results: held-out v2 (257 sentences, opened after the code freeze) scores 97.1% of spans,
    against 93.2% for v1 at the phase 1 freeze. Contract vi_pack2_v75 (26). Other languages are
    unchanged.
- Vietnamese (vi-VN) phase 1, in development (docs/vietnamese-pack.ko.md). Readings are northern
  standard.
  - Coverage: numbers (nghìn, linh, mốt, lăm; the dot groups thousands, the comma is the
    decimal mark), dates, clocks, money, units, ordinals and Roman numerals, phone numbers and
    codes digit by digit.
  - Routing: a Latin sentence written with Vietnamese letters is routed to vi-VN in any request
    (latin_sentence). Letters shared with French and Spanish count less, and French, Spanish and
    German function words rule a sentence out. A sentence cut into units keeps its language.
  - Tokenizer: U+1E00-U+1EFF and combining marks are now Latin (Việt was split into three
    tokens).
  - Held-out v1 (240 sentences, opened after the code freeze): 93.2% of spans. The gold was
    cross-checked with an OpenAI model, and only disagreements were reviewed.
  - Contract vi_pack1_v74 (49). Korean, English, JSUT, ja-tn-eval and the Chinese sets are
    unchanged.
- Second mixed-text review (project-owner sample):
  - An e-mail address or URL in a Chinese sentence no longer fails the request with invalid
    resource. It reads abc 艾特 qq 点 com: a new resource kind `address_symbol`, digits read one by
    one. An address no pack can read keeps its written form.
  - A single-quoted phrase with a space, in a sentence without Hangul, is its own context, so
    'Please send 3 files before 5:30 PM' inside an English quote is no longer read in Japanese.
  - U+20A9 is spelled: 유 플러스 이공에이구, you plus two zero ay nine, ユー プラス にぜろエーきゅう.
  - English: 1M requests reads one million (K/M/B before a plural noun), requests/day reads per
    day, a bare ≥ reads greater than or equal to, 24/7 reads twenty four seven (a ratio of 24/7
    stays over), and v2.1 reads version two point one.
  - Korean readings English 잉글리시, Tech 테크, Set 세트 (setup stays 셋업) and regex 레젝스 (never 정규식).
    Particles stay as written.
  - Contract mixed_lang2_v73 (32). Korean quick: changed 0 (142,255문장). English, JSUT, ja-tn-eval and the
    Chinese sets are unchanged.
- Chinese (zh-CN) is in production (project-owner decision, 2026-10-13; beta since
  2026-10-11). An OpenAI TTS -> STT comparison of 200 held-out v8 sentences read 72.5% right
  from the raw text and 96.0% from our output; still to do: a human native review of the
  evaluation sets and a listening comparison with the target TTS (VoxCPM2).
- Jev next to Decisions on the Korean benchmark: the same `different` sentences are judged by
  Jev too (tools/web/ko_jev.py, jev-final.json). /ko-quality lists the sentences both judges
  reject ("Jev·Decisions 모두 틀림", 2,388 sentences, 1.68% on 20261012-mixed) sorted by either
  judge's score or their mean, ascending or descending; Decisions accepts 18,595, Jev 11,662.
- OpenAI Decisions as the last stage of the Korean benchmark: only the sentences left
  `different` are judged (tools/web/ko_decisions.py, decisions-final.json; accepted at 0.7 with
  nothing left unread and no changed number value). /ko-quality shows the black (Decisions
  accepts) and gray (wrong even for Decisions) segments, a card, a category column, two filters
  and the request. 20261012-mixed: 3,092 sentences (2.17%) wrong even for Decisions out of
  21,687 judged; the judge errs both ways, so it is a coarse indicator. The leftover /jev-eval
  navigation links are removed.
- Jev as the last stage of the English benchmark: only the sentences the reference
  benchmark does not accept are sent to Jev (tools/web/en_jev.py, jev-final.json per report);
  /en-quality shows a colored progress bar (exact, spacing, case/punctuation, spelling variant,
  then misses Jev accepts in black, the rest wrong even for Jev), a "wrong even for Jev" card and
  column, and two case filters. Speech: 568 sentences (2.96%) wrong even for Jev, text 860
  (4.48%); English Jev is uncalibrated, so these are lower bounds. The /jev-eval page that scored
  every sentence with Jev is removed.
- Mixed Korean/English/Chinese/Japanese text (project-owner sample): the hanja/kana reading
  options now apply only inside Korean sentences and to bare words; a whole Chinese or Japanese
  sentence (a full stop, or ten CJK characters) stays Chinese or Japanese (was read character by
  character). 627 simplified and Japanese new-form characters take their traditional reading
  inside Korean sentences (中国チーム에도 중국; a bare 中国 日本 keeps 중国 as frozen). In zh-CN and ja-JP requests a Korean or English sentence keeps its own numbers and
  letters (오전 9시 was 九시 or きゅう시; AI in an English sentence was エーアイ). I'll in a
  Korean sentence is no longer I'l l. English days, months and 69 given names read correctly in
  Korean (Friday 프라이데이, John 존, Alex 알렉스). Contract mixed_lang_v72 (41). Korean,
  English, JSUT, ja-tn-eval and the Chinese sets are unchanged.
- Chinese pack phase 8, on the categories held-out v7 missed: dotted month-days after a date
  word or before an event word (定于9.28开业), shopping festivals by digits (6.18购物节 六一八),
  hyphen scores (战成2-2 二比二), codes after a hanzi and a hyphen (建施-05), 15-15-15, ΔT=20 K,
  μS cm⁻¹ bit, μ 缪, spaced mmHg, (30+50+30)m, degrees and minutes, 1e-3, ％, ⅕..⅞, two-digit
  years (08年那场); 北纬30°15′N no longer repeats 北纬. Contract zh_pack8_v71 (27). An eighth
  held-out set from the neutral prompt (495 sentences): ours 97.0% (phase 7 96.9%), wetext
  73.6%, NeMo 55.5%. Korean, English, JSUT and ja-tn-eval are unchanged.
- Chinese pack phase 7, on the categories held-out v6 missed: units after a space in ranges,
  negatives and expressions (100~240 V, -0.35 V, 1 m处), mSv lx MeV Å n mile MVA dB(A) and more;
  ~ and ≈ before a number read 约; 31 more measure words for 两 and 二年级; colon durations,
  scores and ratios by wider cues (1:10,000 一比一万); dates as eight digits, 2027.06 and
  周报（9.22—9.26）; service numbers after 打, hex codes, 2#机组, chainages, n→∞, σ², pH=.
  Contract zh_pack7_v70 (42). A seventh held-out set (491 sentences): ours 96.1% (phase 6
  93.2%), wetext 67.5%, NeMo 49.9%. Korean, English, JSUT and ja-tn-eval are unchanged.
- Chinese pack phase 6, on the categories held-out v5 missed: units after a formula glued to
  hanzi (λ=550nm), K C J F as units inside formulas, J·s Ω·m kJ/(kg·℃), μF nF pF μL μs ns hm²
  r/min MJ GJ; standards (ISO 9001:2015, IATF 16949, GB/T 19001-2016) read digit by digit;
  bare versions (2.4.1); zero cents dropped, ￥2.6万, currency codes (JPY 1,500,000), ¥32.5/kg;
  +3价, 2核, 318国道; the phone cue no longer fires before an amount (手机起售价5499元).
  Contract zh_pack6_v69 (32). A sixth held-out set (491 sentences): ours 92.5% (phase 5
  91.4%), wetext 71.6%, NeMo 53.8%. Korean, English, JSUT and ja-tn-eval are unchanged.
- Chinese pack phase 5, by category: IPv4 addresses and ports (一九二点一六八点零点一冒号八零八零),
  QQ and group numbers, HTTP statuses after 报, Roman numerals (Ⅲ期 三期), Greek letters by
  name (α 阿尔法, 6σ 六西格玛, χ² 卡方), s d kN kn dL rpm dB, hyphened part and batch
  numbers as codes, latitude and longitude, year-month (2026/08), 30 more measure words; 3A游戏
  is no longer 三安. Contract zh_pack5_v68 (35). A fifth held-out set (487 sentences, everyday
  and professional): ours 92.4% (phase 4 88.1%), wetext 66.2%, NeMo 49.9%. Korean, English,
  JSUT and ja-tn-eval are unchanged.
- Chinese pack phase 4: per units after money (15.6元/股 十五点六元每股, ¥599/晚, 美元/盎司), units
  after a space (9.8 m/s²), MWh Ω N·m kJ mol⁻¹; fixed readings worse than the raw text found
  by held-out v3 (stock codes and years in brackets, notice numbers 2025-083, 33,165亿 was
  三万亿…, 618大促, 2:05.38, size codes, 1/1,000,000); scores before 战平, 9秒91, △DEF, T+0,
  88折, 4!. Contract zh_pack4_v67 (35). A fourth held-out set (500 sentences): ours 90.6%
  (phase 3 90.1%), wetext 70.9%: case-by-case fixes no longer generalize; the remaining
  errors are IT notation, Roman and Greek symbols, SI units and part numbers. Korean,
  English, JSUT and ja-tn-eval are unchanged.
- Chinese pack phase 3: compound units read 每 (15mg/kg 十五毫克每千克, 72次/分 七十二次每分钟,
  5.2万/㎡), minutes and seconds 2'30" 两分三十秒, more measure words (两晚, 三室两厅), dates
  (6/18那天, 12.12 双十二, 11-08), service numbers (12306), spaced phone numbers, case and
  standard numbers, subscripts (H₂O H二O), 95后, 10w+, No.1 第一, 4x4 四乘四. Fixed readings
  worse than the raw text: $500万 (was 五百美元万), 132,500万, H:MM:SS after 以. The shared
  invalid-range guard no longer holds a zh-CN span. Contract zh_pack3_v66 (39). A third
  held-out set (496 sentences, opened after the code freeze): ours 92.2% (phase 2 89.9%),
  wetext 72.8%. Korean, English, JSUT and ja-tn-eval are unchanged.
- Faster resource lookups: the resource hash table has 8191 buckets (was 1021, shared
  with the word and phrase tables) and lives on the heap, so request views copy one
  pointer. A lookup walked about 24 rows; a 30,000-number stream took 45 s and now 12 s
  (Release). The context_cache numeric-30000 test, near its 60 s limit in the Debug CI
  build, kept failing. Outputs are unchanged (Korean quick 0 changed, English, JSUT,
  ja-tn-eval, zh sets, all contracts).
- Chinese pack phase 2: maths (letters, superscripts, ≥ ≤ ≠ ≈ π ∠ ∛ °), more units
  (512GB 五百一十二GB, was 五一二GB; mmHg kcal nm ㎡ mmol/L h min), ratios (∶, 115:109,
  3:2:1), years (2035年前, 80后), dates (10.1假期 十一, 5·12), codes (502错误, plates),
  ranges with units and currencies, 10万+. Engine: a zh-CN request reads any sentence
  with hanzi as Chinese and zh-CN classifies only as the context (was: "at zh int"
  leaked into English); ， between digits is a clause comma in a hanzi sentence (was:
  846201，5 left unread). Contract zh_pack2_v65 (52). A second held-out set (500
  sentences, opened after the code freeze): ours 90.2% (phase 1 81.3%), wetext 72.3%.
  Korean, English, JSUT and ja-tn-eval are unchanged.
- Chinese (Simplified) pack zh-CN, phase 1, beta (project-owner request 2026-10-11;
  Claude reviews as the native speaker). Numbers read as Chinese numerals with 零, 万/亿
  and 二/两 (两个, 两千, 第二, 二月), years and codes digit by digit (一九九零年,
  验证码一二三四五六, 拨打幺幺零), dates, times, durations and scores (十四点零五分,
  两小时零五分三十秒, 二十一比十九), money (HK$100 一百港元), percent, fractions, ranges
  (3～6岁 三到六岁), unit symbols (60km/h 六十公里每小时, -5℃ 零下五摄氏度), letters with
  digits (CA一二三四, COVID十九) and simple maths (十除以二等于五). Routing: a hanzi
  sentence with no Hangul, kana or non-Simplified character (han_foreign: 駅 國 時 …) goes
  to zh-CN in any request except a Japanese one or the Korean hanja/kana options, and
  only zh-CN classifies there; symbols take CLDR zh names (🍔 汉堡). Was: 我买了이个苹果。
  in a Korean request; unicode_fallback_v24 北京 🍔 was 北京 햄버거, now 北京 汉堡. Contract
  zh_pack_v64 (54). zh-tn-eval: 346/346 declared, 98.0% routed (the rest are
  hanzi-free maths), wetext 85.5%; the set and the pack share an author, so this
  includes overfitting. Korean, English, JSUT and ja-tn-eval: unchanged (Korean quick 0 changed sentences, English 90.83%/86.80%, JSUT 203/332, ja-tn-eval 97.3%).
- Japanese is in production (project-owner decision, 2026-10-11; it was beta). Still to
  do: a human native review of the evaluation set (a 200-entry sample is prepared) and a
  listening comparison with the target TTS (VoxCPM2). An OpenAI TTS -> STT comparison of the
  200 entries read 70.0% right from the raw text and 85.0% from our output.
- Japanese fixes from an OpenAI TTS -> STT check of the 200-sentence review sample
  (raw text read right 69.5%, our output 82.5%), where a kanji left between
  readings was misread: 生 after 年 期 回 reads せい (1期生 いっきせい, 3年生の
  さんねんせいの; 生まれ and 生活 stay), 中 between counts reads ちゅう (10人中9人
  じゅうにんちゅうきゅうにん; 一日中 stays じゅう), 月 at the end of a kanji word before
  an amount or a frequency reads つき (家賃月8.5万円 家賃つき…, 月2回 つきにかい;
  今月 毎月 stay), and bit rates are read (1Gbps いちギガビーピーエス, a Japanese-only
  latin_unit). Contract ja_tts_fixes_v63 (14). ja-tn-eval direct 87.8% to 88.5%;
  the reader-based score and JSUT are unchanged.
- Japanese 頃 after a time, date or age count reads ごろ (around_suffix): 8時頃
  はちじごろ, 8時半頃 はちじはんごろ, 5月頃 ごがつごろ, 30歳頃 さんじゅっさいごろ; 年頃
  without a number stays a word. Found by an OpenAI TTS -> STT check of 20
  ja-tn-eval sentences (raw text 10/20 read right, our output 14/20): the TTS
  read はちじ頃 as ころ. Contract ja_goro_v62 (9). The evaluation set and JSUT
  are unchanged (their reader already read 頃 as ごろ).
- A title number chosen by its particle (project-owner request): a number up to
  ten written right after a word is read with the English numeral, as after a
  listed product name, when only that reading fits the particle the writer chose:
  디스트릭트9을 디스트릭트나인을, 아이언맨3를 아이언맨쓰리를, 배민1으로 배민원으로;
  when the Sino-Korean reading fits it stays (어벤져스2를 이를). A number after a space
  (숫자 9을) and the particles 가 and 로, which may be the address units 街 and 路
  (종로3가, 무명3로), choose nothing. Contract ko_particle_title_v60 (13). Korean
  benchmark +5, -0 (the user's sentence 디스트릭트9을 is one of them).
- Korean pronunciation output (project-owner request, 2026-10-09..10): the option
  pronunciation=hangul (CLI --pronunciation hangul, web 한국어 발음열) and the control
  [[group]]{@pronounce} write Korean as it is pronounced, colloquial and without
  vowel length (국물이 맛있다 궁무리 마싣따). The Standard Pronunciation rules
  apply within a word, plus:
  - exceptions and homograph main entries from 표준국어대사전
    (resources/languages/ko-KR/pronunciation/stdict_phonemes.tsv, CC BY-SA 2.0 KR,
    generated by tools/korean/stdict_phonemes.py with a self-check)
  - inflected forms (신고 신꼬, 할 수 할 쑤, 할 일이 할 리리)
  - same-spelling words with context cues (운동화만 신고 신꼬, 신고 접수 신고)
  - listed words with 하다 forms and in compounds (발생한다고 발쌩한다고,
    운전면허증 운전면허쯩)
  - numbers (칠십오 칠씨보)

  - digits read one by one, as separate words (112 일릴리, 119 일릴구; the written
    adverb 일일이 stays 일리리)

  X{@pronounce} on one atom reads it in its sentence (112{@pronounce}에 신고 일릴리에).
  Words across a space are not linked. A native review of 200 random corpus
  sentences: 94.5% of sentences and 99.4% of words right. Contracts
  ko_phoneme_v51..v59 (211). Output with the option off is unchanged.
- Japanese evaluation set ja-tn-eval-v1 (benchmark/datasets/ja-tn-eval, 1,250
  entries with references, 1,343 scored spans, native review by the author) and the gaps it found:
  〜 before kanji, counters, ratios and scores, M/D dates, phone and code contexts,
  and the rest (contracts ja_range_mark_v44, ja_gaps_v45, ja_counters_v46,
  ja_scores_v47, ja_slash_dates_v48, ja_gaps78_v49, ja_rest_v50). On the set: ours
  97.3%, wetext 76.9%, pyopenjtalk alone 76.6%, NeMo 73.6%
  (docs/japanese-benchmark.ko.md).

- Korean PIN cues and 번 counts from the benchmark review (owner: keep speaker
  variants such as 둘/하나 digits, 스무/이십, 티비/티브이 as they are): a code
  reads digit by digit when its password label follows the number (5141인
  비밀번호), a copula ending follows (9234였다) or it is pressed (2120을 눌러), a
  short code may start with 0 (0714번); 틀려/틀렸/오류/잘못 after N번 count, but
  N번을 틀려 stays a question number. Contract ko_pin_cues_v41 (12). Korean
  benchmark +44, -0 (adjusted 84.16%).
- English fixes from the benchmark review: Roman numerals of two or more I/V/X
  letters read as cardinals without a cue (Prime BEEF III three, XIII), a
  numbering word wins over a dotted initial (CHAPTER V. five) and Type, Level,
  Grade … are numbering words; period-less st, dr, mt read as titles before a
  name and st, ave, dr as street words after one (st Kilda Saint, York st
  Street; the references 61 : 0; three negative bare-abbreviation contracts and
  one EngConf row record the change), sr, bros, cwt in lower case; an unknown
  three-letter capital word ending in a vowel is letters (USA u s a, CEO, EPA)
  unless it is a common short word (short_word). Measured alternatives kept
  out: regnal after any capitalized name and an XX exception both lost
  references. Contract en_ref_fixes_v42 (20). English speech 88.15% to 90.83%,
  text 84.20% to 86.80%, EngConf 42% to 52%.
- Japanese rule fixes: ranges with a unit or percent (3〜5km さんからごキロメートル;
  10〜20% spelled its digits before), 〜 between spoken readings reads から,
  泊 発 歩 品 敗 take ぱ after 3 and 4 (さんぱく), 年度 代 年代 counters, 半 after
  a count (12時半 じゅうにじはん), digits with capital letters (3LDK さんエルディーケー,
  4K). Known gap: 〜 merged with following kanji (午前10時〜午後3時). Contract
  ja_rules_v43 (12).
- Korean 번 counts (project-owner request): an event verb right after N번 makes
  it a count with a native numeral (3번 말했다 세 번, 2번 말씀드렸습니다 두 번,
  5번 봤다 다섯 번), 55 counter_role_after frequency cues; label nouns keep the
  number (3번 출구 삼 번, 5번 버스 오 번). Cues that also mean entering, arriving
  or pressing (들어, 왔, 눌렀) were left out: the benchmark lost bus and button
  numbers with them. Contract ko_beon_count_v40 (16). Korean benchmark +25, -0.
- Japanese kanji numerals (project-owner decision; docs/japanese-pack.ko.md): in
  a Japanese sentence a kanji numeral with a counter (三本 さんぼん, 一人 ひとり,
  十月八日), a place structure (二千二十四) or a positional form (二〇二四年)
  reads like digits, as do 三分の一 and 一日中; a lone 一 or 千, 十分 not after
  時, Nつ before a kanji (四つ葉) and listed words (kanji_word: 八百屋, 三日月,
  五月雨, 一行 …) stay. Korean and kanji-only sentences are untouched. The
  phase 1 contract row that kept 三人 records the change. Checked against the
  503 JSUT kanji-numeral sentences (the reader already reads them, so this is a
  regression guard; remaining losses are reader effects and variants).
  Contract ja_kanji_numerals_v39 (37).
- Japanese measurement (docs/japanese-benchmark.ko.md, workbench /ja-quality):
  JSUT BASIC5000 hand-made kana readings (CC-BY-SA 4.0, local only) as the
  reference, every system read by pyopenjtalk: on the 253 digit sentences ours
  203, pyopenjtalk alone 199, wetext 196, NeMo 196 exact; our number-span
  accuracy 96.6% (adjusted 100%, review file benchmark/manifests/
  ja-jsut-review.json). Fixes found by the measurement, contract first
  (ja_bench_fixes_v38, 21): counters 服 等 脚 世紀 組 粒 セント 床 チーム and
  katakana units, 100キロ ひゃっキロ, 17日/27日 ななにち (was しちにち), a
  Japanese comma before three digits groups a number (１、０００), digit
  readings in phone numbers and codes lengthen 2 and 5 (にー, ごー; the phase 1
  phone contract records the change), and a number after a label (内線, 局番,
  請求書, 番号 …) reads digit by digit.
- Japanese phase 2 (project-owner policy; docs/japanese-pack.ko.md): a sentence
  with hiragana grammar (hiragana with kanji or katakana in one word, or attached
  to a number or word: 明日は, 3時に, iPhoneを), or only kana and no Latin word,
  and no Hangul, is a Japanese sentence in a Korean or English request too; the
  whole sentence goes to ja-JP and skips the Korean branches (context ja-JP
  kana_sentence; core kana_sentence in the matrix routes). A Hangul word keeps a
  sentence Korean, a standalone kana word in an English sentence and katakana
  alone are no evidence, kanji-only sentences follow the request language, and
  the Korean kana/hanja options and matrix=declared keep their behaviour. A
  declared-only pack is classified only where it is the context. In ja-JP:
  email and URL read with Japanese symbol names and domain words (they failed
  the request before), ① まるいち, ⑴ かっこいち, Ⓐ エー, symbol and Greek names,
  R&D アールアンドディー, weekday marks （水） すいようび, and the @year,
  @version, @ordinal (だいさん) and digit @alphabet controls. Contract
  ja_phase2_v37 (21).
- Japanese pack, phase 1 (ja-JP; project-owner request; docs/japanese-pack.ko.md):
  in a request declared ja-JP, numbers, counters, dates, times, currency and
  units read in hiragana with the counter sound changes (3本 さんぼん, 6匹
  ろっぴき, 1人 ひとり, 4月1日 しがつついたち, 9時10分 くじじゅっぷん, 1万円
  いちまんえん, 3分の1 さんぶんのいち), unit and letter names in katakana (5kg
  ごキログラム, USB ユーエスビー, A4 エーよん), while kanji, kana and Latin words
  stay written for the speech model. Counters, special readings and units are
  data (resources/languages/ja-JP). The pack is declared-only (context
  declared_only): it neither scores, classifies nor owns a script in a Korean
  or English request, and stays out of their context traces. Segments: a CJK
  full stop ends a sentence without a following space, 、 is a phrase cut (not
  between digits), and text without spaces cuts between kana or kanji at the
  length limit. Workbench language list adds ja-JP. Contract ja_pack_v36 (69),
  test ja_segments. Korean and English benchmarks unchanged.
- Circled and parenthesized letters read (project-owner request): Ⓐ ⓐ ⒜ read
  as the letter (English A, Korean 에이), Korean ㉠ ㈀ as the jamo name (기역)
  and ㉮ ㈎ as the syllable (가), where they were kept as written while ①
  already read (speech_symbol rows). A circled or parenthesized character
  joins a registered particle right after it (①과 일과, ㉮와 가와), while a
  noun keeps its space (⑷항 사 항) and other symbols keep theirs (*을 별표 을)
  (speech_output.c particle_next). Contract enclosed_letters_v35 (7). Korean
  and English benchmarks unchanged.
- The written function word chooses the reading (project-owner idea): in Korean the
  particle attached to a reading shows whether the writer's reading ended in a
  vowel or a consonant, and when ours disagrees an alternative that agrees
  replaces its end, the particle staying as written (normalize.c
  particle_reading; particle_reading resources): L와 라지와, S과 스몰과, a code
  ending 5281를 오이팔하나를 / 6422을 육사이둘을 (the references read codes so), 20여
  t이 톤이, and a spelled capital word with a fitting lexicon word (TIME이 타임이;
  only an inferred initialism, an explicit spelling policy stays). In English an
  article "an" before a consonant letter whose name starts with a vowel sound
  means letters (an LED l e d); "a" is no cue (a RADM, rear admiral). A scan of
  the Korean benchmark outputs found about 1,000 particle/reading
  disagreements. Contracts ko_particle_reading_v33 (10), en_article_reading_v34
  (5). Korean benchmark +47, -0; English unchanged.
- Korean benchmark misses sampled by hand (most are reference habits: 하나/둘 digit
  names, plate fronts, spacing); real errors fixed: a card number before 번
  (6900번 카드로), route numbers with a leading zero (수완 03번 공삼번, it was 세
  번) and branch routes (70-2번 칠십 다시 이번; 3-5번 문제 stays a range), 뿐 as a
  particle (하루에 다섯 번뿐), a hyphen between two spoken words is not said
  (K-팝 케이 팝; speech_silent between), 컵 with native numbers, and clothing
  sizes in a clothing sentence (펄블랙 66을 육육을, 55~77사이즈 오오에서 칠칠; not
  before 만, a decimal, an amount or a list of values; clothing_cue). Contract
  ko_bench_fixes_v32 (23). Korean benchmark +797, -4 (84.08%).
- Korean sentences Jev failed, judged by hand (project owner: Jev is not always
  right; about half were already correct, 4.3사건 사삼사건, 2세 이 세): hotlines
  1301, 1339, 1382, 1391, 1399 … read digit by digit with a call cue (call_cue
  이용 added), a four- to eight-digit code in a password or keypad sentence
  or keyed in (pin_cue; 3043을 입력 삼공사삼; list members too), 231-29번지 이백삼십일
  다시 이십구번지 (not a range), 타율 0.212 이할 일푼 이리 with the following
  particle agreeing (육리를; rate_cue), score cues 잡고/쿼터/마쳤/이닝/전반/후반,
  and 경기 counting with native numbers (아홉 경기; references 176 : 24). Bus
  routes keep their values (the references 277 : 212 against digits), and
  plates keep 삼십일후 (owner judgement). Contract ko_jev_fixes_v31 (20). Korean
  benchmark +768, -29; Korean Jev passed 3448 → 3461, failed 82 → 72.
- EngConf misses reviewed by hand: St./Dr./Ave./Rd./Blvd./Ln. after a name read
  as street words (Grey St. The fare was Grey Saint; St. Louis stays Saint;
  abbreviation_street), Maj./Gen./Col./Capt./Lt./Sgt./Gov. titles, US states
  after a city and a comma (us_state), dotted A.M./P.M. times and no o'clock
  before a meridiem (four a m), short capital words among capitals
  (caps_word), VOL 24 NO 39, 1/2 oz., V75S, PMID digits, and # kept with its
  number across a segment cut. Seven contracts that kept Oak Dr. or o'clock
  record their previous values. Contract en_engconf_v30 (23). English speech
  88.03% → 88.14% (EngConf 35.5% → 41.6%); Korean unchanged.
- Low English Jev scores reviewed by hand (project owner: Jev is not always right):
  a written article before a day-first date is not repeated (the 9th October),
  a capital No before a number inside a sentence reads number (Route NO 128),
  STORY XXIII, ISBN groups split by spaces, and in English a pronounceable host
  label reads as a word (Scrum.com, reasonradionetwork.com; spiegel.de records
  its previous value). Kept on purpose: spelled capitals and NNNN. year lines.
  Contract en_jev_fixes_v29 (12). Jev (600 sentences) 81.5% → 84.5% (wetext
  79.2%); English benchmark speech 87.67% → 88.03% (+69, -0); Korean unchanged.
- A segment's classifiers see the last 64 bytes of the previous segment, so a
  number at a segment start keeps the words before it (Douglas Haig born. 1880.
  eighteen eighty; ( b . 1952 )). Only classification sees the prefix: routing,
  output and spans are unchanged, and batch and stream share the units. A month
  name that ends a sentence is not a cue (May. 2026 stays a cardinal). Contract
  en_segment_context_v28 (7). English benchmark speech 87.58% → 87.67% (+16,
  -0); Korean unchanged; against wetext 88.34% : 68.38%.
- English speech reads rejected numeric spans as written, like the Korean
  written fallback (project-owner decision: read leftover digits as wetext
  does; protected code stays): 97X ninety seven X, 3771A digit by digit, 787s
  seven eighty sevens, 12pm-2pm twelve p m to two p m, 1861--1865 as years,
  99.9FM, 1/7th one seventh. Also scripture ranges after a book name (Micah 6:6-8
  chapter six verses six to eight; scripture_book), year-first dates (2007 Apr
  24), spaced ISBN groups, and a lone NNNN. line as a year (owner left the
  choice to the recommendation; 2026 without the period stays a cardinal).
  Contract en_fallback_v27 (23); seven guard contracts record their previous
  values. English benchmark speech 87.11% → 87.58% (+91, -0); Korean
  unchanged; unread digits 75 → 7 sentences; against wetext 88.27% : 68.38%.
- English gaps against wetext (project-owner question: does wetext read anything
  we do not, and do digits or symbols pass through). A segment no longer starts
  with a number after a word, nor cuts right after a word carrying a number
  (112에, 2-1로) or inside a TeX quote, so a number keeps its context (from 1918
  to 1923, the # 7 seed; an unseen next token waits, so streams cut like
  batches). # 7 number seven, No. 5 number five, Rev./Hon. titles, km2/m2/mi²
  squares and lbs, ISO years before 2000 read as calendar years (numeric_year_style
  cardinal-from-2000; 2026-09-16 keeps two thousand twenty six),
  English m/d/yyyy dates month first (25/12/1999 day first; 6/17/13 thirteen),
  capital ordinals (24TH), THURSDAY as a word, years split as 19 44, ISBN digits
  with a final X (the phone contract that kept an ISBN records its previous
  value), and a silent dash after a period or colon keeps a space.
  Contract en_wetext_gaps_v26 (35). English benchmark speech 86.52% → 87.11%,
  text 83.16% → 83.54%; Korean +28, -0; against wetext 87.96% : 68.38%;
  sentences with unread digits 145 → 75, wetext-only 771 → 693.
- English outputs that still carried digits (project-owner request; 346 of
  30,308 evaluation sentences, wetext none) are down to 144: a double backtick
  that opens prose is a TeX quote, not code (`` Nepal Census 2001 ``; decided
  from the opening so streams agree, and an odd run on the line closes it);
  a doubled hyphen after a number or a separator is a dash (CHAPTER 16 --,
  Scene 2:--); a spaced hyphen before a word ends a range (1957 - 1974 -
  Bryantsev); a named date takes a three-digit year (July 18, 453 four fifty
  three); proper fractions up to sixteenths read with their denominator (1/4
  one quarter, 3/4 three quarters, 2/3 two thirds; references 51 : 0 against
  over; fraction_denominator), and an English mixed number reads "two and one
  half". Nine contracts that pinned "over" record their previous values.
  Contract en_digits_left_v25 (19). English benchmark speech +632, -39
  (85.51% → 86.52%), text +525, -9; Korean unchanged; against wetext 87.48% :
  68.38%.
- English bare years (project-owner goal after the wetext comparison): a
  four-digit number from 1100 to 2099 in English text reads as a year without
  a cue (Civil Rights Act of 1964 nineteen sixty four; references 6537 years
  : 88 cardinals). It stays a cardinal before a unit or count word (1146
  miles, 1500 soldiers, 1996 degrees), after a label (page 1965) or a
  quantity word (rose to 1965, paid 1865, The count is 2026, a list after
  scores were), when it stands alone, or near Hangul, Han or Kana; the test
  reads the same bounded window as the cue rules so streams agree with
  batches. New date_policy uncued_min_year and date_year_label_before /
  date_year_quantity_before tables. Contract en_bare_years_v25 (11); the
  v21 contract "1965 was the year" records its previous cardinal, and the
  phone parenthesis row uses ( 3003 and 3005 ). English benchmark speech
  +400, -0 (83.52% → 85.51%), text +374, -0; Korean unchanged. Against
  VoxCPM2's wetext: 86.84% : 68.38%, held-out 89.13% : 72.97%. A Jev check on
  600 sentences where the two differ: 81.5% : 79.2% at 0.7, so most of the
  reference gap is spelling habit (docs/english-vs-wetext.ko.md).
- English TN compared with VoxCPM2's built-in normalizer (wetext 0.1.8 as
  VoxCPM2 calls it): benchmark/evaluators/compare_wetext.py scores both on
  the same sentences with the English scorer. Speech variant agreement on
  30,308 sentences: ours 84.91%, wetext 68.38%; on a new held-out Sproat set
  of 11,135 sentences never used for development (sproat-heldout-shard58)
  87.32% against 72.97%. wetext leads on NeMo EngConf (58.44% : 26.84%) and
  on years read as cardinals by us (Act of 1964); docs/english-vs-wetext.ko.md.
- A lowercase identifier of five or more letters is spelled when it has no
  vowel or follows 아이디 (lkdfs392 엘케이디에프에스 삼구이, was 르크드프스삼백구십이;
  project-owner report); a long word with a vowel keeps its reading
  (boeing747). Three contracts join ko_owner_judgements_v24 (27). Korean
  benchmark +15, -0 (adjusted 82.98%); English unchanged.
- Readings the project owner marked wrong on /jev-eval, fixed where the
  benchmark references agree (docs/korean-number-identifiers.ko.md 5-2): a
  bare run of seven or more digits in Korean text is read digit by digit
  (200487201 조은희 학생은; references 1354 : 0); a label after a code of four
  or more digits (8469를 비밀번호로, was five or more), a length phrase between
  label and number (비밀번호 네 자리 1834) and 로 끝나는/시작하는 (8282로 끝나는
  번호) mark codes; declared service numbers (112, 119, 114 …) read digit by
  digit near a call cue (119에 신고) and stay values otherwise (114명, 119개,
  120만 원, 무명로 119); 외교후 112 002 reads both groups digit by digit;
  오이스터 joins model_name; lowercase letters with digits after 아이디 are
  spelled (아이디 sun38 에스유엔 삼팔, abc123 에이비씨 일이삼), and three or
  more lowercase letters without a vowel too (csk01 씨에스케이 공일; mp4 stays
  엠피포). Three-digit plate fronts stay values (references 371 : 273).
  Contract ko_owner_judgements_v24 (24). Korean benchmark +170, -0 (adjusted
  82.85% → 82.97%); English unchanged.
- Jev results get a number check in code (project-owner request after the
  Jev review): tools/web/value_check.py turns every number in the input into
  its acceptable Korean readings (Sino, digit by digit with Sino or native
  names, native, English, decimals, fractions, compressed ranges) anchored by
  the input's Hangul, and reports ok, mismatch (another value, e.g. 14789
  일사칠팔이) or unknown. On the Jev test pairs: 300 correct outputs ok, 300
  one-digit errors 292 mismatch and none ok; 40,000 benchmark references
  98.6% ok, 0.14% mismatch, about half of them reference errors. /jev-eval
  judges each sentence pass, review or fail (value mismatch, unread form or
  Jev under 0.3 fail; Jev 0.7 or more passes; codes pass on a matching value
  read digit by digit whatever Jev says, a code read as an amount goes by
  Jev; the rest needs a person), re-judges stored scores
  without calling Jev, shows the Jev-only rate beside it, and stores a
  person's correct/wrong judgements (labels.json) with an agreement table.
  The workbench Jev result shows the number check. Contract
  tests/web/test_value_check.py (32 cases). Verdict version 3 follows the
  project owner's first 100 judgements (Jev 0.03-0.10: 92 correct, 8 wrong):
  a code followed by 번 may be read as one number, a code named just before
  it (우편번호 51393) is judged like a code category, and letters written
  against digits must be spelled (csk01 크스크영일 goes by Jev).
- The Jev request is shown on /jev-eval (Jev 질문 구조: model, endpoint, the
  exact context, question and criteria per language, the pass rule and the
  question history; GET /api/jev/instructions), linked from the workbench.
  Review (project-owner request): 29% of the ko-v1 outputs that equal the
  reference score under 0.7, mostly digit-by-digit codes. On 300 correct
  outputs and 300 one-digit errors, richer conventions (v4) passed 84% / 12.7%
  and per-fragment questions (v5) 86.7% / 15.0%, against 72% / 6.7% for v3;
  at an equal error rate they are no better, and Jev cannot check codes digit
  by digit. The question stays v3; docs/korean-benchmark-representative.ko.md
  has the numbers.
- Engine start-up 42 ms → 20.5 ms (one CLI start with the shipped
  resources; outputs unchanged, Korean benchmark 0 changed). The Korean and
  English validators and the core record validator compare the first byte of
  a resource kind before strcmp (about 2.5 million strcmp calls per start went
  through their kind chains); validation skips sorting the rows by declaration
  order when every row was already checked at load (the order only picks which
  failing row is reported); the CLI freezes first and validates again only to
  name a failing record, instead of validating before freezing as well. The
  test suite starts the CLI thousands of times, so the full ctest runs in
  about 9.5 minutes on 2 jobs (Release).
- Faster checks (project-owner request: one Korean fix took hours of
  checking). tools/ci/run.sh quick and full run the text-evaluation tests once,
  in the release build, instead of in the gcc build as well (the engine has no
  assert(), so the builds agree; 160 tests, about 16 minutes); ctest runs
  CI_JOBS tests at once, and tests with a wall-clock limit are RUN_SERIAL; the
  text-evaluation job clears its own artifacts first, so a previous run no
  longer fails it. speakable_sweep reads its 6,000 inputs through one engine
  per pass when the shared library is available (4 minutes to under a
  second; the same outputs as the CLI on a 600-input sample); sanitizer builds
  keep the CLI. New benchmark/evaluators/korean_quick.py checks the Korean
  release benchmark on two processes (about 3.5 minutes instead of 15).
  Sampling byte splits in quick was tried and dropped: those loops take
  little time, the CLI starts (about 40 ms each) do.
- Lexicon items from the Korean benchmark misses (project-owner request):
  readings the references use at least twice and twice as often as the
  other reading. 42 model_name entries read the number after them in
  English up to ten (시즌 1 시즌 원, 윈도우 10을 윈도우 텐을, 오메가3를
  오메가쓰리를, 빅5 병원 빅파이브 병원; 시즌 is mixed 51 : 38 and added on
  request); three reads 쓰리 (427 : 48, was 스리); TV reads 티비 (699 : 300,
  was 티브이) with tvN 티비엔, HDTV 에이치디티비 and IPTV 아이피티비; KAL 칼,
  TGV 테제베 and VKOSPI 브이코스피 in capitals. CCTV (씨씨티브이 36 : 18) and
  MZ (엠제트 18 : 7) keep their letter readings. Ten earlier contracts that
  recorded 스리 or 티브이 are updated with their previous values in notes.
- Fullwidth Latin in a unit or suffix symbol matches its ASCII entry: 63ｍ
  육십삼 미터, 5ｋｇ 오 킬로그램, also under an explicit {@unit} control.
- Contracts ko_model_names_v24 (11) and ko_bench_lexicon_v24 (9), and 3
  fullwidth rows in ko_unit_symbols_v24. Korean benchmark +915, -260
  (adjusted 82.39% → 82.85%); the losses are TV sentences whose references
  write 티브이.
- Number readings in context (plan item 6, from the 표준국어대사전 number
  examples): a score written N 대 M or N대 M reads Sino like 3:2 (3 대 2 삼 대
  이); N대 before a rank or generation word reads Sino (5대 은행, 18대 대통령,
  4대 보험, 4대째); 번 counting occurrences reads native with frequency cues
  chosen from the benchmark references (5번이나 다섯 번이나, 10번 이상, 3번
  정도, 하루에, 무려) while route, queue and list numbers stay Sino; 정보 is not
  the counter 정. Contracts ko_dae_rank_v24 (25) and ko_beon_frequency_v24
  (22). Korean benchmark +753, -7 (adjusted 81.86% → 82.39%); English
  unchanged.
- Korean reference validator v2 (project-owner request): records whose spans
  carry no source offsets are checked too. The source's Hangul must reach the
  reference in order; digits, Latin, Han, symbols and bracketed one-syllable
  abbreviations are variable, and a particle adapted to the reading before it
  is not an error. 283 more references that dropped, added or changed the
  source's words (50분 거리에 있는 교회로 → 거리에 교회로, 롯데몰 → 롯데마트) are
  excluded; reference-errors-v2.jsonl has 353 entries (v1 70). Reports now use
  v2, so their adjusted scores are compared with v2 reports only.
- Remaining items before plan item 6: a declared event date with a period
  and only a particle reads as the event when the sentence names one (new
  event_date_cue: 40주년 5.18을 맞아 광주 오일팔, 4.19는 ... 부정선거 사일구;
  평균자책점은 4.19를 stays a decimal), 7·10 칠일영; Belize 벨리즈 (new
  proper-additions-v24 lexicon); a third round of 14 loanword spelling rules
  (ripple 리플, sandal 샌들, output 아웃풋, fringe 프린지, column 칼럼),
  held-out 48.4% → 49.1%. Hanja words with two dictionary readings stay on
  the character rules: corpus frequency of the Hangul forms counts homophones
  (人質 would become 인지), so no default is taken from it.
  speakable_sweep may take up to 600 s: the loanword table adds about 10 ms
  to each of its ~6,000 CLI starts.
- 표준국어대사전 loanwords as a dictionary (plan item 5, project-owner
  decision): 6,789 lowercase loanword spellings (camera 카메라, ambulance
  앰뷸런스, chocolate 초콜릿) in loanwords/stdict_loanwords.tsv, CC BY-SA 2.0
  KR, built by tools/lexicon/stdict_loanwords.py. New resource kind loanword:
  read only when no project lexicon entry exists, never a declared word (so
  ham7239 and part25 keep their label readings), not for all-capital
  surfaces, nor in addresses and handles, and only under
  unknown_foreign=guess (the default). Contract
  ko_stdict_loanwords_v24 (15 cases). NIKL corpus: 50 of 1,491 sentences with
  a lowercase word change; a hand-checked sample were all improvements.
- Hanja word readings from the 표준국어대사전 (plan item 3-3, project-owner
  decision): 4,932 words the character rules miss (列車 열차, 樂山樂水
  요산요수, 敗北 패배, 遊說 유세) in cjk/cjk_words_stdict.tsv, CC BY-SA 2.0 KR,
  loaded with the hanja option; words with two dictionary readings are left
  out. Unseen-word check +164, -6; dictionary Hanja words 97.29% → 99.93%.
  Contract ko_hanja_stdict_words_v24 (17 cases).
- English loanword spelling (plan item 5): 71 g2p rules tuned on the
  표준국어대사전 English loanwords (magic e 페이스, final a 카메라, -ism 즘,
  micro- 마이크로, -meter 미터, nk, tch, dge, au, y as 아이), kept only when they
  gained on the development words without losses; the Korean mapper writes
  power 파워, shag 섀그 and teamwork 팀워크. Held-out agreement 40.7% → 48.4%
  (CMU option 52.9% → 55.3%). 44 acronyms read as words (KOSPI 코스피, NIMBY
  님비, DRAM 디램, TOEIC 토익, ...), all-capital only. Contract
  ko_acronym_words_v24 (18 cases); three tests that pinned the old spelling
  guess updated (baba 배바, Samsonite without the brand pack 샘소나이트, the CMU
  option test now uses Worcester). Korean benchmark +6, -1 (Belize 벨라이즈);
  English unchanged.
- Unit symbols (plan item 4): prefixed SI units (mSv, kΩ, μg, MeV, ...), the
  squared unit characters left unread (㏊ 헥타르, ㎍/㎥ 마이크로그램 퍼
  세제곱미터, ㎿, ㎤, ㏈) and Bq/㎏ in both languages. A bare V, A or J (and
  MJ, GJ) is a unit only with a cue word in the sentence (new
  numeric_suffix_cue: 220V 전원 이백이십 볼트; 모니터 253V stays 브이). Contract
  ko_unit_symbols_v24 (24 cases). Korean benchmark +14, -0; English unchanged;
  161 NIKL corpus sentences change, all checked.
- Han characters outside KS X 1001/1002 (強, 絕, 豐, 姊, 郎, ...) read with the
  hanja option from a table derived from the 표준국어대사전's Hanja words
  (project-owner decision, option D): 904 characters in
  cjk/hanja_readings_stdict.tsv, licensed CC BY-SA 2.0 KR on its own
  (STDICT-LICENSE.txt), built by tools/cjk/stdict_readings.py. Held-out check
  97.9%; dictionary Hanja words 95.92% → 97.29%, no character left unread.
  Contract ko_hanja_stdict_v24 (15 cases).
- Hanja reading rules (hanja=korean, plan item 3-1): 렬/률 read 열/율 after a
  vowel or ㄴ (比率 비율, 旋律 선율) and 不 reads 부 before ㄷ/ㅈ (不正 부정,
  不動産 부동산), in a new hand-authored cjk/hanja_rules.tsv loaded with the
  option. On the 표준국어대사전 Hanja words (local check only) agreement goes
  95.41% → 95.92% (+958, -13). Contract ko_hanja_rules_v24 (20 cases).
- 개 compounds as their own counters (plan item 2): 개국 and 개년 Sino (3개국
  삼 개국, 5개년 오 개년), 개사, 개소, 개교 native up to 9 and 개동 up to 19, set
  by number from the benchmark references. Contract ko_gae_counters_v24 (12
  cases). Korean benchmark +142, -26 (the minority native readings such as
  12개사 열두 개사); English unchanged.
- Event dates read as names (plan item 1 from the 표준국어대사전 survey): 6·25
  전쟁 육이오 전쟁, 5.18민주화운동 오일팔, 3·1절 삼일절, 6·10 육십, 12·12 십이십이.
  Known dates (event_date) read so after a middle dot anywhere; otherwise an
  event word (event_date_word) must follow, so 5.18을 and 1·2학년 keep their
  decimal and list readings. Contract ko_event_dates_v24 (24 cases). Korean
  benchmark +211, -0; English unchanged.
- Counting units from the 표준국어대사전 list (project-owner request), with
  native limits set by number from the Korean benchmark references: new native
  counters 곳, 건, 박스, 봉, 포기, 돈, 차례, 접, 홉, 축, 냥, 포대, 게임, 개조 (3곳 세 곳,
  1돈 한 돈, 25건 이십오 건), and limits on 평 (30평 삼십 평), 병, 권, 통.
  Words such as 건물, 봉투, 접종, 축구 are not the counters. Contract
  ko_dict_counters_v24 (14 cases); two v21 rows updated with previous values
  in notes (33평 서른세 평 → 삼십삼 평). Korean benchmark +1,113, -77; English
  unchanged.
- Read 배 as a counter (project-owner request: 2배 두 배): native up to 19,
  Sino from 20 and for decimals (new counter_native_max), following the
  benchmark references; a range past the limit is Sino (2~4.1배 이에서 사 점
  일 배). numeric_suffix_word keeps 5배수, 2배속, 1배럴 as words. Contract
  ko_counter_bae_v24 (10 cases); two 1.5배 rows take the counter spacing 일 점
  오 배. Korean benchmark +359, -5; English unchanged.
- Brand pack: the remaining 167 review entries were checked against KIPRIS
  (2026-10-04); of 21 matches, 13 accepted, 3 accepted with the reading of the
  written name (American Airlines 아메리칸 에어라인스, Korea Zinc 코리아 징크), 5
  not added. Wikidata pack 5,108 entries, FTC 1,350.
- Korean benchmark metric v4 (ko-reference-agreement-v4; raw scores
  unchanged). v3: the variant fold also removes punctuation on both sides, so a
  reference that adds a final period the source lacks or drops the source's
  middle dot no longer counts as a difference (526 sentences; counted as
  punctuation_only). v4 similar (sentences): the source stretches each side
  rewrote are merged and compared by jamo similarity with two characters of
  context; similar when every stretch is >= 0.85 and no difference touches a
  number syllable (공일공 일이삼사에 일이삼사 ~ 공일공 일이삼사 일이삼사; 일이삼사 vs
  천이백삼십사 stays different). The /ko-quality page shows v4 with a red card
  and bar and a 변경 영역 유사(v4) status filter.
- Brand pack: the 227 review entries whose label matched a KIPRIS trademark
  spelling were reviewed one by one (delegated by the project owner): 212
  accepted, 12 accepted with a corrected reading (Fremantle 프리맨틀, Hyundai
  Steel 현대스틸), 3 left out (Hermes, Tongrentang, SOLVANG). Decisions use a
  new accept action in lexicons/reviews/decisions/brands.jsonl; an accepted
  capital brand (POSCO 포스코, IMAX 아이맥스) bypasses the capital-letters rule.
  Wikidata pack 4,888 to 5,093 entries, FTC 1,347 to 1,349; no KIPRIS data is
  stored. Rebuild drift from the newer engine is not applied
  (docs/brand-lexicon.ko.md).
- Engine start-up: a record validated when its file loads is not validated
  again by ttf_engine_freeze or the CLI report while only the engine and pack
  validators are registered (a custom validator still re-checks every row),
  and an accepted record skips the UTF-8 diagnostic sanitizing pass. Debug
  start-up with all resources 62 ms to 35 ms (without the places lexicon 15 ms
  to 10 ms).
- lexicon_case restricts only entries of its own layer or below: a domain,
  customer or user entry above it keeps applying to an all-capital surface
  (customer MY 마이, finance ETF 이티에프). Five contract rows follow the common
  English words (my_name 마이 언더스코어 네임, report.txt 리포트.티엑스티,
  test_case 테스트 언더스코어 케이스) with previous values in notes.
- Web option english_dictionary=cmu (영어 발음 사전): loads the CMUdict runtime
  export (not in Git; benchmark/generated/cmudict-runtime-full or
  TTF_CMU_RUNTIME) as the domain layer so English words outside the lexicons
  take its pronunciation (Helsinki 헬싱키). Off by default; authored lexicons
  still win (Korea 코리아). Test cmu_english_dictionary_option uses a synthetic
  export. The matrix help example now reads My 마이 (common word lexicon).
- Read English words in Korean text by Korean usage (project-owner request).
  A short lowercase word with no vowel letter is letters (tv 티브이, sns
  에스엔에스, www 더블유더블유더블유; was 트브, 스느스, 우우우). 283 common English
  words are authored by the 외래어 표기법 English rules and established usage (of
  오브, with 위드, Korea 코리아, Life 라이프, Time 타임, James 제임스, Guam 괌, ost
  오에스티), all lexicon_case lower so AS, IS and HMM stay letters; codes keep
  their reading (EBS8102, you501073). Contract ko_english_words_v24 (13 cases).
  Korean benchmark +80, -0; English unchanged; docs/english-words.ko.md.
- Read romanized South Korean place names in Korean (project-owner request):
  Incheon 인천, Jamsil 잠실, Gangnam 강남, Myeongdong 명동, Dokdo 독도. A new system
  lexicon category places (9,407 entries) comes from Wikidata (CC0) places in
  South Korea whose English label is the Revised Romanization of the Korean
  label; English words and names are kept out unless they are the place's own
  English label (Seoul stays, Guam and hail do not). Contract
  ko_place_names_v24 (10 cases); the lexicon inventory test checks the category
  in one stream. Korean and English benchmarks unchanged; docs/place-names.ko.md.
- Brand pack: the FTC franchise list now includes base year 2026 (8,668 rows,
  collected 2026-10-03; 2017-2025 unchanged). 50 new owner-registered readings
  (Bean Plus 빈플러스, Dear Burger 디어버거, Millhaus 밀하우스, Busan Ritual Coffee
  부산리추얼커피); the FTC pack has 1,347 entries and 68 wait for review. Source
  record ftc-franchise-brands-2026-10-03.
- Add the join control, the opposite of {@break} (project-owner request):
  [[너 나 안 본 지 두 달 다 되 감]]{@join} reads 너나안본지두달다되감 and
  [[3시 반에 만나]]{@join} 세시반에만나; the group is read as usual and the spaces
  between the readings are dropped. Contract ko_join_control_v24 (6 cases).
- Read rejected numbers as written in Korean speech instead of keeping them
  (project-owner decision; option numeric_fallback=preserve, CLI
  --numeric-fallback, web 숫자 폴백): 27만 천 원 이십칠만 천 원, 1970년 2월 29일,
  4~5억 사에서 오억, 9번째라 아홉번째라, 제15호선 제십오호선, 52,7% 오십이 점 칠
  퍼센트, 088-696-8173 and 9034MDSAP run by run, $12，500 만 이천오백 달러,
  ０９：０５ 아홉 시 오 분. A backtick closed by an apostrophe is a quote, not
  code (`가을야구'의); GTX A reads 지티엑스 에이 (a capital after an initialism is
  a label); 아이폰 6s 식스에스; units Mb, Giga and ％P; 6G 식스지 like 5G.
  English phrases, inline code and 24:00 keep their reading. Contract
  ko_written_fallback_v24 (29 cases); 13 contract rows, the fullwidth guards and
  one web sample now read the span, with previous values in notes. Korean
  benchmark leftovers with digits or Latin 108 to 6 sentences (3 English
  phrases, 3 inline code spans).
- Read a single digit before a lowercase English word with an English numeral
  like 3Bay (3days 스리데이즈, 1day 원데이, 2way 투웨이; it was left unread or
  spelled). Units the shared grammar reads (5min, 3kcal) and words without a
  vowel (4wd) keep their reading. The English G2P reads a word-final -ays,
  -oys, -eys, -uys plural with 즈 (days 데이즈, boys 보이즈; was 데이스).
  Contract ko_numeral_words_v24 (10 cases). Korean benchmark +0, -0 (2
  changed); English unchanged.
- Read numbers the Korean benchmark left unread: 2PM and 2AM as group names
  투피엠, 투에이엠 (project-owner decision; times are written 오후 2시); a single
  digit before a capitalized English word with English numerals (3Bay 스리베이);
  dimensions and fractions with a unit (30x40cm 삼십 곱하기 사십 센티미터, 1/2컵);
  a decimal before one capital (3.3T); model numbers with trailing letters (6S
  식스에스); e-mail addresses that start with digits; two-group codes that start
  with 0 (031-120); a labelled code of five or more digits is grouped like a
  plain code (주문번호 547324544478 오사칠삼 이사오사 사사칠팔). A Hangul unit after
  a space counts only when it ends the word or takes a particle (3 평화 stays
  삼 평화); a scale after a space must end its word (920만 조회); 1백만 reads
  일백만 in speech; an endpoint with its own unit no longer shares the next
  scale (4-6cm만). Units mJ, Ah, mmHg, km/L; counters 군데, 정거장; range families
  for volume, length and weight. Contract ko_bench_leaks_v24 (34 cases);
  updated expectations keep previous values in notes. Korean benchmark +118, -0
  (leftover digits or Latin 202 to 110 sentences); English unchanged.
- Lexicon: Genesis G70, G80, G90 read 지칠공, 지팔공, 지구공, and the card name 2v4
  reads 이브이포 (project-owner decision). Korean benchmark +8, -0.
- Keep the Hangul written after a number, Latin text or symbol: particle
  agreement is off by default (project-owner decision), since a following word
  is not always a particle (성수동1가 성수동일가, 종로3가, 11번가; it became
  성수동일이). The option particle_agreement=on (CLI --particle-agreement on, web
  조사 맞춤) restores it (3랑 삼이랑, 5kg로 오 킬로그램으로). Tests whose
  expectations include agreement opt in. GV60, GV70, GV80 are Genesis model
  names read 지브이육공, 지브이칠공, 지브이팔공. Korean benchmark +54, -10 (ten
  references agree the particle after a code: 90970로 구공구칠공으로).
- Read short digit-first labels as the number and the letter names instead of
  leaving them as written (20F 이십 에프, 1234AB 천이백삼십사 에이비, 5A 오 에이;
  project-owner decision). Units, meridiems (2PM), English ordinals, decades and
  English sentences keep their grammar. {@address} reads F after a number as a
  floor (20F 이십 층, B1F 지하 일 층). Contract ko_short_labels_v24 (10 cases); 13
  expectations updated with previous values in notes. Korean benchmark +48, -0.
- Read a colon pair that is not a valid time as a score or ratio with 대 (3:2로
  승리 삼 대 이로, 1:1 질문 일 대 일; project-owner decision, 24:00 stays); short
  digit-first labels before 번 as routes (38A번 삼십팔에이번) or after a 번호/코드
  label as codes (제품번호 6456A 육사오육 에이); small comma lists one by one (1,2라인
  일, 이라인); phone numbers before words such as 개인 (the counter 개 must end
  the word); apparel multipliers 7-9 (8XL) and 3DS, 8D. Contract
  ko_jev_leaks2_v24 (17 cases); Korean benchmark +156, -0.
- Read numbers that /jev-eval left unread: a compound amount with two or more
  numbers before an attached ending (5억 1,317만 원어치의, 3백2십만 원하는);
  만 after a quantity with its own unit (1시간 만에, 5~6년 만에); an ordinal
  or quantity before the copula (11번째인, 20번째다); ascending duration
  ranges (40분~1시간, families plus range_unit_rank; 1시~2분 stays
  preserved); a trailing fullwidth percent sign (18.1％, １２３％) and %P.
  Contract ko_jev_leaks_v24 (10 cases). Korean benchmark +319, -3: three
  references read a decimal before a scale (264.7만 개다 이백육십사 점 칠만)
  that the compound grammar expands, as it already did before a particle.
- Read digits then letters with five or more digits as a code (516123813ZC 오일육
  일이삼 팔일삼 제트씨; it was left unread); a declared unit stays a quantity
  (300000p 삼십만 포인트) and short labels (20F, 5A, 470G) keep the preservation
  policy. Digit runs of five or more inside codes are grouped like plain codes
  (KR180337594CN 일팔공 삼삼칠 오구사). Contract ko_digit_codes_v24 (7 cases);
  Korean benchmark +28, -0.
- Read 0 as 공 in {@digits}, {@digits:N} and the digits of {@break}, like a card or
  phone digit ([[0210]]{@digits} 공이일공; it was 영). Resource number digits_zero.
- Skip list markers in speech: -, *, •, ·, ▪, ... at the start of a line and
  followed by a space are not read (- 사과 사과; it was 하이픈 사과); -3도 stays
  마이너스 삼도. {@break:N} keeps 세 and 섹 (their code points truncated to the
  ASCII digits 8 and 9 and were dropped). {@digits} accepts spaces or hyphens
  between digit groups ([[0095 6341 2825 0503]]{@digits}). Contract
  ko_list_break_v24 (15 cases).
- Read a plain number of five or more digits (no comma grouping, unit or
  counter) digit by digit; up to four digits stays an amount (project-owner
  decision; it was nine). Groups of five, four or three never leave a group
  shorter than three (12345 일이삼사오, 123456 일이삼 사오육, 1234567 일이삼사
  오육칠). Amounts are requested with {@sino}/{@cardinal}. {@digits:N} reads N
  digits at a time and several digits repeat in turn (1234{@digits:1} 일 이 삼
  사, 01012341234{@digits:344}). Contract ko_digit_groups_v24 (20 cases).
- Read a number named by a word ending in 번호 digit by digit (예매번호 12578
  일이오칠팔; not phone words, bare 번호 or three digits or fewer), also with a
  particle on the label (주문번호를), a copula or 번 after the number (2021034인,
  63107508번이) and the label after it (56478910이 예약번호이며). 만 after a hyphen
  pair with a group of five or more digits is the particle, not a shared scale
  (254412-774521만 있으면, 주민번호 540019-3758929만). A short queue number with 번
  after a label stays a count (예약번호 89번 팔십구 번) unless the label names a
  password or ticket code (short_code_label: 비밀번호 7537번 칠오삼칠번). Contract
  ko_number_labels_v24 (16 cases); Korean benchmark 78.88% -> 79.30% (+593, -0).
- Read a two-digit number after capitals as a model number (GV70 지브이칠십;
  it was 지브이세번지로), and a number after a declared product name with English
  numerals up to ten and Sino-Korean from eleven (아이폰6 대란 아이폰식스 대란,
  갤럭시노트10 노트텐, 아이폰 14 십사; 아이폰 2대 stays a quantity). Codes with
  three or more digits after capitals keep English digits, now from the lexicon
  words (ABC127 원투세븐, was 원투세번). Baseball counts 아웃 and 스트라이크 take
  English numerals (2아웃 투 아웃), lowercase sizes read (2xl 투 엑스라지), and
  2NE1 투애니원, 2TV 이티비, 4D 포디 get readings. Contract ko_alnum_models_v24
  (23 cases).
- Ignore an invalid control instead of failing the request: an unknown name
  (45821{@digit}) or a value the pattern rejects drops only that control and the
  unit is read as if it were absent (사만 오천팔백이십일은); malformed control syntax
  is plain text. invalid_control=error / --invalid-control error keeps the old
  failure (the strict error tests use it). Contract ko_control_ignore_v24 (7 cases).
- The web workbench uses annotated input (발음 제어 표기 사용) by default.
- Read long digit codes in groups of three to five: the largest of 5, 4, 3 that
  divides the length (123456789 일이삼 사오육 칠팔구, 874456932101 팔칠사사 오육구삼
  이일공일), else five first and the rest again (11 digits 5-3-3). Seven contract
  expectations updated with the previous values in notes; four cases added.
- Read the company name 3M as 쓰리엠 (it was left unread; capital only, 3m stays
  삼 미터) and add the {@unit} control: a number with a Latin unit is a measure even
  where a lexicon term or letter case reads it otherwise, the unit looked up as
  written, lowercase, then uppercase (3M{@unit} 삼 미터, 5w{@unit} 오 와트).
  Contract ko_unit_control_v24 (12 cases).
- Add the {@score} and {@math} controls: 10:10{@score} and [[40-30]]{@score} read
  십 대 십 / 사십 대 삼십 (ten to ten), 4-3{@math} reads 사 빼기 삼 (four minus three).
  Colons keep the time grammar and are not guessed as scores; hyphen defaults are
  unchanged. A bare digit atom now includes one `- + * / = :` between digits, so
  4-3{@math} and 2026-10-01{@date} bind the whole value. Contract
  ko_score_math_v24 (23 cases, including {@address} hyphen readings).
- Stop reading a word that only begins with a scale syllable as a scale after a
  hyphen pair (417-00-32154 조회 kept raw because 조회 looked like 조), and read an
  identifier with an attached Hangul word (417-00-32154번) instead of keeping it
  verbatim. Found on /jev-eval in the 사업자번호 category; contract cases 29 → 32.
- Read a short hyphen pair in a sentence with a game cue as a score with 대
  (25-18로 졌다 이십오 대 십팔로, 2-0 승리 이 대 영 승리): the identifier change had
  lowered the sports category on /jev-eval from 59.5% to 53.5%. Cue words
  (`score_cue`), separator policy `score_separator`; Korean benchmark 78.43% →
  78.59% (+236), contract cases 24 → 29.
- Read hyphen-joined digit groups and long plain digit runs as identifiers in
  Korean speech: 27022-8080 이칠공이이 다시 팔공팔공 (was 이만 칠천이십이에서 팔천팔십),
  874456932101 팔칠사사… (was 팔천칠백사십사억…), 2017345678번 이공일칠…번. 에서 is
  the fallback for ascending groups with a unit or counter (3-4개, 10-20%, 7-10일간),
  a range cue (사이), a scale word (100-200만 원) or two years (2019-2020); amounts
  with commas or a currency, measure or quantity counter keep their reading and
  {@sino} reads one on request. Pack rules with priority 100 may replace a longer
  shared numeric object (the shared grammar took 통 from 통장, 시 from 시즌) but not
  a phone number or an identifier it already rejected. Korean benchmark 76.97% →
  78.43% (+1,637/−1). Contract `ko_number_identifiers_v24` (24), 12 contracts
  updated with their previous values, `docs/korean-number-identifiers.ko.md`.
- Jev question v3 adds Korean reading conventions to the context: a digit
  before a counter is a Sino-Korean number (2층 이 층, 4회 사 회), personal,
  account, phone and order numbers are read digit by digit (reading one as a
  large amount is wrong) while bus and room numbers are whole numbers, Latin
  letters take Korean names (Z 제트). On reviewed hard cases the separation of
  correct outputs from errors rose from AUC 0.59 to 0.90 and errors passing
  fell from 29% to 0%, with the value-error calibration unchanged; a first,
  prescriptive v2 lowered correct phone and bus readings and is replaced.
  /jev-eval shows the full sentence, an in-place editable output, icon copy
  buttons and a compact score column.
- Add a Jev TN comparison page (`/jev-eval`): 4,057 representative source
  sentences (up to 200 per category, low duplication, no reference used) in a
  table of category, sentence, TN output and evaluation. Each TN to compare is a
  named system whose outputs are typed, imported (JSONL, JSON, TSV, TXT) or
  filled with this engine; Jev scores everything, only the edited rows, or one
  row, with cached pairs free, and the page compares systems per category in
  blue. The representative selection also builds a reference import
  (`select_representative.py`), and `korean_release.py --predictions/--system`
  scores another system's outputs on the reference protocol. Jev is not shown on
  /ko-quality. `docs/korean-benchmark-representative.ko.md`.
- Web workbench: a **Jev 채점** button scores the current result with one
  TypeSafe Jev Noul, whether the spoken text is a correct way to read the input
  aloud, so readings that differ from the single reference but are right can be
  told from errors. Calibrated on 40 Korean sentences: correct 0.81, accepted
  variants 0.81, one wrong number 0.15 (AUC 0.97), one dropped word 0.37 (AUC
  0.89). `/api/judge` keeps the key on the server, limits input size and calls
  per client and in total, and caches repeated pairs (`tools/web/jev.py`).
- Second brand review: English words as keys keep the word's reading (solid
  솔리드, was 쏠리드; arctic 아크틱; verbatim 버베이텀) and FTC empower/esteem are
  dropped; the Jev 0.5-0.65 band loses translations, extra or missing words and
  foreign-language readings (bristol cars 브리스톨 카스, was 브리스톨 자동차;
  rushydro 루스하이드로, was 루스기드로; shinhwa company 신화컴퍼니, was 신화컴퍼니의);
  the Wikidata fashion_brand class, which held only Polish towns, is excluded.
  124 decisions in total; Wikidata 4,888, FTC 1,297; 33 of 36 corrected entries
  score higher, the 0.5-0.65 band 468 → 418.
- Review the 239 lexicon entries Jev scored below 0.5: 42 brand entries are
  dropped (other names, translations, Polish town aliases, English words as
  keys such as being, shaken, i've, the on) and 34 corrected (Hilti 힐티, was
  히트; NAVER Clova 네이버 클로바, was 네이버 클라우드; Four Seasons 포시즌스, was
  포시즌스 호텔), recorded in `lexicons/reviews/decisions/brands.jsonl` so the
  brand builders reapply them; seed eighth 에잇스 (was 에이스, the reading of
  ace). The rest stay: owner spellings and Korean romanisations Jev undervalues.
  Rejudged: below 0.5 239 → 165, below 0.3 54 → 25.
- Narrow the Wikidata brand builder after Jev triage: an alias takes the
  label's Korean name only as a spelling variant of the label or when that name
  fits it at least as well (PlayStation Trophies 플레이스테이션 네트워크 and Acer
  Aspire Predator 에이서 프레데터 are dropped, Pagani 파가니 stays); a multiword
  name whose content words do not align with the Korean goes to review; a
  trailing kind word the name lacks is dropped (Triumph 트라이엄프, was 트라이엄프
  모터) when the rest still covers the reading; keys another brand pack has are
  skipped so rebuilds converge. Wikidata 5,053 → 4,980 entries, Jev below 0.5
  253 → 127, below 0.3 95 → 34.
- Read Latin words joined by hyphens or apostrophes in Korean speech part by
  part, with the joiner silent and a final 's as 스, instead of a grapheme guess
  that spoke the hyphen: e-mail 이메일 (was 에 하이픈 므에이르), Rolls-Royce
  롤스로이스, Wendy's 웬디스, D-TV 디티브이. Whole-word lexicon and dictionary
  readings, the ampersand initialism (H&M) and capital possessives (OSPF's) are
  unchanged; English output does not change. Lexicon keys accept the joiners
  - ' & inside words and a lone & between words (a-free cafe, mom's pizza,
  procter & gamble); phrase matching follows them, also after a hyphenated word.
  The brand builders keep the joiners (they had replaced them with spaces, so the
  keys matched no text): Wikidata 4,817 → 5,053, FTC 1,202 → 1,302, unreachable
  entries 138 → 3. Korean benchmark +6/−0. Contracts `ko_latin_joiners_v24` (11),
  `ko_brand_lexicon_v24` 14 → 22, `docs/latin-joiners.ko.md`.
- Add 1,202 franchise brands to the optional brand pack from the Fair Trade
  Commission franchise brand list (data.go.kr, 이용허락범위 제한 없음, base years
  2017-2025): brands registered with both a Hangul and a Latin name use the
  owner's Hangul form, WAK BURGER 왁버거 (was read letter by letter), TAFELRUNDE
  타펠룬데, CODE RUN 코드런. Translated names, lone capital abbreviations of four
  letters or fewer, common English words and names the Wikidata pack already has
  are excluded; 69 pairs wait for review. `tools/lexicon/brands/collect_ftc.py`,
  `build_ftc.py`; contract `ko_brand_lexicon_v24` 9 → 14. Add the missing
  `docs/brand-lexicon.ko.md`. OpenDART and KIPRIS data stay local: their terms do
  not allow redistribution.
- Add an optional Korean brand pronunciation pack, off by default and loaded as
  the domain layer (`resources/domains/brands/ko-KR/lexicon.tsv`; CLI `--domain`,
  web "브랜드 발음 사전"). 4,817 company and brand names from Wikidata (CC0) whose
  Korean label is phonetically close to the written name: Samsonite 샘소나이트
  (was 샘소닛), Ericsson 에릭슨, Kooksoondang 국순당. Translated names, all-capital
  abbreviations and names the engine already reads are excluded; 1,421 pairs
  wait for review. Built reproducibly by `tools/lexicon/brands/`; source records
  stay pending review. Contract `ko_brand_lexicon_v24` (9),
  `docs/brand-lexicon.ko.md`.
- Read unit notations that reached Korean speech raw, from the AI Hub 159
  references: gold purity 18K 십팔 케이, 4K/8K 포케이/에잇케이, 5G 파이브지 (was 오지;
  3G/4G 쓰리지/포지), storage 32G 기가 (declared capacities only), m/s km/s 초속,
  W/kg, Kcal/kal, kw/KW, GWh/MWh, cc 씨씨 (was 시시; now a shared unit, so ranges
  and lists read), and 4만 2000km인. A bare all-capital abbreviation is read by
  letters (KB국민은행 케이비국민은행, KG이니시스, MG손해보험; references 100%) through
  lexicon_case lower; lowercase kg당 and 몇 ml keep the unit. Korean adjusted
  agreement 77.00% → 77.27% (+387 / −1); English unchanged; Latin left in Korean
  speech 497 → 365. Contract ko_unit_leaks_v24 (32), docs/korean-unit-leaks.ko.md.
- Read apparel sizes in Korean speech instead of passing them raw: `3XL` →
  쓰리 엑스라지, `2XS` → 투 엑스스몰, `XL` → 엑스라지, `XXL` → 투 엑스라지 (was
  "x x l"), bra sizes `75AA` → 칠십오 에이에이. Rule and words follow the AI Hub 159
  size references (177 multiplied sizes, XL 41:2, XS 8:5, 117 bra sizes); only
  next to Korean text, never in text mode, and the general number+letter policy
  (`20F`, `45A` preserved) is unchanged. Korean adjusted agreement 76.84% →
  77.00% (+229 / −4); sentences with Latin left in speech 729 → 497 (size
  category 214 → 15). Contract `ko_apparel_size_v24` (22 cases),
  `docs/korean-apparel-sizes.ko.md`.
- Read Korean ranges the way the references and the standard approximate
  numerals do (user decision; `docs/korean-range-reading.ko.md`). Adjacent
  numbers contract (`2~3만 원` → 이삼만 원, `3~4개` → 서너 개, `20~30명` → 이삼십
  명); a wider range names the unit once, after the second number (`3~5kg` →
  삼에서 오 킬로그램, `1~3만 원` → 일에서 삼만 원); a unit written on both ends is
  read as written (`3개~4개`). `∼`, `～` and `〜` between numbers mark a range
  like `~`. Korean adjusted agreement 75.39% → 76.84% (+2,086 / −15); English
  unchanged. Resources `range_contract`, `policy range_contract` and
  `range_repeat_unit … wide`; the previous reading is the customer profile
  `customers/range-repeat-unit`. Forty-five contract rows and thirty test
  literals that encoded the previous reading follow the decision (previous
  values kept in notes). Contract `ko_range_reading_v24` (49 cases).
- Name symbols and emoji that no rule reads instead of passing them raw to TTS
  (named option `symbol_fallback=on|native|off`, default `on`; CLI
  `--symbol-fallback`). Each language has an index/data file pair under
  `resources/unicode-fallback/`: the index maps a code point straight to an
  offset of a NUL-terminated name, and a sorted sequence file holds flag,
  skin-tone, keycap, tag and ZWJ sequences; the files are memory-mapped, never
  loaded. Names come from CLDR release-48-2 `tts` annotations (English also from
  UCD 17 names); digit keycaps read the digit alone (`1️⃣` → 일, one). Emoji
  sequences are one object: `👨‍👩‍👧` → 가족, 남자 여자 여자 아이, and an
  unregistered combination is never read in part. Decorative characters keep
  the `decorative` policy. `tools/unicode_fallback/build.py` builds every CLDR
  locale (170) reproducibly; ko and en are committed. Korean and English
  benchmarks: 0 changed sentences. Sixteen rows of older contracts that use an
  emoji as the unnamed-character example now pin `symbol_fallback: "off"`
  (expected text unchanged). See `docs/unicode-fallback.ko.md` and
  `unicode_fallback_v24.jsonl` (28 cases).
- Give each sentence a language before per-token routing (named option
  `matrix=auto|declared|legacy`, default `auto`; CLI `--matrix`). An English
  sentence now reads its own words and numbers in English and keeps Hangul,
  Hanja and Kana words as written: `My friend 민수 bought 3 books yesterday.` →
  `My friend 민수 bought three books yesterday.` (previously `미 프린드 민수
  바우그흐트 삼 부크스 …`). Korean sentences keep the previous routing. Pack
  resources `absorb_foreign`, `attached_grammar` and `sentence_word_weight`
  declare the policy; the core has no language branch and ABI v1 is unchanged.
  Korean and English benchmarks: 0 changed sentences. The v0.18 context-cache
  metadata oracle is checked under `--matrix legacy`. See
  `docs/matrix-language.ko.md` and `matrix_language_v24.jsonl` (28 cases).

- Complete eight mandatory Korean particle pairs in the resource tables:
  `(이)랑`, `(이)나`, `(이)나마`, `(이)라도`, `(이)든`, `(이)든지`,
  `(이)야말로`, `(이)라든지`. `3랑` now reads `삼이랑`; the quantity boundary
  also recognises `1만원라도` as `만 원이라도` with CURRENCY(10000, KRW).
  Preserve the existing character-substitution contracts (`가漢나` → `가한나`,
  mapped keycaps): an unclassified OTHER character or unreadable cluster does
  not establish a noun boundary for particle agreement. Public ABI is unchanged.
  A frozen 72-case table sweep improves 35 → 72
  with no regressions; the corpus, byte-split/override tests and editing guide
  are included. Optional copula contractions and arbitrary suffix splitting
  remain outside this policy.
- Read a four-digit number that a bracket holds on its own as a year, which is
  the citation shape: `( 1965 )` → `( nineteen sixty five )`, `(1965)`, and
  `Bright , William ( 1965 ) .`. An ordinary parenthesis keeps its cardinals
  (`( 2003 and 2005 )`), and the quantity noun and year range still decide
  (`( 1965 copies )`, `( 2100 )`). `date_year_cue` now takes `(`, `[` and `{`
  besides words, and `born` joins the word cues. Two rows of the phone-negative
  contract list that expected `CARDINAL` for `( 2024 )` and `1385 ( 2006 )` now
  expect `DATE`; the rationale is recorded in
  `docs/research/date-diagnostic-v21-results.md`.
- Separate a merged span from a real alignment failure in the benchmark, which is
  what the work queue's first item asked for: the reference splits `6 - 5` into
  `six`, `to`, `five` while the engine emits one RANGE reading `six to five`, so
  the readings agree and only the boundaries differ. Those are now reported as
  `merged-agreement`, which takes CARDINAL from 79 alignment failures to 35 plus
  44 merges, PLAIN from 40 to 14 plus 26, and MEASURE from 3 to 0 plus 3. The
  accuracy is unchanged: an unaligned reference span still counts as a failure.
- Name why a benchmark span missed, so a dataset convention is no longer read as
  an error: the evaluator reports `mismatch_kinds` (case-only, space-only,
  case-and-space, alignment, no-output, different) and a per-class
  `diagnostic_case_insensitive` count. Both are diagnostics; the accuracy still
  compares bytes and folds nothing. They showed that 698 of the 1,144 English
  DATE misses differ only in the capitalisation of a month name, which the
  project keeps.
- Read a decade, which was left as digits for the synthesizer before: `the
  1960s` → `the nineteen sixties`, `the 1900s` → `the nineteen hundreds`, `the
  2000s` → `the two thousands`, `the 80s` → `the eighties`, `the '90s` → `the
  nineties`. The digits must end in a zero and carry nothing after the `s`, so
  `the 1965s`, `787s` and `MP3s` keep their text. The plural words are the new
  `decade_plural` resource, and `classifier_priority en.date.decade` lets the
  rule win over the generic "digits with a tail" fallback across the same bytes.
- Spell an inferred initialism in speech output, which was previously handed to
  the synthesizer unchanged: `FBI` → `f b i`, `NJ` → `n j`, `UK` → `u k`. An
  uppercase run that reads as an English word keeps its form (`NASA`, `FIFA`,
  `EXCEL`, `UNESCO`), decided by the new `syllable_onset` and `syllable_coda`
  resources. Text output is untouched, a value that is not letters alone keeps
  its own reading (`R&D` → `R and D`), and the speech defaults are the new
  `initialism_policy` keys `inferred_speech` and `pronounceable_speech`.
- Read a number that stands before a colon, which was preserved whole: `Chapter
  12: Introduction` left its number unsaid, because a colon counted as structure
  even with no digits after it. A colon is structure only when digits follow
  (`3:4`, `09:30` are unchanged), and a colon with nothing numeric behind it no
  longer makes a time candidate either.
- Read the timeline shape as a year: `1830 : The first Constitution` →
  `eighteen thirty : …`, through the new `date_year_cue_after` resource. The year
  range and the quantity-noun block still decide, so `2500 : list` and
  `Room 12 : open` keep their cardinals.
- Read a hyphen year range with the year style, which the word form already
  did: `the 1988 - 1993 period` was `one thousand nine hundred eighty eight to
  one thousand nine hundred ninety three` and is now `nineteen eighty eight to
  nineteen ninety three`, and `From 1988 - 93` reads `nineteen eighty eight to
  ninety three`. The cue rule is unchanged, so an uncued pair and a page range
  keep their cardinals, and an ISO date is untouched. The reading is one RANGE
  span carrying `unit=year`, which the verbalizer reads endpoint by endpoint.
- Read the citation shape as a year: `University of Wales Press , 1992 .` →
  `… , nineteen ninety two .`. The bracket rule became the `date_year_enclosure`
  resource, where each opening mark declares the closing marks that complete it
  and `$` allows the end of the text, so the comma joins `(`, `[` and `{` as
  data rather than code. Both marks have to match, which keeps a quantity list
  (`scores were 1200 , 1400 and 1600 .`) in cardinals. The streaming planner
  reads the same resource, so batch and stream agree.
- Recognise a country-code domain, which no rule did: `Vecernji.hr` and
  `Amazon.co.uk` reached the synthesizer with their dots. 200 ISO 3166 codes join
  `url_tld`. A code that is also an ordinary English word (`no`, `it`, `in`,
  `at`, `is`, `us`, `me`, …) is deliberately left out, because a missing space
  after a sentence period would otherwise read `fig.no` as a domain — a failure
  the external sample cannot show, since adding every code changed nothing there.
- Add `--unreadable read` (`TTF_UNREADABLE_READ`), a third policy for callers who
  need every segment sayable. The semantic layer still refuses to interpret an
  ambiguous object, so `20F` is not turned into a floor, but the final gate says
  the text it kept: `20F` → `이영에프` / `two zero F`, `11st` → `일일에스티`.
  Digits follow the identifier reading the project already uses (`k007` →
  `케이영영칠`), English letters stay because English speech reads them, a
  character with no declared name is removed as under `drop`, and punctuation
  stays. The default is unchanged and text mode is unaffected.
- Space spelled characters by the declared `speech_join` alphabet value, which
  the final gate ignored: English now reads `two zero F` rather than `twozeroF`.
- Read a long leading-zero digit run as digits rather than a quantity:
  `0774858583` was `seven hundred seventy four million eight hundred fifty eight
  thousand five hundred eighty three` and is now read digit by digit, which is
  what the external sample does with it. The rule starts at seven plain digits,
  where zero padding stops being a plausible quantity, so the project's recorded
  position that a shorter bare run is ambiguous (`001204` → `천이백사`, `00010` →
  `십`) is untouched, and a grouped `0,123,456` keeps its quantity reading. The
  disagreement about shorter runs, and which Korean zero reading a bare
  identifier should use, is recorded as NUM-ZERO in remaining-work rather than
  decided here.
- Read the remaining vulgar fractions and the parenthesised numbers, which a
  numeral-character sweep found unread while `½`, `⅓`, `¼`, `①`..`⑳` and `Ⅰ`..`Ⅹ`
  already worked: `⅕` → `오분의 일` / `one fifth`, `⑴` → `일` / `one`, and the rest
  of ⅖⅗⅘⅙⅚⅐⅛⅜⅝⅞⅑⅒ and ⑴..⑽. CJK numerals stay unread under the documented hanja
  policy, and fullwidth digits are recorded as an open limitation (NUM-WIDTH)
  because folding them reaches every numeric scanner rather than one table.
- Read Greek letters and four technical symbols, found by sweeping 96 symbols at
  once: `π` → `파이` / `pi`, and the same for α β γ δ ε θ λ μ ρ σ τ φ ω and the
  uppercase Δ Σ Π Ω Θ Λ Φ Ψ Γ Ξ, plus `∞` `√` `§` `№`. Punctuation is still kept
  for prosody and a decorative mark still follows the decorative policy, which is
  what most of the 76 raw symbols were.
- Complete the math relation set with `≠`, `≈` and `≡`, where `≤` and `≥` already
  worked: `3≠5` → `삼은 오와 같지 않다` / `three is not equal to five`. The
  classifier's operator table, both packs' relation lists and both validators
  needed the same three characters.
- Declare 24 currencies that reached the synthesizer as written, found by
  sweeping 23 symbols and 30 ISO codes at once: 17 symbols and 21 codes were
  unnormalized, and in a Korean sentence an undeclared code pulled the amount
  into English as well (`5 CNY` read `five c n y`). CNY, INR, RUB, BRL, MXN, ZAR,
  NZD, SGD, HKD, TWD, THB, VND, IDR, MYR, PHP, SEK, DKK, PLN, CZK, HUF, TRY, ILS,
  UAH and NGN now carry their code, symbol and labels in both languages. `¥` maps
  to JPY, the convention for that symbol, with `CN¥` for the yuan, and `¢` stays
  out because it is a minor unit rather than a currency. No minor unit is declared
  for the new set, so a fractional value reads in major units.
- Declare 33 unit abbreviations that reached the synthesizer as written, found
  by sweeping 77 of them at once: 54 were unnormalized. `5ms`, `5hr`, `5px`,
  `5pt`, `5dB`, `5rpm`, `5Pa`, `5kV`, `5mAh`, `5lb`, `5oz`, `5inch`, `5mile`,
  `5ha`, `5Ω` and the rest now read in both languages. A single-letter
  abbreviation (`A`, `V`, `B`, `t`, `s`, `l`) stays undeclared on purpose,
  because `5A` is the ambiguous identifier shape the project keeps alongside
  `20F` and `4K`, and the bps family is left out because its Korean reading is
  not settled. The negatives pin both decisions.
- Read the unit symbols that reached the synthesizer as written: `20°` →
  `twenty degrees` / `이십 도`, `5m²` and `5㎡` → `five square meters` /
  `오 제곱미터`, `5㎢` → `오 제곱킬로미터`, `5kcal` and `5㎉` → `five kilocalories` /
  `오 킬로칼로리`, `5‰` → `five per mille` / `오 퍼밀`. A degree with a scale keeps
  winning (`20°C` → `섭씨 이십 도`). `′`, `″`, `¢` and a superscript `²` stay as
  written: they are ambiguous between feet and minutes, a minor currency unit and
  a footnote mark.
- Stop a phone cue from swallowing what is not a phone number: `문의 12` →
  `문의 십이`, `문의 3-5` → `문의 삼에서 오`, `문의 1+1` → `문의 일 더하기 일`.
  A candidate with too few digits and no international or parenthesised shape no
  longer claims the number, while `+123 456` and a malformed number with enough
  digits are still preserved.
- Add an option-matrix gate: all 216 combinations of the six frontend options
  run, and a byte-by-byte stream produces what one call produces, in both
  languages. It also pins where `--unreadable read` stops — a property sweep
  found that `--unknown-foreign preserve` keeps its word as written even there,
  because that is what the option asks for, and the interaction is now
  documented and contracted rather than discovered.
- Add a sayability sweep over 4,392 generated inputs. Under `--unreadable read`
  nothing is exempt, in both languages, which is what that policy promises; under
  the default policy the Korean pass allows only the inputs whose preserve
  contract docs/speech-resources.md records. It is a property, not an expected
  reading, so a wrong reading still passes.
- Read a unit abbreviation that stands on its own in Korean text instead of
  guessing a pronunciation for it: `kg 단위` → `킬로그램 단위`, and the same for
  cm, km, mm, mg, ml, kW, Hz, kHz and KB, which used to come out as 크그, 큼 and
  므. A number in front still takes the measure path (`5kg` → `오 킬로그램`).
- Stop the Korean 제 prefix from swallowing a measure, a range or a decimal:
  `제 10kg` was preserved whole and reached the synthesizer as digits and
  letters. 제 written against its number stays the ordinal prefix, so `제0` keeps
  its text, but 제 with a space may be the ordinary word, so `제 10kg` is `제 십
  킬로그램`, `제 10개` is `제 열 개`, `제 3-5` is `제 삼에서 오` and `제 0.5` is
  `제 영 점 오`. A malformed ordinal that met an ordinal unit, such as `0번째` or
  `제 1.5회`, is still preserved.
- Declare the lowercase `ml` spelling, which mapped to nothing while `mL` did.
- Spell a lone initial whose period is followed by a mark that carries the
  sentence on: `Cossette , M. , Lecomte` → `Cossette , m , Lecomte`,
  `Chang , Shirley L. ( 1992 )` → `Chang , Shirley l ( nineteen ninety two )`.
  A dotted run already did this (`U.S. Army` → `u s Army`); a single letter was
  only recognised before a name. A period that ends a sentence is still left
  alone, because dropping it would take the sentence boundary with it, and the
  marks that count as a continuation are the new `initial_continuation`
  resource.
- Read a per-unit and two units the sample left as raw symbols: `193.5/km²` →
  `one hundred ninety three point five per square kilometers`, `-2011 kl` →
  `minus two thousand eleven kiloliters`, `20 degrees C` → `twenty degrees
  Celsius`. A slash whose denominator is not a number may now carry a declared
  unit, while a numeric denominator stays a fraction (`5/6` → `five over six`)
  and a space before the slash keeps the protected-path reading. The units are
  the new `numeric_suffix` rows `/km²`, `kl`, `degrees C` and `degrees F`; a
  per-form the sample does not attest, such as `/kg`, stays undeclared.
- Expand an abbreviation written without its period when the reading is not
  contested: `vol 1` → `volume one`, `Records ltd` → `Records limited`,
  `Sadako vs Kayako` → `Sadako versus Kayako`, `etc` → `etcetera`, `jr` →
  `junior`, `mr Smith` → `Mister Smith`. The keys come from the external English
  sample, which reads each of them one way only; `st`, `dr`, `sr`, `no` and `pp`
  stay untouched because the same sample reads `st` as both saint and street and
  `dr` as both doctor and drive. The new `abbreviation_bare` resource holds the
  expansions, and `preserve` disables one.
- Read an identifier after its Korean label digit by digit, which only the
  finance labels (계좌, 참조, 거래) reached before: `우편번호 06236` was
  `육천이백삼십육`, losing the leading zero, and is `공육이삼육`; `주문번호
  20260926-0001` was read as a range and is `이공이육공구이육 공공공일`;
  `송장번호 123456789012` was a twelve-digit quantity. 66 label rows cover 우편,
  주문, 송장, 운송장, 회원, 사업자, 사업자등록, 예약, 인증, 승인, 카드, 등록,
  문서, 고객, 접수 plus 학번, 사번 and 군번. A digit run with no label keeps its
  quantity reading.
- Read 59 more Korean counters with native numerals, which the resource table
  did not declare, so the numeral itself was wrong: `2쌍` was `이쌍` and is `두 쌍`,
  `1정` was `일정` and is `한 정`, `8자` was `팔자` and is `여덟 자`. The first
  batch covers 통, 판, 척, 알, 조각, 묶음, 상자, 봉지, 갑, 채, 그루, 그릇, 접시,
  군데, 가지, 줄, 칸, 끼 and 쌍; the second covers 정, 자, 곡, 편, 컷, 방, 캡슐,
  포, 첩, 술, 모금, 발, 자리, 바퀴, 걸음, 쪽, 캔, 팩, 세트, 다발, 근, 관, 짝, 사발
  and 공기. Counters that take Sino-Korean readings (회, 배, 박, 층, 호, 인분, 점,
  건) are unchanged. The third batch (톨, 마디, 뼘, 아름, 줌, 손, 모, 뿌리, 폭,
  갈래, 종류, 구획, 평, 되, 말) came from sweeping 120 counters at once, and it also
  drew the boundary: 단계, 부, 매, 동, 차선, 학년, 교시 and 회차 read with
  Sino-Korean numerals, so 단, 부, 매 and 동 stay undeclared and the corpus locks
  that. Found by reading realistic Korean commerce, prescription and notice
  phrasings rather than a corpus.
- Read the acronyms Korean says as words instead of spelling them: `FIFA` →
  `피파`, `UNESCO` → `유네스코`, `NATO` → `나토`, `OPEC` → `오펙`, `UEFA` →
  `우에파`, `EXCEL` → `엑셀`, `SIM` → `심`. The runtime already lets a lexicon
  entry override the letter reading (`RAM` → `램`), so these are seven curated
  entries with provenance, not a rule: which uppercase runs Korean reads as
  words is lexical knowledge, and an unknown one is still spelled, which is the
  safe default. A twenty-case contract records the boundary (`CEO` → `씨이오`
  stays spelled).
- Run the benchmark in speech mode: `evaluate.py --output-mode speech` records
  the options in `provenance.frontend_options` and names its own
  `metric_policy`, so a speech report is never compared with the text baseline.
- External English sample, text mode: DATE 394 → 558 exact, PLAIN 33,860 →
  33,881, MEASURE 76 → 83, LETTERS 174 → 242 and 35,053 → 35,313 exact spans,
  every other class unchanged. Speech mode, against its own baseline: 35,236 →
  35,597 exact spans with LETTERS 174 → 539. Speech mode, against its own new
  baseline: 35,236 → 35,501 exact spans, LETTERS 174 → 471, PLAIN 33,860 →
  33,827. The trade is measured in `docs/research/initialism-speech-v21-results.md`.

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
