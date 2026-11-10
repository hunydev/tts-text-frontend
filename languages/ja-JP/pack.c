/* Japanese pack, phase 1 (2026-10-08): numbers, counters, dates, times,
 * currency and units read in hiragana, unit and letter names in katakana.
 * Kanji and kana stay written for the speech model, which reads them itself.
 * The pack works only in a request declared ja-JP (resource context
 * declared_only): Korean and English requests never see it. */
#include "tts_text_frontend/ttf_language_pack.h"
#include "../../src/core/internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JA "ja-JP"
#define NO "\xe3\x81\xae" /* の */

static const char *res(const ttf_engine_t *e, const char *kind, const char *key) {
    return ttf_resource_get(e, kind, JA, key, NULL);
}

static int declared(const ttf_engine_t *e) {
    return e && e->default_language && !strcmp(e->default_language, JA);
}

static int digit(char c) {
    return c >= '0' && c <= '9';
}

static int alpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* The last character becomes a small tsu: いち いっ, じゅう じゅっ, ひゃく ひゃっ. */
static ttf_status_t geminate(ttf_buffer_t *b) {
    size_t n = b->length;
    if (!n)
        return TTF_EINVAL;
    --n;
    while (n && ((unsigned char)b->data[n] & 0xc0) == 0x80)
        --n;
    b->length = n;
    b->data[n] = 0;
    return ttf_buffer_puts(b, "っ");
}

static ttf_status_t put_cp(ttf_buffer_t *b, uint32_t cp) {
    char s[4] = {(char)(0xe0 | (cp >> 12)), (char)(0x80 | ((cp >> 6) & 0x3f)),
                 (char)(0x80 | (cp & 0x3f)), 0};
    return cp >= 0x800 && cp <= 0xffff ? ttf_buffer_puts(b, s) : TTF_EINVAL;
}

/* A counter reading with its first kana voiced (ほん ぼん) or half-voiced (ほん ぽん). */
static ttf_status_t put_voiced(ttf_buffer_t *b, const char *reading, int half) {
    uint32_t cp, voiced = 0;
    size_t w;
    if (ttf_utf8_decode(reading, strlen(reading), &cp, &w) != 1)
        return TTF_ERESOURCE;
    if (cp == 0x308f) /* わ: さんば, ろっぱ */
        voiced = half ? 0x3071 : 0x3070;
    else if (cp >= 0x306f && cp <= 0x307b && (cp - 0x306f) % 3 == 0)
        voiced = cp + (half ? 2 : 1);
    else if (!half && cp >= 0x304b && cp <= 0x3062 && cp % 2 == 1)
        voiced = cp + 1;
    else if (!half && (cp == 0x3064 || cp == 0x3066 || cp == 0x3068))
        voiced = cp + 1;
    if (!voiced)
        return ttf_buffer_puts(b, reading);
    {
        ttf_status_t st = put_cp(b, voiced);
        return st == TTF_OK ? ttf_buffer_puts(b, reading + w) : st;
    }
}

static ttf_status_t word(const ttf_engine_t *e, ttf_buffer_t *b, const char *kind,
                         const char *key) {
    const char *v = res(e, kind, key);
    return v ? ttf_buffer_puts(b, v) : TTF_ERESOURCE;
}

/* One myriad group 1..9999: せん にひゃく さんじゅう よん. */
static ttf_status_t group_reading(const ttf_engine_t *e, unsigned g, ttf_buffer_t *b) {
    static const unsigned place[3] = {1000, 100, 10};
    char key[8];
    size_t k;
    ttf_status_t st;
    for (k = 0; k < 3; ++k) {
        unsigned d = g / place[k] % 10;
        if (!d)
            continue;
        snprintf(key, sizeof(key), "%u", d * place[k]);
        if ((st = word(e, b, "number_place", key)) != TTF_OK)
            return st;
    }
    if (g % 10) {
        snprintf(key, sizeof(key), "%u", g % 10);
        return word(e, b, "number", key);
    }
    return TTF_OK;
}

/* A non-negative integer in myriad groups (まん おく ちょう けい). */
static ttf_status_t integer_reading(const ttf_engine_t *e, uint64_t v, ttf_buffer_t *b) {
    unsigned groups[5];
    int k, count = 0;
    ttf_status_t st;
    if (!v)
        return word(e, b, "number", "0");
    while (v && count < 5) {
        groups[count++] = (unsigned)(v % 10000);
        v /= 10000;
    }
    if (v)
        return TTF_EINVAL;
    for (k = count - 1; k >= 0; --k) {
        char key[4];
        if (!groups[k])
            continue;
        if ((st = group_reading(e, groups[k], b)) != TTF_OK)
            return st;
        if (!k)
            continue;
        /* 1兆 いっちょう, 8兆 はっちょう, 10兆 じゅっちょう */
        if (k >= 3 && (groups[k] % 10 == 1 || groups[k] % 10 == 8 || (groups[k] % 100 == 10)) &&
            (st = geminate(b)) != TTF_OK)
            return st;
        snprintf(key, sizeof(key), "%d", k * 4);
        if ((st = word(e, b, "number_scale", key)) != TTF_OK)
            return st;
    }
    return TTF_OK;
}

typedef struct {
    int negative, decimal, positive; /* positive: a written plus sign (+1.2kg) */
    uint64_t value;         /* whole value, or the integer part of a decimal */
    char fraction[32];      /* decimal digits after the point */
    const char *scale_word; /* 1.5万: a decimal before a written scale */
    int nan;                /* 何本 なんぼん: reads なん with the sound change of 3 */
    int plain;              /* 10-20歳: no whole-value special reading (はたち) */
} ja_number_t;

static int scale_mark(const ttf_engine_t *e, const char *s, size_t n, size_t *width,
                      unsigned *exponent) {
    char key[4];
    const char *v;
    if (n < 3)
        return 0;
    memcpy(key, s, 3);
    key[3] = 0;
    if (!(v = res(e, "number_scale_mark", key)))
        return 0;
    *exponent = (unsigned)atoi(v);
    *width = 3;
    return *exponent > 0;
}

static int power10(unsigned exponent, uint64_t *out) {
    uint64_t p = 1;
    while (exponent--) {
        if (p > UINT64_MAX / 10)
            return 0;
        p *= 10;
    }
    *out = p;
    return 1;
}

/* A group separator before exactly three digits: 1,000 or １，０００ (the view
   folds the digits, not the comma). Returns its width. */
static size_t group_separator(const char *s, size_t n, size_t p) {
    /* １、０００: JSUT style writes a Japanese comma; before fewer digits it lists. */
    size_t g =
        s[p] == ',' ? 1
        : (p + 3 <= n && (!memcmp(s + p, "\xef\xbc\x8c", 3) || !memcmp(s + p, "\xe3\x80\x81", 3)))
            ? 3
            : 0;
    if (!g || p + g + 3 > n || !digit(s[p + g]) || !digit(s[p + g + 1]) || !digit(s[p + g + 2]))
        return 0;
    return p + g + 3 < n && digit(s[p + g + 3]) ? 0 : g;
}

/* Parse digits (1,000 groups, a decimal point) and written scales (1億2000万,
 * 3千, 1.5万) at s. Returns bytes used, zero when no number starts here. */
static size_t parse_number(const ttf_engine_t *e, const char *s, size_t n, ja_number_t *out,
                           char *norm, size_t norm_size, size_t *norm_length) {
    size_t p = 0, written = *norm_length;
    uint64_t total = 0;
    unsigned last_exponent = 99;
    memset(out, 0, sizeof(*out));
    if (p < n && s[p] == '-' && p + 1 < n && digit(s[p + 1])) {
        out->negative = 1;
        ++p;
    } else if (p < n && s[p] == '+' && p + 1 < n && digit(s[p + 1])) {
        /* +1.2kg プラスいってんにキログラム (owner sets, 2026-10-14) */
        out->positive = 1;
        ++p;
    }
    for (;;) {
        uint64_t chunk = 0;
        size_t start = p, width, f = 0;
        unsigned exponent;
        int decimal = 0;
        if (p >= n || !digit(s[p]))
            break;
        while (p < n) {
            size_t g = p > start ? group_separator(s, n, p) : 0;
            if (g) {
                p += g;
                continue;
            }
            if (!digit(s[p]))
                break;
            if (chunk > (UINT64_MAX - 9) / 10)
                return 0;
            chunk = chunk * 10 + (uint64_t)(s[p] - '0');
            ++p;
        }
        if (p + 1 < n && s[p] == '.' && digit(s[p + 1])) {
            decimal = 1;
            ++p;
            while (p < n && digit(s[p])) {
                if (f + 1 >= sizeof(out->fraction))
                    return 0;
                out->fraction[f++] = s[p++];
            }
            out->fraction[f] = 0;
        }
        if (scale_mark(e, s + p, n - p, &width, &exponent)) {
            uint64_t factor;
            if (exponent >= last_exponent) {
                p = start; /* 1万2万: the second number is not part of this one */
                break;
            }
            if (decimal) {
                /* 1.5万 いってんごまん: the decimal keeps its own reading. */
                char key[12];
                if (total || out->decimal)
                    return 0;
                snprintf(key, sizeof(key), "%u", exponent);
                out->scale_word = res(e, "number_scale", key);
                if (!out->scale_word)
                    return 0;
                out->decimal = 1;
                out->value = chunk;
                p += width;
                break;
            }
            if (!power10(exponent, &factor) || (chunk && factor > UINT64_MAX / chunk))
                return 0;
            /* 3千万: a 千 multiplies the digits before a larger scale. */
            if (exponent == 3 && p + width + 3 <= n) {
                size_t w2;
                unsigned e2;
                if (scale_mark(e, s + p + width, n - p - width, &w2, &e2) && e2 > 3) {
                    uint64_t f2;
                    if (!power10(e2, &f2) || chunk * factor > UINT64_MAX / f2)
                        return 0;
                    total += chunk * factor * f2;
                    last_exponent = e2;
                    p += width + w2;
                    continue;
                }
            }
            total += chunk * factor;
            last_exponent = exponent;
            p += width;
            continue;
        }
        if (decimal) {
            if (total)
                return 0;
            out->decimal = 1;
            out->value = chunk;
        } else
            total += chunk;
        break;
    }
    if (p == (size_t)(out->negative || out->positive))
        return 0;
    if (!out->decimal)
        out->value = total;
    /* Normalized source for the IR value: separators dropped. */
    {
        size_t k;
        for (k = 0; k < p; ++k) {
            if (s[k] == ',')
                continue;
            if (k + 3 <= p &&
                (!memcmp(s + k, "\xef\xbc\x8c", 3) || !memcmp(s + k, "\xe3\x80\x81", 3))) {
                k += 2;
                continue;
            }
            if (written + 1 >= norm_size)
                return 0;
            norm[written++] = s[k];
        }
        norm[written] = 0;
        *norm_length = written;
    }
    return p;
}

static unsigned ending(uint64_t v);
/* Bare number reading: いってんご, マイナスご, にせんにじゅうよん. */
static ttf_status_t number_reading(const ttf_engine_t *e, const ja_number_t *x, ttf_buffer_t *b) {
    ttf_status_t st;
    size_t k;
    if (x->negative && (st = word(e, b, "number", "minus")) != TTF_OK)
        return st;
    if (x->positive && (st = word(e, b, "number", "plus")) != TTF_OK)
        return st;
    if (!x->decimal)
        return integer_reading(e, x->value, b);
    if (!x->value && !x->scale_word)
        st = word(e, b, "number", "zero_integer");
    else
        st = integer_reading(e, x->value, b);
    if (st != TTF_OK)
        return st;
    /* 1.5 いってんご, 10.5 じゅってんご, 150.8 ひゃくごじゅってんはち (every ten added
       2026-10-14, owner sets); 8.5 stays はちてんご (ja_tts_fixes_v63) */
    if (x->value && (ending(x->value) == 1 || ending(x->value) == 10) &&
        (st = geminate(b)) != TTF_OK)
        return st;
    if ((st = word(e, b, "number", "point")) != TTF_OK)
        return st;
    for (k = 0; x->fraction[k]; ++k) {
        char key[2] = {x->fraction[k], 0};
        if ((st = word(e, b, "number", key)) != TTF_OK)
            return st;
    }
    return x->scale_word ? ttf_buffer_puts(b, x->scale_word) : TTF_OK;
}

/* The written ending that decides a sound change: 1..9, 10, 100, 1000 or 10000. */
static unsigned ending(uint64_t v) {
    if (v % 10)
        return (unsigned)(v % 10);
    if (v % 100)
        return 10;
    if (v % 1000)
        return 100;
    if (v % 10000)
        return 1000;
    return v ? 10000 : 0;
}

static int in_set(unsigned x, const unsigned *set, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i)
        if (set[i] == x)
            return 1;
    return 0;
}

/* Number plus counter with the counter's sound change. spec is the resource
 * value reading:class[:base]; key names the counter for its special readings. */
static ttf_status_t counted(const ttf_engine_t *e, const ja_number_t *x, const char *key,
                            const char *spec, int date_day, ttf_buffer_t *b) {
    char reading[64], cls[8] = "-", base[24] = "", special_key[96];
    const char *colon = strchr(spec, ':'), *colon2, *special = NULL, *owner;
    ttf_status_t st;
    size_t rn;
    unsigned end;
    if (!colon || (size_t)(colon - spec) >= sizeof(reading))
        return TTF_ERESOURCE;
    rn = (size_t)(colon - spec);
    memcpy(reading, spec, rn);
    reading[rn] = 0;
    colon2 = strchr(colon + 1, ':');
    if (colon2) {
        if ((size_t)(colon2 - colon - 1) >= sizeof(cls) || strlen(colon2 + 1) >= sizeof(base))
            return TTF_ERESOURCE;
        memcpy(cls, colon + 1, (size_t)(colon2 - colon - 1));
        cls[colon2 - colon - 1] = 0;
        snprintf(base, sizeof(base), "%s", colon2 + 1);
    } else if (strlen(colon + 1) < sizeof(cls))
        snprintf(cls, sizeof(cls), "%s", colon + 1);
    else
        return TTF_ERESOURCE;
    /* 日間 みっかかん: the base counter reads first, the suffix follows. */
    if (base[0]) {
        const char *base_spec = res(e, "counter", base);
        if (!base_spec)
            return TTF_ERESOURCE;
        if ((st = counted(e, x, base, base_spec, 0, b)) != TTF_OK)
            return st;
        return ttf_buffer_puts(b, reading);
    }
    /* +2,350歩 プラスにせんさんびゃくごじゅっぽ: a plus sign keeps the sound change (owner v2,
       2026-10-15) */
    if (x->positive && !x->decimal && !x->negative) {
        ja_number_t y = *x;
        y.positive = 0;
        if ((st = word(e, b, "number", "plus")) != TTF_OK)
            return st;
        return counted(e, &y, key, spec, date_day, b);
    }
    if (x->decimal || x->negative || x->positive) {
        if ((st = number_reading(e, x, b)) != TTF_OK)
            return st;
        return ttf_buffer_puts(b, reading);
    }
    owner = key;
    /* 何日 なんにち, not みっか: なん takes the sound change of 3 without its
       special readings. */
    if (x->nan) {
        if ((st = ttf_buffer_puts(b, "なん")) != TTF_OK)
            return st;
        goto sound_change;
    }
    if (date_day) {
        snprintf(special_key, sizeof(special_key), "date_day:%llu", (unsigned long long)x->value);
        special = res(e, "counter_reading", special_key);
    }
    if (!special && !x->plain) {
        snprintf(special_key, sizeof(special_key), "%s:%llu", owner, (unsigned long long)x->value);
        special = res(e, "counter_reading", special_key);
    }
    if (special)
        return ttf_buffer_puts(b, special);
    if (x->value % 10) {
        snprintf(special_key, sizeof(special_key), "%s:*%u", owner, (unsigned)(x->value % 10));
        special = res(e, "counter_reading", special_key);
        if (special) {
            if (x->value >= 10 && (st = integer_reading(e, x->value - x->value % 10, b)) != TTF_OK)
                return st;
            return ttf_buffer_puts(b, special);
        }
    }
    if ((st = integer_reading(e, x->value, b)) != TTF_OK)
        return st;
sound_change:
    end = ending(x->value);
    {
        static const unsigned gem5[] = {1, 6, 8, 10, 100}, gem3[] = {1, 8, 10};
        static const unsigned voice_h[] = {3, 1000, 10000}, half_f[] = {3, 4, 1000, 10000};
        static const unsigned gem_w[] = {6, 8, 10, 100};
        if (!strcmp(cls, "h") || !strcmp(cls, "f")) {
            if (in_set(end, gem5, 5)) {
                if ((st = geminate(b)) != TTF_OK)
                    return st;
                return put_voiced(b, reading, 1);
            }
            if (!strcmp(cls, "h") && in_set(end, voice_h, 3))
                return put_voiced(b, reading, 0);
            if (!strcmp(cls, "f") && in_set(end, half_f, 4))
                return put_voiced(b, reading, 1);
            return ttf_buffer_puts(b, reading);
        }
        if (!strcmp(cls, "k") || !strcmp(cls, "k3")) {
            if (in_set(end, gem5, 5) && (st = geminate(b)) != TTF_OK)
                return st;
            if (!strcmp(cls, "k3") && end == 3)
                return put_voiced(b, reading, 0);
            return ttf_buffer_puts(b, reading);
        }
        if (!strcmp(cls, "s") || !strcmp(cls, "s3") || !strcmp(cls, "t")) {
            if (in_set(end, gem3, 3) && (st = geminate(b)) != TTF_OK)
                return st;
            if (!strcmp(cls, "s3") && end == 3)
                return put_voiced(b, reading, 0);
            return ttf_buffer_puts(b, reading);
        }
        /* p10: 10 じゅっパーセント; k10 also 100 (ごひゃっキロ); 1 and 8 keep いち, はち */
        if (!strcmp(cls, "p10") || !strcmp(cls, "k10")) {
            if ((end == 10 || (end == 100 && cls[0] == 'k')) && (st = geminate(b)) != TTF_OK)
                return st;
            return ttf_buffer_puts(b, reading);
        }
        if (!strcmp(cls, "w")) {
            if (end == 3)
                return put_voiced(b, reading, 0);
            if (in_set(end, gem_w, 4)) {
                if ((st = geminate(b)) != TTF_OK)
                    return st;
                return put_voiced(b, reading, 1);
            }
            return ttf_buffer_puts(b, reading);
        }
        if (strcmp(cls, "-"))
            return TTF_ERESOURCE;
        return ttf_buffer_puts(b, reading);
    }
}

static ttf_status_t digits_each(const ttf_engine_t *e, const char *s, size_t n, int lengthen,
                                ttf_buffer_t *b);

static ttf_status_t counter_named(const ttf_engine_t *e, const ja_number_t *x, const char *key,
                                  int date_day, ttf_buffer_t *b) {
    const char *spec = res(e, "counter", key);
    return spec ? counted(e, x, key, spec, date_day, b) : TTF_ERESOURCE;
}

static ttf_status_t value_counted(const ttf_engine_t *e, const char *value, const char *key,
                                  int date_day, ttf_buffer_t *b) {
    ja_number_t x;
    char norm[TTF_IR_VALUE_SIZE];
    size_t nl = 0, n = strlen(value);
    if (!n || parse_number(e, value, n, &x, norm, sizeof(norm), &nl) != n)
        return TTF_EINVAL;
    return counter_named(e, &x, key, date_day, b);
}

/* Longest counter written at s (本, 日間, ヶ月, パーセント, %). */
/* 3年生の, 1期生。: 生 after the count is a student unless a word goes on (生まれ, 生活). */
static int student_end(const char *s, size_t n) {
    static const char *const next[] = {"の", "が", "は", "を", "に", "と", "も", "で", "へ", "や", "だ", "か"};
    size_t k;
    uint32_t cp;
    size_t w;
    if (!n || (unsigned char)s[0] < 0x80)
        return 1;
    for (k = 0; k < sizeof(next) / sizeof(*next); ++k)
        if (n >= 3 && !memcmp(s, next[k], 3))
            return 1;
    if (ttf_utf8_decode(s, n, &cp, &w) != 1)
        return 0;
    /* Punctuation and symbols end the word; a kanji or another kana goes on. */
    return ttf_script_of(cp) != TTF_SCRIPT_HAN && ttf_script_of(cp) != TTF_SCRIPT_KANA;
}

