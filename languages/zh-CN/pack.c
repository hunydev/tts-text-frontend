/* Chinese (Simplified) pack, phase 1 (2026-10-11, project owner: Simplified
 * first, Claude reviews as the native speaker). Numbers read as Chinese
 * numerals (二/两, 零, 万/亿); years and codes digit by digit; dates, times,
 * durations, scores, money, percentages, fractions, ranges, units and simple
 * maths. Hanzi and Latin letters stay as written for the speech model. The pack
 * works only where zh-CN is the context: a request declared zh-CN, or a hanzi
 * sentence (context han_sentence) in another request.
 *
 * classify decides the extent and the reading mode from the words around the
 * number (a code after 验证码, a year before 年, 两 before a measure word) and
 * keeps the source in value; verbalize reads value in that mode. */
#include "tts_text_frontend/ttf_language_pack.h"
#include "../../src/core/internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZH "zh-CN"
#define TRY(x)                                                                                     \
    do {                                                                                           \
        ttf_status_t try_status = (x);                                                             \
        if (try_status != TTF_OK)                                                                  \
            return try_status;                                                                     \
    } while (0)

static const char *res(const ttf_engine_t *e, const char *kind, const char *key) {
    return ttf_resource_get(e, kind, ZH, key, NULL);
}
static int digit(char c) {
    return c >= '0' && c <= '9';
}
static int alpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
static int alnum(char c) {
    return digit(c) || alpha(c);
}
static int has(const char *t, size_t n, size_t p, const char *s) {
    size_t m = strlen(s);
    return p + m <= n && !memcmp(t + p, s, m);
}
static int prev_is(const char *t, size_t p, const char *s) {
    size_t m = strlen(s);
    return p >= m && !memcmp(t + p - m, s, m);
}
static ttf_status_t word(const ttf_engine_t *e, ttf_buffer_t *b, const char *key) {
    const char *v = res(e, "number", key);
    return v ? ttf_buffer_puts(b, v) : TTF_ERESOURCE;
}

/* ---- readings ---- */

/* One digit name; 1 is 幺 in phone numbers. */
static ttf_status_t digit_word(const ttf_engine_t *e, ttf_buffer_t *b, char c, int yao) {
    char key[3] = {'d', c, 0};
    return word(e, b, yao && c == '1' ? "yao" : key);
}

/* Digits one by one; letters stay, + is 加, separators are dropped. */
static ttf_status_t digits_each(const ttf_engine_t *e, ttf_buffer_t *b, const char *s, size_t n,
                                int yao) {
    size_t i;
    for (i = 0; i < n; ++i)
        if (digit(s[i]))
            TRY(digit_word(e, b, s[i], yao));
        else if (alpha(s[i]))
            TRY(ttf_buffer_append(b, s + i, 1));
        else if (s[i] == '+')
            TRY(word(e, b, "plus"));
    return TTF_OK;
}

/* An integer, commas ignored: 一千零五, 一万零五十, 十五, 一百一十, 十万五千. With
   liang a leading 2 before 百 千 万 亿 is 两 (两千零二十四, 两万). */
static ttf_status_t integer(const ttf_engine_t *e, ttf_buffer_t *b, const char *s, size_t n,
                            int liang) {
    static const char *const place[] = {"1000", "100", "10", ""};
    static const char *const scale[] = {"", "wan", "yi", "wanyi"};
    char d[20];
    size_t len = 0, i, g, k;
    int any = 0, gap = 0;
    for (i = 0; i < n; ++i)
        if (digit(s[i])) {
            if (len + 1 >= sizeof(d))
                return digits_each(e, b, s, n, 0);
            d[len++] = s[i];
        }
    while (len > 1 && d[0] == '0')
        memmove(d, d + 1, --len);
    if (!len)
        return TTF_EINVAL;
    if (len == 1 && d[0] == '0')
        return word(e, b, "d0");
    for (g = (len + 3) / 4; g-- > 0;) {
        char q[4];
        int inner = 0;
        for (k = 0; k < 4; ++k) {
            long at = (long)len - (long)(g + 1) * 4 + (long)k;
            q[k] = at < 0 ? '0' : d[at];
        }
        if (!memcmp(q, "0000", 4)) {
            gap = any;
            continue;
        }
        /* 一亿零五, 一万零五十: a zero group or a missing thousand is 零. */
        if (any && (gap || q[0] == '0'))
            TRY(word(e, b, "d0"));
        gap = 0;
        for (k = 0; k < 4; ++k) {
            if (q[k] == '0') {
                inner = any && k > 0 && q[k - 1] != '0' ? 1 : inner;
                continue;
            }
            if (inner)
                TRY(word(e, b, "d0"));
            inner = 0;
            if (!any && k == 2 && q[k] == '1')
                ; /* 十五, not 一十五 */
            else if (!any && liang && q[k] == '2' && (k < 2 || (k == 3 && g > 0)))
                TRY(word(e, b, "liang"));
            else
                TRY(digit_word(e, b, q[k], 0));
            if (place[k][0])
                TRY(word(e, b, place[k]));
            any = 1;
        }
        if (g)
            TRY(word(e, b, scale[g]));
    }
    return TTF_OK;
}

