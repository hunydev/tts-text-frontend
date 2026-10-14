/* Thai pack, phase 1 (2026-10-14, project owner: Thai next, with the owner's
 * evaluation set benchmark/datasets/user-tn-eval th-TH). Numbers read as Thai
 * words (เอ็ด after a ten, ยี่สิบ, หนึ่ง before ร้อย/พัน…, ล้าน groups of six
 * digits), the dot is the decimal mark and the comma groups thousands; dates
 * (day, month name, the year as written: 2569 สองพันห้าร้อยหกสิบเก้า, พ.ศ.
 * พอสอ), times (นาฬิกา, นาที), money, percentages, units, ranges (ถึง),
 * scores and ratios (ต่อ), fractions (ส่วน), phone numbers and codes digit by
 * digit. Thai is written without spaces between words, so a cue is the end of
 * the run before a number. Thai words, Latin words and brands stay as written.
 * The core folds Thai digits ๐-๙ to ASCII in a th-TH request (numeric width
 * mode 3). The pack works only in a th-TH request (declared_only).
 *
 * classify decides the extent and the reading mode and keeps the source in
 * value; verbalize reads value in that mode. Readings are written without
 * spaces, as Thai writes a number phrase; a phone number keeps a space
 * between its digit groups. */
#include "tts_text_frontend/ttf_language_pack.h"
#include "../../src/core/internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TH "th-TH"
#define TRY(x)                                                                                     \
    do {                                                                                           \
        ttf_status_t try_status = (x);                                                             \
        if (try_status != TTF_OK)                                                                  \
            return try_status;                                                                     \
    } while (0)

static const char *res(const ttf_engine_t *e, const char *kind, const char *key) {
    return ttf_resource_get(e, kind, TH, key, NULL);
}
static int digit(char c) {
    return c >= '0' && c <= '9';
}
static int alpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
/* A Thai character (U+0E00-U+0E7F) starts at p. */
static int thai_at(const char *t, size_t n, size_t p) {
    return p + 3 <= n && (unsigned char)t[p] == 0xe0 &&
           ((unsigned char)t[p + 1] == 0xb8 || (unsigned char)t[p + 1] == 0xb9);
}
/* A Thai character ends at p. */
static int thai_before(const char *t, size_t p) {
    return p >= 3 && thai_at(t, p, p - 3);
}
static int has(const char *t, size_t n, size_t p, const char *s) {
    size_t k = strlen(s);
    return p + k <= n && !memcmp(t + p, s, k);
}

/* ---- readings ---- */

static ttf_status_t word(const ttf_engine_t *e, ttf_buffer_t *b, const char *key) {
    const char *v = res(e, "number", key);
    return v ? ttf_buffer_puts(b, v) : TTF_ERESOURCE;
}
static ttf_status_t digit_word(const ttf_engine_t *e, ttf_buffer_t *b, char c) {
    char key[3] = {'d', c, 0};
    return word(e, b, key);
}
/* Up to six digits (a group below a million). เอ็ด for a final 1 when the group has another
   non-zero digit (11 สิบเอ็ด, 101 หนึ่งร้อยเอ็ด, but 1,000,001 หนึ่งล้านหนึ่ง). */
static ttf_status_t below_million(const ttf_engine_t *e, ttf_buffer_t *b, const char *s, size_t n) {
    static const char *const place[] = {"", "ten", "hundred", "thousand", "tenthousand", "hundredthousand"};
    size_t i;
    unsigned long value = 0;
    for (i = 0; i < n; ++i)
        value = value * 10 + (unsigned long)(s[i] - '0');
    for (i = 0; i < n; ++i) {
        char d = s[i];
        size_t pos = n - 1 - i;
        if (d == '0')
            continue;
        if (pos == 1) {
            if (d == '2')
                TRY(word(e, b, "twenty"));
            else {
                if (d != '1')
                    TRY(digit_word(e, b, d));
                TRY(word(e, b, "ten"));
            }
        } else if (pos == 0) {
            if (d == '1' && value > 9)
                TRY(word(e, b, "ed"));
            else
                TRY(digit_word(e, b, d));
        } else {
            TRY(digit_word(e, b, d));
            TRY(word(e, b, place[pos]));
        }
    }
    return TTF_OK;
}
/* An integer of any length, commas allowed: groups of six digits joined by ล้าน. */
static ttf_status_t integer(const ttf_engine_t *e, ttf_buffer_t *b, const char *s, size_t n) {
    char d[64];
    size_t used = 0, i, k = 0;
    for (i = 0; i < n && used + 1 < sizeof(d); ++i)
        if (digit(s[i]))
            d[used++] = s[i];
    d[used] = 0;
    while (k + 1 < used && d[k] == '0')
        ++k;
    if (!used)
        return TTF_EINVAL;
    if (used - k == 1 && d[k] == '0')
        return digit_word(e, b, '0');
    if (used - k > 6) {
        size_t upper = used - k - 6, j;
        int lower_zero = 1;
        TRY(integer(e, b, d + k, upper));
        TRY(word(e, b, "million"));
        for (j = k + upper; j < used; ++j)
            lower_zero &= d[j] == '0';
        return lower_zero ? TTF_OK : below_million(e, b, d + k + upper, 6);
    }
    return below_million(e, b, d + k, used - k);
}
static ttf_status_t digits_each(const ttf_engine_t *e, ttf_buffer_t *b, const char *s, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i)
        if (digit(s[i]))
            TRY(digit_word(e, b, s[i]));
    return TTF_OK;
}
/* 12.5 สิบสองจุดห้า, -3 ลบสาม, 0.05 ศูนย์จุดศูนย์ห้า */
static ttf_status_t number(const ttf_engine_t *e, ttf_buffer_t *b, const char *s) {
    const char *dot;
    if (*s == '-') {
        TRY(word(e, b, "minus"));
        ++s;
    } else if (*s == '+') {
        TRY(word(e, b, "plus"));
        ++s;
    } else if (!strncmp(s, "\xc2\xb1", 2)) {
        TRY(word(e, b, "plusminus"));
        s += 2;
    }
    dot = strchr(s, '.');
    TRY(integer(e, b, s, dot ? (size_t)(dot - s) : strlen(s)));
    if (dot && dot[1]) {
        TRY(word(e, b, "point"));
        TRY(digits_each(e, b, dot + 1, strlen(dot + 1)));
    }
    return TTF_OK;
}
/* A Latin letter by its Thai name (A เอ, W ดับเบิลยู). */
static ttf_status_t letter(const ttf_engine_t *e, ttf_buffer_t *b, char c) {
    char key[2] = {(char)(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c), 0};
    const char *v = res(e, "th_letter", key);
    return v ? ttf_buffer_puts(b, v) : ttf_buffer_append(b, &c, 1);
}
/* A code: letters by name, digits one by one, a hyphen ขีด, a slash ทับ, a space kept. */
static ttf_status_t code(const ttf_engine_t *e, ttf_buffer_t *b, const char *s, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i) {
        if (digit(s[i]))
            TRY(digit_word(e, b, s[i]));
        else if (alpha(s[i]))
            TRY(letter(e, b, s[i]));
        else if (s[i] == '-')
            TRY(word(e, b, "dash"));
        else if (s[i] == '/')
            TRY(word(e, b, "slash"));
        else if (s[i] == '.')
            TRY(word(e, b, "point"));
        else if (s[i] == ' ')
            TRY(ttf_buffer_puts(b, " "));
        else if (s[i] == '#')
            TRY(word(e, b, "sharp"));
    }
    return TTF_OK;
}
/* field k of a |-separated value */
static void field(const char *v, int k, char *out, size_t cap) {
    const char *bar;
    while (k-- > 0 && v)
        v = (bar = strchr(v, '|')) ? bar + 1 : NULL;
    if (!v) {
        out[0] = 0;
        return;
    }
    bar = strchr(v, '|');
    snprintf(out, cap, "%.*s", (int)(bar ? (size_t)(bar - v) : strlen(v)), v);
}
static ttf_status_t unit_reading(const ttf_engine_t *e, ttf_buffer_t *b, const char *key) {
    const char *v;
    if (!*key)
        return TTF_OK;
    v = res(e, "th_unit", key);
    return v ? ttf_buffer_puts(b, v) : TTF_EINVAL;
}
static ttf_status_t month_name(const ttf_engine_t *e, ttf_buffer_t *b, const char *m) {
    char key[8];
    const char *v;
    snprintf(key, sizeof(key), "m%d", atoi(m));
    v = res(e, "th_month", key);
    return v ? ttf_buffer_puts(b, v) : TTF_EINVAL;
}
/* H:MM H นาฬิกา MM นาที; :00 is not said */
static ttf_status_t clock(const ttf_engine_t *e, ttf_buffer_t *b, const char *h, const char *m) {
    TRY(integer(e, b, h, strlen(h)));
    TRY(word(e, b, "oclock"));
    if (atoi(m)) {
        TRY(integer(e, b, m, strlen(m)));
        TRY(word(e, b, "minute"));
    }
    return TTF_OK;
}
/* day|month|era|year: สิบสามตุลาคม(พอสอ)สองพันห้าร้อยหกสิบเก้า */
static ttf_status_t date(const ttf_engine_t *e, ttf_buffer_t *b, const char *v) {
    char d[16], m[16], era[8], y[16];
    field(v, 0, d, sizeof(d));
    field(v, 1, m, sizeof(m));
    field(v, 2, era, sizeof(era));
    field(v, 3, y, sizeof(y));
    if (*d)
        TRY(integer(e, b, d, strlen(d)));
    TRY(month_name(e, b, m));
    if (*era)
        TRY(word(e, b, era));
    if (*y)
        TRY(integer(e, b, y, strlen(y)));
    return TTF_OK;
}

