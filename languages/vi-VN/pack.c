/* Vietnamese pack, phase 1 (2026-10-13, project owner: Vietnamese next; the
 * northern standard reading first). Numbers read as Vietnamese words (mốt,
 * lăm, linh, nghìn, triệu, tỷ), the comma is the decimal mark and the dot
 * groups thousands; dates (ngày … tháng … năm), times (giờ, phút), money,
 * percentages, units, ranges, ordinals after thứ and phone numbers digit by
 * digit. Latin words stay as written for the speech model. The pack works only
 * where vi-VN is the context: a request declared vi-VN, or a Latin sentence
 * written with Vietnamese letters (context latin_sentence) in another request.
 *
 * classify decides the extent and the reading mode and keeps the source in
 * value; verbalize reads value in that mode. */
#include "tts_text_frontend/ttf_language_pack.h"
#include "../../src/core/internal.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VI "vi-VN"
#define TRY(x)                                                                                     \
    do {                                                                                           \
        ttf_status_t try_status = (x);                                                             \
        if (try_status != TTF_OK)                                                                  \
            return try_status;                                                                     \
    } while (0)

static const char *res(const ttf_engine_t *e, const char *kind, const char *key) {
    return ttf_resource_get(e, kind, VI, key, NULL);
}
static int digit(char c) {
    return c >= '0' && c <= '9';
}
static int alpha(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

/* ---- readings ---- */

/* A word, with one space before it unless it starts the output. */
static ttf_status_t put(ttf_buffer_t *b, const char *s) {
    if (b->length && b->data[b->length - 1] != ' ')
        TRY(ttf_buffer_puts(b, " "));
    return ttf_buffer_puts(b, s);
}
static ttf_status_t word(const ttf_engine_t *e, ttf_buffer_t *b, const char *key) {
    const char *v = res(e, "number", key);
    return v ? put(b, v) : TTF_ERESOURCE;
}
static ttf_status_t digit_word(const ttf_engine_t *e, ttf_buffer_t *b, char c) {
    char key[3] = {'d', c, 0};
    return word(e, b, key);
}
/* Digits one by one; a run of letters stays one written word (gmail, VN). */
static ttf_status_t digits_each(const ttf_engine_t *e, ttf_buffer_t *b, const char *s, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i)
        if (digit(s[i]))
            TRY(digit_word(e, b, s[i]));
        else if (alpha(s[i])) {
            char letter[2] = {s[i], 0};
            TRY(i && alpha(s[i - 1]) ? ttf_buffer_puts(b, letter) : put(b, letter));
        }
    return TTF_OK;
}

/* One group of three digits. `full` when a higher group came before it:
   1.005 một nghìn không trăm linh năm. After mươi, 1 is mốt, 5 is lăm and 4 is
   bốn (number four_after_tens); after mười, 5 is lăm. */
static ttf_status_t group(const ttf_engine_t *e, ttf_buffer_t *b, unsigned g, int full) {
    unsigned h = g / 100, t = g / 10 % 10, u = g % 10;
    if (full || h) {
        TRY(digit_word(e, b, (char)('0' + h)));
        TRY(word(e, b, "hundred"));
    }
    if (t == 0) {
        if (u) {
            if (full || h)
                TRY(word(e, b, "zero_tens"));
            TRY(digit_word(e, b, (char)('0' + u)));
        }
        return TTF_OK;
    }
    if (t == 1)
        TRY(word(e, b, "ten"));
    else {
        TRY(digit_word(e, b, (char)('0' + t)));
        TRY(word(e, b, "tens"));
    }
    if (u == 5)
        return word(e, b, "five_after_tens");
    if (u == 1 && t > 1)
        return word(e, b, "one_after_tens");
    if (u == 4 && t > 1)
        return word(e, b, "four_after_tens");
    return u ? digit_word(e, b, (char)('0' + u)) : TTF_OK;
}

/* Up to nine digits in groups of three with nghìn and triệu. */
static ttf_status_t millions(const ttf_engine_t *e, ttf_buffer_t *b, const char *s, size_t n,
                             int started) {
    static const char *const scale[] = {NULL, "thousand", "million"};
    size_t groups = (n + 2) / 3, head = n % 3 ? n % 3 : 3, i, k;
    for (i = 0; i < groups; ++i) {
        size_t from = i ? head + (i - 1) * 3 : 0, to = head + i * 3, level = groups - 1 - i;
        unsigned g = 0;
        for (k = from; k < to; ++k)
            g = g * 10 + (unsigned)(s[k] - '0');
        if (!g)
            continue;
        TRY(group(e, b, g, started));
        if (level)
            TRY(word(e, b, scale[level]));
        started = 1;
    }
    return TTF_OK;
}

/* An integer of up to 18 digits: tỷ counts its billions as a number of its own
   (1.500.000.000.000 một nghìn năm trăm tỷ), the rest follows in full groups.
   Longer runs are read digit by digit. */
static ttf_status_t integer(const ttf_engine_t *e, ttf_buffer_t *b, const char *s, size_t n) {
    size_t k;
    while (n > 1 && *s == '0')
        ++s, --n;
    if (!n)
        return TTF_EINVAL;
    if (n > 18)
        return digits_each(e, b, s, n);
    if (n == 1 && *s == '0')
        return digit_word(e, b, '0');
    if (n <= 9)
        return millions(e, b, s, n, 0);
    TRY(integer(e, b, s, n - 9));
    TRY(word(e, b, "billion"));
    for (k = n - 9; k < n && s[k] == '0'; ++k) {
    }
    return k < n ? millions(e, b, s + n - 9, 9, 1) : TTF_OK;
}

/* A canonical number: optional -, digits, optional ,fraction. The fraction
   keeps its leading zeros (không) and is read as a number when it has at most
   three digits, else digit by digit: 3,14 ba phẩy mười bốn. */
static ttf_status_t number(const ttf_engine_t *e, ttf_buffer_t *b, const char *v) {
    const char *comma;
    size_t n;
    if (*v == '-') {
        TRY(word(e, b, "minus"));
        ++v;
    }
    comma = strchr(v, ',');
    n = comma ? (size_t)(comma - v) : strlen(v);
    TRY(integer(e, b, v, n));
    if (comma) {
        const char *f = comma + 1;
        size_t fn = strlen(f);
        TRY(word(e, b, "point"));
        while (fn > 1 && *f == '0') {
            TRY(digit_word(e, b, '0'));
            ++f, --fn;
        }
        TRY(fn <= 3 ? integer(e, b, f, fn) : digits_each(e, b, f, fn));
    }
    return TTF_OK;
}

/* tháng 4 is tháng tư; the other months are cardinals */
static ttf_status_t month(const ttf_engine_t *e, ttf_buffer_t *b, const char *m, size_t n) {
    while (n > 1 && *m == '0')
        ++m, --n;
    if (n == 1 && *m == '4')
        return word(e, b, "month4");
    return integer(e, b, m, n);
}

/* A suffix key: %, a unit (u:km), a currency (c:VND) or a scale (s:k). */
static ttf_status_t suffix(const ttf_engine_t *e, ttf_buffer_t *b, const char *key) {
    const char *v;
    if (!*key)
        return TTF_OK;
    if (!strcmp(key, "%"))
        return word(e, b, "percent");
    if (key[0] && key[1] == ':') {
        const char *plus = strchr(key + 2, '+');
        char first[24];
        snprintf(first, sizeof(first), "%.*s", (int)(plus ? (size_t)(plus - key - 2) : strlen(key + 2)),
                 key + 2);
        v = res(e, key[0] == 'u' ? "vi_unit" : key[0] == 'c' ? "vi_currency" : "vi_scale", first);
        if (!v)
            return TTF_ERESOURCE;
        TRY(put(b, v));
        if (plus) {
            TRY(word(e, b, "per"));
            v = res(e, "vi_unit", plus + 1);
            return v ? put(b, v) : TTF_ERESOURCE;
        }
        return TTF_OK;
    }
    return TTF_EINVAL;
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
    snprintf(s->rule, sizeof(s->rule), "vi.%s", mode);
    snprintf(s->unit, sizeof(s->unit), "@vi-%s", mode);
    snprintf(s->source_lang, sizeof(s->source_lang), VI);
    s->input_end = end;
    s->type = TTF_SEM_COUNTER;
    return count;
}

