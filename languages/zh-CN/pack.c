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
                                strcmp(c, "³") && strcmp(c, "·") && strcmp(c, "㎡") &&
                                strcmp(c, "㎞") && strcmp(c, "μ")))
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
    /* 88V61: one capital letter inside a code is no unit */
    if (best == 1 && t[p] >= 'A' && t[p] <= 'Z' && p + 1 < n && digit(t[p + 1]))
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

/* Maths symbols and the words they read (2026-10-11, phase 2). REL marks a
   relation (an expression with one is an equation or inequality), STRONG an
   operator that alone makes an expression (3×4, 2^10, 10², √4). */
enum { REL = 1, STRONG = 2, PREFIX = 4, POSTFIX = 8 };
static const struct {
    const char *mark, *key;
    int kind;
} SYMBOLS[] = {{"=", "equals", REL},          {">", "greater", REL},
               {"<", "less", REL},            {"≥", "greater_eq", REL},
               {"≤", "less_eq", REL},         {"≠", "not_equal", REL},
               {"≈", "approx_equal", REL},    {"×", "times", STRONG},
               {"÷", "divide", STRONG},       {"^", "power", STRONG},
               {"+", "plus", 0},              {"-", "subtract", 0},
               {"√", "root", STRONG | PREFIX}, {"∛", "cube_root", STRONG | PREFIX},
               {"∠", "angle", PREFIX},        {"π", "pi", PREFIX},
               {"°", "degree", POSTFIX},      {"(", "", 0},
               {")", "", 0}};

static int symbol_at(const char *t, size_t n, size_t p, size_t *w) {
    size_t k;
    for (k = 0; k < sizeof(SYMBOLS) / sizeof(*SYMBOLS); ++k)
        if (has(t, n, p, SYMBOLS[k].mark)) {
            *w = strlen(SYMBOLS[k].mark);
            return (int)k;
        }
    return -1;
}

/* A superscript digit or minus at p: its ASCII form, 0 if none. */
static char superscript(const char *t, size_t n, size_t p, size_t *w) {
    static const char *const sup[] = {"⁰", "¹", "²", "³", "⁴", "⁵", "⁶", "⁷", "⁸", "⁹"};
    size_t k;
    for (k = 0; k < 10; ++k)
        if (has(t, n, p, sup[k])) {
            *w = strlen(sup[k]);
            return (char)('0' + k);
        }
    if (has(t, n, p, "⁻")) {
        *w = 3;
        return '-';
    }
    return 0;
}

/* sin cos tan log ln: a function name read as letters. */
static size_t function_name(const char *t, size_t n, size_t p) {
    static const char *const names[] = {"sin", "cos", "tan", "log", "ln"};
    size_t k;
    for (k = 0; k < sizeof(names) / sizeof(*names); ++k) {
        size_t m = strlen(names[k]);
        if (has(t, n, p, names[k]) && !(p + m < n && alpha(t[p + m])))
            return m;
    }
    return 0;
}

/* An expression: x=3, y=2x+3, a²+b²=c², √4=2, 3×4=12, 10÷2=5, 5>3, x≥5, 2^10,
   10², 6.02×10²³, π≈3.14, ∠A=60°, sin30°=1/2, 1/4+1/4=1/2, 8-3=5, 1920×1080.
   A letter is a variable only when one letter stands alone (not 3D, PM2.5,
   km/h); a minus without a relation is a range (3-5), and a bare fraction or
   number is not maths. */
