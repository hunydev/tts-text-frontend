#include "cjk_reading.h"
#include "../pronunciation/trace.h"
#include "../resource/validation.h"

typedef struct {
    char key[5];
    uint32_t cp;
    size_t bytes;
    const ttf_entry_t *fold;
} glyph_t;

int ttf_cjk_manifest_selected(const ttf_engine_t *e, const char *path, int deferred) {
    const char *prefix = "languages/ko-KR/cjk/";
    const char *phonemes = "languages/ko-KR/pronunciation/stdict_phonemes";
    /* The Korean pronunciation dictionary (19k rows) loads at freeze only when the
       output may need it: pronunciation=hangul, or annotated input ({@pronounce}).
       Other engines start as fast as before (2026-10-10). */
    if (!strncmp(path, phonemes, strlen(phonemes))) {
        const char *rest = path + strlen(phonemes);
        if (!strcmp(rest, ".tsv") || !strcmp(rest, ".bin") || !strcmp(rest, ".ttfb"))
            return deferred && (e->phoneme_output ||
                                e->frontend_options.input_format == TTF_INPUT_ANNOTATED);
    }
    const char *names[] = {"hanja_readings", "kana_readings", "cjk_words", "hanja_rules",
                           "hanja_readings_stdict", "cjk_words_stdict"};
    size_t i;
    if (!strncmp(path, prefix, strlen(prefix))) {
        path += strlen(prefix);
        for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
            size_t n = strlen(names[i]);
            if (!strncmp(path, names[i], n) &&
                (!strcmp(path + n, ".tsv") || !strcmp(path + n, ".bin") ||
                 !strcmp(path + n, ".ttfb"))) {
                int han = e->frontend_options.hanja == TTF_HANJA_KOREAN;
                int kana = e->frontend_options.kana == TTF_KANA_KOREAN;
                return deferred && (i == 0 || i >= 3 ? han : i == 1 ? kana : han || kana);
            }
        }
    }
    return !deferred;
}

static int syllable(uint32_t cp) { return cp >= 0xac00 && cp <= 0xd7a3; }
static int selector(uint32_t cp) {
    return (cp >= 0xfe00 && cp <= 0xfe0f) || (cp >= 0xe0100 && cp <= 0xe01ef);
}
static size_t encode(uint32_t cp, char out[5]) {
    size_t n;
    if (cp < 0x80) { out[0] = (char)cp; n = 1; }
    else if (cp < 0x800) {
        out[0] = (char)(0xc0 | (cp >> 6)); out[1] = (char)(0x80 | (cp & 63)); n = 2;
    } else if (cp < 0x10000) {
        out[0] = (char)(0xe0 | (cp >> 12)); out[1] = (char)(0x80 | ((cp >> 6) & 63));
        out[2] = (char)(0x80 | (cp & 63)); n = 3;
    } else {
        out[0] = (char)(0xf0 | (cp >> 18)); out[1] = (char)(0x80 | ((cp >> 12) & 63));
        out[2] = (char)(0x80 | ((cp >> 6) & 63)); out[3] = (char)(0x80 | (cp & 63)); n = 4;
    }
    out[n] = 0;
    return n;
}
static size_t cluster(const char *s, size_t n, uint32_t *cp) {
    size_t at, width;
    uint32_t next;
    if (ttf_utf8_decode(s, n, cp, &at) != 1) return 0;
    while (at < n && ttf_utf8_decode(s + at, n - at, &next, &width) == 1 && selector(next))
        at += width;
    return at;
}