/* A letter of any script at p (a word goes on: 5G, 100km, mèo). */
static int letter_at(const char *t, size_t n, size_t p) {
    uint32_t cp;
    size_t w;
    if (p >= n)
        return 0;
    if ((unsigned char)t[p] < 0x80)
        return alpha(t[p]);
    return ttf_utf8_decode(t + p, n - p, &cp, &w) == 1 &&
           (ttf_script_of(cp) == TTF_SCRIPT_LATIN || ttf_script_of(cp) == TTF_SCRIPT_OTHER);
}

/* The word before p (spaces skipped) equals one of the |-separated words. */
static int previous_word(const char *t, size_t p, const char *words) {
    size_t end = p, begin;
    const char *w = words;
    while (end && t[end - 1] == ' ')
        --end;
    begin = end;
    while (begin && t[begin - 1] != ' ' && t[begin - 1] != '(' && t[begin - 1] != ',' &&
           t[begin - 1] != '"' && t[begin - 1] != '\n')
        --begin;
    if (begin == end)
        return 0;
    while (*w) {
        const char *bar = strchr(w, '|');
        size_t m = bar ? (size_t)(bar - w) : strlen(w);
        if (m == end - begin && !memcmp(t + begin, w, m))
            return 1;
        if (!bar)
            break;
        w = bar + 1;
    }
    return 0;
}
static int previous_cue(const ttf_engine_t *e, const char *t, size_t p, const char *cue) {
    const char *words = res(e, "vi_cue", cue);
    return words && previous_word(t, p, words);
}

typedef struct {
    size_t end;
    char value[TTF_IR_VALUE_SIZE]; /* canonical: -12345,67 */
    int grouped, decimal, decimal_dot, leading_zero;
} parsed_t;

/* Three digits and no fourth at p. */
static int three(const char *t, size_t n, size_t p) {
    return p + 3 <= n && digit(t[p]) && digit(t[p + 1]) && digit(t[p + 2]) &&
           !(p + 3 < n && digit(t[p + 3]));
}

/* Digits with . , or space separators: dots or spaces grouping three digits are
   thousands (15.000, 12 500); a single comma is the decimal mark (2,5); several
   commas of three are English thousands (1,000,000); with both dots and commas
   the last one is decimal; a lone dot before other than three digits is a
   decimal point too (3.5, read like 3,5). */
static int parse_number(const char *t, size_t n, size_t p, parsed_t *out) {
    size_t q = p, seps = 0, used = 0, i, first_len;
    size_t marks[8];
    int dots = 0, commas = 0;
    memset(out, 0, sizeof(*out));
    while (q < n && digit(t[q]))
        ++q;
    if (q == p)
        return 0;
    first_len = q - p;
    while (q + 1 < n && seps < 8 &&
           (((t[q] == '.' || t[q] == ',') && digit(t[q + 1])) ||
            (t[q] == ' ' && first_len <= 3 && !dots && !commas && three(t, n, q + 1)))) {
        marks[seps++] = q;
        dots += t[q] == '.';
        commas += t[q] == ',';
        ++q;
        while (q < n && digit(t[q]))
            ++q;
    }
    out->end = q;
    out->leading_zero = t[p] == '0' && first_len > 1;
    {
        int decimal_at = -1, ok = 1;
        if (dots && commas)
            decimal_at = (int)seps - 1;
        else if (commas == 1 && !dots)
            decimal_at = (int)seps - 1;
        else if (dots == 1 && !commas && seps == 1 && (q - marks[0] - 1 != 3 || first_len > 3))
            decimal_at = 0;
        /* thousands groups are three digits, the first one to three */
        for (i = 0; i < seps; ++i) {
            size_t next = i + 1 < seps ? marks[i + 1] : q, len = next - marks[i] - 1;
            if ((int)i == decimal_at)
                continue;
            if (len != 3 || first_len > 3)
                ok = 0;
        }
        if (!ok) {
            /* 13.10.2026 is a date, 1.2.3 a version: not one number */
            out->end = p + first_len;
            seps = 0;
            decimal_at = -1;
        }
        for (i = p; i < out->end && used + 2 < sizeof(out->value); ++i) {
            if (digit(t[i]))
                out->value[used++] = t[i];
            else {
                size_t k;
                for (k = 0; k < seps && marks[k] != i; ++k) {
                }
                if ((int)k == decimal_at) {
                    out->value[used++] = ',';
                    out->decimal = 1;
                    out->decimal_dot = t[i] == '.';
                } else
                    out->grouped = 1;
            }
        }
        out->value[used] = 0;
    }
    return 1;
}

/* A unit, currency or scale written after the number (one space allowed):
   the longest key that ends at a word boundary. `only` limits the kinds
   (u, c, s; NULL for all). */
static size_t suffix_at(const ttf_engine_t *e, const char *t, size_t n, size_t p, char *key,
                        size_t cap, const char *only) {
    static const char *const kinds[] = {"vi_unit", "vi_currency", "vi_scale"};
    size_t start = p < n && t[p] == ' ' ? p + 1 : p, len, best = 0, k;
    char probe[16];
    key[0] = 0;
    if (!only || strchr(only, '%')) {
        if (p < n && t[p] == '%') {
            snprintf(key, cap, "%%");
            return p + 1;
        }
        if (start < n && t[start] == '%') {
            snprintf(key, cap, "%%");
            return start + 1;
        }
    }
    for (len = 1; len < sizeof(probe) && start + len <= n; ++len) {
        memcpy(probe, t + start, len);
        probe[len] = 0;
        for (k = 0; k < 3; ++k) {
            if (only && !strchr(only, "ucs"[k]))
                continue;
            /* the scale k only attaches (50k); a spaced k is a word; a spaced tr
               before no word is triệu (35 tr) */
            if (k == 2 && start != p && strcmp(probe, "tr"))
                continue;
            if (res(e, kinds[k], probe) && !letter_at(t, n, start + len) &&
                !(start + len < n && digit(t[start + len]))) {
                best = len;
                snprintf(key, cap, "%c:%s", "ucs"[k], probe);
            }
        }
    }
    /* mg/l: a unit over a unit (u:mg+l) */
    if (best && key[0] == 'u' && start + best + 1 < n && t[start + best] == '/' &&
        t[start + best + 1] != ' ') {
        char second[24];
        size_t se = suffix_at(e, t, n, start + best + 1, second, sizeof(second), "u");
        if (se && strlen(key) + strlen(second) < cap) {
            strcat(key, "+");
            strcat(key, second + 2);
            return se;
        }
    }
    return best ? start + best : 0;
}

/* After an amount: a unit, currency or scale (15.000đ, 5 kg); a scale word and a
   currency (2,5 triệu USD); a word and its slash (320 đồng/lít). A slash after
   the amount's unit reads trên, and a unit after it is read (triệu đồng/m²).
   Returns the end; mid gets the scale word, per "/" or "/u:KEY". */
static size_t amount_tail(const ttf_engine_t *e, const char *t, size_t n, size_t q, char *key,
                          size_t kcap, char *mid, size_t mcap, char *per, size_t pcap) {
    size_t end = q, se = suffix_at(e, t, n, q, key, kcap, NULL);
    mid[0] = per[0] = 0;
    if (se)
        end = se;
    else {
        size_t m = q < n && t[q] == ' ' ? q + 1 : q, k = m;
        while (k < n && t[k] != ' ' && t[k] != '/' && k - m < mcap - 1)
            ++k;
        if (k > m && (k < n || (k == n && k - m < mcap))) {
            snprintf(mid, mcap, "%.*s", (int)(k - m), t + m);
            if (res(e, "vi_scale_word", mid) && (se = suffix_at(e, t, n, k, key, kcap, "cu")))
                end = se;
            /* 85,5 triệu: a scale word belongs to the amount, so a unit cut does
               not fall between them (… triệu | đồng/lượng) */
            else if (res(e, "vi_scale_word", mid) && (k == n || t[k] == ' '))
                end = k;
            else if (t[k] == '/' && letter_at(t, n, k + 1) && letter_at(t, n, m))
                end = k;
            else
                mid[0] = 0;
        }
    }
    if ((key[0] == '%' || key[0] == 'c' || mid[0]) && end + 1 < n && t[end] == '/' &&
        letter_at(t, n, end + 1)) {
        char unit[24];
        size_t ue = suffix_at(e, t, n, end + 1, unit, sizeof(unit), "u");
        ++end;
        if (ue && t[end] != ' ') {
            snprintf(per, pcap, "/%.22s", unit);
            end = ue;
        } else
            snprintf(per, pcap, "/");
    }
    return end;
}