static int vowel_in(const char *s, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i)
        if (strchr("aeiouyAEIOUY", s[i]))
            return 1;
    return 0;
}
/* The Thai spelling of the Latin word at p..e (iPhone ไอโฟน, Wi-Fi ไวไฟ), or NULL. */
static const char *thai_word(const ttf_engine_t *e, const char *t, size_t p, size_t end) {
    char low[40];
    size_t k;
    if (end - p >= sizeof(low))
        return NULL;
    for (k = 0; k < end - p; ++k)
        low[k] = (char)(t[p + k] >= 'A' && t[p + k] <= 'Z' ? t[p + k] + 32 : t[p + k]);
    low[end - p] = 0;
    return res(e, "electronic_word", low);
}
/* A Thai consonant at p read as an abbreviation letter: its initial sound and อ (ภ พอ, ง งอ). */
static const char *consonant_sound(const char *t, size_t n, size_t p) {
    static const char *const map[][2] = {
        {"ก", "กอ"}, {"ข", "ขอ"}, {"ค", "คอ"}, {"ฆ", "คอ"}, {"ง", "งอ"}, {"จ", "จอ"}, {"ฉ", "ฉอ"},
        {"ช", "ชอ"}, {"ซ", "ซอ"}, {"ฌ", "ชอ"}, {"ญ", "ยอ"}, {"ฎ", "ดอ"}, {"ฏ", "ตอ"}, {"ฐ", "ถอ"},
        {"ฑ", "ทอ"}, {"ฒ", "ทอ"}, {"ณ", "นอ"}, {"ด", "ดอ"}, {"ต", "ตอ"}, {"ถ", "ถอ"}, {"ท", "ทอ"},
        {"ธ", "ทอ"}, {"น", "นอ"}, {"บ", "บอ"}, {"ป", "ปอ"}, {"ผ", "ผอ"}, {"ฝ", "ฝอ"}, {"พ", "พอ"},
        {"ฟ", "ฟอ"}, {"ภ", "พอ"}, {"ม", "มอ"}, {"ย", "ยอ"}, {"ร", "รอ"}, {"ล", "ลอ"}, {"ว", "วอ"},
        {"ศ", "สอ"}, {"ษ", "สอ"}, {"ส", "สอ"}, {"ห", "หอ"}, {"ฬ", "ลอ"}, {"อ", "ออ"}, {"ฮ", "ฮอ"}};
    size_t k;
    for (k = 0; k < sizeof(map) / sizeof(*map); ++k)
        if (has(t, n, p, map[k][0]))
            return map[k][1];
    return NULL;
}
/* The text before p (spaces skipped) ends with a written month (ต.ค., ตุลาคม). */
static int month_before(const ttf_engine_t *e, const char *t, size_t p) {
    size_t q = p, l;
    char probe[32];
    while (q && t[q - 1] == ' ')
        --q;
    for (l = 3; l < sizeof(probe) && l <= q; ++l) {
        memcpy(probe, t + q - l, l);
        probe[l] = 0;
        if (res(e, "th_month_key", probe))
            return 1;
    }
    return 0;
}

/* ---- classification ---- */

static size_t finish(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t end, const char *mode,
                     const char *value, const char *secondary) {
    size_t count = 0;
    if (strlen(value) >= sizeof(s->value) || strlen(secondary) >= sizeof(s->secondary))
        return 0;
    while (in->index + count < in->token_count && in->tokens[in->index + count].begin < end)
        ++count;
    snprintf(s->value, sizeof(s->value), "%s", value);
    snprintf(s->secondary, sizeof(s->secondary), "%s", secondary);
    snprintf(s->rule, sizeof(s->rule), "th.%s", mode);
    snprintf(s->unit, sizeof(s->unit), "@th-%s", mode);
    snprintf(s->source_lang, sizeof(s->source_lang), TH);
    s->input_end = end;
    s->type = TTF_SEM_COUNTER;
    return count;
}
static size_t spaces(const char *t, size_t n, size_t p) {
    while (p < n && t[p] == ' ')
        ++p;
    return p;
}
/* The run before p (spaces skipped) ends with one of the |-separated cue words of th_cue key. */
static int cue_before(const ttf_engine_t *e, const char *t, size_t p, const char *key) {
    const char *words = res(e, "th_cue", key), *w;
    size_t q = p;
    while (q && (t[q - 1] == ' ' || t[q - 1] == ':'))
        --q;
    for (w = words; w && *w;) {
        size_t l = strcspn(w, "|");
        if (l && q >= l && !memcmp(t + q - l, w, l))
            return 1;
        w += l + (w[l] == '|');
    }
    return 0;
}
/* The same within a window of bytes before p (a cue a few words back: โทร สายด่วน 1669). */
static int cue_within(const ttf_engine_t *e, const char *t, size_t p, size_t window, const char *key) {
    const char *words = res(e, "th_cue", key), *w;
    size_t from = p > window ? p - window : 0;
    for (w = words; w && *w;) {
        size_t l = strcspn(w, "|"), k;
        for (k = from; l && k + l <= p; ++k)
            if (!memcmp(t + k, w, l))
                return 1;
        w += l + (w[l] == '|');
    }
    return 0;
}
/* A range dash at p before a digit: -, – or the same with a space on each side
   (100 - 200 บาท); its length with the spaces, or 0. */
static size_t dash_len(const char *t, size_t n, size_t p) {
    size_t a = p < n && t[p] == ' ' ? p + 1 : p, b;
    if (has(t, n, a, "-"))
        b = a + 1;
    else if (has(t, n, a, "–"))
        b = a + 3;
    else
        return 0;
    if (a > p) {
        if (!(b < n && t[b] == ' '))
            return 0;
        ++b;
    }
    return b < n && digit(t[b]) ? b - p : 0;
}
/* The longest of the |-separated cue words of th_cue key that starts at p; its length, or 0. */
static size_t cue_at(const ttf_engine_t *e, const char *t, size_t n, size_t p, const char *key) {
    const char *words = res(e, "th_cue", key), *w;
    size_t best = 0;
    for (w = words; w && *w;) {
        size_t l = strcspn(w, "|");
        if (l > best && p + l <= n && !memcmp(t + p, w, l))
            best = l;
        w += l + (w[l] == '|');
    }
    return best;
}
/* Digits with comma groups and one decimal point at p; the end, or p when none. */
static size_t scan_number(const char *t, size_t n, size_t p, int *grouped, int *decimal) {
    size_t q = p;
    *grouped = *decimal = 0;
    while (q < n && digit(t[q]))
        ++q;
    if (q == p)
        return p;
    while (q + 3 < n + 0 && t[q] == ',' && digit(t[q + 1]) && digit(t[q + 2]) && digit(t[q + 3]) &&
           !(q + 4 < n && digit(t[q + 4]))) {
        q += 4;
        *grouped = 1;
    }
    if (q + 1 < n && t[q] == '.' && digit(t[q + 1])) {
        size_t z = q + 1;
        while (z < n && digit(t[z]))
            ++z;
        /* 1.2.3 is a version, not a decimal */
        if (!(z + 1 < n && t[z] == '.' && digit(t[z + 1]))) {
            q = z;
            *decimal = 1;
        }
    }
    return q;
}
/* A unit after a number at p (one space allowed): the end, with the key in key. Thai
   abbreviations (กม. ชม. ตร.ม.) and Latin symbols (km, GB, °C); a compound with / (กม./ชม.). */
static size_t unit_after(const ttf_engine_t *e, const char *t, size_t n, size_t p, char *key, size_t cap) {
    size_t q = p < n && t[p] == ' ' ? p + 1 : p, best = 0, l;
    char probe[40];
    for (l = 1; l < sizeof(probe) && q + l <= n; ++l) {
        size_t end = q + l;
        /* stop inside a UTF-8 character */
        if (end < n && ((unsigned char)t[end] & 0xc0) == 0x80)
            continue;
        memcpy(probe, t + q, l);
        probe[l] = 0;
        if (!res(e, "th_unit", probe))
            continue;
        /* a Latin unit ends a word (5 km, not 5 kmh); a Thai abbreviation ends with its dot */
        if (alpha(t[end - 1]) && end < n && alpha(t[end]))
            continue;
        if (thai_at(t, n, end - 3) && end < n && thai_at(t, n, end) && probe[l - 1] != '.')
            continue;
        best = end;
        snprintf(key, cap, "%s", probe);
    }
    return best;
}
/* ---- maths (held-out v1, 2026-10-14): 12 + 8 = 20, x² + 2x - 3 = 0, √81 = 9, 45 ÷ 9 ----
   rel marks a relation, strong an operator that alone makes an expression, post a mark after
   its operand (²). - alone is no expression (3-5 is a range, 2-1 a score). */
static const struct {
    const char *mark, *key;
    int rel, strong, post;
} TH_MATH[] = {{"≥", "greaterequal", 1, 0, 0}, {"≤", "lessequal", 1, 0, 0}, {"≠", "notequal", 1, 0, 0},
               {"=", "equals", 1, 0, 0},       {">", "greater", 1, 0, 0},   {"<", "less", 1, 0, 0},
               {"×", "times", 0, 1, 0},        {"÷", "divide", 0, 1, 0},    {"√", "root", 0, 1, 0},
               {"²", "square", 0, 1, 1},       {"³", "cube", 0, 1, 1},      {"+", "plus", 0, 0, 0},
               {"−", "minus", 0, 0, 0},        {"-", "minus", 0, 0, 0}};
enum { TM_NONE, TM_NUMBER, TM_LETTER, TM_MARK };
/* One element at p: a number (12, 1.75, 1,000), one ASCII letter (x, a) or a mark; its length. */
static size_t math_element(const char *t, size_t n, size_t p, int *kind, int *mark) {
    size_t k;
    *kind = TM_NONE, *mark = -1;
    if (p >= n)
        return 0;
    if (digit(t[p])) {
        int g, d;
        *kind = TM_NUMBER;
        return scan_number(t, n, p, &g, &d) - p;
    }
    if (alpha(t[p]) && !(p + 1 < n && alpha(t[p + 1]))) {
        *kind = TM_LETTER;
        return 1;
    }
    for (k = 0; k < sizeof(TH_MATH) / sizeof(*TH_MATH); ++k)
        if (has(t, n, p, TH_MATH[k].mark)) {
            *kind = TM_MARK, *mark = (int)k;
            return strlen(TH_MATH[k].mark);
        }
    return 0;
}
/* The end of an expression at p, or 0: two operands with a relation, a strong operator or +
   between them, or one operand with √ or ². Letters alone need a relation (a > b). */