/* 3.14 三点一四, 0.5 零点五. */
static ttf_status_t number(const ttf_engine_t *e, ttf_buffer_t *b, const char *s, size_t n,
                           int liang) {
    size_t dot = 0;
    while (dot < n && s[dot] != '.')
        ++dot;
    if (dot == n)
        return integer(e, b, s, n, liang);
    TRY(integer(e, b, s, dot, 0));
    TRY(word(e, b, "point"));
    return digits_each(e, b, s + dot + 1, n - dot - 1, 0);
}

/* A count: 2 before a measure word is 两 (两个, 两次). */
static ttf_status_t count(const ttf_engine_t *e, ttf_buffer_t *b, const char *s, size_t n,
                          int two) {
    if (n == 1 && s[0] == '2' && two)
        return word(e, b, "liang");
    return number(e, b, s, n, 1);
}

/* ---- scanning the source text ---- */

/* Digits with optional thousands commas and a fraction: 12,345 3.14 1,280.50. */
static size_t scan_number(const char *t, size_t n, size_t p) {
    size_t q = p;
    while (q < n && digit(t[q]))
        ++q;
    if (q == p)
        return 0;
    if (q - p <= 3)
        while (q + 3 < n && t[q] == ',' && digit(t[q + 1]) && digit(t[q + 2]) &&
               digit(t[q + 3]) && !(q + 4 < n && digit(t[q + 4])))
            q += 4;
    if (q + 1 < n && t[q] == '.' && digit(t[q + 1])) {
        ++q;
        while (q < n && digit(t[q]))
            ++q;
    }
    return q - p;
}

/* The UTF-8 character at p, copied into out[5]. */
static size_t char_at(const char *t, size_t n, size_t p, char *out) {
    uint32_t cp;
    size_t w;
    out[0] = 0;
    if (p >= n || ttf_utf8_decode(t + p, n - p, &cp, &w) != 1 || w > 4)
        return 0;
    memcpy(out, t + p, w);
    out[w] = 0;
    return w;
}

/* The longest listed word (up to four characters) starting at p. */
static size_t word_after(const ttf_engine_t *e, const char *kind, const char *t, size_t n,
                         size_t p) {
    size_t ends[4], count = 0, q = p, k;
    char key[20], c[5];
    while (count < 4 && q < n) {
        size_t w = char_at(t, n, q, c);
        if (!w)
            break;
        q += w;
        ends[count++] = q;
    }
    for (k = count; k > 0; --k) {
        size_t len = ends[k - 1] - p;
        if (len >= sizeof(key))
            continue;
        memcpy(key, t + p, len);
        key[len] = 0;
        if (res(e, kind, key))
            return len;
    }
    return 0;
}

/* A listed word within the `chars` characters before p, inside the clause
   (您的验证码是123456, 急救电话是120). */
static int cue_before(const ttf_engine_t *e, const char *kind, const char *t, size_t p,
                      size_t chars) {
    size_t at[12], count = 0, i, j, q = p;
    char key[20];
    at[count++] = p;
    while (q > 0 && count <= chars && count < 12) {
        --q;
        while (q > 0 && ((unsigned char)t[q] & 0xc0) == 0x80)
            --q;
        if (t[q] == '\n' || has(t, p, q, "。") || has(t, p, q, "，") || has(t, p, q, "；"))
            break;
        at[count++] = q;
    }
    for (i = 0; i < count; ++i)
        for (j = i + 1; j < count; ++j) {
            size_t len = at[i] - at[j];
            if (len >= sizeof(key))
                break;
            memcpy(key, t + at[j], len);
            key[len] = 0;
            if (res(e, kind, key))
                return 1;
        }
    return 0;
}

static int range_mark(const char *t, size_t n, size_t p, size_t *w) {
    static const char *const marks[] = {"~", "-", "～", "—", "–", "－", "〜"};
    size_t k;
    for (k = 0; k < sizeof(marks) / sizeof(*marks); ++k)
        if (has(t, n, p, marks[k])) {
            *w = strlen(marks[k]);
            return 1;
        }
    return 0;
}