static size_t counter_at(const ttf_engine_t *e, const char *s, size_t n, char *key, size_t size) {
    size_t widths[8], count = 0, at = 0, k;
    while (at < n && count < 8) {
        uint32_t cp;
        size_t w;
        if (ttf_utf8_decode(s + at, n - at, &cp, &w) != 1 || cp <= ' ' ||
            (cp < 128 && digit((char)cp)))
            break;
        at += w;
        widths[count++] = at;
    }
    /* 1足す いち足す: a counter_block word is not a counter. */
    for (k = count; k > 0; --k) {
        char block[32];
        if (widths[k - 1] >= sizeof(block))
            continue;
        memcpy(block, s, widths[k - 1]);
        block[widths[k - 1]] = 0;
        if (res(e, "counter_block", block))
            return 0;
    }
    for (k = count; k > 0; --k) {
        if (widths[k - 1] >= size)
            continue;
        memcpy(key, s, widths[k - 1]);
        key[widths[k - 1]] = 0;
        /* 5ch, 3t: a Latin counter ends before another Latin letter (5chan). */
        if (res(e, "counter", key) &&
            !(alpha(key[widths[k - 1] - 1]) && widths[k - 1] < n && alpha(s[widths[k - 1]])))
            return widths[k - 1];
    }
    return 0;
}

static int range_mark(const char *s, size_t n, size_t *width) {
    if (n >= 1 && (s[0] == '~' || s[0] == '-')) {
        *width = 1;
        return 1;
    }
    if (n >= 3 && (!memcmp(s, "\xe3\x80\x9c", 3) || !memcmp(s, "\xef\xbd\x9e", 3))) {
        *width = 3;
        return 1;
    }
    return 0;
}

static int initialism(const char *s, size_t n) {
    size_t i;
    if (n < 2 || n > 4)
        return 0;
    for (i = 0; i < n; ++i)
        if (s[i] < 'A' || s[i] > 'Z')
            return 0;
    return 1;
}

static size_t finish(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t end,
                     ttf_semantic_type_t type, const char *rule, const char *unit) {
    size_t count = 0;
    while (in->index + count < in->token_count && in->tokens[in->index + count].begin < end)
        ++count;
    s->input_end = end;
    s->type = type;
    snprintf(s->source_lang, sizeof(s->source_lang), JA);
    snprintf(s->rule, sizeof(s->rule), "%s", rule);
    snprintf(s->unit, sizeof(s->unit), "%s", unit);
    return count;
}

static int day_one_after(const ttf_engine_t *e, const char *s, size_t n);

/* 後半, 前半: after a count these are words, not 後 ご / 前 まえ. */
static int half_word(const char *s, size_t n) {
    return n >= 6 && !memcmp(s + 3, "\xe5\x8d\x8a", 3);
}

/* A cue word (score_cue, ratio_cue) within window bytes after `after` or
 * before `before`, inside the sentence. */
static int cue_near(const ttf_engine_t *e, const char *kind, const char *text, size_t n,
                    size_t before, size_t after, size_t window) {
    size_t start = before > window ? before - window : 0, p, k;
    /* Stay inside the sentence: 。 or a line break ends the window. */
    for (k = start; k < before; ++k)
        if ((k + 3 <= before && !memcmp(text + k, "\xe3\x80\x82", 3)) || text[k] == '\n')
            start = k + 1;
    while (start < before && ((unsigned char)text[start] & 0xc0) == 0x80)
        ++start;
    for (p = start; p < n && p < after + window; ++p) {
        size_t len, chars = 0;
        if (((unsigned char)text[p] & 0xc0) == 0x80 || (p >= before && p < after))
            continue;
        if (p >= after && ((p + 3 <= n && !memcmp(text + p, "\xe3\x80\x82", 3)) || text[p] == '\n'))
            break;
        for (len = 0; p + len < n && chars < 4;) {
            char key[16];
            size_t w = 1;
            unsigned char c = (unsigned char)text[p + len];
            if (c >= 0xf0) w = 4; else if (c >= 0xe0) w = 3; else if (c >= 0xc0) w = 2;
            if (len + w >= sizeof(key))
                break;
            len += w;
            ++chars;
            memcpy(key, text + p, len);
            key[len] = 0;
            if (res(e, kind, key))
                return 1;
        }
    }
    return 0;
}

/* A Latin word in a Japanese sentence (2026-10-08, ja-tn-eval-v1 gaps):
 * No. ナンバー, Mr. ミスター (latin_abbreviation), No.1 ナンバーワン; NATO ナトー
 * (latin_word); v1.2.3 / Ver.2.0 バージョン...; a single letter next to Japanese
 * (Aランチ, ビタミンC, x軸) reads its letter name, while one among English words
 * (I love you, a pen) stays English. */
static size_t latin(const ttf_pack_input_t *in, ttf_semantic_t *s, const ttf_token_t *t) {
    const ttf_engine_t *e = in->engine;
    const char *text = in->text, *reading;
    size_t begin = t->begin, end = t->end, n = in->length, len = end - begin;
    char key[16];
    if (len + 2 > sizeof(key))
        return 0;
    memcpy(key, text + begin, len);
    key[len] = 0;
    /* B'z, it's: a letter joined by an apostrophe stays English. */
    if (begin && (alpha(text[begin - 1]) || text[begin - 1] == '\''))
        return 0;
    /* v1.2.3, ver2.1, Ver.2.0, Version 3.1.4 */
    if (!strcmp(key, "v") || !strcmp(key, "V") || !strcmp(key, "ver") || !strcmp(key, "Ver") ||
        !strcmp(key, "VER") || !strcmp(key, "version") || !strcmp(key, "Version")) {
        size_t q = end, groups = 0;
        if (q < n && (text[q] == '.' || text[q] == ' ') && len > 1)
            ++q;
        while (q < n && digit(text[q])) {
            while (q < n && digit(text[q]))
                ++q;
            ++groups;
            if (q + 1 < n && text[q] == '.' && digit(text[q + 1]))
                ++q;
            else
                break;
        }
        if (groups >= 2 && (q >= n || !(alpha(text[q]) || digit(text[q])))) {
            snprintf(s->secondary, sizeof(s->secondary), "v");
            return finish(in, s, q, TTF_SEM_VERSION, "ja.version", "");
        }
    }
    /* AM10:00 ごぜんじゅうじ, PM 3:30 */
    if (len == 2 && (key[0] == 'A' || key[0] == 'a' || key[0] == 'P' || key[0] == 'p') &&
        (key[1] == 'M' || key[1] == 'm')) {
        size_t q = end, h;
        if (q < n && text[q] == ' ')
            ++q;
        h = q;
        while (q < n && digit(text[q]) && q - h < 2)
            ++q;
        if (q > h && q + 2 < n && text[q] == ':' && digit(text[q + 1]) && digit(text[q + 2]) &&
            (q + 3 >= n || !digit(text[q + 3])) && q + 3 - h < sizeof(s->value)) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(q + 3 - h), text + h);
            snprintf(s->secondary, sizeof(s->secondary), "%cM", key[0] == 'a' || key[0] == 'A' ? 'A' : 'P');
            return finish(in, s, q + 3, TTF_SEM_TIME, "ja.meridiem-time", "");
        }
    }
    /* B1F ちかいっかい */
    if (len == 1 && key[0] == 'B' && end + 1 < n && digit(text[end])) {
        size_t q = end;
        while (q < n && digit(text[q]) && q - end < 2)
            ++q;
        if (q < n && text[q] == 'F' && (q + 1 >= n || !(alpha(text[q + 1]) || digit(text[q + 1])))) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - end), text + end);
            return finish(in, s, q + 1, TTF_SEM_SYMBOL, "ja.basement", "@basement");
        }
    }
    /* P.10 ページじゅう */
    if (len == 1 && (key[0] == 'P' || key[0] == 'p') && end + 1 < n && text[end] == '.' &&
        digit(text[end + 1])) {
        snprintf(s->value, sizeof(s->value), "ページ");
        return finish(in, s, end + 1, TTF_SEM_ABBREVIATION, "ja.page", "");
    }
    /* No.1 ナンバーワン; No. ナンバー, etc. エトセトラ */
    if (end < n && text[end] == '.') {
        key[len] = '.';
        key[len + 1] = 0;
        if (!strcmp(key, "No.") && end + 2 <= n && text[end + 1] == '1' &&
            (end + 2 >= n || !(digit(text[end + 2]) || text[end + 2] == '.'))) {
            snprintf(s->value, sizeof(s->value), "ナンバーワン");
            return finish(in, s, end + 2, TTF_SEM_ABBREVIATION, "ja.latin-abbreviation", "");
        }
        if ((reading = res(e, "latin_abbreviation", key)) && strlen(reading) < sizeof(s->value)) {
            snprintf(s->value, sizeof(s->value), "%s", reading);
            return finish(in, s, end + 1, TTF_SEM_ABBREVIATION, "ja.latin-abbreviation", "");
        }
        key[len] = 0;
    }
    /* JAN 4901…, ISBN978-4-…: a Latin number label and its digits are one code. */
    if (len >= 2 && initialism(key, len) && res(e, "digit_cue", key)) {
        size_t q = end + (end < n && text[end] == ' '), r = q;
        while (r < n && digit(text[r]))
            ++r;
        while (r + 1 < n && text[r] == '-' && digit(text[r + 1])) {
            ++r;
            while (r < n && digit(text[r]))
                ++r;
        }
        if (r - q >= 4 && (r >= n || !(alpha(text[r]) || digit(text[r]))) && r - begin < sizeof(s->value)) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(r - begin), text + begin);
            return finish(in, s, r, TTF_SEM_SYMBOL, "ja.code", "@code");
        }
    }
    if (end < n && (alpha(text[end]) || digit(text[end])))
        return 0;
    if ((reading = res(e, "latin_abbreviation", key)) && strlen(reading) < sizeof(s->value)) {
        snprintf(s->value, sizeof(s->value), "%s", reading);
        return finish(in, s, end, TTF_SEM_ABBREVIATION, "ja.latin-abbreviation", "");
    }
    /* iPhone アイフォン, YouTube ユーチューブ: a declared name in its written case
       (latin_name, 2026-10-30; was kept in Latin letters). */
    if ((reading = res(e, "latin_name", key)) && strlen(reading) < sizeof(s->value)) {
        snprintf(s->value, sizeof(s->value), "%s", reading);
        return finish(in, s, end, TTF_SEM_ABBREVIATION, "ja.latin-name", "");
    }
    /* COVID-19 goes on to the hyphen term below. */
    if ((reading = res(e, "latin_word", key)) && strlen(reading) < sizeof(s->value) &&
        !(end + 1 < n && text[end] == '-' && digit(text[end + 1]))) {
        snprintf(s->value, sizeof(s->value), "%s", reading);
        return finish(in, s, end, TTF_SEM_ABBREVIATION, "ja.latin-word", "");
    }
    /* SK-0392 エスケーのぜろさんきゅうに, K-2024-003817, TXN-7F3A9C: capitals, then
       hyphenated groups of digits and capitals. A hyphen reads の; a group with a
       leading zero or five digits or more reads digit by digit. One letter before
       one group of up to three digits (F-15) stays a plain term (owner sets,
       2026-10-14). */
    if (!begin || !(alpha(text[begin - 1]) || digit(text[begin - 1]))) {
        size_t l = begin, q, groups = 0, longest = 0;
        int zero = 0, has_digit = 0, mixed = 0;
        while (l < n && text[l] >= 'A' && text[l] <= 'Z')
            ++l;
        q = l;
        while (l > begin && l - begin <= 4 && q + 1 < n && text[q] == '-' &&
               (digit(text[q + 1]) || (text[q + 1] >= 'A' && text[q + 1] <= 'Z'))) {
            size_t g = ++q, run = 0;
            zero |= text[g] == '0' && g + 1 < n && digit(text[g + 1]);
            for (; q < n && (digit(text[q]) || (text[q] >= 'A' && text[q] <= 'Z')); ++q) {
                has_digit |= digit(text[q]);
                mixed |= !digit(text[q]) && has_digit;
                run = digit(text[q]) ? run + 1 : 0;
                if (run > longest)
                    longest = run;
            }
            ++groups;
        }
        if (has_digit && q - begin < sizeof(s->value) && (q >= n || !(alpha(text[q]) || digit(text[q]))) &&
            !(q + 1 < n && (text[q] == '.' || text[q] == ',') && digit(text[q + 1])) &&
            (groups > 1 || zero || longest >= 4 || mixed)) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - begin), text + begin);
            /* A-12345 エーいちにーさんよんごー: one letter and one group keep the hyphen silent */
            return finish(in, s, q, TTF_SEM_ALNUM_TERM, "ja.code",
                          l - begin == 1 && groups == 1 ? "code-silent" : "code");
        }
    }
    /* F-15, AK-47: capitals, a hyphen and a number read as one term. */
    if ((len <= 3 || res(e, "latin_word", key)) && end + 1 < n && text[end] == '-' &&
        digit(text[end + 1])) {
        size_t q = end + 1, k;
        int caps = 1;
        for (k = begin; k < end; ++k)
            caps &= text[k] >= 'A' && text[k] <= 'Z';
        while (q < n && digit(text[q]) && q - end <= 9)
            ++q;
        if (caps && q - end - 1 <= 8 &&
            (q >= n || !(alpha(text[q]) || digit(text[q]) || text[q] == '-' || text[q] == '.')) &&
            q - begin < sizeof(s->value)) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - begin), text + begin);
            return finish(in, s, q, TTF_SEM_ALNUM_TERM, "ja.alnum", "");
        }
    }
    /* O型 オーがた, AB型, B組 ビーぐみ, X線 エックスせん (letter_suffix). */
    if (len <= 3 && initialism(key, len) + (len == 1 && key[0] >= 'A' && key[0] <= 'Z') && end < n) {
        size_t k, best = 0;
        const char *suffix = NULL;
        for (k = 3; k <= 6 && end + k <= n; k += 3) {
            char w[8];
            const char *r;
            memcpy(w, text + end, k);
            w[k] = 0;
            if ((r = res(e, "letter_suffix", w)))
                best = k, suffix = r;
        }
        if (suffix) {
            size_t i, vl = 0;
            for (i = 0; i < len; ++i) {
                char c[2] = {key[i], 0};
                const char *name = res(e, "letter_name", c);
                if (!name || vl + strlen(name) >= sizeof(s->value))
                    return 0;
                vl += (size_t)snprintf(s->value + vl, sizeof(s->value) - vl, "%s", name);
            }
            if (vl + strlen(suffix) >= sizeof(s->value))
                return 0;
            snprintf(s->value + vl, sizeof(s->value) - vl, "%s", suffix);
            return finish(in, s, end + best, TTF_SEM_ABBREVIATION, "ja.letter-suffix", "");
        }
    }
    if (len == 1 && !(end < n && (text[end] == '.' || text[end] == '\'' || text[end] == '&' ||
                                  (text[end] == '-' && end + 1 < n && alpha(text[end + 1]))))) {
        /* English context: a Latin word one space before or after. */
        if (begin >= 2 && text[begin - 1] == ' ' && alpha(text[begin - 2]))
            return 0;
        if (end + 1 < n && text[end] == ' ' && alpha(text[end + 1]))
            return 0;
        snprintf(s->value, sizeof(s->value), "%s", key);
        return finish(in, s, end, TTF_SEM_INITIALISM, "ja.letter", "");
    }
    return 0;
}

/* Owner evaluation sets, phase 5 (2026-10-14). */
static size_t owner_forms(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t begin) {
    const char *text = in->text;
    size_t n = in->length, q, z;
    /* 前年同期比＋2.8％ ぷらすにてんはちパーセント: the plus sign ends the word token before
       the number (a separate ＋2.8％ is already a signed percentage) */
    {
        size_t end = in->tokens[in->index].end;
        /* the core hands the pack ASCII digits and signs: 前年比+2.8% */
        if (end > begin + 3 && end < n && text[end - 1] == '+' && digit(text[end]) &&
            ((unsigned char)text[end - 2] & 0x80) && end - begin + 9 < sizeof(s->value)) {
            snprintf(s->value, sizeof(s->value), "%.*sプラス", (int)(end - 1 - begin), text + begin);
            return finish(in, s, end, TTF_SEM_ABBREVIATION, "ja.plus", "");
        }
    }
    /* AC100V エーシーひゃくボルト, DC12V: a current type and its voltage */
    if ((!memcmp(text + begin, "AC", 2) || !memcmp(text + begin, "DC", 2)) && begin + 4 <= n && digit(text[begin + 2]) &&
        !(begin && (alpha(text[begin - 1]) || digit(text[begin - 1])))) {
        for (q = begin + 2; q < n && digit(text[q]); ++q)
            ;
        if (q - begin - 2 <= 4 && q < n && text[q] == 'V' && !(q + 1 < n && (alpha(text[q + 1]) || digit(text[q + 1])))) {
            snprintf(s->value, sizeof(s->value), "%.2s|%.*s", text + begin, (int)(q - begin - 2), text + begin + 2);
            return finish(in, s, q + 1, TTF_SEM_SYMBOL, "ja.current", "@current");
        }
    }
    /* R7.6.30 れいわななねんろくがつさんじゅうにち: an abbreviated era date */
    if ((text[begin] == 'R' || text[begin] == 'H') && begin + 6 <= n && digit(text[begin + 1]) &&
        !(begin && (alpha(text[begin - 1]) || digit(text[begin - 1])))) {
        unsigned f[3] = {0, 0, 0};
        int k, ok = 1;
        q = begin + 1;
        for (k = 0; k < 3 && ok; ++k) {
            z = q;
            while (z < n && digit(text[z]) && z - q < 3)
                f[k] = f[k] * 10 + (unsigned)(text[z++] - '0');
            ok = z > q && z - q <= 2 && (k == 2 || (z < n && text[z] == '.'));
            q = k == 2 ? z : z + 1;
        }
        if (ok && f[0] >= 1 && f[1] >= 1 && f[1] <= 12 && f[2] >= 1 && f[2] <= 31 &&
            !(q < n && (digit(text[q]) || alpha(text[q]) || (text[q] == '.' && q + 1 < n && digit(text[q + 1]))))) {
            snprintf(s->value, sizeof(s->value), "%s|%u年%u月%u日", text[begin] == 'R' ? "れいわ" : "へいせい",
                     f[0], f[1], f[2]);
            return finish(in, s, q, TTF_SEM_COUNTER, "ja.era-date", "@era");
        }
    }
    if (!digit(text[begin]) || (begin && (digit(text[begin - 1]) || alpha(text[begin - 1]))))
        return 0;
    for (q = begin; q < n && digit(text[q]); ++q)
        ;
    /* 128/82mmHg ひゃくにじゅうはちのはちじゅうにミリエイチジー: a blood pressure */
    if (q - begin >= 2 && q - begin <= 3 && q + 7 <= n && text[q] == '/' && digit(text[q + 1])) {
        for (z = q + 1; z < n && digit(text[z]); ++z)
            ;
        if (z - q - 1 >= 2 && z - q - 1 <= 3 && z + 4 <= n && !memcmp(text + z, "mmHg", 4)) {
            snprintf(s->value, sizeof(s->value), "%.*s|%.*s", (int)(q - begin), text + begin, (int)(z - q - 1),
                     text + q + 1);
            return finish(in, s, z + 4, TTF_SEM_SYMBOL, "ja.pressure", "@pressure");
        }
    }
    /* 24:00 にじゅうよじ: the end of the day */
    if (q - begin == 2 && !memcmp(text + begin, "24:00", 5) && !(begin + 5 < n && (digit(text[begin + 5]) ||
                                                                                   text[begin + 5] == ':'))) {
        snprintf(s->value, sizeof(s->value), "24時");
        return finish(in, s, begin + 5, TTF_SEM_COUNTER, "ja.end-of-day", "@ja");
    }
    /* 座席は23A にじゅうさんエー: a seat number */
    if (q - begin <= 2 && q < n && text[q] >= 'A' && text[q] <= 'K' &&
        !(q + 1 < n && (alpha(text[q + 1]) || digit(text[q + 1])))) {
        size_t from = begin > 15 ? begin - 15 : 0, k;
        int seat = q + 4 <= n && !memcmp(text + q + 1, "席", 3);
        for (k = from; k + 6 <= begin && !seat; ++k)
            seat = !memcmp(text + k, "座席", 6);
        if (seat) {
            snprintf(s->value, sizeof(s->value), "%.*s|%c", (int)(q - begin), text + begin, text[q]);
            return finish(in, s, q + 1, TTF_SEM_SYMBOL, "ja.seat", "@seat");
        }
    }
    /* 2^10 にのじゅうじょう */
    if (q - begin <= 6 && q + 1 < n && text[q] == '^' && digit(text[q + 1])) {
        for (z = q + 1; z < n && digit(text[z]); ++z)
            ;
        if (z - q - 1 <= 3 && !(z < n && (alpha(text[z]) || text[z] == '.'))) {
            snprintf(s->value, sizeof(s->value), "%.*s|%.*s", (int)(q - begin), text + begin, (int)(z - q - 1),
                     text + q + 1);
            return finish(in, s, z, TTF_SEM_SYMBOL, "ja.power", "@power");
        }
    }
    /* 090 8812 4403, ID: 823 4471 0956: spaced digit groups of a phone number or an ID read with の */
    {
        char v[48];
        size_t used = 0, groups = 0, total = 0, last = begin, g;
        z = begin;
        for (;;) {
            for (g = z; g < n && digit(text[g]); ++g)
                ;
            if (g - z < (groups ? 3u : 2u) || g - z > 4 || used + (g - z) + 2 >= sizeof(v))
                break;
            if (groups)
                v[used++] = '-';
            memcpy(v + used, text + z, g - z);
            used += g - z;
            total += g - z;
            ++groups;
            last = g;
            if (!(g + 1 < n && text[g] == ' ' && digit(text[g + 1])))
                break;
            z = g + 1;
        }
        v[used] = 0;
        if (groups >= 3 && total >= 10 && !(last < n && (digit(text[last]) || alpha(text[last])))) {
            size_t from = begin > 24 ? begin - 24 : 0, k;
            int cue = text[begin] == '0';
            for (k = from; k + 2 <= begin && !cue; ++k)
                cue = !memcmp(text + k, "ID", 2) || (k + 15 <= begin && !memcmp(text + k, "アイディー", 15));
            if (cue && used < sizeof(s->value)) {
                snprintf(s->value, sizeof(s->value), "%s", v);
                return finish(in, s, last, TTF_SEM_PHONE, "ja.spaced-phone", "");
            }
        }
    }
    return 0;
}

