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
        /* 33165,0000,0000 三万三千一百六十五亿: above 亿 the count of 亿 takes 万, and
           万亿 only when no 亿 group follows (一万亿) */
        if (g == 3) {
            int yi_group = 0;
            for (k = 0; k < 4; ++k) {
                long at = (long)len - 12 + (long)k;
                yi_group |= at >= 0 && d[at] != '0';
            }
            TRY(word(e, b, yi_group ? "wan" : "wanyi"));
        } else if (g)
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

/* A listed word ending right at `end`, after skipping spaces back (GB/T 1804). */
static int cue_ending(const ttf_engine_t *e, const char *kind, const char *t, size_t end, size_t most) {
    size_t start;
    char key[20];
    while (end > 0 && t[end - 1] == ' ')
        --end;
    for (start = end; start > 0 && end - start < most;) {
        --start;
        while (start > 0 && ((unsigned char)t[start] & 0xc0) == 0x80)
            --start;
        if (end - start < sizeof(key)) {
            memcpy(key, t + start, end - start);
            key[end - start] = 0;
            if (res(e, kind, key))
                return 1;
        }
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
        /* dB(A) and n mile: a bracket or one inner space inside a listed symbol */
        int inner = steps > 0 && w == 1 &&
                    (c[0] == '(' || c[0] == ')' || (c[0] == ' ' && q + 1 < n && alpha(t[q + 1])));
        if (!w || (w == 1 ? !(alpha(c[0]) || c[0] == '/' || inner)
                          : strcmp(c, "℃") && strcmp(c, "°") && strcmp(c, "²") &&
                                strcmp(c, "³") && strcmp(c, "·") && strcmp(c, "㎡") &&
                                strcmp(c, "㎞") && strcmp(c, "μ") && strcmp(c, "Ω") &&
                                strcmp(c, "⁻") && strcmp(c, "¹") && strcmp(c, "Å") &&
                                strcmp(c, "Å")))
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
    /* 5A级景区, 3A游戏: before a hanzi word A is a grade, not amperes (2A的电流 stays) */
    if (best == 1 && t[p] == 'A' && p + 3 < n && !has(t, n, p + 1, "的")) {
        uint32_t cp;
        size_t cw;
        if (ttf_utf8_decode(t + p + 1, n - p - 1, &cp, &cw) == 1 && cp >= 0x4e00 && cp <= 0x9fff)
            return 0;
    }
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

/* A subscript digit ₀..₉ at p (H₂O, a₁): its ASCII digit, 0 if none. */
static char subscript(const char *t, size_t n, size_t p, size_t *w) {
    if (p + 2 < n && (unsigned char)t[p] == 0xe2 && (unsigned char)t[p + 1] == 0x82 &&
        (unsigned char)t[p + 2] >= 0x80 && (unsigned char)t[p + 2] <= 0x89) {
        *w = 3;
        return (char)('0' + ((unsigned char)t[p + 2] - 0x80));
    }
    return 0;
}

static size_t math_unit(const ttf_engine_t *e, const char *t, size_t n, size_t p, char *spec,
                        size_t size);

/* A unit with an optional per part (phase 3): mg/kg mg#kg, g/cm³ g#cm³, 80mg/100ml
   mg#100@ml, ×10⁹/L #L. Returns the bytes consumed and writes the spec. */
/* A unit, or two joined by a middle dot (Ω·m 欧姆米, kg·℃): spec u1&u2. */
static size_t unit_pair(const ttf_engine_t *e, const char *t, size_t n, size_t p, char *out,
                        size_t size) {
    char u1[24], u2[24];
    size_t a = unit_after(e, t, n, p, u1, sizeof(u1)), b;
    if (!a)
        return 0;
    if (has(t, n, p + a, "·") && ((b = unit_after(e, t, n, p + a + 2, u2, sizeof(u2))) != 0 ||
                                   (b = math_unit(e, t, n, p + a + 2, u2, sizeof(u2))) != 0)) {
        int w = snprintf(out, size, "%s&%s", u1, u2);
        if (w > 0 && (size_t)w < size)
            return a + 2 + b;
    }
    snprintf(out, size, "%s", u1);
    return a;
}

static size_t unit_spec(const ttf_engine_t *e, const char *t, size_t n, size_t p, char *spec,
                        size_t size) {
    char u1[24] = "", u2[24];
    size_t a = unit_pair(e, t, n, p, u1, sizeof(u1)), at = p + a, l, b;
    if (!a)
        u1[0] = 0;
    /* kJ/(kg·℃) 千焦每千克摄氏度 */
    if (at + 1 < n && t[at] == '/' && t[at + 1] == '(' && a &&
        (b = unit_pair(e, t, n, at + 2, u2, sizeof(u2))) != 0 && at + 2 + b < n && t[at + 2 + b] == ')') {
        int w = snprintf(spec, size, "%s#%s", u1, u2);
        if (w > 0 && (size_t)w < size)
            return a + 3 + b;
    }
    if (at < n && t[at] == '/' && (a || at == p)) {
        l = scan_number(t, n, at + 1);
        if ((b = unit_pair(e, t, n, at + 1 + l, u2, sizeof(u2))) != 0) {
            int w = l ? snprintf(spec, size, "%s#%.*s@%s", u1, (int)l, t + at + 1, u2)
                      : snprintf(spec, size, "%s#%s", u1, u2);
            if (w > 0 && (size_t)w < size)
                return a + 1 + l + b;
        }
    }
    if (!a)
        return 0;
    snprintf(spec, size, "%s", u1);
    return a;
}

/* A unit spec at p, or after one space when the unit starts with a letter. */
static size_t spaced_unit(const ttf_engine_t *e, const char *t, size_t n, size_t p, char *spec,
                          size_t size) {
    size_t w = unit_spec(e, t, n, p, spec, size);
    /* after a space: a letter, or a symbol such as μm Å Ω */
    if (w || !(p + 1 < n && t[p] == ' ' && (alpha(t[p + 1]) || ((unsigned char)t[p + 1] & 0x80))))
        return w;
    w = unit_spec(e, t, n, p + 1, spec, size);
    /* one letter after a space is a unit only when no letter, digit or joiner
       follows it (1 m处, 0.02 s); 3 A级 stays */
    if (w == 1 && p + 2 < n && (alnum(t[p + 2]) || t[p + 2] == '-' || t[p + 2] == '+' ||
                                t[p + 2] == '&' || t[p + 2] == '.'))
        return 0;
    return w ? w + 1 : 0;
}

/* A short hanzi run ending right before p+run: 元 美元 万元 次 (up to three). */
static size_t hanzi_run(const char *t, size_t n, size_t p, size_t most) {
    size_t q = p, k = 0;
    while (k < most && q + 3 <= n && ((unsigned char)t[q] & 0xf0) == 0xe0)
        q += 3, ++k;
    return q - p;
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
               {"°", "degree", POSTFIX},      {"→", "tends_to", REL},
               {"(", "", 0},
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
/* K C J F after an expression: a unit read only there (resource zh_unit_math). */
static size_t math_unit(const ttf_engine_t *e, const char *t, size_t n, size_t p, char *spec,
                        size_t size) {
    char k[2];
    if (p >= n || !alpha(t[p]) || (p + 1 < n && alnum(t[p + 1])))
        return 0;
    k[0] = t[p], k[1] = 0;
    if (!res(e, "zh_unit_math", k))
        return 0;
    snprintf(spec, size, "%s", k);
    return 1;
}

/* A Greek letter at p (α σ χ β …): its reading, NULL if none. */
static const char *greek_at(const ttf_engine_t *e, const char *t, size_t n, size_t p, size_t *w) {
    char c[5];
    size_t cw = char_at(t, n, p, c);
    const char *r = cw ? res(e, "zh_greek", c) : NULL;
    if (r)
        *w = cw;
    return r;
}

static size_t maths(const ttf_engine_t *e, const char *t, size_t n, size_t p) {
    size_t r = p, w, last = p;
    char key[24];
    /* √n 根号n: a root of one letter */
    if (has(t, n, p, "√") && p + 3 < n && alpha(t[p + 3]) && !(p + 4 < n && alnum(t[p + 4])))
        return 4;
    /* an operator counts once an operand follows it (5> 以上 is not maths) */
    int rel = 0, strong = 0, minus = 0, prel = 0, pstrong = 0, pminus = 0, digits = 0, depth = 0;
    int letter_start = alpha(t[p]) || greek_at(e, t, n, p, &w), k;
    if (p >= n || (!digit(t[p]) && !letter_start && !(t[p] == '(' && p + 1 < n && digit(t[p + 1])) &&
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
            /* i=3%: a percentage inside an expression */
            if (r < n && t[r] == '%')
                ++r;
            digits = operand = 1;
        } else if (greek_at(e, t, n, r, &w)) {
            /* 0.5μm: after a digit μ starts a unit */
            if (r > p && digit(t[r - 1]) && unit_after(e, t, n, r, key, sizeof(key)))
                break;
            r += w;
            operand = 1;
        } else if (has(t, n, r, "∞")) {
            /* n→∞ n趋于无穷大 */
            r += 3;
            digits = operand = 1;
        } else if (alpha(t[r])) {
            size_t f = function_name(t, n, r), u = r;
            int k2;
            if (f) {
                r += f;
                continue;
            }
            /* OR=2.1, CI=, pH=: a short label right before a relation */
            while (u < n && alpha(t[u]))
                ++u;
            if (u - r >= 2 && u - r <= 3 && !(r > p && alpha(t[r - 1])) &&
                (k2 = symbol_at(t, n, u, &w)) >= 0 && (SYMBOLS[k2].kind & REL)) {
                r = u;
                operand = 1;
                rel |= prel, strong |= pstrong, minus |= pminus;
                prel = pstrong = pminus = 0;
                last = r;
                continue;
            }
            /* one letter followed by no letter and no slash (m/s is a unit), and
               not a unit after a number (100m², 220V) */
            if ((r + 1 < n && (alpha(t[r + 1]) || t[r + 1] == '/')) || (r > p && alpha(t[r - 1])) ||
                (r > p && (digit(t[r - 1]) || t[r - 1] == ')' || (r >= p + 2 && superscript(t, n, r - 2, &w) && w == 2) ||
                           (r >= p + 3 && superscript(t, n, r - 3, &w) && w == 3)) &&
                 (unit_after(e, t, n, r, key, sizeof(key)) || math_unit(e, t, n, r, key, sizeof(key)))))
                break;
            ++r;
            operand = 1;
        } else if (subscript(t, n, r, &w)) {
            r += w;
            operand = 1;
        } else if (superscript(t, n, r, &w)) {
            r += w;
            strong = digits = operand = 1;
        } else if ((k = symbol_at(t, n, r, &w)) >= 0) {
            if (SYMBOLS[k].mark[0] == '^' && !(r + 1 < n && digit(t[r + 1])))
                break;
            if (SYMBOLS[k].mark[0] == ')' && !depth)
                break;
            depth += SYMBOLS[k].mark[0] == '(' ? 1 : SYMBOLS[k].mark[0] == ')' ? -1 : 0;
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
    size_t n = in->length, p, q, len, w, m, plus_end;
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
        int named = 0, math_start = 0;
        char plen[24];
        /* First an expression or an amount reaching past the run; the run before
           it is read with its symbol names (Ⅲ期，显著性水平α=0.01). The prefix is
           kept as a length, so a long run still fits. */
        while (z < end) {
            size_t cw = char_at(t, n, z, c);
            if (!cw)
                break;
            snprintf(plen, sizeof(plen), "%lu", (unsigned long)(z - p));
            /* 水平α=0.01, 圆周率π≈3.14: an expression starting inside the run */
            if ((greek_at(e, t, n, z, &w) || symbol_at(t, n, z, &w) >= 0) &&
                (m = maths(e, t, n, z)) != 0 && z + m > end) {
                if (z > p) {
                    char spec[48], sec[80];
                    size_t uw = spaced_unit(e, t, n, z + m, spec, sizeof(spec));
                    if (!uw)
                        uw = math_unit(e, t, n, z + m, spec, sizeof(spec));
                    if (!uw && z + m + 1 < n && t[z + m] == ' ' &&
                        (uw = math_unit(e, t, n, z + m + 1, spec, sizeof(spec))) != 0)
                        ++uw;
                    snprintf(sec, sizeof(sec), "%s|%s", plen, uw ? spec : "");
                    return finish(in, s, z + m + uw, "hanmath", t + z, m, sec);
                }
                math_start = 1;
                break;
            }
            /* 恤￥89: a currency sign at the end of the run */
            if (z + cw == end && end < n && digit(t[end]) && res(e, "zh_currency", c)) {
                len = scan_number(t, n, end);
                if (has(t, n, end + len, "万") || has(t, n, end + len, "亿"))
                    return finish2(in, s, end + len + 3, "hanmoney", t, end, len, end + len, 3, c, plen);
                return finish2(in, s, end + len, "hanmoney", t, end, len, end, 0, c, plen);
            }
            named |= res(e, "zh_vulgar", c) || res(e, "zh_roman", c) ||
                 (res(e, "zh_greek", c) && !(z + cw < n && alpha(t[z + cw])));
            z += cw;
        }
        /* 加½杯, Ⅲ期, β受体: a fraction, Roman or Greek letter inside the run; σ²
           takes its superscript */
        if (named && !math_start) {
            while (superscript(t, n, end, &w))
                end += w;
            return finish(in, s, end, "hanzi", "", 0, "");
        }
    }
    /* σ² 西格玛的平方: a Greek letter with a power, outside an expression */
    if (greek_at(e, t, n, p, &w) && superscript(t, n, p + w, &m) && !maths(e, t, n, p)) {
        q = p + w;
        while (superscript(t, n, q, &m))
            q += m;
        return finish(in, s, q, "hanzi", "", 0, "");
    }
    /* 时长~2 h 约两小时, 粒径≈35 μm 约三十五微米: before a number after a word */
    if ((has(t, n, p, "~") || has(t, n, p, "～") || has(t, n, p, "〜") || has(t, n, p, "≈")) &&
        (p == 0 || ((unsigned char)t[p - 1] & 0x80) || t[p - 1] == ' ' || t[p - 1] == '(')) {
        size_t mw = t[p] == '~' ? 1 : 3, z = p + mw;
        uint32_t cp = 0;
        size_t cw, back = p;
        while (back > 0 && ((unsigned char)t[back - 1] & 0xc0) == 0x80)
            --back;
        if (back > 0)
            --back;
        if (p > 0 && ttf_utf8_decode(t + back, p - back, &cp, &cw) != 1)
            cp = 0;
        if (z < n && t[z] == ' ')
            ++z;
        /* 1日~7日, 3天~5天: after a counted word the mark is a range */
        if (back > 0 && (digit(t[back - 1]) || (p - back == 3 && word_after(e, "range_unit", t, n, back))))
            z = n;
        if (z < n && digit(t[z]) &&
            (p == 0 || t[p - 1] == ' ' || t[p - 1] == '(' || (cp >= 0x4e00 && cp <= 0x9fff) ||
             (cp >= 0x3000 && cp <= 0x303f) || (cp >= 0xff01 && cp <= 0xff1f)))
            return finish(in, s, z, "approx", "", 0, prev_is(t, p, "约") ? "silent" : "");
    }
    /* K12+300~K12+800 K十二加三百到K十二加八百: a chainage (and its range) */
    if (t[p] >= 'A' && t[p] <= 'Z') {
        size_t z = p, k1, end = 0;
        int part;
        for (part = 0; part < 2; ++part) {
            size_t a0 = z;
            while (z < n && t[z] >= 'A' && t[z] <= 'Z' && z - a0 < 2)
                ++z;
            if (z == a0 || z >= n || !digit(t[z]))
                break;
            k1 = digits_end(t, n, z);
            if (!(k1 + 1 < n && t[k1] == '+' && digit(t[k1 + 1])))
                break;
            end = z = digits_end(t, n, k1 + 1);
            if (part || !range_mark(t, n, z, &w))
                break;
            z += w;
        }
        if (end && !(end < n && alnum(t[end])))
            return finish(in, s, end, "chainage", t + p, end - p, "");
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
        char spec[48];
        size_t uw = spaced_unit(e, t, n, p + m, spec, sizeof(spec));
        if (!uw)
            uw = math_unit(e, t, n, p + m, spec, sizeof(spec));
        if (!uw && p + m + 1 < n && t[p + m] == ' ' &&
            (uw = math_unit(e, t, n, p + m + 1, spec, sizeof(spec))) != 0)
            ++uw;
        if (!uw)
            spec[0] = 0;
        return finish(in, s, p + m + uw, "math", t + p, m, spec);
    }

    /* Currency before the amount: ¥128 $9.99 HK$100 €20 £50 ₩10,000; a range
       ¥80～¥120 八十元到一百二十元. */
    if ((w = word_after(e, "zh_currency", t, n, p)) != 0 && p + w + 1 < n &&
        (digit(t[p + w]) || (t[p + w] == ' ' && alpha(t[p]) && digit(t[p + w + 1])))) {
        size_t w2, sp = t[p + w] == ' ';
        snprintf(key, sizeof(key), "%.*s", (int)w, t + p);
        w += sp;
        len = scan_number(t, n, p + w);
        q = p + w + len;
        /* ¥28.5/kg 二十八点五元每千克 */
        if (q + 1 < n && t[q] == '/') {
            char spec[48];
            size_t uw = unit_spec(e, t, n, q, spec, sizeof(spec));
            if (uw && spec[0] == '#')
                return finish2(in, s, q + uw, "moneyunit", t, p + w, len, q, 0, spec, key);
        }
        if (range_mark(t, n, q, &w2)) {
            size_t b2 = q + w2, cw = word_after(e, "zh_currency", t, n, b2);
            if (cw == w && !memcmp(t + b2, t + p, w) && b2 + cw < n && digit(t[b2 + cw]))
                b2 += cw;
            if (b2 < n && digit(t[b2])) {
                size_t l2 = scan_number(t, n, b2);
                return finish2(in, s, b2 + l2, "moneyrange", t, p + w, len, b2, l2, key, "");
            }
        }
        /* ¥599/晚 五百九十九元每晚 */
        if (q + 3 < n && t[q] == '/' && hanzi_run(t, n, q + 1, 3)) {
            size_t hw = hanzi_run(t, n, q + 1, 1);
            return finish2(in, s, q + 1 + hw, "moneyper", t, p + w, len, q + 1, hw, key, "");
        }
        /* $500万 五百万美元 */
        if (has(t, n, q, "万") || has(t, n, q, "亿"))
            return finish2(in, s, q + 3, "moneyscale", t, p + w, len, q, 3, key, "");
        /* ¥3,560.00 三千五百六十元: zero cents are not read */
        {
            const char *dot = memchr(t + p + w, '.', len);
            size_t k2;
            if (dot) {
                for (k2 = (size_t)(dot - t) + 1; k2 < q && t[k2] == '0'; ++k2)
                    ;
                if (k2 == q)
                    len = (size_t)(dot - t) - p - w;
            }
        }
        return finish(in, s, q, "money", t + p + w, len, key);
    }

    /* △DEF 三角形DEF */
    if (has(t, n, p, "△") && p + 3 < n && alpha(t[p + 3])) {
        q = p + 3;
        while (q < n && alpha(t[q]))
            ++q;
        return finish(in, s, q, "triangle", t + p + 3, q - p - 3, "");
    }
    /* T+0 T加零 */
    if (alpha(t[p]) && p + 2 < n && t[p + 1] == '+' && digit(t[p + 2])) {
        q = digits_end(t, n, p + 2);
        return finish(in, s, q, "tplus", t + p, q - p, "");
    }
    /* No.1 第一 */
    if ((has(t, n, p, "No.") || has(t, n, p, "NO.")) && p + 3 < n && digit(t[p + 3])) {
        q = digits_end(t, n, p + 3);
        return finish(in, s, q, "ordno", t + p + 3, q - p - 3, "");
    }
    /* Letters with digits: CA1234 G20 COVID-19 PM2.5 B2B abc123 USB3.0 H₂O SPF30+. */
    if (alpha(t[p])) {
        int digits_seen = 0;
        q = p;
        for (;;) {
            if (q < n && (alnum(t[q]) || ((t[q] == '-' || t[q] == '.') && q + 1 < n &&
                                          alnum(t[q + 1]) && alnum(t[q - 1])))) {
                digits_seen |= digit(t[q++]);
                continue;
            }
            if (subscript(t, n, q, &w)) {
                q += w;
                digits_seen = 1;
                continue;
            }
            break;
        }
        if (digits_seen && q < n && t[q] == '+' && !(q + 1 < n && (digit(t[q + 1]) || alpha(t[q + 1]))))
            ++q;
        if (digits_seen && q < n && digit(t[q - 1])) {
            char spec[48];
            size_t uw = unit_spec(e, t, n, q, spec, sizeof(spec));
            if (uw && !(q + uw < n && alnum(t[q + uw])))
                return finish2(in, s, q + uw, "latinunit", t, p, q - p, q, 0, spec, "");
        }
        return digits_seen ? finish(in, s, q, "latin", t + p, q - p, "") : 0;
    }

    /* +86 加八六; +3价 正三价 (an amount, +0.15% or +256.3, goes on to 正 below) */
    if (t[p] == '+' && p + 1 < n && digit(t[p + 1]) &&
        !((q = digits_end(t, n, p + 1)) < n &&
          (t[q] == '.' || t[q] == '%' || has(t, n, q, "‰") || has(t, n, q, "亿") || has(t, n, q, "万") ||
           unit_after(e, t, n, q, key, sizeof(key))))) {
        q = digits_end(t, n, p + 1);
        if (has(t, n, q, "价"))
            return finish(in, s, q, "valence", t + p + 1, q - p - 1, "");
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

    /* -5 负五, -5℃ 零下五摄氏度, -2.3% 负百分之二点三, -5~3℃ 零下五到三摄氏度.
       +0.15% 正百分之零点一五, +256.3 正二百五十六点三, +0.25V: an amount with a plus
       sign reads 正 (owner sets, 2026-10-14); +86 stays a code. */
    plus_end = t[p] == '+' && p + 1 < n && digit(t[p + 1]) ? digits_end(t, n, p + 1) : 0;
    if (t[p] == '-' || has(t, n, p, "−") ||
        (plus_end && plus_end < n &&
         (t[plus_end] == '.' || t[plus_end] == '%' || has(t, n, plus_end, "‰") || has(t, n, plus_end, "亿") ||
          has(t, n, plus_end, "万") || unit_after(e, t, n, plus_end, key, sizeof(key))))) {
        size_t a = p + (t[p] == '-' || t[p] == '+' ? 1 : 3), l2, b2, uw;
        const char *sign = t[p] == '+' ? "pos" : "neg";
        if (a >= n || !digit(t[a]))
            return 0;
        if (t[p] == '-' && p >= 3 && ((unsigned char)t[p - 3] & 0xf0) == 0xe0 && t[a] == '0' && a + 1 < n &&
            digit(t[a + 1])) {
            q = digits_end(t, n, a);
            if (!(q < n && (t[q] == '%' || t[q] == '.')) && !has(t, n, q, "℃") &&
                !unit_after(e, t, n, q, key, sizeof(key)))
                return finish(in, s, q, "digits", t + a, q - a, "");
        }
        len = scan_number(t, n, a);
        q = a + len;
        if (range_mark(t, n, q, &w) && q + w < n && digit(t[q + w])) {
            b2 = q + w;
            l2 = scan_number(t, n, b2);
            if ((uw = unit_after(e, t, n, b2 + l2, key, sizeof(key))) != 0)
                return finish2(in, s, b2 + l2 + uw, "range", t, a, len, b2, l2, key, sign);
        }
        if (q < n && t[q] == '%')
            return finish(in, s, q + 1, "percent", t + a, len, sign);
        if (has(t, n, q, "‰"))
            return finish(in, s, q + 3, "permille", t + a, len, sign);
        if ((uw = unit_after(e, t, n, q, key, sizeof(key))) != 0) {
            /* -10℃~45℃ 零下十摄氏度到四十五摄氏度 */
            char u1[24];
            snprintf(u1, sizeof(u1), "-%s", key);
            if (*sign == 'n' && range_mark(t, n, q + uw, &w) && q + uw + w < n && digit(t[q + uw + w])) {
                b2 = q + uw + w;
                l2 = scan_number(t, n, b2);
                if ((uw = unit_after(e, t, n, b2 + l2, key, sizeof(key))) != 0)
                    return finish2(in, s, b2 + l2 + uw, "range2", t, a, len, b2, l2, key, u1);
                uw = unit_after(e, t, n, q, key, sizeof(key));
            }
            return finish2(in, s, q + uw, "unit", t, a, len, a, 0, key, sign);
        }
        {
            char spec[48];
            size_t sw = spaced_unit(e, t, n, q, spec, sizeof(spec));
            if (sw)
                return finish2(in, s, q + sw, "unit", t, a, len, a, 0, spec, sign);
        }
        return finish(in, s, q, sign, t + a, len, "");
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
    /* GB 18580-2017, GB/T 19001-2016: a standard number reads digit by digit */
    if (cue_ending(e, "standard_cue", t, p, 8)) {
        size_t z = p;
        while (z < n && (digit(t[z]) || ((t[z] == '-' || t[z] == '.' || t[z] == ':') && z + 1 < n &&
                                         digit(t[z + 1]))))
            ++z;
        return finish(in, s, z, "standard", t + p, z - p, "");
    }
    /* 10.1假期 十一, 5.1小长假 五一: a holiday named by its date. */
    if (memchr(t + p, '.', len) && word_after(e, "holiday_after", t, n, q)) {
        const char *dot = memchr(t + p, '.', len);
        long mo = value_of(t, p, (size_t)(dot - t)), dd = value_of(t, (size_t)(dot - t) + 1, q);
        /* 6.18购物节 六一八: a shopping festival is named by its digits */
        if (mo >= 1 && mo <= 12 && dd >= 10 && dd <= 31 && mo != dd && q - (size_t)(dot - t) == 3 &&
            word_after(e, "festival_digits", t, n, q))
            return finish(in, s, q, "digits", t + p, len, "");
        if (mo >= 1 && mo <= 12 && dd >= 1 && dd <= 31 && q - (size_t)(dot - t) <= 3)
            return finish2(in, s, q, "holiday", t, p, (size_t)(dot - t) - p, (size_t)(dot - t) + 1,
                           q - (size_t)(dot - t) - 1, "", "");
    }
    /* 定于9.28开业 九月二十八日: a dotted month-day after a date word or before an
       event word, never before a scale, a unit or a measure word */
    if (memchr(t + p, '.', len) && !memchr(t + p, ',', len) && !range_mark(t, n, q, &w)) {
        const char *dot = memchr(t + p, '.', len);
        size_t d0 = (size_t)(dot - t);
        long mo = value_of(t, p, d0), dd = value_of(t, d0 + 1, q);
        if (d0 - p <= 2 && q - d0 - 1 <= 2 && mo >= 1 && mo <= 12 && dd >= 1 && dd <= 31 &&
            !(q < n && (alnum(t[q]) || t[q] == '%' || t[q] == '.')) && !has(t, n, q, "万") &&
            !has(t, n, q, "亿") && !has(t, n, q, "％") && !word_after(e, "liang_counter", t, n, q) &&
            !word_after(e, "money_after", t, n, q) && !unit_after(e, t, n, q, key, sizeof(key)) &&
            (cue_before(e, "md_cue", t, p, 4) || word_after(e, "md_after", t, n, q)))
            return finish2(in, s, q, "monthday", t, p, d0 - p, d0 + 1, q - d0 - 1, "", "");
    }
    /* 2½ 二又二分之一 */
    if (char_at(t, n, q, c) && res(e, "zh_vulgar", c))
        return finish2(in, s, q + strlen(c), "mixedfrac", t, p, len, q, strlen(c), "", "");

    /* 192.168.0.1(:8080): an IPv4 address reads digit by digit with 点 */
    {
        size_t z = p, groups = 0;
        while (groups < 4) {
            size_t g0 = z;
            z = digits_end(t, n, z);
            if (z == g0 || z - g0 > 3)
                break;
            ++groups;
            if (groups < 4 && z + 1 < n && t[z] == '.' && digit(t[z + 1]))
                ++z;
            else
                break;
        }
        if (groups == 4 && !(z < n && (digit(t[z]) || (t[z] == '.' && z + 1 < n && digit(t[z + 1]))))) {
            if (z + 1 < n && t[z] == ':' && digit(t[z + 1]))
                z = digits_end(t, n, z + 1);
            return finish(in, s, z, "ip", t + p, z - p, "");
        }
    }
    /* 3.6.2 三点六点二: a dotted version without letters */
    {
        size_t z = p, groups = 0;
        while (groups < 4) {
            size_t g0 = z;
            z = digits_end(t, n, z);
            if (z == g0 || z - g0 > 3)
                break;
            ++groups;
            if (z + 1 < n && t[z] == '.' && digit(t[z + 1]))
                ++z;
            else
                break;
        }
        if (groups == 3 && !(z < n && (digit(t[z]) || t[z] == '.' || t[z] == '%' || alpha(t[z]))))
            return finish(in, s, z, "version", t + p, z - p, "");
    }
    /* 39°54′N 北纬三十九度五十四分 */
    if (has(t, n, q, "°") && !memchr(t + p, '.', len)) {
        size_t z = q + 2, mz = digits_end(t, n, z), dir = 0;
        if (mz > z && (has(t, n, mz, "′") || has(t, n, mz, "'")))
            dir = mz + (t[mz] == '\'' ? 1 : 3);
        else if (z < n && (t[z] == 'N' || t[z] == 'S' || t[z] == 'E' || t[z] == 'W'))
            dir = z, mz = z;
        if (dir && dir < n && (t[dir] == 'N' || t[dir] == 'S' || t[dir] == 'E' || t[dir] == 'W') &&
            !(dir + 1 < n && alpha(t[dir + 1])) && !prev_is(t, p, "北纬") && !prev_is(t, p, "南纬") &&
            !prev_is(t, p, "东经") && !prev_is(t, p, "西经")) {
            char dk[2] = {t[dir], 0};
            return finish2(in, s, dir + 1, "coord", t, p, len, z, mz - z, dk, "");
        }
        if (mz > z && mz - z <= 2 && (has(t, n, mz, "′") || has(t, n, mz, "'"))) {
            size_t end = mz + (t[mz] == '\'' ? 1 : 3);
            /* 北纬30°15′N: the letter repeats the written word */
            if (dir && dir < n && (t[dir] == 'N' || t[dir] == 'S' || t[dir] == 'E' || t[dir] == 'W') &&
                !(dir + 1 < n && alpha(t[dir + 1])))
                end = dir + 1;
            return finish2(in, s, end, "degmin", t, p, len, z, mz - z, "", "");
        }
    }
    /* 6σ 六西格玛 */
    if (greek_at(e, t, n, q, &w) && !(q + w < n && (alnum(t[q + w]) || t[q + w] == '=')))
        return finish2(in, s, q + w, "numgreek", t, p, len, q, w, "", "");
    /* 4!等于24 四的阶乘 */
    if (q < n && t[q] == '!' && (has(t, n, q + 1, "=") || has(t, n, q + 1, "等于")) &&
        !has(t, n, q + 1, "=="))
        return finish(in, s, q + 1, "factorial", t + p, len, "");
    /* 2:05.38 两分零五秒三八: minutes, seconds and hundredths */
    if (len <= 2 && q + 4 < n && t[q] == ':' && digit(t[q + 1]) && digit(t[q + 2]) && t[q + 3] == '.' &&
        digit(t[q + 4])) {
        size_t z = digits_end(t, n, q + 4);
        return finish2(in, s, z, "swim", t, p, len, q + 1, z - q - 1, "", "");
    }
    /* 9秒91 九秒九一 */
    if (!memchr(t + p, '.', len) && has(t, n, q, "秒") && q + 4 < n && digit(t[q + 3]) &&
        digit(t[q + 4]) && !(q + 5 < n && digit(t[q + 5])) && !unit_after(e, t, n, q + 5, key, sizeof(key)) &&
        !has(t, n, q + 5, "秒"))
        return finish2(in, s, q + 5, "sechund", t, p, len, q + 3, 2, "", "");
    /* （002594） 〔2024〕: a code or a year alone in brackets reads digit by digit */
    if (len >= 4 && len <= 6 && !memchr(t + p, '.', len) && !memchr(t + p, ',', len) &&
        ((prev_is(t, p, "（") && has(t, n, q, "）")) || (prev_is(t, p, "(") && has(t, n, q, ")")) ||
         (prev_is(t, p, "〔") && has(t, n, q, "〕")) || (prev_is(t, p, "【") && has(t, n, q, "】")) ||
         (prev_is(t, p, "[") && has(t, n, q, "]"))))
        return finish(in, s, q, "digits", t + p, len, "");
    /* 2'30" 两分三十秒, 4′08″: minutes and seconds */
    if (!memchr(t + p, '.', len) && (has(t, n, q, "'") || has(t, n, q, "′"))) {
        size_t mw = t[q] == '\'' ? 1 : 3, b2 = q + mw, z = digits_end(t, n, b2);
        if (z > b2 && z - b2 <= 2 && (has(t, n, z, "\"") || has(t, n, z, "″")))
            return finish2(in, s, z + (t[z] == '"' ? 1 : 3), "prime", t, p, len, b2, z - b2, "", "");
    }
    /* 10w+ 十万多, 3.5w 三点五万, 2k+ 两千多 (k alone stays: 4K) */
    if (q < n && t[q] == 'w' && !(q + 1 < n && alnum(t[q + 1])))
        return finish2(in, s, q + 1 + (q + 1 < n && t[q + 1] == '+'), "scaled", t, p, len, q, 1, "",
                       q + 1 < n && t[q + 1] == '+' ? "plus" : "");
    if (q + 1 < n && (t[q] == 'k' || t[q] == 'K') && t[q + 1] == '+')
        return finish2(in, s, q + 2, "scaled", t, p, len, q, 1, "", "plus");
    /* 1e-3 一e负三 */
    if (q + 1 < n && (t[q] == 'e' || t[q] == 'E') &&
        (digit(t[q + 1]) || (q + 2 < n && (t[q + 1] == '-' || t[q + 1] == '+') && digit(t[q + 2])))) {
        size_t z = digits_end(t, n, q + 1 + !digit(t[q + 1]));
        if (!(z < n && alnum(t[z])))
            return finish2(in, s, z, "enot", t, p, len, q + 1, z - q - 1, "", "");
    }
    /* 0x80004005 零x八零零零四零零五: a hexadecimal code */
    if (len == 1 && t[p] == '0' && q + 1 < n && (t[q] == 'x' || t[q] == 'X') &&
        (digit(t[q + 1]) || (t[q + 1] >= 'A' && t[q + 1] <= 'F') || (t[q + 1] >= 'a' && t[q + 1] <= 'f'))) {
        size_t z = q + 1;
        while (z < n && (digit(t[z]) || (t[z] >= 'A' && t[z] <= 'F') || (t[z] >= 'a' && t[z] <= 'f')))
            ++z;
        if (!(z < n && alnum(t[z])))
            return finish(in, s, z, "digits", t + p, z - p, "");
    }
    /* 2#机组 二号机组 */
    if (q + 3 < n && t[q] == '#' && !memchr(t + p, '.', len) && ((unsigned char)t[q + 1] & 0xf0) == 0xe0)
        return finish(in, s, q + 1, "hashno", t + p, len, "");
    /* 4x4 四乘四, 1920X1080 */
    if (q + 1 < n && (t[q] == 'x' || t[q] == 'X') && digit(t[q + 1])) {
        size_t b2 = q + 1, l2 = scan_number(t, n, b2);
        if (!(b2 + l2 < n && alpha(t[b2 + l2]) && !unit_after(e, t, n, b2 + l2, key, sizeof(key))))
            return finish2(in, s, b2 + l2, "times", t, p, len, b2, l2, "", "");
    }
    /* 72次/分 七十二次每分钟, 15元/斤, 5.2万/㎡, 15.6元/股, 3,900美元/盎司: a hanzi
       measure per another */
    {
        size_t hw = hanzi_run(t, n, q, 2);
        if (hw && q + hw < n && t[q + hw] == '/') {
            size_t at = q + hw + 1, uw2, pw = hanzi_run(t, n, at, 3), k;
            char per[16];
            if ((uw2 = unit_after(e, t, n, at, key, sizeof(key))) != 0)
                return finish2(in, s, at + uw2, "perhan", t, p, len, q, hw, key, "unit");
            /* the longest listed word after the slash, else one hanzi */
            for (k = pw; k >= 3; k -= 3) {
                snprintf(per, sizeof(per), "%.*s", (int)k, t + at);
                if (k == 3 || res(e, "per_hanzi", per) || res(e, "liang_counter", per))
                    return finish2(in, s, at + k, "perhan", t, p, len, q, hw, per, "hanzi");
            }
        }
    }

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
        /* 1:10,000 一比一万 */
        {
            size_t g = scan_number(t, n, q + 1);
            if (q + 1 + g > mend && memchr(t + q + 1, ',', g) && !memchr(t + q + 1, '.', g))
                return finish(in, s, q + 1 + g, "ratio", t + p, q + 1 + g - p, "");
        }
        if (mend + 1 < n && t[mend] == ':' && digit(t[mend + 1]) &&
            (send = digits_end(t, n, mend + 1)) - mend - 1 == 2)
            end = send;
        else
            send = 0;
        ratio = cue_before(e, "ratio_cue", t, p, 4) || mend - q - 1 != 2 || mm >= 60 || h > 24;
        /* 完赛时间是1:23:45: a duration word anywhere in the clause before */
        if (!ratio && send && cue_before(e, "duration_cue", t, p, 8))
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
        /* 配比20:10:10: three parts after a ratio word are a ratio */
        if (cue_before(e, "proportion_cue", t, p, 4))
            return finish(in, s, end, "ratio", t + p, end - p, "");
        /* 以1:58:20夺冠: H:MM:SS is never a score */
        return finish(in, s, end, "duration", t + p, end - p, "");
    }

    /* Percent: 50% 百分之五十, 50%-60% 百分之五十到百分之六十, 5‰ 千分之五; 98.6％. */
    if (has(t, n, q, "％"))
        return finish(in, s, q + 3, "percent", t + p, len, "");
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

    /* Dates: 2024-10-11, 2024/10/11, 2024.10.11; a year and a month 2026/08, 2025-11. */
    {
        size_t y = digits_end(t, n, p);
        if (y - p == 4 && is_year(t, p, 4) && y + 1 < n && (t[y] == '-' || t[y] == '/') && digit(t[y + 1])) {
            size_t me = digits_end(t, n, y + 1);
            long mo = value_of(t, y + 1, me);
            if (me - y - 1 <= 2 && mo >= 1 && mo <= 12 && !(me < n && (t[me] == t[y] || t[me] == '.')) &&
                !unit_after(e, t, n, me, key, sizeof(key)) && !word_after(e, "year_after", t, n, me))
                return finish2(in, s, me, "yearmonth", t, p, 4, y + 1, me - y - 1, "", "");
        }
        if (y - p == 4 && y + 1 < n && (t[y] == '-' || t[y] == '/' || t[y] == '.')) {
            char sep = t[y];
            size_t me = digits_end(t, n, y + 1), de;
            long mo = value_of(t, y + 1, me), dd;
            if (mo >= 1 && mo <= 12 && me - y - 1 <= 2 && me + 1 < n && t[me] == sep) {
                de = digits_end(t, n, me + 1);
                dd = value_of(t, me + 1, de);
                if (dd >= 1 && dd <= 31 && de - me - 1 <= 2) {
                    size_t rw, y2;
                    if (range_mark(t, n, de, &rw) && (y2 = digits_end(t, n, de + rw)) - de - rw == 4 &&
                        y2 < n && t[y2] == sep) {
                        size_t m2 = digits_end(t, n, y2 + 1), d2;
                        if (m2 < n && t[m2] == sep && (d2 = digits_end(t, n, m2 + 1)) > m2 + 1 &&
                            d2 - m2 - 1 <= 2)
                            return finish2(in, s, d2, "daterange", t, p, de - p, de + rw, d2 - de - rw, "", "");
                    }
                    return finish2(in, s, de, "date", t, p, 4, y + 1, de - y - 1, "", "");
                }
            }
        }
    }

    /* 生产日期：20250618 二零二五年六月十八日; 有效期至2027.06 二零二七年六月 */
    if (len == 8 && ((t[p] == '1' && t[p + 1] == '9') || (t[p] == '2' && t[p + 1] == '0')) &&
        !(q < n && alnum(t[q])) && cue_before(e, "date_cue", t, p, 6)) {
        long mo = value_of(t, p + 4, p + 6), dd = value_of(t, p + 6, p + 8);
        if (mo >= 1 && mo <= 12 && dd >= 1 && dd <= 31)
            return finish(in, s, q, "date8", t + p, 8, "");
    }
    if (len == 7 && is_year(t, p, 4) && t[p + 4] == '.' && !(q < n && (alnum(t[q]) || t[q] == '.')) &&
        cue_before(e, "date_cue", t, p, 6)) {
        long mo = value_of(t, p + 5, p + 7);
        if (mo >= 1 && mo <= 12)
            return finish2(in, s, q, "yearmonth", t, p, 4, p + 5, 2, "", "");
    }
    /* 日程（9.15—9.20） 九月十五日到九月二十日: a dotted month-day range in brackets
       after a schedule word, or after a date word */
    if (memchr(t + p, '.', len) && range_mark(t, n, q, &w) && q + w < n && digit(t[q + w])) {
        const char *dot = memchr(t + p, '.', len);
        size_t b2 = q + w, l2 = scan_number(t, n, b2), d1 = (size_t)(dot - t);
        const char *dot2 = memchr(t + b2, '.', l2);
        if (dot2 && d1 - p <= 2 && q - d1 - 1 <= 2 && b2 + l2 - (size_t)(dot2 - t) - 1 <= 2 &&
            (size_t)(dot2 - t) - b2 <= 2) {
            size_t d2 = (size_t)(dot2 - t);
            long m1 = value_of(t, p, d1), x1 = value_of(t, d1 + 1, q), m2 = value_of(t, b2, d2),
                 x2 = value_of(t, d2 + 1, b2 + l2);
            int bracket = ((prev_is(t, p, "（") && has(t, n, b2 + l2, "）") &&
                            cue_ending(e, "schedule_word", t, p - 3, 8)) ||
                           (prev_is(t, p, "(") && has(t, n, b2 + l2, ")") &&
                            cue_ending(e, "schedule_word", t, p - 1, 8)));
            if (m1 >= 1 && m1 <= 12 && m2 >= 1 && m2 <= 12 && x1 >= 1 && x1 <= 31 && x2 >= 1 && x2 <= 31 &&
                (m2 > m1 || (m2 == m1 && x2 > x1)) &&
                ((bracket && q - d1 - 1 == 2 && b2 + l2 - d2 - 1 == 2) || cue_before(e, "date_cue", t, p, 4) ||
                 word_after(e, "date_after", t, n, b2 + l2)))
                return finish2(in, s, b2 + l2, "mdrange", t, p, len, b2, l2, "", "");
        }
    }
    /* Fractions: 1/3 三分之一 (1又1/2: 又 stays as written). A month/day near a
       date word is a date when it cannot be a common fraction (12/3, 9/18). */
    if (q + 1 < n && t[q] == '/' && digit(t[q + 1]) && !memchr(t + p, '.', len)) {
        size_t b2 = q + 1, z = digits_end(t, n, b2);
        long a = value_of(t, p, q), d = value_of(t, b2, z);
        /* 205/55 R16, 175/92A: a size code, not a fraction */
        if (len >= 3 && z - b2 >= 2 && z < n &&
            ((t[z] == ' ' && z + 1 < n && t[z + 1] == 'R') || (alpha(t[z]) && !(z + 1 < n && alpha(t[z + 1]))))) {
            if (alpha(t[z]))
                ++z;
            return finish(in, s, z, "digits", t + p, z - p, "");
        }
        /* 1/1,000,000 一百万分之一 */
        if (z < n && t[z] == ',' && !memchr(t + p, ',', len)) {
            size_t g = scan_number(t, n, b2);
            if (b2 + g > z && !memchr(t + b2, '.', g))
                return finish2(in, s, b2 + g, "frac", t, p, len, b2, g, "", "");
        }
        if (a >= 1 && a <= 12 && d >= 1 && d <= 31 && (a > d || d > 12) &&
            (cue_before(e, "date_cue", t, p, 4) || word_after(e, "date_after", t, n, z)))
            return finish2(in, s, z, "monthday", t, p, len, b2, z - b2, "", "");
        if (has(t, n, z, "mmHg") || has(t, n, z, " mmHg"))
            return finish2(in, s, z + 4 + (t[z] == ' '), "pressure", t, p, len, b2, z - b2, "mmHg", "");
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
            if (z + 1 < n && (t[z] == '-' || t[z] == ' ') && digit(t[z + 1]))
                ++z;
            else
                break;
        }
        if ((groups == 2 && t[p] == '0' && gl[0] >= 3 && gl[0] <= 4 && gl[1] >= 7 && gl[1] <= 8) ||
            (groups == 3 && gl[0] == 3 && gl[1] == 3 && gl[2] == 4) ||
            (groups == 1 && gl[0] == 11 && t[p] == '1') ||
            /* 138 1234 5678, 021-6234 5678 */
            (groups == 3 && t[p] == '1' && gl[0] == 3 && gl[1] == 4 && gl[2] == 4) ||
            (groups == 3 && t[p] == '0' && gl[0] >= 3 && gl[0] <= 4 && gl[1] == 4 && gl[2] == 4))
            return finish(in, s, z, "digits", t + p, z - p, "yao");
    }

    /* 11-08 十一月八日: a zero-padded month-day, or one after a date word */
    if (len == 2 && q + 2 < n && t[q] == '-' && digit(t[q + 1]) && digit(t[q + 2]) &&
        !(q + 3 < n && (digit(t[q + 3]) || t[q + 3] == '.'))) {
        long mo = value_of(t, p, q), dd = value_of(t, q + 1, q + 3);
        if (mo >= 1 && mo <= 12 && dd >= 1 && dd <= 31 && q + 6 < n && t[q + 3] == ' ' &&
            digit(t[q + 4]) && (t[q + 5] == ':' || (digit(t[q + 5]) && t[q + 6] == ':'))) {
            size_t c0 = q + 4, cm = digits_end(t, n, c0), ce;
            if (cm + 2 < n && t[cm] == ':' && digit(t[cm + 1]) && digit(t[cm + 2])) {
                ce = cm + 3;
                return finish2(in, s, ce, "mdtime", t, p, len, q + 1, 2, "", "");
            }
        }
        if (mo >= 1 && mo <= 12 && dd >= 1 && dd <= 31 && !unit_after(e, t, n, q + 3, key, sizeof(key)) &&
            (t[p] == '0' || t[q + 1] == '0' ||
             (cue_before(e, "date_cue", t, p, 4) && !word_after(e, "liang_counter", t, n, q + 3) &&
              !has(t, n, q + 3, "点") && !has(t, n, q + 3, "时"))))
            return finish2(in, s, q + 3, "monthday", t, p, len, q + 1, 2, "", "");
    }
    /* Ranges: 3-5天 10~20人 2010-2020年 20-25℃ 1-2个 18—35岁 0-100%; a score
       以7-5赢下 七比五. */
    if (range_mark(t, n, q, &w) && q + w < n && digit(t[q + w])) {
        size_t b2 = q + w, l2 = scan_number(t, n, b2), uw;
        /* 零件号4512-00327, 20250301-02: a code, not a range */
        if (t[q] == '-' && !memchr(t + p, '.', len) &&
            (cue_before(e, "code_cue", t, p, 6) || (len >= 5 && t[b2] == '0' && l2 >= 2))) {
            size_t z = b2 + l2;
            while (z + 1 < n && t[z] == '-' && digit(t[z + 1]))
                z = digits_end(t, n, z + 1);
            return finish(in, s, z, "digits", t + p, z - p, "");
        }
        /* 2025-083: a year and a zero-led serial is a notice number */
        if (t[q] == '-' && is_year(t, p, len) && t[b2] == '0' && l2 >= 3 && !memchr(t + b2, '.', l2) &&
            !(b2 + l2 < n && (t[b2 + l2] == '-' || t[b2 + l2] == '.' || t[b2 + l2] == '/')))
            return finish(in, s, b2 + l2, "digits", t + p, b2 + l2 - p, "");
        /* 战成2-2 二比二, 以6-3、6-2 六比三、六比二: a score after a score word, or
           in a list of set scores; never before a measure word (以3-5人) */
        if (t[q] == '-' && len <= 2 && l2 <= 2 && !memchr(t + p, '.', len) && !memchr(t + b2, '.', l2) &&
            !(b2 + l2 < n && (alnum(t[b2 + l2]) || t[b2 + l2] == '-' || t[b2 + l2] == '%')) &&
            !word_after(e, "liang_counter", t, n, b2 + l2) && !word_after(e, "range_unit", t, n, b2 + l2) &&
            !unit_after(e, t, n, b2 + l2, key, sizeof(key))) {
            int listed = 0;
            if (prev_is(t, p, "、") && p >= 6) {
                size_t k = p - 3;
                while (k > 0 && digit(t[k - 1]))
                    --k;
                if (k < p - 3 && k >= 2 && t[k - 1] == '-' && digit(t[k - 2]))
                    listed = 1;
            }
            if (cue_before(e, "score_cue", t, p, 3) || listed ||
                (prev_is(t, p, "以") && (has(t, n, b2 + l2, "、") || word_after(e, "ratio_after", t, n, b2 + l2)))) {
                char v[24];
                snprintf(v, sizeof(v), "%.*s:%.*s", (int)len, t + p, (int)l2, t + b2);
                return finish(in, s, b2 + l2, "ratio", v, strlen(v), "");
            }
        }
        /* 1-1战平 一比一 */
        if (t[q] == '-' && len <= 2 && l2 <= 2 && word_after(e, "score_after", t, n, b2 + l2)) {
            char v[24];
            snprintf(v, sizeof(v), "%.*s:%.*s", (int)len, t + p, (int)l2, t + b2);
            return finish(in, s, b2 + l2, "ratio", v, strlen(v), "");
        }
        /* 配比16-16-16 十六比十六比十六 */
        if (t[q] == '-' && b2 + l2 + 1 < n && t[b2 + l2] == '-' && digit(t[b2 + l2 + 1]) &&
            cue_before(e, "proportion_cue", t, p, 4)) {
            size_t z = b2 + l2;
            char v[40];
            while (z + 1 < n && t[z] == '-' && digit(t[z + 1]))
                z = digits_end(t, n, z + 1);
            if (z - p < sizeof(v)) {
                size_t k2;
                for (k2 = 0; k2 < z - p; ++k2)
                    v[k2] = t[p + k2] == '-' ? ':' : t[p + k2];
                return finish(in, s, z, "ratio", v, z - p, "");
            }
        }
        /* 3-2-1108, 64-17-5: a chain of hyphened groups is a code, not a range; short
           groups of one length without a leading zero read as numbers (15-15-15
           十五十五十五) */
        if (t[q] == '-' && b2 + l2 + 1 < n && t[b2 + l2] == '-' && digit(t[b2 + l2 + 1])) {
            size_t z = b2 + l2, g0 = p;
            int short_groups = 1;
            while (z + 1 < n && t[z] == '-' && digit(t[z + 1]))
                z = digits_end(t, n, z + 1);
            while (g0 < z) {
                size_t g1 = digits_end(t, n, g0);
                short_groups &= g1 - g0 <= 2 && g1 - g0 == len && t[g0] != '0';
                g0 = g1 + 1;
            }
            if (short_groups && !memchr(t + p, '.', len))
                return finish(in, s, z, "intchain", t + p, z - p, "");
            return finish(in, s, z, "digits", t + p, z - p, "");
        }
        if (t[q] == '-' && cue_before(e, "ratio_cue", t, p, 1) &&
            word_after(e, "ratio_after", t, n, b2 + l2)) {
            char v[24];
            snprintf(v, sizeof(v), "%.*s:%.*s", (int)len, t + p, (int)l2, t + b2);
            return finish(in, s, b2 + l2, "ratio", v, strlen(v), "");
        }
        if (b2 + l2 < n && t[b2 + l2] == '%')
            return finish2(in, s, b2 + l2 + 1, "pctrange2", t, p, len, b2, l2, "", "");
        {
            char spec[48];
            size_t sw = spaced_unit(e, t, n, b2 + l2, spec, sizeof(spec));
            if (sw && (strchr(spec, '#') || strchr(spec, '&') || t[b2 + l2] == ' ')) {
                const char *r = res(e, "zh_unit", spec);
                return finish2(in, s, b2 + l2 + sw, "range", t, p, len, b2, l2, spec,
                               r && res(e, "liang_counter", r) ? "liang" : "");
            }
        }
        uw = unit_after(e, t, n, b2 + l2, key, sizeof(key));
        if (is_year(t, p, len) && (is_year(t, b2, l2) || l2 == 2) &&
            word_after(e, "year_after", t, n, b2 + l2))
            return finish2(in, s, b2 + l2, "yearrange", t, p, len, b2, l2, "", "");
        if (!uw)
            key[0] = 0;
        return finish2(in, s, b2 + l2 + uw, "range", t, p, len, b2, l2, key,
                       word_after(e, "liang_counter", t, n, b2 + l2 + uw) ? "liang" : "");
    }

    /* A unit symbol: 5kg 2L 220V 25℃ 60km/h 100m²; 20℃~30℃ with a unit on
       both ends. */
    {
        char spec[48];
        size_t uw = unit_after(e, t, n, q, key, sizeof(key)), sw;
        /* 15mg/kg 十五毫克每千克, 2.7g/cm³, 80mg/100ml */
        if ((sw = unit_spec(e, t, n, q, spec, sizeof(spec))) > uw && (strchr(spec, '#') || strchr(spec, '&')))
            return finish2(in, s, q + sw, "unit", t, p, len, p, 0, spec, "");
        if (!uw && (sw = spaced_unit(e, t, n, q, spec, sizeof(spec))) != 0)
            return finish2(in, s, q + sw, "unit", t, p, len, p, 0, spec, "");
        if (uw) {
            if (range_mark(t, n, q + uw, &w) && q + uw + w < n && digit(t[q + uw + w])) {
                size_t b2 = q + uw + w, l2 = scan_number(t, n, b2), uw2;
                char u1[24];
                snprintf(u1, sizeof(u1), "%s", key);
                if ((uw2 = unit_after(e, t, n, b2 + l2, key, sizeof(key))) != 0)
                    return finish2(in, s, b2 + l2 + uw2, "range2", t, p, len, b2, l2, key, u1);
                snprintf(key, sizeof(key), "%s", u1);
            }
            /* 1.8m×2m 一点八米乘两米 */
            if (has(t, n, q + uw, "×") && q + uw + 2 < n && digit(t[q + uw + 2])) {
                size_t b2 = q + uw + 2, l2 = scan_number(t, n, b2), uw2;
                char u1[24];
                snprintf(u1, sizeof(u1), "%s", key);
                if ((uw2 = unit_after(e, t, n, b2 + l2, key, sizeof(key))) != 0)
                    return finish2(in, s, b2 + l2 + uw2, "dims", t, p, len, b2, l2, key, u1);
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

    /* Letters right after the digits: 5G 3D 4K 123X; 4K120Hz 四K一百二十赫兹. */
    if (q < n && alpha(t[q])) {
        size_t z = q, y, uw;
        while (z < n && alpha(t[z]))
            ++z;
        if (z < n && digit(t[z]) && (y = z + scan_number(t, n, z)) &&
            (uw = unit_after(e, t, n, y, key, sizeof(key))) != 0 &&
            !(y + uw < n && (alnum(t[y + uw]) || t[y + uw] == '-')))
            return finish2(in, s, y + uw, "latinunit", t, p, z - p, z, y - z, key, "");
        while (z < n && (alnum(t[z]) || (t[z] == '-' && z + 1 < n && alnum(t[z + 1]) && alnum(t[z - 1]))))
            ++z;
        return finish(in, s, z, "latin", t + p, z - p, "");
    }

    /* A plain number: its reading depends on the words around it. */
    /* 2万m³ 两万立方米, 12.6亿kWh 十二点六亿千瓦时 */
    if (has(t, n, q, "万") || has(t, n, q, "亿")) {
        char spec[48];
        size_t sw = unit_spec(e, t, n, q + 3, spec, sizeof(spec));
        if (sw)
            return finish2(in, s, q + 3 + sw, "scaleunit", t, p, len, q, 3, spec, "");
    }
    /* 500+条 五百多条 */
    if (q < n && t[q] == '+' && !(q + 1 < n && digit(t[q + 1])))
        return finish(in, s, q + 1, "int", t + p, len, "plus");
    if ((has(t, n, q, "万+") || has(t, n, q, "亿+")) && !(q + 4 < n && digit(t[q + 4])))
        return finish2(in, s, q + 4, "plusscale", t, p, len, q, 3, "", "");
    /* 132,500万 十三亿二千五百万: a grouped number with its scale is one number */
    if (memchr(t + p, ',', len) && !memchr(t + p, '.', len) && (has(t, n, q, "万") || has(t, n, q, "亿"))) {
        char v[40];
        size_t vn = 0, k;
        for (k = p; k < q && vn + 9 < sizeof(v); ++k)
            if (digit(t[k]))
                v[vn++] = t[k];
        memcpy(v + vn, has(t, n, q, "万") ? "0000" : "00000000", has(t, n, q, "万") ? 4 : 8);
        vn += has(t, n, q, "万") ? 4 : 8;
        return finish(in, s, q + 3, "int", v, vn, "count");
    }
    /* 1,280.00元 一千二百八十元: zero cents are not read */
    if (memchr(t + p, '.', len) && word_after(e, "money_after", t, n, q)) {
        const char *dot = memchr(t + p, '.', len);
        size_t k2;
        for (k2 = (size_t)(dot - t) + 1; k2 < q && t[k2] == '0'; ++k2)
            ;
        if (k2 == q)
            return finish(in, s, q, "int", t + p, (size_t)(dot - t) - p, "count");
    }
    if (memchr(t + p, '.', len) || memchr(t + p, ',', len))
        return finish(in, s, q, "int", t + p, len, "");
    /* 第2025097期 第二零二五零九七期 */
    if (prev_is(t, p, "第") && len >= 5 && has(t, n, q, "期"))
        return finish(in, s, q, "digits", t + p, len, "");
    if (prev_is(t, p, "第"))
        return finish(in, s, q, "int", t + p, len, "");
    /* 报503 五零三: an HTTP status */
    if (len == 3 && (t[p] == '4' || t[p] == '5') && cue_before(e, "status_cue", t, p, 2))
        return finish(in, s, q, "digits", t + p, len, "");
    /* 回复“666”: a number alone in quotes is a code */
    if (len >= 2 && ((prev_is(t, p, "“") && has(t, n, q, "”")) || (prev_is(t, p, "\"") && has(t, n, q, "\"")) ||
                     (prev_is(t, p, "「") && has(t, n, q, "」"))))
        return finish(in, s, q, "digits", t + p, len, "");
    /* 88折 八八折, 618大促 六一八 */
    if ((len == 2 && has(t, n, q, "折")) || (len == 3 && word_after(e, "holiday_after", t, n, q)))
        return finish(in, s, q, "digits", t + p, len, "");
    /* 手机售价3999元: a price or a count after a phone word is no phone number */
    if (len >= 3 && (cue_before(e, "phone_cue", t, p, 5) || word_after(e, "phone_after", t, n, q)) &&
        !word_after(e, "money_after", t, n, q) && !word_after(e, "liang_counter", t, n, q))
        return finish(in, s, q, "digits", t + p, len, "yao");
    /* 818路公交: a route of three digits or more reads digit by digit */
    if (len >= 3 && has(t, n, q, "路") && !memchr(t + p, '.', len))
        return finish(in, s, q, "digits", t + p, len, "");
    /* 验证码123456, 301房间, 502错误, 7号楼1502, 0571, 80后: a code reads digit by
       digit. */
    if ((len >= 3 && (cue_before(e, "code_cue", t, p, 6) || word_after(e, "room_after", t, n, q) ||
                      word_after(e, "code_after", t, n, q))) ||
        (len >= 3 && len <= 4 && cue_before(e, "room_before", t, p, 2) &&
         !word_after(e, "room_not", t, n, q)) ||
        (len >= 2 && t[p] == '0') || (len == 2 && has(t, n, q, "后")) ||
        /* GB 18580: a standard number; （2024）: a year in a case number */
        (len >= 3 && cue_ending(e, "standard_cue", t, p, 8)) ||
        (len == 4 && (prev_is(t, p, "（") || prev_is(t, p, "(")) &&
         (has(t, n, q, "）") || has(t, n, q, ")"))))
        return finish(in, s, q, "digits", t + p, len, "");
    /* 12306, 120: a listed service number, unless a measure word follows (110座桥);
       96110的来电, 114还能查: also before a word a call takes (service_after,
       owner sets 2026-10-14) */
    if (len >= 3 && !word_after(e, "liang_counter", t, n, q) && !unit_after(e, t, n, q, key, sizeof(key)) &&
        (cue_before(e, "service_cue", t, p, 2) || word_after(e, "service_after", t, n, q) ||
         !(q < n && ((unsigned char)t[q] & 0xf0) == 0xe0 && !has(t, n, q, "，") && !has(t, n, q, "。") &&
           !has(t, n, q, "！") && !has(t, n, q, "？") && !has(t, n, q, "、") && !has(t, n, q, "）")))) {
        char num[8];
        snprintf(num, sizeof(num), "%.*s", (int)(len < 7 ? len : 7), t + p);
        if (len < 7 && res(e, "service_number", num) && !prev_is(t, p, "等于") && !prev_is(t, p, "="))
            return finish(in, s, q, "digits", t + p, len, "yao");
    }
    /* 1990年, 1998那年, 1980年代, 2021款, 2035年前 (not 2000年历史, 将近1000年): a
       year. */
    if (is_year(t, p, len) && word_after(e, "year_after", t, n, q) &&
        ((!word_after(e, "year_not", t, n, q) && !cue_before(e, "count_before", t, p, 3)) ||
         ((prev_is(t, p, "至") || prev_is(t, p, "到")) && p >= 7 && is_year(t, p - 7, 4) &&
          !(p > 7 && digit(t[p - 8])))))
        return finish(in, s, q, "year", t + p, len, "");
    /* 08年那场 零八年, 95年出生 九五年: a two-digit year before a time word */
    if (len == 2 && word_after(e, "short_year_after", t, n, q) && !cue_before(e, "count_before", t, p, 3))
        return finish(in, s, q, "year", t + p, len, "");
    /* 1990至2020年 一九九零至二零二零年: the first year of a range in words */
    if (is_year(t, p, len) && (has(t, n, q, "至") || has(t, n, q, "到")) && is_year(t, q + 3, digits_end(t, n, q + 3) - q - 3) &&
        word_after(e, "year_after", t, n, q + 7))
        return finish(in, s, q, "year", t + p, len, "");
    /* 2年级 二年级: a measure word that starts a name of an order */
    return finish(in, s, q, "int", t + p, len,
                  word_after(e, "liang_counter", t, n, q) && !word_after(e, "liang_not", t, n, q) ? "liang"
                                                                                                : "count");
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
    while (m - v > 1 && v[0] == '0')
        ++v;
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
    size_t i = 0, n = strlen(v), w, w2;
    int after_operand = 0;
    /* 12×15=180 十二乘以十五: × in an equation is 乘以 (owner, 2026-10-14); 4×4,
       1920×1080 and 7.9×10³ keep 乘. */
    int equation = strchr(v, '=') != NULL;
    int k;
    while (i < n) {
        char sc;
        if (digit(v[i])) {
            size_t z = i, slash;
            while (z < n && (digit(v[z]) || (v[z] == '.' && z + 1 < n && digit(v[z + 1]))))
                ++z;
            if (z < n && v[z] == '%') {
                /* 3% 百分之三 */
                TRY(word(e, b, "percent"));
                TRY(number(e, b, v + i, z - i, 0));
                i = z + 1;
                after_operand = 1;
                continue;
            }
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
        if ((sc = subscript(v, n, i, &w)) != 0) {
            TRY(digit_word(e, b, sc, 0));
            i += w;
            after_operand = 1;
            continue;
        }
        if (has(v, n, i, "∞")) {
            TRY(word(e, b, "infinity"));
            i += 3;
            after_operand = 1;
            continue;
        }
        {
            const char *g = greek_at(e, v, n, i, &w);
            if (g) {
                /* χ² 卡方 */
                if (has(v, n, i, "χ") && has(v, n, i + w, "²")) {
                    TRY(word(e, b, "chi_square"));
                    i += w + 2;
                } else {
                    TRY(ttf_buffer_puts(b, g));
                    i += w;
                }
                after_operand = 1;
                continue;
            }
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
            if (equation && !strcmp(key, "times") && !(has(v, n, i + w, "10") &&
                                                       (has(v, n, i + w + 2, "^") || superscript(v, n, i + w + 2, &w2))))
                key = "multiply";
            if (key[0])
                TRY(word(e, b, key));
            i += w;
            after_operand = (SYMBOLS[k].kind & POSTFIX) || SYMBOLS[k].mark[0] == ')';
            continue;
        }
        if (alpha(v[i])) {
            size_t f = function_name(v, n, i);
            /* sin²x sin平方x: a power of a function goes before its argument */
            if (f && has(v, n, i + f, "²")) {
                TRY(ttf_buffer_append(b, v + i, f));
                TRY(word(e, b, "square_bare"));
                i += f + 2;
                continue;
            }
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
        } else {
            char sc;
            size_t w;
            if ((sc = subscript(v, n, i, &w)) != 0) {
                /* H₂O H二O */
                TRY(digit_word(e, b, sc, 0));
                i += w;
                continue;
            }
            if (v[i] == '+' && i + 1 == n)
                TRY(word(e, b, "plus"));
            ++i;
        }
    }
    return TTF_OK;
}

/* A hanzi run as written, with ½ Ⅲ α replaced by their readings. */
static ttf_status_t hanzi_text(const ttf_engine_t *e, ttf_buffer_t *b, const char *text, size_t length) {
    size_t i = 0;
    char ch[5];
    while (i < length) {
        size_t cw = char_at(text, length, i, ch), w;
        const char *r = cw ? res(e, "zh_vulgar", ch) : NULL;
        if (cw && superscript(text, length, i, &w)) {
            char x[16];
            size_t xn = 0;
            char sc;
            while (xn + 1 < sizeof(x) && (sc = superscript(text, length, i, &w)) != 0)
                x[xn++] = sc, i += w;
            TRY(power(e, b, x, xn));
            continue;
        }
        if (cw && !r)
            r = res(e, "zh_roman", ch);
        if (cw && !r && !(i + cw < length && alpha(text[i + cw])))
            r = res(e, "zh_greek", ch);
        if (!cw)
            return ttf_buffer_append(b, text + i, length - i);
        TRY(r ? ttf_buffer_puts(b, r) : ttf_buffer_append(b, text + i, cw));
        i += cw;
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
    const char *hash = strchr(unit, '#'), *amp = strchr(unit, '&'), *u;
    if (amp && (!hash || amp < hash)) {
        /* Ω&m 欧姆米 */
        char u1[24];
        snprintf(u1, sizeof(u1), "%.*s", (int)(amp - unit), unit);
        TRY(unit_word(e, b, u1));
        return unit_word(e, b, amp + 1);
    }
    if (hash) {
        /* u1#u2 u1每u2, u1#N@u2 u1每N u2 (80mg/100ml 八十毫克每一百毫升) */
        char u1[24];
        const char *at = strchr(hash + 1, '@');
        snprintf(u1, sizeof(u1), "%.*s", (int)(hash - unit), unit);
        /* kg/m³ 千克每立方米: both parts of a per unit take the per reading */
        if (u1[0] && (u = res(e, "zh_unit_per", u1)) != NULL)
            TRY(ttf_buffer_puts(b, u));
        else if (u1[0])
            TRY(unit_word(e, b, u1));
        TRY(word(e, b, "per"));
        if (at)
            TRY(number(e, b, hash + 1, (size_t)(at - hash - 1), 0));
        /* a per unit may read differently (mg/kg 毫克每千克, (kg·℃) 千克摄氏度) */
        if (strchr(at ? at + 1 : hash + 1, '&')) {
            const char *rest = at ? at + 1 : hash + 1, *a2 = strchr(rest, '&');
            char p1[24];
            snprintf(p1, sizeof(p1), "%.*s", (int)(a2 - rest), rest);
            u = res(e, "zh_unit_per", p1);
            TRY(u ? ttf_buffer_puts(b, u) : unit_word(e, b, p1));
            return unit_word(e, b, a2 + 1);
        }
        u = res(e, "zh_unit_per", at ? at + 1 : hash + 1);
        return u ? ttf_buffer_puts(b, u) : unit_word(e, b, at ? at + 1 : hash + 1);
    }
    u = unit[0] ? res(e, "zh_unit", unit) : "";
    if (!u)
        u = res(e, "zh_unit_math", unit);
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
        /* An address label (abc123@163.com): digits one by one, letters as written. */
        if (s->type == TTF_SEM_CUSTOM && !strcmp(mode, "@alphabet"))
            return !strcmp(s->secondary, "control-surface") ? digits_each(e, b, surface, length, 0)
                                                           : digits_each(e, b, v, vn, 0);
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
        if (!strcmp(s->secondary, "pos"))
            TRY(word(e, b, "positive"));
        TRY(word(e, b, "percent"));
        return number(e, b, v, vn, 0);
    }
    if (!strcmp(mode, "permille")) {
        if (!strcmp(s->secondary, "neg"))
            TRY(word(e, b, "minus"));
        if (!strcmp(s->secondary, "pos"))
            TRY(word(e, b, "positive"));
        TRY(word(e, b, "permille"));
        return number(e, b, v, vn, 0);
    }
    if (!strcmp(mode, "vulgar")) {
        const char *r = res(e, "zh_vulgar", v);
        return r ? ttf_buffer_puts(b, r) : TTF_ERESOURCE;
    }
    if (!strcmp(mode, "neg") || !strcmp(mode, "pos")) {
        TRY(word(e, b, !strcmp(mode, "pos") ? "positive" : "minus"));
        return number(e, b, v, vn, 0);
    }
    if (!strcmp(mode, "money")) {
        const char *cur = res(e, "zh_currency", s->secondary);
        if (!cur)
            return TTF_ERESOURCE;
        TRY(count(e, b, v, vn, 1));
        return ttf_buffer_puts(b, cur);
    }
    if (!strcmp(mode, "math")) {
        TRY(maths_reading(e, b, v));
        return s->secondary[0] ? unit_word(e, b, s->secondary) : TTF_OK;
    }
    if (!strcmp(mode, "latin"))
        return latin(e, b, v);
    if (!strcmp(mode, "valence")) {
        TRY(word(e, b, "positive"));
        return number(e, b, v, vn, 0);
    }
    if (!strcmp(mode, "standard") || !strcmp(mode, "version")) {
        /* GB 7258.1-2017 七二五八点一二零一七; 3.6.2 三点六点二 */
        size_t i = 0;
        int version = !strcmp(mode, "version");
        while (i < vn) {
            if (digit(v[i])) {
                size_t z = i;
                while (z < vn && digit(v[z]))
                    ++z;
                TRY(version ? integer(e, b, v + i, z - i, 0) : digits_each(e, b, v + i, z - i, 0));
                i = z;
                continue;
            }
            if (v[i] == '.')
                TRY(word(e, b, "point"));
            else if (v[i] == ':')
                TRY(word(e, b, "colon"));
            ++i;
        }
        return TTF_OK;
    }
    if (!strcmp(mode, "ip")) {
        /* 192.168.0.1:8080 一九二点一六八点零点一冒号八零八零 */
        size_t i;
        for (i = 0; i < vn; ++i)
            if (digit(v[i]))
                TRY(digit_word(e, b, v[i], 0));
            else if (v[i] == '.')
                TRY(word(e, b, "point"));
            else if (v[i] == ':')
                TRY(word(e, b, "colon"));
        return TTF_OK;
    }
    if (!strcmp(mode, "factorial")) {
        TRY(number(e, b, v, vn, 0));
        return word(e, b, "factorial");
    }
    if (!strcmp(mode, "triangle")) {
        TRY(word(e, b, "triangle"));
        return ttf_buffer_append(b, v, vn);
    }
    if (!strcmp(mode, "tplus")) {
        TRY(ttf_buffer_append(b, v, 1));
        TRY(word(e, b, "plus"));
        return number(e, b, v + 2, vn - 2, 0);
    }
    if (!strcmp(mode, "ordno")) {
        TRY(word(e, b, "ordinal"));
        return number(e, b, v, vn, 0);
    }
    if (!strcmp(mode, "to"))
        return word(e, b, "range");
    if (!strcmp(mode, "intchain")) {
        /* 15-15-15 十五十五十五 */
        size_t i = 0;
        while (i < vn) {
            size_t z = i;
            while (z < vn && digit(v[z]))
                ++z;
            TRY(integer(e, b, v + i, z - i, 0));
            i = z + 1;
        }
        return TTF_OK;
    }
    if (!strcmp(mode, "approx"))
        return strcmp(s->secondary, "silent") ? word(e, b, "approx") : TTF_OK;
    if (!strcmp(mode, "hashno")) {
        TRY(number(e, b, v, vn, 0));
        return word(e, b, "hao");
    }
    if (!strcmp(mode, "date8")) {
        TRY(digits_each(e, b, v, 4, 0));
        TRY(word(e, b, "year"));
        TRY(integer(e, b, v + 4, 2, 0));
        TRY(word(e, b, "month"));
        TRY(integer(e, b, v + 6, 2, 0));
        return word(e, b, "day");
    }
    if (!strcmp(mode, "chainage")) {
        /* K12+300~K12+800: letters, the kilometre, 加, the metres; the mark is 到 */
        size_t i = 0;
        while (i < vn) {
            if (alpha(v[i])) {
                TRY(ttf_buffer_append(b, v + i, 1));
                ++i;
            } else if (digit(v[i])) {
                size_t z = i;
                while (z < vn && digit(v[z]))
                    ++z;
                TRY(integer(e, b, v + i, z - i, 0));
                i = z;
            } else if (v[i] == '+') {
                TRY(word(e, b, "plus"));
                ++i;
            } else {
                TRY(word(e, b, "range"));
                ++i;
                while (i < vn && ((unsigned char)v[i] & 0xc0) == 0x80)
                    ++i;
            }
        }
        return TTF_OK;
    }
    if (!strcmp(mode, "hanzi"))
        return hanzi_text(e, b, surface, length);
    if (!strcmp(mode, "hanmath")) {
        /* secondary: the length of the run before the expression */
        size_t plen = (size_t)strtoul(s->secondary, NULL, 10);
        const char *bar = strchr(s->secondary, '|');
        if (plen > length)
            return TTF_EINVAL;
        TRY(hanzi_text(e, b, surface, plen));
        TRY(maths_reading(e, b, v));
        return bar && bar[1] ? unit_word(e, b, bar + 1) : TTF_OK;
    }
    if (!split(v, &a, &an, &bp, &bn, &u))
        return TTF_EINVAL;
    if (!strcmp(mode, "unit")) {
        const char *reading = res(e, "zh_unit", u);
        if (!strcmp(s->secondary, "neg"))
            TRY(minus(e, b, u));
        if (!strcmp(s->secondary, "pos"))
            TRY(word(e, b, "positive"));
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
    if (!strcmp(mode, "moneyper")) {
        const char *cur = res(e, "zh_currency", u);
        if (!cur)
            return TTF_ERESOURCE;
        TRY(count(e, b, a, an, 1));
        TRY(ttf_buffer_puts(b, cur));
        TRY(word(e, b, "per"));
        return ttf_buffer_append(b, bp, bn);
    }
    if (!strcmp(mode, "swim")) {
        /* a = minutes, b = SS.hh */
        long ss = strtol(bp, NULL, 10);
        const char *dot = memchr(bp, '.', bn);
        TRY(count(e, b, a, an, 1));
        TRY(word(e, b, "minutes"));
        if (ss < 10)
            TRY(word(e, b, "d0"));
        TRY(integer(e, b, bp, dot ? (size_t)(dot - bp) : bn, 0));
        TRY(word(e, b, "seconds"));
        return dot ? digits_each(e, b, dot + 1, bn - (size_t)(dot + 1 - bp), 0) : TTF_OK;
    }
    if (!strcmp(mode, "sechund")) {
        TRY(number(e, b, a, an, 0));
        TRY(word(e, b, "seconds"));
        return digits_each(e, b, bp, bn, 0);
    }
    if (!strcmp(mode, "mdtime")) {
        /* a = MM, b = DD; the clock time follows the space in the surface */
        const char *sp = memchr(surface, ' ', length);
        TRY(integer(e, b, a, an, 0));
        TRY(word(e, b, "month"));
        TRY(integer(e, b, bp, bn, 0));
        TRY(word(e, b, "day"));
        if (!sp)
            return TTF_EINVAL;
        return clock(e, b, sp + 1, length - (size_t)(sp + 1 - surface), 0);
    }
    if (!strcmp(mode, "daterange")) {
        /* a = first date text, b = second date text, both YYYY<sep>M<sep>D */
        size_t k;
        for (k = 0; k < 2; ++k) {
            const char *d = k ? bp : a;
            size_t dn = k ? bn : an, m1 = 4, m2;
            if (k)
                TRY(word(e, b, "range"));
            for (m2 = m1 + 1; m2 < dn && digit(d[m2]); ++m2)
                ;
            if (m2 >= dn)
                return TTF_EINVAL;
            TRY(digits_each(e, b, d, 4, 0));
            TRY(word(e, b, "year"));
            TRY(integer(e, b, d + m1 + 1, m2 - m1 - 1, 0));
            TRY(word(e, b, "month"));
            TRY(integer(e, b, d + m2 + 1, dn - m2 - 1, 0));
            TRY(word(e, b, "day"));
        }
        return TTF_OK;
    }
    if (!strcmp(mode, "coord")) {
        /* a = degrees, b = minutes, u = N S E W */
        char key2[16];
        snprintf(key2, sizeof(key2), "coord_%s", u);
        TRY(word(e, b, key2));
        TRY(number(e, b, a, an, 0));
        TRY(word(e, b, "degree"));
        if (!bn)
            return TTF_OK;
        TRY(number(e, b, bp, bn, 0));
        return word(e, b, "minutes");
    }
    if (!strcmp(mode, "degmin")) {
        TRY(number(e, b, a, an, 0));
        TRY(word(e, b, "degree"));
        TRY(number(e, b, bp, bn, 0));
        return word(e, b, "minutes");
    }
    if (!strcmp(mode, "enot")) {
        /* a = mantissa, b = [+-]exponent */
        TRY(number(e, b, a, an, 0));
        TRY(ttf_buffer_puts(b, "e"));
        if (bn && bp[0] == '-')
            TRY(word(e, b, "minus"));
        if (bn && (bp[0] == '-' || bp[0] == '+'))
            ++bp, --bn;
        return number(e, b, bp, bn, 0);
    }
    if (!strcmp(mode, "numgreek")) {
        char g[8];
        size_t w;
        const char *r;
        snprintf(g, sizeof(g), "%.*s", (int)bn, bp);
        r = greek_at(e, g, strlen(g), 0, &w);
        TRY(number(e, b, a, an, 0));
        return r ? ttf_buffer_puts(b, r) : TTF_ERESOURCE;
    }
    if (!strcmp(mode, "yearmonth")) {
        TRY(digits_each(e, b, a, an, 0));
        TRY(word(e, b, "year"));
        TRY(integer(e, b, bp, bn, 0));
        return word(e, b, "month");
    }
    if (!strcmp(mode, "pressure")) {
        TRY(number(e, b, a, an, 0));
        TRY(word(e, b, "ratio"));
        TRY(number(e, b, bp, bn, 0));
        return unit_word(e, b, u);
    }
    if (!strcmp(mode, "dims")) {
        TRY(number(e, b, a, an, 0));
        TRY(unit_word(e, b, s->secondary));
        TRY(word(e, b, "times"));
        TRY(count(e, b, bp, bn, 1));
        return unit_word(e, b, u);
    }
    if (!strcmp(mode, "scaleunit")) {
        TRY(count(e, b, a, an, 1));
        TRY(ttf_buffer_append(b, bp, bn));
        return unit_word(e, b, u);
    }
    if (!strcmp(mode, "prime")) {
        /* 2'30" 两分三十秒, 4'08" 四分零八秒 */
        long ss = strtol(bp, NULL, 10);
        TRY(count(e, b, a, an, 1));
        TRY(word(e, b, "minutes"));
        if (!ss)
            return TTF_OK;
        if (ss < 10)
            TRY(word(e, b, "d0"));
        TRY(number(e, b, bp, bn, 0));
        return word(e, b, "seconds");
    }
    if (!strcmp(mode, "scaled")) {
        /* 10w+ 十万多, 2k+ 两千多 */
        TRY(count(e, b, a, an, 1));
        TRY(word(e, b, bp[0] == 'w' ? "wan" : "1000"));
        return !strcmp(s->secondary, "plus") ? word(e, b, "approx_plus") : TTF_OK;
    }
    if (!strcmp(mode, "times")) {
        TRY(number(e, b, a, an, 0));
        TRY(word(e, b, "times"));
        return number(e, b, bp, bn, 0);
    }
    if (!strcmp(mode, "perhan")) {
        /* 72次/分 七十二次每分钟: the measure as written, 每, the second unit */
        const char *r = !strcmp(s->secondary, "hanzi") ? res(e, "per_hanzi", u) : NULL;
        TRY(count(e, b, a, an, 1));
        TRY(ttf_buffer_append(b, bp, bn));
        TRY(word(e, b, "per"));
        if (!strcmp(s->secondary, "unit"))
            return unit_word(e, b, u);
        return ttf_buffer_puts(b, r ? r : u);
    }
    if (!strcmp(mode, "moneyscale")) {
        const char *cur = res(e, "zh_currency", u);
        if (!cur)
            return TTF_ERESOURCE;
        TRY(number(e, b, a, an, 1));
        TRY(ttf_buffer_append(b, bp, bn));
        return ttf_buffer_puts(b, cur);
    }
    if (!strcmp(mode, "latinunit")) {
        char x[TTF_IR_VALUE_SIZE];
        snprintf(x, sizeof(x), "%.*s", (int)an, a);
        TRY(latin(e, b, x));
        if (bn)
            TRY(number(e, b, bp, bn, 0));
        return unit_word(e, b, u);
    }
    if (!strcmp(mode, "plusscale")) {
        /* 10万+ 十万多 */
        TRY(number(e, b, a, an, 1));
        TRY(ttf_buffer_append(b, bp, bn));
        return word(e, b, "approx_plus");
    }
    if (!strcmp(mode, "hanmoney")) {
        /* a = the amount, u = the currency, secondary = the length of the run */
        const char *cur = res(e, "zh_currency", u);
        size_t plen = (size_t)strtoul(s->secondary, NULL, 10);
        if (!cur || plen > length)
            return cur ? TTF_EINVAL : TTF_ERESOURCE;
        TRY(hanzi_text(e, b, surface, plen));
        TRY(count(e, b, a, an, 1));
        if (bn)
            TRY(ttf_buffer_append(b, bp, bn));
        return ttf_buffer_puts(b, cur);
    }
    if (!strcmp(mode, "moneyunit")) {
        /* u = the per spec (#kg), secondary = the currency */
        const char *cur = res(e, "zh_currency", s->secondary);
        if (!cur)
            return TTF_ERESOURCE;
        TRY(count(e, b, a, an, 1));
        TRY(ttf_buffer_puts(b, cur));
        return unit_word(e, b, u);
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
    if (!strcmp(mode, "mdrange")) {
        /* a = M.D, b = M.D: 九月十五日到九月二十日 */
        size_t k;
        for (k = 0; k < 2; ++k) {
            const char *d = k ? bp : a, *dot;
            size_t dn = k ? bn : an;
            dot = memchr(d, '.', dn);
            if (!dot)
                return TTF_EINVAL;
            if (k)
                TRY(word(e, b, "range"));
            TRY(integer(e, b, d, (size_t)(dot - d), 0));
            TRY(word(e, b, "month"));
            TRY(integer(e, b, dot + 1, dn - (size_t)(dot + 1 - d), 0));
            TRY(word(e, b, "day"));
        }
        return TTF_OK;
    }
    if (!strcmp(mode, "holiday")) {
        /* 10.1 十一, 5.1 五一, 3.8 三八; 11.11 双十一; 6.18 六月十八日 */
        long mo = strtol(a, NULL, 10), dd = strtol(bp, NULL, 10);
        if (mo == dd) {
            TRY(word(e, b, "double"));
            return integer(e, b, bp, bn, 0);
        }
        TRY(integer(e, b, a, an, 0));
        if (dd < 10)
            return integer(e, b, bp, bn, 0);
        TRY(word(e, b, "month"));
        TRY(integer(e, b, bp, bn, 0));
        return word(e, b, "day");
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
        if (!strcmp(s->secondary, "pos"))
            TRY(word(e, b, "positive"));
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
    /* 오전 9시 in a Korean sentence, 3 pm in an English one: their own language */
    {
        const ttf_language_pack_t *sentence = ttf_sentence_language(in);
        /* hanzi (a name: 王明이) stay with the request pack, as written */
        if (sentence && strcmp(sentence->language_tag, ZH) &&
            in->tokens[in->index].script != TTF_SCRIPT_HAN)
            return 0;
    }
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