static size_t maths(const ttf_engine_t *e, const char *t, size_t n, size_t p) {
    size_t r = p, w, last = p;
    char key[24];
    /* an operator counts once an operand follows it (5> 以上 is not maths) */
    int rel = 0, strong = 0, minus = 0, prel = 0, pstrong = 0, pminus = 0, digits = 0;
    int letter_start = alpha(t[p]), k;
    if (p >= n || (!digit(t[p]) && !alpha(t[p]) &&
                   ((k = symbol_at(t, n, p, &w)) < 0 || !(SYMBOLS[k].kind & PREFIX))))
        return 0;
    while (r < n) {
        int operand = 0;
        if (digit(t[r])) {
            while (r < n && digit(t[r]))
                ++r;
            if (r + 1 < n && t[r] == '.' && digit(t[r + 1]))
                for (++r; r < n && digit(t[r]); ++r)
                    ;
            /* 1/4: a fraction inside an expression */
            if (r + 1 < n && t[r] == '/' && digit(t[r + 1]))
                for (++r; r < n && digit(t[r]); ++r)
                    ;
            digits = operand = 1;
        } else if (alpha(t[r])) {
            size_t f = function_name(t, n, r);
            if (f) {
                r += f;
                continue;
            }
            /* one letter followed by no letter and no slash (m/s is a unit), and
               not a unit after a number (100m², 220V) */
            if ((r + 1 < n && (alpha(t[r + 1]) || t[r + 1] == '/')) || (r > p && alpha(t[r - 1])) ||
                (r > p && digit(t[r - 1]) && unit_after(e, t, n, r, key, sizeof(key))))
                break;
            ++r;
            operand = 1;
        } else if (superscript(t, n, r, &w)) {
            r += w;
            strong = digits = operand = 1;
        } else if ((k = symbol_at(t, n, r, &w)) >= 0) {
            if (SYMBOLS[k].mark[0] == '^' && !(r + 1 < n && digit(t[r + 1])))
                break;
            r += w;
            if (SYMBOLS[k].kind & (PREFIX | POSTFIX) || SYMBOLS[k].mark[0] == ')') {
                strong |= (SYMBOLS[k].kind & STRONG) != 0;
                operand = (SYMBOLS[k].kind & POSTFIX) || SYMBOLS[k].mark[0] == ')';
                if (!operand)
                    continue;
            } else {
                prel |= SYMBOLS[k].kind & REL;
                pstrong |= (SYMBOLS[k].kind & STRONG) || SYMBOLS[k].mark[0] == '+';
                pminus |= SYMBOLS[k].mark[0] == '-';
                continue;
            }
        } else
            break;
        if (operand) {
            rel |= prel, strong |= pstrong, minus |= pminus;
            prel = pstrong = pminus = 0;
            last = r;
        }
    }
    if (!digits || !(rel || strong) || (minus && !rel) || (letter_start && !rel))
        return 0;
    return last - p;
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
            /* 恤￥89: a currency sign at the end of a hanzi run */
            if (z + cw == end && end < n && digit(t[end]) && res(e, "zh_currency", c)) {
                len = scan_number(t, n, end);
                return finish2(in, s, end + len, "hanmoney", t, p, z - p, end, len, c, "");
            }
            /* 圆周率π≈3.14: an expression starting inside a hanzi run */
            if (z > p && symbol_at(t, n, z, &w) >= 0 && (m = maths(e, t, n, z)) != 0 && z + m > end)
                return finish2(in, s, z + m, "hanmath", t, p, z - p, z, m, "", "");
            z += cw;
        }
    }
    /* 沪B·6Y819: the middle dot of a plate goes with the code after it */
    if (has(t, n, p, "·") && p && alpha(t[p - 1]) && p + 2 < n && digit(t[p + 2])) {
        q = p + 2;
        while (q < n && alnum(t[q]))
            ++q;
        return finish(in, s, q, "digits", t + p, q - p, "");
    }
    if (p && (alnum(t[p - 1]) || t[p - 1] == '.'))
        return 0;

    /* an expression may end in a unit: 3×10⁸m/s */
    if ((m = maths(e, t, n, p)) != 0) {
        size_t uw = unit_after(e, t, n, p + m, key, sizeof(key));
        if (!uw)
            key[0] = 0;
        return finish(in, s, p + m + uw, "math", t + p, m, key);
    }

    /* Currency before the amount: ¥128 $9.99 HK$100 €20 £50 ₩10,000; a range
       ¥80～¥120 八十元到一百二十元. */
    if ((w = word_after(e, "zh_currency", t, n, p)) != 0 && p + w < n && digit(t[p + w])) {
        size_t w2;
        len = scan_number(t, n, p + w);
        q = p + w + len;
        snprintf(key, sizeof(key), "%.*s", (int)w, t + p);
        if (range_mark(t, n, q, &w2)) {
            size_t b2 = q + w2, cw = word_after(e, "zh_currency", t, n, b2);
            if (cw == w && !memcmp(t + b2, t + p, w) && b2 + cw < n && digit(t[b2 + cw]))
                b2 += cw;
            if (b2 < n && digit(t[b2])) {
                size_t l2 = scan_number(t, n, b2);
                return finish2(in, s, b2 + l2, "moneyrange", t, p + w, len, b2, l2, key, "");
            }
        }
        return finish(in, s, q, "money", t + p + w, len, key);
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
        if (has(t, n, q, "‰"))
            return finish(in, s, q + 3, "permille", t + a, len, "neg");
        if ((uw = unit_after(e, t, n, q, key, sizeof(key))) != 0) {
            /* -10℃~45℃ 零下十摄氏度到四十五摄氏度 */
            char u1[24];
            snprintf(u1, sizeof(u1), "-%s", key);
            if (range_mark(t, n, q + uw, &w) && q + uw + w < n && digit(t[q + uw + w])) {
                b2 = q + uw + w;
                l2 = scan_number(t, n, b2);
                if ((uw = unit_after(e, t, n, b2 + l2, key, sizeof(key))) != 0)
                    return finish2(in, s, b2 + l2 + uw, "range2", t, a, len, b2, l2, key, u1);
                uw = unit_after(e, t, n, q, key, sizeof(key));
            }
            return finish2(in, s, q + uw, "unit", t, a, len, a, 0, key, "neg");
        }
        return finish(in, s, q, "neg", t + a, len, "");
    }

    if (!digit(t[p]))
        return 0;
    len = scan_number(t, n, p);
    q = p + len;

    /* 沪B·6Y819: a plate after the middle dot reads digit by digit. */
    if (prev_is(t, p, "·")) {
        size_t z = p;
        while (z < n && alnum(t[z]))
            ++z;
        return finish(in, s, z, "digits", t + p, z - p, "");
    }
    /* 5·12 五一二, 12·9 一二九: a date name with a middle dot. */
    if (has(t, n, q, "·") && q + 2 < n && digit(t[q + 2]) && !memchr(t + p, '.', len)) {
        size_t z = digits_end(t, n, q + 2);
        return finish(in, s, z, "digits", t + p, z - p, "");
    }
    /* 10.1假期 十一, 5.1小长假 五一: a holiday named by its date. */
    if (memchr(t + p, '.', len) && word_after(e, "holiday_after", t, n, q)) {
        const char *dot = memchr(t + p, '.', len);
        long mo = value_of(t, p, (size_t)(dot - t)), dd = value_of(t, (size_t)(dot - t) + 1, q);
        if (mo >= 1 && mo <= 12 && dd >= 1 && dd <= 31 && q - (size_t)(dot - t) <= 3)
            return finish2(in, s, q, "holiday", t, p, (size_t)(dot - t) - p, (size_t)(dot - t) + 1,
                           q - (size_t)(dot - t) - 1, "", "");
    }
    /* 2½ 二又二分之一 */
    if (char_at(t, n, q, c) && res(e, "zh_vulgar", c))
        return finish2(in, s, q + strlen(c), "mixedfrac", t, p, len, q, strlen(c), "", "");

    /* Ratios and scores the clock cannot be: 2∶1 (the ratio colon), 1.5:1,
       115:109, 3:2:1. */
    {
        char v[TTF_IR_VALUE_SIZE];
        size_t z = q, vn = len, parts = 1, cw, l;
        int full = 0, two = 1, decimal = memchr(t + p, '.', len) != NULL;
        if (len < sizeof(v)) {
            memcpy(v, t + p, len);
            while ((cw = has(t, n, z, ":") ? 1 : has(t, n, z, "∶") ? 3 : 0) != 0 && z + cw < n &&
                   digit(t[z + cw])) {
                l = scan_number(t, n, z + cw);
                if (vn + l + 2 >= sizeof(v))
                    break;
                full |= cw == 3;
                two &= l == 2;
                decimal |= memchr(t + z + cw, '.', l) != NULL;
                v[vn++] = ':';
                memcpy(v + vn, t + z + cw, l);
                vn += l;
                z += cw + l;
                ++parts;
            }
            if (parts >= 2 && (full || decimal || len >= 3 || (parts >= 3 && !(two && len <= 2))))
                return finish(in, s, z, "ratio", v, vn, "");
        }
    }

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
        /* 时长4:05 四分零五秒: the length of a song or a video */
        if (!ratio && !send && cue_before(e, "length_cue", t, p, 4))
            return finish(in, s, end, "minsec", t + p, end - p, "");
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

    /* Fractions: 1/3 三分之一 (1又1/2: 又 stays as written). A month/day near a
       date word is a date when it cannot be a common fraction (12/3, 9/18). */
    if (q + 1 < n && t[q] == '/' && digit(t[q + 1]) && !memchr(t + p, '.', len)) {
        size_t b2 = q + 1, z = digits_end(t, n, b2);
        long a = value_of(t, p, q), d = value_of(t, b2, z);
        if (a >= 1 && a <= 12 && d >= 1 && d <= 31 && (a > d || d > 12) &&
            (cue_before(e, "date_cue", t, p, 4) || word_after(e, "date_after", t, n, z)))
            return finish2(in, s, z, "monthday", t, p, len, b2, z - b2, "", "");
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

    /* Ranges: 3-5天 10~20人 2010-2020年 20-25℃ 1-2个 18—35岁 0-100%; a score
       以7-5赢下 七比五. */
    if (range_mark(t, n, q, &w) && q + w < n && digit(t[q + w])) {
        size_t b2 = q + w, l2 = scan_number(t, n, b2), uw;
        if (t[q] == '-' && cue_before(e, "ratio_cue", t, p, 1) &&
            word_after(e, "ratio_after", t, n, b2 + l2)) {
            char v[24];
            snprintf(v, sizeof(v), "%.*s:%.*s", (int)len, t + p, (int)l2, t + b2);
            return finish(in, s, b2 + l2, "ratio", v, strlen(v), "");
        }
        if (b2 + l2 < n && t[b2 + l2] == '%')
            return finish2(in, s, b2 + l2 + 1, "pctrange2", t, p, len, b2, l2, "", "");
        uw = unit_after(e, t, n, b2 + l2, key, sizeof(key));
        if (is_year(t, p, len) && is_year(t, b2, l2) && word_after(e, "year_after", t, n, b2 + l2))
            return finish2(in, s, b2 + l2, "yearrange", t, p, len, b2, l2, "", "");
        if (!uw)
            key[0] = 0;
        return finish2(in, s, b2 + l2 + uw, "range", t, p, len, b2, l2, key,
                       word_after(e, "liang_counter", t, n, b2 + l2 + uw) ? "liang" : "");
    }

    /* A unit symbol: 5kg 2L 220V 25℃ 60km/h 100m²; 20℃~30℃ with a unit on
       both ends. */
    {
        size_t uw = unit_after(e, t, n, q, key, sizeof(key));
        if (uw) {
            if (range_mark(t, n, q + uw, &w) && q + uw + w < n && digit(t[q + uw + w])) {
                size_t b2 = q + uw + w, l2 = scan_number(t, n, b2), uw2;
                char u1[24];
                snprintf(u1, sizeof(u1), "%s", key);
                if ((uw2 = unit_after(e, t, n, b2 + l2, key, sizeof(key))) != 0)
                    return finish2(in, s, b2 + l2 + uw2, "range2", t, p, len, b2, l2, key, u1);
                snprintf(key, sizeof(key), "%s", u1);
            }
            /* 2h15min 两小时十五分钟, 1m80 is not a compound (no second unit) */
            if (q + uw < n && digit(t[q + uw])) {
                size_t b2 = q + uw, l2 = scan_number(t, n, b2), uw2;
                char u1[24];
                snprintf(u1, sizeof(u1), "%s", key);
                if ((uw2 = unit_after(e, t, n, b2 + l2, key, sizeof(key))) != 0)
                    return finish2(in, s, b2 + l2 + uw2, "compound", t, p, len, b2, l2, key, u1);
                snprintf(key, sizeof(key), "%s", u1);
            }
            return finish2(in, s, q + uw, "unit", t, p, len, p, 0, key, "");
        }
    }

    /* Letters right after the digits: 5G 3D 4K 123X. */
    if (q < n && alpha(t[q])) {
        size_t z = q;
        while (z < n && alnum(t[z]))
            ++z;
        return finish(in, s, z, "latin", t + p, z - p, "");
    }

    /* A plain number: its reading depends on the words around it. */
    /* 500+条 五百多条 */
    if (q < n && t[q] == '+' && !(q + 1 < n && digit(t[q + 1])))
        return finish(in, s, q + 1, "int", t + p, len, "plus");
    if ((has(t, n, q, "万+") || has(t, n, q, "亿+")) && !(q + 4 < n && digit(t[q + 4])))
        return finish2(in, s, q + 4, "plusscale", t, p, len, q, 3, "", "");
    if (memchr(t + p, '.', len) || memchr(t + p, ',', len))
        return finish(in, s, q, "int", t + p, len, "");
    if (prev_is(t, p, "第"))
        return finish(in, s, q, "int", t + p, len, "");
    if (len >= 3 && (cue_before(e, "phone_cue", t, p, 5) || word_after(e, "phone_after", t, n, q)))
        return finish(in, s, q, "digits", t + p, len, "yao");
    /* 验证码123456, 301房间, 502错误, 7号楼1502, 0571, 80后: a code reads digit by
       digit. */
    if ((len >= 3 && (cue_before(e, "code_cue", t, p, 6) || word_after(e, "room_after", t, n, q) ||
                      word_after(e, "code_after", t, n, q))) ||
        (len >= 3 && len <= 4 && cue_before(e, "room_before", t, p, 2) &&
         !word_after(e, "room_not", t, n, q)) ||
        (len >= 2 && t[p] == '0') || (len == 2 && t[p + 1] == '0' && has(t, n, q, "后")))
        return finish(in, s, q, "digits", t + p, len, "");
    /* 1990年, 1998那年, 1980年代, 2021款, 2035年前 (not 2000年历史, 将近1000年): a
       year. */
    if (is_year(t, p, len) && word_after(e, "year_after", t, n, q) &&
        !word_after(e, "year_not", t, n, q) && !cue_before(e, "count_before", t, p, 3))
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

/* 的平方 for ², 的N次方 otherwise (10²³ 十的二十三次方, ⁻¹ 的负一次方). */
static ttf_status_t power(const ttf_engine_t *e, ttf_buffer_t *b, const char *x, size_t xn) {
    int negative = xn && x[0] == '-';
    if (xn == 1 && x[0] == '2')
        return word(e, b, "square");
    TRY(word(e, b, "power"));
    if (negative)
        TRY(word(e, b, "minus"));
    TRY(number(e, b, x + negative, xn - negative, 0));
    return word(e, b, "power_end");
}

static ttf_status_t maths_reading(const ttf_engine_t *e, ttf_buffer_t *b, const char *v) {
    size_t i = 0, n = strlen(v), w;
    int after_operand = 0;
    int k;
    while (i < n) {
        char sc;
        if (digit(v[i])) {
            size_t z = i, slash;
            while (z < n && (digit(v[z]) || (v[z] == '.' && z + 1 < n && digit(v[z + 1]))))
                ++z;
            if (z + 1 < n && v[z] == '/' && digit(v[z + 1])) {
                /* 1/4 四分之一 */
                slash = z;
                for (z = slash + 1; z < n && digit(v[z]); ++z)
                    ;
                TRY(number(e, b, v + slash + 1, z - slash - 1, 0));
                TRY(word(e, b, "fraction"));
                TRY(number(e, b, v + i, slash - i, 0));
            } else
                TRY(number(e, b, v + i, z - i, 0));
            i = z;
            after_operand = 1;
            continue;
        }
        if ((sc = superscript(v, n, i, &w)) != 0) {
            char x[16];
            size_t xn = 0;
            while (xn + 1 < sizeof(x) && (sc = superscript(v, n, i, &w)) != 0)
                x[xn++] = sc, i += w;
            TRY(power(e, b, x, xn));
            after_operand = 1;
            continue;
        }
        if (v[i] == '^') {
            size_t z = ++i;
            while (z < n && digit(v[z]))
                ++z;
            TRY(power(e, b, v + i, z - i));
            i = z;
            continue;
        }
        if ((k = symbol_at(v, n, i, &w)) >= 0) {
            const char *key = SYMBOLS[k].key;
            /* a minus with no operand before it is 负 (x>-2) */
            if (SYMBOLS[k].mark[0] == '-' && !after_operand)
                key = "minus";
            if (key[0])
                TRY(word(e, b, key));
            i += w;
            after_operand = (SYMBOLS[k].kind & POSTFIX) || SYMBOLS[k].mark[0] == ')';
            continue;
        }
        if (alpha(v[i])) {
            TRY(ttf_buffer_append(b, v + i, 1));
            after_operand = 1;
        }
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
            size_t z = i, y, dots = 0;
            while (z < n && digit(v[z]))
                ++z;
            for (y = z; y + 1 < n && v[y] == '.' && digit(v[y + 1]);)
                for (++dots, ++y; y < n && digit(v[y]); ++y)
                    ;
            if (dots >= 2) {
                /* a version: V2.3.1 V二点三点一 */
                while (i < y) {
                    size_t part = i;
                    while (part < y && digit(v[part]))
                        ++part;
                    TRY(integer(e, b, v + i, part - i, 0));
                    i = part;
                    if (i < y) {
                        TRY(word(e, b, "point"));
                        ++i;
                    }
                }
                continue;
            }
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
    if (!strcmp(mode, "int")) {
        if (!strcmp(s->secondary, "plus")) {
            TRY(number(e, b, v, vn, 1));
            return word(e, b, "approx_plus");
        }
        return !strcmp(s->secondary, "liang") ? count(e, b, v, vn, 1)
                                              : number(e, b, v, vn, !strcmp(s->secondary, "count"));
    }
    if (!strcmp(mode, "digits"))
        return digits_each(e, b, v, vn, !strcmp(s->secondary, "yao"));
    if (!strcmp(mode, "year"))
        return digits_each(e, b, v, vn, 0);
    if (!strcmp(mode, "time") || !strcmp(mode, "duration"))
        return clock(e, b, v, vn, !strcmp(mode, "duration"));
    if (!strcmp(mode, "minsec")) {
        /* 4:05 四分零五秒 */
        const char *m = strchr(v, ':');
        long ss;
        char x[8];
        if (!m)
            return TTF_EINVAL;
        TRY(number(e, b, v, (size_t)(m - v), 0));
        TRY(word(e, b, "minutes"));
        ss = strtol(m + 1, NULL, 10);
        if (!ss)
            return TTF_OK;
        if (ss < 10)
            TRY(word(e, b, "d0"));
        snprintf(x, sizeof(x), "%ld", ss);
        TRY(integer(e, b, x, strlen(x), 0));
        return word(e, b, "seconds");
    }
    if (!strcmp(mode, "ratio")) {
        /* 3:2 三比二, 3:2:1 三比二比一 */
        const char *part = v;
        for (;;) {
            const char *m = strchr(part, ':');
            TRY(number(e, b, part, m ? (size_t)(m - part) : strlen(part), 0));
            if (!m)
                return TTF_OK;
            TRY(word(e, b, "ratio"));
            part = m + 1;
        }
    }
    if (!strcmp(mode, "percent")) {
        if (!strcmp(s->secondary, "neg"))
            TRY(word(e, b, "minus"));
        TRY(word(e, b, "percent"));
        return number(e, b, v, vn, 0);
    }
    if (!strcmp(mode, "permille")) {
        if (!strcmp(s->secondary, "neg"))
            TRY(word(e, b, "minus"));
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
    if (!strcmp(mode, "math")) {
        TRY(maths_reading(e, b, v));
        return s->secondary[0] ? unit_word(e, b, s->secondary) : TTF_OK;
    }
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
        const char *reading = res(e, "zh_unit", u);
        if (!strcmp(s->secondary, "neg"))
            TRY(minus(e, b, u));
        /* a measurement keeps 二 (二百二十伏), but 2 before a measure word is 两
           (2h 两小时) */
        if (an == 1 && a[0] == '2' && reading && res(e, "liang_counter", reading))
            TRY(word(e, b, "liang"));
        else
            TRY(number(e, b, a, an, 0));
        return unit_word(e, b, u);
    }
    if (!strcmp(mode, "range2")) {
        /* secondary: the first unit, -unit when negative */
        const char *u1 = s->secondary[0] == '-' ? s->secondary + 1 : s->secondary;
        if (s->secondary[0] == '-')
            TRY(minus(e, b, u1));
        TRY(number(e, b, a, an, 0));
        TRY(unit_word(e, b, u1));
        TRY(word(e, b, "range"));
        TRY(number(e, b, bp, bn, 0));
        return unit_word(e, b, u);
    }
    if (!strcmp(mode, "compound")) {
        const char *r1 = res(e, "zh_unit", s->secondary);
        if (an == 1 && a[0] == '2' && r1 && res(e, "liang_counter", r1))
            TRY(word(e, b, "liang"));
        else
            TRY(number(e, b, a, an, 0));
        TRY(unit_word(e, b, s->secondary));
        TRY(number(e, b, bp, bn, 0));
        return unit_word(e, b, u);
    }
    if (!strcmp(mode, "hanmath")) {
        char x[TTF_IR_VALUE_SIZE];
        snprintf(x, sizeof(x), "%.*s", (int)bn, bp);
        TRY(ttf_buffer_append(b, a, an));
        return maths_reading(e, b, x);
    }
    if (!strcmp(mode, "plusscale")) {
        /* 10万+ 十万多 */
        TRY(number(e, b, a, an, 1));
        TRY(ttf_buffer_append(b, bp, bn));
        return word(e, b, "approx_plus");
    }
    if (!strcmp(mode, "hanmoney")) {
        const char *cur = res(e, "zh_currency", u);
        if (!cur)
            return TTF_ERESOURCE;
        TRY(ttf_buffer_append(b, a, an));
        TRY(number(e, b, bp, bn, 1));
        return ttf_buffer_puts(b, cur);
    }
    if (!strcmp(mode, "moneyrange")) {
        const char *cur = res(e, "zh_currency", u);
        if (!cur)
            return TTF_ERESOURCE;
        TRY(number(e, b, a, an, 1));
        TRY(ttf_buffer_puts(b, cur));
        TRY(word(e, b, "range"));
        TRY(number(e, b, bp, bn, 1));
        return ttf_buffer_puts(b, cur);
    }
    if (!strcmp(mode, "pctrange2")) {
        /* 0-100% 零到百分之一百 */
        TRY(number(e, b, a, an, 0));
        TRY(word(e, b, "range"));
        TRY(word(e, b, "percent"));
        return number(e, b, bp, bn, 0);
    }
    if (!strcmp(mode, "monthday")) {
        TRY(integer(e, b, a, an, 0));
        TRY(word(e, b, "month"));
        TRY(integer(e, b, bp, bn, 0));
        return word(e, b, "day");
    }
    if (!strcmp(mode, "holiday")) {
        /* 10.1 十一, 5.1 五一 */
        TRY(integer(e, b, a, an, 0));
        return integer(e, b, bp, bn, 0);
    }
    if (!strcmp(mode, "mixedfrac")) {
        const char *r;
        char ch[8];
        snprintf(ch, sizeof(ch), "%.*s", (int)bn, bp);
        r = res(e, "zh_vulgar", ch);
        if (!r)
            return TTF_ERESOURCE;
        TRY(number(e, b, a, an, 0));
        TRY(word(e, b, "and_fraction"));
        return ttf_buffer_puts(b, r);
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