/* A unit symbol after the number: kg km/h ℃ °C m² (the longest listed form). */
static size_t unit_after(const ttf_engine_t *e, const char *t, size_t n, size_t p, char *key,
                         size_t size) {
    size_t q = p, best = 0, steps = 0;
    char c[5];
    while (q < n && steps < 6) {
        size_t w = char_at(t, n, q, c);
        if (!w || (w == 1 ? !(alpha(c[0]) || c[0] == '/')
                          : strcmp(c, "℃") && strcmp(c, "°") && strcmp(c, "²") &&
                                strcmp(c, "³")))
            break;
        q += w;
        ++steps;
        if (q - p < size) {
            memcpy(key, t + p, q - p);
            key[q - p] = 0;
            if (res(e, "zh_unit", key))
                best = q - p;
        }
    }
    if (!best || (p + best < n && alpha(t[p + best]) && alpha(t[p + best - 1])))
        return 0;
    memcpy(key, t + p, best);
    key[best] = 0;
    return best;
}

static long value_of(const char *t, size_t a, size_t b) {
    char buf[8];
    if (b <= a || b - a >= sizeof(buf))
        return -1;
    memcpy(buf, t + a, b - a);
    buf[b - a] = 0;
    return strtol(buf, NULL, 10);
}

static int is_year(const char *t, size_t p, size_t len) {
    return len == 4 && (t[p] == '1' || t[p] == '2');
}

static size_t digits_end(const char *t, size_t n, size_t p) {
    while (p < n && digit(t[p]))
        ++p;
    return p;
}

/* ---- classification ---- */

static size_t finish(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t end, const char *mode,
                     const char *value, size_t vn, const char *secondary) {
    size_t count = 0;
    if (vn >= sizeof(s->value))
        return 0;
    while (in->index + count < in->token_count && in->tokens[in->index + count].begin < end)
        ++count;
    memcpy(s->value, value, vn);
    s->value[vn] = 0;
    snprintf(s->secondary, sizeof(s->secondary), "%s", secondary);
    snprintf(s->rule, sizeof(s->rule), "zh.%s", mode);
    snprintf(s->unit, sizeof(s->unit), "@zh-%s", mode);
    snprintf(s->source_lang, sizeof(s->source_lang), ZH);
    s->input_end = end;
    s->type = TTF_SEM_COUNTER;
    return count;
}

/* Two parts and a unit joined by | for verbalize: "3|5|kg". */
static size_t finish2(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t end, const char *mode,
                      const char *t, size_t a, size_t an, size_t b, size_t bn, const char *unit,
                      const char *secondary) {
    char v[TTF_IR_VALUE_SIZE];
    int w = snprintf(v, sizeof(v), "%.*s|%.*s|%s", (int)an, t + a, (int)bn, t + b, unit);
    return w > 0 && (size_t)w < sizeof(v) ? finish(in, s, end, mode, v, (size_t)w, secondary) : 0;
}

/* x=3, √4=2, 1+1=2, 3×4=12, 10÷2=5, 5>3, 2^10, 10², 8-3=5, 1920×1080. A minus
   counts only inside an equation; without = it is a range. */
static size_t maths(const char *t, size_t n, size_t p) {
    size_t r = p;
    int ops = 0, equals = 0, strong = 0;
    if (has(t, n, r, "√"))
        r += 3, strong = 1;
    if (r + 1 < n && (t[r] == 'x' || t[r] == 'y') && t[r + 1] == '=')
        ++r;
    else if (r >= n || !digit(t[r]))
        return 0;
    else
        r += scan_number(t, n, r);
    while (r < n) {
        size_t ow = 0;
        if (has(t, n, r, "²")) {
            r += 2, ++ops, strong = 1;
            continue;
        }
        if (t[r] == '+' || t[r] == '=' || t[r] == '>' || t[r] == '<' || t[r] == '^' || t[r] == '-')
            ow = 1;
        else if (has(t, n, r, "×") || has(t, n, r, "÷"))
            ow = 2;
        if (!ow || r + ow >= n || !digit(t[r + ow]))
            break;
        if (t[r] == '-' && !equals) {
            size_t z = r + 1 + scan_number(t, n, r + 1);
            if (!(z < n && t[z] == '='))
                break;
        }
        equals |= t[r] == '=';
        strong |= t[r] != '-' && t[r] != '=';
        ++ops;
        r += ow;
        r += scan_number(t, n, r);
    }
    return ops && (equals || strong) ? r - p : 0;
}