static size_t math_end(const char *t, size_t n, size_t p) {
    size_t z = p, last = 0, len;
    int rel = 0, strong = 0, plus = 0, numbers = 0, operands = 0, single = 0, after = 0, kind, mark;
    while (z < n) {
        size_t s0 = z > p && t[z] == ' ' ? z + 1 : z;
        if (!(len = math_element(t, n, s0, &kind, &mark)))
            break;
        if (kind == TM_NUMBER || kind == TM_LETTER) {
            /* 12 8, a b: two operands without an operator are prose; 2x is a product */
            if (after && (kind == TM_NUMBER || s0 > z))
                break;
            numbers += kind == TM_NUMBER;
            ++operands;
            after = 1;
            last = s0 + len;
        } else if (TH_MATH[mark].post) {
            if (!after)
                break;
            single = strong = 1;
            last = s0 + len;
        } else if (!strcmp(TH_MATH[mark].mark, "√")) {
            if (after)
                break;
            single = strong = 1;
        } else {
            if (!after)
                break;
            rel |= TH_MATH[mark].rel;
            strong |= TH_MATH[mark].strong;
            plus |= *TH_MATH[mark].mark == '+';
            after = 0;
        }
        z = s0 + len;
    }
    if (!last || (!numbers && !rel) || !(rel || strong || plus) || (operands < 2 && !single))
        return 0;
    return last;
}
static ttf_status_t math_reading(const ttf_engine_t *e, ttf_buffer_t *b, const char *v) {
    size_t n = strlen(v), z = 0, len, i, k;
    int kind, mark;
    char num[64];
    while (z < n) {
        if (v[z] == ' ') {
            ++z;
            continue;
        }
        if (!(len = math_element(v, n, z, &kind, &mark)))
            return TTF_EINVAL;
        if (kind == TM_NUMBER) {
            for (i = z, k = 0; i < z + len && k + 1 < sizeof(num); ++i)
                if (v[i] != ',')
                    num[k++] = v[i];
            num[k] = 0;
            TRY(number(e, b, num));
        } else if (kind == TM_LETTER)
            TRY(letter(e, b, v[z]));
        else
            TRY(word(e, b, TH_MATH[mark].key));
        z += len;
    }
    return TTF_OK;
}
/* A Thai abbreviation of th_abbr that may stand anywhere (เดือน ต.ค., พ.ศ.) at p; the end. */
static size_t abbr_at(const ttf_engine_t *e, const char *t, size_t n, size_t p, char *key, size_t cap) {
    size_t l, best = 0;
    char probe[32];
    for (l = 2; l < sizeof(probe) && p + l <= n; ++l) {
        if (p + l < n && ((unsigned char)t[p + l] & 0xc0) == 0x80)
            continue;
        memcpy(probe, t + p, l);
        probe[l] = 0;
        if (res(e, "th_abbr", probe)) {
            best = p + l;
            snprintf(key, cap, "%s", probe);
        }
    }
    return best;
}
/* A month after a day at p (one space allowed): ต.ค., ตุลาคม; the end and the number 1-12. */
static size_t month_after(const ttf_engine_t *e, const char *t, size_t n, size_t p, int *month) {
    size_t q = p < n && t[p] == ' ' ? p + 1 : p, l, best = 0;
    char probe[32];
    for (l = 3; l < sizeof(probe) && q + l <= n; ++l) {
        const char *v;
        if (q + l < n && ((unsigned char)t[q + l] & 0xc0) == 0x80)
            continue;
        memcpy(probe, t + q, l);
        probe[l] = 0;
        if ((v = res(e, "th_month_key", probe)) != NULL) {
            best = q + l;
            *month = atoi(v);
        }
    }
    return best;
}
/* An optional year after a month at p: (space) (พ.ศ./ค.ศ./ปี) (space) 2 or 4 digits. */
static size_t year_after(const char *t, size_t n, size_t p, char *era, size_t ecap, char *y, size_t ycap) {
    size_t q = spaces(t, n, p), z;
    era[0] = y[0] = 0;
    if (has(t, n, q, "พ.ศ.")) {
        snprintf(era, ecap, "be");
        q = spaces(t, n, q + strlen("พ.ศ."));
    } else if (has(t, n, q, "ค.ศ.")) {
        snprintf(era, ecap, "ce");
        q = spaces(t, n, q + strlen("ค.ศ."));
    }
    for (z = q; z < n && digit(t[z]); ++z)
        ;
    if ((z - q == 4 || z - q == 2) && !(z < n && (digit(t[z]) || t[z] == '.' || t[z] == ','))) {
        snprintf(y, ycap, "%.*s", (int)(z - q), t + q);
        return z;
    }
    era[0] = 0;
    return p;
}