int ttf_cjk_applies(const char *language, ttf_script_t script) {
    return !strcmp(language, "ko-KR") && ttf_script_legacy(script) == TTF_SCRIPT_OTHER;
}
uint32_t ttf_cjk_initial_variant(uint32_t cp, ttf_script_t script) {
    unsigned lead, replacement, vowel;
    if (!syllable(cp)) return cp;
    lead = (cp - 0xac00) / 588;
    replacement = lead;
    vowel = (cp - 0xac00) % 588 / 28;
    if (script == TTF_SCRIPT_KANA) {
        if (lead == 15) replacement = 0;       /* ㅋ -> ㄱ */
        else if (lead == 16) replacement = 3;  /* ㅌ -> ㄷ */
        else if (lead == 14) replacement = 12; /* ㅊ -> ㅈ; ㅍ unchanged */
    } else if (script == TTF_SCRIPT_HAN) {
        if (lead == 2 && (vowel == 6 || vowel == 12 || vowel == 17 || vowel == 20))
            replacement = 11; /* 녀뇨뉴니 -> 여요유이 */
        else if (lead == 5) {
            if (vowel == 2 || vowel == 6 || vowel == 7 || vowel == 12 ||
                vowel == 17 || vowel == 20)
                replacement = 11; /* 랴려례료류리 */
            else if (vowel == 0 || vowel == 1 || vowel == 8 || vowel == 11 ||
                     vowel == 13 || vowel == 18)
                replacement = 2;  /* 라래로뢰루르 */
        }
    }
    return 0xac00 + replacement * 588 + (cp - 0xac00) % 588;
}

#define CJK_WORD_LIMIT 6u
static const char *word_key(const ttf_entry_t *entry) {
    if (!strcmp(entry->kind, "cjk_word")) return entry->key;
    /* CJK source is unclaimed (und); no JA/ZH language pack is fabricated. */
    if (!strcmp(entry->kind, "lexicon") && !strncmp(entry->key, "und:", 4))
        return entry->key + 4;
    return NULL;
}
static size_t word_length(const char *key, uint32_t *first) {
    size_t at = 0, length = strlen(key), count = 0, width;
    uint32_t cp;
    while (at < length) {
        ttf_script_t script;
        if (ttf_utf8_decode(key + at, length - at, &cp, &width) != 1) return 0;
        script = ttf_script_of(cp);
        if (script != TTF_SCRIPT_HAN && script != TTF_SCRIPT_KANA) return 0;
        if (!count) *first = cp;
        if (++count > CJK_WORD_LIMIT) return 0;
        at += width;
    }
    return count;
}
ttf_status_t ttf_cjk_index(ttf_engine_t *e) {
    size_t bucket, table;
    ttf_cjk_index_t *index = calloc(1, sizeof(*index));
    if (!index) return TTF_ENOMEM; /* No entries have moved; freeze may be retried. */
    for (bucket = 0; bucket < TTF_HASH_BUCKETS; ++bucket) {
        ttf_entry_t **at = &e->resources[bucket];
        while (*at) {
            ttf_entry_t *entry = *at;
            if (!ttf_cjk_resource_kind(entry->kind)) { at = &entry->next; continue; }
            *at = entry->next;
            entry->next = index->resources[bucket];
            index->resources[bucket] = entry;
        }
    }
    e->cjk = index; /* The remaining operations cannot fail or allocate. */
    for (table = 0; table < 2; ++table) for (bucket = 0; bucket < TTF_HASH_BUCKETS; ++bucket) {
        ttf_entry_t *entry;
        for (entry = table ? index->resources[bucket] : e->resources[bucket];
             entry; entry = entry->next) {
            const char *key = word_key(entry);
            ttf_entry_t **position;
            entry->cjk_next = NULL;
            if (!key || (strcmp(entry->lang, "ko-KR") && strcmp(entry->lang, "*"))) continue;
            entry->cjk_length = word_length(key, &entry->cjk_first);
            if (!entry->cjk_length) continue;
            if (ttf_resource_entry(e, entry->kind, entry->lang, entry->key) != entry) continue;
            position = &index->words[entry->cjk_first % TTF_HASH_BUCKETS];
            while (*position && (*position)->cjk_length >= entry->cjk_length)
                position = &(*position)->cjk_next;
            entry->cjk_next = *position;
            *position = entry;
            if (entry->cjk_length > index->max_length) index->max_length = entry->cjk_length;
        }
    }
    return TTF_OK;
}
static int enabled_script(const ttf_engine_t *e, ttf_script_t script) {
    return (script == TTF_SCRIPT_HAN && e->frontend_options.hanja == TTF_HANJA_KOREAN) ||
           (script == TTF_SCRIPT_KANA && e->frontend_options.kana == TTF_KANA_KOREAN);
}
size_t ttf_cjk_protect_until(const ttf_engine_t *e, const char *text,
                              size_t length, size_t begin) {
    size_t at = begin, count = 0;
    if (!e->cjk) return begin;
    while (at < length && count < e->cjk->max_length) {
        uint32_t cp;
        size_t width = cluster(text + at, length - at, &cp);
        if (!width || !enabled_script(e, ttf_script_of(cp))) break;
        at += width;
        ++count;
    }
    return at;
}
static const ttf_entry_t *word_match(const ttf_engine_t *e, const char *lang,
                                    const char *text, size_t length, uint32_t first,
                                    size_t *used) {
    const ttf_entry_t *row, *best = NULL;
    if (!e->cjk) return NULL;
    for (row = e->cjk->words[first % TTF_HASH_BUCKETS]; row; row = row->cjk_next) {
        const char *key;
        size_t at = 0, kt = 0, kn;
        if (row->cjk_first != first || (strcmp(row->lang, lang) && strcmp(row->lang, "*")))
            continue;
        key = word_key(row); kn = strlen(key);
        while (at < length && kt < kn) {
            uint32_t cp, expected;
            size_t width = cluster(text + at, length - at, &cp), kw;
            if (!width || !enabled_script(e, ttf_script_of(cp)) ||
                ttf_utf8_decode(key + kt, kn - kt, &expected, &kw) != 1 || cp != expected)
                break;
            at += width; kt += kw;
        }
        if (kt != kn) continue;
        if (!best || row->layer > best->layer ||
            (row->layer == best->layer &&
             (row->cjk_length > best->cjk_length ||
              (row->cjk_length == best->cjk_length &&
               ((!strcmp(row->lang, lang) && strcmp(best->lang, lang)) ||
                (!strcmp(row->lang, best->lang) && row->order < best->order)))))) {
            best = row; *used = at;
        }
    }
    return best;
}