static size_t classify(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const ttf_engine_t *e = in->engine;
    const char *t = in->text;
    size_t n = in->length, p, q, len, w, m;
    char key[24], c[5];
    if (in->index >= in->token_count)
        return 0;
    p = in->tokens[in->index].begin;
    if (p >= n)
        return 0;
    /* 加½杯: a vulgar fraction inside a hanzi run reads with the run. */
    if (in->tokens[in->index].script == TTF_SCRIPT_HAN ||
        in->tokens[in->index].script == TTF_SCRIPT_OTHER) {
        size_t z = p, end = in->tokens[in->index].end < n ? in->tokens[in->index].end : n;
        while (z < end) {
            size_t cw = char_at(t, n, z, c);
            if (!cw)
                break;
            if (res(e, "zh_vulgar", c))
                return finish(in, s, end, "hanzi", "", 0, "");
            z += cw;
        }
    }
    if (p && (alnum(t[p - 1]) || t[p - 1] == '.'))
        return 0;

    if ((m = maths(t, n, p)) != 0)
        return finish(in, s, p + m, "math", t + p, m, "");

    /* Currency before the amount: ¥128 $9.99 HK$100 €20 £50 ₩10,000. */
    if ((w = word_after(e, "zh_currency", t, n, p)) != 0 && p + w < n && digit(t[p + w])) {
        len = scan_number(t, n, p + w);
        snprintf(key, sizeof(key), "%.*s", (int)w, t + p);
        return finish(in, s, p + w + len, "money", t + p + w, len, key);
    }

    /* Letters with digits: CA1234 G20 COVID-19 PM2.5 B2B abc123 USB3.0. */
    if (alpha(t[p])) {
        int digits_seen = 0;
        q = p;
        while (q < n && (alnum(t[q]) || ((t[q] == '-' || t[q] == '.') && q + 1 < n &&
                                          alnum(t[q + 1]) && alnum(t[q - 1]))))
            digits_seen |= digit(t[q++]);
        return digits_seen ? finish(in, s, q, "latin", t + p, q - p, "") : 0;
    }

    /* +86 加八六 */
    if (t[p] == '+' && p + 1 < n && digit(t[p + 1])) {
        q = digits_end(t, n, p + 1);
        return finish(in, s, q, "digits", t + p, q - p, "");
    }

    /* ½ 二分之一 */
    if (char_at(t, n, p, c) && res(e, "zh_vulgar", c))
        return finish(in, s, p + strlen(c), "vulgar", c, strlen(c), "");

    /* 10月1日-7日: a range mark after a counted hanzi is 到. */
    if (range_mark(t, n, p, &w) && p + w < n && digit(t[p + w]) && p >= 3) {
        char prev[4];
        memcpy(prev, t + p - 3, 3);
        prev[3] = 0;
        if (((unsigned char)prev[0] & 0xf0) == 0xe0 && res(e, "range_unit", prev))
            return finish(in, s, p + w, "to", t + p, w, "");
    }

    /* -5 负五, -5℃ 零下五摄氏度, -2.3% 负百分之二点三, -5~3℃ 零下五到三摄氏度. */
    if (t[p] == '-' || has(t, n, p, "−")) {
        size_t a = p + (t[p] == '-' ? 1 : 3), l2, b2, uw;
        if (a >= n || !digit(t[a]))
            return 0;
        len = scan_number(t, n, a);
        q = a + len;
        if (range_mark(t, n, q, &w) && q + w < n && digit(t[q + w])) {
            b2 = q + w;
            l2 = scan_number(t, n, b2);
            if ((uw = unit_after(e, t, n, b2 + l2, key, sizeof(key))) != 0)
                return finish2(in, s, b2 + l2 + uw, "range", t, a, len, b2, l2, key, "neg");
        }
        if (q < n && t[q] == '%')
            return finish(in, s, q + 1, "percent", t + a, len, "neg");
        if ((uw = unit_after(e, t, n, q, key, sizeof(key))) != 0)
            return finish2(in, s, q + uw, "unit", t, a, len, a, 0, key, "neg");
        return finish(in, s, q, "neg", t + a, len, "");
    }

    if (!digit(t[p]))
        return 0;
    len = scan_number(t, n, p);
    q = p + len;

    /* Times, durations and scores: 8:30, 10:30:45, 用时2:05:30, 比分3:2, 16:9. */
    if (q + 1 < n && t[q] == ':' && digit(t[q + 1]) && len <= 2) {
        size_t mend = digits_end(t, n, q + 1), end = mend, send = 0;
        long h = value_of(t, p, q), mm = value_of(t, q + 1, mend);
        int ratio;
        if (mend + 1 < n && t[mend] == ':' && digit(t[mend + 1]) &&
            (send = digits_end(t, n, mend + 1)) - mend - 1 == 2)
            end = send;
        else
            send = 0;
        ratio = cue_before(e, "ratio_cue", t, p, 4) || mend - q - 1 != 2 || mm >= 60 || h > 24;
        if (!ratio && send && cue_before(e, "duration_cue", t, p, 4))
            return finish(in, s, end, "duration", t + p, end - p, "");
        if (!ratio) {
            /* 8:00-17:00, 9:00~11:30 */
            if (!send && range_mark(t, n, end, &w) && end + w < n && digit(t[end + w])) {
                size_t b2 = end + w, z = digits_end(t, n, b2);
                if (z - b2 <= 2 && z + 2 < n && t[z] == ':' && digit(t[z + 1]) &&
                    digit(t[z + 2]) && !(z + 3 < n && digit(t[z + 3])))
                    return finish2(in, s, z + 3, "timerange", t, p, end - p, b2, z + 3 - b2, "",
                                   "");
            }
            return finish(in, s, end, "time", t + p, end - p, "");
        }
        if (!send)
            return finish(in, s, mend, "ratio", t + p, mend - p, "");
    }

    /* Percent: 50% 百分之五十, 50%-60% 百分之五十到百分之六十, 5‰ 千分之五. */
    if (q < n && t[q] == '%') {
        if (range_mark(t, n, q + 1, &w) && q + 1 + w < n && digit(t[q + 1 + w])) {
            size_t b2 = q + 1 + w, l2 = scan_number(t, n, b2);
            if (b2 + l2 < n && t[b2 + l2] == '%')
                return finish2(in, s, b2 + l2 + 1, "pctrange", t, p, len, b2, l2, "", "");
        }
        return finish(in, s, q + 1, "percent", t + p, len, "");
    }
    if (has(t, n, q, "‰"))
        return finish(in, s, q + 3, "permille", t + p, len, "");

    /* Dates: 2024-10-11, 2024/10/11, 2024.10.11. */
    {
        size_t y = digits_end(t, n, p);
        if (y - p == 4 && y + 1 < n && (t[y] == '-' || t[y] == '/' || t[y] == '.')) {
            char sep = t[y];
            size_t me = digits_end(t, n, y + 1), de;
            long mo = value_of(t, y + 1, me), dd;
            if (mo >= 1 && mo <= 12 && me - y - 1 <= 2 && me + 1 < n && t[me] == sep) {
                de = digits_end(t, n, me + 1);
                dd = value_of(t, me + 1, de);
                if (dd >= 1 && dd <= 31 && de - me - 1 <= 2)
                    return finish2(in, s, de, "date", t, p, 4, y + 1, de - y - 1, "", "");
            }
        }
    }

    /* Fractions: 1/3 三分之一 (1又1/2: 又 stays as written). */
    if (q + 1 < n && t[q] == '/' && digit(t[q + 1]) && !memchr(t + p, '.', len)) {
        size_t b2 = q + 1, z = digits_end(t, n, b2);
        if (!(z < n && (t[z] == '/' || t[z] == '.')))
            return finish2(in, s, z, "frac", t, p, len, b2, z - b2, "", "");
    }

    /* Phone numbers: 010-12345678, 400-123-4567, 13812345678. */
    {
        size_t z = p, groups = 0, gl[4] = {0};
        while (groups < 4) {
            size_t g0 = z;
            z = digits_end(t, n, z);
            gl[groups++] = z - g0;
            if (z + 1 < n && t[z] == '-' && digit(t[z + 1]))
                ++z;
            else
                break;
        }
        if ((groups == 2 && t[p] == '0' && gl[0] >= 3 && gl[0] <= 4 && gl[1] >= 7 && gl[1] <= 8) ||
            (groups == 3 && gl[0] == 3 && gl[1] == 3 && gl[2] == 4) ||
            (groups == 1 && gl[0] == 11 && t[p] == '1'))
            return finish(in, s, z, "digits", t + p, z - p, "yao");
    }

    /* Ranges: 3-5天 10~20人 2010-2020年 20-25℃ 1-2个 18—35岁. */
    if (range_mark(t, n, q, &w) && q + w < n && digit(t[q + w])) {
        size_t b2 = q + w, l2 = scan_number(t, n, b2), uw;
        uw = unit_after(e, t, n, b2 + l2, key, sizeof(key));
        if (is_year(t, p, len) && is_year(t, b2, l2) && word_after(e, "year_after", t, n, b2 + l2))
            return finish2(in, s, b2 + l2, "yearrange", t, p, len, b2, l2, "", "");
        if (!uw)
            key[0] = 0;
        return finish2(in, s, b2 + l2 + uw, "range", t, p, len, b2, l2, key,
                       word_after(e, "liang_counter", t, n, b2 + l2 + uw) ? "liang" : "");
    }

    /* A unit symbol: 5kg 2L 220V 25℃ 60km/h 100m². */
    {
        size_t uw = unit_after(e, t, n, q, key, sizeof(key));
        if (uw)
            return finish2(in, s, q + uw, "unit", t, p, len, p, 0, key, "");
    }

    /* Letters right after the digits: 5G 3D 4K 123X. */
    if (q < n && alpha(t[q])) {
        size_t z = q;
        while (z < n && alnum(t[z]))
            ++z;
        return finish(in, s, z, "latin", t + p, z - p, "");
    }

    /* A plain number: its reading depends on the words around it. */
    if (memchr(t + p, '.', len) || memchr(t + p, ',', len))
        return finish(in, s, q, "int", t + p, len, "");
    if (prev_is(t, p, "第"))
        return finish(in, s, q, "int", t + p, len, "");
    if (len >= 3 && (cue_before(e, "phone_cue", t, p, 5) || word_after(e, "phone_after", t, n, q)))
        return finish(in, s, q, "digits", t + p, len, "yao");
    /* 验证码123456, 301房间, 0571: a code reads digit by digit. */
    if ((len >= 3 && (cue_before(e, "code_cue", t, p, 6) || word_after(e, "room_after", t, n, q))) ||
        (len >= 2 && t[p] == '0'))
        return finish(in, s, q, "digits", t + p, len, "");
    /* 1990年, 1998那年, 1980年代 (not 2000年前, 2000年历史): a year. */
    if (is_year(t, p, len) && word_after(e, "year_after", t, n, q) &&
        !word_after(e, "year_not", t, n, q))
        return finish(in, s, q, "year", t + p, len, "");
    return finish(in, s, q, "int", t + p, len,
                  word_after(e, "liang_counter", t, n, q) ? "liang" : "count");
}