/* 3本, 2024年10月8日, 4時15分, 1万円, 3〜5個, -5度: digits with written counters
 * become one counted span; the value keeps the normalized source. */
/* ---- Japanese dev set v1 forms (2026-10-17) ---- */
static int ja_prev_is(const char *t, size_t p, const char *w) {
    size_t l = strlen(w);
    return p >= l && !memcmp(t + p - l, w, l);
}
static int ja_next_is(const char *t, size_t n, size_t q, const char *w) {
    size_t l = strlen(w);
    return q + l <= n && !memcmp(t + q, w, l);
}
static size_t ja_dev_forms(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t begin) {
    const char *t = in->text;
    size_t n = in->length, p = begin, q, r;
    if (p >= n)
        return 0;
    /* x² エックスのにじょう, 2¹⁰ にのじゅうじょう: superscript digits after a number, a letter or a bracket */
    if (p && (digit(t[p - 1]) || alpha(t[p - 1]) || t[p - 1] == ')')) {
        size_t z = p;
        unsigned x = 0, cnt = 0;
        for (;;) {
            unsigned char c0 = (unsigned char)t[z], c1 = z + 1 < n ? (unsigned char)t[z + 1] : 0,
                          c2 = z + 2 < n ? (unsigned char)t[z + 2] : 0;
            int d = -1;
            if (c0 == 0xC2 && (c1 == 0xB2 || c1 == 0xB3 || c1 == 0xB9))
                d = c1 == 0xB2 ? 2 : c1 == 0xB3 ? 3 : 1, z += 2;
            else if (c0 == 0xE2 && c1 == 0x81 && (c2 == 0xB0 || (c2 >= 0xB4 && c2 <= 0xB9)))
                d = c2 == 0xB0 ? 0 : c2 - 0xB0, z += 3;
            if (d < 0 || cnt > 3)
                break;
            x = x * 10 + (unsigned)d, ++cnt;
            if (z >= n)
                break;
        }
        if (cnt) {
            snprintf(s->value, sizeof(s->value), "w:の|n:%u|w:じょう", x);
            return finish(in, s, z, TTF_SEM_COUNTER, "ja.dev-power", "@say");
        }
    }
    /* 〒100-8926 ゆうびんばんごう…: the postal mark before a code (alone it stays ゆうびん) */
    if (ja_next_is(t, n, p, "〒") && p + 3 < n && (digit(t[p + 3]) || (t[p + 3] == ' ' && p + 4 < n && digit(t[p + 4])))) {
        snprintf(s->value, sizeof(s->value), "ゆうびんばんごう");
        return finish(in, s, p + 3, TTF_SEM_ABBREVIATION, "ja.dev-postal-mark", "");
    }
    /* 第2位 だいにい, 午後3時 ごごさんじ, 令和8年 れいわはちねん, 平成元年 へいせいがんねん: a kanji prefix
       of a number reads in kana with it, so the span is one reading (project-owner decision 2026-10-21) */
    {
        static const char *const pre[] = {"第", "だい", "午前", "ごぜん", "午後", "ごご", "令和", "れいわ",
                                          "平成", "へいせい", "昭和", "しょうわ", "大正", "たいしょう"};
        size_t i;
        for (i = 0; i < sizeof(pre) / sizeof(pre[0]); i += 2) {
            size_t l = strlen(pre[i]);
            if (ja_next_is(t, n, p, pre[i]) && p + l < n && digit(t[p + l])) {
                snprintf(s->value, sizeof(s->value), "%s", pre[i + 1]);
                return finish(in, s, p + l, TTF_SEM_ABBREVIATION, "ja.dev-prefix", "");
            }
            if (i >= 6 && ja_next_is(t, n, p, pre[i]) && ja_next_is(t, n, p + l, "元年")) {
                snprintf(s->value, sizeof(s->value), "%sがんねん", pre[i + 1]);
                return finish(in, s, p + l + 6, TTF_SEM_ABBREVIATION, "ja.dev-prefix", "");
            }
        }
    }
    /* #Jリーグ ハッシュタグジェーリーグ: a hash before a word is a hashtag */
    if ((t[p] == '#' || ja_next_is(t, n, p, "\xef\xbc\x83")) && !(p && (alpha(t[p - 1]) || digit(t[p - 1])))) {
        size_t z = p + (t[p] == '#' ? 1 : 3);
        if (z < n && t[z] != ' ' && !digit(t[z]) && (alpha(t[z]) || ((unsigned char)t[z] & 0x80))) {
            snprintf(s->value, sizeof(s->value), "ハッシュタグ");
            return finish(in, s, z, TTF_SEM_ABBREVIATION, "ja.dev-hashtag", "");
        }
    }
    /* ⅛ はちぶんのいち: a vulgar fraction */
    if ((unsigned char)t[p] == 0xE2 && p + 2 < n && (unsigned char)t[p + 1] == 0x85 &&
        (unsigned char)t[p + 2] >= 0x93 && (unsigned char)t[p + 2] <= 0x9E) {
        static const unsigned char nd[][2] = {{1, 3}, {2, 3}, {1, 5}, {2, 5}, {3, 5}, {4, 5}, {1, 6},
                                              {5, 6}, {1, 8}, {3, 8}, {5, 8}, {7, 8}};
        unsigned i = (unsigned)((unsigned char)t[p + 2] - 0x93);
        snprintf(s->value, sizeof(s->value), "%u", nd[i][0]);
        snprintf(s->secondary, sizeof(s->secondary), "%u", nd[i][1]);
        return finish(in, s, p + 3, TTF_SEM_FRACTION, "ja.dev-vulgar", "");
    }
    if (!digit(t[p]) || (p && (digit(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ',' || alpha(t[p - 1]))))
        goto symbols;
    for (q = p; q < n && digit(t[q]); ++q)
        ;
    /* 15−8 じゅうごひくはち: a minus sign between numbers is subtraction, not a range */
    if (ja_next_is(t, n, q, "\xe2\x88\x92") && q + 3 < n && digit(t[q + 3])) {
        for (r = q + 3; r < n && digit(t[r]); ++r)
            ;
        if (!(r < n && (digit(t[r]) || t[r] == '.' || ja_next_is(t, n, r, "\xe2\x88\x92") || alpha(t[r]))) &&
            q - p + r - q + 4 < sizeof(s->value)) {
            snprintf(s->value, sizeof(s->value), "%.*s \xe2\x88\x92 %.*s", (int)(q - p), t + p, (int)(r - q - 3), t + q + 3);
            return finish(in, s, r, TTF_SEM_SYMBOL, "ja.dev-minus", "@expression");
        }
    }
    /* Python 3.12 さんてんじゅうに: a version after a Latin name or バージョン */
    if (q + 2 < n && t[q] == '.' && digit(t[q + 1]) && p >= 2 &&
        ((t[p - 1] == ' ' && alpha(t[p - 2])) || ja_prev_is(t, p, "バージョン") || ja_prev_is(t, p, "Ver.") ||
         ja_prev_is(t, p, "ver."))) {
        size_t z = q + 1, k2;
        char *o = s->value;
        while (z < n && digit(t[z]))
            ++z;
        if (z - q - 1 >= 2 && !(z < n && (digit(t[z]) || alpha(t[z]) || (t[z] == '.' && z + 1 < n && digit(t[z + 1]))))) {
            o += snprintf(o, sizeof(s->value), "n:%.*s|w:てん|n:%.*s", (int)(q - p), t + p, (int)(z - q - 1), t + q + 1);
            (void)k2;
            return finish(in, s, z, TTF_SEM_COUNTER, "ja.dev-version", "@say");
        }
    }
    /* M/D or a fraction: 全日程の7/10 じゅうぶんのなな, 2と3/4, 1/5出発 いちがついつか, 12/29の便 */
    if (q - p <= 2 && q + 1 < n && t[q] == '/' && digit(t[q + 1])) {
        unsigned a = (unsigned)atoi(t + p), d;
        for (r = q + 1; r < n && digit(t[r]); ++r)
            ;
        d = (unsigned)atoi(t + q + 1);
        if (r - q - 1 <= 3 && !(r < n && (digit(t[r]) || t[r] == '/' || t[r] == '.' || alpha(t[r]) || t[r] == '(' ||
                                           ja_next_is(t, n, r, "\xef\xbc\x88")))) {
            int common = d == 2 || d == 3 || d == 4 || d == 5 || d == 6 || d == 8 || d == 10 || d == 12 || d == 16 ||
                         d == 20 || d == 25 || d == 50 || d == 100 || d == 1000;
            int fraction_cue = ja_prev_is(t, p, "の") || ja_prev_is(t, p, "約") || ja_prev_is(t, p, "およそ") ||
                               ja_prev_is(t, p, "と");
            int date_cue = ja_next_is(t, n, r, "出発") || ja_next_is(t, n, r, "発") || ja_next_is(t, n, r, "着") ||
                           ja_next_is(t, n, r, "入社") || ja_next_is(t, n, r, "生まれ") || ja_next_is(t, n, r, "公開") ||
                           ja_next_is(t, n, r, "開催") || ja_next_is(t, n, r, "発売") || ja_next_is(t, n, r, "締め切り") ||
                           ja_next_is(t, n, r, "締切") || ja_next_is(t, n, r, "以降") || ja_next_is(t, n, r, "ごろ") ||
                           ja_next_is(t, n, r, "頃") || ja_next_is(t, n, r, "まで") || ja_next_is(t, n, r, "から") ||
                           ja_next_is(t, n, r, "の便") || ja_next_is(t, n, r, "限り") || ja_next_is(t, n, r, "当日") ||
                           ja_next_is(t, n, r, "予定");
            if (a >= 1 && a < d && common && fraction_cue &&
                (!date_cue || ja_prev_is(t, p, "と") || ja_prev_is(t, p, "の"))) {
                snprintf(s->value, sizeof(s->value), "%u", a);
                snprintf(s->secondary, sizeof(s->secondary), "%u", d);
                return finish(in, s, r, TTF_SEM_FRACTION, "ja.dev-fraction", "");
            }
            if (a >= 1 && a <= 12 && d >= 1 && d <= 31 && r - q - 1 <= 2 && !ja_prev_is(t, p, "と") && date_cue) {
                snprintf(s->value, sizeof(s->value), "r:%u月%u日", a, d);
                return finish(in, s, r, TTF_SEM_COUNTER, "ja.dev-date", "@say");
            }
        }
    }
    /* 2丁目14-3 にちょうめじゅうよんのさん: a block and house number after 丁目 */
    if (ja_prev_is(t, p, "丁目") && q + 1 < n && t[q] == '-' && digit(t[q + 1])) {
        for (r = q + 1; r < n && digit(t[r]); ++r)
            ;
        if (r - p < 16 && !(r < n && (digit(t[r]) || alpha(t[r])))) {
            size_t r2 = r;
            if (r + 1 < n && t[r] == '-' && digit(t[r + 1]))
                for (r2 = r + 1; r2 < n && digit(t[r2]); ++r2)
                    ;
            if (r2 == r)
                snprintf(s->value, sizeof(s->value), "n:%.*s|w:の|n:%.*s", (int)(q - p), t + p, (int)(r - q - 1), t + q + 1);
            else
                snprintf(s->value, sizeof(s->value), "n:%.*s|w:の|n:%.*s|w:の|n:%.*s", (int)(q - p), t + p, (int)(r - q - 1),
                         t + q + 1, (int)(r2 - r - 1), t + r + 1);
            return finish(in, s, r2, TTF_SEM_COUNTER, "ja.dev-address", "@say");
        }
    }
    /* 12.50ドル じゅうにてんごドル: a currency amount drops the trailing zero of its cents */
    if (q + 2 < n && t[q] == '.' && digit(t[q + 1])) {
        size_t f = q + 1, z;
        for (z = f; z < n && digit(t[z]); ++z)
            ;
        if (z - f == 2 && t[z - 1] == '0' &&
            (ja_next_is(t, n, z, "ドル") || ja_next_is(t, n, z, "ユーロ") || ja_next_is(t, n, z, "ポンド") ||
             ja_next_is(t, n, z, "元"))) {
            if (t[f] == '0')
                snprintf(s->value, sizeof(s->value), "n:%.*s", (int)(q - p), t + p);
            else
                snprintf(s->value, sizeof(s->value), "n:%.*s.%c", (int)(q - p), t + p, t[f]);
            return finish(in, s, z, TTF_SEM_COUNTER, "ja.dev-cents", "@say");
        }
    }
    /* 38万km さんじゅうはちまんキロメートル: a unit after 万 or 億 */
    if (ja_next_is(t, n, q, "万") || ja_next_is(t, n, q, "億")) {
        static const char *const unit[] = {"km", "キロメートル", "kg", "キログラム", "kW", "キロワット", "kWh",
                                           "キロワットアワー", "t", "トン", "m", "メートル", "ha", "ヘクタール",
                                           "TB", "テラバイト", "GB", "ギガバイト"};
        size_t z = q + 3, i;
        for (i = 0; i < sizeof(unit) / sizeof(unit[0]); i += 2) {
            size_t l = strlen(unit[i]);
            if (ja_next_is(t, n, z, unit[i]) && !(z + l < n && (alpha(t[z + l]) || t[z + l] == '/'))) {
                snprintf(s->value, sizeof(s->value), "r:%.*s%.3s|w:%s", (int)(q - p), t + p, t + q, unit[i + 1]);
                return finish(in, s, z + l, TTF_SEM_COUNTER, "ja.dev-scaled-unit", "@say");
            }
        }
    }
    /* 86°F かしはちじゅうろくど: Fahrenheit before the number */
    {
        size_t z = q;
        if (z + 1 < n && t[z] == '.' && digit(t[z + 1]))
            for (++z; z < n && digit(t[z]); ++z)
                ;
        if (ja_next_is(t, n, z, "°F") || ja_next_is(t, n, z, "℉")) {
            snprintf(s->value, sizeof(s->value), "w:かし|n:%.*s|w:ど", (int)(z - p), t + p);
            return finish(in, s, z + 3, TTF_SEM_COUNTER, "ja.dev-fahrenheit", "@say");
        }
        /* 0.05μSv れいてんぜろごマイクロシーベルト: a micro unit */
        if (ja_next_is(t, n, z, "μ") || ja_next_is(t, n, z, "µ")) {
            static const char *const unit[] = {"Sv", "シーベルト", "g", "グラム", "m", "メートル", "L", "リットル",
                                               "l", "リットル", "s", "秒", "F", "ファラド", "A", "アンペア"};
            size_t u = z + 2, i;
            for (i = 0; i < sizeof(unit) / sizeof(unit[0]); i += 2) {
                size_t l = strlen(unit[i]);
                if (ja_next_is(t, n, u, unit[i]) && !(u + l < n && alpha(t[u + l]))) {
                    snprintf(s->value, sizeof(s->value), "n:%.*s|w:マイクロ%s", (int)(z - p), t + p, unit[i + 1]);
                    return finish(in, s, u + l, TTF_SEM_COUNTER, "ja.dev-micro", "@say");
                }
            }
        }
        /* 25.4km/L にじゅうごてんよんキロメートルパーリットル */
        if (ja_next_is(t, n, z, "km/L") && !(z + 4 < n && alpha(t[z + 4]))) {
            snprintf(s->value, sizeof(s->value), "n:%.*s|w:キロメートルパーリットル", (int)(z - p), t + p);
            return finish(in, s, z + 4, TTF_SEM_COUNTER, "ja.dev-fuel", "@say");
        }
    }
    return 0;
symbols:
    /* B1階 ちかいっかい: a basement floor */
    if (t[p] == 'B' && p + 1 < n && digit(t[p + 1]) && !(p && alpha(t[p - 1]))) {
        for (q = p + 1; q < n && digit(t[q]); ++q)
            ;
        if (q - p <= 2 && (ja_next_is(t, n, q, "階") || ja_next_is(t, n, q, "F"))) {
            snprintf(s->value, sizeof(s->value), "w:ちか|r:%.*s階", (int)(q - p - 1), t + p + 1);
            return finish(in, s, q + (t[q] == 'F' ? 1 : 3), TTF_SEM_COUNTER, "ja.dev-basement", "@say");
        }
    }
    /* 1ドル＝152円 いちドルひゃくごじゅうにえん: an exchange rate keeps no equals word */
    if ((ja_next_is(t, n, p, "＝") || t[p] == '=') &&
        (ja_prev_is(t, p, "ドル") || ja_prev_is(t, p, "ユーロ") || ja_prev_is(t, p, "ポンド") || ja_prev_is(t, p, "元")) &&
        p + (t[p] == '=' ? 1 : 3) < n && digit(t[p + (t[p] == '=' ? 1 : 3)])) {
        snprintf(s->value, sizeof(s->value), "、");
        return finish(in, s, p + (t[p] == '=' ? 1 : 3), TTF_SEM_ABBREVIATION, "ja.dev-rate", "");
    }
    /* £5.60 ごポンドろくじゅうペンス, $12.50 じゅうにてんごドル */
    if ((ja_next_is(t, n, p, "£") || t[p] == '$') && !(p && (alpha(t[p - 1]) || digit(t[p - 1])))) {
        size_t a = p + (t[p] == '$' ? 1 : 2), z, f;
        for (z = a; z < n && digit(t[z]); ++z)
            ;
        if (z > a && z - a <= 6 && z + 2 < n && t[z] == '.' && digit(t[z + 1]) && digit(t[z + 2]) &&
            !(z + 3 < n && digit(t[z + 3]))) {
            f = z + 1;
            if (t[p] == '$')
                snprintf(s->value, sizeof(s->value), "n:%.*s.%.*s|w:ドル", (int)(z - a), t + a, t[f + 1] == '0' ? 1 : 2, t + f);
            else if (t[f] == '0' && t[f + 1] == '0')
                snprintf(s->value, sizeof(s->value), "n:%.*s|w:ポンド", (int)(z - a), t + a);
            else
                snprintf(s->value, sizeof(s->value), "n:%.*s|w:ポンド|n:%u|w:ペンス", (int)(z - a), t + a, (unsigned)atoi(t + f));
            if (t[p] == '$' && t[f] == '0' && t[f + 1] == '0')
                snprintf(s->value, sizeof(s->value), "n:%.*s|w:ドル", (int)(z - a), t + a);
            return finish(in, s, z + 3, TTF_SEM_COUNTER, "ja.dev-money", "@say");
        }
    }
    return 0;
}

static size_t classify(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const ttf_engine_t *e = in->engine;
    const ttf_token_t *t;
    const char *text = in->text;
    size_t begin, p, n = in->length, pairs = 0, nl = 0;
    char norm[TTF_IR_VALUE_SIZE];
    /* The core calls this only where ja-JP is the context: a declared request
       or a kana sentence (phase 2). */
    if (in->index >= in->token_count)
        return 0;
    t = &in->tokens[in->index];
    begin = t->begin;
    {
        size_t owned = ja_dev_forms(in, s, begin);
        if (owned)
            return owned;
        if ((owned = owner_forms(in, s, begin)) != 0)
            return owned;
    }
    /* B'z ビーズ, Wi-Fi ワイファイ, 7-Eleven セブンイレブン (latin_phrase). */
    if ((t->script == TTF_SCRIPT_LATIN || digit(text[begin])) && !(begin && (alpha(text[begin - 1]) ||
                                                                               digit(text[begin - 1])))) {
        size_t q = begin;
        char key[24];
        const char *reading;
        while (q < n && (alpha(text[q]) || digit(text[q]) || text[q] == '-' || text[q] == '\'') &&
               q - begin < sizeof(key) - 1)
            ++q;
        while (q > begin) {
            memcpy(key, text + begin, q - begin);
            key[q - begin] = 0;
            if ((q >= n || !(alpha(text[q]) || digit(text[q]))) &&
                (reading = res(e, "latin_phrase", key)) && strlen(reading) < sizeof(s->value)) {
                snprintf(s->value, sizeof(s->value), "%s", reading);
                return finish(in, s, q, TTF_SEM_ABBREVIATION, "ja.latin-phrase", "");
            }
            if (text[q - 1] != '-' && text[q - 1] != '\'')
                break;
            --q;
        }
    }
    /* ファイナルファンタジーⅦ セブン: a Roman numeral after katakana reads in English. */
    if (begin + 3 <= n && (unsigned char)text[begin] == 0xe2 && (unsigned char)text[begin + 1] == 0x85 &&
        (unsigned char)text[begin + 2] >= 0xa0 && (unsigned char)text[begin + 2] <= 0xab && begin >= 3) {
        uint32_t prev;
        size_t pw;
        char d[8];
        const char *reading;
        if (ttf_utf8_decode(text + begin - 3, 3, &prev, &pw) == 1 && prev >= 0x30a1 && prev <= 0x30fc) {
            snprintf(d, sizeof(d), "%d", (unsigned char)text[begin + 2] - 0xa0 + 1);
            if ((reading = res(e, "english_number", d))) {
                snprintf(s->value, sizeof(s->value), "%s", reading);
                return finish(in, s, begin + 3, TTF_SEM_ABBREVIATION, "ja.roman-english", "");
            }
        }
    }
    /* .pdf ドットピーディーエフ */
    if (text[begin] == '.' && begin + 2 < n && alpha(text[begin + 1])) {
        size_t q = begin + 1;
        char ext[8];
        const char *reading;
        while (q < n && text[q] >= 'a' && text[q] <= 'z' && q - begin <= 5)
            ++q;
        if (q - begin - 1 < sizeof(ext) && (q >= n || !(alpha(text[q]) || digit(text[q]) || text[q] == '.'))) {
            memcpy(ext, text + begin + 1, q - begin - 1);
            ext[q - begin - 1] = 0;
            if ((reading = res(e, "file_extension", ext)) && strlen(reading) + 12 < sizeof(s->value)) {
                snprintf(s->value, sizeof(s->value), "ドット%s", reading);
                return finish(in, s, q, TTF_SEM_ABBREVIATION, "ja.extension", "");
            }
        }
    }
    /* 月〜金 げつからきん: one-character weekdays around a range mark. */
    if ((t->script == TTF_SCRIPT_HAN || t->script == TTF_SCRIPT_KANA) && t->end - begin >= 3) {
        char a[4], z[4];
        size_t w = 0, at = t->end;
        uint32_t before = 0;
        size_t bw;
        memcpy(a, text + at - 3, 3);
        a[3] = 0;
        if (at - 3 > begin) {
            size_t k = at - 3;
            do
                --k;
            while (k > begin && ((unsigned char)text[k] & 0xc0) == 0x80);
            if (ttf_utf8_decode(text + k, at - 3 - k, &before, &bw) != 1)
                before = 0;
        }
        if (res(e, "weekday_short", a) && ttf_script_of(before) != TTF_SCRIPT_HAN &&
            range_mark(text + at, n - at, &w) && text[at] != '-' && at + w + 3 <= n) {
            memcpy(z, text + at + w, 3);
            z[3] = 0;
            if (res(e, "weekday_short", z) &&
                !(at + w + 6 <= n && !memcmp(text + at + w + 3, "曜", 3)) &&
                at + w + 3 - begin < sizeof(s->value)) {
                snprintf(s->value, sizeof(s->value), "%.*s", (int)(at + w + 3 - begin), text + begin);
                snprintf(s->secondary, sizeof(s->secondary), "%u", (unsigned)w);
                return finish(in, s, at + w + 3, TTF_SEM_SYMBOL, "ja.weekday-range", "@weekday-range");
            }
        }
    }
    if (t->script == TTF_SCRIPT_LATIN) {
        size_t claimed = latin(in, s, t);
        if (claimed)
            return claimed;
    }
    /* ￥12,800 いちまんにせんはっぴゃくえん */
    if (begin + 3 < n && !memcmp(text + begin, "\xef\xbf\xa5", 3) && digit(text[begin + 3])) {
        ja_number_t x;
        size_t nl2 = 0, used = parse_number(e, text + begin + 3, n - begin - 3, &x, norm, sizeof(norm), &nl2);
        if (used && !x.negative && nl2 + 4 < sizeof(s->value)) {
            snprintf(s->value, sizeof(s->value), "%.*s円", (int)nl2, norm);
            return finish(in, s, begin + 3 + used, TTF_SEM_COUNTER, "ja.yen", "@ja");
        }
    }
    /* #拡散希望 ハッシュタグ */
    if (text[begin] == '#' && begin + 1 < n && (unsigned char)text[begin + 1] >= 0xe3) {
        snprintf(s->value, sizeof(s->value), "ハッシュタグ");
        return finish(in, s, begin + 1, TTF_SEM_ABBREVIATION, "ja.hashtag", "");
    }
    /* 〒100-0001 ゆうびんばんごう… */
    if (begin + 11 <= n && !memcmp(text + begin, "\xe3\x80\x92", 3) && digit(text[begin + 3]) &&
        digit(text[begin + 4]) && digit(text[begin + 5]) && text[begin + 6] == '-' &&
        digit(text[begin + 7]) && digit(text[begin + 8]) && digit(text[begin + 9]) &&
        digit(text[begin + 10]) && (begin + 11 >= n || !digit(text[begin + 11]))) {
        snprintf(s->value, sizeof(s->value), "%.8s", text + begin + 3);
        return finish(in, s, begin + 11, TTF_SEM_SYMBOL, "ja.postal", "@postal");
    }
    /* 打率.325 さんわりにぶごりん, .300バッター さんわり */
    if (text[begin] == '.' && begin + 4 <= n && digit(text[begin + 1]) && digit(text[begin + 2]) &&
        digit(text[begin + 3]) && text[begin + 1] != '0' && (begin + 4 >= n || !digit(text[begin + 4])) &&
        !(begin && digit(text[begin - 1])) && cue_near(e, "rate_cue", text, n, begin, begin + 4, 12)) {
        size_t vl = (size_t)snprintf(s->value, sizeof(s->value), "%c割", text[begin + 1]);
        if (text[begin + 2] != '0')
            vl += (size_t)snprintf(s->value + vl, sizeof(s->value) - vl, "%c分", text[begin + 2]);
        if (text[begin + 3] != '0')
            snprintf(s->value + vl, sizeof(s->value) - vl, "%c厘", text[begin + 3]);
        return finish(in, s, begin + 4, TTF_SEM_COUNTER, "ja.rate", "@ja");
    }
    /* 家賃月8.5万円 やちんつき…, 月2回 つきにかい: 月 at the end of a kanji word, right before
       an amount or a frequency, is つき (2026-10-11); 今月 毎月 … are words (month_word). */
    if (t->script == TTF_SCRIPT_HAN && t->end - begin >= 3 && t->end < n && digit(text[t->end]) &&
        !memcmp(text + t->end - 3, "\xe6\x9c\x88", 3) && t->end - begin < sizeof(s->value)) {
        char prev[4] = {0}, key2[32], nb[64];
        size_t nl2 = 0, used, cw = 0;
        ja_number_t x;
        if (t->end - 3 > begin)
            memcpy(prev, text + t->end - 6, 3);
        used = parse_number(e, text + t->end, n - t->end, &x, nb, sizeof(nb), &nl2);
        if (used)
            cw = counter_at(e, text + t->end + used, n - t->end - used, key2, sizeof(key2));
        if (!(prev[0] && res(e, "month_word", prev)) && cw && res(e, "month_rate_counter", key2)) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(t->end - begin), text + begin);
            return finish(in, s, t->end, TTF_SEM_SYMBOL, "ja.month-rate", "@month-rate");
        }
    }
    /* 小6 しょうろく, 中2 ちゅうに: a school grade before one digit (not 中2階). */
    if ((t->script == TTF_SCRIPT_HAN || t->script == TTF_SCRIPT_KANA) && t->end - begin >= 3 &&
        t->end + 1 <= n && digit(text[t->end]) &&
        !(t->end + 1 < n && (digit(text[t->end + 1]) || text[t->end + 1] == '.' || alpha(text[t->end + 1])))) {
        char g[4], key2[32];
        uint32_t before = 0;
        size_t bw;
        memcpy(g, text + t->end - 3, 3);
        g[3] = 0;
        if (t->end - 3 > begin) {
            size_t k = t->end - 3;
            do
                --k;
            while (k > begin && ((unsigned char)text[k] & 0xc0) == 0x80);
            if (ttf_utf8_decode(text + k, t->end - 3 - k, &before, &bw) != 1)
                before = 0;
        }
        if (res(e, "grade_prefix", g) && ttf_script_of(before) != TTF_SCRIPT_HAN &&
            !counter_at(e, text + t->end + 1, n - t->end - 1, key2, sizeof(key2)) &&
            t->end + 1 - begin < sizeof(s->value)) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(t->end + 1 - begin), text + begin);
            return finish(in, s, t->end + 1, TTF_SEM_SYMBOL, "ja.grade", "@grade");
        }
    }
    if (digit(text[begin]) && !(begin && (digit(text[begin - 1]) || alpha(text[begin - 1]) ||
                                          text[begin - 1] == '-' || text[begin - 1] == '.'))) {
        size_t q = begin, r, parts = 1;
        char key2[32];
        const char *reading;
        while (q < n && digit(text[q]))
            ++q;
        /* 1:23:45のタイム いちじかんにじゅうさんぷんよんじゅうごびょう: a duration. */
        if (q - begin <= 2 && q + 6 <= n && text[q] == ':' && digit(text[q + 1]) && digit(text[q + 2]) &&
            text[q + 3] == ':' && digit(text[q + 4]) && digit(text[q + 5]) &&
            (q + 6 >= n || !digit(text[q + 6]))) {
            size_t k, cue = 0;
            for (k = 3; k <= 15 && q + 6 + k <= n && !cue; k += 3) {
                char w[16];
                memcpy(w, text + q + 6, k);
                w[k] = 0;
                cue = res(e, "duration_cue", w) != NULL;
            }
            for (k = 6; k <= 15 && k <= begin && !cue; k += 3) {
                char w[16];
                memcpy(w, text + begin - k, k);
                w[k] = 0;
                cue = res(e, "duration_cue", w) != NULL;
            }
            if (cue) {
                unsigned long h = strtoul(text + begin, NULL, 10), m = strtoul(text + q + 1, NULL, 10),
                              sec = strtoul(text + q + 4, NULL, 10);
                size_t vl = 0;
                if (h)
                    vl += (size_t)snprintf(s->value + vl, sizeof(s->value) - vl, "%lu時間", h);
                if (m)
                    vl += (size_t)snprintf(s->value + vl, sizeof(s->value) - vl, "%lu分", m);
                if (sec || !vl)
                    snprintf(s->value + vl, sizeof(s->value) - vl, "%lu秒", sec);
                return finish(in, s, q + 6, TTF_SEM_COUNTER, "ja.duration", "@ja");
            }
        }
        /* 12 345件 いちまんにせん…: digits grouped by spaces before a counter. */
        if (q - begin <= 3 && q + 4 <= n && text[q] == ' ' && digit(text[q + 1])) {
            size_t z = q, nd = q - begin;
            char joined[24];
            memcpy(joined, text + begin, q - begin);
            while (z + 4 <= n && text[z] == ' ' && digit(text[z + 1]) && digit(text[z + 2]) && digit(text[z + 3]) &&
                   (z + 4 >= n || !digit(text[z + 4])) && nd + 3 < sizeof(joined)) {
                memcpy(joined + nd, text + z + 1, 3);
                nd += 3;
                z += 4;
            }
            joined[nd] = 0;
            if (z > q && counter_at(e, text + z, n - z, key2, sizeof(key2))) {
                size_t cw2 = counter_at(e, text + z, n - z, key2, sizeof(key2));
                if (nd + cw2 < sizeof(s->value)) {
                    snprintf(s->value, sizeof(s->value), "%s%.*s", joined, (int)cw2, text + z);
                    return finish(in, s, z + cw2, TTF_SEM_COUNTER, "ja.space-grouped", "@ja");
                }
            }
        }
        /* ベスト4 ベストフォー */
        if (begin >= 9 && !memcmp(text + begin - 9, "ベスト", 9) && q - begin <= 2 &&
            (q >= n || !digit(text[q]))) {
            char d[4];
            snprintf(d, sizeof(d), "%.*s", (int)(q - begin), text + begin);
            if ((reading = res(e, "english_number", d))) {
                snprintf(s->value, sizeof(s->value), "%s", reading);
                return finish(in, s, q, TTF_SEM_ABBREVIATION, "ja.english-number", "");
            }
        }
        /* 2アウト ツーアウト */
        if (q - begin <= 2) {
            size_t k, best = 0;
            for (k = q + 3; k <= n && k <= q + 18; k += 3) {
                char w[24];
                memcpy(w, text + q, k - q);
                w[k - q] = 0;
                if (res(e, "english_counter", w))
                    best = k;
            }
            if (best) {
                char d[4];
                snprintf(d, sizeof(d), "%.*s", (int)(q - begin), text + begin);
                if ((reading = res(e, "english_number", d)) &&
                    strlen(reading) + best - q < sizeof(s->value)) {
                    snprintf(s->value, sizeof(s->value), "%s%.*s", reading, (int)(best - q), text + q);
                    return finish(in, s, best, TTF_SEM_ABBREVIATION, "ja.english-number", "");
                }
            }
        }
        /* 1日から3日まで ついたち: the first day of a day range. */
        if (q - begin == 1 && text[begin] == '1' && q + 3 <= n && !memcmp(text + q, "日", 3)) {
            size_t a = q + 3, w;
            if (a + 6 <= n && !memcmp(text + a, "から", 6))
                a += 6;
            else if (range_mark(text + a, n - a, &w) && text[a] != '-')
                a += w;
            else
                a = 0;
            if (!a && (day_one_after(e, text + q + 3, n - q - 3) ||
                       /* 毎月1日, 今月1日: the first of a month (owner v2, 2026-10-15) */
                       (begin >= 6 && (!memcmp(text + begin - 6, "毎月", 6) || !memcmp(text + begin - 6, "今月", 6) ||
                                       !memcmp(text + begin - 6, "来月", 6) || !memcmp(text + begin - 6, "先月", 6) ||
                                       !memcmp(text + begin - 6, "翌月", 6)))) &&
                (reading = res(e, "counter_reading", "date_day:1"))) {
                snprintf(s->value, sizeof(s->value), "%s", reading);
                return finish(in, s, q + 3, TTF_SEM_ABBREVIATION, "ja.day-one", "");
            }
            if (a && a < n && digit(text[a])) {
                while (a < n && digit(text[a]))
                    ++a;
                if (a + 3 <= n && !memcmp(text + a, "日", 3) && (reading = res(e, "counter_reading", "date_day:1"))) {
                    snprintf(s->value, sizeof(s->value), "%s", reading);
                    return finish(in, s, q + 3, TTF_SEM_ABBREVIATION, "ja.day-one", "");
                }
            }
        }
        /* 2F にかい, 1F〜3F */
        if (q - begin <= 3 && q < n && text[q] == 'F' &&
            (q + 1 >= n || !(alpha(text[q + 1]) || digit(text[q + 1])))) {
            snprintf(s->value, sizeof(s->value), "%.*s階", (int)(q - begin), text + begin);
            return finish(in, s, q + 1, TTF_SEM_COUNTER, "ja.floor", "@ja");
        }
        /* 03（1234）5678 */
        if (text[begin] == '0' && q - begin >= 2 && q - begin <= 4) {
            size_t open = q < n && text[q] == '(' ? 1 : (q + 3 <= n && !memcmp(text + q, "（", 3)) ? 3 : 0;
            if (open) {
                size_t m = q + open, m2, close;
                while (m < n && digit(text[m]) && m - q - open < 4)
                    ++m;
                close = m < n && text[m] == ')' ? 1 : (m + 3 <= n && !memcmp(text + m, "）", 3)) ? 3 : 0;
                m2 = m + close;
                while (m2 < n && digit(text[m2]))
                    ++m2;
                if (m > q + open && close && m2 - m - close >= 3 && m2 - m - close <= 4 &&
                    (m2 >= n || !digit(text[m2]))) {
                    snprintf(s->value, sizeof(s->value), "%.*s-%.*s-%.*s", (int)(q - begin), text + begin,
                             (int)(m - q - open), text + q + open, (int)(m2 - m - close), text + m + close);
                    return finish(in, s, m2, TTF_SEM_PHONE, "ja.phone-paren", "");
                }
            }
        }
        /* 3-12-5 さんのじゅうにのご: a block address. */
        r = q;
        if (text[begin] != '0' && q - begin <= 2) {
            while (r + 1 < n && text[r] == '-' && digit(text[r + 1]) && parts < 4) {
                size_t z = r + 1;
                while (z < n && digit(text[z]))
                    ++z;
                if (z - r - 1 > 3)
                    break;
                r = z;
                ++parts;
            }
            if (parts == 3 && (r >= n || !(digit(text[r]) || alpha(text[r]) || text[r] == '-' || text[r] == '.')) &&
                !counter_at(e, text + r, n - r, key2, sizeof(key2)) && r - begin < sizeof(s->value)) {
                snprintf(s->value, sizeof(s->value), "%.*s", (int)(r - begin), text + begin);
                return finish(in, s, r, TTF_SEM_SYMBOL, "ja.address", "@ja-address");
            }
        }
    }
    /* 1st ファースト, 10th テンス (latin_ordinal). */
    if (digit(text[begin])) {
        size_t q = begin;
        char key[8];
        const char *reading;
        while (q < n && digit(text[q]))
            ++q;
        if (q - begin < sizeof(key) && q + 2 <= n &&
            (!strncmp(text + q, "st", 2) || !strncmp(text + q, "nd", 2) ||
             !strncmp(text + q, "rd", 2) || !strncmp(text + q, "th", 2)) &&
            (q + 2 >= n || !(alpha(text[q + 2]) || digit(text[q + 2])))) {
            memcpy(key, text + begin, q - begin);
            key[q - begin] = 0;
            if ((reading = res(e, "latin_ordinal", key)) && strlen(reading) < sizeof(s->value)) {
                snprintf(s->value, sizeof(s->value), "%s", reading);
                return finish(in, s, q + 2, TTF_SEM_ABBREVIATION, "ja.latin-ordinal", "");
            }
        }
    }
    if (t->script == TTF_SCRIPT_LATIN && t->end - begin <= 3 && t->end + 1 < n &&
        text[t->end] == '&' && text[t->end + 1] >= 'A' && text[t->end + 1] <= 'Z' &&
        (!begin || !alpha(text[begin - 1]))) {
        size_t q = t->end + 1, k;
        int caps = 1;
        for (k = begin; k < t->end; ++k)
            caps &= text[k] >= 'A' && text[k] <= 'Z';
        while (q < n && text[q] >= 'A' && text[q] <= 'Z' && q - t->end <= 3)
            ++q;
        if (caps && q - t->end - 1 <= 3 && (q >= n || !alpha(text[q]))) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - begin), text + begin);
            return finish(in, s, q, TTF_SEM_INITIALISM, "ja.initialism", "");
        }
    }
    if (t->script == TTF_SCRIPT_LATIN && initialism(text + begin, t->end - begin) &&
        (t->end >= n || !((text[t->end] >= 'a' && text[t->end] <= 'z') || digit(text[t->end])))) {
        snprintf(s->value, sizeof(s->value), "%.*s", (int)(t->end - begin), text + begin);
        return finish(in, s, t->end, TTF_SEM_INITIALISM, "ja.initialism", "");
    }
    /* A4, MP3, mp3, B2B: letter groups of up to three with digit groups of up
       to four read as letter names and numbers; COVID19 keeps its word. */
    if (t->script == TTF_SCRIPT_LATIN &&
        (!begin || !(alpha(text[begin - 1]) || digit(text[begin - 1])))) {
        size_t q = begin, run = 0, letters = 0, digits = 0;
        int ok = 1, was_digit = -1;
        while (q < n && (alpha(text[q]) || digit(text[q])) && q - begin <= 10) {
            int is_digit = digit(text[q]);
            run = is_digit == was_digit ? run + 1 : 1;
            was_digit = is_digit;
            if ((is_digit && run > 4) || (!is_digit && run > 3))
                ok = 0;
            letters += !is_digit;
            digits += is_digit;
            ++q;
        }
        /* PM2.5, F2.8: one decimal part after the digits (2026-10-08). */
        if (ok && letters && was_digit == 1 && q + 1 < n && text[q] == '.' && digit(text[q + 1])) {
            ++q;
            while (q < n && digit(text[q]))
                ++q;
        }
        if (ok && letters && digits && q - begin <= 14 &&
            (q >= n || (text[q] != '.' && text[q] != ','))) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - begin), text + begin);
            return finish(in, s, q, TTF_SEM_ALNUM_TERM, "ja.alnum", "");
        }
    }
    if (!digit(text[begin]) &&
        !((text[begin] == '-' || text[begin] == '+') && begin + 1 < n && digit(text[begin + 1])))
        return 0;
    /* 1Gbps, 100Mbps: a Latin unit that only the Japanese table lists (latin_unit; bit rates,
       2026-10-11). Units every language reads (GB, km) come from numeric_suffix. */
    if (digit(text[begin]) && !(begin && (digit(text[begin - 1]) || alpha(text[begin - 1])))) {
        size_t q = begin, r;
        char unit[16];
        while (q < n && (digit(text[q]) || (text[q] == '.' && q + 1 < n && digit(text[q + 1]))))
            ++q;
        r = q;
        /* 142mg/dL, 1.2N·m: a compound unit with / or a middle dot (2026-10-14) */
        while (r < n && (alpha(text[r]) || (r > q && text[r] == '/' && r + 1 < n && alpha(text[r + 1])) ||
                         (r > q && r + 2 < n && (unsigned char)text[r] == 0xc2 && (unsigned char)text[r + 1] == 0xb7 &&
                          alpha(text[r + 2]))))
            r += (unsigned char)text[r] == 0xc2 ? 2 : 1;
        if (r > q && r - q < sizeof(unit) && q - begin < sizeof(s->value) && !(r < n && digit(text[r])) &&
            (!memchr(text + q, '/', r - q) && !memchr(text + q, 0xc2, r - q) ? 1 : (memcpy(unit, text + q, r - q), unit[r - q] = 0, res(e, "latin_unit", unit) != NULL))) {
            memcpy(unit, text + q, r - q);
            unit[r - q] = 0;
            if (res(e, "latin_unit", unit) && res(e, "unit", unit)) {
                snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - begin), text + begin);
                return finish(in, s, r, TTF_SEM_MEASURE, "ja.latin-unit", unit);
            }
        }
    }
    /* 6m95 ろくメートルきゅうじゅうご, 73m09: a field-event mark, metres and centimetres
       (2026-11-10, real-text cycle R10; the digits were left as written) */
    if (digit(text[begin]) && !(begin && (digit(text[begin - 1]) || alpha(text[begin - 1]) ||
                                          text[begin - 1] == '.'))) {
        size_t q = begin;
        while (q < n && digit(text[q]) && q - begin < 3)
            ++q;
        if (q - begin <= 2 && q + 3 <= n && text[q] == 'm' && digit(text[q + 1]) && digit(text[q + 2]) &&
            !(q + 3 < n && (digit(text[q + 3]) || alpha(text[q + 3]) || text[q + 3] == '.'))) {
            snprintf(s->value, sizeof(s->value), "%.*s|%.2s", (int)(q - begin), text + begin, text + q + 1);
            return finish(in, s, q + 3, TTF_SEM_SYMBOL, "ja.field-mark", "@field-mark");
        }
    }
    /* 136hp ひゃくさんじゅうろくhp, 26.5kgm, 50kt: a number before a Latin unit with a small
       letter that no table lists reads the number and keeps the letters as written, like
       136 hp (2026-11-09, real-text cycle R4; the digits were left as written). Capital codes
       (4K, 5G, 2R) keep their rules. */
    if (digit(text[begin]) && !(begin && (digit(text[begin - 1]) || alpha(text[begin - 1])))) {
        size_t q = begin, r;
        int small = 0;
        while (q < n && (digit(text[q]) || (text[q] == '.' && q + 1 < n && digit(text[q + 1]))))
            ++q;
        for (r = q; r < n && alpha(text[r]) && r - q < 5; ++r)
            small |= text[r] >= 'a' && text[r] <= 'z';
        char letters[8] = {0};
        if (r > q && r - q < sizeof(letters))
            memcpy(letters, text + q, r - q);
        /* a unit or counter some table reads (100km, 5mg, 900t) keeps its reading */
        if (small && r > q && r - q <= 4 && q - begin <= 9 && q - begin < sizeof(s->value) &&
            !ttf_resource_get(e, "numeric_suffix", "*", letters, NULL) && !res(e, "unit", letters) &&
            !res(e, "latin_unit", letters) && !res(e, "counter", letters) &&
            !(r < n && (alpha(text[r]) || digit(text[r]) || text[r] == '-' || text[r] == '.' ||
                        text[r] == '/' || text[r] == '+'))) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - begin), text + begin);
            return finish(in, s, q, TTF_SEM_CARDINAL, "ja.number-before-latin", "");
        }
    }
    /* 7G3K2 ななじーさんけーに: digits and capitals that alternate three times or more are a
       code (owner v2, 2026-10-15) */
    if (digit(text[begin]) && !(begin && (alpha(text[begin - 1]) || digit(text[begin - 1])))) {
        size_t q = begin, turns = 0;
        while (q < n && (digit(text[q]) || (text[q] >= 'A' && text[q] <= 'Z')) && q - begin < 16) {
            if (q > begin && !digit(text[q]) != !digit(text[q - 1]))
                ++turns;
            ++q;
        }
        if (turns >= 3 && !(q < n && (alpha(text[q]) || digit(text[q]) || text[q] == '.')) &&
            q - begin < sizeof(s->value)) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - begin), text + begin);
            return finish(in, s, q, TTF_SEM_ALNUM_TERM, "ja.code", "code");
        }
    }
    /* 3LDK, 4K, 5G: a short number with capital letters (2026-10-08). */
    if (digit(text[begin])) {
        size_t q = begin, r;
        while (q < n && digit(text[q]))
            ++q;
        r = q;
        while (r < n && text[r] >= 'A' && text[r] <= 'Z')
            ++r;
        char unit[8] = "";
        if (r > q && r - q < sizeof(unit))
            memcpy(unit, text + q, r - q), unit[r - q] = 0;
        /* 100KB, 3L stay measures (a unit row). */
        if (q - begin <= 4 && r > q && r - q <= 4 &&
            (r >= n || !(alpha(text[r]) || digit(text[r]))) && r - begin < sizeof(s->value) &&
            !res(e, "unit", unit)) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(r - begin), text + begin);
            /* 第2G だいにジー: after 第 the digits stay Japanese. */
            if (begin >= 3 && !memcmp(text + begin - 3, "\xe7\xac\xac", 3))
                snprintf(s->secondary, sizeof(s->secondary), "after-dai");
            return finish(in, s, r, TTF_SEM_ALNUM_TERM, "ja.alnum", "");
        }
    }
    /* 3/15に提出 さんがつじゅうごにち, 1/20の確率 にじゅうぶんのいち (2026-10-09): M/D is a
       fraction with a counter or a fraction cue after it, and a date with a
       weekday mark, a date range or a date cue; otherwise the core decides. */
    if (digit(text[begin]) && !(begin && (digit(text[begin - 1]) || text[begin - 1] == '/' ||
                                          text[begin - 1] == '.' || alpha(text[begin - 1])))) {
        size_t q = begin, r, cw;
        char key[32];
        while (q < n && digit(text[q]))
            ++q;
        /* 2026/12 にせんにじゅうろくねんじゅうにがつ: a year and a month (owner v2, 2026-10-15) */
        if (q - begin == 4 && (!memcmp(text + begin, "19", 2) || !memcmp(text + begin, "20", 2)) && q + 1 < n &&
            text[q] == '/' && digit(text[q + 1])) {
            size_t r2 = q + 1;
            unsigned long m2;
            while (r2 < n && digit(text[r2]))
                ++r2;
            m2 = strtoul(text + q + 1, NULL, 10);
            if (r2 - q - 1 <= 2 && m2 >= 1 && m2 <= 12 &&
                !(r2 < n && (digit(text[r2]) || text[r2] == '/' || text[r2] == '.' || alpha(text[r2])))) {
                snprintf(s->value, sizeof(s->value), "%.4s\xe5\xb9\xb4%lu\xe6\x9c\x88", text + begin, m2);
                return finish(in, s, r2, TTF_SEM_COUNTER, "ja.year-month", "@ja");
            }
        }
        /* 〒100-0005 いちぜろぜろのぜろぜろぜろご: a postal code after the postal mark */
        if (q - begin == 3 && begin >= 3 && q + 5 <= n && text[q] == '-' &&
            (!memcmp(text + begin - 3, "\xe3\x80\x92", 3) ||
             (begin >= 4 && text[begin - 1] == ' ' && !memcmp(text + begin - 4, "\xe3\x80\x92", 3))) &&
            digit(text[q + 1]) && digit(text[q + 2]) && digit(text[q + 3]) && digit(text[q + 4]) &&
            !(q + 5 < n && digit(text[q + 5]))) {
            snprintf(s->value, sizeof(s->value), "%.8s", text + begin);
            return finish(in, s, q + 5, TTF_SEM_ALNUM_TERM, "ja.code", "code");
        }
        if (q - begin <= 2 && q + 1 < n && text[q] == '/' && digit(text[q + 1])) {
            unsigned long m = strtoul(text + begin, NULL, 10), d;
            r = q + 1;
            while (r < n && digit(text[r]))
                ++r;
            d = strtoul(text + q + 1, NULL, 10);
            if (r - q - 1 <= 3 && (r >= n || !(digit(text[r]) || text[r] == '/' || text[r] == '.' ||
                                              alpha(text[r])))) {
                static const unsigned days[13] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                int date = m >= 1 && m <= 12 && d >= 1 && d <= days[m] && r - q - 1 <= 2;
                size_t open = r < n && text[r] == '(' ? 1
                              : (r + 3 <= n && !memcmp(text + r, "\xef\xbc\x88", 3)) ? 3 : 0;
                size_t close = 0, w;
                char day[4] = "";
                int weekday = 0, range = 0;
                int holiday = 0;
                if (open && r + open + 3 < n) {
                    size_t z = r + open + 3;
                    /* 11/3(月・祝): a holiday mark after the weekday (owner v2, 2026-10-15) */
                    if (z + 6 <= n && !memcmp(text + z, "\xe3\x83\xbb\xe7\xa5\x9d", 6))
                        holiday = 6, z += 6;
                    close = z < n && text[z] == ')' ? 1
                            : (z + 3 <= n && !memcmp(text + z, "\xef\xbc\x89", 3)) ? 3 : 0;
                    memcpy(day, text + r + open, 3);
                    day[3] = 0;
                    weekday = close && res(e, "weekday_mark", day);
                }
                /* 5/3〜5/5: a slash date next to another one across a range mark. */
                if (range_mark(text + r, n - r, &w) && text[r] != '-' && r + w + 2 < n &&
                    digit(text[r + w]) && (text[r + w + 1] == '/' || (digit(text[r + w + 1]) &&
                                                                       text[r + w + 2] == '/')))
                    range = 1;
                {
                    size_t mark = SIZE_MAX;
                    if (begin >= 3 && (!memcmp(text + begin - 3, "\xe3\x80\x9c", 3) ||
                                       !memcmp(text + begin - 3, "\xef\xbd\x9e", 3)))
                        mark = begin - 3;
                    else if (begin >= 1 && text[begin - 1] == '~')
                        mark = begin - 1;
                    if (mark != SIZE_MAX && mark >= 3 && digit(text[mark - 1]) &&
                        (text[mark - 2] == '/' || (digit(text[mark - 2]) && text[mark - 3] == '/')))
                        range = 1;
                }
                cw = counter_at(e, text + r, n - r, key, sizeof(key));
                if (cw && strcmp(key, "日") && cw < 16) {
                    char unit[sizeof(s->unit)];
                    snprintf(unit, sizeof(unit), "counter:%.15s", key);
                    snprintf(s->value, sizeof(s->value), "%lu", m);
                    snprintf(s->secondary, sizeof(s->secondary), "%lu", d);
                    return finish(in, s, r + cw, TTF_SEM_FRACTION, "ja.slash-fraction", unit);
                }
                if (cue_near(e, "fraction_cue", text, n, begin, r, 12)) {
                    snprintf(s->value, sizeof(s->value), "%lu", m);
                    snprintf(s->secondary, sizeof(s->secondary), "%lu", d);
                    return finish(in, s, r, TTF_SEM_FRACTION, "ja.slash-fraction", "");
                }
                /* 4/1入社 しがつついたち: nothing is a fraction over one (owner v2, 2026-10-15) */
                /* 11/3に栗ご飯 じゅういちがつみっか: over one before に, から or まで (round 33); not
                   before になる or a number after から (7/3から1/3を引く) */
                if (date && (weekday || range || d == 1 || cue_near(e, "date_cue", text, n, begin, r, 18) ||
                             (m > d && ((ja_next_is(text, n, r, "\xe3\x81\xab") &&
                                         !ja_next_is(text, n, r, "\xe3\x81\xab\xe3\x81\xaa")) ||
                                        (ja_next_is(text, n, r, "\xe3\x81\x8b\xe3\x82\x89") &&
                                         !(r + 6 < n && digit(text[r + 6]))) ||
                                        ja_next_is(text, n, r, "\xe3\x81\xbe\xe3\x81\xa7"))))) {
                    snprintf(s->value, sizeof(s->value), "%lu\xe6\x9c\x88%lu\xe6\x97\xa5", m, d);
                    if (weekday && !holiday) {
                        size_t vl = strlen(s->value);
                        snprintf(s->value + vl, sizeof(s->value) - vl, "(%s)", day);
                        r += open + 3 + close;
                    }
                    return finish(in, s, r, TTF_SEM_COUNTER, "ja.slash-date", "@ja");
                }
            }
        }
    }
    /* 3:1の割合 さんたいいち, 3-2で勝利 さんたいに (2026-10-09): N:M or N-M with a
       cue nearby, or N:M whose second part has one digit (not minutes). */
    if (digit(text[begin]) && !(begin && (digit(text[begin - 1]) || alpha(text[begin - 1]) ||
                                          text[begin - 1] == '-' || text[begin - 1] == ':' ||
                                          text[begin - 1] == '.'))) {
        size_t q = begin, r;
        char key[32];
        while (q < n && digit(text[q]))
            ++q;
        if (q - begin <= 3 && q + 1 < n && (text[q] == ':' || text[q] == '-') && digit(text[q + 1])) {
            r = q + 1;
            while (r < n && digit(text[r]))
                ++r;
            if (r - q - 1 <= 3 &&
                (r >= n || !(digit(text[r]) || alpha(text[r]) || text[r] == ':' || text[r] == '-' ||
                             text[r] == '.' || text[r] == '/')) &&
                !counter_at(e, text + r, n - r, key, sizeof(key)) && r - begin < sizeof(s->value)) {
                int clock = text[q] == ':' && r - q - 1 == 2 &&
                            (!strncmp(text + r, "に", 3) || !strncmp(text + r, "から", 6) ||
                             !strncmp(text + r, "まで", 6) || !strncmp(text + r, "頃", 3) ||
                             !strncmp(text + r, "ごろ", 6) || !strncmp(text + r, "開始", 6) ||
                             !strncmp(text + r, "終了", 6) || !strncmp(text + r, "キックオフ", 15));
                int score = !clock && cue_near(e, "score_cue", text, n, begin, r, 40);
                int ratio = text[q] == ':' &&
                            (r - q - 1 == 1 || cue_near(e, "ratio_cue", text, n, begin, r, 15));
                if (score || ratio) {
                    snprintf(s->value, sizeof(s->value), "%.*s", (int)(r - begin), text + begin);
                    return finish(in, s, r, TTF_SEM_SYMBOL, "ja.score", "@score");
                }
            }
        }
    }
    /* 10² じゅうのにじょう, 5³ ごのさんじょう */
    if (digit(text[begin])) {
        size_t q = begin;
        while (q < n && digit(text[q]))
            ++q;
        if (q - begin <= 6 && q + 2 <= n && (unsigned char)text[q] == 0xc2 &&
            ((unsigned char)text[q + 1] == 0xb2 || (unsigned char)text[q + 1] == 0xb3) &&
            q - begin + 2 < sizeof(s->value)) {
            snprintf(s->value, sizeof(s->value), "%.*s%c", (int)(q - begin), text + begin,
                     (unsigned char)text[q + 1] == 0xb2 ? '2' : '3');
            return finish(in, s, q + 2, TTF_SEM_SYMBOL, "ja.power", "@power");
        }
    }
    /* 内線214, 暗証番号は1234, ISBN978-4-…: after a number label (a particle or a space
       may come between) the digits read one by one; hyphen groups keep の. */
    if (digit(text[begin])) {
        size_t k, q = begin, cb = begin, groups = 1;
        char key2[32];
        int gap = 0;
        while (q < n && digit(text[q]))
            ++q;
        while (q + 1 < n && text[q] == '-' && digit(text[q + 1])) {
            ++q;
            while (q < n && digit(text[q]))
                ++q;
            ++groups;
        }
        if (cb >= 3 && (!memcmp(text + cb - 3, "は", 3) || !memcmp(text + cb - 3, "が", 3) ||
                        !memcmp(text + cb - 3, "を", 3) || !memcmp(text + cb - 3, "：", 3)))
            cb -= 3, gap = 1;
        else if (cb >= 3 && !memcmp(text + cb - 3, "「", 3) && groups == 1 && q + 3 <= n &&
                 !memcmp(text + q, "」", 3))
            cb -= 3; /* 下4桁「4471」, ホットライン「188」 (owner sets, 2026-10-14) */
        else if (cb >= 1 && (text[cb - 1] == ' ' || text[cb - 1] == ':'))
            cb -= 1, gap = 1;
        for (k = 2; k <= 21 && k <= cb; ++k) {
            char cue[24];
            if (((unsigned char)text[cb - k] & 0xc0) == 0x80)
                continue;
            memcpy(cue, text + cb - k, k);
            cue[k] = 0;
            if (q - begin >= 2 && q - begin < sizeof(s->value) && res(e, "digit_cue", cue) &&
                (q >= n || !(text[q] == '.' || text[q] == ',')) &&
                !(gap && (q - begin < 4 || counter_at(e, text + q, n - q, key2, sizeof(key2)))) &&
                /* 受付番号15番 じゅうごばん (ja dev v1, 2026-10-17) */
                !(q - begin <= 3 && q + 3 <= n && !memcmp(text + q, "番", 3) && !(q + 6 <= n && !memcmp(text + q + 3, "号", 3)) &&
                  k >= 6 && !memcmp(cue + k - 6, "番号", 6))) {
                snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - begin), text + begin);
                return finish(in, s, q, groups > 1 ? TTF_SEM_PHONE : TTF_SEM_DIGIT_SEQUENCE,
                              "ja.digit-cue", "");
            }
        }
        /* 117で時報, 177の天気予報: a service number before its cue word. */
        if (groups == 1 && q - begin >= 3 && q - begin <= 4 && text[begin] == '1' &&
            (q >= n || !(text[q] == '.' || text[q] == ',')) &&
            !counter_at(e, text + q, n - q, key2, sizeof(key2)) &&
            cue_near(e, "digit_after_cue", text, n, q, q, 15)) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - begin), text + begin);
            return finish(in, s, q, TTF_SEM_DIGIT_SEQUENCE, "ja.digit-cue", "");
        }
    }
    if (begin && (digit(text[begin - 1]) || text[begin - 1] == '.' || text[begin - 1] == ',' ||
                  (text[begin - 1] >= 'A' && text[begin - 1] <= 'Z') ||
                  (text[begin - 1] >= 'a' && text[begin - 1] <= 'z')))
        return 0;
    p = begin;
    for (;;) {
        ja_number_t x;
        char key[32];
        size_t used, cw, rw, save = nl;
        if (pairs && p < n && text[p] == '-')
            break;
        used = parse_number(e, text + p, n - p, &x, norm, sizeof(norm), &nl);
        if (!used)
            break;
        cw = counter_at(e, text + p + used, n - p - used, key, sizeof(key));
        if (!cw && !pairs && range_mark(text + p + used, n - p - used, &rw)) {
            /* 3〜5個: the range needs a counter after its second number. */
            ja_number_t y;
            size_t used2, nl2 = nl;
            if (nl2 + rw + 1 >= sizeof(norm))
                break;
            memcpy(norm + nl2, text + p + used, rw);
            nl2 += rw;
            norm[nl2] = 0;
            used2 = parse_number(e, text + p + used + rw, n - p - used - rw, &y, norm, sizeof(norm),
                                 &nl2);
            if (!used2 || y.negative)
                break;
            cw = counter_at(e, text + p + used + rw + used2, n - p - used - rw - used2, key,
                            sizeof(key));
            if (!cw)
                break;
            used += rw + used2;
            nl = nl2;
        } else if (!cw && !x.scale_word && pairs == 0) {
            /* 1万: a written scale alone is still a Japanese number, and so is a
               number grouped with a Japanese comma (1、000). */
            const char *q = text + p;
            size_t k, mark = 0;
            for (k = 0; k + 3 <= used; ++k)
                if (!memcmp(q + k, "\xe3\x80\x81", 3))
                    mark = 1;
            for (k = 0; k + 3 <= used; ++k) {
                size_t w;
                unsigned ex;
                if (scale_mark(e, q + k, used - k, &w, &ex))
                    mark = 1;
            }
            if (!mark) {
                nl = save;
                break;
            }
        } else if (!cw && !x.scale_word) {
            nl = save;
            break;
        }
        if (nl + cw + 1 >= sizeof(norm)) {
            nl = save;
            break;
        }
        memcpy(norm + nl, text + p + used, cw);
        nl += cw;
        norm[nl] = 0;
        p += used + cw;
        ++pairs;
        if (!cw)
            break;
        if (p + 3 <= n && nl + 4 < sizeof(norm) &&
            ((!memcmp(text + p, "\xe7\x9b\xae", 3) && strcmp(key, "分") && strcmp(key, "丁目") && strcmp(key, "代目")) ||
             (!memcmp(text + p, "\xe4\xb8\xad", 3) && res(e, "long_suffix", key)) ||
             (!memcmp(text + p, "\xe5\x8d\x8a", 3) && res(e, "half_suffix", key)) ||
             (!memcmp(text + p, "\xe5\xbe\x8c", 3) && res(e, "after_suffix", key) && !half_word(text + p, n - p)) ||
             (!memcmp(text + p, "\xe5\x89\x8d", 3) && res(e, "before_suffix", key) && !half_word(text + p, n - p)) ||
             (!memcmp(text + p, "\xe9\xa0\x83", 3) && res(e, "around_suffix", key)) ||
             (!memcmp(text + p, "\xe4\xb8\xad", 3) && p + 3 < n && digit(text[p + 3])) ||
             (!memcmp(text + p, "\xe7\x94\x9f", 3) && res(e, "student_suffix", key) && student_end(text + p + 3, n - p - 3)))) {
            memcpy(norm + nl, text + p, 3);
            nl += 3;
            norm[nl] = 0;
            p += 3;
            /* 8時半頃 はちじはんごろ */
            if (!memcmp(text + p - 3, "\xe5\x8d\x8a", 3) && p + 3 <= n && nl + 4 < sizeof(norm) &&
                !memcmp(text + p, "\xe9\xa0\x83", 3) && res(e, "around_suffix", key)) {
                memcpy(norm + nl, text + p, 3);
                nl += 3;
                norm[nl] = 0;
                p += 3;
            }
            break;
        }
        /* 3分の1 さんぶんのいち: a written fraction, not minutes. */
        if (!strcmp(key, "分") && p + 4 <= n && !memcmp(text + p, NO, 3) && digit(text[p + 3])) {
            ja_number_t y;
            size_t nl2 = nl + 3, used2;
            if (nl2 + 1 >= sizeof(norm))
                break;
            memcpy(norm + nl, NO, 3);
            norm[nl2] = 0;
            used2 = parse_number(e, text + p + 3, n - p - 3, &y, norm, sizeof(norm), &nl2);
            if (used2 && !y.negative && !y.scale_word) {
                nl = nl2;
                p += 3 + used2;
            } else
                norm[nl] = 0;
            break;
        }
    }
    if (!pairs)
        return 0;
    norm[nl] = 0;
    /* 12月25日（水）, 10月8日(火): a weekday mark after a day reads ようび. */
    if (nl >= 3 && !strcmp(norm + nl - 3, "\xe6\x97\xa5")) {
        size_t open = p < n && text[p] == '('                                ? 1
                      : (p + 3 <= n && !memcmp(text + p, "\xef\xbc\x88", 3)) ? 3
                                                                             : 0;
        char day[4];
        if (open && p + open + 3 < n) {
            size_t q = p + open + 3;
            size_t close = q < n && text[q] == ')'                                ? 1
                           : (q + 3 <= n && !memcmp(text + q, "\xef\xbc\x89", 3)) ? 3
                                                                                  : 0;
            memcpy(day, text + p + open, 3);
            day[3] = 0;
            if (close && res(e, "weekday_mark", day) && nl + 6 < sizeof(norm)) {
                norm[nl++] = '(';
                memcpy(norm + nl, day, 3);
                nl += 3;
                norm[nl++] = ')';
                norm[nl] = 0;
                p = q + close;
            }
        }
    }
    snprintf(s->value, sizeof(s->value), "%s", norm);
    /* 3t, 5ch: a Latin counter outranks the core's unreadable-number span. */
    return finish(in, s, p, TTF_SEM_COUNTER, p && alpha(text[p - 1]) ? "ja.latin-counter" : "ja.counter",
                  "@ja");
}