/* Folding is local and retains original byte widths, including selectors.
   Try a two-codepoint key first so halfwidth voicing marks are never lost. */
static glyph_t glyph(const ttf_engine_t *e, const char *lang, const char *s, size_t n) {
    glyph_t g = {{0}, 0, 0, NULL};
    uint32_t next;
    size_t extra, width;
    char pair[9], second[5];
    g.bytes = cluster(s, n, &g.cp);
    if (!g.bytes) return g;
    encode(g.cp, g.key);
    if (g.bytes < n && (extra = cluster(s + g.bytes, n - g.bytes, &next))) {
        encode(next, second);
        snprintf(pair, sizeof(pair), "%s%s", g.key, second);
        g.fold = ttf_resource_entry(e, "kana_fold", lang, pair);
        if (g.fold) g.bytes += extra;
    }
    if (!g.fold) g.fold = ttf_resource_entry(e, "kana_fold", lang, g.key);
    if (g.fold) {
        snprintf(g.key, sizeof(g.key), "%s", g.fold->value);
        (void)ttf_utf8_decode(g.key, strlen(g.key), &g.cp, &width);
    }
    return g;
}
static ttf_status_t row_trace(const ttf_engine_t *e, const char *lang,
                              const ttf_entry_t *row, size_t begin, size_t end,
                              const char *detail) {
    if (!row) return TTF_OK;
    ttf_trace_stage(e, TTF_TRACE_POLICY, begin, end);
    return ttf_trace_resource(e, row->kind, lang, row->key, row->key, detail, SIZE_MAX);
}

/* Hand-authored reading rules over the generated table (hanja_rules.tsv):
   hanja_soft_after 렬 열 after a vowel or ㄴ (比率 비율, 旋律 선율), and
   hanja_before_initial 不:ㄷ 부 before a syllable with that initial (不正 부정). */
static uint32_t rule_syllable(const ttf_entry_t *rule) {
    uint32_t cp = 0;
    size_t w;
    if (!rule || ttf_utf8_decode(rule->value, strlen(rule->value), &cp, &w) != 1 || !syllable(cp))
        return 0;
    return cp;
}

static const ttf_entry_t *soft_after(const ttf_engine_t *e, const char *lang, uint32_t rendered,
                                     uint32_t last) {
    char key[5];
    unsigned final;
    if (!syllable(last))
        return NULL;
    final = (last - 0xac00) % 28;
    if (final != 0 && final != 4)               /* none or ㄴ */
        return NULL;
    encode(rendered, key);
    return ttf_resource_entry(e, "hanja_soft_after", lang, key);
}