/* ---- verbalization ---- */

/* 8:30 八点三十分, 14:05 十四点零五分, 12:00 十二点; a duration 两小时零五分三十秒. */
static ttf_status_t clock(const ttf_engine_t *e, ttf_buffer_t *b, const char *v, size_t vn,
                          int duration) {
    const char *m = memchr(v, ':', vn), *sec;
    long mm, ss = 0;
    char x[8];
    if (!m)
        return TTF_EINVAL;
    sec = memchr(m + 1, ':', vn - (size_t)(m + 1 - v));
    TRY(count(e, b, v, (size_t)(m - v), 1));
    TRY(word(e, b, duration ? "hours" : "oclock"));
    mm = strtol(m + 1, NULL, 10);
    if (sec)
        ss = strtol(sec + 1, NULL, 10);
    if (mm) {
        if (mm < 10)
            TRY(word(e, b, "d0"));
        snprintf(x, sizeof(x), "%ld", mm);
        TRY(integer(e, b, x, strlen(x), 0));
        TRY(word(e, b, "minutes"));
    }
    if (ss) {
        snprintf(x, sizeof(x), "%ld", ss);
        TRY(integer(e, b, x, strlen(x), 0));
        TRY(word(e, b, "seconds"));
    }
    return TTF_OK;
}