/* Lead and trail flags: a reading next to a letter gets a space there (5G năm G). */
static void flags(const char *t, size_t n, size_t p, size_t end, char *out, size_t cap) {
    snprintf(out, cap, "%s%s", p && letter_at(t, n, p - 1) ? "<" : "",
             letter_at(t, n, end) ? ">" : "");
}

/* A clock at p: 9:30, 14:00:05, 9h, 9h30, 21g30. Writes H:MM:SS (parts may be empty). */
static size_t clock_at(const char *t, size_t n, size_t p, char *v, size_t cap) {
    size_t q = p, z;
    char mm[3] = "", ss[3] = "";
    while (q < n && digit(t[q]))
        ++q;
    if (q == p || q - p > 2 || atoi(t + p) > 24)
        return 0;
    if (q + 2 < n && t[q] == ':' && digit(t[q + 1]) && digit(t[q + 2]) &&
        !(q + 3 < n && digit(t[q + 3]))) {
        z = q + 3;
        mm[0] = t[q + 1], mm[1] = t[q + 2];
        if (z + 2 < n && t[z] == ':' && digit(t[z + 1]) && digit(t[z + 2]) &&
            !(z + 3 < n && digit(t[z + 3]))) {
            ss[0] = t[z + 1], ss[1] = t[z + 2];
            z += 3;
        }
        if (atoi(mm) > 59 || letter_at(t, n, z))
            return 0;
    } else if (q < n && (t[q] == 'h' || t[q] == 'g' || t[q] == 'H')) {
        z = q + 1;
        if (z + 1 < n && digit(t[z]) && digit(t[z + 1]) && !(z + 2 < n && digit(t[z + 2]))) {
            mm[0] = t[z], mm[1] = t[z + 1];
            z += 2;
            if (z < n && t[z] == '\'')
                ++z;
        }
        if (letter_at(t, n, z) || (mm[0] && atoi(mm) > 59) || (t[q] == 'g' && !mm[0]))
            return 0;
    } else
        return 0;
    snprintf(v, cap, "%.*s:%s:%s", (int)(q - p), t + p, mm, ss);
    return z;
}

/* A dash between two objects (3-5, 8h - 17h, 2020–2025): the position after it. */
static size_t dash_after(const char *t, size_t n, size_t q) {
    size_t r = q;
    while (r < n && t[r] == ' ')
        ++r;
    if (r < n && (t[r] == '-' || t[r] == '~'))
        ++r;
    else if (r + 3 <= n && (!strncmp(t + r, "–", 3) || !strncmp(t + r, "—", 3)))
        r += 3;
    else if (r + 2 <= n && !strncmp(t + r, "÷", 2))
        r += 2;
    else
        return 0;
    while (r < n && t[r] == ' ')
        ++r;
    return r;
}

/* A Roman numeral (I..MMMM) and its value. */
static int roman(const char *t, size_t n, size_t p, size_t *end) {
    static const char sym[] = "IVXLCDM";
    static const int val[] = {1, 5, 10, 50, 100, 500, 1000};
    size_t q = p;
    int total = 0, prev = 0;
    while (q < n && strchr(sym, t[q]) && t[q])
        ++q;
    if (q == p || q - p > 7 || letter_at(t, n, q))
        return 0;
    for (size_t i = q; i > p; --i) {
        int x = val[strchr(sym, t[i - 1]) - sym];
        total += x < prev ? -x : x;
        if (x > prev)
            prev = x;
    }
    *end = q;
    return total > 0 && total < 4000 ? total : 0;
}

/* One of the |-separated phrases ends within `window` bytes before p (gọi ngay 114). */
static int cue_before(const ttf_engine_t *e, const char *t, size_t p, const char *cue,
                      size_t window) {
    const char *list = res(e, "vi_cue", cue), *w;
    size_t from = p > window ? p - window : 0, k0;
    if (!list)
        return 0;
    /* the clause only: 1.105 mã cổ phiếu giảm, chỉ 214 mã tăng */
    for (k0 = p; k0 > from; --k0)
        if (strchr(",;.!?\n", t[k0 - 1]) && (k0 == p || t[k0] == ' ' || t[k0] == '\n')) {
            from = k0;
            break;
        }
    for (w = list; *w;) {
        const char *bar = strchr(w, '|');
        size_t m = bar ? (size_t)(bar - w) : strlen(w), k;
        for (k = from; k + m <= p; ++k)
            if (!memcmp(t + k, w, m) && (!k || t[k - 1] == ' ' || t[k - 1] == '(' || t[k - 1] == '\n'))
                return 1;
        if (!bar)
            break;
        w = bar + 1;
    }
    return 0;
}

/* 1-5 in "30-4 và 1-5": the word before và/hoặc is a day-month (or T7 for CN). */
static int coordinated(const char *t, size_t p, int weekday) {
    size_t end = p, begin;
    while (end && t[end - 1] == ' ')
        --end;
    begin = end;
    while (begin && t[begin - 1] != ' ')
        --begin;
    if (!((end - begin == strlen("và") && !memcmp(t + begin, "và", end - begin)) ||
          (end - begin == strlen("hoặc") && !memcmp(t + begin, "hoặc", end - begin))))
        return 0;
    end = begin;
    while (end && t[end - 1] == ' ')
        --end;
    begin = end;
    while (begin && t[begin - 1] != ' ')
        --begin;
    if (weekday)
        return (end - begin == 2 && t[begin] == 'T' && t[begin + 1] >= '2' && t[begin + 1] <= '7') ||
               (end - begin == 2 && t[begin] == 'C' && t[begin + 1] == 'N');
    {
        size_t k = begin, d1 = 0, d2 = 0;
        while (k < end && digit(t[k]))
            ++k, ++d1;
        if (!d1 || d1 > 2 || k >= end || !strchr("-/.", t[k]))
            return 0;
        for (++k; k < end && digit(t[k]); ++k)
            ++d2;
        return d2 >= 1 && d2 <= 2;
    }
}