static const ttf_entry_t *before_initial(const ttf_engine_t *e, const char *lang, const char *key,
                                         const char *next, size_t n) {
    static const uint16_t jamo[19] = {0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141,
                                      0x3142, 0x3143, 0x3145, 0x3146, 0x3147, 0x3148, 0x3149,
                                      0x314a, 0x314b, 0x314c, 0x314d, 0x314e};
    const ttf_entry_t *reading;
    char next_key[9], rule_key[16], letter[5];
    uint32_t cp, first;
    size_t width = n ? cluster(next, n, &cp) : 0, w;
    if (!width || ttf_script_of(cp) != TTF_SCRIPT_HAN)
        return NULL;
    encode(cp, next_key);
    reading = ttf_resource_entry(e, "hanja_reading", lang, next_key);
    if (!reading || ttf_utf8_decode(reading->value, strlen(reading->value), &first, &w) != 1 ||
        !syllable(first))
        return NULL;
    encode(jamo[(first - 0xac00) / 588], letter);
    snprintf(rule_key, sizeof(rule_key), "%s:%s", key, letter);
    return ttf_resource_entry(e, "hanja_before_initial", lang, rule_key);
}

ttf_status_t ttf_cjk_render(const ttf_engine_t *e, const char *lang, const char *text,
                            size_t length, ttf_semantic_t *sem, ttf_buffer_t *out,
                            char lexicon[64], ttf_resource_layer_t *pronunciation_layer) {
    size_t at = 0, last_position = 0;
    uint32_t last = 0;
    ttf_script_t previous = TTF_SCRIPT_OTHER;
    int initial = 1, mapped = 0, sequence = 0;
    const char *path = NULL;
    ttf_status_t st = TTF_OK;
    while (at < length && st == TTF_OK) {
        uint32_t cp;
        size_t width = cluster(text + at, length - at, &cp), used;
        ttf_script_t script;
        const ttf_entry_t *reading = NULL, *mark = NULL, *word;
        glyph_t first = {{0}, 0, 0, NULL}, second = {{0}, 0, 0, NULL};
        char key[9];
        int enabled;
        if (!width) return TTF_EUTF8;
        script = ttf_script_of(cp);
        enabled = enabled_script(e, script);
        if (script != previous) { initial = 1; last = 0; }
        previous = script;
        used = width;
        encode(cp, key);
        if (!enabled) {
            st = ttf_buffer_append(out, text + at, width);
            initial = 1; last = 0; at += width;
            continue;
        }
        word = word_match(e, lang, text + at, length - at, cp, &used);
        if (word) {
            const char *wk = word_key(word);
            size_t wi = 0, wn = strlen(wk), ww;
            uint32_t wc;
            st = row_trace(e, lang, word, at, at + used, "cjk-word");
            if (st == TTF_OK) st = ttf_buffer_puts(out, word->value);
            if (!lexicon[0] || word->layer > *pronunciation_layer) {
                snprintf(lexicon, 64, "lexicon:%s", ttf_layer_name(word->layer));
                *pronunciation_layer = word->layer;
                path = script == TTF_SCRIPT_HAN ? "cjk.hanja.word" : "cjk.kana.word";
            }
            if (word->layer > sem->layer) sem->layer = word->layer;
            initial = 0; mapped = 1; last = 0;
            while (wi < wn && ttf_utf8_decode(wk + wi, wn - wi, &wc, &ww) == 1) {
                previous = ttf_script_of(wc); wi += ww;
            }
            /* A following Kana mark may attach to the word's final syllable. */
            if (out->length >= 3) {
                size_t rw;
                uint32_t tail;
                if (ttf_utf8_decode(out->data + out->length - 3, 3, &tail, &rw) == 1 &&
                    syllable(tail)) { last = tail; last_position = out->length - 3; }
            }
            at += used;
            continue;
        }
        if (script == TTF_SCRIPT_HAN)
            reading = ttf_resource_entry(e, "hanja_reading", lang, key);
        if (script == TTF_SCRIPT_KANA) {
            first = glyph(e, lang, text + at, length - at);
            used = first.bytes;
            if (at + used < length) {
                second = glyph(e, lang, text + at + used, length - at - used);
                if (ttf_script_of(second.cp) == TTF_SCRIPT_KANA) {
                    snprintf(key, sizeof(key), "%s%s", first.key, second.key);
                    reading = ttf_resource_entry(e, "kana_reading", lang, key);
                }
            }
            if (reading) { used += second.bytes; sequence = 1; }
            else {
                snprintf(key, sizeof(key), "%s", first.key);
                second.fold = NULL;
                reading = ttf_resource_entry(e, "kana_reading", lang, key);
            }
            mark = ttf_resource_entry(e, "kana_mark", lang, key);
            st = row_trace(e, lang, first.fold, at, at + first.bytes, "cjk-kana-fold");
            if (st == TTF_OK)
                st = row_trace(e, lang, second.fold, at + first.bytes, at + used, "cjk-kana-fold");
        }
        if (st != TTF_OK) break;
        if (mark) {
            st = row_trace(e, lang, mark, at, at + used, "cjk-kana-mark");
            if (!strcmp(mark->value, "long-vowel")) {
                mapped = 1;
                if (!path) path = "cjk.kana.char";
                if (mark->layer > sem->layer) sem->layer = mark->layer;
                at += used;
                continue;
            }
            if (syllable(last) && (last - 0xac00) % 28 == 0) {
                char joined[5];
                last += !strcmp(mark->value, "sokuon") ? 19 : 4;
                encode(last, joined);
                memcpy(out->data + last_position, joined, 3);
                mapped = 1;
                if (!path) path = "cjk.kana.char";
                if (mark->layer > sem->layer) sem->layer = mark->layer;
                at += used;
                continue;
            }
        } else if (reading) {
            uint32_t rendered;
            size_t rw;
            const ttf_entry_t *long_after = NULL;
            if (script == TTF_SCRIPT_KANA && syllable(last) && (last - 0xac00) % 28 == 0) {
                char vowel[5], long_key[16];
                encode(0xac00 + 11 * 588 + ((last - 0xac00) % 588 / 28) * 28, vowel);
                snprintf(long_key, sizeof(long_key), "%s:%s", vowel, key);
                long_after = ttf_resource_entry(e, "kana_long_after", lang, long_key);
                st = row_trace(e, lang, long_after, at, at + used, "cjk-kana-long-after");
            }
            if (st != TTF_OK) break;
            if (long_after && !strcmp(long_after->value, "1")) {
                if (long_after->layer > sem->layer) sem->layer = long_after->layer;
                at += used;
                continue;
            }
            st = row_trace(e, lang, reading, at, at + used,
                           script == TTF_SCRIPT_HAN ? "cjk-hanja-reading" : "cjk-kana-reading");
            if (ttf_utf8_decode(reading->value, strlen(reading->value), &rendered, &rw) != 1)
                return TTF_ERESOURCE;
            if (initial) {
                uint32_t changed = ttf_cjk_initial_variant(rendered, script);
                if (changed != rendered && st == TTF_OK) {
                    char canonical[5], variant[5];
                    encode(rendered, canonical); encode(changed, variant);
                    ttf_trace_stage(e, TTF_TRACE_POLICY, at, at + used);
                    st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, lang,
                                        "decision", canonical, variant, key,
                                        "cjk-initial-variant", SIZE_MAX);
                }
                rendered = changed;
            }
            if (script == TTF_SCRIPT_HAN && st == TTF_OK) {
                const ttf_entry_t *rule = initial ? NULL : soft_after(e, lang, rendered, last);
                if (!rule)
                    rule = before_initial(e, lang, key, text + at + used, length - at - used);
                if (rule_syllable(rule)) {
                    st = row_trace(e, lang, rule, at, at + used, "cjk-hanja-rule");
                    rendered = rule_syllable(rule);
                    if (rule->layer > sem->layer) sem->layer = rule->layer;
                }
            }
            if (st == TTF_OK) {
                char encoded[5];
                last_position = out->length;
                encode(rendered, encoded);
                st = ttf_buffer_puts(out, encoded);
                last = rendered; initial = 0; mapped = 1;
                if (!path) path = script == TTF_SCRIPT_HAN ? "cjk.hanja.char" : "cjk.kana.char";
                if (reading->layer > sem->layer) sem->layer = reading->layer;
            }
            at += used;
            continue;
        }
        /* Preserve the original spelling (not the folded key) on a miss. */
        if (st == TTF_OK) st = ttf_buffer_append(out, text + at, used);
        ttf_trace_stage(e, TTF_TRACE_POLICY, at, at + used);
        if (st == TTF_OK)
            st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, lang,
                                "decision", key, "", key, "cjk-unresolved", SIZE_MAX);
        initial = 1; last = 0; at += used;
    }
    if (mapped) {
        sem->type = TTF_SEM_WORD;
        sem->lexical_class = TTF_LEX_WORD;
        if (sequence && path && !strcmp(path, "cjk.kana.char")) path = "cjk.kana.seq";
        snprintf(sem->rule, sizeof(sem->rule), "%s", path);
    }
    return st;
}