static ttf_status_t maths_reading(const ttf_engine_t *e, ttf_buffer_t *b, const char *v) {
    size_t i = 0, n = strlen(v);
    while (i < n) {
        if (digit(v[i])) {
            size_t z = i;
            while (z < n && (digit(v[z]) || v[z] == '.' || v[z] == ','))
                ++z;
            TRY(number(e, b, v + i, z - i, 0));
            i = z;
            if (i < n && v[i] == '^') {
                z = ++i;
                while (z < n && digit(v[z]))
                    ++z;
                TRY(word(e, b, "power"));
                TRY(number(e, b, v + i, z - i, 0));
                TRY(word(e, b, "power_end"));
                i = z;
            }
            continue;
        }
        {
            static const struct {
                const char *mark, *key;
            } ops[] = {{"²", "square"}, {"×", "times"},   {"÷", "divide"}, {"√", "root"},
                       {"+", "plus"},   {"-", "subtract"}, {"=", "equals"}, {">", "greater"},
                       {"<", "less"}};
            size_t k;
            for (k = 0; k < sizeof(ops) / sizeof(*ops) && !has(v, n, i, ops[k].mark); ++k)
                ;
            if (k < sizeof(ops) / sizeof(*ops)) {
                TRY(word(e, b, ops[k].key));
                i += strlen(ops[k].mark);
                continue;
            }
        }
        if (alpha(v[i]))
            TRY(ttf_buffer_append(b, v + i, 1));
        ++i;
    }
    return TTF_OK;
}