/* Re-read a classified span: pairs of number and counter, a range, a scale. */
static ttf_status_t render_chain(const ttf_engine_t *e, const char *v, ttf_buffer_t *b) {
    size_t n = strlen(v), p = 0;
    char previous[32] = "";
    int after_range = 0;
    ttf_status_t st;
    while (p < n) {
        ja_number_t x;
        char key[32], norm[TTF_IR_VALUE_SIZE];
        if (v[p] == '(') {
            /* (水) すいようび */
            char day[4];
            if (p + 5 > n || v[p + 4] != ')')
                return TTF_EINVAL;
            memcpy(day, v + p + 1, 3);
            day[3] = 0;
            if ((st = word(e, b, "weekday_mark", day)) != TTF_OK)
                return st;
            p += 5;
            continue;
        }
        size_t nl = 0, used = parse_number(e, v + p, n - p, &x, norm, sizeof(norm), &nl), cw, rw;
        size_t start = b->length;
        int issen = p + 4 <= n && v[p] == '1' && !memcmp(v + p + 1, "\xe5\x8d\x83", 3) &&
                    !(p && digit(v[p - 1]));
        if (!used)
            return TTF_EINVAL;
        p += used;
        if (range_mark(v + p, n - p, &rw)) {
            /* 2〜3日 ふつかからみっか: with 日 or つ both ends take the counter. */
            ja_number_t y;
            char k2[32];
            size_t nl2 = 0, u2 = parse_number(e, v + p + rw, n - p - rw, &y, norm, sizeof(norm), &nl2);
            size_t c2 = u2 ? counter_at(e, v + p + rw + u2, n - p - rw - u2, k2, sizeof(k2)) : 0;
            st = c2 && (!strcmp(k2, "日") || !strcmp(k2, "つ")) ? counter_named(e, &x, k2, 0, b)
                                                               : number_reading(e, &x, b);
            if (st != TTF_OK || (st = word(e, b, "number", "range")) != TTF_OK)
                return st;
            p += rw;
            after_range = 1;
            continue;
        }
        cw = counter_at(e, v + p, n - p, key, sizeof(key));
        if (!cw) {
            if ((st = number_reading(e, &x, b)) != TTF_OK)
                return st;
            continue;
        }
        if (!strcmp(key, "分") && p + cw + 4 <= n && !memcmp(v + p + cw, NO, 3) &&
            digit(v[p + cw + 3])) {
            ja_number_t y;
            size_t nl2 = 0;
            p += cw + 3;
            used = parse_number(e, v + p, n - p, &y, norm, sizeof(norm), &nl2);
            if (!used || (st = number_reading(e, &x, b)) != TTF_OK ||
                (st = word(e, b, "number", "fraction")) != TTF_OK ||
                (st = number_reading(e, &y, b)) != TTF_OK)
                return used ? st : TTF_EINVAL;
            p += used;
            continue;
        }
        if (issen) {
            /* 1千万円 いっせんまんえん: the written 1 before 千 is read. */
            ttf_buffer_t tmp = {0};
            st = counter_named(e, &x, key, 0, &tmp);
            if (st == TTF_OK)
                st = tmp.length >= 6 && !memcmp(tmp.data, "せん", 6) ? ttf_buffer_puts(b, "いっ") : TTF_OK;
            if (st == TTF_OK)
                st = ttf_buffer_append(b, tmp.data, tmp.length);
            ttf_buffer_free(&tmp);
            if (st != TTF_OK)
                return st;
            snprintf(previous, sizeof(previous), "%s", key);
            p += cw;
            after_range = 0;
            (void)start;
            continue;
        }
        if (after_range && (!strcmp(key, "歳") || !strcmp(key, "才")))
            x.plain = 1; /* 10-20歳 にじゅっさい, not はたち */
        after_range = 0;
        if (!strcmp(previous, "割") && !strcmp(key, "分"))
            st = counted(e, &x, key, "ぶ:-", 0, b); /* 3割2分 さんわりにぶ */
        else
            st = counter_named(e, &x, key, !strcmp(previous, "月") && !strcmp(key, "日"), b);
        if (st != TTF_OK)
            return st;
        snprintf(previous, sizeof(previous), "%s", key);
        p += cw;
        /* 一日中 いちにちじゅう, 12時半 じゅうにじはん */
        if (p + 3 <= n && !memcmp(v + p, "\xe7\x9b\xae", 3)) {
            /* ２杯目 にはいめ, 3回目 さんかいめ (owner sets, 2026-10-14) */
            if ((st = ttf_buffer_puts(b, "め")) != TTF_OK)
                return st;
            p += 3;
        } else if (p + 3 <= n && !memcmp(v + p, "\xe4\xb8\xad", 3)) {
            /* 一日中 いちにちじゅう; 10人中9人 じゅうにんちゅうきゅうにん (2026-10-11) */
            if ((st = ttf_buffer_puts(b, res(e, "long_suffix", key) ? "じゅう" : "ちゅう")) != TTF_OK)
                return st;
            p += 3;
        } else if (p + 3 <= n && !memcmp(v + p, "\xe7\x94\x9f", 3) && res(e, "student_suffix", key)) {
            if ((st = ttf_buffer_puts(b, "せい")) != TTF_OK) /* 1期生 いっきせい (2026-10-11) */
                return st;
            p += 3;
        } else if (p + 3 <= n && !memcmp(v + p, "\xe5\x8d\x8a", 3) && res(e, "half_suffix", key)) {
            if ((st = ttf_buffer_puts(b, "はん")) != TTF_OK)
                return st;
            p += 3;
        } else if (p + 3 <= n && !memcmp(v + p, "\xe5\xbe\x8c", 3) && res(e, "after_suffix", key) &&
                   !half_word(v + p, n - p)) {
            if ((st = ttf_buffer_puts(b, "ご")) != TTF_OK) /* 3日後 みっかご */
                return st;
            p += 3;
        } else if (p + 3 <= n && !memcmp(v + p, "\xe5\x89\x8d", 3) && res(e, "before_suffix", key) &&
                   !half_word(v + p, n - p)) {
            if ((st = ttf_buffer_puts(b, "まえ")) != TTF_OK) /* 10分前 じゅっぷんまえ */
                return st;
            p += 3;
        } else if (p + 3 <= n && !memcmp(v + p, "\xe9\xa0\x83", 3) && res(e, "around_suffix", key)) {
            if ((st = ttf_buffer_puts(b, "ごろ")) != TTF_OK) /* 8時頃 はちじごろ (2026-10-11) */
                return st;
            p += 3;
        }
        /* 8時半頃 はちじはんごろ */
        if (p >= 3 && p + 3 <= n && !memcmp(v + p - 3, "\xe5\x8d\x8a", 3) && !memcmp(v + p, "\xe9\xa0\x83", 3) &&
            res(e, "around_suffix", key)) {
            if ((st = ttf_buffer_puts(b, "ごろ")) != TTF_OK)
                return st;
            p += 3;
        }
    }
    return TTF_OK;
}