static size_t classify(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const ttf_engine_t *e = in->engine;
    const char *t = in->text;
    size_t n = in->length, p, q, z, u;
    char v[TTF_IR_VALUE_SIZE], key[40], era[8], y[16], sign[4] = "";
    int grouped, decimal, month;
    if (in->index >= in->token_count)
        return 0;
    p = in->tokens[in->index].begin;
    if (p >= n)
        return 0;

    /* ½ ครึ่ง */
    if (has(t, n, p, "½"))
        return finish(in, s, p + 2, "lit", res(e, "number", "half") ? res(e, "number", "half") : "½", "");
    /* 12 + 8 = 20, √81 = 9, x > 5: an expression */
    if ((digit(t[p]) || has(t, n, p, "√") || (alpha(t[p]) && !(p && alpha(t[p - 1])))) &&
        !(p && (digit(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ','))) {
        size_t me = math_end(t, n, p);
        /* 3×4 ม. stays a product with its unit; an equation takes its unit as written */
        if (me && me - p < sizeof(v) && !(unit_after(e, t, n, me, key, sizeof(key)) && !memchr(t + p, '=', me - p))) {
            snprintf(v, sizeof(v), "%.*s", (int)(me - p), t + p);
            return finish(in, s, me, "math", v, "");
        }
    }
    /* ~500 บาท ประมาณห้าร้อยบาท */
    if (t[p] == '~' && spaces(t, n, p + 1) < n && digit(t[spaces(t, n, p + 1)]))
        return finish(in, s, p + 1, "lit", res(e, "number", "approx"), "");
    /* จันทร์-ศุกร์ จันทร์ถึงศุกร์: a dash between weekdays */
    if ((t[p] == '-' || has(t, n, p, "–")) && cue_before(e, t, p, "weekday") &&
        cue_at(e, t, n, spaces(t, n, p + (t[p] == '-' ? 1 : 3)), "weekday"))
        return finish(in, s, p + (t[p] == '-' ? 1 : 3), "lit", res(e, "number", "to"), "");
    /* 1 ม.ค. - 31 มี.ค., ต.ค.-ธ.ค.: a dash between dates or months reads ถึง */
    if ((t[p] == '-' || has(t, n, p, "–")) && month_before(e, t, p)) {
        size_t w = t[p] == '-' ? 1 : 3, a = spaces(t, n, p + w);
        int m2;
        if (a < n && (digit(t[a]) || month_after(e, t, n, a, &m2) != 0))
            return finish(in, s, a, "lit", res(e, "number", "to"), "");
    }
    /* ---- Thai text: an abbreviation (ต.ค. ตุลาคม, พ.ศ. พอสอ), ม.3/5 มอสามทับห้า ---- */
    if (thai_at(t, n, p) && !has(t, n, p, "฿")) {
        size_t end = in->tokens[in->index].end < n ? in->tokens[in->index].end : n, at;
        /* ม.3/5: a school class */
        if (has(t, n, p, "ม.") && p + strlen("ม.") < n && digit(t[p + strlen("ม.")]) && !thai_before(t, p)) {
            q = p + strlen("ม.");
            for (z = q; z < n && digit(t[z]); ++z)
                ;
            if (z + 1 < n && t[z] == '/' && digit(t[z + 1])) {
                size_t z2 = z + 1;
                while (z2 < n && digit(t[z2]))
                    ++z2;
                snprintf(v, sizeof(v), "%.*s|%.*s", (int)(z - q), t + q, (int)(z2 - z - 1), t + z + 1);
                return finish(in, s, z2, "class", v, "");
            }
            snprintf(v, sizeof(v), "%.*s|", (int)(z - q), t + q);
            return finish(in, s, z, "class", v, "");
        }
        /* นร 0505/ว 212, สธ 0201/ว 1234: a government document number; ฮย 2047: a plate */
        if (!thai_before(t, p)) {
            size_t c = p;
            while (c + 3 <= n && consonant_sound(t, n, c) && c - p < 9)
                c += 3;
            if (c - p >= 6 && c + 1 < n && t[c] == ' ' && digit(t[c + 1]) && !(c < n && thai_at(t, n, c))) {
                size_t d = c + 1, d1;
                while (d < n && digit(t[d]))
                    ++d;
                /* มท 0808.2/ว 1234 */
                if (d + 1 < n && t[d] == '.' && digit(t[d + 1]))
                    for (++d; d < n && digit(t[d]); ++d)
                        ;
                d1 = d;
                /* /ว 212 */
                if (d + 1 < n && t[d] == '/') {
                    size_t c2 = d + 1;
                    while (c2 + 3 <= n && consonant_sound(t, n, c2) && c2 - d - 1 < 9)
                        c2 += 3;
                    if (c2 > d + 1 && c2 + 1 < n && t[c2] == ' ' && digit(t[c2 + 1])) {
                        d1 = c2 + 1;
                        while (d1 < n && digit(t[d1]))
                            ++d1;
                    }
                }
                if ((d1 > d || d - c - 1 == 4) && !(d1 < n && (digit(t[d1]) || alpha(t[d1])))) {
                    snprintf(v, sizeof(v), "%.*s", (int)(d1 - p), t + p);
                    return finish(in, s, d1, "doc", v, "");
                }
            }
        }
        /* ภ.ง.ด.91 พองอดอเก้าสิบเอ็ด, ป.4 ปอสี่: single consonants with dots, then maybe a number */
        if (!thai_before(t, p) && consonant_sound(t, n, p) && p + 4 <= n && t[p + 3] == '.' && !abbr_at(e, t, n, p, key, sizeof(key))) {
            size_t a = p, used = 0;
            int letters = 0;
            v[0] = 0;
            while (a + 4 <= n && consonant_sound(t, n, a) && t[a + 3] == '.' && !(a + 4 < n && thai_at(t, n, a + 4) && !consonant_sound(t, n, a + 4))) {
                const char *snd = consonant_sound(t, n, a);
                if (used + strlen(snd) + 1 >= sizeof(v))
                    break;
                used += (size_t)snprintf(v + used, sizeof(v) - used, "%s", snd);
                a += 4;
                ++letters;
                if (!(a + 4 <= n && thai_at(t, n, a)))
                    break;
            }
            if (letters >= 2 || (letters == 1 && a < n && digit(t[a]))) {
                if (a < n && digit(t[a])) {
                    size_t z2 = a;
                    while (z2 < n && digit(t[z2]))
                        ++z2;
                    snprintf(v + used, sizeof(v) - used, "|%.*s", (int)(z2 - a), t + a);
                    return finish(in, s, z2, "abbrnum", v, "");
                }
                return finish(in, s, a, "lit", v, "");
            }
        }
        /* the abbreviation at the token or at the end of the run (จนถึงต.ค.) */
        for (at = p; at < end; at += 3) {
            size_t ae = abbr_at(e, t, n, at, key, sizeof(key));
            if (ae && !(ae < n && thai_at(t, n, ae) && t[ae - 1] != '.')) {
                snprintf(v, sizeof(v), "%.*s%s", (int)(at - p), t + p, res(e, "th_abbr", key));
                return finish(in, s, ae, "lit", v, "");
            }
            if (!thai_at(t, n, at))
                break;
        }
        return 0;
    }

    /* ---- Latin: v5.2.0 เวอร์ชัน, codes A4 GE101 ORD-2569-00481 ---- */
    /* @shop88 แอทช็อปแปดแปด: a handle reads like an address */
    if (t[p] == '@' && p + 1 < n && (alpha(t[p + 1]) || digit(t[p + 1])) && !(p && (alpha(t[p - 1]) || digit(t[p - 1])))) {
        for (q = p + 1; q < n && (alpha(t[q]) || digit(t[q]) || t[q] == '_' || t[q] == '.') && q - p < 60; ++q)
            ;
        while (q > p + 1 && t[q - 1] == '.')
            --q;
        snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
        return finish(in, s, q, "address", v, "");
    }
    if (alpha(t[p]) && !(p && (alpha(t[p - 1]) || digit(t[p - 1])))) {
        size_t letters = 0, digits_n = 0;
        /* budget_2026.xlsx, manual_v2.pdf: a file name reads like an address */
        {
            static const char *const exts[] = {".pdf", ".xlsx", ".xls", ".docx", ".doc", ".pptx", ".csv", ".txt",
                                               ".jpg", ".jpeg", ".png", ".zip", ".mp4", ".mp3", ".json", ".xml"};
            size_t r = p, k;
            while (r < n && (alpha(t[r]) || digit(t[r]) || t[r] == '_' || t[r] == '-' || t[r] == '.') && r - p < 80)
                ++r;
            while (r > p && t[r - 1] == '.')
                --r;
            for (k = 0; k < sizeof(exts) / sizeof(*exts); ++k) {
                size_t l = strlen(exts[k]);
                if (r - p > l && !memcmp(t + r - l, exts[k], l) && !(r < n && alpha(t[r]))) {
                    snprintf(v, sizeof(v), "%.*s", (int)(r - p), t + p);
                    return finish(in, s, r, "address", v, "");
                }
            }
        }
        /* iPhone 13 ไอโฟนสิบสาม, Boeing 737 โบอิ้งเจ็ดสามเจ็ด, SAVE50 เซฟห้าสิบ, coffee2024,
           COVID-19 โควิดสิบเก้า, LOT 2409B ล็อต: an English word before a number takes its Thai
           spelling (an English word elsewhere stays as written) */
        {
            size_t l = p;
            const char *th;
            while (l < n && (alpha(t[l]) || (t[l] == '-' && l + 1 < n && alpha(t[l + 1]) && l > p)))
                ++l;
            if (l - p >= 2 && (th = thai_word(e, t, p, l)) != NULL) {
                size_t d0 = l < n && (t[l] == ' ' || t[l] == '-') ? l + 1 : l, d1 = d0;
                while (d1 < n && digit(t[d1]))
                    ++d1;
                if (d1 > d0 && d1 - d0 <= 4 && !(d1 < n && (alpha(t[d1]) || t[d1] == '.' || t[d1] == '/' ||
                                                             t[d1] == ',' || t[d1] == '%' || digit(t[d1])))) {
                    snprintf(v, sizeof(v), "%s|%.*s", th, (int)(d1 - d0), t + d0);
                    return finish(in, s, d1, "wordnum", v, "");
                }
                if (d1 > d0 || (l < n && t[l] == ' ' && l + 1 < n && digit(t[l + 1])))
                    return finish(in, s, l, "lit", th, "");
            }
        }
        /* UTC+7 ยูทีซีบวกเจ็ด; SN 7H2, LOT 2409B: two to four capitals before a number are spelled */
        {
            size_t l = p;
            while (l < n && t[l] >= 'A' && t[l] <= 'Z')
                ++l;
            if (l - p >= 2 && l - p <= 4 && l + 1 < n && digit(t[l + 1]) &&
                (t[l] == '+' || (t[l] == '-' && (has(t, n, p, "UTC") || has(t, n, p, "GMT"))))) {
                size_t d1 = l + 1;
                while (d1 < n && digit(t[d1]))
                    ++d1;
                snprintf(v, sizeof(v), "%.*s|%c%.*s", (int)(l - p), t + p, t[l], (int)(d1 - l - 1), t + l + 1);
                return finish(in, s, d1, "lettersnum", v, "");
            }
            if (l - p >= 2 && l - p <= 4 && l + 1 < n && t[l] == ' ' && digit(t[l + 1]) &&
                !(has(t, n, p, "THB") || has(t, n, p, "USD") || has(t, n, p, "EUR") || has(t, n, p, "JPY"))) {
                size_t d1 = l + 1, nd;
                while (d1 < n && digit(t[d1]))
                    ++d1;
                nd = d1 - l - 1;
                /* TG 628 ทีจีหกสองแปด, HN 650123: a flight or record code reads digit by digit
                   (not a year: APEC 2022; not a quantity: RAM 512 GB) */
                if (nd >= 3 && nd <= 6 &&
                    !(nd == 4 && (has(t, n, l + 1, "19") || has(t, n, l + 1, "20") || has(t, n, l + 1, "25"))) &&
                    !(d1 < n && (alpha(t[d1]) || digit(t[d1]) || strchr(".,%/:-", t[d1]))) &&
                    !unit_after(e, t, n, d1, key, sizeof(key))) {
                    snprintf(v, sizeof(v), "%.*s", (int)(d1 - p), t + p);
                    return finish(in, s, d1, "code", v, "");
                }
                snprintf(v, sizeof(v), "%.*s", (int)(l - p), t + p);
                return finish(in, s, l, "code", v, "");
            }
        }
        /* THB 3,210 สามพันสองร้อยสิบบาท, USD 50 */
        if (p + 4 < n && t[p + 3] == ' ' && digit(t[p + 4]) &&
            (has(t, n, p, "THB") || has(t, n, p, "USD") || has(t, n, p, "EUR") || has(t, n, p, "JPY"))) {
            size_t end = scan_number(t, n, p + 4, &grouped, &decimal);
            snprintf(v, sizeof(v), "%.*s|%.3s", (int)(end - p - 4), t + p + 4, t + p);
            return finish(in, s, end, "num", v, "");
        }
        /* E20 อียี่สิบ, A4 เอสี่, F-16 เอฟสิบหก: a few capitals and a short number */
        {
            size_t l = p;
            while (l < n && t[l] >= 'A' && t[l] <= 'Z')
                ++l;
            if (l - p >= 1 && l - p <= 3) {
                size_t d0 = l + (l < n && t[l] == '-'), d1 = d0;
                while (d1 < n && digit(t[d1]))
                    ++d1;
                if (d1 > d0 && d1 - d0 <= 2 && !(d1 < n && (alpha(t[d1]) || digit(t[d1]) || t[d1] == '-' ||
                                                             (t[d1] == '.' && d1 + 1 < n && digit(t[d1 + 1]))))) {
                    snprintf(v, sizeof(v), "%.*s|%.*s", (int)(l - p), t + p, (int)(d1 - d0), t + d0);
                    return finish(in, s, d1, "lettersnum", v, "");
                }
                /* PM2.5 พีเอ็มสองจุดห้า */
                if (d1 > d0 && d1 < n && t[d1] == '.' && d1 + 1 < n && digit(t[d1 + 1])) {
                    size_t d2 = d1 + 1;
                    while (d2 < n && digit(t[d2]))
                        ++d2;
                    if (!(d2 < n && (alpha(t[d2]) || digit(t[d2]) || t[d2] == '.'))) {
                        snprintf(v, sizeof(v), "%.*s|%.*s", (int)(l - p), t + p, (int)(d2 - d0), t + d0);
                        return finish(in, s, d2, "lettersnum", v, "");
                    }
                }
            }
        }
        if ((t[p] == 'v' || t[p] == 'V') && p + 1 < n && digit(t[p + 1])) {
            for (q = p + 1; q < n && (digit(t[q]) || (t[q] == '.' && q + 1 < n && digit(t[q + 1]))); ++q)
                ;
            /* v0.9-beta เวอร์ชันศูนย์จุดเก้าขีดเบต้า */
            if (q + 2 < n && t[q] == '-' && alpha(t[q + 1])) {
                size_t r = q + 1;
                const char *th;
                while (r < n && alpha(t[r]))
                    ++r;
                if ((th = thai_word(e, t, q + 1, r)) != NULL && !(r < n && (alpha(t[r]) || digit(t[r])))) {
                    snprintf(v, sizeof(v), "%.*s|%s", (int)(q - p - 1), t + p + 1, th);
                    return finish(in, s, r, "version", v, "");
                }
            }
            if (!(q < n && alpha(t[q]))) {
                snprintf(v, sizeof(v), "%.*s", (int)(q - p - 1), t + p + 1);
                return finish(in, s, q, "version", v, "");
            }
        }
        for (q = p; q < n && (alpha(t[q]) || digit(t[q]) ||
                              (t[q] == '-' && q + 1 < n && (alpha(t[q + 1]) || digit(t[q + 1])) && q > p));
             ++q) {
            letters += alpha(t[q]) != 0;
            digits_n += digit(t[q]) != 0;
        }
        /* letters with digits; a lowercase word with digits (coffee2024) keeps the word */
        if (digits_n && letters && q - p < sizeof(v)) {
            size_t lw = 0;
            while (p + lw < q && t[p + lw] >= 'a' && t[p + lw] <= 'z')
                ++lw;
            if (lw >= 3 && p + lw < q && digit(t[p + lw])) {
                snprintf(v, sizeof(v), "%.*s|%.*s", (int)lw, t + p, (int)(q - p - lw), t + p + lw);
                return finish(in, s, q, "wordcode", v, "");
            }
            snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
            return finish(in, s, q, "code", v, "");
        }
        return 0;
    }

    /* ---- a sign or a currency before the number ---- */
    q = p;
    /* โควิด-19 โควิดสิบเก้า: a hyphen joining a Thai word to a number is silent */
    if (t[q] == '-' && q + 1 < n && digit(t[q + 1]) && thai_before(t, q))
        return finish(in, s, q + 1, "lit", "", "");
    if ((t[q] == '-' || t[q] == '+') && q + 1 < n && digit(t[q + 1]) &&
        !(q && (digit(t[q - 1]) || alpha(t[q - 1])))) {
        sign[0] = t[q];
        sign[1] = 0;
        ++q;
    } else if (has(t, n, q, "\xc2\xb1") && spaces(t, n, q + 2) < n && digit(t[spaces(t, n, q + 2)])) {
        /* ±3%, ± 5 ก. */
        snprintf(sign, sizeof(sign), "\xc2\xb1");
        q = spaces(t, n, q + 2);
    }
    if (q < n && (t[q] == '$' || has(t, n, q, "฿") || has(t, n, q, "€") || has(t, n, q, "£") ||
                  has(t, n, q, "¥"))) {
        size_t w = t[q] == '$' ? 1 : has(t, n, q, "฿") || has(t, n, q, "€") ? 3 : 2, end;
        char cur[8];
        snprintf(cur, sizeof(cur), "%.*s", (int)w, t + q);
        if (q + w < n && digit(t[q + w])) {
            end = scan_number(t, n, q + w, &grouped, &decimal);
            snprintf(v, sizeof(v), "%s%.*s|%s", sign, (int)(end - q - w), t + q + w, cur);
            return finish(in, s, end, "num", v, "");
        }
        return 0;
    }
    if (t[p] == '#' && p + 1 < n && digit(t[p + 1])) {
        for (q = p + 1; q < n && (digit(t[q]) || (t[q] == '-' && q + 1 < n && digit(t[q + 1]))); ++q)
            ;
        snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
        return finish(in, s, q, "code", v, "");
    }
    if (q >= n || !digit(t[q]) || (q == p && p && (digit(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ',')))
        return 0;

    /* ---- digit patterns ---- */
    for (z = q; z < n && digit(t[z]); ++z)
        ;
    /* 192.168.1.1: an IP address, each part digit by digit */
    if (!*sign && z - q <= 3) {
        size_t r = z, parts = 1;
        while (r + 1 < n && t[r] == '.' && digit(t[r + 1]) && parts < 4) {
            size_t r2 = r + 1;
            while (r2 < n && digit(t[r2]))
                ++r2;
            if (r2 - r - 1 > 3)
                break;
            r = r2;
            ++parts;
        }
        if (parts == 4 && !(r < n && (digit(t[r]) || (t[r] == '.' && r + 1 < n && digit(t[r + 1]))))) {
            snprintf(v, sizeof(v), "%.*s", (int)(r - q), t + q);
            return finish(in, s, r, "ip", v, "");
        }
    }
    /* 0 1055 60001 23 4, 3-1022-01234-56-7: a Thai ID number of thirteen digits (1-4-5-2-1) */
    if (!*sign && z - q == 1 && z + 1 < n && (t[z] == ' ' || t[z] == '-')) {
        static const size_t sizes[] = {4, 5, 2, 1};
        size_t r = z, k;
        char sep = t[z];
        for (k = 0; k < 4; ++k) {
            size_t r2 = r + 1;
            if (!(r < n && t[r] == sep && r2 < n && digit(t[r2])))
                break;
            while (r2 < n && digit(t[r2]))
                ++r2;
            if (r2 - r - 1 != sizes[k])
                break;
            r = r2;
        }
        if (k == 4 && !(r < n && digit(t[r]))) {
            snprintf(v, sizeof(v), "%.*s", (int)(r - q), t + q);
            return finish(in, s, r, "phone", v, "");
        }
    }
    /* +66 2 123 4567 บวกหกหก สอง หนึ่งสองสาม สี่ห้าหกเจ็ด: an international number */
    if (sign[0] == '+' && z - q >= 1 && z - q <= 3 && z + 1 < n && (t[z] == ' ' || t[z] == '-') && digit(t[z + 1])) {
        size_t r = z, groups = 1, digits_n = z - q;
        while (r + 1 < n && (t[r] == ' ' || t[r] == '-') && digit(t[r + 1])) {
            size_t r2 = r + 1;
            while (r2 < n && digit(t[r2]))
                ++r2;
            if (r2 - r - 1 > 4)
                break;
            digits_n += r2 - r - 1;
            r = r2;
            ++groups;
        }
        if (groups >= 3 && digits_n >= 9) {
            snprintf(v, sizeof(v), "+%.*s", (int)(r - q), t + q);
            return finish(in, s, r, "phone", v, "");
        }
    }
    /* phone numbers: 02-123-4567, 081-234-5678, 0-2579-0123, 0 2222 0001, 0812345678 */
    if (!*sign && t[q] == '0' && z - q <= 3) {
        size_t r = z, groups = 1, digits_n = z - q;
        char sep = 0;
        while (r + 1 < n && (t[r] == '-' || t[r] == ' ') && (!sep || t[r] == sep) && digit(t[r + 1])) {
            size_t r2 = r + 1;
            while (r2 < n && digit(t[r2]))
                ++r2;
            if (r2 - r - 1 < 3 || r2 - r - 1 > 4)
                break;
            sep = t[r];
            digits_n += r2 - r - 1;
            r = r2;
            ++groups;
        }
        if (groups >= 3 && digits_n >= 9 && digits_n <= 10 && !(r < n && (digit(t[r]) || alpha(t[r])))) {
            snprintf(v, sizeof(v), "%.*s", (int)(r - q), t + q);
            return finish(in, s, r, "phone", v, "");
        }
    }
    if (!*sign && z - q == 10 && t[q] == '0' && !(z < n && (digit(t[z]) || t[z] == '.' || t[z] == ','))) {
        snprintf(v, sizeof(v), "%.3s %.3s %.4s", t + q, t + q + 3, t + q + 6);
        return finish(in, s, z, "phone", v, "");
    }
    /* 1800-123-456: a toll-free number */
    if (!*sign && z - q == 4 && has(t, n, q, "1800") && z + 1 < n && t[z] == '-' && digit(t[z + 1])) {
        size_t r = z;
        while (r + 1 < n && t[r] == '-' && digit(t[r + 1]))
            for (++r; r < n && digit(t[r]); ++r)
                ;
        snprintf(v, sizeof(v), "%.*s", (int)(r - q), t + q);
        return finish(in, s, r, "phone", v, "");
    }
    /* 123-4-56789-0: three or more hyphen-joined digit groups (an account) read digit by digit;
       13-10-2569 and 2569-10-13 stay dates */
    if (!*sign && z + 1 < n && t[z] == '-' && digit(t[z + 1])) {
        size_t r = z, groups = 1, len[3] = {z - q, 0, 0};
        while (r + 1 < n && t[r] == '-' && digit(t[r + 1])) {
            size_t r0 = ++r;
            while (r < n && digit(t[r]))
                ++r;
            if (groups < 3)
                len[groups] = r - r0;
            ++groups;
        }
        if (groups >= 3 && !(r < n && (alpha(t[r]) || t[r] == '.' || t[r] == ',' || t[r] == '/')) &&
            !(groups == 3 && ((len[0] <= 2 && len[1] <= 2 && len[2] == 4) || (len[0] == 4 && len[1] <= 2 && len[2] <= 2)))) {
            snprintf(v, sizeof(v), "%.*s", (int)(r - q), t + q);
            return finish(in, s, r, "phone", v, "");
        }
    }
    /* 1.0.7 หนึ่งจุดศูนย์จุดเจ็ด, iOS 17.4.1: three dotted numbers */
    if (!*sign && z - q <= 3 && z + 1 < n && t[z] == '.' && digit(t[z + 1])) {
        size_t r = z, parts = 1;
        while (r + 1 < n && t[r] == '.' && digit(t[r + 1])) {
            for (++r; r < n && digit(t[r]); ++r)
                ;
            ++parts;
        }
        if (parts == 3 && !(r < n && (digit(t[r]) || alpha(t[r]))) &&
            !(z - q <= 2 && r >= 5 && digit(t[r - 4]) && t[r - 5] == '.' && atoi(t + q) >= 1 && atoi(t + q) <= 31)) {
            snprintf(v, sizeof(v), "%.*s", (int)(r - q), t + q);
            return finish(in, s, r, "dotted", v, "");
        }
    }
    /* a hotline after โทร, สายด่วน, กด: 1669 หนึ่งหกหกเก้า, 191 */
    if (!*sign && z - q >= 3 && z - q <= 5 && t[q] == '1' && cue_within(e, t, q, 90, "phone") &&
        !(z < n && (digit(t[z]) || t[z] == '.' || t[z] == ',' || t[z] == '-' || t[z] == '/')) &&
        !unit_after(e, t, n, z, key, sizeof(key))) {
        snprintf(v, sizeof(v), "%.*s", (int)(z - q), t + q);
        return finish(in, s, z, "digits", v, "");
    }
    if (!*sign && z - q >= 3 && z - q <= 5 && t[q] != '0' && cue_within(e, t, q, 30, "phone") &&
        !(z < n && (digit(t[z]) || t[z] == '.' || t[z] == ',' || t[z] == '-' || t[z] == '/'))) {
        snprintf(v, sizeof(v), "%.*s", (int)(z - q), t + q);
        return finish(in, s, z, "digits", v, "");
    }
    /* dates: 17/12/2569, 13-10-2569, 13.10.2026, 2569-10-13, 15/11 */
    if (!*sign && z - q <= 2 && z + 2 < n && (t[z] == '/' || t[z] == '-' || t[z] == '.') && digit(t[z + 1])) {
        char sep = t[z];
        size_t m0 = z + 1, m1 = m0;
        int d = atoi(t + q), m;
        while (m1 < n && digit(t[m1]))
            ++m1;
        m = atoi(t + m0);
        if (m1 - m0 <= 2 && d >= 1 && d <= 31 && m >= 1 && m <= 12) {
            if (m1 + 2 < n && t[m1] == sep && digit(t[m1 + 1])) {
                size_t y0 = m1 + 1, y1 = y0;
                while (y1 < n && digit(t[y1]))
                    ++y1;
                if ((y1 - y0 == 4 || (y1 - y0 == 2 && sep == '/')) && !(y1 < n && (digit(t[y1]) || alpha(t[y1])))) {
                    snprintf(v, sizeof(v), "%d|%d||%.*s", d, m, (int)(y1 - y0), t + y0);
                    return finish(in, s, y1, "date", v, "");
                }
            } else if (sep == '/' && !(m1 < n && (digit(t[m1]) || t[m1] == '/' || alpha(t[m1]))) &&
                       (d > 12 || cue_before(e, t, q, "date") || has(t, n, spaces(t, n, m1), "นี้"))) {
                /* 15/11 สิบห้าพฤศจิกายน; 1/4 stays a fraction */
                snprintf(v, sizeof(v), "%d|%d||", d, m);
                return finish(in, s, m1, "date", v, "");
            }
        }
    }
    if (!*sign && z - q == 4 && z + 5 < n && t[z] == '-' && digit(t[z + 1]) && digit(t[z + 2]) && t[z + 3] == '-' &&
        digit(t[z + 4]) && digit(t[z + 5]) && !(z + 6 < n && digit(t[z + 6]))) {
        int m = atoi(t + z + 1), d = atoi(t + z + 4);
        if (m >= 1 && m <= 12 && d >= 1 && d <= 31) {
            snprintf(v, sizeof(v), "%d|%d||%.4s", d, m, t + q);
            return finish(in, s, z + 6, "date", v, "");
        }
    }
    /* 18 ต.ค. 2569, 13-15 ต.ค., ๑๓ ตุลาคม */
    if (!*sign && z - q <= 2 && atoi(t + q) >= 1 && atoi(t + q) <= 31) {
        size_t me = month_after(e, t, n, z, &month), d2e = 0;
        int d2 = 0;
        if (!me && dash_len(t, n, z)) {
            size_t d2b = z + dash_len(t, n, z);
            for (d2e = d2b; d2e < n && digit(t[d2e]); ++d2e)
                ;
            d2 = atoi(t + d2b);
            if (d2e - d2b <= 2 && d2 > atoi(t + q) && d2 <= 31)
                me = month_after(e, t, n, d2e, &month);
        }
        if (me) {
            size_t ye = year_after(t, n, me, era, sizeof(era), y, sizeof(y));
            if (d2)
                snprintf(v, sizeof(v), "%.*s|%d|%d|%s|%s", (int)(z - q), t + q, d2, month, era, y);
            else
                snprintf(v, sizeof(v), "%.*s|%d|%s|%s", (int)(z - q), t + q, month, era, y);
            return finish(in, s, ye, d2 ? "daterange" : "date", v, "");
        }
    }
    /* 2:05:37 สองชั่วโมงห้านาทีสามสิบเจ็ดวินาที: a duration (a clock with seconds takes น.) */
    if (!*sign && z - q <= 2 && z + 5 < n && t[z] == ':' && digit(t[z + 1]) && digit(t[z + 2]) && t[z + 3] == ':' &&
        digit(t[z + 4]) && digit(t[z + 5]) && !(z + 6 < n && digit(t[z + 6])) &&
        !has(t, n, spaces(t, n, z + 6), "น.")) {
        snprintf(v, sizeof(v), "%d|%d|%d", atoi(t + q), atoi(t + z + 1), atoi(t + z + 4));
        return finish(in, s, z + 6, "duration", v, "");
    }
    /* times: 14.00 น., 06:00, 20:00 น., 10.00-22.00 น. */
    if (!*sign && z - q <= 2 && z + 2 < n && (t[z] == '.' || t[z] == ':') && digit(t[z + 1]) && digit(t[z + 2]) &&
        !(z + 3 < n && digit(t[z + 3])) && atoi(t + q) <= 24 && atoi(t + z + 1) <= 59) {
        size_t te = z + 3, after = spaces(t, n, te);
        int nat = has(t, n, after, "น."), colon = t[z] == ':';
        size_t r = 0, re = 0;
        /* a time range: 10.00-22.00 น. */
        if (dash_len(t, n, te)) {
            r = te + dash_len(t, n, te);
            if (r + 3 < n && digit(t[r])) {
                size_t r2 = r;
                while (r2 < n && digit(t[r2]))
                    ++r2;
                if (r2 - r <= 2 && r2 + 2 < n && t[r2] == t[z] && digit(t[r2 + 1]) && digit(t[r2 + 2]) &&
                    !(r2 + 3 < n && digit(t[r2 + 3]))) {
                    re = r2 + 3;
                    after = spaces(t, n, re);
                    nat = has(t, n, after, "น.");
                }
            }
        }
        /* 00.30, 08.15: a two-digit hour is a clock */
        if (nat || colon || cue_before(e, t, q, "time") || (z - q == 2 && t[q] == '0')) {
            size_t end = re ? re : te;
            if (nat)
                end = after + strlen("น.");
            if (colon && !nat && cue_within(e, t, q, 120, "ratio"))
                goto not_time;
            if (re)
                snprintf(v, sizeof(v), "%.*s|%.2s|%.*s|%.2s", (int)(z - q), t + q, t + z + 1,
                         (int)(strcspn(t + r, ".:")), t + r, t + re - 2);
            else
                snprintf(v, sizeof(v), "%.*s|%.2s", (int)(z - q), t + q, t + z + 1);
            return finish(in, s, end, re ? "timerange" : "time", v, "");
        }
    }
not_time:
    /* 08/2027 แปดทับสองพันยี่สิบเจ็ด, 11/11 สิบเอ็ดทับสิบเอ็ด */
    if (!*sign && z - q <= 2 && z + 1 < n && t[z] == '/' && digit(t[z + 1])) {
        size_t r = z + 1;
        while (r < n && digit(t[r]))
            ++r;
        if (!(r < n && (digit(t[r]) || t[r] == '/' || t[r] == '.' || alpha(t[r]))) &&
            ((r - z - 1 == 4 && atoi(t + q) >= 1 && atoi(t + q) <= 12) || (r - z - 1 == z - q && !memcmp(t + q, t + z + 1, z - q)))) {
            snprintf(v, sizeof(v), "%d|%.*s", atoi(t + q), (int)(r - z - 1), t + z + 1);
            return finish(in, s, r, "slashed", v, "");
        }
    }
    /* 1 1/2 เม็ด หนึ่งเม็ดครึ่ง: a whole number and a half before a classifier */
    if (!*sign && z - q <= 2 && z + 5 < n && t[z] == ' ' && has(t, n, z + 1, "1/2") && t[z + 4] == ' ' &&
        cue_at(e, t, n, z + 5, "classifier")) {
        size_t w = z + 5 + cue_at(e, t, n, z + 5, "classifier");
        snprintf(v, sizeof(v), "%.*s|%.*s", (int)(z - q), t + q, (int)(w - z - 5), t + z + 5);
        return finish(in, s, w, "halfafter", v, "");
    }
    /* codes: a leading zero, a long run, or digits after รหัส/เลขที่ read digit by digit */
    if (!*sign && ((t[q] == '0' && z - q >= 2 && !(z < n && t[z] == '.')) ||
                   (z - q >= 8 && !(z < n && t[z] == ',')) ||
                   (z - q >= 4 && cue_within(e, t, q, 40, "code") && !(z < n && (t[z] == ',' || t[z] == '.'))) ||
                   /* รหัสไปรษณีย์เขตปทุมวันคือ 10330 */
                   (z - q == 5 && cue_within(e, t, q, 120, "postal") && !(z < n && (t[z] == ',' || t[z] == '.'))) ||
                   /* 67-0412: a group after a hyphen starting with 0 is a code, never a range */
                   (z + 2 < n && t[z] == '-' && t[z + 1] == '0' && digit(t[z + 2])) ||
                   /* LOT 2409B, 7H2: digits with letters after them */
                   (z - q >= 3 && z < n && t[z] >= 'A' && t[z] <= 'Z' && !unit_after(e, t, n, z, key, sizeof(key))) ||
                   (z < n && t[z] >= 'A' && t[z] <= 'Z' && z + 1 < n && digit(t[z + 1])))) {
        size_t r = z;
        /* 67-0412, 1234-5678, 0032-77A: hyphenated groups stay one code */
        while (r < n && (digit(t[r]) || (t[r] >= 'A' && t[r] <= 'Z') ||
                         (t[r] == '-' && r + 1 < n && (digit(t[r + 1]) || (t[r + 1] >= 'A' && t[r + 1] <= 'Z')))))
            ++r;
        if (!(r < n && (alpha(t[r]) || t[r] == '.'))) {
            snprintf(v, sizeof(v), "%.*s", (int)(r - q), t + q);
            return finish(in, s, r, "code", v, "");
        }
    }
    u = scan_number(t, n, q, &grouped, &decimal);
    /* 1023/2568 หนึ่งพันยี่สิบสามทับสองพันห้าร้อยหกสิบแปด: a number of a year */
    if (!*sign && !grouped && !decimal && u + 5 <= n && t[u] == '/' && (has(t, n, u + 1, "25") || has(t, n, u + 1, "20")) &&
        digit(t[u + 3]) && digit(t[u + 4]) && !(u + 5 < n && (digit(t[u + 5]) || t[u + 5] == '/'))) {
        snprintf(v, sizeof(v), "%.*s|%.4s", (int)(u - q), t + q, t + u + 1);
        return finish(in, s, u + 5, "slashed", v, "");
    }
    /* 56x36x23 ซม., 350 × 2 = 700 บาท: dimensions and a product */
    if (!*sign && !decimal) {
        size_t r = u, terms = 1;
        char expr[TTF_IR_VALUE_SIZE];
        size_t used = (size_t)snprintf(expr, sizeof(expr), "%.*s", (int)(u - q), t + q);
        for (;;) {
            size_t a = spaces(t, n, r), w = 0, b0, b1;
            int g, dd;
            if (a < n && (t[a] == 'x' || t[a] == 'X'))
                w = 1;
            else if (has(t, n, a, "×"))
                w = 2;
            if (!w)
                break;
            b0 = spaces(t, n, a + w);
            if (!(b0 < n && digit(t[b0])))
                break;
            b1 = scan_number(t, n, b0, &g, &dd);
            if (used + (b1 - b0) + 3 >= sizeof(expr))
                break;
            used += (size_t)snprintf(expr + used, sizeof(expr) - used, "x%.*s", (int)(b1 - b0), t + b0);
            r = b1;
            ++terms;
        }
        if (terms >= 2) {
            size_t a = spaces(t, n, r), ue;
            char ukey[40] = "";
            if (a < n && t[a] == '=' && spaces(t, n, a + 1) < n && digit(t[spaces(t, n, a + 1)])) {
                size_t b0 = spaces(t, n, a + 1), b1;
                int g, dd;
                b1 = scan_number(t, n, b0, &g, &dd);
                used += (size_t)snprintf(expr + used, sizeof(expr) - used, "=%.*s", (int)(b1 - b0), t + b0);
                r = b1;
            }
            ue = unit_after(e, t, n, r, ukey, sizeof(ukey));
            if (strlen(expr) + strlen(ukey) + 2 > sizeof(v))
                return 0;
            snprintf(v, sizeof(v), "%.*s|%s", (int)(sizeof(v) - 42), expr, ue ? ukey : "");
            return finish(in, s, ue ? ue : r, "product", v, "");
        }
    }
    /* ranges and scores: 3-5 วัน สามถึงห้าวัน, 4,500-6,000 บาท, 2-1 สองต่อหนึ่ง */
    if (!*sign && dash_len(t, n, u)) {
        size_t b0 = u + dash_len(t, n, u);
        int g2, d2;
        size_t b1 = scan_number(t, n, b0, &g2, &d2);
        if (!(b1 < n && (digit(t[b1]) || t[b1] == '-' || alpha(t[b1]))) && t[b0] != '0') {
            double a = atof(t + q), bb = atof(t + b0);
            char ua[40] = "";
            size_t ue = b1 < n && t[b1] == '%' ? (snprintf(ua, sizeof(ua), "%%"), b1 + 1)
                                               : unit_after(e, t, n, b1, ua, sizeof(ua));
            int versus = cue_within(e, t, q, 40, "score") || (a >= bb && b1 - b0 <= 2 && u - q <= 2);
            snprintf(v, sizeof(v), "%.*s|%.*s|%s", (int)(u - q), t + q, (int)(b1 - b0), t + b0, ue ? ua : "");
            return finish(in, s, ue ? ue : b1, versus ? "versus" : "range", v, "");
        }
    }
    /* fractions and ratios: 1/4 หนึ่งส่วนสี่, 1:10 หนึ่งต่อสิบ, 3/4 ถ้วย */
    if (!*sign && !grouped && !decimal && u + 1 < n && (t[u] == '/' || t[u] == ':') && digit(t[u + 1])) {
        size_t b1 = u + 1;
        while (b1 < n && digit(t[b1]))
            ++b1;
        if (!(b1 < n && (digit(t[b1]) || t[b1] == '/' || t[b1] == '.' || t[b1] == ':'))) {
            char ua[40] = "";
            size_t ue = t[u] == '/' ? unit_after(e, t, n, b1, ua, sizeof(ua)) : 0;
            int bp = ue && (!strcmp(ua, "มม.ปรอท") || !strcmp(ua, "mmHg"));
            /* บ้านเลขที่ 47/12 สี่สิบเจ็ดทับสิบสอง, 150/90 มม.ปรอท a blood pressure */
            if (t[u] == '/' && (bp || cue_before(e, t, q, "address"))) {
                snprintf(v, sizeof(v), "%.*s|%.*s|%s", (int)(u - q), t + q, (int)(b1 - u - 1), t + u + 1, bp ? ua : "");
                return finish(in, s, bp ? ue : b1, "slashnum", v, "");
            }
            snprintf(v, sizeof(v), "%.*s|%.*s", (int)(u - q), t + q, (int)(b1 - u - 1), t + u + 1);
            return finish(in, s, b1, t[u] == ':' ? "versus" : "over", v, "");
        }
    }
    /* a number, then %, °C or a unit: 15% สิบห้าเปอร์เซ็นต์, 5.2 กม., 1TB, 9,000 บ. */
    {
        size_t end = u, numend = u;
        key[0] = 0;
        /* 65.- หกสิบห้าบาท: a price */
        if (u + 1 < n && t[u] == '.' && t[u + 1] == '-' && !(u + 2 < n && digit(t[u + 2]))) {
            snprintf(v, sizeof(v), "%s%.*s|บาท", sign, (int)(u - q), t + q);
            return finish(in, s, u + 2, "num", v, "");
        }
        if (u < n && t[u] == '%')
            snprintf(key, sizeof(key), "%%"), end = u + 1;
        else if ((z = unit_after(e, t, n, u, key, sizeof(key))) != 0) {
            end = z;
            /* 12,500.00 บาท: zero satang is not read */
            if (decimal && res(e, "th_money", key)) {
                size_t dot = q;
                while (dot < u && t[dot] != '.')
                    ++dot;
                if (dot < u && strspn(t + dot + 1, "0") >= u - dot - 1)
                    numend = dot;
            }
            /* 60 บาท/วัน, 30 kg/m², 78 มคก./ลบ.ม.: a unit per another unit or a word */
            if (end + 1 < n && t[end] == '/' && (thai_at(t, n, end + 1) || alpha(t[end + 1]))) {
                char k2[40] = "";
                size_t e2 = unit_after(e, t, n, end + 1, k2, sizeof(k2));
                snprintf(v, sizeof(v), "%s%.*s|%s|%s", sign, (int)(numend - q), t + q, key, k2);
                return finish(in, s, e2 ? e2 : end + 1, "numper", v, "");
            }
        }
        /* 5G ห้าจี, 3G: letters right after the number that are no unit */
        else if (u < n && alpha(t[u]) && t[u] >= 'A' && t[u] <= 'Z') {
            size_t le = u;
            while (le < n && t[le] >= 'A' && t[le] <= 'Z')
                ++le;
            if (le - u <= 3 && !(le < n && (alpha(t[le]) || digit(t[le])))) {
                snprintf(v, sizeof(v), "%s%.*s|%.*s", sign, (int)(u - q), t + q, (int)(le - u), t + u);
                return finish(in, s, le, "numletters", v, "");
            }
        }
        if (u < n && alpha(t[u]) && !key[0])
            return 0;
        snprintf(v, sizeof(v), "%s%.*s|%s", sign, (int)(numend - q), t + q, key);
        return finish(in, s, end, "num", v, "");
    }
}

/* ---- verbalization ---- */

static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *b) {
    const char *mode = s->unit, *v = s->value;
    char f0[TTF_IR_VALUE_SIZE], f1[TTF_IR_VALUE_SIZE], f2[TTF_IR_VALUE_SIZE], f3[32], f4[32];
    if (strncmp(mode, "@th-", 4)) {
        /* shared objects in a Thai sentence */
        if (s->type == TTF_SEM_PHONE || s->type == TTF_SEM_DIGIT_SEQUENCE ||
            (s->type == TTF_SEM_TEXT && (!strcmp(s->rule, "syntax.invalid-phone") || !strcmp(s->rule, "syntax.invalid-range")))) {
            /* +66 2 123 4567 บวกหกหก สอง หนึ่งสองสาม สี่ห้าหกเจ็ด; 0032-77A a code */
            size_t i;
            for (i = 0; i < length; ++i)
                if (i + 3 <= length && (unsigned char)surface[i] == 0xe0 && (unsigned char)surface[i + 1] == 0xb9 &&
                    (unsigned char)surface[i + 2] >= 0x90 && (unsigned char)surface[i + 2] <= 0x99) {
                    /* ๐-๓๘๔๕-๖๗๘๙: the surface keeps its Thai digits */
                    TRY(digit_word(e, b, (char)('0' + (unsigned char)surface[i + 2] - 0x90)));
                    i += 2;
                } else if (digit(surface[i]))
                    TRY(digit_word(e, b, surface[i]));
                else if (alpha(surface[i]))
                    TRY(letter(e, b, surface[i]));
                else if (surface[i] == '+')
                    TRY(word(e, b, "plus"));
                else if (b->length && b->data[b->length - 1] != ' ')
                    TRY(ttf_buffer_puts(b, " "));
            return TTF_OK;
        }
        if ((s->type == TTF_SEM_CARDINAL || s->type == TTF_SEM_NUMBER) && *v &&
            strspn(v, "0123456789,.-") == strlen(v))
            return number(e, b, v);
        /* 12 + 8 = 20 found by the shared expression rule */
        if (s->type == TTF_SEM_SYMBOL && !strcmp(mode, "@expression") && *v)
            return math_reading(e, b, v);
        if (s->type == TTF_SEM_IP_ADDRESS) {
            snprintf(f0, sizeof(f0), "%.*s", (int)(length < sizeof(f0) - 1 ? length : sizeof(f0) - 1), surface);
            mode = "@th-ip";
            v = f0;
        } else if (s->type == TTF_SEM_CUSTOM && !strcmp(mode, "@alphabet")) {
            /* an address label or an initialism: letters by name, digits one by one; a word of
               three letters or more with a small letter stays as written (iPhone, gmail is a word) */
            const char *x = !strcmp(s->secondary, "control-surface") ? surface : v;
            size_t xn = !strcmp(s->secondary, "control-surface") ? length : strlen(v), i = 0;
            /* run2026 รัน + สองศูนย์สองหก: a run of letters is a Thai-spelled word, a word kept as
               written (three letters or more with a small letter) or spelled; digits one by one */
            while (i < xn) {
                size_t z = i, small = 0;
                if (digit(x[i])) {
                    TRY(digit_word(e, b, x[i]));
                    ++i;
                    continue;
                }
                if (!alpha(x[i])) {
                    ++i;
                    continue;
                }
                while (z < xn && alpha(x[z]))
                    small += x[z++] >= 'a' && x[z - 1] <= 'z';
                {
                    char low[40];
                    const char *th = NULL;
                    size_t k;
                    if (z - i < sizeof(low)) {
                        for (k = 0; k < z - i; ++k)
                            low[k] = (char)(x[i + k] >= 'A' && x[i + k] <= 'Z' ? x[i + k] + 32 : x[i + k]);
                        low[z - i] = 0;
                        th = res(e, "electronic_word", low);
                    }
                    if (th)
                        TRY(ttf_buffer_puts(b, th));
                    else if (small && z - i >= (!strcmp(s->rule, "electronic.label") ? 5u : 3u) &&
                             vowel_in(x + i, z - i))
                        TRY(ttf_buffer_append(b, x + i, z - i)); /* xlsx, pdf have no vowel: spelled */
                    else
                        for (k = i; k < z; ++k)
                            TRY(letter(e, b, x[k]));
                }
                i = z;
            }
            return TTF_OK;
        } else
            return ttf_buffer_append(b, surface, length);
    }
    mode += 4;
    if (!strcmp(mode, "lit"))
        return ttf_buffer_puts(b, v);
    if (!strcmp(mode, "digits"))
        return digits_each(e, b, v, strlen(v));
    if (!strcmp(mode, "code"))
        return code(e, b, v, strlen(v));
    if (!strcmp(mode, "phone")) {
        /* digit groups with a space between them; +66 บวกหกหก */
        size_t i;
        if (*v == '+')
            TRY(word(e, b, "plus"));
        for (i = 0; v[i]; ++i)
            if (digit(v[i]))
                TRY(digit_word(e, b, v[i]));
            else if (v[i] != '+' && b->length && b->data[b->length - 1] != ' ')
                TRY(ttf_buffer_puts(b, " "));
        return TTF_OK;
    }
    if (!strcmp(mode, "ip")) {
        size_t i;
        for (i = 0; v[i]; ++i)
            if (digit(v[i]))
                TRY(digit_word(e, b, v[i]));
            else {
                TRY(ttf_buffer_puts(b, " "));
                TRY(word(e, b, "point"));
                TRY(ttf_buffer_puts(b, " "));
            }
        return TTF_OK;
    }
    if (!strcmp(mode, "version")) {
        size_t i = 0, n = strlen(v);
        const char *bar = strchr(v, '|');
        if (bar)
            n = (size_t)(bar - v);
        TRY(word(e, b, "version"));
        while (i < n) {
            size_t z = i;
            while (z < n && digit(v[z]))
                ++z;
            TRY(integer(e, b, v + i, z - i));
            if (z < n)
                TRY(word(e, b, "point"));
            i = z + 1;
        }
        if (bar) {
            TRY(word(e, b, "dash"));
            TRY(ttf_buffer_puts(b, bar + 1));
        }
        return TTF_OK;
    }
    if (!strcmp(mode, "wordnum")) {
        /* ไอโฟน|13 ไอโฟนสิบสาม; โบอิ้ง|737 โบอิ้งเจ็ดสามเจ็ด: three digits or more one by one */
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        TRY(ttf_buffer_puts(b, f0));
        return strlen(f1) >= 3 ? digits_each(e, b, f1, strlen(f1)) : integer(e, b, f1, strlen(f1));
    }
    if (!strcmp(mode, "address"))
        return ttf_electronic_render(e, ttf_find_pack(e, "en-US"), ttf_find_pack(e, TH), v, strlen(v), b);
    if (!strcmp(mode, "doc")) {
        /* นร 0505/ว 212 นอรอ ศูนย์ห้าศูนย์ห้า ทับ วอ สองหนึ่งสอง; ฮย 2047 */
        size_t i = 0, n2 = strlen(v);
        while (i < n2) {
            const char *snd = consonant_sound(v, n2, i);
            if (snd) {
                TRY(ttf_buffer_puts(b, snd));
                i += 3;
            } else if (digit(v[i])) {
                TRY(digit_word(e, b, v[i]));
                ++i;
            } else if (v[i] == '.' && i + 1 < n2 && digit(v[i + 1])) {
                TRY(word(e, b, "point"));
                ++i;
            } else {
                if (v[i] == '/') {
                    TRY(ttf_buffer_puts(b, " "));
                    TRY(word(e, b, "slash"));
                }
                if (b->length && b->data[b->length - 1] != ' ')
                    TRY(ttf_buffer_puts(b, " "));
                ++i;
            }
        }
        return TTF_OK;
    }
    if (!strcmp(mode, "math"))
        return math_reading(e, b, v);
    if (!strcmp(mode, "duration")) {
        /* 2|5|37 สองชั่วโมงห้านาทีสามสิบเจ็ดวินาที */
        int part[3] = {0, 0, 0}, k;
        static const char *const unit[] = {"hour", "minute", "second"};
        sscanf(v, "%d|%d|%d", &part[0], &part[1], &part[2]);
        for (k = 0; k < 3; ++k)
            if (part[k] || (k == 2 && !part[0] && !part[1])) {
                snprintf(f0, sizeof(f0), "%d", part[k]);
                TRY(number(e, b, f0));
                TRY(word(e, b, unit[k]));
            }
        return TTF_OK;
    }
    if (!strcmp(mode, "halfafter")) {
        /* 1|เม็ด หนึ่งเม็ดครึ่ง */
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        TRY(number(e, b, f0));
        TRY(ttf_buffer_puts(b, f1));
        return word(e, b, "half");
    }
    if (!strcmp(mode, "wordcode")) {
        /* coffee2024 coffee + สองศูนย์สองสี่ */
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        TRY(ttf_buffer_puts(b, f0));
        return code(e, b, f1, strlen(f1));
    }
    if (!strcmp(mode, "numletters")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        TRY(number(e, b, f0));
        return code(e, b, f1, strlen(f1));
    }
    if (!strcmp(mode, "abbrnum")) {
        /* พองอดอ|91 */
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        TRY(ttf_buffer_puts(b, f0));
        return integer(e, b, f1, strlen(f1));
    }
    if (!strcmp(mode, "lettersnum")) {
        /* E|20 อียี่สิบ */
        size_t i;
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        for (i = 0; f0[i]; ++i)
            TRY(letter(e, b, f0[i]));
        return number(e, b, f1);  /* +7 บวกเจ็ด */
    }
    if (!strcmp(mode, "dotted")) {
        /* 17.4.1 สิบเจ็ดจุดสี่จุดหนึ่ง */
        size_t i = 0, n = strlen(v);
        while (i < n) {
            size_t z = i;
            while (z < n && digit(v[z]))
                ++z;
            TRY(integer(e, b, v + i, z - i));
            if (z < n)
                TRY(word(e, b, "point"));
            i = z + 1;
        }
        return TTF_OK;
    }
    if (!strcmp(mode, "slashed")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        TRY(integer(e, b, f0, strlen(f0)));
        TRY(word(e, b, "slash"));
        return integer(e, b, f1, strlen(f1));
    }
    if (!strcmp(mode, "product")) {
        /* 56x36x23|ซม. ห้าสิบหกคูณสามสิบหก…; 350x2=700|บาท */
        char ex[TTF_IR_VALUE_SIZE];
        size_t i = 0, n2;
        field(v, 0, ex, sizeof(ex));
        field(v, 1, f1, sizeof(f1));
        n2 = strlen(ex);
        while (i < n2) {
            size_t z = i;
            char num[48];
            while (z < n2 && (digit(ex[z]) || ex[z] == ',' || ex[z] == '.'))
                ++z;
            snprintf(num, sizeof(num), "%.*s", (int)(z - i), ex + i);
            TRY(number(e, b, num));
            if (z < n2)
                TRY(word(e, b, ex[z] == '=' ? "equals" : "times"));
            i = z + 1;
        }
        return unit_reading(e, b, f1);
    }
    if (!strcmp(mode, "numper")) {
        /* 60|บาท|  หกสิบบาทต่อ(วัน); 30|kg|m² สามสิบกิโลกรัมต่อตารางเมตร */
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        field(v, 2, f2, sizeof(f2));
        TRY(number(e, b, f0));
        TRY(unit_reading(e, b, f1));
        TRY(word(e, b, "versus"));
        return unit_reading(e, b, f2);
    }
    if (!strcmp(mode, "class")) {
        /* ม.3/5 มอสามทับห้า */
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        TRY(word(e, b, "class"));
        TRY(integer(e, b, f0, strlen(f0)));
        if (*f1) {
            TRY(word(e, b, "slash"));
            TRY(integer(e, b, f1, strlen(f1)));
        }
        return TTF_OK;
    }
    if (!strcmp(mode, "num")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        /* ฿250 สองร้อยห้าสิบบาท, $20 ยี่สิบดอลลาร์: a currency sign reads after the amount */
        TRY(number(e, b, f0));
        if (!strcmp(f1, "%"))
            return word(e, b, "percent");
        return unit_reading(e, b, f1);
    }
    if (!strcmp(mode, "range") || !strcmp(mode, "versus") || !strcmp(mode, "over") || !strcmp(mode, "slashnum")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        field(v, 2, f2, sizeof(f2));
        TRY(number(e, b, f0));
        TRY(word(e, b, !strcmp(mode, "range")     ? "to"
                       : !strcmp(mode, "versus") ? "versus"
                       : !strcmp(mode, "over")   ? "over"
                                                 : "slash"));
        TRY(number(e, b, f1));
        return unit_reading(e, b, f2);
    }
    if (!strcmp(mode, "date"))
        return date(e, b, v);
    if (!strcmp(mode, "daterange")) {
        /* 13|15|10|era|year สิบสามถึงสิบห้าตุลาคม */
        char rest[4 * TTF_IR_VALUE_SIZE];
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        field(v, 2, f2, sizeof(f2));
        field(v, 3, f3, sizeof(f3));
        field(v, 4, f4, sizeof(f4));
        TRY(integer(e, b, f0, strlen(f0)));
        TRY(word(e, b, "to"));
        snprintf(rest, sizeof(rest), "%s|%s|%s|%s", f1, f2, f3, f4);
        return date(e, b, rest);
    }
    if (!strcmp(mode, "time")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        return clock(e, b, f0, f1);
    }
    if (!strcmp(mode, "timerange")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        field(v, 2, f2, sizeof(f2));
        field(v, 3, f3, sizeof(f3));
        TRY(clock(e, b, f0, f1));
        TRY(word(e, b, "to"));
        return clock(e, b, f2, f3);
    }
    return TTF_EINVAL;
}

/* Thai letters are no script of the core's (OTHER); this pack takes a Thai sentence
   (thai_sentence) or a declared request. Latin letters score 1 so that an English word
   in a Thai sentence stays as written (the English pack scores higher). */
static int script_score(ttf_script_t script) {
    return script == TTF_SCRIPT_LATIN ? 1 : 0;
}

/* A th-TH request routes everything but Hangul, kana and hanzi to this pack, except a
   sentence decided for another language (Please send 3 files). */
static int context_score(const ttf_pack_input_t *in) {
    const ttf_engine_t *e = in->engine;
    const ttf_language_pack_t *sentence;
    if (!e || !e->default_language || strcmp(e->default_language, TH) || in->index >= in->token_count ||
        in->tokens[in->index].script == TTF_SCRIPT_HANGUL || in->tokens[in->index].script == TTF_SCRIPT_KANA ||
        in->tokens[in->index].script == TTF_SCRIPT_HAN)
        return 0;
    /* a sentence with Thai letters is Thai, however many English words it has (iPhone, Call
       center): Thai writes no spaces, so its words never outvote them */
    {
        size_t k;
        for (k = 0; k + 3 <= in->length; ++k)
            if (thai_at(in->text, in->length, k) && !has(in->text, in->length, k, "฿"))
                return 1000;
    }
    sentence = ttf_sentence_language(in);
    if (sentence && strcmp(sentence->language_tag, TH))
        return 0;
    return 1000;
}

const ttf_language_pack_t ttf_pack_th_TH = {sizeof(ttf_language_pack_t),
                                            TTF_ABI_VERSION,
                                            TH,
                                            NULL,
                                            script_score,
                                            context_score,
                                            classify,
                                            verbalize,
                                            NULL,
                                            NULL,
                                            NULL};