/* CA1234 CA一二三四, G20 G二十, COVID-19 COVID十九, PM2.5 PM二点五, 123X 一二三X. */
static ttf_status_t latin(const ttf_engine_t *e, ttf_buffer_t *b, const char *v) {
    size_t i = 0, n = strlen(v);
    while (i < n) {
        if (alpha(v[i])) {
            TRY(ttf_buffer_append(b, v + i, 1));
            ++i;
        } else if (digit(v[i])) {
            size_t z = i;
            while (z < n && digit(v[z]))
                ++z;
            if (z + 1 < n && v[z] == '.' && digit(v[z + 1])) {
                ++z;
                while (z < n && digit(v[z]))
                    ++z;
                TRY(number(e, b, v + i, z - i, 0));
            } else if (z - i >= 3 || (v[i] == '0' && z - i > 1))
                TRY(digits_each(e, b, v + i, z - i, 0));
            else
                TRY(integer(e, b, v + i, z - i, 0));
            i = z;
        } else
            ++i;
    }
    return TTF_OK;
}

/* "a|b|unit" into its parts. */
static int split(const char *v, const char **a, size_t *an, const char **bp, size_t *bn,
                 const char **u) {
    const char *bar = strchr(v, '|'), *bar2 = bar ? strchr(bar + 1, '|') : NULL;
    if (!bar2)
        return 0;
    *a = v, *an = (size_t)(bar - v);
    *bp = bar + 1, *bn = (size_t)(bar2 - bar - 1);
    *u = bar2 + 1;
    return 1;
}

static ttf_status_t unit_word(const ttf_engine_t *e, ttf_buffer_t *b, const char *unit) {
    const char *u = unit[0] ? res(e, "zh_unit", unit) : "";
    return u ? ttf_buffer_puts(b, u) : TTF_ERESOURCE;
}

/* A negative temperature is 零下, any other negative 负. */
static ttf_status_t minus(const ttf_engine_t *e, ttf_buffer_t *b, const char *unit) {
    const char *u = unit[0] ? res(e, "zh_unit", unit) : NULL;
    return word(e, b, u && strstr(u, "摄氏度") ? "below_zero" : "minus");
}