/* 付け, から営業 …: a day_one_cue right after 1日 makes it the first of the month. */
static int day_one_after(const ttf_engine_t *e, const char *s, size_t n) {
    size_t k;
    for (k = 3; k <= n && k <= 15; k += 3) {
        char key[16];
        memcpy(key, s, k);
        key[k] = 0;
        if (res(e, "day_one_cue", key))
            return 1;
    }
    return 0;
}

/* Kanji numerals (2026-10-08): 〇一二三四五六七八九 digits, 十百千 places, 万億兆 myriads. */
static int kanji_digit(uint32_t cp) {
    static const uint32_t digits[10] = {0x3007, 0x4e00, 0x4e8c, 0x4e09, 0x56db,
                                        0x4e94, 0x516d, 0x4e03, 0x516b, 0x4e5d};
    int d;
    for (d = 0; d < 10; ++d)
        if (digits[d] == cp)
            return d;
    return -1;
}

static int kanji_place(uint32_t cp) {
    return cp == 0x5341 ? 1 : cp == 0x767e ? 2 : cp == 0x5343 ? 3 : 0;
}

static int kanji_myriad(uint32_t cp) {
    return cp == 0x4e07 ? 4 : cp == 0x5104 ? 8 : cp == 0x5146 ? 12 : 0;
}