static int one_cp(const char *s, uint32_t *cp) {
    size_t n = strlen(s), w;
    return ttf_utf8_decode(s, n, cp, &w) == 1 && w == n;
}
static int kana_key(const char *s, size_t maximum, int halfwidth) {
    size_t at = 0, n = strlen(s), count = 0, w;
    uint32_t cp;
    while (at < n) {
        if (ttf_utf8_decode(s + at, n - at, &cp, &w) != 1 ||
            (ttf_script_of(cp) != TTF_SCRIPT_KANA &&
             !(halfwidth && count && (cp == 0xff9e || cp == 0xff9f)))) return 0;
        at += w; ++count;
    }
    return count && count <= maximum;
}
static int hangul_value(const char *s, int alternatives) {
    size_t at = 0, n = strlen(s), w;
    uint32_t cp;
    if (!n) return 0;
    while (at < n) {
        if (ttf_utf8_decode(s + at, n - at, &cp, &w) != 1 || !syllable(cp)) return 0;
        at += w;
        if (alternatives && at < n) {
            if (s[at++] != '|' || at == n) return 0;
        }
    }
    return 1;
}
ttf_status_t ttf_cjk_validate(const ttf_resource_record_t *r, int *handled,
                              ttf_resource_diagnostic_t *d) {
    uint32_t cp;
    int valid;
    if (!strcmp(r->kind, "hanja_reading"))
        valid = one_cp(r->key, &cp) && ttf_script_of(cp) == TTF_SCRIPT_HAN &&
                hangul_value(r->value, 1);
    else if (!strcmp(r->kind, "cjk_word"))
        valid = word_length(r->key, &cp) && hangul_value(r->value, 0);
    else if (!strcmp(r->kind, "kana_reading"))
        valid = kana_key(r->key, 2, 0) && one_cp(r->value, &cp) && syllable(cp);
    else if (!strcmp(r->kind, "kana_fold"))
        valid = kana_key(r->key, 2, 1) && one_cp(r->value, &cp) &&
                ttf_script_of(cp) == TTF_SCRIPT_KANA;
    else if (!strcmp(r->kind, "kana_mark"))
        valid = kana_key(r->key, 1, 0) &&
                ttf_validation_enum(r->value, "sokuon|moraic-n|long-vowel");
    else if (!strcmp(r->kind, "hanja_soft_after"))
        valid = one_cp(r->key, &cp) && syllable(cp) && one_cp(r->value, &cp) && syllable(cp);
    else if (!strcmp(r->kind, "hanja_before_initial")) {
        const char *sep = strchr(r->key, ':');
        char han[5];
        valid = 0;
        if (sep && sep > r->key && sep - r->key < 5) {
            memcpy(han, r->key, (size_t)(sep - r->key)); han[sep - r->key] = 0;
            valid = one_cp(han, &cp) && ttf_script_of(cp) == TTF_SCRIPT_HAN &&
                    one_cp(sep + 1, &cp) && cp >= 0x3131 && cp <= 0x314e &&
                    one_cp(r->value, &cp) && syllable(cp);
        }
    } else if (!strcmp(r->kind, "kana_long_after")) {
        char vowel[5];
        const char *sep = strchr(r->key, ':');
        valid = 0;
        if (sep && sep - r->key == 3) {
            memcpy(vowel, r->key, 3); vowel[3] = 0;
            valid = one_cp(vowel, &cp) && syllable(cp) && (cp - 0xac00) / 588 == 11 &&
                    (cp - 0xac00) % 28 == 0 && kana_key(sep + 1, 1, 0) &&
                    ttf_validation_enum(r->value, "0|1");
        }
    } else return TTF_OK;
    *handled = 1;
    return valid ? TTF_OK : ttf_validation_error(d, "Invalid bounded CJK reading resource");
}
