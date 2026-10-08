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
    int negative, decimal;
    uint64_t value;         /* whole value, or the integer part of a decimal */
    char fraction[32];      /* decimal digits after the point */
    const char *scale_word; /* 1.5万: a decimal before a written scale */
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
    if (p == (size_t)out->negative)
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

/* Bare number reading: いってんご, マイナスご, にせんにじゅうよん. */
static ttf_status_t number_reading(const ttf_engine_t *e, const ja_number_t *x, ttf_buffer_t *b) {
    ttf_status_t st;
    size_t k;
    if (x->negative && (st = word(e, b, "number", "minus")) != TTF_OK)
        return st;
    if (!x->decimal)
        return integer_reading(e, x->value, b);
    if (!x->value && !x->scale_word)
        st = word(e, b, "number", "zero_integer");
    else
        st = integer_reading(e, x->value, b);
    if (st != TTF_OK)
        return st;
    /* 1.5 いってんご, 10.5 じゅってんご */
    if (x->value && (x->value % 10 == 1 || x->value % 100 == 10) && (st = geminate(b)) != TTF_OK)
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
    if (x->decimal || x->negative) {
        if ((st = number_reading(e, x, b)) != TTF_OK)
            return st;
        return ttf_buffer_puts(b, reading);
    }
    owner = key;
    if (date_day) {
        snprintf(special_key, sizeof(special_key), "date_day:%llu", (unsigned long long)x->value);
        special = res(e, "counter_reading", special_key);
    }
    if (!special) {
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
        if (!strcmp(cls, "p10")) {
            if (end == 10 && (st = geminate(b)) != TTF_OK)
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
    for (k = count; k > 0; --k) {
        if (widths[k - 1] >= size)
            continue;
        memcpy(key, s, widths[k - 1]);
        key[widths[k - 1]] = 0;
        if (res(e, "counter", key))
            return widths[k - 1];
    }
    return 0;
}

static int range_mark(const char *s, size_t n, size_t *width) {
    if (n >= 1 && s[0] == '~') {
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

/* 3本, 2024年10月8日, 4時15分, 1万円, 3〜5個, -5度: digits with written counters
 * become one counted span; the value keeps the normalized source. */
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
        if (ok && letters && digits && q - begin <= 10 &&
            (q >= n || (text[q] != '.' && text[q] != ','))) {
            snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - begin), text + begin);
            return finish(in, s, q, TTF_SEM_ALNUM_TERM, "ja.alnum", "");
        }
    }
    if (!digit(text[begin]) && !(text[begin] == '-' && begin + 1 < n && digit(text[begin + 1])))
        return 0;
    /* 内線214, 請求書1111: after a number label the digits read one by one. */
    if (digit(text[begin])) {
        size_t k, q = begin;
        while (q < n && digit(text[q]))
            ++q;
        for (k = 3; k <= 15 && k <= begin; k += 3) {
            char cue[16];
            memcpy(cue, text + begin - k, k);
            cue[k] = 0;
            if (q - begin >= 2 && q - begin < sizeof(s->value) && res(e, "digit_cue", cue) &&
                (q >= n || !(text[q] == '.' || text[q] == ','))) {
                snprintf(s->value, sizeof(s->value), "%.*s", (int)(q - begin), text + begin);
                return finish(in, s, q, TTF_SEM_DIGIT_SEQUENCE, "ja.digit-cue", "");
            }
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
    return finish(in, s, p, TTF_SEM_COUNTER, "ja.counter", "@ja");
}

/* Re-read a classified span: pairs of number and counter, a range, a scale. */
static ttf_status_t render_chain(const ttf_engine_t *e, const char *v, ttf_buffer_t *b) {
    size_t n = strlen(v), p = 0;
    char previous[32] = "";
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
        if (!used)
            return TTF_EINVAL;
        p += used;
        if (range_mark(v + p, n - p, &rw)) {
            if ((st = number_reading(e, &x, b)) != TTF_OK ||
                (st = word(e, b, "number", "range")) != TTF_OK)
                return st;
            p += rw;
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
        st = counter_named(e, &x, key, !strcmp(previous, "月") && !strcmp(key, "日"), b);
        if (st != TTF_OK)
            return st;
        snprintf(previous, sizeof(previous), "%s", key);
        p += cw;
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
static ttf_status_t alnum(const ttf_engine_t *e, const char *s, size_t n, ttf_buffer_t *b) {
    size_t i = 0;
    ttf_status_t st;
    while (i < n) {
        size_t j = i;
        if (digit(s[i])) {
            char value[24];
            while (j < n && digit(s[j]))
                ++j;
            if (j - i <= 4) {
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
            st = (upper && j - i <= 5) || (j - i <= 3 && j < n && digit(s[j]))
                     ? letters(e, s + i, j - i, b)
                     : ttf_buffer_append(b, s + i, j - i);
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
        if (!strcmp(s->unit, "@ja"))
            return render_chain(e, s->value, b);
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
        if (s->unit[0])
            return TTF_EINVAL;
        return date_reading(e, s->value, b);
    case TTF_SEM_FRACTION:
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
        else if (s->unit[0] && !(counter = currency_counter(e, s->unit)))
            return TTF_EINVAL;
        if ((st = plain_number(e, s->value, b)) != TTF_OK ||
            (st = word(e, b, "number", "range")) != TTF_OK)
            return st;
        return counter ? value_counted(e, s->secondary, counter, 0, b)
                       : plain_number(e, s->secondary, b);
    }
    case TTF_SEM_VERSION:
    case TTF_SEM_IP_ADDRESS: {
        size_t i = 0, n = strlen(s->value);
        while (i < n && alpha(s->value[i]))
            if ((st = letters(e, s->value + i++, 1, b)) != TTF_OK)
                return st;
        while (i < n) {
            size_t j = i;
            char part[24];
            while (j < n && digit(s->value[j]))
                ++j;
            if (j == i || j - i >= sizeof(part))
                return TTF_EINVAL;
            snprintf(part, sizeof(part), "%.*s", (int)(j - i), s->value + i);
            if ((st = plain_number(e, part, b)) != TTF_OK)
                return st;
            if (j < n) {
                if (s->value[j] != '.' ||
                    (st = word(e, b, "number", "version_separator")) != TTF_OK)
                    return st == TTF_OK ? TTF_EINVAL : st;
                ++j;
            }
            i = j;
        }
        return TTF_OK;
    }
    case TTF_SEM_INITIALISM:
        if (letters(e, surface, length, b) == TTF_OK)
            return TTF_OK;
        return ttf_buffer_append(b, surface, length);
    case TTF_SEM_ALNUM_TERM:
        return alnum(e, surface, length, b);
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
        if (!strcmp(s->unit, "@counter"))
            return TTF_EINVAL;
        if (!strcmp(s->unit, "@email") || !strcmp(s->unit, "@url"))
            return ttf_electronic_control(e, s, surface, length, b);
        return ttf_semantic_control(e, s, surface, length, b);
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