static int kanji_numeral(uint32_t cp) {
    return kanji_digit(cp) >= 0 || kanji_place(cp) || kanji_myriad(cp);
}

/* 三 3, 二千二十四 2024, 二〇二四 2024, 三十五万 350000. Returns bytes used;
 *shape is 1 for one character, 2 for a place structure, 3 for positional digits. */
static size_t kanji_number(const char *s, size_t n, char *out, size_t size, int *shape) {
    uint64_t total = 0, section = 0, current = 0;
    size_t at = 0, chars = 0, positional = 0;
    int places = 0, zero = 0, have = 0;
    char digits[24];
    while (at < n) {
        uint32_t cp;
        size_t w;
        int d, place, myriad;
        if (ttf_utf8_decode(s + at, n - at, &cp, &w) != 1 || !kanji_numeral(cp))
            break;
        d = kanji_digit(cp);
        place = kanji_place(cp);
        myriad = kanji_myriad(cp);
        if (!chars && myriad)
            return 0;
        if (d >= 0) {
            if (positional + 1 >= sizeof(digits))
                return 0;
            digits[positional++] = (char)('0' + d);
            zero |= d == 0;
            current = current * 10 + (uint64_t)d;
            have = 1;
        } else if (place) {
            uint64_t factor = place == 1 ? 10 : place == 2 ? 100 : 1000;
            section += (have ? current : 1) * factor;
            current = 0;
            have = 0;
            places = 1;
        } else {
            uint64_t factor = 1;
            int k;
            for (k = 0; k < myriad; ++k)
                factor *= 10;
            total += (section + current) * factor;
            section = current = 0;
            have = 0;
            places = 1;
        }
        at += w;
        ++chars;
    }
    if (!chars)
        return 0;
    if (!places) {
        digits[positional] = 0;
        /* Positional reading needs 〇 or three or more digits (九九 stays). */
        if (chars > 1 && !zero && chars < 3)
            return 0;
        *shape = chars == 1 ? 1 : 3;
        snprintf(out, size, "%s", digits);
        return at;
    }
    if (zero)
        return 0;
    /* A lone 十, 百 or 千 is one character like 三 (千葉 stays without a counter). */
    *shape = chars == 1 ? 1 : 2;
    snprintf(out, size, "%llu", (unsigned long long)(total + section + current));
    return at;
}

static ttf_status_t render_chain(const ttf_engine_t *e, const char *v, ttf_buffer_t *b);

/* In a Japanese sentence a kanji numeral reads like digits when it carries a
   counter (三本), a place structure (二千二十四) or a positional form (二〇二四).
   A lone 一 stays, a 十分 not after 時 stays (じゅうぶん, enough), and words
   that only look like numbers stay through kanji_word (八百屋, 三日月). */