static size_t classify(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const ttf_engine_t *e = in->engine;
    const char *t = in->text;
    size_t n = in->length, p, q, de, z;
    parsed_t a, b;
    /* sec and v hold two values; finish rejects what does not fit the IR */
    char key[24], sec[2 * TTF_IR_VALUE_SIZE + 8], v[2 * TTF_IR_VALUE_SIZE + 8], fl[4],
        w[TTF_IR_VALUE_SIZE];
    int negative = 0, approx = 0;
    const char *prefix_currency = NULL;
    if (in->index >= in->token_count)
        return 0;
    p = in->tokens[in->index].begin;
    if (p >= n)
        return 0;

    if (alpha(t[p]) && (!p || !letter_at(t, n, p - 1))) {
        size_t re;
        int r;
        /* T2-T6 thứ hai đến thứ sáu, T2-CN, CN chủ nhật (after sáng, chiều or T7 và) */
        if (t[p] == 'T' && p + 1 < n && t[p + 1] >= '2' && t[p + 1] <= '7' &&
            !letter_at(t, n, p + 2) && !(p + 2 < n && digit(t[p + 2]))) {
            z = p + 2;
            if ((de = dash_after(t, n, z)) && de + 1 < n &&
                ((t[de] == 'T' && t[de + 1] >= '2' && t[de + 1] <= '7') ||
                 (t[de] == 'C' && t[de + 1] == 'N')) &&
                !letter_at(t, n, de + 2) && !(de + 2 < n && digit(t[de + 2]))) {
                snprintf(v, sizeof(v), "%c|%c", t[p + 1], t[de + 1] == 'N' ? 'C' : t[de + 1]);
                return finish(in, s, de + 2, "weekday", v, "");
            }
            snprintf(v, sizeof(v), "%c|", t[p + 1]);
            return finish(in, s, z, "weekday", v, "");
        }
        if (t[p] == 'C' && p + 1 < n && t[p + 1] == 'N' && !letter_at(t, n, p + 2) &&
            !(p + 2 < n && digit(t[p + 2])) &&
            (previous_cue(e, t, p, "daypart") || coordinated(t, p, 1)))
            return finish(in, s, p + 2, "weekday", "C|", "");
        /* P.308 phòng, P.3 phường, Q.10 quận */
        if ((t[p] == 'P' || t[p] == 'Q') && p + 2 < n && t[p + 1] == '.') {
            size_t d0 = p + 2 + (t[p + 2] == ' '), d = d0;
            while (d < n && digit(t[d]))
                ++d;
            if (d > d0 && d - d0 <= 4 && !letter_at(t, n, d) &&
                !(d + 1 < n && strchr(".,", t[d]) && digit(t[d + 1]))) {
                snprintf(v, sizeof(v), "%s|%.*s",
                         t[p] == 'Q' ? "district" : d - d0 >= 3 ? "room" : "ward", (int)(d - d0), t + d0);
                return finish(in, s, d, "addr", v, "");
            }
        }
        /* Thế kỷ XXI, quý II/2026, lần thứ XIV, vùng I */
        if ((r = roman(t, n, p, &re)) != 0 &&
            (previous_cue(e, t, p, "roman") ||
             /* Quy hoạch điện VIII: I/V/X only, two letters or more, after a
                lowercase word (CD, MC stay letters) */
             (re - p >= 2 && strspn(t + p, "IVX") >= re - p && p >= 2 && t[p - 1] == ' ' &&
              ((t[p - 2] >= 'a' && t[p - 2] <= 'z') || (unsigned char)t[p - 2] >= 0x80)))) {
            char year[8] = "";
            if (re + 4 < n + 0 && t[re] == '/' && digit(t[re + 1]) && digit(t[re + 2]) &&
                digit(t[re + 3]) && digit(t[re + 4]) && !(re + 5 < n && digit(t[re + 5]))) {
                memcpy(year, t + re + 1, 4);
                year[4] = 0;
                re += 5;
            }
            snprintf(v, sizeof(v), "%d|%s", r, year);
            return finish(in, s, re, "roman", v, previous_cue(e, t, p, "ordinal") ? "ord" : "");
        }
        /* A4, VN215: letters then digits. Letters stay as written; one or two
           digits read as a number, longer runs digit by digit. */
        {
            size_t l = p, d, u;
            char prefix[16];
            while (l < n && alpha(t[l]))
                ++l;
            d = l;
            while (d < n && digit(t[d]))
                ++d;
            /* QL1A: one or two capitals after the digits */
            for (u = d; u < n && u - d < 3 && t[u] >= 'A' && t[u] <= 'Z';)
                ++u;
            if (u > d && (u - d > 2 || letter_at(t, n, u)))
                u = d;
            if (d > l && l - p <= 8 && !letter_at(t, n, u) &&
                !(u + 1 < n && (t[u] == '.' || t[u] == ',') && digit(t[u + 1]))) {
                snprintf(prefix, sizeof(prefix), "%.*s", (int)(l - p), t + p);
                snprintf(v, sizeof(v), "%s|%.*s|%.*s", res(e, "vi_prefix", prefix) ? res(e, "vi_prefix", prefix) : prefix,
                         (int)(d - l), t + l, (int)(u - d), t + d);
                return finish(in, s, u, "alnum", v, "");
            }
        }
        return 0;
    }

    /* $20, €5, ~100, -5 */
    q = p;
    if (t[q] == '~' && q + 1 < n && digit(t[q + 1]))
        approx = 1, ++q;
    else if (t[q] == '$' || !strncmp(t + q, "€", 3) || !strncmp(t + q, "£", 2) ||
             !strncmp(t + q, "¥", 2)) {
        size_t sw = t[q] == '$' ? 1 : !strncmp(t + q, "€", 3) ? 3 : 2;
        char sym[4] = {0};
        memcpy(sym, t + q, sw);
        if (q + sw < n && digit(t[q + sw]) && res(e, "vi_currency", sym)) {
            prefix_currency = res(e, "vi_currency", sym);
            q += sw;
        }
    } else if ((t[q] == '-' || !strncmp(t + q, "−", 3) || !strncmp(t + q, "–", 3)) &&
               (!q || t[q - 1] == ' ' || t[q - 1] == '(' || t[q - 1] == '=' || t[q - 1] == ':')) {
        size_t sw = t[q] == '-' ? 1 : 3;
        if (q + sw < n && digit(t[q + sw]))
            negative = 1, q += sw;
    }
    if (q >= n || !digit(t[q]))
        return 0;
    if (p == q && p && (digit(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ','))
        return 0;
    de = q;
    while (de < n && digit(t[de]))
        ++de;
    {
        int plain = !negative && !prefix_currency && !approx;

        /* lúc 22g: g without minutes is giờ after a clock cue (500g stays gam) */
        if (plain && p == q && de - q <= 2 && atoi(t + q) <= 24 && de < n && t[de] == 'g' &&
            !letter_at(t, n, de + 1) && !(de + 1 < n && digit(t[de + 1])) && previous_cue(e, t, p, "clock")) {
            snprintf(v, sizeof(v), "%.*s::", (int)(de - q), t + q);
            return finish(in, s, de + 1, "time", v, "");
        }
        /* 1tr5 một triệu năm trăm nghìn, 2tr25 */
        if (plain && p == q && de - q <= 3 && de + 2 < n && t[de] == 't' && t[de + 1] == 'r' &&
            digit(t[de + 2])) {
            size_t z2 = de + 2;
            while (z2 < n && digit(t[z2]))
                ++z2;
            if (z2 - de - 2 <= 3 && !letter_at(t, n, z2) && !(z2 < n && digit(t[z2]))) {
                snprintf(v, sizeof(v), "%.*s|%.*s", (int)(de - q), t + q, (int)(z2 - de - 2), t + de + 2);
                return finish(in, s, z2, "trd", v, "");
            }
        }
        /* 4N3Đ bốn ngày ba đêm */
        if (plain && p == q && de - q <= 2 && de + 1 < n && t[de] == 'N' && digit(t[de + 1])) {
            size_t z2 = de + 1;
            while (z2 < n && digit(t[z2]))
                ++z2;
            if (z2 - de - 1 <= 2 && z2 + 2 <= n && !strncmp(t + z2, "Đ", 2) && !letter_at(t, n, z2 + 2)) {
                snprintf(v, sizeof(v), "%.*s|%.*s", (int)(de - q), t + q, (int)(z2 - de - 1), t + de + 1);
                return finish(in, s, z2 + 2, "tour", v, "");
            }
        }
        /* 2024-05-14, 2024/05/14: an ISO date */
        if (plain && de - q == 4 && de + 5 < n && (t[de] == '-' || t[de] == '/') && digit(t[de + 1]) &&
            digit(t[de + 2]) && t[de + 3] == t[de] && digit(t[de + 4]) && digit(t[de + 5]) &&
            !(de + 6 < n && (digit(t[de + 6]) || letter_at(t, n, de + 6)))) {
            unsigned mm = (unsigned)atoi(t + de + 1), dd = (unsigned)atoi(t + de + 4);
            if (mm >= 1 && mm <= 12 && dd >= 1 && dd <= 31) {
                snprintf(v, sizeof(v), "%u/%u/%.4s", dd, mm, t + q);
                return finish(in, s, de + 6, "date", v,
                              previous_cue(e, t, p, "day") || previous_cue(e, t, p, "noday") ? "" : "ngay");
            }
        }

        /* 1m78 một mét bảy mươi tám */
        if (plain && p == q && de - q <= 1 && de + 2 < n && t[de] == 'm' && digit(t[de + 1]) &&
            digit(t[de + 2]) && !(de + 3 < n && digit(t[de + 3])) && !letter_at(t, n, de + 3)) {
            snprintf(v, sizeof(v), "%.*s|%c%c", (int)(de - q), t + q, t[de + 1], t[de + 2]);
            return finish(in, s, de + 3, "height", v, "");
        }

        /* 5G, 4K: digits then letters that are no unit */
        if (plain && p == q && (!p || !letter_at(t, n, p - 1)) && de - q <= 4) {
            size_t l = de;
            while (l < n && alpha(t[l]))
                ++l;
            if (l > de && !letter_at(t, n, l) && !(l < n && digit(t[l])) &&
                !suffix_at(e, t, n, de, key, sizeof(key), NULL) && !clock_at(t, n, q, w, sizeof(w))) {
                snprintf(v, sizeof(v), "%.*s|%.*s", (int)(de - q), t + q, (int)(l - de), t + de);
                return finish(in, s, l, "numalpha", v, "");
            }
        }

        /* phone: 0912 345 678, 0912.345.678, 090-123-4567; a hotline 1900 1234;
           a code after mã, OTP, tổng đài; a service number after gọi (114) */
        {
            int hotline = de - q == 4 && (!strncmp(t + q, "1900", 4) || !strncmp(t + q, "1800", 4));
            /* mã nguồn (source code) is no code cue */
            int code = cue_before(e, t, p, "code", 80) && !cue_before(e, t, p, "code_not", 80);
            /* seven or more ungrouped digits not ending in 000 are a code (7480201);
               so are five after số (mang số 240715) and four after cổng (8080) */
            int ungrouped = !(de + 1 < n && strchr(".,", t[de]) && digit(t[de + 1])) &&
                            strncmp(t + de - 3, "000", 3);
            int long_code = ungrouped && (de - q >= 7 ||
                                          (de - q >= 5 && previous_cue(e, t, p, "id5")) ||
                                          (de - q >= 4 && previous_cue(e, t, p, "id4")));
            if (plain && (t[q] == '0' || hotline || code || long_code)) {
                size_t digits = 0, groups = 0;
                z = q;
                while (z < n) {
                    size_t g = z;
                    while (z < n && digit(t[z]))
                        ++z;
                    if (z == g)
                        break;
                    digits += z - g, ++groups;
                    if (z + 1 < n && (t[z] == ' ' || t[z] == '.' || t[z] == '-') && digit(t[z + 1]))
                        ++z;
                    else
                        break;
                }
                if (!letter_at(t, n, z) && !suffix_at(e, t, n, z, key, sizeof(key), NULL) &&
                    ((t[q] == '0' && digits >= 9 && digits <= 11 && groups <= 4) ||
                     (hotline && groups >= 2 && digits >= 8) ||
                     (code && digits >= 3) || (long_code && groups == 1))) {
                    size_t k, used = 0;
                    for (k = q; k < z && used + 1 < sizeof(v); ++k)
                        if (digit(t[k]))
                            v[used++] = t[k];
                    v[used] = 0;
                    return finish(in, s, z, "digits", v, "");
                }
            }
            if (plain && de - q == 3 && !letter_at(t, n, de) && !(de < n && digit(t[de])) &&
                !(de + 1 < n && strchr(".,", t[de]) && digit(t[de + 1]))) {
                char three_digits[4] = {t[q], t[q + 1], t[q + 2], 0};
                if (res(e, "vi_service", three_digits) && cue_before(e, t, p, "call", 16))
                    return finish(in, s, de, "digits", three_digits, "");
            }
            /* gửi tới 8085: a short number right after a sending phrase */
            if (plain && de - q >= 3 && de - q <= 6 && !letter_at(t, n, de) && !(de < n && digit(t[de])) &&
                !(de + 1 < n && strchr(".,", t[de]) && digit(t[de + 1])) && cue_before(e, t, p, "sms", 12)) {
                snprintf(v, sizeof(v), "%.*s", (int)(de - q), t + q);
                return finish(in, s, de, "digits", v, "");
            }
        }

        /* 24/7 */
        if (plain && de < n && t[de] == '/') {
            size_t y = de + 1;
            while (y < n && digit(t[y]))
                ++y;
            if (y > de + 1 && y - q < sizeof(key) && !letter_at(t, n, y) && !(y < n && digit(t[y]))) {
                snprintf(key, sizeof(key), "%.*s", (int)(y - q), t + q);
                if (res(e, "vi_idiom", key))
                    return finish(in, s, y, "idiom", key, "");
            }
        }

        /* clock and clock range: 9:30, 8h-17h */
        if (plain && (z = clock_at(t, n, q, v, sizeof(v))) != 0) {
            size_t r = dash_after(t, n, z), y;
            if (r && (y = clock_at(t, n, r, w, sizeof(w))) != 0) {
                snprintf(sec, sizeof(sec), "%.95s|%.95s", v, w);
                flags(t, n, p, y, fl, sizeof(fl));
                return finish(in, s, y, "timerange", sec, fl);
            }
            flags(t, n, p, z, fl, sizeof(fl));
            return finish(in, s, z, "time", v, fl);
        }

        /* days of one month: 20-25/11, 12-14/10/2026 */
        if (plain && de - q <= 2) {
            size_t r = dash_after(t, n, de), r2 = r, m0, m1, y0, y;
            while (r && r2 < n && digit(t[r2]))
                ++r2;
            if (r && r2 > r && r2 - r <= 2 && r2 + 1 < n && t[r2] == '/' && digit(t[r2 + 1])) {
                unsigned d1 = (unsigned)atoi(t + q), d2 = (unsigned)atoi(t + r), m;
                m0 = m1 = r2 + 1;
                while (m1 < n && digit(t[m1]))
                    ++m1;
                m = (unsigned)atoi(t + m0);
                y0 = y = m1;
                if (m1 + 2 < n && t[m1] == '/' && digit(t[m1 + 1]))
                    for (y0 = y = m1 + 1; y < n && digit(t[y]);)
                        ++y;
                if (m1 - m0 <= 2 && m >= 1 && m <= 12 && d1 >= 1 && d1 < d2 && d2 <= 31 &&
                    (y == y0 || y - y0 == 4 || y - y0 == 2) && !letter_at(t, n, y) &&
                    !(y < n && digit(t[y]))) {
                    snprintf(v, sizeof(v), "%u|%u/%u/%.*s", d1, d2, m, (int)(y - y0), t + y0);
                    return finish(in, s, y == y0 ? m1 : y, "daterange", v, "");
                }
            }
        }

        /* dates: 13/10/2026, 13-10-2026, 13.10.2026; 30/4, ngày 26-6, ngày 2.9, lễ 30-4,
           30-4 và 1-5; 30/4-1/5; tháng 10/2026 */
        if (plain && de - q <= 2 && de + 1 < n && (t[de] == '/' || t[de] == '-' || t[de] == '.') &&
            digit(t[de + 1])) {
            char sep = t[de];
            size_t m0 = de + 1, m1 = m0;
            unsigned d = (unsigned)atoi(t + q), m;
            /* ngày is said unless a day word, lễ or và comes before */
            int day_said = previous_cue(e, t, p, "day") || previous_cue(e, t, p, "noday") ||
                           (sep != '/' && cue_before(e, t, p, "daywide", 40));
            while (m1 < n && digit(t[m1]))
                ++m1;
            m = (unsigned)atoi(t + m0);
            if (m1 - m0 <= 2 && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
                if (m1 + 2 < n && t[m1] == sep && digit(t[m1 + 1])) {
                    size_t y0 = m1 + 1, y = y0;
                    while (y < n && digit(t[y]))
                        ++y;
                    if ((y - y0 == 4 || y - y0 == 2) && !letter_at(t, n, y) &&
                        !(y < n && (digit(t[y]) || (t[y] == sep && y + 1 < n && digit(t[y + 1]))))) {
                        snprintf(v, sizeof(v), "%u/%u/%.*s", d, m, (int)(y - y0), t + y0);
                        return finish(in, s, y, "date", v, day_said ? "" : "ngay");
                    }
                }
                if (!(m1 < n && (digit(t[m1]) || letter_at(t, n, m1) ||
                                 /* ngày 30.6. ends the sentence */
                                 (t[m1] == sep && !(sep == '.' && (m1 + 1 == n || t[m1 + 1] == ' ' ||
                                                                   t[m1 + 1] == '\n'))))) &&
                    !(m1 + 1 < n && strchr("/.,", t[m1]) && digit(t[m1 + 1])) &&
                    ((sep == '/' && (previous_cue(e, t, p, "date") || d > m || coordinated(t, p, 0) ||
                                     cue_before(e, t, p, "deadline", 48))) ||
                     (sep != '/' && (previous_cue(e, t, p, "day") || previous_cue(e, t, p, "holiday") ||
                                     coordinated(t, p, 0) || cue_before(e, t, p, "daywide", 40))))) {
                    /* 30/4-1/5: a second day-month after a dash */
                    size_t r = sep == '/' ? dash_after(t, n, m1) : 0, r1 = r, r2;
                    while (r && r1 < n && digit(t[r1]))
                        ++r1;
                    if (r && r1 > r && r1 - r <= 2 && r1 + 1 < n && t[r1] == '/' && digit(t[r1 + 1])) {
                        unsigned d2 = (unsigned)atoi(t + r), m2 = (unsigned)atoi(t + r1 + 1);
                        for (r2 = r1 + 1; r2 < n && digit(t[r2]);)
                            ++r2;
                        if (r2 - r1 - 1 <= 2 && d2 >= 1 && d2 <= 31 && m2 >= 1 && m2 <= 12 &&
                            !letter_at(t, n, r2) && !(r2 < n && (digit(t[r2]) || t[r2] == '/'))) {
                            snprintf(v, sizeof(v), "%u/%u/|%u/%u/", d, m, d2, m2);
                            return finish(in, s, r2, "date2", v, day_said ? "" : "ngay");
                        }
                    }
                    snprintf(v, sizeof(v), "%u/%u/", d, m);
                    return finish(in, s, m1, "date", v, day_said ? "" : "ngay");
                }
            }
            if ((sep == '/' || sep == '-') && d >= 1 && d <= 12 && m1 - m0 == 4 &&
                !(m1 < n && (digit(t[m1]) || letter_at(t, n, m1)))) {
                size_t r = dash_after(t, n, m1), r1 = r, r2;
                snprintf(v, sizeof(v), "%u/%.*s", d, 4, t + m0);
                /* 3/2023-12/2024: a second month and year after a dash */
                while (r && r1 < n && digit(t[r1]))
                    ++r1;
                if (r && r1 > r && r1 - r <= 2 && r1 + 1 < n && t[r1] == '/') {
                    for (r2 = r1 + 1; r2 < n && digit(t[r2]);)
                        ++r2;
                    if (r2 - r1 - 1 == 4 && atoi(t + r) >= 1 && atoi(t + r) <= 12 &&
                        !letter_at(t, n, r2) && !(r2 < n && digit(t[r2]))) {
                        size_t used = strlen(v);
                        snprintf(v + used, sizeof(v) - used, "|%u/%.4s", (unsigned)atoi(t + r), t + r1 + 1);
                        return finish(in, s, r2, "monthyear2", v,
                                      previous_cue(e, t, p, "month") ? "" : "thang");
                    }
                }
                return finish(in, s, m1, "monthyear", v,
                              previous_cue(e, t, p, "month") ? "" : "thang");
            }
        }

        /* versions: 2.1.3, iOS 18.2, phiên bản 2.0 */
        if (plain && de + 1 < n && t[de] == '.' && digit(t[de + 1])) {
            size_t y = de, parts = 1;
            int ip = de - q <= 3 && atoi(t + q) <= 255;
            while (y + 1 < n && t[y] == '.' && digit(t[y + 1])) {
                size_t g = ++y;
                while (y < n && digit(t[y]))
                    ++y;
                ip &= y - g <= 3 && atoi(t + g) <= 255;
                ++parts;
            }
            ip &= parts == 4;
            /* 1.250.000.000: groups of three after the first are thousands, not an IP */
            if (ip) {
                size_t g = de, all_three = 1;
                while (g + 1 < y && t[g] == '.') {
                    size_t h = g + 1;
                    while (h < y && digit(t[h]))
                        ++h;
                    all_three &= h - g - 1 == 3;
                    g = h;
                }
                ip = !all_three;
            }
            if (!letter_at(t, n, y) && !(y + 1 < n && t[y] == '.' && digit(t[y + 1]))) {
                size_t back = p, ascii = 1, letters = 0;
                while (back && t[back - 1] == ' ')
                    --back;
                while (back && t[back - 1] != ' ') {
                    --back;
                    letters += alpha(t[back]);
                    ascii &= (unsigned char)t[back] < 0x80;
                }
                /* 4.0 (công nghiệp 4.0): a dotted .0 label */
                if (ip || (parts == 2 && y == de + 2 && t[de + 1] == '0') ||
                    (parts >= 3 && !three(t, n, de + 1)) ||
                    (parts == 2 && !three(t, n, de + 1) && ((ascii && letters) ||
                                                            previous_cue(e, t, p, "version")))) {
                    snprintf(v, sizeof(v), "%.*s", (int)(y - q), t + q);
                    return finish(in, s, y, "version", v, "");
                }
            }
        }
    }

    if (!parse_number(t, n, q, &a))
        return 0;
    q = a.end;
    if (prefix_currency && a.decimal) {
        const char *comma = strchr(a.value, ',');
        if (comma && comma - a.value <= 3 && strlen(comma + 1) == 3 && strspn(comma + 1, "0123456789") == 3) {
            memmove((char *)comma, comma + 1, strlen(comma + 1) + 1);
            a.decimal = 0;
            a.grouped = 1;
        }
    }

    /* dimensions: 60x90 cm, 40 × 30 × 20 cm, 1920x1080 */
    if (!negative && !approx) {
        size_t z1 = q, k2;
        char dims[2 * TTF_IR_VALUE_SIZE + 8];
        int count = 1;
        snprintf(dims, sizeof(dims), "%s", a.value);
        for (k2 = 0; k2 < 2; ++k2) {
            size_t x = z1 + (z1 < n && t[z1] == ' ');
            size_t w2 = x < n && (t[x] == 'x' || t[x] == 'X' || t[x] == '*') ? 1
                        : x + 2 <= n && !strncmp(t + x, "×", 2) ? 2 : 0;
            size_t y = x + w2 + (w2 && x + w2 < n && t[x + w2] == ' ');
            if (!w2 || y >= n || !digit(t[y]) || !parse_number(t, n, y, &b) || b.leading_zero)
                break;
            if (strlen(dims) + strlen(b.value) + 2 >= sizeof(dims))
                break;
            strcat(dims, "|");
            strcat(dims, b.value);
            z1 = b.end;
            ++count;
        }
        if (count >= 2 && !letter_at(t, n, z1)) {
            char mid[24], per[24];
            size_t end = amount_tail(e, t, n, z1, key, sizeof(key), mid, sizeof(mid), per, sizeof(per));
            return finish(in, s, end, "dims", dims, key);
        }
    }

    /* ranges: 3-5, 2020–2025, 10-20%; a higher first number is a score (2-1) */
    {
        /* 500k-800k: the first end may carry its own scale */
        size_t qa = suffix_at(e, t, n, q, w, sizeof(w), "s");
        if (!qa || (qa < n && t[qa] != '-' && t[qa] != ' ' && (unsigned char)t[qa] < 0x80))
            qa = q, w[0] = 0;
        z = dash_after(t, n, qa);
        if (!z)
            w[0] = 0;
    }
    if (!negative && !approx && z != 0 && digit(t[z]) &&
        parse_number(t, n, z, &b) && !b.leading_zero && !a.leading_zero &&
        !(b.end < n && (t[b.end] == '-' || t[b.end] == '/' || t[b.end] == ':'))) {
        char mid[24], per[24];
        double x = atof(a.value), y = atof(b.value);
        size_t end = amount_tail(e, t, n, b.end, key, sizeof(key), mid, sizeof(mid), per, sizeof(per));
        flags(t, n, p, end, fl, sizeof(fl));
        snprintf(sec, sizeof(sec), "%s|%s|%s|%s|%s|%s", key, x <= y ? "to" : "pair", fl, mid, per, w);
        snprintf(v, sizeof(v), "%s|%s", a.value, b.value);
        return finish(in, s, end, "range", v, sec);
    }

    /* fractions: 1/2 một phần hai; 120/80 mmHg and Nghị định 100/2019 read trên */
    if (!a.grouped && !a.decimal && q + 1 < n && t[q] == '/' && digit(t[q + 1]) &&
        parse_number(t, n, q + 1, &b) && !b.grouped && !b.decimal) {
        int year = strlen(b.value) == 4 && (!strncmp(b.value, "19", 2) || !strncmp(b.value, "20", 2)) &&
                   strlen(a.value) <= 4;
        if (!(b.end < n && ((t[b.end] == '/' && !(year && letter_at(t, n, b.end + 1))) ||
                            letter_at(t, n, b.end)))) {
            size_t ue = suffix_at(e, t, n, b.end, key, sizeof(key), "u");
            snprintf(v, sizeof(v), "%s%s|%s", negative ? "-" : "", a.value, b.value);
            snprintf(sec, sizeof(sec), "%s|%s", ue || year ? "tren" : "", ue ? key : "");
            return finish(in, s, ue ? ue : b.end, "frac", v, sec);
        }
    }

    /* codes with a leading zero: 05, 007 */
    if (a.leading_zero && !a.decimal && !negative && !prefix_currency) {
        flags(t, n, p, q, fl, sizeof(fl));
        return finish(in, s, q, "digits", a.value, fl);
    }

    /* thứ 4 thứ tư, tháng 4 tháng tư */
    if (!negative && !a.grouped && !a.decimal && strlen(a.value) <= 2 && !letter_at(t, n, q) &&
        !(q + 1 < n && strchr("/-.,", t[q]) && digit(t[q + 1]))) {
        if (previous_cue(e, t, p, "ordinal"))
            return finish(in, s, q, "ord", a.value, "");
        if (previous_cue(e, t, p, "month") && atoi(a.value) >= 1 && atoi(a.value) <= 12)
            return finish(in, s, q, "month", a.value, "");
    }

    /* a number and its suffix: 15.000đ, 2,5 triệu USD, 4,8%/năm, 320 đồng/lít */
    {
        char mid[24], per[24];
        size_t end = amount_tail(e, t, n, q, key, sizeof(key), mid, sizeof(mid), per, sizeof(per));
        if (prefix_currency) {
            char sym[4] = {0};
            memcpy(sym, t + p, t[p] == '$' ? 1 : t[p] == '\xe2' ? 3 : 2);
            snprintf(key, sizeof(key), "c:%s", sym);
        }
        flags(t, n, p, end, fl, sizeof(fl));
        snprintf(v, sizeof(v), "%s%s", negative ? "-" : "", a.value);
        snprintf(sec, sizeof(sec), "%s|%s%s|%s|%s", key, fl, approx ? "~" : "", mid, per);
        return finish(in, s, end, "num", v, sec);
    }
}

/* ---- verbalization ---- */

static ttf_status_t date(const ttf_engine_t *e, ttf_buffer_t *b, const char *v, int day_word) {
    const char *s1 = strchr(v, '/'), *s2 = s1 ? strchr(s1 + 1, '/') : NULL;
    if (!s1 || !s2)
        return TTF_EINVAL;
    if (day_word)
        TRY(word(e, b, "day"));
    TRY(integer(e, b, v, (size_t)(s1 - v)));
    TRY(word(e, b, "month"));
    TRY(month(e, b, s1 + 1, (size_t)(s2 - s1 - 1)));
    if (s2[1]) {
        TRY(word(e, b, "year"));
        TRY(integer(e, b, s2 + 1, strlen(s2 + 1)));
    }
    return TTF_OK;
}

/* H:MM:SS, parts may be empty: chín giờ ba mươi phút; :00 is not said */
static ttf_status_t clock_words(const ttf_engine_t *e, ttf_buffer_t *b, const char *v,
                                size_t n) {
    const char *c1 = memchr(v, ':', n), *c2 = c1 ? memchr(c1 + 1, ':', n - (size_t)(c1 + 1 - v)) : NULL;
    size_t mn, sn;
    if (!c1 || !c2)
        return TTF_EINVAL;
    TRY(integer(e, b, v, (size_t)(c1 - v)));
    TRY(word(e, b, "hour"));
    mn = (size_t)(c2 - c1 - 1);
    sn = n - (size_t)(c2 + 1 - v);
    if (mn && strncmp(c1 + 1, "00", 2)) {
        TRY(integer(e, b, c1 + 1, mn));
        TRY(word(e, b, "minute"));
    }
    if (sn && strncmp(c2 + 1, "00", 2)) {
        TRY(integer(e, b, c2 + 1, sn));
        TRY(word(e, b, "second"));
    }
    return TTF_OK;
}

/* The n-th |-separated field of s, copied. */
static void field(const char *s, int k, char *out, size_t cap) {
    const char *bar;
    while (k-- > 0 && (bar = strchr(s, '|')) != NULL)
        s = bar + 1;
    if (k >= 0) {
        *out = 0;
        return;
    }
    bar = strchr(s, '|');
    snprintf(out, cap, "%.*s", (int)(bar ? (size_t)(bar - s) : strlen(s)), s);
}

static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *b) {
    const char *mode = s->unit, *v = s->value;
    char f0[TTF_IR_VALUE_SIZE], f1[TTF_IR_VALUE_SIZE], f2[TTF_IR_VALUE_SIZE], f3[32], f4[32];
    if (strncmp(mode, "@vi-", 4)) {
        /* Shared objects in a Vietnamese sentence. */
        if (s->type == TTF_SEM_PHONE || s->type == TTF_SEM_DIGIT_SEQUENCE)
            return digits_each(e, b, surface, length);
        if ((s->type == TTF_SEM_CARDINAL || s->type == TTF_SEM_NUMBER) && *v &&
            strspn(v, "0123456789") == strlen(v))
            return integer(e, b, v, strlen(v));
        /* an address label (abc123@gmail.com): digits one by one, letters as written */
        if (s->type == TTF_SEM_CUSTOM && !strcmp(mode, "@alphabet"))
            return !strcmp(s->secondary, "control-surface") ? digits_each(e, b, surface, length)
                                                           : digits_each(e, b, v, strlen(v));
        return ttf_buffer_append(b, surface, length);
    }
    mode += 4;
    if (strchr(s->secondary, '<'))
        TRY(ttf_buffer_puts(b, " "));
    if (!strcmp(mode, "alnum") || !strcmp(mode, "numalpha")) {
        int letters_first = mode[0] == 'a';
        size_t dn;
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        dn = strlen(letters_first ? f1 : f0);
        if (letters_first)
            TRY(put(b, f0));
        TRY(dn <= 2 ? integer(e, b, letters_first ? f1 : f0, dn)
                    : digits_each(e, b, letters_first ? f1 : f0, dn));
        if (!letters_first)
            TRY(put(b, f1));
        else {
            field(v, 2, f2, sizeof(f2));
            if (*f2)
                TRY(put(b, f2));
        }
    } else if (!strcmp(mode, "trd")) {
        char rest[4] = "000";
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        memcpy(rest, f1, strlen(f1) < 3 ? strlen(f1) : 3);
        TRY(integer(e, b, f0, strlen(f0)));
        TRY(word(e, b, "million"));
        TRY(integer(e, b, rest, 3));
        TRY(word(e, b, "thousand"));
    } else if (!strcmp(mode, "tour")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        TRY(integer(e, b, f0, strlen(f0)));
        TRY(word(e, b, "day"));
        TRY(integer(e, b, f1, strlen(f1)));
        TRY(word(e, b, "night"));
    } else if (!strcmp(mode, "dims")) {
        int k2;
        for (k2 = 0; k2 < 3; ++k2) {
            field(v, k2, f0, sizeof(f0));
            if (!*f0)
                break;
            if (k2)
                TRY(word(e, b, "times"));
            TRY(number(e, b, f0));
        }
        TRY(suffix(e, b, s->secondary));
    } else if (!strcmp(mode, "digits"))
        TRY(digits_each(e, b, v, strlen(v)));
    else if (!strcmp(mode, "time"))
        TRY(clock_words(e, b, v, strlen(v)));
    else if (!strcmp(mode, "timerange")) {
        const char *bar = strchr(v, '|');
        if (!bar)
            return TTF_EINVAL;
        TRY(clock_words(e, b, v, (size_t)(bar - v)));
        TRY(word(e, b, "to"));
        TRY(clock_words(e, b, bar + 1, strlen(bar + 1)));
    } else if (!strcmp(mode, "date"))
        TRY(date(e, b, v, !strcmp(s->secondary, "ngay")));
    else if (!strcmp(mode, "date2")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        TRY(date(e, b, f0, !strcmp(s->secondary, "ngay")));
        TRY(word(e, b, "to"));
        TRY(date(e, b, f1, 1));
    } else if (!strcmp(mode, "daterange")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        TRY(integer(e, b, f0, strlen(f0)));
        TRY(word(e, b, "to"));
        TRY(date(e, b, f1, 0));
    } else if (!strcmp(mode, "height")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        TRY(integer(e, b, f0, strlen(f0)));
        TRY(suffix(e, b, "u:m"));
        if (f1[0] == '0') {
            /* 1m05 một mét linh năm */
            TRY(word(e, b, "zero_tens"));
            TRY(digit_word(e, b, f1[1]));
        } else
            TRY(integer(e, b, f1, strlen(f1)));
    } else if (!strcmp(mode, "addr")) {
        char k2[24];
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        snprintf(k2, sizeof(k2), "addr_%s", f0);
        TRY(word(e, b, k2));
        TRY(integer(e, b, f1, strlen(f1)));
    }
    else if (!strcmp(mode, "monthyear")) {
        const char *sl = strchr(v, '/');
        if (!sl)
            return TTF_EINVAL;
        if (!strcmp(s->secondary, "thang"))
            TRY(word(e, b, "month"));
        TRY(month(e, b, v, (size_t)(sl - v)));
        TRY(word(e, b, "year"));
        TRY(integer(e, b, sl + 1, strlen(sl + 1)));
    } else if (!strcmp(mode, "monthyear2")) {
        const char *sl;
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        if (!(sl = strchr(f0, '/')))
            return TTF_EINVAL;
        if (!strcmp(s->secondary, "thang"))
            TRY(word(e, b, "month"));
        TRY(month(e, b, f0, (size_t)(sl - f0)));
        TRY(word(e, b, "year"));
        TRY(integer(e, b, sl + 1, strlen(sl + 1)));
        TRY(word(e, b, "to"));
        if (!(sl = strchr(f1, '/')))
            return TTF_EINVAL;
        TRY(word(e, b, "month"));
        TRY(month(e, b, f1, (size_t)(sl - f1)));
        TRY(word(e, b, "year"));
        TRY(integer(e, b, sl + 1, strlen(sl + 1)));
    } else if (!strcmp(mode, "month"))
        TRY(month(e, b, v, strlen(v)));
    else if (!strcmp(mode, "ord") || !strcmp(mode, "roman")) {
        int x;
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        x = atoi(f0);
        if (!strcmp(mode, "ord") || !strcmp(s->secondary, "ord"))
            TRY(x == 1 ? word(e, b, "ordinal1") : x == 4 ? word(e, b, "ordinal4")
                                                          : integer(e, b, f0, strlen(f0)));
        else
            TRY(integer(e, b, f0, strlen(f0)));
        if (*f1) {
            TRY(word(e, b, "year"));
            TRY(integer(e, b, f1, strlen(f1)));
        }
    } else if (!strcmp(mode, "weekday")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        if (f0[0] == 'C')
            TRY(word(e, b, "sunday"));
        else {
            TRY(word(e, b, "weekday"));
            TRY(digit_word(e, b, f0[0]));
        }
        if (*f1) {
            TRY(word(e, b, "to"));
            if (f1[0] == 'C')
                TRY(word(e, b, "sunday"));
            else {
                TRY(word(e, b, "weekday"));
                TRY(digit_word(e, b, f1[0]));
            }
        }
    } else if (!strcmp(mode, "version")) {
        const char *p = v;
        while (*p) {
            size_t k = strspn(p, "0123456789");
            if (!k)
                return TTF_EINVAL;
            TRY(integer(e, b, p, k));
            p += k;
            if (*p == '.') {
                TRY(word(e, b, "dot"));
                ++p;
            }
        }
    } else if (!strcmp(mode, "idiom")) {
        const char *reading = res(e, "vi_idiom", v);
        TRY(reading ? put(b, reading) : TTF_ERESOURCE);
    } else if (!strcmp(mode, "frac")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        field(s->secondary, 0, f2, sizeof(f2));
        field(s->secondary, 1, f3, sizeof(f3));
        TRY(number(e, b, f0));
        TRY(word(e, b, *f2 ? "per" : "over"));
        TRY(number(e, b, f1));
        TRY(suffix(e, b, f3));
    } else if (!strcmp(mode, "range")) {
        field(v, 0, f0, sizeof(f0));
        field(v, 1, f1, sizeof(f1));
        field(s->secondary, 0, f2, sizeof(f2));
        field(s->secondary, 1, f3, sizeof(f3));
        TRY(number(e, b, f0));
        field(s->secondary, 5, f4, sizeof(f4));
        if (*f4)
            TRY(suffix(e, b, f4));
        if (!strcmp(f3, "to"))
            TRY(word(e, b, "to"));
        TRY(number(e, b, f1));
        field(s->secondary, 3, f3, sizeof(f3));
        field(s->secondary, 4, f4, sizeof(f4));
        if (*f3)
            TRY(put(b, f3));
        TRY(suffix(e, b, f2));
        if (*f4) {
            TRY(word(e, b, "per_amount"));
            TRY(suffix(e, b, f4 + 1));
        }
    } else if (!strcmp(mode, "num")) {
        field(s->secondary, 0, f0, sizeof(f0));
        field(s->secondary, 1, f1, sizeof(f1));
        field(s->secondary, 2, f2, sizeof(f2));
        field(s->secondary, 3, f3, sizeof(f3));
        if (strchr(f1, '~'))
            TRY(word(e, b, "approx"));
        TRY(number(e, b, v));
        if (*f2)
            TRY(put(b, f2));
        TRY(suffix(e, b, f0));
        if (*f3) {
            /* 4.960.000 đồng/tháng: đồng một tháng (a unit over a unit keeps trên) */
            TRY(word(e, b, "per_amount"));
            TRY(suffix(e, b, f3 + 1));
        }
    } else
        return TTF_EINVAL;
    if (strchr(s->secondary, '>'))
        TRY(ttf_buffer_puts(b, " "));
    return TTF_OK;
}

/* Latin letters belong to the English pack; this pack takes a Latin sentence
   through context (latin_sentence) or a declared request. */
static int script_score(ttf_script_t script) {
    return script == TTF_SCRIPT_LATIN ? 1 : 0;
}

/* A declared request routes everything but Hangul and kana to this pack,
   except a sentence decided for another language (Please send 3 files). */
static int context_score(const ttf_pack_input_t *in) {
    const ttf_engine_t *e = in->engine;
    const ttf_language_pack_t *sentence;
    if (!e || !e->default_language || strcmp(e->default_language, VI) ||
        in->index >= in->token_count || in->tokens[in->index].script == TTF_SCRIPT_HANGUL ||
        in->tokens[in->index].script == TTF_SCRIPT_KANA)
        return 0;
    sentence = ttf_sentence_language(in);
    if (sentence && strcmp(sentence->language_tag, VI))
        return 0;
    return 1000;
}

const ttf_language_pack_t ttf_pack_vi_VN = {sizeof(ttf_language_pack_t),
                                            TTF_ABI_VERSION,
                                            VI,
                                            NULL,
                                            script_score,
                                            context_score,
                                            classify,
                                            verbalize,
                                            NULL,
                                            NULL,
                                            NULL};