static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *b) {
    const char *mode = s->unit, *v = s->value, *a, *bp, *u;
    size_t vn = strlen(v), an, bn;
    if (strncmp(mode, "@zh-", 4)) {
        /* Shared objects in a Chinese sentence. */
        if (s->type == TTF_SEM_PHONE || s->type == TTF_SEM_DIGIT_SEQUENCE)
            return digits_each(e, b, surface, length, s->type == TTF_SEM_PHONE);
        if ((s->type == TTF_SEM_CARDINAL || s->type == TTF_SEM_NUMBER) && vn &&
            strspn(v, "0123456789.,") == vn)
            return number(e, b, v, vn, 1);
        return ttf_buffer_append(b, surface, length);
    }
    mode += 4;
    if (!strcmp(mode, "int"))
        return !strcmp(s->secondary, "liang") ? count(e, b, v, vn, 1)
                                              : number(e, b, v, vn, !strcmp(s->secondary, "count"));
    if (!strcmp(mode, "digits"))
        return digits_each(e, b, v, vn, !strcmp(s->secondary, "yao"));
    if (!strcmp(mode, "year"))
        return digits_each(e, b, v, vn, 0);
    if (!strcmp(mode, "time") || !strcmp(mode, "duration"))
        return clock(e, b, v, vn, !strcmp(mode, "duration"));
    if (!strcmp(mode, "ratio")) {
        const char *m = strchr(v, ':');
        if (!m)
            return TTF_EINVAL;
        TRY(number(e, b, v, (size_t)(m - v), 0));
        TRY(word(e, b, "ratio"));
        return number(e, b, m + 1, strlen(m + 1), 0);
    }
    if (!strcmp(mode, "percent")) {
        if (!strcmp(s->secondary, "neg"))
            TRY(word(e, b, "minus"));
        TRY(word(e, b, "percent"));
        return number(e, b, v, vn, 0);
    }
    if (!strcmp(mode, "permille")) {
        TRY(word(e, b, "permille"));
        return number(e, b, v, vn, 0);
    }
    if (!strcmp(mode, "vulgar")) {
        const char *r = res(e, "zh_vulgar", v);
        return r ? ttf_buffer_puts(b, r) : TTF_ERESOURCE;
    }
    if (!strcmp(mode, "neg")) {
        TRY(word(e, b, "minus"));
        return number(e, b, v, vn, 0);
    }
    if (!strcmp(mode, "money")) {
        const char *cur = res(e, "zh_currency", s->secondary);
        if (!cur)
            return TTF_ERESOURCE;
        TRY(number(e, b, v, vn, 1));
        return ttf_buffer_puts(b, cur);
    }
    if (!strcmp(mode, "math"))
        return maths_reading(e, b, v);
    if (!strcmp(mode, "latin"))
        return latin(e, b, v);
    if (!strcmp(mode, "to"))
        return word(e, b, "range");
    if (!strcmp(mode, "hanzi")) {
        size_t i = 0;
        char ch[5];
        while (i < length) {
            size_t cw = char_at(surface, length, i, ch);
            const char *r = cw ? res(e, "zh_vulgar", ch) : NULL;
            if (!cw)
                return ttf_buffer_append(b, surface + i, length - i);
            TRY(r ? ttf_buffer_puts(b, r) : ttf_buffer_append(b, surface + i, cw));
            i += cw;
        }
        return TTF_OK;
    }
    if (!split(v, &a, &an, &bp, &bn, &u))
        return TTF_EINVAL;
    if (!strcmp(mode, "unit")) {
        if (!strcmp(s->secondary, "neg"))
            TRY(minus(e, b, u));
        /* a measurement keeps 二: 二百二十伏 */
        TRY(number(e, b, a, an, 0));
        return unit_word(e, b, u);
    }
    if (!strcmp(mode, "range")) {
        if (!strcmp(s->secondary, "neg"))
            TRY(minus(e, b, u));
        TRY(number(e, b, a, an, 0));
        TRY(word(e, b, "range"));
        TRY(count(e, b, bp, bn, !strcmp(s->secondary, "liang")));
        return unit_word(e, b, u);
    }
    if (!strcmp(mode, "pctrange")) {
        TRY(word(e, b, "percent"));
        TRY(number(e, b, a, an, 0));
        TRY(word(e, b, "range"));
        TRY(word(e, b, "percent"));
        return number(e, b, bp, bn, 0);
    }
    if (!strcmp(mode, "yearrange")) {
        TRY(digits_each(e, b, a, an, 0));
        TRY(word(e, b, "range"));
        return digits_each(e, b, bp, bn, 0);
    }
    if (!strcmp(mode, "timerange")) {
        TRY(clock(e, b, a, an, 0));
        TRY(word(e, b, "range"));
        return clock(e, b, bp, bn, 0);
    }
    if (!strcmp(mode, "frac")) {
        TRY(number(e, b, bp, bn, 0));
        TRY(word(e, b, "fraction"));
        return number(e, b, a, an, 0);
    }
    if (!strcmp(mode, "date")) {
        /* a = YYYY, b = M<sep>D */
        size_t sep = 0;
        while (sep < bn && digit(bp[sep]))
            ++sep;
        if (sep == bn)
            return TTF_EINVAL;
        TRY(digits_each(e, b, a, an, 0));
        TRY(word(e, b, "year"));
        TRY(integer(e, b, bp, sep, 0));
        TRY(word(e, b, "month"));
        TRY(integer(e, b, bp + sep + 1, bn - sep - 1, 0));
        return word(e, b, "day");
    }
    return TTF_EINVAL;
}

/* Hanzi are this pack's own; Latin letters stay below the English pack's claim. */
static int script_score(ttf_script_t script) {
    if (script == TTF_SCRIPT_HAN)
        return 100;
    return script == TTF_SCRIPT_LATIN ? 1 : 0;
}

/* A declared request routes everything but Hangul and kana to this pack. */
static int context_score(const ttf_pack_input_t *in) {
    const ttf_engine_t *e = in->engine;
    if (!e || !e->default_language || strcmp(e->default_language, ZH) ||
        in->index >= in->token_count || in->tokens[in->index].script == TTF_SCRIPT_HANGUL ||
        in->tokens[in->index].script == TTF_SCRIPT_KANA)
        return 0;
    return 1000;
}

const ttf_language_pack_t ttf_pack_zh_CN = {sizeof(ttf_language_pack_t),
                                            TTF_ABI_VERSION,
                                            ZH,
                                            NULL,
                                            script_score,
                                            context_score,
                                            classify,
                                            verbalize,
                                            NULL,
                                            NULL,
                                            NULL};