static ttf_status_t kanji_numerals(const ttf_engine_t *e, const char *s, size_t n,
                                   ttf_buffer_t *b) {
    size_t at = 0;
    uint32_t previous = 0;
    ttf_status_t st;
    while (at < n) {
        uint32_t cp;
        size_t w, k, p, chars[8], count = 0, pairs = 0, nl = 0;
        char norm[TTF_IR_VALUE_SIZE];
        if (ttf_utf8_decode(s + at, n - at, &cp, &w) != 1)
            return ttf_buffer_append(b, s + at, n - at);
        /* Exceptions: the longest listed word starting here stays as written. */
        for (p = at; p < n && count < 8;) {
            uint32_t c2;
            size_t w2;
            if (ttf_utf8_decode(s + p, n - p, &c2, &w2) != 1)
                break;
            p += w2;
            chars[count++] = p;
        }
        for (k = count; k > 1; --k) {
            char key[32];
            if (chars[k - 1] - at >= sizeof(key))
                continue;
            memcpy(key, s + at, chars[k - 1] - at);
            key[chars[k - 1] - at] = 0;
            if (res(e, "kanji_word", key))
                break;
        }
        if (k > 1) {
            if ((st = ttf_buffer_append(b, s + at, chars[k - 1] - at)) != TTF_OK)
                return st;
            at = chars[k - 1];
            previous = 0;
            continue;
        }
        /* 何本 なんぼん, 何階 なんがい (2026-10-09). */
        if (cp == 0x4f55 && !kanji_numeral(previous)) {
            char key[32];
            size_t cw = counter_at(e, s + at + w, n - at - w, key, sizeof(key));
            const char *spec = cw ? res(e, "counter", key) : NULL;
            if (spec && strcmp(key, "つ")) {
                ja_number_t x = {0};
                x.value = 3;
                x.nan = 1;
                if ((st = counted(e, &x, key, spec, 0, b)) != TTF_OK)
                    return st;
                at += w + cw;
                previous = 0;
                continue;
            }
        }
        /* ファイナルファンタジーⅦ セブン: a Roman numeral after katakana reads in English. */
        if (cp >= 0x2160 && cp <= 0x216b && previous >= 0x30a1 && previous <= 0x30fc) {
            char d[8];
            const char *reading;
            snprintf(d, sizeof(d), "%u", (unsigned)(cp - 0x2160 + 1));
            if ((reading = res(e, "english_number", d))) {
                if ((st = ttf_buffer_puts(b, reading)) != TTF_OK)
                    return st;
                at += w;
                previous = 0;
                continue;
            }
        }
        /* 二・二六事件 にーにーろく: kanji digits around ・ read one by one. */
        if (kanji_digit(cp) > 0 && !kanji_numeral(previous) && at + w + 3 < n &&
            !memcmp(s + at + w, "\xe3\x83\xbb", 3)) {
            size_t q = at + w + 3, count = 0, nw;
            uint32_t c2 = 0;
            char digits[8];
            int d = kanji_digit(cp);
            digits[count++] = (char)('0' + d);
            while (q < n && count < 4 && ttf_utf8_decode(s + q, n - q, &c2, &nw) == 1 && kanji_digit(c2) > 0) {
                digits[count++] = (char)('0' + kanji_digit(c2));
                q += nw;
            }
            if (count >= 2 && count <= 3 &&
                !(q < n && ttf_utf8_decode(s + q, n - q, &c2, &nw) == 1 && kanji_numeral(c2))) {
                if ((st = digits_each(e, digits, count, 1, b)) != TTF_OK)
                    return st;
                at = q;
                previous = 0;
                continue;
            }
        }
        /* 〇〇さん: a run of 〇 not next to another numeral is a placeholder (まるまる). */
        if (cp == 0x3007 && !kanji_numeral(previous)) {
            size_t q = at, nw;
            uint32_t next = 0;
            while (q + 3 <= n && !memcmp(s + q, "\xe3\x80\x87", 3))
                q += 3;
            if (q >= n || ttf_utf8_decode(s + q, n - q, &next, &nw) != 1 || !kanji_numeral(next)) {
                if ((st = ttf_buffer_append(b, s + at, q - at)) != TTF_OK)
                    return st;
                at = q;
                previous = 0;
                continue;
            }
        }
        if (kanji_numeral(cp) && !kanji_numeral(previous) && !kanji_myriad(cp)) {
            p = at;
            norm[0] = 0;
            for (;;) {
                char digits[24], key[32];
                int shape = 0;
                size_t used = kanji_number(s + p, n - p, digits, sizeof(digits), &shape), cw;
                if (!used)
                    break;
                cw = counter_at(e, s + p + used, n - p - used, key, sizeof(key));
                /* 十分: minutes after 時, otherwise maybe じゅうぶん. */
                if (cw && !strcmp(key, "分") && !strcmp(digits, "10") && used == 3 && !pairs &&
                    !(at >= 3 && !memcmp(s + at - 3, "\xe6\x99\x82", 3)))
                    cw = 0, shape = 1;
                /* 四つ葉 よつば: Nつ before a kanji is a compound. */
                if (cw && !strcmp(key, "つ") && p + used + cw < n) {
                    uint32_t next;
                    size_t nw;
                    if (ttf_utf8_decode(s + p + used + cw, n - p - used - cw, &next, &nw) == 1 &&
                        next >= 0x4e00 && next <= 0x9fff)
                        cw = 0, shape = 1;
                }
                if (!cw && (pairs || shape == 1))
                    break;
                if (nl + strlen(digits) + cw + 1 >= sizeof(norm))
                    break;
                memcpy(norm + nl, digits, strlen(digits));
                nl += strlen(digits);
                memcpy(norm + nl, s + p + used, cw);
                nl += cw;
                norm[nl] = 0;
                p += used + cw;
                ++pairs;
                if (!cw)
                    break;
                /* 三分の一 さんぶんのいち */
                if (!strcmp(key, "分") && p + 3 < n && !memcmp(s + p, NO, 3)) {
                    char second[24];
                    int shape2 = 0;
                    size_t used2 =
                        kanji_number(s + p + 3, n - p - 3, second, sizeof(second), &shape2);
                    if (used2 && nl + 3 + strlen(second) + 1 < sizeof(norm)) {
                        memcpy(norm + nl, NO, 3);
                        nl += 3;
                        memcpy(norm + nl, second, strlen(second));
                        nl += strlen(second);
                        norm[nl] = 0;
                        p += 3 + used2;
                    }
                    break;
                }
                if (p + 3 <= n && nl + 4 < sizeof(norm) &&
                    ((!memcmp(s + p, "\xe7\x9b\xae", 3) && strcmp(key, "分") && strcmp(key, "丁目") && strcmp(key, "代目")) ||
             (!memcmp(s + p, "\xe4\xb8\xad", 3) && res(e, "long_suffix", key)) ||
                     (!memcmp(s + p, "\xe5\x8d\x8a", 3) && res(e, "half_suffix", key)) ||
                     (!memcmp(s + p, "\xe5\xbe\x8c", 3) && res(e, "after_suffix", key) && !half_word(s + p, n - p)) ||
                     (!memcmp(s + p, "\xe5\x89\x8d", 3) && res(e, "before_suffix", key) && !half_word(s + p, n - p)) ||
                     (!memcmp(s + p, "\xe9\xa0\x83", 3) && res(e, "around_suffix", key)) ||
             (!memcmp(s + p, "\xe4\xb8\xad", 3) && p + 3 < n && digit(s[p + 3])) ||
             (!memcmp(s + p, "\xe7\x94\x9f", 3) && res(e, "student_suffix", key) && student_end(s + p + 3, n - p - 3)))) {
                    memcpy(norm + nl, s + p, 3);
                    nl += 3;
                    norm[nl] = 0;
                    p += 3;
                    if (!memcmp(s + p - 3, "\xe5\x8d\x8a", 3) && p + 3 <= n && nl + 4 < sizeof(norm) &&
                        !memcmp(s + p, "\xe9\xa0\x83", 3) && res(e, "around_suffix", key)) {
                        memcpy(norm + nl, s + p, 3);
                        nl += 3;
                        norm[nl] = 0;
                        p += 3;
                    }
                    break;
                }
            }
            if (pairs) {
                size_t mark = b->length;
                /* 一日付け, 一日から営業 ついたち */
                if (!strcmp(norm, "1日") && day_one_after(e, s + p, n - p)) {
                    if ((st = word(e, b, "counter_reading", "date_day:1")) != TTF_OK)
                        return st;
                } else if ((st = render_chain(e, norm, b)) != TTF_OK)
                    return st;
                /* 一千万 いっせんまん */
                if (n - at >= 6 && !memcmp(s + at, "\xe4\xb8\x80\xe5\x8d\x83", 6) &&
                    b->length - mark >= 6 && !memcmp(b->data + mark, "せん", 6)) {
                    ttf_buffer_t tmp = {0};
                    st = ttf_buffer_append(&tmp, b->data + mark, b->length - mark);
                    b->length = mark;
                    if (st == TTF_OK)
                        st = ttf_buffer_puts(b, "いっ");
                    if (st == TTF_OK)
                        st = ttf_buffer_append(b, tmp.data, tmp.length);
                    ttf_buffer_free(&tmp);
                    if (st != TTF_OK)
                        return st;
                }
                at = p;
                previous = 0;
                continue;
            }
        }
        if ((st = ttf_buffer_append(b, s + at, w)) != TTF_OK)
            return st;
        previous = cp;
        at += w;
    }
    return TTF_OK;
}

