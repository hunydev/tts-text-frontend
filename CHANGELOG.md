# Changelog

## Unreleased — speech output, measurement and the final gate

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