static ttf_status_t letters(const ttf_engine_t *e, const char *s, size_t n, ttf_buffer_t *b) {
    size_t i;
    int any = 0;
    /* Check first, so a word that is not letters appends nothing. */
    for (i = 0; i < n; ++i)
        if (!((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z') || s[i] == '.' ||
              s[i] == '-' || s[i] == '&'))
            return TTF_EINVAL;
    for (i = 0; i < n; ++i) {
        char key[2] = {s[i], 0};
        ttf_status_t st;
        if (key[0] >= 'a' && key[0] <= 'z')
            key[0] = (char)(key[0] - 'a' + 'A');
        if (key[0] == '&') {
            if ((st = word(e, b, "speech_symbol", "&")) != TTF_OK)
                return st;
            continue;
        }
        if (key[0] == '.' || key[0] == '-')
            continue;
        if (key[0] < 'A' || key[0] > 'Z')
            return TTF_EINVAL;
        if ((st = word(e, b, "letter_name", key)) != TTF_OK)
            return st;
        any = 1;
    }
    return any ? TTF_OK : TTF_EINVAL;
}

static ttf_status_t digits_each(const ttf_engine_t *e, const char *s, size_t n, int lengthen,
                                ttf_buffer_t *b) {
    size_t i;
    for (i = 0; i < n; ++i) {
        char key[2] = {s[i], 0};
        const char *lengthened;
        ttf_status_t st;
        if (!digit(s[i]))
            return TTF_EINVAL;
        /* にー, ごー: phone numbers and digit sequences lengthen 2 and 5; a
           digit spelled among letters (B2 ビーに) does not. */
        lengthened = lengthen ? res(e, "digit", key) : NULL;
        st = lengthened ? ttf_buffer_puts(b, lengthened) : word(e, b, "number", key);
        if (st != TTF_OK)
            return st;
    }
    return TTF_OK;
}

static ttf_status_t plain_number(const ttf_engine_t *e, const char *value, ttf_buffer_t *b) {
    ja_number_t x;
    char norm[TTF_IR_VALUE_SIZE];
    size_t nl = 0, n = strlen(value);
    if (!n || parse_number(e, value, n, &x, norm, sizeof(norm), &nl) != n)
        return TTF_EINVAL;
    return number_reading(e, &x, b);
}

/* 3:15 さんじじゅうごふん; a meridiem leads (ごご). */
static ttf_status_t time_reading(const ttf_engine_t *e, const ttf_semantic_t *s, ttf_buffer_t *b) {
    unsigned h, m = 0, sec = 0;
    char value[16];
    ttf_status_t st;
    int fields = sscanf(s->value, "%u:%u:%u", &h, &m, &sec);
    if (fields < 2)
        return TTF_EINVAL;
    if (s->secondary[0] && (st = word(e, b, "meridiem", s->secondary)) != TTF_OK)
        return st;
    snprintf(value, sizeof(value), "%u", h);
    if ((st = value_counted(e, value, "時", 0, b)) != TTF_OK)
        return st;
    if (m) {
        snprintf(value, sizeof(value), "%u", m);
        if ((st = value_counted(e, value, "分", 0, b)) != TTF_OK)
            return st;
    }
    if (fields == 3 && sec) {
        snprintf(value, sizeof(value), "%u", sec);
        return value_counted(e, value, "秒", 0, b);
    }
    return TTF_OK;
}

/* 2024-10-08 にせんにじゅうよねんじゅうがつようか; empty fields are absent. */
static ttf_status_t date_reading(const ttf_engine_t *e, const char *v, ttf_buffer_t *b) {
    static const char *const counters[3] = {"年", "月", "日"};
    char field[3][16] = {"", "", ""};
    size_t f = 0, k = 0, i;
    ttf_status_t st;
    for (i = 0; v[i]; ++i) {
        if (v[i] == '-') {
            if (++f > 2)
                return TTF_EINVAL;
            k = 0;
            continue;
        }
        if (!digit(v[i]) || k + 1 >= sizeof(field[0]))
            return TTF_EINVAL;
        field[f][k++] = v[i];
        field[f][k] = 0;
    }
    if (f != 2)
        return TTF_EINVAL;
    for (i = 0; i < 3; ++i) {
        char value[16];
        if (!field[i][0])
            continue;
        snprintf(value, sizeof(value), "%lu", strtoul(field[i], NULL, 10));
        if ((st = value_counted(e, value, counters[i], i == 2 && field[1][0], b)) != TTF_OK)
            return st;
    }
    return TTF_OK;
}

static const char *currency_counter(const ttf_engine_t *e, const char *code) {
    return code[0] ? res(e, "currency", code) : NULL;
}

/* A unit row reads like a counter (reading:class); an unknown unit keeps its
   written form after the number. */
static ttf_status_t measure(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                            size_t length, ttf_buffer_t *b) {
    const char *spec = res(e, "unit", s->unit);
    ja_number_t x;
    char norm[TTF_IR_VALUE_SIZE];
    size_t nl = 0, n = strlen(s->value), k;
    ttf_status_t st;
    if (!n || parse_number(e, s->value, n, &x, norm, sizeof(norm), &nl) != n)
        return TTF_EINVAL;
    if (spec)
        return counted(e, &x, s->unit, spec, 0, b);
    if ((st = number_reading(e, &x, b)) != TTF_OK)
        return st;
    for (k = 0; k < length && (digit(surface[k]) || surface[k] == '.' || surface[k] == ',' ||
                               surface[k] == '-' || surface[k] == ' ');
         ++k)
        ;
    return k < length ? ttf_buffer_append(b, surface + k, length - k) : TTF_EINVAL;
}

/* A4 エーよん, MP3 エムピーさん; a longer word keeps its letters. */
static ttf_status_t alnum(const ttf_engine_t *e, const char *s, size_t n, int japanese,
                          int code, ttf_buffer_t *b) {
    size_t i = 0, k, turns = 0;
    ttf_status_t st;
    /* R7K2M9 あーるななけーにーえむきゅう, H0729 えいちぜろななにきゅう: letters and digits that
       alternate three times or more, or a digit group with a leading zero after a letter, read as
       a code (owner v2, 2026-10-15) */
    for (k = 1; k < n; ++k) {
        turns += digit(s[k]) != digit(s[k - 1]) && (digit(s[k]) || alpha(s[k])) && (digit(s[k - 1]) || alpha(s[k - 1]));
        if (!code && digit(s[k]) && s[k] == '0' && alpha(s[k - 1]) && k + 1 < n && digit(s[k + 1]))
            code = 1;
    }
    if (turns >= 3)
        code = 1;
    if (code)
        japanese = 1;
    if (n >= 3 && n <= 4 && s[0] == 'R' && digit(s[1]) && digit(s[2]) && (n == 3 || digit(s[3]))) {
        ttf_status_t rs = letters(e, s, 1, b);
        return rs == TTF_OK ? digits_each(e, s + 1, n - 1, 1, b) : rs;
    }
    {
        char whole[16];
        const char *reading;
        if (n < sizeof(whole)) {
            memcpy(whole, s, n);
            whole[n] = 0;
            if ((reading = res(e, "alnum_word", whole)))
                return ttf_buffer_puts(b, reading); /* B2B ビートゥービー */
        }
    }
    while (i < n) {
        size_t j = i;
        if (digit(s[i])) {
            char value[24];
            while (j < n && digit(s[j]))
                ++j;
            /* F1 エフワン, H2O エイチツーオー, 3D スリーディー: one digit after letters, or
               before a single D or G, reads in English; A4, B5 (paper) stay Japanese. */
            if (j - i == 1 && !(j + 1 < n && s[j] == '.')) {
                char d[2] = {s[i], 0};
                size_t h = i;
                int caps = 0, lower = 0;
                while (h && alpha(s[h - 1])) {
                    --h;
                    caps |= s[h] >= 'A' && s[h] <= 'Z';
                    lower |= s[h] >= 'a' && s[h] <= 'z';
                }
                /* pH7 (mixed case) and M8 (a thread size) keep Japanese digits, as do A4, B5. */
                int after = i && alpha(s[i - 1]) && !(caps && lower) && !(i - h == 1 && s[h] == 'M');
                int paper = after && i == 1 && (s[0] == 'A' || s[0] == 'B') && s[i] >= '3' && s[i] <= '6';
                int dg = !i && j + 1 == n && (s[j] == 'D' || s[j] == 'G');
                if (!japanese && ((after && !paper) || dg)) {
                    if ((st = word(e, b, "english_number", d)) != TTF_OK)
                        return st;
                    i = j;
                    continue;
                }
            }
            if (j - i <= 4 && j + 1 < n && s[j] == '.' && digit(s[j + 1])) {
                ++j;
                while (j < n && digit(s[j]) && j - i < 20)
                    ++j;
                snprintf(value, sizeof(value), "%.*s", (int)(j - i), s + i);
                st = plain_number(e, value, b);
            } else if (code && j - i > 1) {
                /* KS-2026-0418, SN-10A: a code reads its digit groups digit by digit (owner v2,
                   2026-10-15) */
                st = digits_each(e, s + i, j - i, 1, b);
            } else if (j - i <= 4) {
                snprintf(value, sizeof(value), "%.*s", (int)(j - i), s + i);
                st = plain_number(e, value, b);
            } else
                st = digits_each(e, s + i, j - i, 1, b);
        } else if ((s[i] >= 'A' && s[i] <= 'Z') || (s[i] >= 'a' && s[i] <= 'z')) {
            int upper = 1;
            while (j < n && ((s[j] >= 'A' && s[j] <= 'Z') || (s[j] >= 'a' && s[j] <= 'z'))) {
                upper &= s[j] >= 'A' && s[j] <= 'Z';
                ++j;
            }
            char key[16] = "";
            const char *reading = NULL;
            if (upper && j - i < sizeof(key)) {
                memcpy(key, s + i, j - i);
                key[j - i] = 0;
                reading = res(e, "latin_word", key);
            }
            /* COVID-19 コビッドじゅうきゅう: a latin_word keeps its reading. */
            st = reading ? ttf_buffer_puts(b, reading)
                 : (upper && j - i <= 5) || (j - i <= 3 && j < n && digit(s[j]))
                     ? letters(e, s + i, j - i, b)
                     : ttf_buffer_append(b, s + i, j - i);
        } else if (code && s[i] == '-') {
            ++j;
            st = code == 2 ? TTF_OK : word(e, b, "number", "phone_separator");
        } else if (s[i] == '-' && i && alpha(s[i - 1]) && i + 1 < n && digit(s[i + 1])) {
            ++j; /* F-15 エフじゅうご */
            st = TTF_OK;
        } else {
            ++j;
            st = ttf_buffer_append(b, s + i, 1);
        }
        if (st != TTF_OK)
            return st;
        i = j;
    }
    return TTF_OK;
}

/* x = 3 エックスイコールさん, 2 + 3 = 5 にたすさんイコールご: the classifier writes the
   expression with spaces between numbers, letters and operators. */
static ttf_status_t expression(const ttf_engine_t *e, const char *v, ttf_buffer_t *b) {
    size_t i = 0, n = strlen(v);
    ttf_status_t st;
    while (i < n) {
        size_t j = i, k, numeric = 1, letter = 1;
        char part[32];
        while (j < n && v[j] != ' ')
            ++j;
        if (j == i) {
            ++i;
            continue;
        }
        if (j - i >= sizeof(part))
            return TTF_EINVAL;
        memcpy(part, v + i, j - i);
        part[j - i] = 0;
        for (k = 0; part[k]; ++k) {
            numeric &= digit(part[k]) || part[k] == '.' || part[k] == ',';
            letter &= alpha(part[k]);
        }
        /* 1 / 2 にぶんのいち: a proper fraction of small whole numbers (ja dev v1, 2026-10-17) */
        if (numeric && digit(part[0]) && !strchr(part, '.') && j + 3 < n && v[j + 1] == '/' && v[j + 2] == ' ' &&
            digit(v[j + 3])) {
            size_t d0 = j + 3, d1 = d0;
            char den[8];
            while (d1 < n && digit(v[d1]))
                ++d1;
            if ((d1 >= n || v[d1] == ' ') && d1 - d0 < sizeof(den) && strtoul(part, NULL, 10) >= 1 &&
                strtoul(part, NULL, 10) < strtoul(v + d0, NULL, 10) && strtoul(v + d0, NULL, 10) <= 100) {
                snprintf(den, sizeof(den), "%.*s", (int)(d1 - d0), v + d0);
                if ((st = plain_number(e, den, b)) != TTF_OK || (st = ttf_buffer_puts(b, "ぶんの")) != TTF_OK ||
                    (st = plain_number(e, part, b)) != TTF_OK)
                    return st;
                i = d1;
                continue;
            }
        }
        if (numeric && digit(part[0]))
            st = plain_number(e, part, b);
        else if (letter)
            st = letters(e, part, j - i, b);
        else if (res(e, "math_operator", part))
            st = word(e, b, "math_operator", part);
        else
            st = word(e, b, "speech_symbol", part);
        if (st != TTF_OK)
            return st;
        i = j;
    }
    return TTF_OK;
}

/* 3-2 さんたいに, 10:10 じゅうたいじゅう: two numbers around - or : read with たい. */
static ttf_status_t score(const ttf_engine_t *e, const char *v, ttf_buffer_t *b) {
    const char *mark = strpbrk(v, ":-");
    char left[16], right[16];
    ttf_status_t st;
    if (!mark || mark == v || (size_t)(mark - v) >= sizeof(left) || strlen(mark + 1) >= sizeof(right) ||
        !mark[1])
        return TTF_EINVAL;
    snprintf(left, sizeof(left), "%.*s", (int)(mark - v), v);
    snprintf(right, sizeof(right), "%s", mark + 1);
    if ((st = plain_number(e, left, b)) != TTF_OK ||
        (st = word(e, b, "number", "score_separator")) != TTF_OK)
        return st;
    return plain_number(e, right, b);
}

static ttf_status_t phone(const ttf_engine_t *e, const char *v, ttf_buffer_t *b) {
    size_t i;
    int separator = 0, any = 0;
    ttf_status_t st;
    for (i = 0; v[i]; ++i) {
        if (digit(v[i])) {
            if (separator && any && (st = word(e, b, "number", "phone_separator")) != TTF_OK)
                return st;
            separator = 0;
            if ((st = digits_each(e, v + i, 1, 1, b)) != TTF_OK)
                return st;
            any = 1;
        } else if (v[i] == '+') {
            if ((st = word(e, b, "number", "plus")) != TTF_OK)
                return st;
        } else
            separator = 1;
    }
    return any ? TTF_OK : TTF_EINVAL;
}

static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *b) {
    ttf_status_t st;
    const char *key;
    switch (s->type) {
    case TTF_SEM_COUNTER:
        if (!strcmp(s->unit, "@era")) {
            /* R7.6.30 れいわ + 7年6月30日 */
            const char *bar = strchr(s->value, '|');
            if (!bar)
                return TTF_EINVAL;
            if ((st = ttf_buffer_append(b, s->value, (size_t)(bar - s->value))) != TTF_OK)
                return st;
            return render_chain(e, bar + 1, b);
        }
        if (!strcmp(s->unit, "@ja"))
            return render_chain(e, s->value, b);
        /* @say (ja_dev_forms): w:text as written, n:NUM a number, r:CHAIN a number with its counter */
        if (!strcmp(s->unit, "@say")) {
            const char *it = s->value;
            while (*it) {
                const char *bar = strchr(it, '|');
                size_t l = bar ? (size_t)(bar - it) : strlen(it);
                char item[64];
                if (l < 2 || l - 2 >= sizeof(item) || it[1] != ':')
                    return TTF_EINVAL;
                memcpy(item, it + 2, l - 2);
                item[l - 2] = 0;
                st = it[0] == 'w' ? ttf_buffer_puts(b, item)
                     : it[0] == 'n' ? plain_number(e, item, b)
                     : it[0] == 'r' ? render_chain(e, item, b) : TTF_EINVAL;
                if (st != TTF_OK)
                    return st;
                if (!bar)
                    break;
                it = bar + 1;
            }
            return TTF_OK;
        }
        return measure(e, s, surface, length, b);
    case TTF_SEM_MEASURE:
        return measure(e, s, surface, length, b);
    case TTF_SEM_CARDINAL:
    case TTF_SEM_NUMBER:
        return plain_number(e, s->value, b);
    case TTF_SEM_ORDINAL:
        /* 3{@ordinal}, 1st: だいさん, だいいち */
        if ((st = word(e, b, "number", "ordinal_prefix")) != TTF_OK)
            return st;
        return plain_number(e, s->value, b);
    case TTF_SEM_DIGIT_SEQUENCE:
        return digits_each(e, s->value, strlen(s->value), 1, b);
    case TTF_SEM_PHONE:
        return phone(e, s->value, b);
    case TTF_SEM_PERCENT:
        return value_counted(e, s->value, "%", 0, b);
    case TTF_SEM_CURRENCY:
        if (s->secondary[0] || !(key = currency_counter(e, s->unit)))
            return TTF_EINVAL;
        return value_counted(e, s->value, key, 0, b);
    case TTF_SEM_TIME:
        return time_reading(e, s, b);
    case TTF_SEM_DATE:
        if (!strcmp(s->unit, "year"))
            return value_counted(e, s->value, "年", 0, b);
        /* --03-15 さんがつじゅうごにち */
        if (!strcmp(s->unit, "month-day") && !strncmp(s->value, "--", 2))
            return date_reading(e, s->value + 1, b);
        if (s->unit[0])
            return TTF_EINVAL;
        return date_reading(e, s->value, b);
    case TTF_SEM_FRACTION:
        /* 1/250秒 にひゃくごじゅうぶんのいちびょう: the counter reads after. */
        if (!strncmp(s->unit, "counter:", 8)) {
            const char *spec = res(e, "counter", s->unit + 8), *colon;
            if (!spec || !(colon = strchr(spec, ':')))
                return TTF_ERESOURCE;
            if ((st = plain_number(e, s->secondary, b)) != TTF_OK ||
                (st = word(e, b, "number", "fraction")) != TTF_OK ||
                (st = plain_number(e, s->value, b)) != TTF_OK)
                return st;
            return ttf_buffer_append(b, spec, (size_t)(colon - spec));
        }
        if (s->unit[0] && strncmp(s->unit, "mixed:", 6))
            return TTF_EINVAL;
        if (!strncmp(s->unit, "mixed:", 6) && ((st = plain_number(e, s->unit + 6, b)) != TTF_OK ||
                                               (st = word(e, b, "number", "mixed")) != TTF_OK))
            return st;
        if ((st = plain_number(e, s->secondary, b)) != TTF_OK ||
            (st = word(e, b, "number", "fraction")) != TTF_OK)
            return st;
        return plain_number(e, s->value, b);
    case TTF_SEM_RANGE: {
        const char *counter = NULL;
        if (!strcmp(s->unit, "time")) {
            ttf_semantic_t endpoint = *s;
            endpoint.type = TTF_SEM_TIME;
            endpoint.unit[0] = endpoint.secondary[0] = 0;
            if ((st = time_reading(e, &endpoint, b)) != TTF_OK ||
                (st = word(e, b, "number", "range")) != TTF_OK)
                return st;
            snprintf(endpoint.value, sizeof(endpoint.value), "%s", s->secondary);
            return time_reading(e, &endpoint, b);
        }
        if (!strcmp(s->unit, "year"))
            counter = "年";
        else if (!strcmp(s->unit, "percent"))
            counter = "%";
        else if (s->unit[0] && !(counter = currency_counter(e, s->unit))) {
            /* 3〜5km さんからごキロメートル: a measure unit reads after the second number. */
            const char *spec = res(e, "unit", s->unit);
            ja_number_t y;
            char norm[TTF_IR_VALUE_SIZE];
            size_t nl = 0, n = strlen(s->secondary);
            if (!spec || !n || parse_number(e, s->secondary, n, &y, norm, sizeof(norm), &nl) != n)
                return TTF_EINVAL;
            if ((st = plain_number(e, s->value, b)) != TTF_OK ||
                (st = word(e, b, "number", "range")) != TTF_OK)
                return st;
            return counted(e, &y, s->unit, spec, 0, b);
        }
        if ((st = plain_number(e, s->value, b)) != TTF_OK ||
            (st = word(e, b, "number", "range")) != TTF_OK)
            return st;
        return counter ? value_counted(e, s->secondary, counter, 0, b)
                       : plain_number(e, s->secondary, b);
    }
    case TTF_SEM_VERSION:
    case TTF_SEM_IP_ADDRESS: {
        /* v1.2.3 バージョンいってんにてんさん; 192.168.0.1 reads each part digit by
           digit. The parts come from the written surface. */
        const char *v = s->value[0] ? s->value : surface;
        size_t i = 0, n = s->value[0] ? strlen(s->value) : length;
        int ip = s->type == TTF_SEM_IP_ADDRESS;
        while (i < n && !digit(v[i]))
            ++i;
        if (i && (st = word(e, b, "number", "version_prefix")) != TTF_OK)
            return st;
        while (i < n) {
            size_t j = i;
            char part[24];
            unsigned long value;
            while (j < n && digit(v[j]))
                ++j;
            if (j == i || j - i >= sizeof(part))
                return TTF_EINVAL;
            snprintf(part, sizeof(part), "%.*s", (int)(j - i), v + i);
            value = strtoul(part, NULL, 10);
            st = ip || (j - i > 1 && part[0] == '0') ? digits_each(e, part, j - i, 0, b)
                                                     : plain_number(e, part, b);
            if (st != TTF_OK)
                return st;
            if (j < n) {
                if (v[j] != '.')
                    return TTF_EINVAL;
                /* 1.5 いってん, 2.10.1 にてんじゅってん */
                if (!ip && value && (value % 10 == 1 || value % 100 == 10) &&
                    (st = geminate(b)) != TTF_OK)
                    return st;
                if ((st = word(e, b, "number", "version_separator")) != TTF_OK)
                    return st;
                ++j;
            }
            i = j;
        }
        return TTF_OK;
    }
    case TTF_SEM_ABBREVIATION:
        if (!s->value[0])
            return TTF_EINVAL;
        return ttf_buffer_puts(b, s->value);
    case TTF_SEM_SYMBOL:
        if (!strcmp(s->unit, "@expression"))
            return expression(e, s->value, b);
        if (!strcmp(s->unit, "@score"))
            return score(e, s->value, b);
        if (!strcmp(s->unit, "@code")) {
            /* JAN 4901… ジャン よんきゅう…: the label, then digits (hyphen groups with の). */
            size_t k = 0, len = strlen(s->value);
            char label[16];
            const char *word_reading;
            while (k < len && alpha(s->value[k]) && k + 1 < sizeof(label))
                ++k;
            memcpy(label, s->value, k);
            label[k] = 0;
            word_reading = res(e, "latin_word", label);
            st = word_reading ? ttf_buffer_puts(b, word_reading) : letters(e, label, k, b);
            if (st != TTF_OK)
                return st;
            if (k < len && s->value[k] == ' ') {
                if ((st = ttf_buffer_puts(b, " ")) != TTF_OK)
                    return st;
                ++k;
            }
            return strchr(s->value + k, '-') ? phone(e, s->value + k, b)
                                             : digits_each(e, s->value + k, len - k, 1, b);
        }
        if (!strcmp(s->unit, "@weekday-range")) {
            /* 営業は月〜土 営業はげつからど */
            size_t k = strlen(s->value), w = (size_t)strtoul(s->secondary, NULL, 10);
            char a[4], z[4];
            if (k < 6 + w)
                return TTF_EINVAL;
            if (k > 6 + w && (st = kanji_numerals(e, s->value, k - 6 - w, b)) != TTF_OK)
                return st;
            memcpy(a, s->value + k - 6 - w, 3);
            a[3] = 0;
            memcpy(z, s->value + k - 3, 3);
            z[3] = 0;
            if ((st = word(e, b, "weekday_short", a)) != TTF_OK ||
                (st = word(e, b, "number", "range")) != TTF_OK)
                return st;
            return word(e, b, "weekday_short", z);
        }
        if (!strcmp(s->unit, "@postal")) {
            if ((st = word(e, b, "number", "postal_prefix")) != TTF_OK)
                return st;
            return phone(e, s->value, b);
        }
        if (!strcmp(s->unit, "@field-mark")) {
            const char *bar = strchr(s->value, '|'), *metre = res(e, "unit", "m");
            char whole[4], part[4];
            size_t k = bar ? (size_t)(bar - s->value) : 0;
            if (!bar || !k || k >= sizeof(whole) || !metre || !strchr(metre, ':'))
                return TTF_EINVAL;
            memcpy(whole, s->value, k);
            whole[k] = 0;
            snprintf(part, sizeof(part), "%s", bar + 1);
            if ((st = plain_number(e, whole, b)) != TTF_OK)
                return st;
            if ((st = ttf_buffer_append(b, metre, (size_t)(strchr(metre, ':') - metre))) != TTF_OK)
                return st;
            /* 6m05 ろくメートルれいご: a leading zero is read */
            if (part[0] == '0' && (st = word(e, b, "number", "0")) != TTF_OK)
                return st;
            return plain_number(e, part[0] == '0' ? part + 1 : part, b);
        }
        if (!strcmp(s->unit, "@basement")) {
            if ((st = word(e, b, "number", "basement")) != TTF_OK)
                return st;
            return value_counted(e, s->value, "階", 0, b);
        }
        if (!strcmp(s->unit, "@ja-address")) {
            /* 3-12-5 さんのじゅうにのご */
            size_t i = 0, k = strlen(s->value);
            while (i < k) {
                size_t j = i;
                char part[8];
                while (j < k && digit(s->value[j]))
                    ++j;
                if (j == i || j - i >= sizeof(part))
                    return TTF_EINVAL;
                snprintf(part, sizeof(part), "%.*s", (int)(j - i), s->value + i);
                if ((st = plain_number(e, part, b)) != TTF_OK)
                    return st;
                if (j < k && (st = word(e, b, "number", "phone_separator")) != TTF_OK)
                    return st;
                i = j + 1;
            }
            return TTF_OK;
        }
        if (!strcmp(s->unit, "@month-rate")) {
            size_t k = strlen(s->value);
            if (k < 3)
                return TTF_EINVAL;
            if (k > 3 && (st = kanji_numerals(e, s->value, k - 3, b)) != TTF_OK)
                return st;
            return ttf_buffer_puts(b, "つき");
        }
        if (!strcmp(s->unit, "@grade")) {
            /* 息子は小6 息子はしょうろく: the words before the grade read as usual. */
            size_t k = strlen(s->value);
            char g[4], d[2];
            if (k < 4)
                return TTF_EINVAL;
            if (k > 4 && (st = kanji_numerals(e, s->value, k - 4, b)) != TTF_OK)
                return st;
            memcpy(g, s->value + k - 4, 3);
            g[3] = 0;
            d[0] = s->value[k - 1];
            d[1] = 0;
            if ((st = word(e, b, "grade_prefix", g)) != TTF_OK)
                return st;
            return plain_number(e, d, b);
        }
        if (!strcmp(s->unit, "@power") && strchr(s->value, '|')) {
            /* 2^10 にのじゅうじょう */
            const char *bar = strchr(s->value, '|');
            char base[24];
            if ((size_t)(bar - s->value) >= sizeof(base))
                return TTF_EINVAL;
            snprintf(base, sizeof(base), "%.*s", (int)(bar - s->value), s->value);
            if ((st = plain_number(e, base, b)) != TTF_OK || (st = ttf_buffer_puts(b, "の")) != TTF_OK ||
                (st = plain_number(e, bar + 1, b)) != TTF_OK)
                return st;
            return word(e, b, "number", "power");
        }
        if (!strcmp(s->unit, "@pressure") || !strcmp(s->unit, "@current")) {
            const char *bar = strchr(s->value, '|');
            char left[8];
            if (!bar || (size_t)(bar - s->value) >= sizeof(left))
                return TTF_EINVAL;
            snprintf(left, sizeof(left), "%.*s", (int)(bar - s->value), s->value);
            if (!strcmp(s->unit, "@current")) {
                /* AC|100 エーシーひゃくボルト */
                if ((st = letters(e, left, strlen(left), b)) != TTF_OK || (st = plain_number(e, bar + 1, b)) != TTF_OK)
                    return st;
                return ttf_buffer_puts(b, res(e, "unit", "V") ? "ボルト" : "ブイ");
            }
            /* 128|82 ひゃくにじゅうはちのはちじゅうに + mmHg */
            if ((st = plain_number(e, left, b)) != TTF_OK || (st = ttf_buffer_puts(b, "の")) != TTF_OK ||
                (st = plain_number(e, bar + 1, b)) != TTF_OK)
                return st;
            {
                const char *u = res(e, "unit", "mmHg");
                size_t ul = u ? strcspn(u, ":") : 0;
                return u ? ttf_buffer_append(b, u, ul) : TTF_OK;
            }
        }
        if (!strcmp(s->unit, "@seat")) {
            /* 23A にじゅうさんエー */
            const char *bar = strchr(s->value, '|');
            char base[8];
            if (!bar || (size_t)(bar - s->value) >= sizeof(base))
                return TTF_EINVAL;
            snprintf(base, sizeof(base), "%.*s", (int)(bar - s->value), s->value);
            if ((st = plain_number(e, base, b)) != TTF_OK)
                return st;
            return word(e, b, "letter_name", bar + 1);
        }
        if (!strcmp(s->unit, "@power")) {
            size_t k = strlen(s->value);
            char base[24];
            if (k < 2 || k >= sizeof(base))
                return TTF_EINVAL;
            snprintf(base, sizeof(base), "%.*s", (int)(k - 1), s->value);
            if ((st = plain_number(e, base, b)) != TTF_OK || (st = ttf_buffer_puts(b, "の")) != TTF_OK)
                return st;
            snprintf(base, sizeof(base), "%c", s->value[k - 1]);
            if ((st = plain_number(e, base, b)) != TTF_OK)
                return st;
            return word(e, b, "number", "power");
        }
        return ttf_buffer_append(b, surface, length);
    case TTF_SEM_INITIALISM:
        if (letters(e, surface, length, b) == TTF_OK)
            return TTF_OK;
        return ttf_buffer_append(b, surface, length);
    case TTF_SEM_ALNUM_TERM:
        return alnum(e, surface, length, !strcmp(s->secondary, "after-dai"),
                     !strcmp(s->unit, "code") ? 1 : !strcmp(s->unit, "code-silent") ? 2 : 0, b);
    case TTF_SEM_CUSTOM:
        if (!strcmp(s->unit, "@alphabet")) {
            const char *v = !strcmp(s->secondary, "control-surface") ? surface : s->value;
            size_t n = v == surface ? length : strlen(v);
            size_t i;
            for (i = 0; i < n; ++i) {
                st = digit(v[i]) ? digits_each(e, v + i, 1, 0, b) : letters(e, v + i, 1, b);
                if (st != TTF_OK)
                    return st;
            }
            return n ? TTF_OK : TTF_EINVAL;
        }
        /* U+20A9: ユー プラス にぜろエーきゅう, spelled like an address (2026-10-13). */
        if (!strcmp(s->unit, "@codepoint")) {
            const char *plus = ttf_resource_get(e, "speech_symbol", "ja-JP", "+", NULL);
            size_t i;
            if (e->frontend_options.output_mode != TTF_OUTPUT_SPEECH)
                return ttf_buffer_append(b, surface, length);
            if (!plus)
                return TTF_ERESOURCE;
            if ((st = letters(e, "U", 1, b)) != TTF_OK || (st = ttf_buffer_puts(b, " ")) != TTF_OK ||
                (st = ttf_buffer_puts(b, plus)) != TTF_OK || (st = ttf_buffer_puts(b, " ")) != TTF_OK)
                return st;
            for (i = 0; s->value[i]; ++i)
                if ((st = digit(s->value[i]) ? digits_each(e, s->value + i, 1, 0, b)
                                             : letters(e, s->value + i, 1, b)) != TTF_OK)
                    return st;
            return TTF_OK;
        }
        if (!strcmp(s->unit, "@counter"))
            return TTF_EINVAL;
        if (!strcmp(s->unit, "@score"))
            return score(e, s->value, b);
        if (!strcmp(s->unit, "@email") || !strcmp(s->unit, "@url"))
            return ttf_electronic_control(e, s, surface, length, b);
        return ttf_semantic_control(e, s, surface, length, b);
    case TTF_SEM_WORD: {
        size_t i;
        for (i = 0; i + 3 <= length; ++i) {
            uint32_t cp;
            size_t w;
            if (ttf_utf8_decode(surface + i, length - i, &cp, &w) == 1 &&
                (kanji_numeral(cp) || cp == 0x4f55 || (cp >= 0x2160 && cp <= 0x216b)))
                return kanji_numerals(e, surface, length, b);
        }
        return ttf_buffer_append(b, surface, length);
    }
    default:
        return ttf_buffer_append(b, surface, length);
    }
}

/* Kana and kanji are this pack's own; Latin letters are kept as written, below
   the English pack's claim. The core ignores these outside a ja-JP request. */
static int script_score(ttf_script_t script) {
    if (script == TTF_SCRIPT_KANA || script == TTF_SCRIPT_HAN)
        return 100;
    return script == TTF_SCRIPT_LATIN ? 1 : 0;
}

/* A declared request routes everything but Hangul words to this pack. */
static int context_score(const ttf_pack_input_t *in) {
    if (!declared(in->engine) || in->index >= in->token_count ||
        in->tokens[in->index].script == TTF_SCRIPT_HANGUL)
        return 0;
    /* 오전 9시 in a Korean sentence, AI in an English one: their own language */
    {
        const ttf_language_pack_t *sentence = ttf_sentence_language(in);
        /* hanzi (a name: 王明이) stay with the request pack, as written */
        if (sentence && strcmp(sentence->language_tag, JA) &&
            in->tokens[in->index].script != TTF_SCRIPT_HAN &&
            in->tokens[in->index].script != TTF_SCRIPT_KANA)
            return 0;
    }
    return 1000;
}

const ttf_language_pack_t ttf_pack_ja_JP = {sizeof(ttf_language_pack_t),
                                            TTF_ABI_VERSION,
                                            JA,
                                            NULL,
                                            script_score,
                                            context_score,
                                            classify,
                                            verbalize,
                                            NULL,
                                            NULL,
                                            NULL};
