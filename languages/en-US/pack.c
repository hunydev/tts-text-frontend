#include "tts_text_frontend/ttf_language_pack.h"
#include "../../src/pronunciation/trace.h"
#include "date.h"
#include "abbreviation.h"
#include "identifiers.h"
#include "speech.h"
#include "../../src/semantic/phone.h"
#include "../../src/language/context_memo.h"
#include "../context_run.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EN "en-US"
static char ascii_lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c;
}
static const char *res(const ttf_engine_t *e, const char *kind, const char *key) {
    return ttf_resource_get(e, kind, EN, key, NULL);
}
/* Lowercase ASCII letters that read as a word: two vowel letters or more (y after the
   first letter counts) with no run of four consonant letters (MAESTRO, SANCTUARY), or one
   vowel letter with no run of three (SPARK, CRUSH; NCSOFT stays letters). A romanized Korean
   syllable (eun, jeong, seol: EUNJOO, JEONGHO) is a name spelled out, not an English word. */
static int word_shaped(const char *key, size_t n) {
    size_t j, vowels = 0, run = 0, longest = 0;
    for (j = 0; j + 2 < n; ++j)
        if (key[j] == 'e' && (key[j + 1] == 'o' || key[j + 1] == 'u') && strchr("nlm", key[j + 2]))
            return 0;
    for (j = 0; j < n; ++j) {
        int vowel = strchr("aeiou", key[j]) != NULL || (j && key[j] == 'y');
        if (key[j] < 'a' || key[j] > 'z')
            return 0;
        vowels += vowel;
        run = vowel ? 0 : run + 1;
        if (run > longest)
            longest = run;
    }
    return longest <= 2 ? vowels >= 1 : longest == 3 && vowels >= 2;
}
/* An all-capital run that is still a word (caller checked the run is all capitals).
   THERE IS NO BODY, Silent NO More, I AM THE BEST: a short capital word that is a common
   English word (caps_word) next to a capitalized word of three letters or more is that
   word, not letters; NO between lowercase words stays n o (2026-10-07; AM, THE 2026-10-29).
   OPEN, SOLD OUT, NEWS: a run of four letters or more that is a common English word (the
   electronic_word list) is that word, as signs and headlines write it; shorter runs stay
   letters (IT, CAT, WHO). A lexicon entry still wins (2026-10-28; was 오피이엔).
   FANCY, FEARLESS, KOSPI, UNESCO: a run of five letters or more shaped like a word is that
   word, as song titles, brands and headlines write it; UNHCR, NCSOFT stay letters
   (2026-10-29 owner request: English song titles; was 에프에이엔씨와이). */
static int caps_as_word(const ttf_pack_input_t *in, const char *key, size_t n, size_t consumed) {
    if (n <= 3 && res(in->engine, "caps_word", key)) {
        size_t k = in->index;
        while (k && in->tokens[k - 1].script == TTF_SCRIPT_WHITESPACE)
            --k;
        if (k && in->tokens[k - 1].script == TTF_SCRIPT_LATIN &&
            in->tokens[k - 1].end - in->tokens[k - 1].begin >= 3 &&
            in->text[in->tokens[k - 1].begin] >= 'A' && in->text[in->tokens[k - 1].begin] <= 'Z')
            return 1;
        k = in->index + consumed;
        while (k < in->token_count && in->tokens[k].script == TTF_SCRIPT_WHITESPACE)
            ++k;
        if (k < in->token_count && in->tokens[k].script == TTF_SCRIPT_LATIN &&
            in->tokens[k].end - in->tokens[k].begin >= 3 &&
            in->text[in->tokens[k].begin] >= 'A' && in->text[in->tokens[k].begin] <= 'Z')
            return 1;
    }
    if (consumed != 1)
        return 0;
    if (n >= 4 && res(in->engine, "electronic_word", key))
        return 1;
    return n >= 5 && word_shaped(key, n);
}
static int weight(const char *s, int fallback) {
    char *end;
    long n;
    if (!s)
        return fallback;
    n = strtol(s, &end, 10);
    return !*end && n >= 0 && n <= 1000 ? (int)n : fallback;
}
static ttf_status_t word(const ttf_engine_t *e, ttf_buffer_t *b, const char *kind,
                         const char *key) {
    const char *v = res(e, kind, key);
    if (!v)
        return TTF_ERESOURCE;
    return ttf_buffer_puts(b, v);
}
static ttf_status_t space(ttf_buffer_t *b) {
    return ttf_buffer_puts(b, " ");
}
static ttf_status_t card(const ttf_engine_t *e, uint64_t n, ttf_buffer_t *b) {
    static const uint64_t scales[] = {
        UINT64_C(1000000000000000000), UINT64_C(1000000000000000), UINT64_C(1000000000000),
        UINT64_C(1000000000),          UINT64_C(1000000),          UINT64_C(1000)};
    size_t i;
    char key[32];
    ttf_status_t st;
    for (i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i) {
        if (n >= scales[i]) {
            st = card(e, n / scales[i], b);
            if (st != TTF_OK)
                return st;
            if ((st = space(b)) != TTF_OK)
                return st;
            (void)snprintf(key, sizeof(key), "%llu", (unsigned long long)scales[i]);
            if ((st = word(e, b, "number", key)) != TTF_OK)
                return st;
            n %= scales[i];
            if (!n)
                return TTF_OK;
            if ((st = space(b)) != TTF_OK)
                return st;
            return card(e, n, b);
        }
    }
    if (n >= 100) {
        if ((st = card(e, n / 100, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if ((st = word(e, b, "number", "100")) != TTF_OK)
            return st;
        n %= 100;
        if (!n)
            return TTF_OK;
        if ((st = space(b)) != TTF_OK)
            return st;
    }
    if (n >= 20) {
        (void)snprintf(key, sizeof(key), "%u", (unsigned)(n / 10 * 10));
        if ((st = word(e, b, "number", key)) != TTF_OK)
            return st;
        n %= 10;
        if (!n)
            return TTF_OK;
        if ((st = space(b)) != TTF_OK)
            return st;
    }
    (void)snprintf(key, sizeof(key), "%u", (unsigned)n);
    return word(e, b, "number", key);
}
static ttf_status_t digits(const ttf_engine_t *e, const char *s, ttf_buffer_t *b) {
    size_t i;
    ttf_status_t st;
    int emitted = 0;
    for (i = 0; s[i]; ++i) {
        char key[2] = {s[i], '\0'};
        if (s[i] < '0' || s[i] > '9')
            continue;
        if (emitted && (st = space(b)) != TTF_OK)
            return st;
        if ((st = word(e, b, "number", key)) != TTF_OK)
            return st;
        emitted = 1;
    }
    return TTF_OK;
}
static int uint64_value(const char *text, uint64_t *value) {
    const char *p;
    *value = 0;
    if (!*text)
        return 0;
    for (p = text; *p; ++p) {
        unsigned digit_value = (unsigned)(*p - '0');
        if (digit_value > 9 || *value > (UINT64_MAX - digit_value) / 10)
            return 0;
        *value = *value * 10 + digit_value;
    }
    return 1;
}
static int singular(const char *text) {
    if (*text == '-' || *text == '+')
        ++text;
    while (*text == '0')
        ++text;
    if (*text++ != '1')
        return 0;
    if (*text == '.') {
        ++text;
        while (*text == '0')
            ++text;
    }
    return !*text;
}
static int thousand_or_larger(const char *text) {
    size_t integral = 0;
    if (*text == '-' || *text == '+')
        ++text;
    while (*text == '0')
        ++text;
    while (*text >= '0' && *text <= '9') {
        ++integral;
        ++text;
    }
    return integral >= 4;
}
/* A scaled currency keeps the exact expanded amount in the universal IR.
   Recover its written coefficient by moving the decimal point, never through
   floating point. Only named cardinal scales can be rendered this way. */
static ttf_status_t currency_coefficient(const ttf_semantic_t *s, char *coefficient,
                                         char *scale_key) {
    char digits_text[TTF_IR_VALUE_SIZE], raw[TTF_IR_VALUE_SIZE + 20];
    const char *p = s->secondary, *value = s->value;
    size_t n = 0, integral = 0, used = 0, i, start;
    unsigned exponent = 0;
    uint64_t scale = 1;
    int decimal = 0;
    char sign = '\0';
    if (!*p)
        return TTF_ERESOURCE;
    for (; *p; ++p) {
        if (*p < '0' || *p > '9' || exponent > 18)
            return TTF_ERESOURCE;
        exponent = exponent * 10 + (unsigned)(*p - '0');
    }
    if (exponent < 3 || exponent > 18 || exponent % 3)
        return TTF_ERESOURCE;
    if (*value == '-' || *value == '+')
        sign = *value++;
    for (p = value; *p; ++p) {
        if (*p == '.' && !decimal && n) {
            integral = n;
            decimal = 1;
        } else if (*p >= '0' && *p <= '9' && n < sizeof(digits_text) - 1) {
            digits_text[n++] = *p;
        } else
            return TTF_EINVAL;
    }
    if (!n || (decimal && integral == n))
        return TTF_EINVAL;
    if (!decimal)
        integral = n;
    if (integral <= exponent) {
        raw[used++] = '0';
        raw[used++] = '.';
        for (i = integral; i < exponent; ++i)
            raw[used++] = '0';
        memcpy(raw + used, digits_text, n);
        used += n;
    } else {
        size_t before = integral - exponent;
        memcpy(raw, digits_text, before);
        used = before;
        raw[used++] = '.';
        memcpy(raw + used, digits_text + before, n - before);
        used += n - before;
    }
    while (used && raw[used - 1] == '0')
        --used;
    if (used && raw[used - 1] == '.')
        --used;
    raw[used] = '\0';
    start = 0;
    while (raw[start] == '0' && raw[start + 1] && raw[start + 1] != '.')
        ++start;
    used = 0;
    if (sign)
        coefficient[used++] = sign;
    memcpy(coefficient + used, raw + start, strlen(raw + start) + 1);
    for (i = 0; i < exponent; ++i)
        scale *= 10;
    (void)snprintf(scale_key, 24, "%llu", (unsigned long long)scale);
    return TTF_OK;
}
static ttf_status_t numeric(const ttf_engine_t *e, const char *s, ttf_buffer_t *b) {
    char integral[TTF_IR_VALUE_SIZE];
    const char *dot;
    size_t len;
    uint64_t value;
    ttf_status_t st;
    if (*s == '-' || *s == '+') {
        if ((st = word(e, b, "number", *s == '-' ? "minus" : "plus")) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        ++s;
    }
    dot = strchr(s, '.');
    len = dot ? (size_t)(dot - s) : strlen(s);
    if (len >= sizeof(integral))
        return TTF_ELIMIT;
    memcpy(integral, s, len);
    integral[len] = '\0';
    if (uint64_value(integral, &value))
        st = card(e, value, b);
    else
        st = digits(e, integral, b);
    if (st != TTF_OK || !dot)
        return st;
    if ((st = space(b)) != TTF_OK)
        return st;
    if ((st = word(e, b, "number", "point")) != TTF_OK)
        return st;
    if ((st = space(b)) != TTF_OK)
        return st;
    return digits(e, dot + 1, b);
}
static ttf_status_t ordinal(const ttf_engine_t *e, uint64_t n, ttf_buffer_t *b) {
    char key[32];
    const char *v;
    ttf_status_t st;
    (void)snprintf(key, sizeof(key), "%llu", (unsigned long long)n);
    v = res(e, "ordinal", key);
    if (v)
        return ttf_buffer_puts(b, v);
    if (n < 20 || (n < 100 && n % 10 == 0))
        return TTF_ERESOURCE;
    if (n < 100) {
        if ((st = card(e, n / 10 * 10, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        return ordinal(e, n % 10, b);
    }
    if (n % 100) {
        if ((st = card(e, n - n % 100, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        return ordinal(e, n % 100, b);
    }
    if ((st = card(e, n, b)) != TTF_OK)
        return st;
    return word(e, b, "ordinal", "suffix");
}
/* Calendar readings are a language policy. The stored year remains the exact
   numeric value, and ISO input retains the existing cardinal default. */
static ttf_status_t date_year(const ttf_engine_t *e, unsigned year, const char *policy,
                              ttf_buffer_t *b) {
    const char *style = res(e, "date_policy", policy);
    ttf_status_t st;
    unsigned century, remainder;
    if (!style)
        style = !strcmp(policy, "numeric_year_style") ? "cardinal" : "calendar";
    /* cardinal-from-2000: 2026-09-16 two thousand twenty six as before, while
       1997-04-10 reads nineteen ninety seven, not one thousand nine hundred
       ninety seven (the English references read ISO years before 2000 so;
       2026-10-07). */
    if (!strcmp(style, "cardinal") || (!strcmp(style, "cardinal-from-2000") && year >= 2000))
        return card(e, year, b);
    if (!strcmp(style, "cardinal-from-2000"))
        style = "calendar";
    if (strcmp(style, "calendar"))
        return TTF_ERESOURCE;
    /* A three-digit year reads the same way (453 four fifty three, the English
       references; 2026-10-07). */
    if (year < 100 || (year >= 2000 && year <= 2009) || year % 1000 == 0)
        return card(e, year, b);
    century = year / 100;
    remainder = year % 100;
    if ((st = card(e, century, b)) != TTF_OK || (st = space(b)) != TTF_OK)
        return st;
    if (!remainder)
        return word(e, b, "number", "100");
    if (remainder < 10) {
        if ((st = word(e, b, "number", "oh")) != TTF_OK || (st = space(b)) != TTF_OK)
            return st;
    }
    return card(e, remainder, b);
}
/* A decade is the plural of the year reading: 1960s nineteen sixties, 1900s
   nineteen hundreds, 2000s two thousands, 80s eighties. The plural words are
   data because they are English morphology, not arithmetic. */
static ttf_status_t date_decade(const ttf_engine_t *e, unsigned value, ttf_buffer_t *b) {
    char key[16];
    ttf_status_t st;
    unsigned century, remainder;
    if (!value)
        value = 2000; /* '00s is the decade of 2000: two thousands */
    century = value / 100;
    remainder = value % 100;
    if (value < 100) {
        (void)snprintf(key, sizeof(key), "%u", value);
        return word(e, b, "decade_plural", key);
    }
    if (!remainder) {
        if (!(value % 1000)) {
            if ((st = card(e, value / 1000, b)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            return word(e, b, "decade_plural", "1000");
        }
        if ((st = card(e, century, b)) != TTF_OK || (st = space(b)) != TTF_OK)
            return st;
        return word(e, b, "decade_plural", "100");
    }
    if ((st = card(e, century, b)) != TTF_OK || (st = space(b)) != TTF_OK)
        return st;
    (void)snprintf(key, sizeof(key), "%u", remainder);
    return word(e, b, "decade_plural", key);
}

/* The plural of a number's last word: seven sevens, fifty fifties, six sixes. */
static ttf_status_t plural_last(ttf_buffer_t *b, size_t from) {
    size_t end = b->length;
    if (end <= from)
        return TTF_OK;
    if (b->data[end - 1] == 'y') {
        b->length = end - 1;
        b->data[b->length] = '\0';
        return ttf_buffer_puts(b, "ies");
    }
    return ttf_buffer_puts(b, b->data[end - 1] == 'x' ? "es" : "s");
}
static ttf_status_t gap(ttf_buffer_t *b) {
    return b->length && b->data[b->length - 1] != ' ' ? space(b) : TTF_OK;
}
static int ascii_digit_at(const char *s, size_t n, size_t i) {
    return i < n && s[i] >= '0' && s[i] <= '9';
}
static int ascii_letter_at(const char *s, size_t n, size_t i) {
    return i < n && ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z'));
}
/* A rejected numeric span read as written (the English counterpart of the Korean
   written fallback, project-owner decision 2026-10-07): runs of up to three digits
   are cardinals, longer runs and leading zeros are digits, a decimal point is
   "point", grouped thousands are one number, a plural s after a number makes its
   last word plural and a three-digit plural reads as a pair (787s seven eighty
   sevens, 690s six nineties); letters stay letters (97X ninety seven X, 6a six a)
   and a meridiem is read (12pm twelve p m). One hyphen between two numbers is a
   range (to); other marks keep their speech names. */
static ttf_status_t written(const ttf_engine_t *e, const char *s, size_t n, ttf_buffer_t *b) {
    size_t i = 0, hyphens = 0, k;
    ttf_status_t st = TTF_OK;
    for (k = 0; k < n; ++k)
        hyphens += s[k] == '-' && (!k || s[k - 1] != '-');
    while (i < n && st == TTF_OK) {
        if (ascii_digit_at(s, n, i)) {
            size_t j = i, d;
            uint64_t v = 0;
            while (ascii_digit_at(s, n, j) && j - i < 18)
                v = v * 10 + (uint64_t)(s[j++] - '0');
            d = j - i;
            /* 10/618,543: grouped thousands are one number. */
            while (d <= 3 && j + 3 < n + 1 && s[j] == ',' && ascii_digit_at(s, n, j + 1) &&
                   ascii_digit_at(s, n, j + 2) && ascii_digit_at(s, n, j + 3) &&
                   !ascii_digit_at(s, n, j + 4) && v < 1000000000000ULL) {
                v = v * 1000 + (uint64_t)((s[j + 1] - '0') * 100 + (s[j + 2] - '0') * 10 + (s[j + 3] - '0'));
                j += 4;
                d = 3;
            }
            if ((st = gap(b)) != TTF_OK)
                break;
            if (j < n && s[j] == 's' && !ascii_letter_at(s, n, j + 1) && s[i] != '0' && d <= 4) {
                size_t from;
                if (d == 4) {
                    st = date_decade(e, (unsigned)v, b);
                } else if (d == 3 && v % 100) {
                    if ((st = card(e, v / 100, b)) == TTF_OK && (st = space(b)) == TTF_OK) {
                        if (v % 100 < 10 && (st = word(e, b, "number", "oh")) == TTF_OK)
                            st = space(b);
                        from = b->length;
                        if (st == TTF_OK && (st = card(e, v % 100, b)) == TTF_OK)
                            st = plural_last(b, from);
                    }
                } else {
                    from = b->length;
                    if ((st = card(e, v, b)) == TTF_OK)
                        st = plural_last(b, from);
                }
                i = j + 1;
                continue;
            }
            /* 1/7th, 2/3rd: a fraction written with an ordinal (one seventh, two
               thirds). */
            if (d <= 3 && j + 3 < n + 1 && s[j] == '/' && ascii_digit_at(s, n, j + 1)) {
                size_t f = j + 1;
                uint64_t den = 0;
                while (ascii_digit_at(s, n, f) && f - j <= 3)
                    den = den * 10 + (uint64_t)(s[f++] - '0');
                if (den > 1 && f + 1 < n + 1 && f + 2 <= n && !ascii_letter_at(s, n, f + 2) &&
                    ((s[f] == 't' && s[f + 1] == 'h') || (s[f] == 'r' && s[f + 1] == 'd') ||
                     (s[f] == 'n' && s[f + 1] == 'd') || (s[f] == 's' && s[f + 1] == 't'))) {
                    size_t from;
                    if ((st = card(e, v, b)) == TTF_OK && (st = space(b)) == TTF_OK) {
                        from = b->length;
                        if ((st = ordinal(e, den, b)) == TTF_OK && v != 1)
                            st = plural_last(b, from);
                    }
                    i = f + 2;
                    continue;
                }
            }
            /* 1861--1865: a lone four-digit year reads as a year; with a letter
               attached (3771A) or longer it is a code read digit by digit. */
            if (d == 4 && s[i] != '0' && v >= 1100 && v <= 2099 && !ascii_letter_at(s, n, j) &&
                !(i && ascii_letter_at(s, n, i - 1)))
                st = date_year(e, (unsigned)v, "year_style", b);
            else if ((s[i] == '0' && d > 1) || d >= 4) {
                char run[24];
                memcpy(run, s + i, d);
                run[d] = '\0';
                st = digits(e, run, b);
            } else
                st = card(e, v, b);
            i = j;
            /* 99.9, 1.1.3: a point and the digits after it. */
            while (st == TTF_OK && i + 1 < n && s[i] == '.' && ascii_digit_at(s, n, i + 1)) {
                char run[24];
                size_t f = i + 1;
                while (ascii_digit_at(s, n, f) && f - i - 1 < sizeof(run) - 1)
                    ++f;
                memcpy(run, s + i + 1, f - i - 1);
                run[f - i - 1] = '\0';
                if ((st = space(b)) == TTF_OK && (st = word(e, b, "number", "point")) == TTF_OK &&
                    (st = space(b)) == TTF_OK)
                    st = digits(e, run, b);
                i = f;
            }
        } else if (ascii_letter_at(s, n, i)) {
            size_t j = i;
            char key[8];
            const char *meridiem;
            while (ascii_letter_at(s, n, j))
                ++j;
            if ((st = gap(b)) != TTF_OK)
                break;
            meridiem = NULL;
            if (j - i == 2) {
                key[0] = (char)(s[i] & ~0x20);
                key[1] = (char)(s[i + 1] & ~0x20);
                key[2] = '\0';
                meridiem = res(e, "meridiem", key);
            }
            st = meridiem ? ttf_buffer_puts(b, meridiem) : ttf_buffer_append(b, s + i, j - i);
            i = j;
        } else if (s[i] == ' ' || s[i] == '\t') {
            st = gap(b);
            ++i;
        } else if (s[i] == '-' && hyphens == 1 && i && i + 1 < n) {
            if ((st = gap(b)) == TTF_OK && (st = ttf_buffer_puts(b, "to")) == TTF_OK)
                st = space(b);
            while (i < n && s[i] == '-')
                ++i;
        } else if (s[i] == '-' || s[i] == ':' || s[i] == '_') {
            st = gap(b);
            ++i;
        } else {
            char key[2] = {s[i], '\0'};
            const char *name = (unsigned char)s[i] < 128 ? res(e, "speech_symbol", key) : NULL;
            if (name && strcmp(key, ".") && strcmp(key, ",")) {
                if ((st = gap(b)) == TTF_OK && (st = ttf_buffer_puts(b, name)) == TTF_OK)
                    st = space(b);
            } else
                st = ttf_buffer_append(b, s + i, 1);
            ++i;
        }
    }
    while (st == TTF_OK && b->length && b->data[b->length - 1] == ' ')
        b->data[--b->length] = '\0';
    return st;
}
/* Genesis 6:14-16: chapter six verses fourteen to sixteen (the references). */
static ttf_status_t scripture(const ttf_engine_t *e, const char *value, ttf_buffer_t *b) {
    unsigned chapter, first, last;
    ttf_status_t st;
    if (sscanf(value, "%u:%u-%u", &chapter, &first, &last) != 3)
        return TTF_EINVAL;
    if ((st = ttf_buffer_puts(b, "chapter ")) != TTF_OK || (st = card(e, chapter, b)) != TTF_OK ||
        (st = ttf_buffer_puts(b, " verses ")) != TTF_OK || (st = card(e, first, b)) != TTF_OK ||
        (st = ttf_buffer_puts(b, " to ")) != TTF_OK)
        return st;
    return card(e, last, b);
}

static ttf_status_t date(const ttf_engine_t *e, const ttf_semantic_t *s, ttf_buffer_t *b) {
    unsigned year = 0, month = 0, day = 0;
    int consumed = 0, fields, has_year = 1, has_month = 1, has_day = 1, short_year = 0;
    int bare_article = !strcmp(s->secondary, "dmyn");
    int day_first = !strcmp(s->secondary, "dmy") || bare_article, year_first = !strcmp(s->secondary, "ymd");
    char key[16];
    const char *policy = s->secondary[0] ? "named_year_style" : "numeric_year_style";
    ttf_status_t st;
    if (s->secondary[0] && strcmp(s->secondary, "mdy") && !day_first && !year_first)
        return TTF_EINVAL;
    if (!strcmp(s->unit, "decade")) {
        if (sscanf(s->value, "%u%n", &year, &consumed) != 1 || s->value[consumed])
            return TTF_EINVAL;
        return date_decade(e, year, b);
    }
    if (!strcmp(s->unit, "year")) {
        fields = sscanf(s->value, "%u%n", &year, &consumed);
        has_month = has_day = 0;
        if (fields != 1)
            return TTF_EINVAL;
        policy = "year_style";
    } else if (!strcmp(s->unit, "year-month")) {
        fields = sscanf(s->value, "%u-%u%n", &year, &month, &consumed);
        has_day = 0;
        if (fields != 2)
            return TTF_EINVAL;
    } else if (!strcmp(s->unit, "month-day")) {
        fields = sscanf(s->value, "--%u-%u%n", &month, &day, &consumed);
        has_year = 0;
        if (fields != 2)
            return TTF_EINVAL;
    } else if (!s->unit[0]) {
        if (sscanf(s->value, "%u-%u-%u%n", &year, &month, &day, &consumed) != 3)
            return TTF_EINVAL;
    } else if (!strcmp(s->unit, "short-year")) {
        /* 5/20/16 May twentieth twenty sixteen: a two-digit year reads in full, 00-49 in the
           2000s and 50-99 in the 1900s, so exp 09/31 is 2031 (project-owner decision 2026-10-22;
           Was: as written, sixteen, oh eight, oh oh). */
        if (sscanf(s->value, "%u-%u-%u%n", &year, &month, &day, &consumed) != 3 || year > 99)
            return TTF_EINVAL;
        year += year <= 49 ? 2000 : 1900;
    } else
        return TTF_EINVAL;
    if (s->value[consumed] || (has_year && !short_year && (!year || year > 9999)) ||
        (has_month && (!month || month > 12)) || (has_day && (!day || day > 31)))
        return TTF_EINVAL;
    if (!has_month)
        return date_year(e, year, policy, b);
    /* 2007 Apr 24: year, month and day in the written order. */
    if (year_first && has_year && has_day) {
        (void)snprintf(key, sizeof(key), "%u", month);
        if ((st = date_year(e, year, policy, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
            (st = word(e, b, "month", key)) != TTF_OK || (st = space(b)) != TTF_OK)
            return st;
        return ordinal(e, day, b);
    }
    if (day_first && has_day) {
        if ((!bare_article &&
             ((st = word(e, b, "date_word", "the")) != TTF_OK || (st = space(b)) != TTF_OK)) ||
            (st = ordinal(e, day, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
            (st = word(e, b, "date_word", "of")) != TTF_OK || (st = space(b)) != TTF_OK)
            return st;
    }
    (void)snprintf(key, sizeof(key), "%u", month);
    if ((st = word(e, b, "month", key)) != TTF_OK)
        return st;
    if (has_day && !day_first) {
        if ((st = space(b)) != TTF_OK || (st = ordinal(e, day, b)) != TTF_OK)
            return st;
    }
    if (has_year) {
        if ((st = space(b)) != TTF_OK)
            return st;
        if (short_year && year < 10) {
            if ((st = word(e, b, "number", "oh")) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            return year ? card(e, year, b) : word(e, b, "number", "oh");
        }
        if (short_year)
            return card(e, year, b);
        return date_year(e, year, policy, b);
    }
    return TTF_OK;
}
static int script_score(ttf_script_t s) {
    return s == TTF_SCRIPT_LATIN ? 10 : 0;
}
/* Units, scale markers and ordinal suffixes convey semantics, not independent
   English words. */
static int neutral_marker(const ttf_pack_input_t *in, size_t index, const char *lower) {
    const ttf_token_t *t = &in->tokens[index];
    size_t previous = index, start, length;
    char original[96];
    const char *dotted_neutral = res(in->engine, "context_neutral", "dotted_initials");
    /* Letter labels do not establish an English clause by themselves. Dotted
       initials inherit the configured/surrounding language, while ordinary
       words alongside them still contribute their independent evidence. */
    if (dotted_neutral && strcmp(dotted_neutral, "0") && t->end == t->begin + 1 &&
        in->text[t->begin] >= 'A' && in->text[t->begin] <= 'Z' && t->end < in->length &&
        in->text[t->end] == '.')
        return 1;
    /* A-B, x-y: joined single letters label something; they are not the
       English article or a word of an English clause. */
    if (t->end == t->begin + 1) {
        const char *joined_neutral = res(in->engine, "context_neutral", "joined_letters");
        int after =
            index + 2 < in->token_count && in->tokens[index + 1].begin == t->end &&
            in->tokens[index + 1].end == t->end + 1 &&
            (in->text[t->end] == '-' || in->text[t->end] == '&' || in->text[t->end] == '/') &&
            in->tokens[index + 2].begin == t->end + 1 &&
            in->tokens[index + 2].script == TTF_SCRIPT_LATIN &&
            in->tokens[index + 2].end == t->end + 2;
        int before = index >= 2 && in->tokens[index - 1].end == t->begin &&
                     in->tokens[index - 1].begin + 1 == t->begin &&
                     (in->text[t->begin - 1] == '-' || in->text[t->begin - 1] == '&' ||
                      in->text[t->begin - 1] == '/') &&
                     in->tokens[index - 2].script == TTF_SCRIPT_LATIN &&
                     in->tokens[index - 2].end + 1 == t->begin &&
                     in->tokens[index - 2].begin + 2 == t->begin;
        if (joined_neutral && !strcmp(joined_neutral, "1") && (after || before))
            return 1;
    }
    if (!strcmp(lower, "www") && t->end < in->length && in->text[t->end] == '.') {
        const char *web_neutral = res(in->engine, "context_neutral", "web_initialism");
        size_t after = t->end + 1;
        if (web_neutral && !strcmp(web_neutral, "1") &&
            (after == in->length || (unsigned char)in->text[after] <= 32 ||
             (unsigned char)in->text[after] >= 127))
            return 1;
    }
    if (in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
        const char *alnum_neutral = res(in->engine, "context_neutral", "alnum_label");
        size_t next = t->end;
        int after_digit = index && in->tokens[index - 1].end == t->begin &&
                          in->tokens[index - 1].script == TTF_SCRIPT_DIGIT;
        if (next + 1 < in->length && in->text[next] == '-')
            ++next;
        if (alnum_neutral && !strcmp(alnum_neutral, "1") &&
            (after_digit || (next < in->length && in->text[next] >= '0' && in->text[next] <= '9')))
            return 1;
    }
    /* The exponent marker inside scientific numeric syntax is not an English
       word. The universal classifier remains responsible for validation. */
    if (!strcmp(lower, "e") && index && in->tokens[index - 1].script == TTF_SCRIPT_DIGIT &&
        in->tokens[index - 1].end == t->begin) {
        size_t next = index + 1, end = t->end;
        if (next < in->token_count && in->tokens[next].begin == end &&
            in->tokens[next].end == end + 1 && (in->text[end] == '+' || in->text[end] == '-'))
            end = in->tokens[next++].end;
        if (next < in->token_count && in->tokens[next].begin == end &&
            in->tokens[next].script == TTF_SCRIPT_DIGIT)
            return 1;
    }
    while (previous && (in->tokens[previous - 1].script == TTF_SCRIPT_WHITESPACE ||
                        in->tokens[previous - 1].script == TTF_SCRIPT_SYMBOL))
        --previous;
    if (!previous || in->tokens[previous - 1].script != TTF_SCRIPT_DIGIT)
        return 0;
    if (in->tokens[previous - 1].end == t->begin &&
        ttf_resource_get(in->engine, "ordinal_suffix", "*", lower, NULL))
        return 1;
    start = in->tokens[previous - 1].end;
    while (start < t->begin && (in->text[start] == ' ' || in->text[start] == '\t'))
        ++start;
    length = t->end - start;
    if (length >= sizeof(original))
        return 0;
    memcpy(original, in->text + start, length);
    original[length] = '\0';
    return ttf_resource_get(in->engine, "numeric_suffix", "*", original, NULL) != NULL ||
           ttf_resource_get(in->engine, "currency_scale", "*", original, NULL) != NULL;
}
/* An explicit label selects the speech language of its immediately following
   lexical run. The labels and their weights are resource data, and a zero
   weight disables an inherited label. Sentence/line boundaries end the cue. */
static int context_cue_score(const ttf_pack_input_t *in, size_t begin) {
    const ttf_token_t *token;
    char key[96];
    size_t i, n;
    int colon = 0;
    while (begin) {
        token = &in->tokens[begin - 1];
        if (token->script == TTF_SCRIPT_WHITESPACE) {
            for (i = token->begin; i < token->end; ++i)
                if (in->text[i] == '\r' || in->text[i] == '\n')
                    return 0;
            --begin;
        } else if (!colon && token->end == token->begin + 1 && in->text[token->begin] == ':') {
            colon = 1;
            --begin;
        } else
            break;
    }
    if (!begin)
        return 0;
    token = &in->tokens[begin - 1];
    n = token->end - token->begin;
    if (n >= sizeof(key))
        return 0;
    memcpy(key, in->text + token->begin, n);
    key[n] = '\0';
    return weight(res(in->engine, "context_cue", key), 0);
}
/* Shared run evidence is index-independent. The source order of lexical roles
 * remains significant (for example an imperative following a subject). */
static int run_context_score(const ttf_pack_input_t *in, size_t begin, size_t end) {
    size_t i, words = 0;
    int total = 0, predicate = 0, imperative = 0, subject = 0, polite = 0, previous_caps = 0;
    int ceiling = weight(res(in->engine, "context", "phrase_ceiling"), 160);
    int latin = weight(res(in->engine, "context", "latin_weight"), 8);
    for (i = begin; i < end; ++i) {
        const ttf_token_t *t = &in->tokens[i];
        char key[96];
        const char *role;
        size_t n = t->end - t->begin, j;
        int addition;
        if (t->script != TTF_SCRIPT_LATIN || n >= sizeof(key))
            continue;
        for (j = 0; j < n; ++j)
            key[j] = ascii_lower(in->text[t->begin + j]);
        key[n] = '\0';
        if (neutral_marker(in, i, key))
            continue;
        ++words;
        {
            /* GTX A, SRT A: a capital letter after a capitalized initialism is a
               label, not the article a (2026-10-02). */
            int caps = 1, label;
            for (j = 0; j < n; ++j)
                caps &= !(in->text[t->begin + j] >= 'a' && in->text[t->begin + j] <= 'z');
            label = n == 1 && caps && previous_caps;
            previous_caps = caps;
            addition = latin + (label ? 0 : weight(res(in->engine, "context_word", key), 0));
        }
        /* Saturation avoids an arbitrarily long noun list defeating an attached
           particle, and prevents an integer overflow on a large input. */
        total = addition >= ceiling - total ? ceiling : total + addition;
        role = res(in->engine, "context_role", key);
        if (!role)
            continue;
        if (!strcmp(role, "finite"))
            predicate = 1;
        else if (!strcmp(role, "imperative")) {
            if (words == 1 || polite || subject)
                imperative = 1;
        } else if (!strcmp(role, "subject"))
            subject = 1;
        else if (!strcmp(role, "polite"))
            polite = 1;
    }
    /* A predicate with another word supplies a clause signal. Bare nouns and
       a conjunction alone do not. The finite/imperative vocabulary is data. */
    if ((predicate && words >= 2) ||
        (imperative && ((words >= 3) || ((polite || subject) && words >= 2))))
        total += weight(res(in->engine, "context", "clause_weight"), 500);
    /* An ordinal alone is a quantity title and keeps the surrounding speech
       language. At least one lexical word is required for a foreign title. */
    return total + (words ? context_cue_score(in, begin) : 0);
}

static int numeric_label_context_score(const ttf_pack_input_t *in) {
    const ttf_token_t *current = &in->tokens[in->index];
    size_t width = current->end - current->begin;
    int numeric_start =
        current->script == TTF_SCRIPT_DIGIT ||
        (width == 1 && (in->text[current->begin] == '+' || in->text[current->begin] == '-' ||
                        in->text[current->begin] == '('));
    if (numeric_start &&
        (ttf_phone_preceding_label(in, current->begin, "phone_label", EN, NULL, NULL) ||
         ttf_phone_preceding_label(in, current->begin, "identifier_label", EN, NULL, NULL)))
        return weight(res(in->engine, "context", "numeric_label_weight"), 24);
    return 0;
}

static const char context_memo_identity = 0;
static ttf_status_t build_context_scores(const ttf_pack_input_t *in, void **out) {
    ttf_pack_input_t cursor = *in;
    size_t i, last_begin = SIZE_MAX, last_end = SIZE_MAX;
    int *scores, last_score = 0;
    *out = NULL;
    if (in->token_count > SIZE_MAX / sizeof(*scores))
        return TTF_ELIMIT;
    scores = (int *)malloc(in->token_count * sizeof(*scores));
    if (!scores)
        return TTF_ENOMEM;
    for (i = 0; i < in->token_count; ++i) {
        size_t begin, end;
        if (in->tokens[i].script == TTF_SCRIPT_HANGUL) {
            scores[i] = 0;
            continue;
        }
        cursor.index = i;
        context_run_bounds(&cursor, &begin, &end);
        /* Leading/trailing whitespace can share one trimmed run. Each distinct
         * contiguous run is evaluated once, without changing its extent. */
        if (begin != last_begin || end != last_end) {
            last_score = run_context_score(&cursor, begin, end);
            last_begin = begin;
            last_end = end;
        }
        scores[i] = last_score;
    }
    *out = scores;
    return TTF_OK;
}
static void dispose_context_scores(void *scores) {
    free(scores);
}
static int context_score(const ttf_pack_input_t *in) {
    int *scores = NULL, run_score;
    size_t begin, end;
    if (in->index >= in->token_count || in->tokens[in->index].script == TTF_SCRIPT_HANGUL)
        return 0;
    if (ttf_context_memo_enabled(in))
        scores = (int *)ttf_context_memo_get(in, &context_memo_identity, build_context_scores,
                                             dispose_context_scores);
    if (scores)
        run_score = scores[in->index];
    else {
        /* Direct pack callbacks retain an uncached reference path. Memo errors
         * remain sticky on the request and are propagated by the core. */
        context_run_bounds(in, &begin, &end);
        run_score = run_context_score(in, begin, end);
    }
    return run_score + numeric_label_context_score(in);
}

static int upper_run(const ttf_pack_input_t *in, size_t index) {
    const ttf_token_t *t = &in->tokens[index];
    size_t i;
    if (t->script != TTF_SCRIPT_LATIN || t->end - t->begin > 4)
        return 0;
    for (i = t->begin; i < t->end; ++i)
        if (in->text[i] < 'A' || in->text[i] > 'Z')
            return 0;
    return 1;
}

/* R&D, AT&T: short capital runs joined by an unspaced ampersand spell their
   letters. Lowercase words (rock&roll) and longer runs keep word grammar. */
static size_t ampersand_initialism(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    size_t index = in->index, end;
    if (!upper_run(in, index) ||
        (in->tokens[index].begin && ((in->text[in->tokens[index].begin - 1] >= 'a' &&
                                      in->text[in->tokens[index].begin - 1] <= 'z') ||
                                     (in->text[in->tokens[index].begin - 1] >= '0' &&
                                      in->text[in->tokens[index].begin - 1] <= '9') ||
                                     in->text[in->tokens[index].begin - 1] == '&')))
        return 0;
    while (index + 2 < in->token_count) {
        const ttf_token_t *amp = &in->tokens[index + 1], *next = &in->tokens[index + 2];
        if (amp->begin != in->tokens[index].end || amp->end != amp->begin + 1 ||
            in->text[amp->begin] != '&' || next->begin != amp->end || !upper_run(in, index + 2))
            break;
        index += 2;
    }
    end = in->tokens[index].end;
    if (index == in->index || end - in->tokens[in->index].begin >= sizeof(out->value) ||
        (end < in->length && ((in->text[end] >= '0' && in->text[end] <= '9') ||
                              in->text[end] == '&' || in->text[end] == '_')))
        return 0;
    memcpy(out->value, in->text + in->tokens[in->index].begin, end - in->tokens[in->index].begin);
    out->value[end - in->tokens[in->index].begin] = '\0';
    out->type = TTF_SEM_INITIALISM;
    out->lexical_class = TTF_LEX_INITIALISM;
    ttf_en_initialism_policy(in->engine, out, out->value, strlen(out->value), "inferred");
    out->input_end = end;
    (void)snprintf(out->rule, sizeof(out->rule), "pack.ampersand-initialism");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    return index - in->index + 1;
}

static int single_letter(const ttf_pack_input_t *in, size_t index) {
    const ttf_token_t *t = &in->tokens[index];
    char c = in->text[t->begin];
    return t->script == TTF_SCRIPT_LATIN && t->end == t->begin + 1 &&
           ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}
static int joiner_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
           c == '&' || c == '_';
}
/* x-y, A-B: single letters joined by hyphens spell one letter sequence. */
static size_t hyphen_letters(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    size_t index = in->index, used = 0, end;
    if (!single_letter(in, index) ||
        (in->tokens[index].begin && joiner_char(in->text[in->tokens[index].begin - 1])))
        return 0;
    out->value[used++] = (char)toupper((unsigned char)in->text[in->tokens[index].begin]);
    while (index + 2 < in->token_count && used + 1 < sizeof(out->value)) {
        const ttf_token_t *dash = &in->tokens[index + 1];
        if (dash->begin != in->tokens[index].end || dash->end != dash->begin + 1 ||
            (in->text[dash->begin] != '-' && in->text[dash->begin] != '/') ||
            in->tokens[index + 2].begin != dash->end || !single_letter(in, index + 2))
            break;
        index += 2;
        out->value[used++] = (char)toupper((unsigned char)in->text[in->tokens[index].begin]);
    }
    end = in->tokens[index].end;
    if (index == in->index || (end < in->length && joiner_char(in->text[end]))) {
        out->value[0] = '\0';
        return 0;
    }
    out->value[used] = '\0';
    out->type = TTF_SEM_INITIALISM;
    out->lexical_class = TTF_LEX_INITIALISM;
    ttf_en_initialism_policy(in->engine, out, out->value, used, "inferred");
    out->input_end = end;
    (void)snprintf(out->rule, sizeof(out->rule), "pack.hyphen-letters");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    return index - in->index + 1;
}

/* An uppercase Roman numeral named by the word before it: a numbering word
   (CHAPTER IX, World War II) makes it a cardinal, a regnal name (Henry II,
   Pope Benedict XVI) an ordinal read with "the". The cue words are data
   (roman_cue). Initials such as Louis C, the pronoun I and bare letters
   without a cue are left alone. */
static int alpha_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
static unsigned roman_value(const char *s, size_t n) {
    static const char *const thousands[] = {"", "M", "MM", "MMM"};
    static const char *const hundreds[] = {"", "C", "CC", "CCC", "CD", "D", "DC", "DCC", "DCCC", "CM"};
    static const char *const tens[] = {"", "X", "XX", "XXX", "XL", "L", "LX", "LXX", "LXXX", "XC"};
    static const char *const ones[] = {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};
    unsigned value = 0, i;
    char canonical[20];
    for (i = 0; i < n; ++i) {
        unsigned digit;
        switch (s[i]) {
        case 'I': digit = 1; break;
        case 'V': digit = 5; break;
        case 'X': digit = 10; break;
        case 'L': digit = 50; break;
        case 'C': digit = 100; break;
        case 'D': digit = 500; break;
        case 'M': digit = 1000; break;
        default: return 0;
        }
        if (i + 1 < n) {
            unsigned next = 0;
            switch (s[i + 1]) {
            case 'I': next = 1; break;
            case 'V': next = 5; break;
            case 'X': next = 10; break;
            case 'L': next = 50; break;
            case 'C': next = 100; break;
            case 'D': next = 500; break;
            case 'M': next = 1000; break;
            default: return 0;
            }
            if (digit < next) {
                value -= digit;
                continue;
            }
        }
        value += digit;
    }
    if (!value || value > 3999)
        return 0;
    /* Only the canonical spelling is a numeral (IIX, VV are not). */
    (void)snprintf(canonical, sizeof(canonical), "%s%s%s%s", thousands[value / 1000],
                   hundreds[value / 100 % 10], tens[value / 10 % 10], ones[value % 10]);
    return strlen(canonical) == n && !memcmp(canonical, s, n) ? value : 0;
}
/* III, VII, XIII: two or more of I, V and X, a whole word, read as a cardinal
   without a cue (Prime BEEF III, from VI to VIII, a heading XIII; the English
   references 250 : 4 for "the third"). A lone I, V or X needs a cue. */
static int roman_standalone(const ttf_pack_input_t *in, const ttf_token_t *t) {
    size_t p, n = t->end - t->begin;
    if (n < 2)
        return 0;
    for (p = t->begin; p < t->end; ++p)
        if (in->text[p] != 'I' && in->text[p] != 'V' && in->text[p] != 'X')
            return 0;
    if (t->begin && (alpha_char(in->text[t->begin - 1]) || in->text[t->begin - 1] == '-'))
        return 0;
    return !(t->end < in->length && (alpha_char(in->text[t->end]) || in->text[t->end] == '-' ||
                                     (in->text[t->end] >= '0' && in->text[t->end] <= '9')));
}

static size_t roman_numeral(const ttf_pack_input_t *in, ttf_semantic_t *out, int cardinal_only) {
    const ttf_token_t *t = &in->tokens[in->index], *space_token, *word;
    size_t n = t->end - t->begin, i, p;
    char key[32];
    const char *cue = NULL;
    unsigned value;
    int capitalized = 1, ok = 1;
    if (t->script != TTF_SCRIPT_LATIN || n > 15)
        return 0;
    value = roman_value(in->text + t->begin, n);
    if (!value)
        return 0;
    if (in->index < 2)
        ok = 0;
    else {
        space_token = &in->tokens[in->index - 1];
        word = &in->tokens[in->index - 2];
        if (space_token->script != TTF_SCRIPT_WHITESPACE || word->script != TTF_SCRIPT_LATIN ||
            word->end != space_token->begin || word->end - word->begin >= sizeof(key))
            ok = 0;
        for (p = space_token->begin; ok && p < space_token->end; ++p)
            if (in->text[p] != ' ')
                ok = 0;
        for (i = 0, p = word->begin; ok && p < word->end; ++p, ++i) {
            char c = in->text[p];
            if (c >= 'A' && c <= 'Z') {
                if (i)
                    capitalized = 0;
                c = (char)(c + 32);
            } else if (c < 'a' || c > 'z')
                ok = 0;
            else if (!i)
                capitalized = 0;
            key[i] = c;
        }
        if (ok) {
            key[i] = '\0';
            cue = res(in->engine, "roman_cue", key);
        }
    }
    if (cardinal_only && (!cue || strcmp(cue, "cardinal")))
        return 0;
    if (!cue) {
        if (!roman_standalone(in, t))
            return 0;
        /* Measured: reading it as regnal after any capitalized name (Astor IV)
           lost more references than it gained (90.83% to 90.71%), so names
           need a regnal cue (roman_cue). */
        cue = "cardinal";
    }
    if (!strcmp(cue, "regnal")) {
        /* A regnal numeral is written with I, V and X after a capitalized
           name; a lone I is the pronoun unless punctuation or "of" follows. */
        for (p = t->begin; p < t->end; ++p)
            if (in->text[p] != 'I' && in->text[p] != 'V' && in->text[p] != 'X')
                return 0;
        if (!capitalized)
            return 0;
        if (n == 1 && in->text[t->begin] == 'I') {
            size_t q = t->end;
            if (q < in->length && in->text[q] == ' ' &&
                !(q + 3 <= in->length && !memcmp(in->text + q, " of", 3) &&
                  (q + 3 == in->length || in->text[q + 3] == ' ')))
                return 0;
        }
        out->type = TTF_SEM_ORDINAL;
        (void)snprintf(out->secondary, sizeof(out->secondary), "regnal");
        (void)snprintf(out->rule, sizeof(out->rule), "en.roman.regnal");
    } else if (!strcmp(cue, "cardinal")) {
        out->type = TTF_SEM_CARDINAL;
        (void)snprintf(out->rule, sizeof(out->rule), "en.roman.cardinal");
    } else
        return 0;
    (void)snprintf(out->value, sizeof(out->value), "%u", value);
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    out->input_end = t->end;
    return 1;
}
/* 2 1/2, 1-1/2, 1 -1/2: a whole number, a space or hyphen, and a proper fraction
   with a denominator up to 16 is one mixed number (two and one half) in English
   text; Hangul, Han or Kana in the 64 bytes before keeps the parts apart. */
static size_t mixed_fraction(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *s = in->text;
    size_t n = in->length, begin = in->tokens[in->index].begin, p = begin, whole_end, num_begin, num_end,
           den_begin, den_end, i, from = begin > 64 ? begin - 64 : 0;
    unsigned long num, den;
    if (begin && (isdigit((unsigned char)s[begin - 1]) || s[begin - 1] == '/' || s[begin - 1] == '.' ||
                  s[begin - 1] == ',' || s[begin - 1] == '-'))
        return 0;
    for (i = from; i < begin; ++i)
        if ((unsigned char)s[i] >= 0xE3 && (unsigned char)s[i] <= 0xED)
            return 0;
    while (p < n && isdigit((unsigned char)s[p]))
        ++p;
    if (p == begin || p - begin > 3)
        return 0;
    whole_end = p;
    if (p < n && s[p] == ' ')
        ++p;
    if (p < n && s[p] == '-')
        ++p;
    if (p == whole_end || p >= n || !isdigit((unsigned char)s[p]))
        return 0;
    num_begin = p;
    while (p < n && isdigit((unsigned char)s[p]))
        ++p;
    num_end = p;
    if (p >= n || s[p] != '/')
        return 0;
    den_begin = ++p;
    while (p < n && isdigit((unsigned char)s[p]))
        ++p;
    den_end = p;
    if (den_end == den_begin || num_end - num_begin > 2 || den_end - den_begin > 2 ||
        (p < n && (isalnum((unsigned char)s[p]) || s[p] == '/' || s[p] == '.' || s[p] == '-')))
        return 0;
    num = strtoul(s + num_begin, NULL, 10);
    den = strtoul(s + den_begin, NULL, 10);
    if (!num || num >= den || den < 2 || den > 16)
        return 0;
    /* English only: no Hangul, Han or Kana within 16 bytes after either (2 1/2개),
       and an English default or a Latin word in the window before. */
    for (i = den_end; i < n && i < den_end + 16; ++i)
        if ((unsigned char)s[i] >= 0xE3 && (unsigned char)s[i] <= 0xED)
            return 0;
    {
        const char *lang = in->engine->config.default_language;
        int latin = 0;
        for (i = from; i + 1 < begin; ++i)
            latin |= isalpha((unsigned char)s[i]) && isalpha((unsigned char)s[i + 1]);
        if (!latin && !(lang && !strncmp(lang, "en", 2)))
            return 0;
    }
    (void)snprintf(out->unit, sizeof(out->unit), "mixed:%.*s", (int)(whole_end - begin), s + begin);
    (void)snprintf(out->value, sizeof(out->value), "%lu", num);
    (void)snprintf(out->secondary, sizeof(out->secondary), "%lu", den);
    out->type = TTF_SEM_FRACTION;
    (void)snprintf(out->rule, sizeof(out->rule), "en.mixed-fraction");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    out->input_end = den_end;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < den_end; ++i) {
    }
    return i - in->index;
}

/* Micah 6:6-8 after a book name (scripture_book): chapter six verses six to eight. A
   verse never starts with 0, so a clock range (9:00-17:00) is not one. */
static size_t scripture_verse(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *s = in->text;
    size_t n = in->length, start = in->tokens[in->index].begin, p = start, w, q, i;
    unsigned field[3] = {0, 0, 0};
    char key[24];
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    /* a book name comes first: a digit at the start is no verse (2026-10-26: p 0 read two
       bytes before the text) */
    if (!ascii_digit_at(s, n, p) || !p || s[p - 1] != ' ')
        return 0;
    w = p - 1;
    while (w && s[w - 1] == ' ')
        --w;
    q = w;
    while (q && ascii_letter_at(s, n, q - 1))
        --q;
    if (q == w || w - q >= sizeof(key))
        return 0;
    for (i = 0; i < w - q; ++i)
        key[i] = (char)(s[q + i] | 0x20);
    key[w - q] = '\0';
    if (!ttf_resource_get(in->engine, "scripture_book", EN, key, &layer))
        return 0;
    for (i = 0; i < 3; ++i) {
        size_t d = p;
        if (i == 1) {
            if (p >= n || s[p] != ':')
                return 0;
            d = ++p;
        } else if (i == 2) {
            while (p < n && s[p] == ' ')
                ++p;
            if (p >= n || s[p] != '-')
                return 0;
            ++p;
            while (p < n && s[p] == ' ')
                ++p;
            d = p;
        }
        if (i && s[d] == '0')
            return 0;
        while (ascii_digit_at(s, n, p) && p - d < 3)
            field[i] = field[i] * 10 + (unsigned)(s[p++] - '0');
        if (p == d || ascii_digit_at(s, n, p))
            return 0;
    }
    if (ascii_letter_at(s, n, p) || (p < n && (s[p] == ':' || s[p] == '-')) || field[2] <= field[1])
        return 0;
    (void)snprintf(out->value, sizeof(out->value), "%u:%u-%u", field[0], field[1], field[2]);
    (void)snprintf(out->unit, sizeof(out->unit), "scripture");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    (void)snprintf(out->rule, sizeof(out->rule), "en.scripture-verse");
    out->type = TTF_SEM_DIGIT_SEQUENCE;
    out->input_end = p;
    out->layer = layer;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < p; ++i) {
    }
    return i - in->index;
}

/* 1/2 oz. one half ounce: a unit word after a proper fraction (2026-10-07). */
static size_t fraction_unit(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *s = in->text, *measure;
    size_t n = in->length, begin = in->tokens[in->index].begin, p = begin, num_end, den_begin, u, i;
    unsigned long num, den;
    char key[16], name[24];
    if (begin && (isdigit((unsigned char)s[begin - 1]) || s[begin - 1] == '/' || s[begin - 1] == '.'))
        return 0;
    while (p < n && isdigit((unsigned char)s[p]) && p - begin < 2)
        ++p;
    num_end = p;
    if (p == begin || p >= n || s[p] != '/')
        return 0;
    den_begin = ++p;
    while (p < n && isdigit((unsigned char)s[p]) && p - den_begin < 2)
        ++p;
    if (p == den_begin || p + 1 >= n || s[p] != ' ' || !isalpha((unsigned char)s[p + 1]))
        return 0;
    num = strtoul(s + begin, NULL, 10);
    den = strtoul(s + den_begin, NULL, 10);
    if (!num || num >= den || den < 2 || den > 16 || num_end == begin)
        return 0;
    u = p + 1;
    while (u < n && isalpha((unsigned char)s[u]) && u - p - 1 < sizeof(key) - 1)
        ++u;
    if (u < n && isalnum((unsigned char)s[u]))
        return 0;
    memcpy(key, s + p + 1, u - p - 1);
    key[u - p - 1] = '\0';
    measure = ttf_resource_get(in->engine, "numeric_suffix", "*", key, NULL);
    if (!measure || strncmp(measure, "MEASURE:", 8) || strlen(measure + 8) + 6 >= sizeof(name))
        return 0;
    (void)snprintf(name, sizeof(name), "%s.one", measure + 8);
    if (!ttf_resource_get(in->engine, "unit", EN, name, NULL))
        return 0;
    for (i = begin > 64 ? begin - 64 : 0; i < begin; ++i)
        if ((unsigned char)s[i] >= 0xE3 && (unsigned char)s[i] <= 0xED)
            return 0;
    (void)snprintf(out->unit, sizeof(out->unit), "unit:%s", measure + 8);
    (void)snprintf(out->value, sizeof(out->value), "%lu", num);
    (void)snprintf(out->secondary, sizeof(out->secondary), "%lu", den);
    out->type = TTF_SEM_FRACTION;
    (void)snprintf(out->rule, sizeof(out->rule), "en.fraction-unit");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    out->input_end = u;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < u; ++i) {
    }
    return i - in->index;
}

/* As in mixed_fraction: an English-only window, no Hangul, Han or Kana within
   64 bytes before or 16 after. */
static int english_window(const ttf_pack_input_t *in, size_t begin, size_t end) {
    size_t i;
    for (i = begin > 64 ? begin - 64 : 0; i < in->length && i < end + 16; ++i)
        if ((unsigned char)in->text[i] >= 0xE3 && (unsigned char)in->text[i] <= 0xED)
            return 0;
    return 1;
}
static size_t tokens_until(const ttf_pack_input_t *in, size_t end) {
    size_t i;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < end; ++i) {
    }
    return i - in->index;
}

/* 1M requests, 5K items: a K/M/B count before a plural noun reads its scale
   word (count_scale, 2026-10-13). 3M tape (a singular noun, a brand) keeps
   its letters. */
static size_t count_scale(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *s = in->text, *scale;
    size_t n = in->length, begin = in->tokens[in->index].begin, p = begin, w, end;
    char key[2];
    if (!isdigit((unsigned char)s[begin]) ||
        (begin && (isalnum((unsigned char)s[begin - 1]) || strchr(".,$/-+", s[begin - 1]))))
        return 0;
    while (p < n && isdigit((unsigned char)s[p]))
        ++p;
    if (p - begin > 3)
        return 0;
    if (p + 1 < n && s[p] == '.' && isdigit((unsigned char)s[p + 1]))
        for (++p; p < n && isdigit((unsigned char)s[p]);)
            ++p;
    if (p >= n || p - begin >= sizeof(out->value))
        return 0;
    key[0] = s[p];
    key[1] = '\0';
    if (!(scale = res(in->engine, "count_scale", key)))
        return 0;
    end = p + 1;
    if (end + 1 >= n || s[end] != ' ')
        return 0;
    for (w = end + 1; w < n && s[w] >= 'a' && s[w] <= 'z';)
        ++w;
    if (w - end - 1 < 3 || (w < n && (isalnum((unsigned char)s[w]) || s[w] == '-' || s[w] == '\'')) ||
        !(s[w - 1] == 's' || (w - end - 1 == 6 && !memcmp(s + end + 1, "people", 6))) ||
        !english_window(in, begin, end))
        return 0;
    memcpy(out->value, s + begin, p - begin);
    out->value[p - begin] = '\0';
    (void)snprintf(out->secondary, sizeof(out->secondary), "%s", scale);
    (void)snprintf(out->unit, sizeof(out->unit), "@scaled-count");
    (void)snprintf(out->rule, sizeof(out->rule), "en.count-scale");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    out->type = TTF_SEM_CUSTOM;
    out->input_end = end;
    return tokens_until(in, end);
}

/* requests/day: a word over a time word reads per (per_word, 2026-10-13);
   input/output keeps its slash. */
static size_t per_slash(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *s = in->text;
    size_t at = in->tokens[in->index].begin, n = in->length, w, k = 0;
    char key[16];
    size_t from = at + 1;
    if (s[at] != '/' || in->tokens[in->index].end != at + 1 || !at)
        return 0;
    /* 10/Pack, cm./each, 30 pins / pack (owner v2, 2026-10-15) */
    if (s[at - 1] == ' ' && at >= 2 && isalpha((unsigned char)s[at - 2]) && at + 1 < n && s[at + 1] == ' ')
        from = at + 2;
    else if (!isalpha((unsigned char)s[at - 1]) && !isdigit((unsigned char)s[at - 1]) && s[at - 1] != '.')
        return 0;
    for (w = from; w < n && isalpha((unsigned char)s[w]) && k + 1 < sizeof(key); ++w)
        key[k++] = (char)tolower((unsigned char)s[w]);
    key[k] = '\0';
    if (!k || (w < n && (isalnum((unsigned char)s[w]) || s[w] == '/' || s[w] == '-')) ||
        !res(in->engine, "per_word", key) || !english_window(in, at, w))
        return 0;
    (void)snprintf(out->value, sizeof(out->value), "%s", key);
    (void)snprintf(out->unit, sizeof(out->unit), "@per");
    (void)snprintf(out->rule, sizeof(out->rule), "en.per-slash");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    out->type = TTF_SEM_CUSTOM;
    out->input_end = w;
    return tokens_until(in, w);
}

/* 24/7: an idiom, not a fraction (fraction_idiom, 2026-10-13), except after a
   word such as "ratio of" (fraction_idiom_not), where the fraction stays. */
static size_t fraction_idiom(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *s = in->text;
    size_t n = in->length, begin = in->tokens[in->index].begin, p = begin, slash, end, w, last, k;
    char key[16], previous[16];
    if (!isdigit((unsigned char)s[begin]) ||
        (begin && (isalnum((unsigned char)s[begin - 1]) || strchr("/.,-", s[begin - 1]))))
        return 0;
    while (p < n && isdigit((unsigned char)s[p]) && p - begin < 4)
        ++p;
    if (p >= n || s[p] != '/')
        return 0;
    slash = p++;
    while (p < n && isdigit((unsigned char)s[p]) && p - slash < 5)
        ++p;
    end = p;
    if (end == slash + 1 || end - begin >= sizeof(key) ||
        (end < n && (isalnum((unsigned char)s[end]) || s[end] == '/' ||
                     (s[end] == '.' && end + 1 < n && isdigit((unsigned char)s[end + 1])))))
        return 0;
    memcpy(key, s + begin, end - begin);
    key[end - begin] = '\0';
    if (!res(in->engine, "fraction_idiom", key) || !english_window(in, begin, end))
        return 0;
    for (w = begin; w && s[w - 1] == ' ';)
        --w;
    for (last = w; w && isalpha((unsigned char)s[w - 1]) && last - w < sizeof(previous) - 1;)
        --w;
    for (k = 0; w + k < last; ++k)
        previous[k] = (char)tolower((unsigned char)s[w + k]);
    previous[k] = '\0';
    if (k && res(in->engine, "fraction_idiom_not", previous))
        return 0;
    memcpy(out->value, s + begin, slash - begin);
    out->value[slash - begin] = '\0';
    memcpy(out->secondary, s + slash + 1, end - slash - 1);
    out->secondary[end - slash - 1] = '\0';
    (void)snprintf(out->unit, sizeof(out->unit), "idiom");
    (void)snprintf(out->rule, sizeof(out->rule), "en.fraction-idiom");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    out->type = TTF_SEM_FRACTION;
    out->input_end = end;
    return tokens_until(in, end);
}

/* ---- owner evaluation sets, phase 3 (2026-10-14) ---- */
static size_t owner_finish(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t end,
                           ttf_semantic_type_t type, const char *unit, const char *value, const char *rule) {
    size_t i;
    if (strlen(value) >= sizeof(out->value))
        return 0;
    out->type = type;
    out->input_end = end;
    out->layer = TTF_LAYER_COMMON;
    (void)snprintf(out->value, sizeof(out->value), "%s", value);
    (void)snprintf(out->unit, sizeof(out->unit), "%s", unit);
    (void)snprintf(out->rule, sizeof(out->rule), "%s", rule);
    for (i = in->index; i < in->token_count && in->tokens[i].begin < end; ++i)
        ;
    return i - in->index;
}
static int ascii_digit_char(char c) {
    return c >= '0' && c <= '9';
}
static int ascii_letter_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
/* The lowercase word that ends at p (spaces skipped) is one of the |-separated words. */
static int word_before(const char *t, size_t p, const char *words) {
    size_t q = p, w;
    char key[24];
    const char *s;
    while (q && t[q - 1] == ' ')
        --q;
    for (w = q; w && ascii_letter_char(t[w - 1]); --w)
        ;
    if (w == q || q - w >= sizeof(key))
        return 0;
    for (size_t k = 0; k < q - w; ++k)
        key[k] = (char)(t[w + k] >= 'A' && t[w + k] <= 'Z' ? t[w + k] - 'A' + 'a' : t[w + k]);
    key[q - w] = 0;
    for (s = words; *s;) {
        size_t len = strcspn(s, "|");
        if (len == q - w && !strncmp(s, key, len))
            return 1;
        s += len + (s[len] == '|');
    }
    return 0;
}
static int strstr_before(const char *t, size_t p, const char *phrase) {
    size_t q = p, k = strlen(phrase);
    while (q && t[q - 1] == ' ')
        --q;
    return q >= k && !strncmp(t + q - k, phrase, k);
}
/* A Hangul, kana or hanzi lead byte (U+3000..U+DFFF), or a Vietnamese letter
   (ă đ ơ ư, U+1EA0..U+1EF9) at q. */
static int foreign_letter(const char *t, size_t n, size_t q) {
    unsigned char c = (unsigned char)t[q], d = q + 1 < n ? (unsigned char)t[q + 1] : 0;
    return (c >= 0xe3 && c <= 0xed) || (c == 0xe1 && (d == 0xba || d == 0xbb)) || c == 0xc4 || c == 0xc6;
}
static size_t owner_forms(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q, r;
    char v[48];
    if (p >= n || (p && (ascii_digit_char(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == '$')))
        return 0;
    /* English lines only: no Hangul, kana, hanzi or Vietnamese letters on the line
       (an en dash or a curly quote is fine) */
    for (q = p; q && t[q - 1] != '\n'; --q)
        if (foreign_letter(t, n, q - 1))
            return 0;
    for (q = p; q < n && t[q] != '\n'; ++q)
        if (foreign_letter(t, n, q))
            return 0;
    /* ---- owner v2 forms (real sentences, 2026-10-15) ---- */
    {
        static const char *const month[] = {"January", "February", "March", "April", "May", "June", "July",
                                            "August", "September", "October", "November", "December"};
        char w[96];
        size_t a, k;
        /* 8-3/8"Dia. eight and three eighths inch diameter, 36"L thirty six inch long, 3/8" three
           eighths inch, 8"x 7" eight inch by seven inch */
        if (ascii_digit_char(t[p])) {
            size_t num_end;
            for (a = p; a < n && ascii_digit_char(t[a]); ++a)
                ;
            num_end = a;
            if (a + 3 < n && (t[a] == '-' || t[a] == ' ') && ascii_digit_char(t[a + 1])) {
                size_t b2 = a + 1;
                while (b2 < n && ascii_digit_char(t[b2]))
                    ++b2;
                if (b2 + 1 < n && t[b2] == '/' && ascii_digit_char(t[b2 + 1])) {
                    for (++b2; b2 < n && ascii_digit_char(t[b2]); ++b2)
                        ;
                    num_end = b2;
                }
            } else if (a + 1 < n && (t[a] == '/' || t[a] == '.') && ascii_digit_char(t[a + 1])) {
                for (++a; a < n && ascii_digit_char(t[a]); ++a)
                    ;
                num_end = a;
            }
            if (num_end < n && t[num_end] == '"' && num_end - p < 16) {
                size_t z = num_end + 1, quotes = 0, end;
                const char *suffix = "";
                int attached = z < n && ascii_letter_char(t[z]), by = 0, inch_mark = 1;
                if (!attached && !memchr(t + p, '/', num_end - p)) {
                    /* "I have 5" and left: an odd quote before it (not an inch mark) closes here */
                    for (k = p; k && t[k - 1] != '\n'; --k)
                        quotes += t[k - 1] == '"' && !(k >= 2 && ascii_digit_char(t[k - 2]));
                    inch_mark = !(quotes % 2) && (z >= n || t[z] == ' ' || t[z] == ',' || t[z] == ')' ||
                                                   t[z] == '.' || t[z] == ';');
                }
                if (inch_mark) {
                    if (!strncmp(t + z, "Dia.", 4) || !strncmp(t + z, "dia.", 4))
                        suffix = "diameter", z += 4;
                    else if (!strncmp(t + z, "D.", 2))
                        suffix = "diameter", z += 2;
                    else if (!strncmp(t + z, " diam.", 6) || !strncmp(t + z, " dia.", 5))
                        suffix = "diameter", z += t[z + 4] == '.' ? 5 : 6;
                    else if (attached && (t[z] == 'L' || t[z] == 'W' || t[z] == 'H') &&
                             !(z + 1 < n && ascii_letter_char(t[z + 1])))
                        suffix = t[z] == 'L' ? "long" : t[z] == 'W' ? "wide" : "high", ++z;
                    end = z;
                    a = z < n && t[z] == ' ' ? z + 1 : z;
                    if (a < n && (t[a] == 'x' || t[a] == 'X') && a + 1 < n &&
                        (ascii_digit_char(t[a + 1]) || (t[a + 1] == ' ' && a + 2 < n && ascii_digit_char(t[a + 2])))) {
                        by = 1;
                        end = a + 1;
                    }
                    if (!(attached && !*suffix && !by)) {
                        (void)snprintf(w, sizeof(w), "%.*s|%s|%d", (int)(num_end - p), t + p, suffix, by);
                        return owner_finish(in, out, end, TTF_SEM_SYMBOL, "@inch-mark", w, "en.inch-mark");
                    }
                }
            }
            /* 97.6x 57.1x 112.8 cm: an x with a space on one side between numbers (the last term
               keeps its unit reading) */
            {
                size_t r = p, last = 0, used = 0;
                int odd = 0, terms = 0;
                for (;;) {
                    size_t b0 = r, xa, xb;
                    while (r < n && ascii_digit_char(t[r]))
                        ++r;
                    if (r + 1 < n && t[r] == '.' && ascii_digit_char(t[r + 1]))
                        for (++r; r < n && ascii_digit_char(t[r]); ++r)
                            ;
                    if (r == b0 || used + (r - b0) + 2 >= sizeof(w))
                        break;
                    xa = r < n && t[r] == ' ' ? r + 1 : r;
                    if (!(xa < n && (t[xa] == 'x' || t[xa] == 'X')))
                        break;
                    xb = xa + 1 < n && t[xa + 1] == ' ' ? xa + 2 : xa + 1;
                    if (!(xb < n && ascii_digit_char(t[xb])))
                        break;
                    odd |= (xa > r) != (xb > xa + 1);
                    used += (size_t)snprintf(w + used, sizeof(w) - used, "%.*s*", (int)(r - b0), t + b0);
                    last = xb;
                    ++terms;
                    r = xb;
                }
                if (odd && terms)
                    return owner_finish(in, out, last, TTF_SEM_SYMBOL, "@dims", w, "en.dims");
            }
            /* 10/Pack ten per pack, 12/Box: a count, a slash and a container word */
            {
                size_t r = p, wl = 0;
                char key[16];
                const char *per;
                while (r < n && ascii_digit_char(t[r]))
                    ++r;
                if (r + 1 < n && t[r] == '/' && ascii_letter_char(t[r + 1])) {
                    size_t z = r + 1;
                    while (z < n && ascii_letter_char(t[z]) && wl + 1 < sizeof(key))
                        key[wl++] = (char)(t[z] >= 'A' && t[z] <= 'Z' ? t[z] + 32 : t[z]), ++z;
                    key[wl] = 0;
                    if (!(z < n && (ascii_letter_char(t[z]) || ascii_digit_char(t[z]) || t[z] == '/')) &&
                        (per = res(in->engine, "per_word", key)) != NULL &&
                        strcmp(key, "second") && strcmp(key, "minute") && strcmp(key, "hour") &&
                        strcmp(key, "day") && strcmp(key, "week") && strcmp(key, "month") && strcmp(key, "year")) {
                        (void)snprintf(w, sizeof(w), "%.*s|%s", (int)(r - p), t + p, per);
                        return owner_finish(in, out, z, TTF_SEM_SYMBOL, "@count-per", w, "en.count-per");
                    }
                }
            }
            /* 15 to 30 June 2013 the fifteenth to the thirtieth of June: a day before a range to a
               named date */
            if (t[p] != '0' && (a = p + 1 + (p + 1 < n && ascii_digit_char(t[p + 1]))) <= n && atoi(t + p) >= 1 &&
                atoi(t + p) <= 31 && !(a < n && ascii_digit_char(t[a]))) {
                size_t r = a;
                while (r < n && t[r] == ' ')
                    ++r;
                if (!strncmp(t + r, "to ", 3))
                    r += 3;
                else if (!strncmp(t + r, "through ", 8))
                    r += 8;
                else if (r < n && t[r] == '-')
                    ++r;
                else if (!strncmp(t + r, "\xe2\x80\x93", 3))
                    r += 3;
                else
                    r = 0;
                if (r) {
                    while (r < n && t[r] == ' ')
                        ++r;
                    if (r < n && ascii_digit_char(t[r])) {
                        size_t d = r;
                        while (d < n && ascii_digit_char(t[d]))
                            ++d;
                        if (d - r <= 2 && d + 1 < n && t[d] == ' ')
                            for (k = 0; k < 12; ++k)
                                if (!strncmp(t + d + 1, month[k], 3)) {
                                    (void)snprintf(w, sizeof(w), "%.*s", (int)(a - p), t + p);
                                    return owner_finish(in, out, a, TTF_SEM_SYMBOL, "@ord-the", w, "en.day-range-first");
                                }
                    }
                }
                /* February 19 to 26 February nineteenth to the twenty sixth: the second day */
                {
                    size_t b = p, d;
                    while (b && t[b - 1] == ' ')
                        --b;
                    if (b >= 3 && !strncmp(t + b - 3, " to", 3))
                        b -= 3;
                    else if (b >= 3 && !strncmp(t + b - 3, "\xe2\x80\x93", 3))
                        b -= 3;
                    else
                        b = 0;
                    if (b) {
                        while (b && t[b - 1] == ' ')
                            --b;
                        d = b;
                        while (d && ascii_digit_char(t[d - 1]))
                            --d;
                        if (d < b && b - d <= 2 && d >= 2 && t[d - 1] == ' ' &&
                            !(a < n && (t[a] == ' ' && a + 1 < n && ascii_letter_char(t[a + 1]) && t[a + 1] >= 'A' && t[a + 1] <= 'Z')))
                            for (k = 0; k < 12; ++k) {
                                size_t l = strlen(month[k]);
                                if (d >= l + 1 && !strncmp(t + d - 1 - l, month[k], l)) {
                                    (void)snprintf(w, sizeof(w), "%.*s", (int)(a - p), t + p);
                                    return owner_finish(in, out, a, TTF_SEM_SYMBOL, "@ord-the", w, "en.day-range-second");
                                }
                            }
                    }
                }
            }
        }
        /* July-September July to September */
        for (k = 0; k < 12; ++k) {
            size_t l = strlen(month[k]), j;
            if (strncmp(t + p, month[k], l) || p + l >= n || t[p + l] != '-')
                continue;
            for (j = 0; j < 12; ++j) {
                size_t m = strlen(month[j]);
                if (!strncmp(t + p + l + 1, month[j], m) && !(p + l + 1 + m < n && ascii_letter_char(t[p + l + 1 + m]))) {
                    (void)snprintf(w, sizeof(w), "%s to %s", month[k], month[j]);
                    return owner_finish(in, out, p + l + 1 + m, TTF_SEM_SYMBOL, "@word", w, "en.month-range");
                }
            }
        }
        /* Index .SPX dot S P X: a ticker */
        if (t[p] == '.' && (!p || t[p - 1] == ' ' || t[p - 1] == '(')) {
            for (a = p + 1; a < n && t[a] >= 'A' && t[a] <= 'Z'; ++a)
                ;
            if (a - p - 1 >= 2 && a - p - 1 <= 5 && !(a < n && (ascii_letter_char(t[a]) || ascii_digit_char(t[a])))) {
                size_t used = (size_t)snprintf(w, sizeof(w), "dot");
                for (k = p + 1; k < a; ++k)
                    used += (size_t)snprintf(w + used, sizeof(w) - used, " %c", t[k]);
                return owner_finish(in, out, a, TTF_SEM_SYMBOL, "@word", w, "en.ticker");
            }
        }
        /* $53.7-million fifty three point seven million dollars, $US6 million six million US dollars */
        if (t[p] == '$' && p + 2 < n) {
            static const char *const prefix[][2] = {{"US", "u s dollars"}, {"NZ", "new zealand dollars"},
                                                    {"HK", "hong kong dollars"}, {"A", "australian dollars"},
                                                    {"C", "canadian dollars"}, {"S", "singapore dollars"}};
            const char *money = "dollars";
            size_t q0 = p + 1, r, s0;
            for (k = 0; k < 6; ++k)
                if (!strncmp(t + q0, prefix[k][0], strlen(prefix[k][0])) &&
                    ascii_digit_char(t[q0 + strlen(prefix[k][0])])) {
                    money = prefix[k][1];
                    q0 += strlen(prefix[k][0]);
                    break;
                }
            for (r = q0; r < n && (ascii_digit_char(t[r]) || (t[r] == '.' && r + 1 < n && ascii_digit_char(t[r + 1])) ||
                                   (t[r] == ',' && r + 1 < n && ascii_digit_char(t[r + 1])));
                 ++r)
                ;
            if (r > q0 && r < n && (t[r] == '-' || (t[r] == ' ' && k < 6))) {
                static const char *const scale[] = {"million", "billion", "trillion", "thousand"};
                for (s0 = 0; s0 < 4; ++s0) {
                    size_t l = strlen(scale[s0]);
                    if (!strncmp(t + r + 1, scale[s0], l) && !(r + 1 + l < n && ascii_letter_char(t[r + 1 + l]))) {
                        int attributive = t[r] == '-' && p >= 2 && t[p - 1] == ' ' &&
                                          (word_before(t, p, "a|an"));
                        size_t used = 0, i2;
                        for (i2 = q0; i2 < r && used + 1 < sizeof(w); ++i2)
                            if (t[i2] != ',')
                                w[used++] = t[i2];
                        (void)snprintf(w + used, sizeof(w) - used, "|%s|%.*s", scale[s0],
                                       (int)(attributive && !strcmp(money, "dollars") ? 6 : strlen(money)), money);
                        return owner_finish(in, out, r + 1 + l, TTF_SEM_SYMBOL, "@money-scale", w, "en.money-scale");
                    }
                }
            }
        }
        /* Q4/2010 Q four twenty ten: a quarter of a year */
        if (t[p] == 'Q' && p + 7 < n + 1 && t[p + 1] >= '1' && t[p + 1] <= '4' && t[p + 2] == '/' &&
            (!strncmp(t + p + 3, "19", 2) || !strncmp(t + p + 3, "20", 2)) && p + 7 <= n &&
            ascii_digit_char(t[p + 5]) && ascii_digit_char(t[p + 6]) && !(p + 7 < n && ascii_digit_char(t[p + 7]))) {
            (void)snprintf(w, sizeof(w), "%c|%.4s", t[p + 1], t + p + 3);
            return owner_finish(in, out, p + 7, TTF_SEM_SYMBOL, "@quarter", w, "en.quarter");
        }
        /* (W x H) W by H: x between single capitals */
        if ((t[p] == 'x' || t[p] == 'X') && p >= 2 && p + 2 < n && t[p - 1] == ' ' && t[p + 1] == ' ' &&
            t[p - 2] >= 'A' && t[p - 2] <= 'Z' && !(p >= 3 && ascii_letter_char(t[p - 3])) &&
            t[p + 2] >= 'A' && t[p + 2] <= 'Z' && !(p + 3 < n && ascii_letter_char(t[p + 3])))
            return owner_finish(in, out, p + 1, TTF_SEM_SYMBOL, "@by", "by", "en.by");
        /* 3.0 – 4.0 percent three to four: a spaced en dash between numbers */
        if (!strncmp(t + p, "\xe2\x80\x93", 3) && p >= 2 && t[p - 1] == ' ' && ascii_digit_char(t[p - 2]) &&
            p + 4 < n && t[p + 3] == ' ' && ascii_digit_char(t[p + 4]))
            return owner_finish(in, out, p + 3, TTF_SEM_SYMBOL, "@to", "to", "en.spaced-dash");
    }
    /* WDT750SAKZ, 7KXJ442, LB26X: capitals and digits that change more than once are a code,
       read letter by letter and digit by digit (owner sets, 2026-10-14) */
    if ((ascii_digit_char(t[p]) || (t[p] >= 'A' && t[p] <= 'Z')) && !(p && ascii_letter_char(t[p - 1]))) {
        size_t letters = 0, digits_n = 0, runs = 0;
        int last = -1;
        for (q = p; q < n && (ascii_digit_char(t[q]) || (t[q] >= 'A' && t[q] <= 'Z')); ++q) {
            int kind = ascii_digit_char(t[q]);
            letters += !kind;
            digits_n += kind;
            runs += kind != last;
            last = kind;
        }
        if (runs >= 3 && letters >= 2 && digits_n >= 2 && q - p >= 5 && q - p < sizeof(v) &&
            !(q < n && (ascii_letter_char(t[q]) || t[q] == '-' || (t[q] == '.' && q + 1 < n && ascii_digit_char(t[q + 1]))))) {
            (void)snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
            return owner_finish(in, out, q, TTF_SEM_SYMBOL, "@code", v, "en.mixed-code");
        }
    }
    if (ascii_digit_char(t[p])) {
        for (q = p; q < n && ascii_digit_char(t[q]); ++q)
            ;
        /* 6'2" six foot two, 5'11" */
        if (q - p == 1 && q + 2 < n && t[q] == '\'' && ascii_digit_char(t[q + 1])) {
            for (r = q + 1; r < n && ascii_digit_char(t[r]) && r - q <= 2; ++r)
                ;
            if (r < n && t[r] == '"' && r - q - 1 <= 2) {
                (void)snprintf(v, sizeof(v), "%.*s|%.*s", (int)(q - p), t + p, (int)(r - q - 1), t + q + 1);
                return owner_finish(in, out, r + 1, TTF_SEM_SYMBOL, "@feet-inches", v, "en.feet-inches");
            }
        }
        /* 67¢ sixty seven cents (owner sets, 2026-10-14; English lines only) */
        if (q + 2 <= n && !strncmp(t + q, "\xc2\xa2", 2) && q - p <= 4) {
            (void)snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
            return owner_finish(in, out, q + 2, TTF_SEM_SYMBOL, "@cents", v, "en.cents");
        }
        /* a 65" TV sixty five inch: inches before a word */
        if (q - p <= 3 && q + 2 < n && t[q] == '"' && t[q + 1] == ' ' && ascii_letter_char(t[q + 2])) {
            size_t k, quotes = 0;
            /* "I have 5" and left: a closing quote, not inches */
            for (k = p; k && t[k - 1] != '\n'; --k)
                quotes += t[k - 1] == '"';
            if (quotes % 2)
                return 0;
            (void)snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
            return owner_finish(in, out, q + 1, TTF_SEM_SYMBOL, "@inch", v, "en.inch");
        }
        /* 4.5/5 stars four point five out of five: a decimal score over ten or less */
        if (q + 2 < n && t[q] == '.' && ascii_digit_char(t[q + 1])) {
            size_t d = q + 1, e2;
            while (d < n && ascii_digit_char(t[d]))
                ++d;
            if (d + 1 < n && t[d] == '/' && ascii_digit_char(t[d + 1])) {
                for (e2 = d + 1; e2 < n && ascii_digit_char(t[e2]); ++e2)
                    ;
                /* 1.5/2 stays as written (golden_en); a rating is out of four to ten */
                if (e2 - d - 1 <= 2 && atoi(t + d + 1) <= 10 && atoi(t + d + 1) >= 4 &&
                    !(e2 < n && (ascii_digit_char(t[e2]) || t[e2] == '/' || (t[e2] == '.' && e2 + 1 < n && ascii_digit_char(t[e2 + 1]))))) {
                    (void)snprintf(v, sizeof(v), "%.*s|%.*s", (int)(d - p), t + p, (int)(e2 - d - 1), t + d + 1);
                    return owner_finish(in, out, e2, TTF_SEM_SYMBOL, "@out-of", v, "en.out-of");
                }
            }
        }
        /* Expiration: 08/2027 August twenty twenty seven */
        if (q - p <= 2 && q + 5 <= n && t[q] == '/' && ascii_digit_char(t[q + 1]) && ascii_digit_char(t[q + 2]) &&
            ascii_digit_char(t[q + 3]) && ascii_digit_char(t[q + 4]) && !(q + 5 < n && ascii_digit_char(t[q + 5])) &&
            atoi(t + p) >= 1 && atoi(t + p) <= 12 &&
            (word_before(t, p, "expiration|expires|exp|expiry|valid|thru|through|until|since") ||
             strstr_before(t, p, "Expiration:") || strstr_before(t, p, "Exp:"))) {
            size_t used;
            (void)snprintf(v, sizeof(v), "%.4s-%02d", t + q + 1, atoi(t + p));
            used = owner_finish(in, out, q + 5, TTF_SEM_DATE, "year-month", v, "en.month-year");
            (void)snprintf(out->secondary, sizeof(out->secondary), "mdy");
            return used;
        }
        /* dilute 1:10 one to ten, a ratio of 3:1; a 3:1 ratio, a 16:9 aspect ratio (the
           ratio word after the numbers, 2026-10-30) */
        if (q + 1 < n && t[q] == ':' && ascii_digit_char(t[q + 1])) {
            size_t a;
            int after;
            for (r = q + 1; r < n && (ascii_digit_char(t[r]) || (t[r] == ',' && r + 1 < n && ascii_digit_char(t[r + 1])));
                 ++r)
                ;
            for (a = r; a < n && t[a] == ' '; ++a)
                ;
            if (a + 7 <= n && !strncmp(t + a, "aspect ", 7))
                a += 7;
            after = a > r && a + 5 <= n && !strncmp(t + a, "ratio", 5) && !(a + 5 < n && ascii_letter_char(t[a + 5]));
            /* stands at 4:1, is 18:1, a 2:1 split: a one-digit second number is never minutes, so
               after a lowercase word it is a ratio without a cue (round 22; was left as 4:1). A
               capitalized word before it may be a book and verse (Genesis 1:1). */
            int lone = r == q + 2 && p >= 2 && t[p - 1] == ' ' && t[p - 2] >= 'a' && t[p - 2] <= 'z' &&
                       !(r < n && (t[r] == '-' || (t[r] == '.' && r + 1 < n && ascii_digit_char(t[r + 1]))));
            if (lone) {
                size_t w = p - 2;
                while (w && ascii_letter_char(t[w - 1]))
                    --w;
                lone = t[w] >= 'a' && t[w] <= 'z';
            }
            if ((after || lone || word_before(t, p, "dilute|diluted|dilution|ratio|mix|mixed|scale|odds|concentrate|stock") ||
                 (word_before(t, p, "of") && strstr_before(t, p, "ratio of"))) &&
                !(r < n && (ascii_digit_char(t[r]) || t[r] == ':')) && r - p < sizeof(v)) {
                size_t k, used = 0;
                for (k = p; k < r; ++k)
                    if (t[k] != ',')
                        v[used++] = t[k] == ':' ? '|' : t[k];
                v[used] = 0;
                return owner_finish(in, out, r, TTF_SEM_SYMBOL, "@ratio", v, "en.ratio");
            }
        }
        /* 1½ hours an hour and a half, 2½ two and a half, 7¾ in seven and three quarters inches
           (owner sets, 2026-10-14) */
        if (q - p <= 3 && q + 2 <= n && (unsigned char)t[q] == 0xc2 &&
            ((unsigned char)t[q + 1] == 0xbd || (unsigned char)t[q + 1] == 0xbc || (unsigned char)t[q + 1] == 0xbe)) {
            const char *frac = (unsigned char)t[q + 1] == 0xbd   ? "a half"
                               : (unsigned char)t[q + 1] == 0xbc ? "a quarter"
                                                                 : "three quarters";
            size_t z = q + 2, w = q + 3, we = q + 3;
            int inches;
            if (q - p == 1 && t[p] == '1' && (unsigned char)t[q + 1] == 0xbd && z < n && t[z] == ' ') {
                while (we < n && t[we] >= 'a' && t[we] <= 'z')
                    ++we;
                if (we - w >= 4 && t[we - 1] == 's' && !(we < n && ascii_letter_char(t[we]))) {
                    (void)snprintf(v, sizeof(v), "%s %.*s and a half",
                                   strchr("aeiou", t[w]) || !strncmp(t + w, "hour", 4) ? "an" : "a", (int)(we - w - 1),
                                   t + w);
                    return owner_finish(in, out, we, TTF_SEM_SYMBOL, "@word", v, "en.mixed-fraction");
                }
            }
            inches = z + 3 <= n && !strncmp(t + z, " in", 3) && !(z + 3 < n && ascii_letter_char(t[z + 3]));
            /* 1½ tbsp one and a half tablespoons (2026-10-31, TN round 19; was one and a half tbsp) */
            if (!inches && z + 1 < n && t[z] == ' ') {
                static const char *const abbr[][2] = {{"tbsp", " tablespoons"}, {"tsp", " teaspoons"}, {"lbs", " pounds"},
                                                      {"lb", " pounds"}, {"oz", " ounces"}};
                for (size_t ai = 0; ai < sizeof(abbr) / sizeof(*abbr); ++ai) {
                    size_t l = strlen(abbr[ai][0]);
                    if (z + 1 + l <= n && !strncmp(t + z + 1, abbr[ai][0], l) && !(z + 1 + l < n && ascii_letter_char(t[z + 1 + l]))) {
                        (void)snprintf(v, sizeof(v), "%.*s|%s%s", (int)(q - p), t + p, frac, abbr[ai][1]);
                        return owner_finish(in, out, z + 1 + l, TTF_SEM_SYMBOL, "@mixed", v, "en.mixed-fraction");
                    }
                }
            }
            (void)snprintf(v, sizeof(v), "%.*s|%s%s", (int)(q - p), t + p, frac, inches ? " inches" : "");
            return owner_finish(in, out, inches ? z + 3 : z, TTF_SEM_SYMBOL, "@mixed", v, "en.mixed-fraction");
        }
        /* 1.2M one point two million: a decimal before M or B (owner sets, 2026-10-14) */
        if (q + 2 < n && t[q] == '.' && ascii_digit_char(t[q + 1])) {
            size_t d = q + 1;
            while (d < n && ascii_digit_char(t[d]))
                ++d;
            if (d < n && (t[d] == 'M' || t[d] == 'B') &&
                !(d + 1 < n && (ascii_letter_char(t[d + 1]) || ascii_digit_char(t[d + 1])))) {
                (void)snprintf(v, sizeof(v), "%.*s|%s", (int)(d - p), t + p, t[d] == 'M' ? "million" : "billion");
                return owner_finish(in, out, d + 1, TTF_SEM_SYMBOL, "@scaled", v, "en.scaled");
            }
        }
        /* a 90 s target ninety second, 4 min 12 s four minutes twelve seconds */
        if (q - p <= 4 && q + 2 <= n && t[q] == ' ' && t[q + 1] == 's' &&
            !(q + 2 < n && (ascii_letter_char(t[q + 2]) || ascii_digit_char(t[q + 2]) || t[q + 2] == '\'' ||
                            t[q + 2] == '-')) &&
            (word_before(t, p, "min|mins|minute|minutes|wait|waited|took|takes|take|lasted|lasts|after|every|within|"
                               "timeout|latency|delay|interval|about|around|under|over|only|than") ||
             (q + 3 < n && t[q + 2] == ' ' &&
              (!strncmp(t + q + 3, "target", 6) || !strncmp(t + q + 3, "window", 6) || !strncmp(t + q + 3, "timeout", 7) ||
               !strncmp(t + q + 3, "delay", 5) || !strncmp(t + q + 3, "limit", 5) || !strncmp(t + q + 3, "interval", 8) ||
               !strncmp(t + q + 3, "cooldown", 8) || !strncmp(t + q + 3, "countdown", 9))))) {
            static const char *const nouns[] = {"target", "window", "timeout", "delay", "limit", "interval",
                                                "clip", "cooldown", "countdown", "video", "rest", "break"};
            size_t k, w = q + 3;
            int adjective = 0;
            if (q + 2 < n && t[q + 2] == ' ')
                for (k = 0; k < sizeof(nouns) / sizeof(*nouns); ++k) {
                    size_t l = strlen(nouns[k]);
                    if (w + l <= n && !strncmp(t + w, nouns[k], l) && !(w + l < n && ascii_letter_char(t[w + l])))
                        adjective = 1;
                }
            (void)snprintf(v, sizeof(v), "%.*s|%s", (int)(q - p), t + p, adjective ? "second" : "s");
            return owner_finish(in, out, q + 2, TTF_SEM_SYMBOL, "@seconds", v, "en.seconds");
        }
        /* 737 MAX seven thirty seven (Boeing 737 alone stays seven hundred thirty seven, nemo-engconf) */
        if (q - p == 3 && in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH &&
            q + 4 <= n && !strncmp(t + q, " MAX", 4) && !(q + 4 < n && ascii_letter_char(t[q + 4]))) {
            (void)snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
            return owner_finish(in, out, q, TTF_SEM_ALNUM_TERM, "", v, "en.model-number");
        }
        /* Room 1408 fourteen oh eight, Gate 214 two fourteen */
        if (q - p >= 3 && q - p <= 4 && t[p] != '0' &&
            in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH &&
            word_before(t, p, "room|suite|gate|flight|train|bus|platform|apartment|apt") &&
            !(q < n && (ascii_letter_char(t[q]) || ascii_digit_char(t[q]) || t[q] == '.' || t[q] == ',' ||
                        t[q] == '-' || t[q] == '/'))) {
            (void)snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
            return owner_finish(in, out, q, TTF_SEM_ALNUM_TERM, "", v, "en.pair-label");
        }
        return 0;
    }
    /* 8am–1pm eight a m to one p m: an en dash between clock times (owner sets, 2026-10-14) */
    if (p >= 3 && p + 3 < n && !strncmp(t + p, "\xe2\x80\x93", 3) && ascii_digit_char(t[p + 3]) &&
        (t[p - 1] == 'm' || t[p - 1] == 'M' || (t[p - 1] == '.' && (t[p - 2] == 'm' || t[p - 2] == 'M')))) {
        size_t m = t[p - 1] == '.' ? p - 2 : p - 1;
        char c = m ? t[m - 1] : 0;
        if ((c == 'a' || c == 'p' || c == 'A' || c == 'P' || c == '.') && m >= 2 &&
            (ascii_digit_char(t[m - 2]) || t[m - 2] == ' ' || t[m - 2] == 'a' || t[m - 2] == 'p'))
            return owner_finish(in, out, p + 3, TTF_SEM_SYMBOL, "@word", " to ", "en.time-range");
    }
    /* x − 3 x minus three: a spaced minus sign (U+2212) between terms */
    if (p && p + 4 < n && !strncmp(t + p, "\xe2\x88\x92", 3) && t[p - 1] == ' ' && t[p + 3] == ' ')
        return owner_finish(in, out, p + 3, TTF_SEM_SYMBOL, "@word", "minus", "en.minus-sign");
    /* x² x squared, πr² pi r squared: a superscript after a letter */
    if (p && (!strncmp(t + p, "\xc2\xb2", 2) || !strncmp(t + p, "\xc2\xb3", 2)) &&
        (ascii_letter_char(t[p - 1]) || t[p - 1] == ')') &&
        /* 53.5 /km², 50 m²: a unit keeps its own reading */
        !word_before(t, p, "m|cm|mm|km|dm|ft|in|mi|yd|nm|um"))
    {
        /* I²R eye squared R: a space only before a following letter. Otherwise written ^2, as
           the Korean pack writes a power, so a Korean reading says 제곱 ((x+1)²=0, E=mc²;
           2026-10-26, was the English word spelled 에스큐유에이알이디). */
        int tight = p + 2 < n && (ascii_letter_char(t[p + 2]) || ascii_digit_char(t[p + 2]));
        if (!tight)
            return owner_finish(in, out, p + 2, TTF_SEM_SYMBOL, "@expression", t[p + 1] == '\xb2' ? "^2" : "^3",
                                "en.power");
        return owner_finish(in, out, p + 2, TTF_SEM_SYMBOL, "@word", t[p + 1] == '\xb2' ? " squared " : " cubed ",
                            "en.power");
    }
    /* (x + 2)(x − 3) x plus two times x minus three */
    if (p && t[p] == '(' && t[p - 1] == ')')
        return owner_finish(in, out, p + 1, TTF_SEM_SYMBOL, "@word", " times (", "en.product");
    /* CAD 64.30 sixty four Canadian dollars and thirty cents: a currency code before an amount */
    if (t[p] >= 'A' && t[p] <= 'Z' && p + 5 < n && t[p + 3] == ' ' && ascii_digit_char(t[p + 4]) &&
        !(p && ascii_letter_char(t[p - 1]))) {
        static const char *const codes[] = {"GBP", "CAD", "AUD", "CHF", "JPY", "CNY",
                                            "NZD", "SGD", "HKD", "INR", "MXN"};
        size_t k, used = 0;
        for (k = 0; k < sizeof(codes) / sizeof(*codes); ++k)
            if (!strncmp(t + p, codes[k], 3)) {
                for (r = p + 4; r < n && used + 1 < sizeof(v) &&
                                (ascii_digit_char(t[r]) ||
                                 ((t[r] == ',' || t[r] == '.') && r + 1 < n && ascii_digit_char(t[r + 1])));
                     ++r)
                    if (t[r] != ',')
                        v[used++] = t[r];
                v[used] = 0;
                if (r < n && (ascii_letter_char(t[r]) || ascii_digit_char(t[r])))
                    return 0;
                return owner_finish(in, out, r, TTF_SEM_CURRENCY, codes[k], v, "en.currency-code");
            }
    }
    /* *67 star six seven: a star code */
    if (t[p] == '*' && p + 3 <= n && ascii_digit_char(t[p + 1]) && ascii_digit_char(t[p + 2]) &&
        !(p && !(t[p - 1] == ' ' || t[p - 1] == '('))) {
        for (q = p + 1; q < n && ascii_digit_char(t[q]); ++q)
            ;
        if (q - p - 1 <= 3 && !(q < n && (ascii_letter_char(t[q]) || t[q] == '*'))) {
            (void)snprintf(v, sizeof(v), "%.*s", (int)(q - p - 1), t + p + 1);
            return owner_finish(in, out, q, TTF_SEM_SYMBOL, "@star", v, "en.star-code");
        }
    }
    /* localhost:8080 localhost colon eight zero eight zero */
    if (p + 11 <= n && !strncmp(t + p, "localhost:", 10) && ascii_digit_char(t[p + 10])) {
        for (q = p + 10; q < n && ascii_digit_char(t[q]); ++q)
            ;
        if (q - p - 10 <= 5 && !(q < n && ascii_letter_char(t[q]))) {
            (void)snprintf(v, sizeof(v), "%.*s", (int)(q - p - 10), t + p + 10);
            return owner_finish(in, out, q, TTF_SEM_SYMBOL, "@hostport", v, "en.host-port");
        }
    }
    /* Oct 24-26 October twenty fourth to twenty sixth: a day range after a month */
    if (t[p] >= 'A' && t[p] <= 'Z') {
        static const char *const names[] = {"January", "February", "March", "April", "May", "June", "July",
                                            "August", "September", "October", "November", "December"};
        size_t k;
        for (k = 0; k < 12; ++k) {
            size_t l = strlen(names[k]), m = 0, d1 = 0, d2 = 0, r2;
            if (p + 3 <= n && !strncmp(t + p, names[k], 3)) {
                m = !strncmp(t + p, names[k], l) && !(p + l < n && ascii_letter_char(t[p + l])) ? l
                    : !(p + 3 < n && ascii_letter_char(t[p + 3]))                                ? 3
                                                                                                  : 0;
                if (m == 3 && p + 3 < n && t[p + 3] == '.')
                    ++m;
            }
            if (!m || p + m + 4 > n || t[p + m] != ' ' || !ascii_digit_char(t[p + m + 1]))
                continue;
            for (q = p + m + 1; q < n && ascii_digit_char(t[q]) && q - p - m <= 2; ++q)
                d1 = d1 * 10 + (size_t)(t[q] - '0');
            if (q - p - m - 1 > 2 || q >= n)
                break;
            r2 = q + (t[q] == '-' ? 1 : !strncmp(t + q, "â", 3) ? 3 : 0);
            if (r2 == q || r2 >= n || !ascii_digit_char(t[r2]))
                break;
            for (q = r2; q < n && ascii_digit_char(t[q]) && q - r2 < 3; ++q)
                d2 = d2 * 10 + (size_t)(t[q] - '0');
            if (q - r2 > 2 || (q < n && (ascii_digit_char(t[q]) || ascii_letter_char(t[q]) || t[q] == '-')) ||
                !d1 || d1 >= d2 || d2 > 31)
                break;
            (void)snprintf(v, sizeof(v), "%s|%u|%u", names[k], (unsigned)d1, (unsigned)d2);
            return owner_finish(in, out, q, TTF_SEM_SYMBOL, "@day-range", v, "en.day-range");
        }
    }
    /* +/- 3 plus or minus three */
    if (p + 3 < n && !strncmp(t + p, "+/-", 3))
        return owner_finish(in, out, p + 3, TTF_SEM_SYMBOL, "@plus-minus", "+/-", "en.plus-minus");
    /* Monday–Friday Monday to Friday: an en dash between weekday names */
    if (t[p] >= 'A' && t[p] <= 'Z') {
        static const char *const days[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
        size_t k, j, a2;
        for (k = 0; k < 7; ++k) {
            size_t l = strlen(days[k]);
            if (p + l + 3 < n && !strncmp(t + p, days[k], l) && !strncmp(t + p + l, "\xe2\x80\x93", 3))
                for (j = 0; j < 7; ++j) {
                    a2 = strlen(days[j]);
                    if (p + l + 3 + a2 <= n && !strncmp(t + p + l + 3, days[j], a2) &&
                        !(p + l + 3 + a2 < n && ascii_letter_char(t[p + l + 3 + a2]))) {
                        (void)snprintf(v, sizeof(v), "%s to %s", days[k], days[j]);
                        return owner_finish(in, out, p + l + 3 + a2, TTF_SEM_SYMBOL, "@word", v, "en.weekday-range");
                    }
                }
        }
    }
    /* Wed, Oct 15 Wednesday: a weekday abbreviation before a date */
    if (t[p] >= 'A' && t[p] <= 'Z' && p + 4 < n) {
        static const char *const abbr[][2] = {{"Mon", "Monday"}, {"Tue", "Tuesday"}, {"Wed", "Wednesday"},
                                              {"Thu", "Thursday"}, {"Fri", "Friday"}, {"Sat", "Saturday"},
                                              {"Sun", "Sunday"}};
        static const char *const months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
                                             "Aug", "Sep", "Oct", "Nov", "Dec"};
        size_t k, m, after;
        for (k = 0; k < 7; ++k)
            if (!strncmp(t + p, abbr[k][0], 3) && !ascii_letter_char(t[p + 3])) {
                after = p + 3 + (t[p + 3] == '.');
                if (after < n && t[after] == ',')
                    ++after;
                if (after >= n || t[after] != ' ')
                    return 0;
                ++after;
                if (after < n && ascii_digit_char(t[after]))
                    return owner_finish(in, out, p + 3 + (t[p + 3] == '.'), TTF_SEM_SYMBOL, "@word", abbr[k][1],
                                        "en.weekday-abbr");
                for (m = 0; m < 12; ++m)
                    if (after + 3 <= n && !strncmp(t + after, months[m], 3))
                        return owner_finish(in, out, p + 3 + (t[p + 3] == '.'), TTF_SEM_SYMBOL, "@word",
                                            abbr[k][1], "en.weekday-abbr");
                return 0;
            }
    }
    /* Flight UA 2314: an airline code, one space and a three- or four-digit number */
    if (t[p] >= 'A' && t[p] <= 'Z' && in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH &&
        word_before(t, p, "flight|flights")) {
        static const char *const stop[] = {"AM", "PM", "AD", "BC", "CE", "BCE", "US", "UK", "TV", "OK",
                                           "ID", "IT", "NO", "ON", "IN", "AT", "OF", "TO", "BY", "IS",
                                           "AS", "OR", "AN", "IF", "SO", "UP", "WE", "MY", "HE", "BE",
                                           "DO", "GO", "ME", "FY", "CO", "PH", "CD", "DVD", "THE", "AND",
                                           "FOR", "ALL", "GDP", "CEO"};
        size_t k;
        for (q = p; q < n && t[q] >= 'A' && t[q] <= 'Z'; ++q)
            ;
        if (q - p < 2 || q - p > 3 || q + 4 > n || t[q] != ' ' || !ascii_digit_char(t[q + 1]) || t[q + 1] == '0' ||
            (p && ascii_letter_char(t[p - 1])))
            return 0;
        for (k = 0; k < sizeof(stop) / sizeof(*stop); ++k)
            if (strlen(stop[k]) == q - p && !strncmp(stop[k], t + p, q - p))
                return 0;
        for (r = q + 1; r < n && ascii_digit_char(t[r]); ++r)
            ;
        if (r - q - 1 < 3 || r - q - 1 > 4 ||
            (r < n && (ascii_letter_char(t[r]) || ascii_digit_char(t[r]) || t[r] == '.' || t[r] == ',' ||
                       t[r] == '-' || t[r] == '/' || t[r] == ':' || t[r] == '%')))
            return 0;
        if (r < n && t[r] == ',' && !(r + 1 < n && t[r + 1] == ' '))
            return 0;
        (void)snprintf(v, sizeof(v), "%.*s", (int)(r - p), t + p);
        return owner_finish(in, out, r, TTF_SEM_ALNUM_TERM, "", v, "en.spaced-code");
    }
    /* 4 x 8 ft four by eight, 2x4s two by fours: x or × between numbers outside an equation */
    if ((t[p] == 'x' || t[p] == 'X' || !strncmp(t + p, "\xc3\x97", 2)) && p) {
        size_t w = t[p] == 'x' || t[p] == 'X' ? 1 : 2, a = p, b = p + w, k;
        while (a && t[a - 1] == ' ')
            --a;
        if (!(a && ascii_digit_char(t[a - 1])) || (p - a > 1))
            return 0;
        if (b < n && t[b] == ' ')
            ++b;
        if (!(b < n && ascii_digit_char(t[b])) || (p - a) != (b - p - w))
            return 0;
        for (k = b; k < n && k < b + 24 && t[k] != '\n'; ++k)
            if (t[k] == '=' || (t[k] == ' ' && k + 1 < n && (t[k + 1] == '+' || t[k + 1] == '-')))
                return 0;
        return owner_finish(in, out, p + w, TTF_SEM_SYMBOL, "@by", "by", "en.by");
    }
    return 0;
}

/* ---- English dev set v1 forms (2026-10-17) ----
   A reading is written as @say items separated by '|': c:N cardinal, o:N ordinal, d:DIGITS digit
   by digit, k:DIGITS a label number in pairs (214 two fourteen, 1600 sixteen hundred), n:NUM a
   number with its decimals, w:words as written. */
static size_t say_span(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t end, const char *value,
                       const char *rule) {
    if (strlen(value) >= sizeof(out->value))
        return 0;
    (void)snprintf(out->value, sizeof(out->value), "%s", value);
    (void)snprintf(out->unit, sizeof(out->unit), "@say");
    (void)snprintf(out->rule, sizeof(out->rule), "en.%s", rule);
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    out->type = TTF_SEM_CUSTOM;
    out->input_end = end;
    return tokens_until(in, end);
}
/* The lowercase word that starts at p (after one space when skip). */
static size_t word_at(const char *t, size_t n, size_t p, char *key, size_t cap) {
    size_t w = p, k = 0;
    while (w < n && ascii_letter_char(t[w]) && k + 1 < cap)
        key[k++] = (char)(t[w] >= 'A' && t[w] <= 'Z' ? t[w] - 'A' + 'a' : t[w]), ++w;
    key[k] = 0;
    return w < n && ascii_letter_char(t[w]) ? 0 : k;
}
static int in_list(const char *key, const char *words) {
    size_t n = strlen(key);
    const char *s = words;
    while (*s) {
        const char *bar = strchr(s, '|');
        size_t l = bar ? (size_t)(bar - s) : strlen(s);
        if (l == n && !memcmp(s, key, n))
            return 1;
        if (!bar)
            break;
        s = bar + 1;
    }
    return 0;
}
static const char *const dev_month[] = {"January", "February", "March", "April", "May", "June", "July",
                                        "August", "September", "October", "November", "December"};
static size_t dev_forms(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q, r, k;
    char v[96], key[24], num[40];
    /* only in an English request and speech output (text output keeps the written forms) */
    if (p >= n || !english_window(in, p, p + 1) || !in->engine->default_language ||
        strcmp(in->engine->default_language, EN) || in->engine->frontend_options.output_mode != TTF_OUTPUT_SPEECH)
        return 0;
    /* ---- listening check forms (TTS to STT on the owner set dev split, 2026-10-22) ---- */
    /* °F = °C × 9/5 + 32 degrees Fahrenheit equals degrees Celsius: a degree sign without a number */
    if (p + 2 < n && (unsigned char)t[p] == 0xC2 && (unsigned char)t[p + 1] == 0xB0 && (t[p + 2] == 'F' || t[p + 2] == 'C') &&
        !(p + 3 < n && ascii_letter_char(t[p + 3]))) {
        size_t z = p;
        while (z && t[z - 1] == ' ')
            --z;
        if (!(z && ascii_digit_char(t[z - 1])))
            return say_span(in, out, p + 3, t[p + 2] == 'F' ? "w:degrees Fahrenheit" : "w:degrees Celsius", "degree-name");
    }
    /* the ½" shank half inch: a fraction glyph and an inch mark */
    if (p + 2 < n && (unsigned char)t[p] == 0xC2 &&
        ((unsigned char)t[p + 1] == 0xBD || (unsigned char)t[p + 1] == 0xBC || (unsigned char)t[p + 1] == 0xBE) &&
        (t[p + 2] == '"' || (p + 4 < n && !memcmp(t + p + 2, "\xe2\x80\xb3", 3))) && !(p && ascii_digit_char(t[p - 1])))
        return say_span(in, out, p + (t[p + 2] == '"' ? 3 : 5),
                        (unsigned char)t[p + 1] == 0xBD ? "w:half inch"
                        : (unsigned char)t[p + 1] == 0xBC ? "w:quarter inch" : "w:three quarter inch",
                        "fraction-inch");
    /* C-3PO see three pee oh: a letter, a hyphen, a digit and capitals */
    if (t[p] >= 'A' && t[p] <= 'Z' && p + 3 < n && t[p + 1] == '-' && ascii_digit_char(t[p + 2]) &&
        t[p + 3] >= 'A' && t[p + 3] <= 'Z' && !(p && (ascii_letter_char(t[p - 1]) || ascii_digit_char(t[p - 1])))) {
        size_t z = p + 3;
        while (z < n && t[z] >= 'A' && t[z] <= 'Z' && z - p < 7)
            ++z;
        if (z - p - 3 <= 3 && !(z < n && (ascii_letter_char(t[z]) || ascii_digit_char(t[z]) || t[z] == '-'))) {
            (void)snprintf(v, sizeof(v), "s:%c|c:%c|s:%.*s", t[p], t[p + 2], (int)(z - p - 3), t + p + 3);
            return say_span(in, out, z, v, "letter-digit-letters");
        }
    }
    /* 2×4s two by fours, a 4 × 8 ft sheet four by eight feet: × between dimensions */
    if (ascii_digit_char(t[p]) && !(p && (ascii_digit_char(t[p - 1]) || ascii_letter_char(t[p - 1]) || t[p - 1] == '.'))) {
        size_t a = p, x, b, z, sp1, sp2;
        while (a < n && ascii_digit_char(t[a]) && a - p < 3)
            ++a;
        for (x = a; x < n && t[x] == ' '; ++x)
            ;
        sp1 = x - a;
        z = x + (x < n && (t[x] == 'x' || t[x] == 'X') ? 1 : x + 1 < n && !memcmp(t + x, "\xc3\x97", 2) ? 2 : 0);
        for (b = z; z > x && b < n && t[b] == ' '; ++b)
            ;
        sp2 = b - z;
        if (z > x && a - p <= 2 && sp1 == sp2 && sp1 <= 1 && b < n && ascii_digit_char(t[b])) {
            size_t c = b, u, k2;
            int dims = 0, eq = 0;
            while (c < n && ascii_digit_char(t[c]) && c - b < 3)
                ++c;
            for (k2 = c; k2 < n && k2 < c + 24 && t[k2] != '\n'; ++k2)
                if (t[k2] == '=')
                    eq = 1;
            for (k2 = p > 24 ? p - 24 : 0; k2 < p; ++k2)
                if (t[k2] == '=')
                    eq = 1;
            if (c - b <= 2 && !(c < n && ascii_digit_char(t[c]))) {
                if (c < n && t[c] == 's' && !(c + 1 < n && ascii_letter_char(t[c + 1])))
                    dims = 1; /* 2×4s */
                else if (c < n && (t[c] == '\'' || t[c] == '"' || t[c] == 'x' || !memcmp(t + c, "\xc3\x97", 2)))
                    dims = 1; /* 4×8', 4×8×10 */
                else {
                    for (u = c; u < n && t[u] == ' ' && u - c < 2; ++u)
                        ;
                    if (word_at(t, n, u, key, sizeof(key)) &&
                        in_list(key, "ft|feet|foot|in|inch|inches|cm|mm|m|meter|meters|yd|yards|sheet|sheets|rug|"
                                     "board|boards|tarp|frame|print|poster|photo|grid|room|tile|tiles|pan|pans|loaf|dish|tray|tin"))
                        dims = 1;
                }
            }
            if (dims && !eq) {
                static const char *const plural[] = {"", "ones", "twos", "threes", "fours", "fives", "sixes", "sevens",
                                                     "eights", "nines", "tens", "elevens", "twelves"};
                unsigned bv = (unsigned)atoi(t + b);
                size_t end = c, l;
                (void)snprintf(v, sizeof(v), "c:%.*s|w:by|c:%.*s", (int)(a - p), t + p, (int)(c - b), t + b);
                if (c < n && t[c] == 's') {
                    if (bv < 1 || bv > 12)
                        return 0;
                    (void)snprintf(v, sizeof(v), "c:%.*s|w:by|w:%s", (int)(a - p), t + p, plural[bv]);
                    end = c + 1;
                } else {
                    const char *unit = NULL;
                    size_t ue = c;
                    if (c < n && t[c] == '\'')
                        unit = "feet", ue = c + 1;
                    else if (c < n && t[c] == '"')
                        unit = "inches", ue = c + 1;
                    else {
                        for (u = c; u < n && t[u] == ' ' && u - c < 2; ++u)
                            ;
                        l = word_at(t, n, u, key, sizeof(key));
                        if (l && !(u + l < n && t[u + l] == '.' && u + l + 1 < n && ascii_letter_char(t[u + l + 1]))) {
                            unit = in_list(key, "ft|feet") ? "feet" : in_list(key, "in|inch|inches") ? "inches"
                                   : !strcmp(key, "cm") ? "centimeters" : !strcmp(key, "mm") ? "millimeters"
                                   : !strcmp(key, "m") ? "meters" : in_list(key, "yd|yards") ? "yards" : NULL;
                            /* 4 x 8 in the box: in is a preposition before an article */
                            if (unit && !strcmp(key, "in") && u + 3 < n && t[u + 2] == ' ')
                                unit = NULL;
                            ue = u + l;
                        }
                    }
                    /* a 4 x 8 ft sheet four by eight foot sheet: singular before a noun */
                    if (unit && ue + 2 < n && t[ue] == ' ' && t[ue + 1] >= 'a' && t[ue + 1] <= 'z' &&
                        word_at(t, n, ue + 1, key, sizeof(key)) &&
                        !in_list(key, "of|for|in|on|at|to|and|or|is|was|are|were|from|with|by|long|high|wide|deep|tall|"
                                      "each|apart|a|an|the|that|which|overall|total|altogether|across|thick|"
                                      "square|max|maximum|minimum|min|so|but|if|when|then|plus|minus|approx|approximately|about|ca"))
                        unit = !strcmp(unit, "feet") ? "foot" : !strcmp(unit, "inches") ? "inch"
                               : !strcmp(unit, "centimeters") ? "centimeter" : !strcmp(unit, "millimeters") ? "millimeter"
                               : !strcmp(unit, "meters") ? "meter" : "yard";
                    if (unit) {
                        size_t vl = strlen(v);
                        (void)snprintf(v + vl, sizeof(v) - vl, "|w:%s", unit);
                        end = ue;
                    }
                }
                return say_span(in, out, end, v, "dimension-by");
            }
        }
    }
    /* report.pdf report dot p d f, my-file.csv my dash file dot c s v, store12.txt store twelve
       dot t x t: a file name with a known extension reads in full; the body in words (letters
       without a vowel or a known word are spelled), the separators by name and the extension by
       letter names or its set word (zip, jpeg). Earlier parts joined by _ are read by the core
       (project-owner decision 2026-10-22; Was: only after an underscore, report.pdf as written). */
    if (ascii_letter_char(t[p]) || ascii_digit_char(t[p])) {
        size_t z = p, x, y;
        const char *ext_say = NULL;
        if (p && (ascii_letter_char(t[p - 1]) || ascii_digit_char(t[p - 1]) || t[p - 1] == '-' || t[p - 1] == '.' ||
                  t[p - 1] == '_' || t[p - 1] == '/' || t[p - 1] == '@'))
            goto no_file;
        while (z < n && (ascii_letter_char(t[z]) || ascii_digit_char(t[z]) || t[z] == '-' || t[z] == '_') && z - p < 60)
            ++z;
        /* draft_v7(2).docx: a copy number in brackets before the extension */
        if (z + 3 < n && z > p && t[z] == '(' && ascii_digit_char(t[z + 1])) {
            size_t zz = z + 1;
            while (zz < n && ascii_digit_char(t[zz]) && zz - z < 4)
                ++zz;
            if (zz + 1 < n && t[zz] == ')' && t[zz + 1] == '.')
                z = zz + 1;
        }
        if (z == p || t[z - 1] == '-' || t[z - 1] == '_' || !(z + 2 < n && t[z] == '.'))
            goto no_file;
        y = z;
        {
            char vb[160];
            size_t vl = 0, q2 = p;
            vb[0] = 0;
            /* the extension (two chained: tar.gz) */
            for (;;) {
                char ek[8];
                size_t k3;
                for (x = y + 1; x < n && (ascii_letter_char(t[x]) || ascii_digit_char(t[x])) && x - y <= 5; ++x)
                    ;
                if (x - y - 1 < 2 || x - y - 1 > 4)
                    goto no_file;
                for (k3 = 0; k3 < x - y - 1; ++k3)
                    ek[k3] = (char)(t[y + 1 + k3] >= 'A' && t[y + 1 + k3] <= 'Z' ? t[y + 1 + k3] - 'A' + 'a' : t[y + 1 + k3]);
                ek[k3] = 0;
                ext_say = res(in->engine, "file_extension", ek);
                if (!ext_say)
                    goto no_file;
                {
                    /* all lowercase or all capitals (REPORT.PDF), not a capitalized word (it.Png) */
                    int up = 0, low = 0;
                    for (k3 = y + 1; k3 < x; ++k3)
                        up += t[k3] >= 'A' && t[k3] <= 'Z', low += t[k3] >= 'a' && t[k3] <= 'z';
                    if (up && low)
                        goto no_file;
                }
                if (vl + strlen(ext_say) + 12 >= sizeof(vb))
                    goto no_file;
                vl += (size_t)snprintf(vb + vl, sizeof(vb) - vl, "|w:dot|%s", ext_say);
                if (x + 2 < n && t[x] == '.' && ascii_letter_char(t[x + 1])) {
                    size_t x2;
                    for (x2 = x + 1; x2 < n && ascii_letter_char(t[x2]); ++x2)
                        ;
                    if (!(x2 < n && (ascii_digit_char(t[x2]) || (t[x2] == '.' && x2 + 1 < n && ascii_letter_char(t[x2 + 1]))))) {
                        char ek2[8];
                        if (x2 - x - 1 <= 4 && x2 - x - 1 >= 2) {
                            for (k3 = 0; k3 < x2 - x - 1; ++k3)
                                ek2[k3] = (char)(t[x + 1 + k3] | 0x20);
                            ek2[k3] = 0;
                            if (res(in->engine, "file_extension", ek2)) {
                                y = x;
                                continue;
                            }
                        }
                    }
                }
                break;
            }
            if (x < n && (ascii_letter_char(t[x]) || ascii_digit_char(t[x]) || t[x] == '@' || t[x] == '/' ||
                          (t[x] == '.' && x + 1 < n && (ascii_letter_char(t[x + 1]) || ascii_digit_char(t[x + 1])))))
                goto no_file;
            /* the body: words, dashes, letter runs and numbers */
            v[0] = 0;
            {
                size_t o = 0;
                while (q2 < z) {
                    size_t r3 = q2;
                    char piece[48];
                    const char *item;
                    if (t[q2] == '(' || t[q2] == ')') {
                        ++q2;
                        continue;
                    }
                    if (t[q2] == '-' || t[q2] == '_') {
                        o += (size_t)snprintf(v + o, sizeof(v) - o, "%sw:%s", o ? "|" : "", t[q2] == '-' ? "dash" : "underscore");
                        ++q2;
                        continue;
                    }
                    if (ascii_digit_char(t[q2])) {
                        while (r3 < z && ascii_digit_char(t[r3]))
                            ++r3;
                        item = (t[q2] == '0' && r3 - q2 > 1) || r3 - q2 > 4 ? "d" : r3 - q2 == 4 ? "k" : "c";
                    } else {
                        size_t k4, vowels = 0, upper = 0;
                        while (r3 < z && ascii_letter_char(t[r3]))
                            ++r3;
                        /* a capital starting a word inside (scanA) splits nothing: kept as one run */
                        for (k4 = q2; k4 < r3 && k4 - q2 + 1 < sizeof(piece); ++k4) {
                            piece[k4 - q2] = (char)(t[k4] >= 'A' && t[k4] <= 'Z' ? t[k4] - 'A' + 'a' : t[k4]);
                            vowels += strchr("aeiouy", piece[k4 - q2]) != NULL;
                            upper += t[k4] >= 'A' && t[k4] <= 'Z';
                        }
                        piece[k4 - q2] = 0;
                        /* my-file my dash file: a short common word stays a word in a file name */
                        if (res(in->engine, "electronic_word", piece) ||
                            in_list(piece, "my|me|to|go|do|so|up|in|on|of|at|it|is|be|we|no|by|or|an|as|if|us|hi"))
                            item = "w";
                        /* cam3 cam three: three small letters consonant-vowel-consonant read as a word;
                           nba, gtfs, tmpl and capitals (CPI) are spelled */
                        else if (r3 - q2 == 3 && !upper && !strchr("aeiou", piece[0]) && strchr("aeiou", piece[1]) &&
                                 !strchr("aeiouy", piece[2]))
                            item = "w";
                        else if (r3 - q2 <= 3 || !vowels || (upper == r3 - q2 && r3 - q2 <= 4))
                            item = "s";
                        else
                            item = "w";
                        if (item[0] == 'w') {
                            o += (size_t)snprintf(v + o, sizeof(v) - o, "%sw:%s", o ? "|" : "", piece);
                            q2 = r3;
                            if (o >= sizeof(v))
                                goto no_file;
                            continue;
                        }
                    }
                    o += (size_t)snprintf(v + o, sizeof(v) - o, "%s%s:%.*s", o ? "|" : "", item, (int)(r3 - q2), t + q2);
                    if (o >= sizeof(v))
                        goto no_file;
                    q2 = r3;
                }
                if (o + vl + 1 >= sizeof(v))
                    goto no_file;
                memcpy(v + o, vb, vl + 1);
            }
            return say_span(in, out, x, v, "file-name");
        }
    }
no_file:
    /* $0.99 ninety nine cents; $2.99/mo two dollars and ninety nine cents a month; a $25 deposit
       twenty five dollar; a $3.2B transit budget three point two billion dollar */
    if ((t[p] == '$') && p + 1 < n && ascii_digit_char(t[p + 1]) && !(p && ascii_letter_char(t[p - 1]))) {
        size_t a = p + 1, ip = 0, dot = 0;
        char ints[24] = "", frac[8] = "";
        const char *scale = NULL;
        for (q = a; q < n && (ascii_digit_char(t[q]) || (t[q] == ',' && q + 3 < n && ascii_digit_char(t[q + 1]) &&
                                                          ascii_digit_char(t[q + 2]) && ascii_digit_char(t[q + 3])));
             ++q)
            if (t[q] != ',' && ip + 1 < sizeof(ints))
                ints[ip++] = t[q];
        ints[ip] = 0;
        if (q + 1 < n && t[q] == '.' && ascii_digit_char(t[q + 1])) {
            dot = q;
            for (r = q + 1, k = 0; r < n && ascii_digit_char(t[r]) && k + 1 < sizeof(frac); ++r)
                frac[k++] = t[r];
            frac[k] = 0;
            q = r;
        }
        r = q;
        if (q < n && (t[q] == 'M' || t[q] == 'B' || t[q] == 'K') && !(q + 1 < n && ascii_letter_char(t[q + 1])))
            scale = t[q] == 'M' ? "million" : t[q] == 'B' ? "billion" : "thousand", r = q + 1;
        else if (q + 1 < n && t[q] == ' ' && word_at(t, n, q + 1, key, sizeof(key)) &&
                 in_list(key, "million|billion|trillion|thousand"))
            scale = key[0] == 'm' ? "million" : key[0] == 'b' ? "billion" : key[0] == 't' && key[1] == 'r' ? "trillion"
                                                                                                  : "thousand",
            r = q + 1 + strlen(key);
        if (ip && ip < 16) {
            /* $0.99: cents only */
            if (!scale && !strcmp(ints, "0") && dot && strlen(frac) == 2 && strcmp(frac, "00") &&
                !(r < n && (t[r] == '/' || ascii_letter_char(t[r])))) {
                unsigned c = (unsigned)atoi(frac);
                (void)snprintf(v, sizeof(v), "c:%u|w:%s", c, c == 1 ? "cent" : "cents");
                return say_span(in, out, r, v, "cents");
            }
            /* $2.99/mo: a price per period */
            if (!scale && r + 1 < n && t[r] == '/') {
                size_t z = word_at(t, n, r + 1, key, sizeof(key));
                const char *per = !z ? NULL
                                  : in_list(key, "mo|month") ? "a month"
                                  : in_list(key, "yr|year") ? "a year"
                                  : in_list(key, "hr|hour") ? "an hour"
                                  : in_list(key, "wk|week") ? "a week"
                                  : in_list(key, "day") ? "a day" : NULL;
                if (per) {
                    unsigned c = dot ? (unsigned)atoi(frac) : 0;
                    if (dot && strlen(frac) != 2)
                        return 0;
                    if (c)
                        (void)snprintf(v, sizeof(v), "n:%s|w:%s and|c:%u|w:%s %s", ints,
                                       strcmp(ints, "1") ? "dollars" : "dollar", c, c == 1 ? "cent" : "cents", per);
                    else
                        (void)snprintf(v, sizeof(v), "n:%s|w:%s %s", ints, strcmp(ints, "1") ? "dollars" : "dollar", per);
                    return say_span(in, out, r + 1 + z, v, "price-per");
                }
            }
            /* a $25 deposit, a $45 million extension: an amount before a noun is singular */
            if ((!dot || scale) && r + 1 < n && t[r] == ' ' && word_before(t, p, "a|an|the|this|that|each|every|another|one|its|their|our|your|his|her|my") &&
                word_at(t, n, r + 1, key, sizeof(key)) && t[r + 1] >= 'a' && t[r + 1] <= 'z' &&
                !in_list(key, "of|for|in|on|at|to|and|or|but|per|each|a|an|the|from|with|by|is|was|are|were|be|"
                               "will|would|can|could|has|have|had|if|than|then|so|as|plus|off|more|less|worth|"
                               "apiece|total|now|today|back|after|before|when|that|which|who|it|this|less|over|"
                               "under|up|down|out|into|instead|already|only|just|too|also|still|ago|later|a|"
                               "annually|monthly|weekly|daily|cash|or|vs|versus")) {
                (void)snprintf(num, sizeof(num), "%s%s%s", ints, dot ? "." : "", dot ? frac : "");
                (void)snprintf(v, sizeof(v), "n:%s|w:%s%sdollar", num, scale ? scale : "", scale ? " " : "");
                return say_span(in, out, r, v, "money-adjective");
            }
        }
    }
    /* 555-0140, x204 extension two zero four; reached at x31 extension three one: x before an
       extension number in a phone sentence, digit by digit as contract en-ja-dev-v97 reads x2210
       (2026-10-31, TN round 17; was ex two oh four) */
    if (t[p] == 'x' && p >= 2 && p + 1 < n && t[p - 1] == ' ' && ascii_digit_char(t[p + 1])) {
        size_t z = p + 1;
        while (z < n && ascii_digit_char(t[z]))
            ++z;
        if (z - p - 1 <= 4 && !(z < n && (ascii_letter_char(t[z]) || t[z] == '-' ||
                                          ((t[z] == '.' || t[z] == ',') && z + 1 < n && ascii_digit_char(t[z + 1]))))) {
            int comma = p >= 3 && t[p - 2] == ',' && ascii_digit_char(t[p - 3]), cue = comma;
            for (k = 0; !cue && k < n; ++k)
                if ((k == 0 || !ascii_letter_char(t[k - 1])) && word_at(t, n, k, key, sizeof(key)) &&
                    in_list(key, "call|calls|reach|reached|transfer|dial|phone|line|desk|operator|extension|sales|support"))
                    cue = 1;
            if (cue && (comma || word_before(t, p, "at|to|on|dial|press|enter|use"))) {
                (void)snprintf(v, sizeof(v), "w:extension|d:%.*s", (int)(z - p - 1), t + p + 1);
                return say_span(in, out, z, v, "x-extension");
            }
        }
    }
    /* W 125th St west one twenty fifth street: a compass letter before a numbered street (2026-10-31,
       TN round 16; was W one hundred twenty fifth Street) */
    if ((t[p] == 'N' || t[p] == 'S' || t[p] == 'E' || t[p] == 'W') && p + 2 < n && t[p + 1] == ' ' &&
        ascii_digit_char(t[p + 2]) && !(p && (ascii_letter_char(t[p - 1]) || ascii_digit_char(t[p - 1])))) {
        size_t z = p + 2;
        while (z < n && ascii_digit_char(t[z]))
            ++z;
        if (z + 3 < n && (!strncmp(t + z, "st ", 3) || !strncmp(t + z, "nd ", 3) || !strncmp(t + z, "rd ", 3) ||
                          !strncmp(t + z, "th ", 3)) &&
            word_at(t, n, z + 3, key, sizeof(key)) &&
            in_list(key, "St|st|Ave|ave|Street|street|Avenue|avenue|Blvd|blvd|Rd|rd|Road|road|Pl|pl|Dr|dr|Drive|drive"))
            return say_span(in, out, p + 1,
                            t[p] == 'N' ? "w:north" : t[p] == 'S' ? "w:south" : t[p] == 'E' ? "w:east" : "w:west",
                            "street-direction");
    }
    if (ascii_digit_char(t[p]) && !(p && (ascii_digit_char(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ',' ||
                                          t[p - 1] == '/' || t[p - 1] == ':' || ascii_letter_char(t[p - 1])))) {
        size_t a;
        for (a = p; a < n && ascii_digit_char(t[a]); ++a)
            ;
        /* I made 3x the usual amount three times: x after a count before the, as or a comparative word
           (2026-10-31, TN round 19; was three x) */
        if (a - p <= 2 && a + 2 < n && t[a] == 'x' && t[a + 1] == ' ' && word_at(t, n, a + 2, key, sizeof(key)) &&
            in_list(key, "the|as|more|faster|bigger|larger|longer|higher|stronger|cheaper|slower|smaller|less|better")) {
            (void)snprintf(v, sizeof(v), "c:%.*s|w:times", (int)(a - p), t + p);
            return say_span(in, out, a + 1, v, "times-multiplier");
        }
        /* 1-800-FLOWERS one eight hundred flowers, 1-866-BOOK-NOW, 1-800-555-CARE: a toll-free number
           spelled with words (2026-10-31, TN round 17; was one eight hundredFLOWERS) */
        if (a - p == 1 && t[p] == '1' && a + 5 < n && t[a] == '-' && ascii_digit_char(t[a + 1]) &&
            ascii_digit_char(t[a + 2]) && ascii_digit_char(t[a + 3]) && t[a + 4] == '-') {
            size_t z = a + 4, used = (size_t)snprintf(v, sizeof(v), "d:1|%s%.3s", !strncmp(t + a + 1, "800", 3) ? "c:" : "d:", t + a + 1);
            int lettered = 0, fine = 1;
            while (fine && z + 1 < n && t[z] == '-') {
                size_t g = z + 1, e2 = g;
                if (ascii_digit_char(t[e2])) {
                    while (e2 < n && ascii_digit_char(t[e2]))
                        ++e2;
                    used += (size_t)snprintf(v + used, used < sizeof(v) ? sizeof(v) - used : 0, "|d:%.*s", (int)(e2 - g), t + g);
                } else if (t[e2] >= 'A' && t[e2] <= 'Z') {
                    while (e2 < n && t[e2] >= 'A' && t[e2] <= 'Z')
                        ++e2;
                    if (e2 - g < 2 || used + (e2 - g) + 4 >= sizeof(v))
                        fine = 0;
                    else {
                        used += (size_t)snprintf(v + used, sizeof(v) - used, "|w:");
                        for (k = g; k < e2; ++k)
                            v[used++] = (char)(t[k] - 'A' + 'a');
                        v[used] = 0;
                        ++lettered;
                    }
                } else
                    fine = 0;
                if (used >= sizeof(v))
                    fine = 0;
                z = e2;
            }
            if (fine && lettered && !(z < n && (ascii_letter_char(t[z]) || ascii_digit_char(t[z]))))
                return say_span(in, out, z, v, "vanity-phone");
        }
        /* text HOME to 741741 seven four one seven four one: a text short code (2026-10-31, TN round 17;
           was seven hundred forty one thousand ...) */
        if (a - p >= 5 && a - p <= 6 && word_before(t, p, "to") &&
            !(a < n && (ascii_letter_char(t[a]) || ascii_digit_char(t[a]) || t[a] == '-' ||
                        ((t[a] == '.' || t[a] == ',') && a + 1 < n && ascii_digit_char(t[a + 1]))))) {
            int texting = 0;
            for (k = p > 40 ? p - 40 : 0; !texting && k < p; ++k)
                if ((k == 0 || !ascii_letter_char(t[k - 1])) && word_at(t, n, k, key, sizeof(key)) && in_list(key, "text|texting"))
                    texting = 1;
            if (texting) {
                (void)snprintf(v, sizeof(v), "d:%.*s", (int)(a - p), t + p);
                return say_span(in, out, a, v, "text-code");
            }
        }
        /* call or text 988 nine eight eight: a three-digit number right after call, text or dial
           (2026-10-31, TN round 17; was nine hundred eighty eight) */
        if (a - p == 3 && t[p] != '0' && word_before(t, p, "call|text|dial|calling|texting|dialing") &&
            !(a < n && (ascii_letter_char(t[a]) || ascii_digit_char(t[a]) || t[a] == '-' || t[a] == '/' ||
                        ((t[a] == '.' || t[a] == ',') && a + 1 < n && ascii_digit_char(t[a + 1]))))) {
            (void)snprintf(v, sizeof(v), "d:%.3s", t + p);
            return say_span(in, out, a, v, "dial-short");
        }
        /* on a 747 seven forty seven: a Boeing model number (2026-10-31, TN round 17; was seven hundred
           forty seven) */
        if (a - p == 3 && t[p] == '7' && t[p + 2] == '7' && t[p + 1] >= '0' && t[p + 1] <= '8' &&
            word_before(t, p, "a|the|boeing|their|our|this|that|new|old") &&
            !(a < n && (ascii_letter_char(t[a]) || ascii_digit_char(t[a]) || t[a] == '-' || t[a] == '/' ||
                        ((t[a] == '.' || t[a] == ',') && a + 1 < n && ascii_digit_char(t[a + 1]))))) {
            (void)snprintf(v, sizeof(v), "k:%.3s", t + p);
            return say_span(in, out, a, v, "jet-model");
        }
        /* CA 90210 nine oh two one oh: a ZIP code after a state abbreviation or near a mail word
           (2026-10-31, TN round 16; was ninety thousand two hundred ten) */
        if (a - p == 5 && !(a < n && (ascii_letter_char(t[a]) || ascii_digit_char(t[a]) || t[a] == '-' ||
                                      ((t[a] == ',' || t[a] == '.') && a + 1 < n && ascii_digit_char(t[a + 1]))))) {
            int zip = p >= 3 && t[p - 1] == ' ' && t[p - 2] >= 'A' && t[p - 2] <= 'Z' && t[p - 3] >= 'A' &&
                      t[p - 3] <= 'Z' && (p == 3 || !ascii_letter_char(t[p - 4]));
            for (k = p > 80 ? p - 80 : 0; !zip && k < n && k < a + 60; ++k)
                if ((k == 0 || !ascii_letter_char(t[k - 1])) && word_at(t, n, k, key, sizeof(key)) &&
                    in_list(key, "zip|ZIP|Zip|postal|envelope|envelopes|address|addresses|mail|mailing|postcode"))
                    zip = 1;
            if (zip) {
                (void)snprintf(v, sizeof(v), "d:%.5s", t + p);
                return say_span(in, out, a, v, "zip-code");
            }
        }
        /* finished the season 12-3 twelve and three: a win-loss record (2026-10-31, TN round 16; was
           twelve to three) */
        /* went 10-6-1 ten six and one; at 45-37, a 98-64 record, closed the year 52-30 (2026-10-31, TN round 20;
           was forty five to thirty seven, one zero six one) */
        if (a - p <= 2 && a + 1 < n && t[a] == '-' && ascii_digit_char(t[a + 1])) {
            size_t r3;
            for (r = a + 1; r < n && ascii_digit_char(t[r]); ++r)
                ;
            if (r - a - 1 <= 2 && r + 1 < n && t[r] == '-' && ascii_digit_char(t[r + 1]) &&
                word_before(t, p, "went|finished|closed|stands|sits|improved|record|season|year")) {
                for (r3 = r + 1; r3 < n && ascii_digit_char(t[r3]); ++r3)
                    ;
                if (r3 - r - 1 <= 2 && !(r3 < n && (ascii_letter_char(t[r3]) || ascii_digit_char(t[r3]) || t[r3] == '-'))) {
                    (void)snprintf(v, sizeof(v), "c:%.*s|c:%.*s|w:and|c:%.*s", (int)(a - p), t + p, (int)(r - a - 1), t + a + 1,
                                   (int)(r3 - r - 1), t + r + 1);
                    return say_span(in, out, r3, v, "win-loss");
                }
            }
        }
        if (a - p <= 2 && a + 1 < n && t[a] == '-' && ascii_digit_char(t[a + 1]) &&
            (word_before(t, p, "season|record|went|finished|closed|stands|sits|improved|year") ||
             (word_before(t, p, "at") && strncmp(t + p, t + a + 1, a - p)) ||
             (a + 9 < n && !strncmp(t + a + 1 + strspn(t + a + 1, "0123456789"), " record", 7)))) {
            for (r = a + 1; r < n && ascii_digit_char(t[r]); ++r)
                ;
            if (r - a - 1 <= 2 && !(r < n && (ascii_letter_char(t[r]) || ascii_digit_char(t[r]) || t[r] == '-' || t[r] == '%' ||
                                              (r + 1 < n && t[r] == ' ' && word_at(t, n, r + 1, key, sizeof(key)) &&
                                               in_list(key, "percent|degrees|points|years|days|hours|minutes|am|pm"))))) {
                (void)snprintf(v, sizeof(v), "c:%.*s|w:and|c:%.*s", (int)(a - p), t + p, (int)(r - a - 1), t + a + 1);
                return say_span(in, out, r, v, "win-loss");
            }
        }
        /* 9:00am-12:00pm nine a m to twelve p m, 10 p.m.-6 a.m. ten p m to six a m: two clock times,
           each with its meridiem, joined by a hyphen or an en dash (2026-10-31, TN round 15; was nine
           zero zero a m, ten p.m.minus six) */
        {
            size_t z = p, h0, used = 0;
            unsigned hh, mm;
            int side, dotted = 0;
            for (side = 0; side < 2; ++side) {
                char mer;
                for (h0 = z; z < n && ascii_digit_char(t[z]); ++z)
                    ;
                if (z == h0 || z - h0 > 2)
                    break;
                hh = (unsigned)atoi(t + h0);
                mm = 0;
                if (z + 2 < n && t[z] == ':' && ascii_digit_char(t[z + 1]) && ascii_digit_char(t[z + 2])) {
                    mm = (unsigned)atoi(t + z + 1);
                    z += 3;
                }
                if (z < n && t[z] == ' ')
                    ++z;
                if (hh < 1 || hh > 12 || mm > 59 || z + 1 >= n || ((t[z] | 0x20) != 'a' && (t[z] | 0x20) != 'p'))
                    break;
                mer = (char)(t[z] | 0x20);
                if ((t[z + 1] | 0x20) == 'm')
                    dotted = 0, z += 2;
                else if (z + 3 < n && t[z + 1] == '.' && (t[z + 2] | 0x20) == 'm' && t[z + 3] == '.')
                    dotted = 1, z += 4;
                else
                    break;
                if (used >= sizeof(v))
                    break;
                if (mm)
                    used += (size_t)snprintf(v + used, sizeof(v) - used, "%sc:%u|%sc:%u|w:%c m", side ? "|w:to|" : "", hh,
                                             mm < 10 ? "w:oh|" : "", mm, mer);
                else
                    used += (size_t)snprintf(v + used, sizeof(v) - used, "%sc:%u|w:%c m", side ? "|w:to|" : "", hh, mer);
                if (side == 0) {
                    if (z < n && t[z] == '-')
                        ++z;
                    else if (z + 3 <= n && !memcmp(t + z, "\xe2\x80\x93", 3))
                        z += 3;
                    else
                        break;
                    if (!(z < n && ascii_digit_char(t[z])))
                        break;
                }
            }
            if (side == 2 && used < sizeof(v) && !(z < n && (ascii_letter_char(t[z]) || ascii_digit_char(t[z])))) {
                /* at 6 a.m. The: the last dot of a final meridiem is the full stop */
                if (dotted) {
                    size_t r2 = z;
                    while (r2 < n && t[r2] == ' ')
                        ++r2;
                    if (r2 == n || t[r2] == '\n' || (r2 > z && t[r2] >= 'A' && t[r2] <= 'Z'))
                        --z;
                }
                return say_span(in, out, z, v, "clock-range");
            }
        }
        /* a 3.2 lb package three point two pound: a decimal measure before a noun */
        if (a + 4 < n && t[a] == '.' && ascii_digit_char(t[a + 1]) && word_before(t, p, "a|an|the|this|that|each|every|one|your|my")) {
            size_t f = a + 1, z;
            char raw[8], noun[24];
            const char *sing;
            while (f < n && ascii_digit_char(t[f]))
                ++f;
            if (f + 2 < n && t[f] == ' ' && (z = word_at(t, n, f + 1, raw, sizeof(raw))) != 0) {
                (void)snprintf(raw, sizeof(raw), "%.*s", (int)z, t + f + 1);
                sing = !strcmp(raw, "lb") ? "pound" : !strcmp(raw, "L") ? "liter" : !strcmp(raw, "oz") ? "ounce"
                       : !strcmp(raw, "kg") ? "kilogram" : !strcmp(raw, "mi") ? "mile" : !strcmp(raw, "km") ? "kilometer"
                       : !strcmp(raw, "gal") ? "gallon" : !strcmp(raw, "ft") ? "foot" : !strcmp(raw, "in") ? "inch" : NULL;
                if (sing && f + 2 + z < n && t[f + 1 + z] == ' ' && word_at(t, n, f + 2 + z, noun, sizeof(noun)) &&
                    t[f + 2 + z] >= 'a' && t[f + 2 + z] <= 'z' &&
                    !in_list(noun, "of|for|in|on|at|to|and|or|per|is|was|are|were|from|with|by|away|long|high|wide|"
                                   "deep|tall|ago|later|each|apart|left|more|less|than|a|an|the")) {
                    (void)snprintf(v, sizeof(v), "n:%.*s|w:%s", (int)(f - p), t + p, sing);
                    return say_span(in, out, f + 1 + z, v, "measure-adjective");
                }
            }
        }
        /* 9/12 September twelfth (a date word before it), 10/27 October twenty seventh (a day after
           the twelfth); 7/16 is less than 1/2 stays a fraction; 9/11 nine eleven */
        if (a - p <= 2 && a + 1 < n && t[a] == '/' && ascii_digit_char(t[a + 1])) {
            unsigned m = (unsigned)atoi(t + p), d = (unsigned)atoi(t + a + 1);
            for (r = a + 1; r < n && ascii_digit_char(t[r]); ++r)
                ;
            if (r - a - 1 <= 2 && !(r < n && (ascii_digit_char(t[r]) || t[r] == '/' || ascii_letter_char(t[r]) ||
                                               (t[r] == '.' && r + 1 < n && ascii_digit_char(t[r + 1]))))) {
                int math = 0, date_range = 0;
                unsigned range_m = 0, range_d = 0;
                size_t range_end = 0;
                char w2[16];
                /* exp 09/31 September twenty thirty one: a card expiry month and two-digit year after
                   exp, expires or thru, or where the day cannot be one (project-owner decision
                   2026-10-22: 00-49 are the 2000s) */
                {
                    static const unsigned mdays[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                    size_t zc = p;
                    while (zc && (t[zc - 1] == ' ' || t[zc - 1] == ':' || t[zc - 1] == '.'))
                        --zc;
                    if (a - p == 2 && r - a - 1 == 2 && m >= 1 && m <= 12 &&
                        (word_before(t, zc, "exp|expires|expiry|expiration|thru|expiring") || d > mdays[m - 1] ||
                         (d == 0 && t[a + 1] == '0'))) {
                        (void)snprintf(v, sizeof(v), "w:%s|y:%u", dev_month[m - 1], d + (d <= 49 ? 2000 : 1900));
                        return say_span(in, out, r, v, "month-year");
                    }
                }
                /* 9/1 – 9/30, 09/01-09/30: a second month and day after a dash or to (listening
                   check, 2026-10-22) */
                {
                    size_t z = r, y;
                    while (z < n && t[z] == ' ')
                        ++z;
                    if (z + 2 < n && !memcmp(t + z, "\xe2\x80\x93", 3))
                        z += 3;
                    else if (z < n && t[z] == '-')
                        ++z;
                    else if (z + 2 < n && !memcmp(t + z, "to ", 3))
                        z += 2;
                    else
                        z = r;
                    for (y = z; z > r && y < n && t[y] == ' '; ++y)
                        ;
                    if (z > r && y < n && ascii_digit_char(t[y])) {
                        unsigned m2 = (unsigned)atoi(t + y), d2;
                        while (y < n && ascii_digit_char(t[y]))
                            ++y;
                        if (y + 1 < n && t[y] == '/' && ascii_digit_char(t[y + 1])) {
                            size_t y2 = y + 1;
                            d2 = (unsigned)atoi(t + y + 1);
                            while (y2 < n && ascii_digit_char(t[y2]))
                                ++y2;
                            date_range = m2 >= 1 && m2 <= 12 && d2 >= 1 && d2 <= 31 && y2 - y - 1 <= 2 &&
                                         !(y2 < n && (ascii_digit_char(t[y2]) || t[y2] == '/')) &&
                                         (m2 > m || (m2 == m && d2 > d)) && (t[p] == '0' || d > 12 || d2 > 12);
                            range_m = m2, range_d = d2, range_end = y2;
                        }
                    }
                }
                /* a comparison or arithmetic word nearby keeps the fraction */
                for (k = p > 40 ? p - 40 : 0; k < r + 30 && k < n; ++k)
                    if (k == 0 || !ascii_letter_char(t[k - 1]))
                        if (word_at(t, n, k, w2, sizeof(w2)) &&
                            in_list(w2, "less|greater|than|equals|equal|plus|minus|times|fraction|fractions|"
                                        "simplify|compare|sum|halves|thirds|quarters|ratio|odds"))
                            math = 1;
                for (k = p; k < r; ++k)
                    ;
                /* on 1/2 lb of beef one half pound: a fraction before a unit is never a date (2026-10-31,
                   TN round 19; was January second) */
                if (word_at(t, n, r < n && t[r] == ' ' ? r + 1 : r, w2, sizeof(w2)) &&
                    in_list(w2, "cup|cups|tsp|tbsp|teaspoon|teaspoons|tablespoon|tablespoons|lb|lbs|pound|pounds|oz|"
                                "ounce|ounces|inch|inches|stick|sticks|gallon|gallons|quart|quarts|qt|pint|pints")) {
                    static const char *const abbr[][2] = {{"tsp", "teaspoon"}, {"tbsp", "tablespoon"}, {"lb", "pound"},
                                                          {"lbs", "pound"}, {"oz", "ounce"}, {"qt", "quart"}};
                    math = 1;
                    /* 1/2 lb one half pound, 3/4 tsp three quarters teaspoon: the abbreviation is read with it */
                    for (size_t ai = 0; ai < sizeof(abbr) / sizeof(*abbr); ++ai)
                        if (!strcmp(w2, abbr[ai][0]) && m < d && (d == 2 || d == 3 || d == 4 || d == 8 || d == 16)) {
                            const char *den = d == 2 ? "half" : d == 3 ? "third" : d == 4 ? "quarter" : d == 8 ? "eighth" : "sixteenth";
                            (void)snprintf(v, sizeof(v), "c:%u|w:%s%s %s", m, den, m > 1 ? "s" : "", abbr[ai][1]);
                            return say_span(in, out, r + 1 + strlen(w2), v, "fraction-unit");
                        }
                }
                /* Page 1/2 page one of two (2026-10-31, TN round 19; was page one half) */
                if (word_before(t, p, "page|pages") && !(r < n && (t[r] == '/' || ascii_digit_char(t[r])))) {
                    (void)snprintf(v, sizeof(v), "c:%u|w:of|c:%u", m, d);
                    return say_span(in, out, r, v, "page-of");
                }
                if (m == 9 && d == 11 && !math)
                    return say_span(in, out, r, "w:nine eleven", "nine-eleven");
                /* Show that 7/16 is less than 1/2: a fraction where a date could be read */
                if (math && m < d && d <= 16) {
                    (void)snprintf(out->value, sizeof(out->value), "%u", m);
                    (void)snprintf(out->secondary, sizeof(out->secondary), "%u", d);
                    (void)snprintf(out->rule, sizeof(out->rule), "en.math-fraction");
                    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
                    out->unit[0] = 0;
                    out->type = TTF_SEM_FRACTION;
                    out->input_end = r;
                    return tokens_until(in, r);
                }
                /* Tuesday, 3/4 Tuesday March fourth: a weekday before M/D names the date (owner set en-US-0062;
                   2026-10-31, TN round 19; was Tuesday, three quarters) */
                if (m >= 1 && m <= 12 && d >= 1 && d <= 31 && !math &&
                    (d > 12 || date_range || (t[p] == '0' && a - p == 2) ||
                     word_before(t, p, "monday|tuesday|wednesday|thursday|friday|saturday|sunday") ||
                     (p >= 2 && t[p - 1] == ' ' && t[p - 2] == ',' &&
                      word_before(t, p - 2, "monday|tuesday|wednesday|thursday|friday|saturday|sunday")) ||
                     word_before(t, p, "birthday|on|until|through|since|due|deadline|expires|ends|opens|"
                                                 "closes|thru") ||
                     (!(m < d && (d == 2 || d == 3 || d == 4 || d == 8 || d == 16)) &&
                      word_before(t, p, "by|before|after|from|starting|is|was|for")))) {
                    (void)snprintf(v, sizeof(v), "w:%s|o:%u", dev_month[m - 1], d);
                    if (date_range) {
                        size_t l = strlen(v);
                        (void)snprintf(v + l, sizeof(v) - l, "|w:to|w:%s|o:%u", dev_month[range_m - 1], range_d);
                        return say_span(in, out, range_end, v, "slash-date");
                    }
                    /* 6/21-24 June twenty first to twenty fourth */
                    if (r + 1 < n && t[r] == '-' && ascii_digit_char(t[r + 1])) {
                        size_t r2;
                        unsigned d2 = (unsigned)atoi(t + r + 1);
                        for (r2 = r + 1; r2 < n && ascii_digit_char(t[r2]); ++r2)
                            ;
                        if (r2 - r - 1 <= 2 && d2 > d && d2 <= 31 && !(r2 < n && (ascii_digit_char(t[r2]) || t[r2] == '/'))) {
                            size_t l = strlen(v);
                            (void)snprintf(v + l, sizeof(v) - l, "|w:to|o:%u", d2);
                            r = r2;
                        }
                    }
                    return say_span(in, out, r, v, "slash-date");
                }
            }
        }
        /* Room 214 two fourteen, Form 1040 ten forty, Boeing 737, Error 404 four oh four, port 8080,
           Biology 101 one oh one, PO Box 3120 thirty one twenty: a label number in pairs */
        if ((a - p == 3 || a - p == 4) && t[p] != '0' &&
            !(a < n && (ascii_digit_char(t[a]) || (t[a] == ',' && a + 1 < n && ascii_digit_char(t[a + 1])) ||
                                                                    (t[a] == '.' && a + 1 < n && ascii_digit_char(t[a + 1])) ||
                                                                    t[a] == '/' || (t[a] == ':' && !(a + 1 < n && t[a + 1] == ' ')) || t[a] == '%'))) {
            int label = word_before(t, p, "room|suite|ste|flight|route|bus|highway|interstate|form|boeing|airbus|"
                                         "model|error|port|exit|platform|track|channel|box|unit|course|ward|"
                                         "terminal|pier|hangar|cabin|car|lot|bay|dock|level|floor") ||
                        (word_before(t, p, "biology|chemistry|physics|history|english|math|economics|psychology|"
                                           "spanish|french|calculus|statistics|sociology|philosophy|algebra|art|music|"
                                           "accounting|marketing|writing|composition|government|geography|anatomy") &&
                         a - p == 3 && t[p] <= '4');
            /* Your train is number 171 one seventy one (listening check, 2026-10-22) */
            if (!label && word_before(t, p, "number")) {
                size_t z = p > 28 ? p - 28 : 0, y;
                for (y = z; !label && y < p; ++y)
                    if (y == 0 || !ascii_letter_char(t[y - 1]))
                        if (word_at(t, n, y, key, sizeof(key)) && in_list(key, "train|flight|bus|route|tram|ferry"))
                            label = 1;
            }
            /* 3015 Broadway, 350 Fifth Avenue: a house number before a street */
            if (!label && a - p == 4 && a + 2 < n && t[a] == ' ' && t[a + 1] >= 'A' && t[a + 1] <= 'Z') {
                size_t z = a + 1, words = 0;
                while (z < n && words < 3) {
                    size_t e2 = z;
                    while (e2 < n && ascii_letter_char(t[e2]))
                        ++e2;
                    if (e2 == z)
                        break;
                    (void)snprintf(key, sizeof(key), "%.*s", (int)(e2 - z < 20 ? e2 - z : 20), t + z);
                    for (k = 0; key[k]; ++k)
                        key[k] = (char)(key[k] >= 'A' && key[k] <= 'Z' ? key[k] - 'A' + 'a' : key[k]);
                    if (in_list(key, "street|st|avenue|ave|road|rd|boulevard|blvd|broadway|drive|dr|lane|ln|way|"
                                     "place|pl|court|ct|parkway|pkwy|highway|hwy|plaza|square|terrace|circle|trail"))
                        label = 1;
                    ++words;
                    z = e2;
                    if (z < n && t[z] == '.')
                        ++z;
                    if (z < n && t[z] == ' ')
                        ++z;
                    else
                        break;
                }
            }
            if (label) {
                (void)snprintf(v, sizeof(v), "k:%.*s", (int)(a - p), t + p);
                return say_span(in, out, a, v, "label-pairs");
            }
        }
        /* card ending in 4417: digits */
        if (a - p == 4 && (word_before(t, p, "in") && p >= 10 && (!memcmp(t + p - 10, "ending in ", 10) ||
                                                                  !memcmp(t + p - 8, "ends in ", 8)))) {
            int card = 0;
            char w3[16];
            for (k = p > 40 ? p - 40 : 0; k < p; ++k)
                if ((k == 0 || !ascii_letter_char(t[k - 1])) && word_at(t, n, k, w3, sizeof(w3)) &&
                    in_list(w3, "card|account|number|phone|cell|ssn|visa|mastercard|amex|debit|credit|acct"))
                    card = 1;
            if (!card)
                goto not_card;
            (void)snprintf(v, sizeof(v), "d:%.*s", 4, t + p);
            return say_span(in, out, a, v, "card-ending");
        }
    not_card:
        /* in 2:31:08 two hours thirty one minutes and eight seconds; 1:18.442 one minute eighteen
           point four four two seconds; 2h 40m two hours forty minutes */
        if (a - p <= 2 && a + 5 < n && t[a] == ':' && ascii_digit_char(t[a + 1]) && ascii_digit_char(t[a + 2]) &&
            t[a + 3] == ':' && ascii_digit_char(t[a + 4]) && ascii_digit_char(t[a + 5]) &&
            !(a + 6 < n && ascii_digit_char(t[a + 6])) &&
            word_before(t, p, "in|of|time|clocked|lasted|record|took|timer|duration|ends|countdown|runs")) {
            unsigned h = (unsigned)atoi(t + p), mi = (unsigned)atoi(t + a + 1), se = (unsigned)atoi(t + a + 4);
            (void)snprintf(v, sizeof(v), "c:%u|w:%s|c:%u|w:%s and|c:%u|w:%s", h, h == 1 ? "hour" : "hours", mi,
                           mi == 1 ? "minute" : "minutes", se, se == 1 ? "second" : "seconds");
            return say_span(in, out, a + 6, v, "duration");
        }
        if (a - p <= 2 && a + 4 < n && t[a] == ':' && ascii_digit_char(t[a + 1]) && ascii_digit_char(t[a + 2]) &&
            t[a + 3] == '.' && ascii_digit_char(t[a + 4])) {
            unsigned mi = (unsigned)atoi(t + p);
            for (r = a + 4; r < n && ascii_digit_char(t[r]); ++r)
                ;
            if (r - a - 4 <= 3 && !(r < n && ascii_letter_char(t[r]))) {
                (void)snprintf(v, sizeof(v), "c:%u|w:%s|n:%.2s.%.*s|w:seconds", mi, mi == 1 ? "minute" : "minutes",
                               t + a + 1, (int)(r - a - 4), t + a + 4);
                return say_span(in, out, r, v, "lap-time");
            }
        }
        if (a - p <= 2 && a + 3 < n && t[a] == 'h' && t[a + 1] == ' ' && ascii_digit_char(t[a + 2])) {
            for (r = a + 2; r < n && ascii_digit_char(t[r]); ++r)
                ;
            if (r < n && t[r] == 'm' && !(r + 1 < n && ascii_letter_char(t[r + 1])) && r - a - 2 <= 2) {
                unsigned h = (unsigned)atoi(t + p), mi = (unsigned)atoi(t + a + 2);
                /* 2h 05m 41s two hours five minutes forty one seconds (2026-10-31, TN round 15) */
                if (r + 3 < n && t[r + 1] == ' ' && ascii_digit_char(t[r + 2])) {
                    size_t z;
                    for (z = r + 2; z < n && ascii_digit_char(t[z]); ++z)
                        ;
                    if (z < n && t[z] == 's' && z - r - 2 <= 2 && !(z + 1 < n && ascii_letter_char(t[z + 1]))) {
                        unsigned se = (unsigned)atoi(t + r + 2);
                        (void)snprintf(v, sizeof(v), "c:%u|w:%s|c:%u|w:%s|c:%u|w:%s", h, h == 1 ? "hour" : "hours", mi,
                                       mi == 1 ? "minute" : "minutes", se, se == 1 ? "second" : "seconds");
                        return say_span(in, out, z + 1, v, "hms-duration");
                    }
                }
                (void)snprintf(v, sizeof(v), "c:%u|w:%s|c:%u|w:%s", h, h == 1 ? "hour" : "hours", mi,
                               mi == 1 ? "minute" : "minutes");
                return say_span(in, out, r + 1, v, "hm-duration");
            }
        }
        /* 45m 20s forty five minutes twenty seconds: minutes and seconds written short (2026-10-31,
           TN round 15; was forty five meters twenties) */
        if (a - p <= 2 && a + 3 < n && t[a] == 'm' && t[a + 1] == ' ' && ascii_digit_char(t[a + 2])) {
            for (r = a + 2; r < n && ascii_digit_char(t[r]); ++r)
                ;
            if (r < n && t[r] == 's' && r - a - 2 <= 2 && !(r + 1 < n && ascii_letter_char(t[r + 1]))) {
                unsigned mi = (unsigned)atoi(t + p), se = (unsigned)atoi(t + a + 2);
                (void)snprintf(v, sizeof(v), "c:%u|w:%s|c:%u|w:%s", mi, mi == 1 ? "minute" : "minutes", se,
                               se == 1 ? "second" : "seconds");
                return say_span(in, out, r + 1, v, "ms-duration");
            }
        }
        /* 10.82s ten point eight two seconds: a decimal before s is seconds, never a decade
           (2026-10-31, TN round 15; was ten point eight two s) */
        if (a - p <= 3 && a + 2 < n && t[a] == '.' && ascii_digit_char(t[a + 1])) {
            for (r = a + 1; r < n && ascii_digit_char(t[r]); ++r)
                ;
            if (r < n && t[r] == 's' && r - a - 1 <= 3 &&
                !(r + 1 < n && (ascii_letter_char(t[r + 1]) || ascii_digit_char(t[r + 1]) || t[r + 1] == '\''))) {
                (void)snprintf(v, sizeof(v), "n:%.*s|w:seconds", (int)(r - p), t + p);
                return say_span(in, out, r + 1, v, "decimal-seconds");
            }
        }
        /* timed out after 30s, hold for 1s: whole seconds right after a duration word; the 30s,
           their 20s, early 1990s and 1s and 0s stay plurals (2026-10-31, TN round 15; was thirties, ones) */
        if (a - p <= 3 && a < n && t[a] == 's' &&
            !(a + 1 < n && (ascii_letter_char(t[a + 1]) || ascii_digit_char(t[a + 1]) || t[a + 1] == '\'')) &&
            word_before(t, p, "after|for|within|every|in|under|took|takes|lasted|lasts|than|only|just") &&
            !(a + 6 < n && (!memcmp(t + a + 1, " and ", 5) || !memcmp(t + a + 1, " or ", 4) || t[a + 1] == '-' ||
                            t[a + 1] == '/'))) {
            unsigned se = (unsigned)atoi(t + p);
            (void)snprintf(v, sizeof(v), "c:%u|w:%s", se, se == 1 ? "second" : "seconds");
            return say_span(in, out, a + 1, v, "whole-seconds");
        }
        /* 213.555.0115: a dotted North American phone number */
        if (a - p == 3 && a + 8 < n && t[a] == '.' && ascii_digit_char(t[a + 1]) && ascii_digit_char(t[a + 2]) &&
            ascii_digit_char(t[a + 3]) && t[a + 4] == '.') {
            for (r = a + 5; r < n && ascii_digit_char(t[r]); ++r)
                ;
            if (r - a - 5 == 4 && !(r < n && (ascii_digit_char(t[r]) || (t[r] == '.' && r + 1 < n && ascii_digit_char(t[r + 1]))))) {
                (void)snprintf(v, sizeof(v), "d:%.3s|d:%.3s|d:%.4s", t + p, t + a + 1, t + a + 5);
                return say_span(in, out, r, v, "dotted-phone");
            }
        }
        /* 14 in of snow fourteen inches: inches before of or a clause end */
        if (a + 3 < n && t[a] == ' ' && t[a + 1] == 'i' && t[a + 2] == 'n' &&
            (a + 3 == n || t[a + 3] == '.' || t[a + 3] == ',' || t[a + 3] == ';' ||
             (a + 6 < n && !memcmp(t + a + 3, " of ", 4)))) {
            unsigned x = (unsigned)atoi(t + p);
            (void)snprintf(v, sizeof(v), "c:%u|w:%s", x, x == 1 ? "inch" : "inches");
            return say_span(in, out, a + 3, v, "inches");
        }
        /* 40-ft forty foot: a hyphenated measure is an adjective */
        if (a + 3 < n && t[a] == '-' && word_at(t, n, a + 1, key, sizeof(key)) &&
            in_list(key, "ft|foot|in|inch|lb|pound|mi|mile|oz|ounce|gal|gallon|yd|yard|km|kilometer|cm|mm|kg|m|"
                         "meter|metre|ton|watt|volt|hour|day|minute|year|month|week") &&
            !(a + 2 + strlen(key) < n && t[a + 1 + strlen(key)] == '-' && ascii_digit_char(t[a + 2 + strlen(key)]))) {
            const char *sing = in_list(key, "ft|foot") ? "foot" : in_list(key, "in|inch") ? "inch"
                               : in_list(key, "lb|pound") ? "pound" : in_list(key, "mi|mile") ? "mile"
                               : in_list(key, "oz|ounce") ? "ounce" : in_list(key, "gal|gallon") ? "gallon"
                               : in_list(key, "yd|yard") ? "yard" : in_list(key, "km|kilometer") ? "kilometer"
                               : !strcmp(key, "cm") ? "centimeter" : !strcmp(key, "mm") ? "millimeter"
                               : !strcmp(key, "kg") ? "kilogram" : in_list(key, "m|meter|metre") ? "meter" : key;
            (void)snprintf(v, sizeof(v), "c:%u|w:%s", (unsigned)atoi(t + p), sing);
            {
                size_t e2 = a + 1 + strlen(key);
                if (e2 + 2 < n && t[e2] == '.' && t[e2 + 1] == ' ' && t[e2 + 2] >= 'a' && t[e2 + 2] <= 'z')
                    ++e2;
                return say_span(in, out, e2, v, "measure-adjective");
            }
        }
        /* the 65W charger sixty five watt, a 12V battery: an attached unit before a noun */
        if (a + 3 < n && (t[a] == 'W' || t[a] == 'V') && t[a + 1] == ' ' && t[a + 2] >= 'a' && t[a + 2] <= 'z' &&
            word_before(t, p, "a|an|the|this|that|each|every|one|new|old")) {
            char noun[24];
            if (word_at(t, n, a + 2, noun, sizeof(noun)) &&
                in_list(noun, "charger|adapter|bulb|battery|motor|speaker|amp|amplifier|heater|fan|light|lamp|system|"
                              "outlet|plug|supply|brick|panel|microwave|blender|drill")) {
                (void)snprintf(v, sizeof(v), "c:%u|w:%s", (unsigned)atoi(t + p), t[a] == 'W' ? "watt" : "volt");
                return say_span(in, out, a + 1, v, "measure-adjective");
            }
        }
        /* the 200 m butterfly, a 2 L bottle: a measure before a noun is singular */
        if (a + 3 < n && t[a] == ' ' && word_before(t, p, "a|an|the|this|that|each|every|one")) {
            size_t z = word_at(t, n, a + 1, key, sizeof(key));
            char noun[24];
            const char *sing = NULL;
            if (z && strlen(key) <= 3) {
                char raw[8];
                (void)snprintf(raw, sizeof(raw), "%.*s", (int)z, t + a + 1);
                sing = !strcmp(raw, "m") ? "meter" : !strcmp(raw, "km") ? "kilometer" : !strcmp(raw, "L") ? "liter"
                       : !strcmp(raw, "ft") ? "foot" : !strcmp(raw, "lb") ? "pound" : !strcmp(raw, "kg") ? "kilogram"
                       : !strcmp(raw, "mi") ? "mile" : !strcmp(raw, "oz") ? "ounce" : !strcmp(raw, "cm") ? "centimeter"
                       : !strcmp(raw, "mm") ? "millimeter" : !strcmp(raw, "gal") ? "gallon" : !strcmp(raw, "mL") ? "milliliter"
                       : !strcmp(raw, "GB") ? "gigabyte" : !strcmp(raw, "TB") ? "terabyte" : NULL;
            }
            if (sing && a + 1 + z + 1 < n && t[a + 1 + z] == ' ' && word_at(t, n, a + 2 + z, noun, sizeof(noun)) &&
                t[a + 2 + z] >= 'a' && t[a + 2 + z] <= 'z' &&
                !in_list(noun, "of|for|in|on|at|to|and|or|per|is|was|are|were|from|with|by|away|long|high|wide|deep|"
                               "tall|away|ago|later|each|apart|left|more|less|than|a|an|the")) {
                (void)snprintf(v, sizeof(v), "c:%u|w:%s", (unsigned)atoi(t + p), sing);
                return say_span(in, out, a + 1 + z, v, "measure-adjective");
            }
        }
    }
    /* '08 oh eight, May '16 May twenty sixteen: a year written with an apostrophe reads as
       written, and in full after a month name, 00-49 in the 2000s and 50-99 in the 1900s
       (project-owner decision 2026-10-22: the rush of '58 in old books is 1858, and back in '08 is
       said oh eight) */
    if ((t[p] == '\'' || (p + 2 < n && (unsigned char)t[p] == 0xE2 && (unsigned char)t[p + 1] == 0x80 &&
                          (unsigned char)t[p + 2] == 0x99)) &&
        !(p && ascii_letter_char(t[p - 1]))) {
        size_t a = p + (t[p] == '\'' ? 1 : 3);
        if (a + 1 < n && ascii_digit_char(t[a]) && ascii_digit_char(t[a + 1]) &&
            !(a + 2 < n && (ascii_digit_char(t[a + 2]) || ascii_letter_char(t[a + 2])))) {
            unsigned y = (unsigned)atoi(t + a);
            size_t e2 = a + 2, k5;
            /* the nation's '10 Best' list: an opening quote closed after a word is no year */
            for (k5 = e2; k5 < n && k5 < e2 + 40 && t[k5] != '\n'; ++k5)
                if (t[k5] == '\'' && ascii_letter_char(t[k5 - 1]) &&
                    !(k5 + 1 < n && ascii_letter_char(t[k5 + 1])) && t[p] == '\'')
                    return 0;
            int month_before;
            {
                size_t zm = p;
                while (zm && t[zm - 1] == ' ')
                    --zm;
                if (zm && t[zm - 1] == '.')
                    --zm;
                month_before = word_before(t, zm, "january|february|march|april|may|june|july|august|september|"
                                                  "october|november|december|jan|feb|mar|apr|jun|jul|aug|sep|"
                                                  "sept|oct|nov|dec");
            }
            if (month_before)
                (void)snprintf(v, sizeof(v), "y:%u", y + (y <= 49 ? 2000 : 1900));
            else if (y < 10)
                (void)snprintf(v, sizeof(v), "w:oh|c:%u", y);
            else
                (void)snprintf(v, sizeof(v), "c:%u", y);
            /* '58-'59 fifty eight to fifty nine */
            if (e2 + 3 < n && t[e2] == '-' && t[e2 + 1] == '\'' && ascii_digit_char(t[e2 + 2]) &&
                ascii_digit_char(t[e2 + 3]) && !(e2 + 4 < n && ascii_digit_char(t[e2 + 4]))) {
                unsigned y2 = (unsigned)atoi(t + e2 + 2);
                size_t l = strlen(v);
                if (month_before)
                    (void)snprintf(v + l, sizeof(v) - l, "|w:to|y:%u", y2 + (y2 <= 49 ? 2000 : 1900));
                else
                    (void)snprintf(v + l, sizeof(v) - l, y2 < 10 ? "|w:to|w:oh|c:%u" : "|w:to|c:%u", y2);
                e2 += 4;
            }
            return say_span(in, out, e2, v, "short-year");
        }
    }
    /* Mon–Fri Monday through Friday, Thurs., Feb. 11 Thursday: a weekday abbreviation */
    if (t[p] >= 'A' && t[p] <= 'Z' && !(p && t[p - 1] == '(')) {
        static const char *const abbr[] = {"Mon", "Monday", "Tue", "Tuesday", "Tues", "Tuesday", "Wed", "Wednesday",
                                           "Thu", "Thursday", "Thur", "Thursday", "Thurs", "Thursday", "Fri", "Friday",
                                           "Sat", "Saturday", "Sun", "Sunday"};
        for (k = 0; k < sizeof(abbr) / sizeof(abbr[0]); k += 2) {
            size_t l = strlen(abbr[k]), z = p + l;
            if (!strncmp(t + p, abbr[k], l) && !(z < n && ascii_letter_char(t[z]))) {
                int dot = z < n && t[z] == '.';
                size_t after = z + dot;
                int dash = after + 1 < n && (t[after] == '-' || ((unsigned char)t[after] == 0xE2 && after + 2 < n &&
                                                                  (unsigned char)t[after + 1] == 0x80 &&
                                                                  (unsigned char)t[after + 2] == 0x93));
                size_t dw = t[after] == '-' ? 1 : 3;
                if (dash) {
                    size_t k2;
                    for (k2 = 0; k2 < sizeof(abbr) / sizeof(abbr[0]); k2 += 2) {
                        size_t l2 = strlen(abbr[k2]), z2 = after + dw + l2;
                        if (!strncmp(t + after + dw, abbr[k2], l2) && !(z2 < n && ascii_letter_char(t[z2]))) {
                            (void)snprintf(v, sizeof(v), "w:%s through %s", abbr[k + 1], abbr[k2 + 1]);
                            return say_span(in, out, z2 + (z2 < n && t[z2] == '.'), v, "weekday-range");
                        }
                    }
                }
                /* Thurs., Feb. 11 / Sat. Oct 3: an abbreviation before a date */
                if (strcmp(abbr[k], abbr[k + 1]) && (dot || (z < n && t[z] == ',')) ) {
                    size_t m = after;
                    if (m < n && t[m] == ',')
                        ++m;
                    if (m < n && t[m] == ' ')
                        ++m;
                    for (k = 0; k < 12; ++k)
                        if (!strncmp(t + m, dev_month[k], 3))
                            break;
                    if (k < 12) {
                        size_t kk;
                        for (kk = 0; kk < sizeof(abbr) / sizeof(abbr[0]); kk += 2)
                            if (!strncmp(t + p, abbr[kk], strlen(abbr[kk])) && p + strlen(abbr[kk]) == z)
                                break;
                        (void)snprintf(v, sizeof(v), "w:%s", abbr[kk + 1]);
                        return say_span(in, out, after, v, "weekday-abbr");
                    }
                }
                break;
            }
        }
    }
    /* Dec. 24–Jan. 1, K–5, A1–A12: a dash between dates, grades and labels reads to */
    if (((unsigned char)t[p] == 0xE2 && p + 2 < n && (unsigned char)t[p + 1] == 0x80 && (unsigned char)t[p + 2] == 0x93) &&
        p && p + 3 < n) {
        size_t z = p + 3;
        int left_digit = ascii_digit_char(t[p - 1]) || (t[p - 1] == '.' && p >= 2 && ascii_digit_char(t[p - 2]));
        for (k = 0; k < 12; ++k)
            if (!strncmp(t + z, dev_month[k], 3))
                break;
        if (left_digit && k < 12)
            return say_span(in, out, z, "w: to ", "date-dash");
        if (p >= 1 && t[p - 1] == 'K' && !(p >= 2 && ascii_letter_char(t[p - 2])) && ascii_digit_char(t[z]))
            return say_span(in, out, z, "w: through ", "grade-dash");
        if (ascii_digit_char(t[p - 1]) && t[z] >= 'A' && t[z] <= 'Z' && z + 1 < n && ascii_digit_char(t[z + 1]))
            return say_span(in, out, z, "w: to ", "label-dash");
    }
    /* PO Box 88 P O box eighty eight, Apt. 5C apartment five C, RR 2 rural route two */
    if (t[p] >= 'A' && t[p] <= 'Z') {
        size_t z = 0;
        const char *word = NULL;
        if (!strncmp(t + p, "PO Box ", 7) || !strncmp(t + p, "P.O. Box ", 9))
            z = p + (t[p + 1] == '.' ? 9 : 7), word = "P O box";
        else if (!strncmp(t + p, "Apt. ", 5) || !strncmp(t + p, "Apt #", 5) || !strncmp(t + p, "Apt ", 4))
            z = p + (t[p + 3] == '.' || t[p + 4] == '#' ? 5 : 4), word = "apartment";
        else if (!strncmp(t + p, "RR ", 3))
            z = p + 3, word = "rural route";
        if (word && z < n && t[z] == '#')
            ++z;
        if (word && z < n && ascii_digit_char(t[z])) {
            size_t a;
            for (a = z; a < n && ascii_digit_char(t[a]); ++a)
                ;
            if (a - z <= 5) {
                char lab[8] = "";
                if (a < n && t[a] >= 'A' && t[a] <= 'Z' && !(a + 1 < n && ascii_letter_char(t[a + 1])))
                    (void)snprintf(lab, sizeof(lab), "|w:%c", t[a]), ++a;
                (void)snprintf(v, sizeof(v), "w:%s|%s:%.*s%s", word, a - z - (lab[0] != 0) >= 3 ? "k" : "c",
                               (int)(a - z - (lab[0] != 0)), t + z, lab);
                return say_span(in, out, a, v, "address-abbr");
            }
        }
    }
    /* x² squared, 2¹⁰ to the tenth: superscript digits after a number, a letter or a bracket */
    if (p && (ascii_digit_char(t[p - 1]) || t[p - 1] == ')' ||
              (ascii_letter_char(t[p - 1]) && !(p >= 2 && ascii_letter_char(t[p - 2]))))) {
        size_t z = p;
        unsigned x = 0, cnt = 0;
        /* 10⁻³ ten to the minus three: a superscript minus before the digits (2026-10-26; was
           ten superscript minus ³) */
        int minus = z + 3 < n && !strncmp(t + z, "\xe2\x81\xbb", 3);
        if (minus)
            z += 3;
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
        if (cnt && !(z < n && (ascii_digit_char(t[z]) || ascii_letter_char(t[z])))) {
            /* written ^N, as the Korean pack writes a power, so a Korean reading of the sentence
               says 제곱 (E=mc² E 이퀄 mc 제곱; 2026-10-26, was the English word spelled
               에스큐유에이알이디) and English says squared, cubed or to the tenth */
            (void)snprintf(v, sizeof(v), "^%s%u", minus ? "-" : "", x);
            (void)say_span(in, out, z, v, "power");
            out->type = TTF_SEM_SYMBOL;
            (void)snprintf(out->unit, sizeof(out->unit), "@expression");
            return tokens_until(in, z);
        }
    }
    /* ¼ tsp one quarter teaspoon, ⅔ cup two thirds cup: a fraction glyph before a recipe unit (2026-10-31,
       TN round 19; was one quarter tsp) */
    if (p + 3 < n && !(p && ascii_digit_char(t[p - 1]))) {
        const char *fw = NULL;
        size_t gw = 0;
        if ((unsigned char)t[p] == 0xC2)
            fw = (unsigned char)t[p + 1] == 0xBD ? "one half" : (unsigned char)t[p + 1] == 0xBC ? "one quarter"
                 : (unsigned char)t[p + 1] == 0xBE ? "three quarter" : NULL, gw = 2;
        else if ((unsigned char)t[p] == 0xE2 && (unsigned char)t[p + 1] == 0x85)
            fw = (unsigned char)t[p + 2] == 0x93 ? "one third" : (unsigned char)t[p + 2] == 0x94 ? "two thirds"
                 : (unsigned char)t[p + 2] == 0x9B ? "one eighth" : NULL, gw = 3;
        if (fw && p + gw + 1 < n && t[p + gw] == ' ' && word_at(t, n, p + gw + 1, key, sizeof(key)) &&
            in_list(key, "tsp|tbsp|lb|oz|cup")) {
            const char *unit = !strcmp(key, "tsp") ? "teaspoon" : !strcmp(key, "tbsp") ? "tablespoon"
                               : !strcmp(key, "lb") ? "pound" : !strcmp(key, "oz") ? "ounce" : "cup";
            (void)snprintf(v, sizeof(v), "w:%s %s", fw, unit);
            return say_span(in, out, p + gw + 1 + strlen(key), v, "glyph-unit");
        }
    }
    /* ½ the group half the group */
    if ((unsigned char)t[p] == 0xC2 && p + 1 < n && (unsigned char)t[p + 1] == 0xBD &&
        (p + 6 < n && (!memcmp(t + p + 2, " the ", 5) || !memcmp(t + p + 2, " a ", 3))))
        return say_span(in, out, p + 2, "w:half", "half-the");
    /* PM2.5 p m two point five: capitals before a decimal number (V1.4 keeps its version word) */
    if (t[p] >= 'A' && t[p] <= 'Z' && !(p && ascii_letter_char(t[p - 1])) && !(t[p] == 'V' && p + 1 < n && ascii_digit_char(t[p + 1]))) {
        size_t z = p, a;
        while (z < n && t[z] >= 'A' && t[z] <= 'Z' && z - p < 3)
            ++z;
        if (z < n && ascii_digit_char(t[z])) {
            for (a = z; a < n && ascii_digit_char(t[a]); ++a)
                ;
            if (a + 1 < n && t[a] == '.' && ascii_digit_char(t[a + 1]) && a - z <= 2) {
                for (r = a + 1; r < n && ascii_digit_char(t[r]); ++r)
                    ;
                if (r - a - 1 <= 2 && !(r < n && (ascii_letter_char(t[r]) || ascii_digit_char(t[r]) ||
                                                   (t[r] == '.' && r + 1 < n && ascii_digit_char(t[r + 1]))))) {
                    (void)snprintf(v, sizeof(v), "s:%.*s|n:%.*s", (int)(z - p), t + p, (int)(r - z), t + z);
                    return say_span(in, out, r, v, "caps-decimal");
                }
            }
        }
    }
    /* 401(k) four oh one k, 403(b) */
    if (p + 6 <= n && ascii_digit_char(t[p]) && ascii_digit_char(t[p + 1]) && ascii_digit_char(t[p + 2]) &&
        t[p + 3] == '(' && (t[p + 4] == 'k' || t[p + 4] == 'b') && t[p + 5] == ')' && !(p && ascii_digit_char(t[p - 1]))) {
        (void)snprintf(v, sizeof(v), "k:%.3s|w:%c", t + p, t[p + 4]);
        return say_span(in, out, p + 6, v, "retirement-plan");
    }
    /* K-12 K through twelve */
    if (t[p] == 'K' && p + 2 < n && t[p + 1] == '-' && ascii_digit_char(t[p + 2]) && !(p && ascii_letter_char(t[p - 1]))) {
        unsigned g = (unsigned)atoi(t + p + 2);
        for (r = p + 2; r < n && ascii_digit_char(t[r]); ++r)
            ;
        if ((g == 5 || g == 6 || g == 8 || g == 12) && !(r < n && (ascii_letter_char(t[r]) || ascii_digit_char(t[r])))) {
            (void)snprintf(v, sizeof(v), "w:K through|c:%u", g);
            return say_span(in, out, r, v, "grade-range");
        }
    }
    /* AAA batteries triple A, AA double A */
    if ((!strncmp(t + p, "AAA batter", 10) || !strncmp(t + p, "AA batter", 9)) && !(p && ascii_letter_char(t[p - 1])))
        return say_span(in, out, p + (t[p + 2] == 'A' ? 3 : 2), t[p + 2] == 'A' ? "w:triple A" : "w:double A", "battery");
    /* Agent 007 double oh seven */
    if (!strncmp(t + p, "007", 3) && !(p + 3 < n && ascii_digit_char(t[p + 3])) && !(p && ascii_digit_char(t[p - 1])) &&
        word_before(t, p, "agent|bond"))
        return say_span(in, out, p + 3, "w:double oh seven", "bond");
    /* Python 3.12 three point twelve: a software version */
    if (ascii_digit_char(t[p]) && !(p && (ascii_digit_char(t[p - 1]) || t[p - 1] == '.')) &&
        word_before(t, p, "python|java|node|ios|android|macos|windows|ubuntu|firefox|chrome|version|php|ruby|perl|"
                          "kotlin|swift|excel|photoshop|unity|blender|django|rails|scratch|xcode|debian|fedora|"
                          "safari|edge|wordpress|bluetooth|usb|hdmi|release|build")) {
        size_t a, parts = 0;
        char *o = v;
        v[0] = 0;
        for (a = p; a < n && parts < 4;) {
            size_t b = a;
            while (b < n && ascii_digit_char(t[b]))
                ++b;
            if (b == a || b - a > 4)
                return 0;
            o += snprintf(o, sizeof(v) - (size_t)(o - v), "%sc:%.*s", parts ? "|w:point|" : "", (int)(b - a), t + a);
            ++parts;
            a = b;
            if (a + 1 < n && t[a] == '.' && ascii_digit_char(t[a + 1]))
                ++a;
            else
                break;
        }
        if (parts >= 2 && !(a < n && (ascii_letter_char(t[a]) || ascii_digit_char(t[a]))))
            return say_span(in, out, a, v, "version");
    }
    /* 85 kt eighty five knots, 120,000 sq ft, 18 ng/mL, 2 million bbl: English-only units
       (numeric_suffix rows are shared by every language) */
    if (ascii_digit_char(t[p]) && !(p && (ascii_digit_char(t[p - 1]) || t[p - 1] == '.' || ascii_letter_char(t[p - 1]) ||
                                          t[p - 1] == '/'))) {
        static const char *const unit[] = {"kt", "knot", "knots", "inHg", "inch of mercury", "inches of mercury",
                                           "nmi", "nautical mile", "nautical miles", "bbl", "barrel", "barrels",
                                           "yds", "yard", "yards", "ppm", "part per million", "parts per million",
                                           "ppb", "part per billion", "parts per billion", "ng/mL",
                                           "nanogram per milliliter", "nanograms per milliliter", "sq ft",
                                           "square foot", "square feet", "sq. ft.", "square foot", "square feet",
                                           "mmHg", "millimeter of mercury", "millimeters of mercury", "yrs", "year", "years",
                                           "wks", "week", "weeks", "bpm", "beat per minute", "beats per minute"};
        size_t a = p, ip = 0;
        char numtxt[32];
        const char *scale = NULL;
        while (a < n && (ascii_digit_char(t[a]) || (t[a] == ',' && a + 3 < n && ascii_digit_char(t[a + 1]) &&
                                                    ascii_digit_char(t[a + 2]) && ascii_digit_char(t[a + 3])) ||
                         (t[a] == '.' && a + 1 < n && ascii_digit_char(t[a + 1]))) && ip + 1 < sizeof(numtxt)) {
            if (t[a] != ',')
                numtxt[ip++] = t[a];
            ++a;
        }
        numtxt[ip] = 0;
        r = a;
        if (r + 1 < n && t[r] == ' ' && word_at(t, n, r + 1, key, sizeof(key)) && in_list(key, "million|billion|thousand"))
            scale = key[0] == 'm' ? "million" : key[0] == 'b' ? "billion" : "thousand", r += 1 + strlen(key);
        if (r < n && t[r] == ' ')
            ++r;
        for (k = 0; k < sizeof(unit) / sizeof(unit[0]); k += 3) {
            size_t l = strlen(unit[k]);
            if (r + l <= n && !strncmp(t + r, unit[k], l) &&
                !(r + l < n && (ascii_letter_char(t[r + l]) || ascii_digit_char(t[r + l]) || t[r + l] == '/')) &&
                !(l == 2 && r == a)) {
                int one = !scale && !strcmp(numtxt, "1");
                (void)snprintf(v, sizeof(v), "n:%s|w:%s%s%s", numtxt, scale ? scale : "", scale ? " " : "",
                               unit[k + (one ? 1 : 2)]);
                return say_span(in, out, r + l, v, "unit-en");
            }
        }
        /* 128/82 mmHg one twenty eight over eighty two millimeters of mercury */
        if (a - p <= 3 && a + 1 < n && t[a] == '/' && ascii_digit_char(t[a + 1])) {
            size_t b2;
            for (b2 = a + 1; b2 < n && ascii_digit_char(t[b2]); ++b2)
                ;
            if (b2 - a - 1 <= 3 && b2 + 5 < n && !strncmp(t + b2, " mmHg", 5) && !(b2 + 5 < n && ascii_letter_char(t[b2 + 5]))) {
                (void)snprintf(v, sizeof(v), "c:%.*s|w:over|c:%.*s|w:millimeters of mercury", (int)(a - p), t + p,
                               (int)(b2 - a - 1), t + a + 1);
                return say_span(in, out, b2 + 5, v, "blood-pressure");
            }
        }
    }
    /* Helena, Montana 59601, Austin, TX 78701: a ZIP code after a state name or a capital state code */
    if (ascii_digit_char(t[p]) && p >= 3 && !(ascii_digit_char(t[p - 1]))) {
        size_t a, q2 = p, w0;
        for (a = p; a < n && ascii_digit_char(t[a]); ++a)
            ;
        while (q2 && (t[q2 - 1] == ' ' || t[q2 - 1] == ','))
            --q2;
        for (w0 = q2; w0 && ascii_letter_char(t[w0 - 1]); --w0)
            ;
        if (a - p == 5 && !(a < n && (ascii_digit_char(t[a]) || ascii_letter_char(t[a]))) && q2 > w0 &&
            ((q2 - w0 == 2 && t[w0] >= 'A' && t[w0] <= 'Z' && t[w0 + 1] >= 'A' && t[w0 + 1] <= 'Z' &&
              strstr("|AL|AK|AZ|AR|CA|CO|CT|DE|FL|GA|HI|ID|IL|IN|IA|KS|KY|LA|ME|MD|MA|MI|MN|MS|MO|MT|NE|NV|NH|NJ|"
                     "NM|NY|NC|ND|OH|OK|OR|PA|RI|SC|SD|TN|TX|UT|VT|VA|WA|WV|WI|WY|DC|",
                     (char[]){'|', t[w0], t[w0 + 1], '|', 0})) ||
             (q2 - w0 > 2 && t[w0] >= 'A' && t[w0] <= 'Z' &&
              word_before(t, p, "alabama|alaska|arizona|arkansas|california|colorado|connecticut|delaware|florida|"
                                "georgia|hawaii|idaho|illinois|indiana|iowa|kansas|kentucky|louisiana|maine|maryland|"
                                "massachusetts|michigan|minnesota|mississippi|missouri|montana|nebraska|nevada|"
                                "hampshire|jersey|mexico|york|carolina|dakota|ohio|oklahoma|oregon|pennsylvania|"
                                "island|tennessee|texas|utah|vermont|virginia|washington|wisconsin|wyoming")))) {
            (void)snprintf(v, sizeof(v), "d:%.5s", t + p);
            return say_span(in, out, a, v, "zip");
        }
    }
    /* Tues. Tuesday, Sept. September: a weekday or month abbreviation with its period alone; 5/8(Fri.)
       keeps the date's weekday mark */
    if (t[p] >= 'A' && t[p] <= 'Z' && !(p && (ascii_letter_char(t[p - 1]) || t[p - 1] == '('))) {
        static const char *const full[] = {"Mon", "Monday", "Tues", "Tuesday", "Tue", "Tuesday", "Wed", "Wednesday",
                                           "Thurs", "Thursday", "Thur", "Thursday", "Thu", "Thursday", "Fri", "Friday",
                                           "Sat", "Saturday", "Jan", "January", "Feb", "February", "Mar", "March",
                                           "Apr", "April", "Aug", "August", "Sept", "September", "Sep", "September",
                                           "Oct", "October", "Nov", "November", "Dec", "December"};
        for (k = 0; k < sizeof(full) / sizeof(full[0]); k += 2) {
            size_t l = strlen(full[k]);
            if (!strncmp(t + p, full[k], l) && p + l < n && t[p + l] == '.' &&
                !(p + l + 2 < n && t[p + l + 1] == ' ' && ascii_digit_char(t[p + l + 2])) &&
                /* Mar.19, 2009: a month before its day; weekdays stay before a number */
                !(p + l + 1 < n && ascii_digit_char(t[p + l + 1]) && k < 18)) {
                (void)snprintf(v, sizeof(v), "w:%s%s", full[k + 1],
                               p + l + 1 < n && ascii_digit_char(t[p + l + 1]) ? " " : "");
                return say_span(in, out, p + l + 1 - (p + l + 1 == n || t[p + l + 1] == '\n' ||
                                                       (p + l + 2 < n && t[p + l + 1] == ' ' && t[p + l + 2] >= 'A' &&
                                                        t[p + l + 2] <= 'Z')),
                                v, "abbr-period");
            }
        }
    }
    /* 4th St fourth Street, 5th Ave fifth Avenue */
    if (ascii_digit_char(t[p]) && !(p && (ascii_digit_char(t[p - 1]) || ascii_letter_char(t[p - 1])))) {
        size_t a;
        for (a = p; a < n && ascii_digit_char(t[a]); ++a)
            ;
        if (a + 5 < n && (!strncmp(t + a, "th ", 3) || !strncmp(t + a, "st ", 3) || !strncmp(t + a, "nd ", 3) ||
                          !strncmp(t + a, "rd ", 3)) &&
            (!strncmp(t + a + 3, "St", 2) || !strncmp(t + a + 3, "Ave", 3)) &&
            !(a + 5 < n && ascii_letter_char(t[a + 3 + (t[a + 3] == 'S' ? 2 : 3)]))) {
            size_t z = a + 3 + (t[a + 3] == 'S' ? 2 : 3);
            (void)snprintf(v, sizeof(v), "o:%.*s|w:%s", (int)(a - p), t + p, t[a + 3] == 'S' ? "Street" : "Avenue");
            return say_span(in, out, z + (z < n && t[z] == '.'), v, "street-ordinal");
        }
        /* 3/$1 three for a dollar, 2/$5 two for five dollars */
        if (a - p <= 2 && a + 2 < n && t[a] == '/' && t[a + 1] == '$' && ascii_digit_char(t[a + 2])) {
            size_t z;
            for (z = a + 2; z < n && ascii_digit_char(t[z]); ++z)
                ;
            if (!(z < n && (t[z] == '.' || ascii_digit_char(t[z])))) {
                unsigned d2 = (unsigned)atoi(t + a + 2);
                if (d2 == 1)
                    (void)snprintf(v, sizeof(v), "c:%u|w:for a dollar", (unsigned)atoi(t + p));
                else
                    (void)snprintf(v, sizeof(v), "c:%u|w:for|c:%u|w:dollars", (unsigned)atoi(t + p), d2);
                return say_span(in, out, z, v, "multi-price");
            }
        }
        /* 1099-NEC ten ninety nine N E C, 529 plan five twenty nine plan: a form or plan number */
        if ((a - p == 3 || a - p == 4) && t[p] != '0' &&
            ((a + 2 < n && t[a] == '-' && t[a + 1] >= 'A' && t[a + 1] <= 'Z' && t[a + 2] >= 'A' && t[a + 2] <= 'Z') ||
             (a + 5 <= n && !strncmp(t + a, " plan", 5) && !(a + 5 < n && ascii_letter_char(t[a + 5]))))) {
            (void)snprintf(v, sizeof(v), "k:%.*s", (int)(a - p), t + p);
            return say_span(in, out, a, v, "label-pairs");
        }
        /* 988 crisis line, call 211: a three-digit service number is read digit by digit */
        if (a - p == 3 && (!strncmp(t + p, "988", 3) || (t[p + 1] == '1' && t[p + 2] == '1')) &&
            !(a < n && (ascii_digit_char(t[a]) || ascii_letter_char(t[a]) || t[a] == '.' || t[a] == ','  ? (t[a] == ',' ? 0 : 1) : 0))) {
            int phone = 0;
            char w3[16];
            for (k = p > 40 ? p - 40 : 0; k < p; ++k)
                if ((k == 0 || !ascii_letter_char(t[k - 1])) && word_at(t, n, k, w3, sizeof(w3)) &&
                    in_list(w3, "call|dial|text|line|hotline|lifeline|number|crisis"))
                    phone = 1;
            if (phone) {
                (void)snprintf(v, sizeof(v), "d:%.3s", t + p);
                return say_span(in, out, a, v, "service-number");
            }
        }
        /* .312 hitting three twelve (a batting average) */
    }
    if (t[p] == '.' && p + 3 < n && ascii_digit_char(t[p + 1]) && ascii_digit_char(t[p + 2]) && ascii_digit_char(t[p + 3]) &&
        !(p + 4 < n && ascii_digit_char(t[p + 4])) && !(p && (ascii_digit_char(t[p - 1]) || ascii_letter_char(t[p - 1]))) &&
        word_before(t, p, "hitting|batting|batted|hit|average|avg|hits|at|of|just|above|below|over|under")) {
        int baseball = 0;
        char w3[16];
        /* hit just .250, finished above .300 in the league (2026-10-31, TN round 20; was zero point two five zero) */
        for (k = p > 50 ? p - 50 : 0; k < p; ++k)
            if ((k == 0 || !ascii_letter_char(t[k - 1])) && word_at(t, n, k, w3, sizeof(w3)) &&
                in_list(w3, "hitting|batting|batted|average|avg|hits|hit|hitter|season|plate|league|rookie|catcher|"
                            "shortstop|pitcher|batter|slugger|lineup|inning|innings"))
                baseball = 1;
        if (baseball) {
            (void)snprintf(v, sizeof(v), "k:%.3s", t + p + 1);
            return say_span(in, out, p + 4, v, "batting-average");
        }
    }
    /* x4417 extension four four one seven */
    if (t[p] == 'x' && p && t[p - 1] == ' ' && p + 3 < n && ascii_digit_char(t[p + 1])) {
        size_t a;
        for (a = p + 1; a < n && ascii_digit_char(t[a]); ++a)
            ;
        if (a - p - 1 >= 3 && a - p - 1 <= 5 && !(a < n && (ascii_letter_char(t[a]) || ascii_digit_char(t[a]) || t[a] == '.'))) {
            (void)snprintf(v, sizeof(v), "w:extension|d:%.*s", (int)(a - p - 1), t + p + 1);
            return say_span(in, out, a, v, "extension");
        }
    }
    /* B2B B to B, 4x100 four by one hundred */
    if (p + 3 <= n && t[p] >= 'A' && t[p] <= 'Z' && t[p + 1] == '2' && p + 2 < n && t[p + 2] >= 'A' && t[p + 2] <= 'Z' &&
        !(p && ascii_letter_char(t[p - 1])) && !(p + 3 < n && (ascii_letter_char(t[p + 3]) || ascii_digit_char(t[p + 3]))) &&
        strstr("|B2B|B2C|C2B|C2C|P2P|D2D|B2G|G2B|G2C|", (char[]){'|', t[p], '2', t[p + 2], '|', 0})) {
        (void)snprintf(v, sizeof(v), "s:%c|w:to|s:%c", t[p], t[p + 2]);
        return say_span(in, out, p + 3, v, "b-to-b");
    }
    if (ascii_digit_char(t[p]) && p + 4 < n && t[p + 1] == 'x' && ascii_digit_char(t[p + 2]) &&
        !(p && (ascii_digit_char(t[p - 1]) || ascii_letter_char(t[p - 1])))) {
        size_t a;
        for (a = p + 2; a < n && ascii_digit_char(t[a]); ++a)
            ;
        if ((!strncmp(t + p + 2, "100", 3) || !strncmp(t + p + 2, "200", 3) || !strncmp(t + p + 2, "400", 3) ||
             !strncmp(t + p + 2, "800", 3)) && a - p - 2 == 3 && !(a < n && (ascii_letter_char(t[a]) || ascii_digit_char(t[a])))) {
            (void)snprintf(v, sizeof(v), "c:%c|w:by|c:%.3s", t[p], t + p + 2);
            return say_span(in, out, a, v, "relay");
        }
        /* 4x100m four by one hundred meter (2026-10-31, TN round 15; was four x one hundred m) */
        if ((!strncmp(t + p + 2, "100", 3) || !strncmp(t + p + 2, "200", 3) || !strncmp(t + p + 2, "400", 3) ||
             !strncmp(t + p + 2, "800", 3)) && a - p - 2 == 3 && a < n && t[a] == 'm' &&
            !(a + 1 < n && (ascii_letter_char(t[a + 1]) || ascii_digit_char(t[a + 1])))) {
            (void)snprintf(v, sizeof(v), "c:%c|w:by|c:%.3s|w:meter", t[p], t + p + 2);
            return say_span(in, out, a + 1, v, "relay");
        }
    }
    return 0;
}


static size_t classify(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const ttf_token_t *t = &in->tokens[in->index];
    char key[96];
    size_t n = t->end - t->begin, j, consumed = 1, end = t->end;
    size_t date_tokens = ttf_en_classify_identifier(in, out);
    const char *kind;
    if (date_tokens)
        return date_tokens;
    if ((date_tokens = owner_forms(in, out)) != 0)
        return date_tokens;
    if ((date_tokens = dev_forms(in, out)) != 0)
        return date_tokens;
    date_tokens = ttf_en_classify_date(in, out);
    if (date_tokens)
        return date_tokens;
    date_tokens = mixed_fraction(in, out);
    if (date_tokens)
        return date_tokens;
    date_tokens = scripture_verse(in, out);
    if (date_tokens)
        return date_tokens;
    date_tokens = count_scale(in, out);
    if (date_tokens)
        return date_tokens;
    date_tokens = per_slash(in, out);
    if (date_tokens)
        return date_tokens;
    date_tokens = fraction_idiom(in, out);
    if (date_tokens)
        return date_tokens;
    date_tokens = fraction_unit(in, out);
    if (date_tokens)
        return date_tokens;
    /* CHAPTER V.: a numbering word before a dotted initial. */
    date_tokens = roman_numeral(in, out, 1);
    if (date_tokens)
        return date_tokens;
    date_tokens = ttf_en_classify_abbreviation(in, out);
    if (date_tokens)
        return date_tokens;
    if (t->script != TTF_SCRIPT_LATIN || n >= sizeof(key))
        return 0;
    date_tokens = roman_numeral(in, out, 0);
    if (date_tokens)
        return date_tokens;
    date_tokens = ampersand_initialism(in, out);
    if (date_tokens)
        return date_tokens;
    date_tokens = hyphen_letters(in, out);
    if (date_tokens)
        return date_tokens;
    /* Treat an adjacent hyphenated word as one lexical unit so the existing
       layered lexicon can choose a pronunciation for the complete spelling.
       An apostrophe after a word with a small letter joins the same way
       (McDonald's, Hold'em).
       Spaces, doubled hyphens and numeric terms are separate grammar. */
    while (in->index + consumed + 1 < in->token_count) {
        const ttf_token_t *separator = &in->tokens[in->index + consumed];
        const ttf_token_t *next = &in->tokens[in->index + consumed + 1];
        if (separator->begin != end || separator->end != separator->begin + 1 ||
            (in->text[separator->begin] != '-' && in->text[separator->begin] != '\'') ||
            next->begin != separator->end || next->script != TTF_SCRIPT_LATIN)
            break;
        /* OSPF's, CW's: a capital abbreviation keeps its possessive apart and is
           spelled; only a word with a small letter joins its apostrophe. */
        if (in->text[separator->begin] == '\'') {
            const ttf_token_t *left = &in->tokens[in->index + consumed - 1];
            size_t k, nn = next->end - next->begin;
            const char *nx = in->text + next->begin;
            /* I'll, I'm, I'd, I've: the pronoun I joins its contraction */
            int pronoun = left->end - left->begin == 1 && in->text[left->begin] == 'I' &&
                          ((nn == 1 && (nx[0] == 'm' || nx[0] == 'd')) ||
                           (nn == 2 && ((nx[0] == 'l' && nx[1] == 'l') || (nx[0] == 'v' && nx[1] == 'e'))));
            for (k = left->begin; k < left->end; ++k)
                if (in->text[k] >= 'a' && in->text[k] <= 'z')
                    break;
            if (k == left->end && !pronoun)
                break;
        }
        /* Bound lookahead as well as the copied key. Otherwise an overlong
           chain revisited from each component would take quadratic time. */
        if (next->end - t->begin >= sizeof(key))
            return 0;
        end = next->end;
        consumed += 2;
    }
    n = end - t->begin;
    if (n >= sizeof(key))
        return 0;
    for (j = 0; j < n; ++j)
        key[j] = ascii_lower(in->text[t->begin + j]);
    key[n] = '\0';
    kind = res(in->engine, "lexical", key);
    out->type = TTF_SEM_WORD;
    out->lexical_class = TTF_LEX_UNKNOWN_LATIN;
    /* IT, AT, BE: an all-uppercase run whose spelling is also an ordinary word
       is an abbreviation under the spelling policy. A lexicon entry still wins
       later, because it is an explicit reading for the term. */
    if (kind && in->engine->initialism_policy == TTF_INITIALISM_SPELL && consumed == 1 && n > 1 &&
        strcmp(kind, "ACRONYM") && strcmp(kind, "INITIALISM") && strcmp(kind, "PROPER_NOUN") &&
        !caps_as_word(in, key, n, consumed)) {
        size_t upper;
        for (upper = 0; upper < n; ++upper)
            if (in->text[t->begin + upper] < 'A' || in->text[t->begin + upper] > 'Z')
                break;
        if (upper == n)
            kind = "INITIALISM";
    }
    if (kind) {
        if (!strcmp(kind, "INITIALISM")) {
            out->type = TTF_SEM_INITIALISM;
            out->lexical_class = TTF_LEX_INITIALISM;
        } else if (!strcmp(kind, "ACRONYM")) {
            out->type = TTF_SEM_ACRONYM;
            out->lexical_class = TTF_LEX_ACRONYM;
        } else if (!strcmp(kind, "PROPER_NOUN"))
            out->lexical_class = TTF_LEX_PROPER_NOUN;
        else
            out->lexical_class = TTF_LEX_WORD;
    } else {
        int all_upper = n > 1;
        for (j = 0; j < n; ++j)
            if (in->text[t->begin + j] < 'A' || in->text[t->begin + j] > 'Z')
                all_upper = 0;
        /* tv, sns, kbs, www in Korean text: a short word with no vowel letter is
           letters, not a word (트브, 스느스 before; 2026-10-03). y counts as a vowel
           except first (sky, gym stay words; ytn is letters); declared
           abbreviations (Mr, Dr) keep their reading. */
        /* ...but never a contraction fragment after an apostrophe (I'll), and never a word
           with a letter beyond ASCII: Văn, phở in a Korean sentence are words whose vowels
           the key leaves out, not letters v n, p h (2026-10-14). */
        int beyond_ascii = 0;
        for (j = 0; j < n; ++j)
            beyond_ascii |= (unsigned char)in->text[t->begin + j] >= 0x80;
        if (!all_upper && !beyond_ascii && consumed == 1 && n >= 2 && n <= 6 &&
            !res(in->engine, "abbreviation", key) &&
            !(t->begin >= 2 && in->text[t->begin - 1] == '\'' &&
              ((in->text[t->begin - 2] | 0x20) >= 'a' && (in->text[t->begin - 2] | 0x20) <= 'z'))) {
            int vowel = 0;
            size_t k;
            for (j = 0; j < n; ++j)
                vowel |= strchr("aeiou", key[j]) != NULL || (j && key[j] == 'y');
            for (k = 0; !vowel && k < in->token_count; ++k)
                if (in->tokens[k].script == TTF_SCRIPT_HANGUL) {
                    all_upper = 1;
                    break;
                }
        }
        if (all_upper && caps_as_word(in, key, n, consumed))
            all_upper = 0;
        if (all_upper) {
            out->type = TTF_SEM_INITIALISM;
            out->lexical_class = TTF_LEX_INITIALISM;
        }
    }
    /* an LED, an FAQ: the writer's article shows the first sound they said. After
       "an", a run starting with a consonant letter whose name starts with a vowel
       sound (F ef, L el, S ess …) was said letter by letter (an LED el ee dee,
       not led; project-owner idea, 2026-10-08). "a" before such a run is no cue:
       an abbreviation is often said as its expansion (a RADM, a rear admiral). */
    const char *inferred = "inferred";
    if (out->type == TTF_SEM_INITIALISM && !kind && t->begin >= 2) {
        size_t p = t->begin, w;
        while (p && (in->text[p - 1] == ' ' || in->text[p - 1] == '\t'))
            --p;
        w = p;
        while (p && ((in->text[p - 1] | 0x20) >= 'a' && (in->text[p - 1] | 0x20) <= 'z'))
            --p;
        if (w < t->begin && (!p || !((in->text[p - 1] | 0x20) >= 'a' && (in->text[p - 1] | 0x20) <= 'z'))) {
            char first = in->text[t->begin];
            int vowel_name = strchr("AEFHILMNORSX", first) != NULL;
            int consonant_letter = !strchr("AEIOU", first);
            if (w - p == 2 && (in->text[p] | 0x20) == 'a' && (in->text[p + 1] | 0x20) == 'n' &&
                     vowel_name && consonant_letter)
                inferred = "article_letters";
        }
    }
    if (out->type == TTF_SEM_INITIALISM)
        ttf_en_initialism_policy(in->engine, out, in->text + t->begin, n,
                                 kind ? "known" : inferred);
    if (consumed == 1 && t->end < in->length && in->text[t->end] == '.' &&
        res(in->engine, "abbreviation", key)) {
        out->type = TTF_SEM_ABBREVIATION;
        out->input_end = t->end + 1;
        (void)snprintf(out->rule, sizeof(out->rule), "pack.abbreviation");
        return in->index + 1 < in->token_count ? 2 : 1;
    }
    out->input_end = end;
    (void)snprintf(out->rule, sizeof(out->rule), "%s",
                   consumed > 1 ? "pack.hyphenated-word" : "pack.lexical");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    return consumed;
}
static ttf_status_t phone_digits(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    int previous_digit = 0;
    for (; *value; ++value) {
        const char *label;
        ttf_status_t status;
        if (*value == '-') {
            label = res(e, "phone_render", "separator");
            if (!label)
                label = " "; /* Legacy phone group separator. */
            previous_digit = 0;
        } else if (*value >= '0' && *value <= '9') {
            char key[2] = {*value, '\0'};
            label = *value == '0' ? res(e, "phone_render", "zero") : res(e, "number", key);
            if (!label && *value == '0')
                label = res(e, "number", "0");
            if (previous_digit && (status = space(out)) != TTF_OK)
                return status;
            previous_digit = 1;
        } else
            return TTF_EINVAL;
        if (!label)
            return TTF_ERESOURCE;
        if ((status = ttf_buffer_puts(out, label)) != TTF_OK)
            return status;
    }
    return TTF_OK;
}
static ttf_status_t phone(const ttf_engine_t *e, const ttf_semantic_t *s, ttf_buffer_t *out) {
    const char *value = s->value;
    ttf_status_t status;
    if (*value == '+') {
        if ((status = word(e, out, "phone_render", "plus")) != TTF_OK)
            return status;
        ++value;
    }
    /* 1-800-555-0123 one eight hundred five five five ...: a toll-free 800 area code
       reads eight hundred (owner sets, 2026-10-14). */
    if (!strncmp(value, "1-800-", 6) || !strncmp(value, "800-", 4)) {
        if (*value == '1') {
            if ((status = phone_digits(e, "1", out)) != TTF_OK || (status = space(out)) != TTF_OK)
                return status;
            value += 2;
        }
        if ((status = card(e, 800, out)) != TTF_OK)
            return status;
        value += 3;
    }
    if ((status = phone_digits(e, value, out)) != TTF_OK)
        return status;
    if (s->secondary[0]) {
        if ((status = word(e, out, "phone_render", "extension")) != TTF_OK)
            return status;
        return phone_digits(e, s->secondary, out);
    }
    return TTF_OK;
}

/* Same-language spoken labels keep ordinary words and spell isolated letters
   or uppercase initialisms. Source verbalization for a foreign target must
   retain its original atom so the generic composition pipeline can split it. */
/* The number in a code (owner sets, 2026-10-14): five digits or more, or four after a
   hyphen (PN-4432), one by one; four as two pairs (CR2032 twenty thirty two, UA 2314);
   three as a digit and a pair (B214 two fourteen, ENG 401 four oh one, F-150 one
   fifty), or one by one before a letter (WDT750SAKZ); fewer as a number. */
static ttf_status_t code_pair(const ttf_engine_t *e, unsigned n, ttf_buffer_t *out) {
    ttf_status_t st;
    if (n >= 10)
        return card(e, n, out);
    if ((st = word(e, out, "number", "oh")) != TTF_OK)
        return st;
    if ((st = space(out)) != TTF_OK)
        return st;
    return card(e, n, out);
}
static ttf_status_t code_number(const ttf_engine_t *e, const char *v, size_t n, int hyphen_before,
                                int letter_after, ttf_buffer_t *out) {
    unsigned x = 0;
    size_t i;
    ttf_status_t st;
    for (i = 0; i < n; ++i)
        x = x * 10 + (unsigned)(v[i] - '0');
    if (n >= 5 || (n == 4 && hyphen_before) || (n == 3 && letter_after))
        return digits(e, v, out);
    if (n == 4 && x % 100) {
        if ((st = card(e, x / 100, out)) != TTF_OK || (st = space(out)) != TTF_OK)
            return st;
        return code_pair(e, x % 100, out);
    }
    if (n == 3 && x % 100) {
        if ((st = card(e, x / 100, out)) != TTF_OK || (st = space(out)) != TTF_OK)
            return st;
        return code_pair(e, x % 100, out);
    }
    return card(e, x, out);
}
/* SAVE20, WELCOME10, SPRING15, COVID-19: capitals of four letters or more with a vowel
   and no three consonants in a row (three may begin it: SPR, STR), before a number,
   read as a word. */
static int caps_word(const char *t, size_t n) {
    size_t i, run = 0, vowels = 0;
    if (n < 4)
        return 0;
    for (i = 0; i < n; ++i) {
        int vowel = strchr("AEIOU", t[i]) != NULL;
        vowels += vowel;
        run = vowel ? 0 : run + 1;
        if (run > 3 || (run == 3 && i != 2))
            return 0;
    }
    return vowels > 0;
}

static ttf_status_t spoken_alnum(const ttf_engine_t *e, const char *text, size_t length,
                                 ttf_buffer_t *out) {
    size_t pos = 0;
    int has_digit = 0, emitted = 0, initialism = 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c >= '0' && c <= '9')
            has_digit = 1;
        else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-' || c == ' '))
            return ttf_buffer_append(out, text, length);
    }
    if (!has_digit)
        return ttf_buffer_append(out, text, length);
    while (pos < length) {
        size_t end = pos;
        ttf_status_t status;
        if (text[pos] == '-' || text[pos] == ' ') {
            ++pos;
            continue;
        }
        if (emitted && (status = space(out)) != TTF_OK)
            return status;
        if (text[pos] >= '0' && text[pos] <= '9') {
            char value[TTF_IR_VALUE_SIZE];
            const char *policy = initialism ? res(e, "alnum_number", "initialism") : NULL;
            int digit_mode;
            while (end < length && text[end] >= '0' && text[end] <= '9')
                ++end;
            if (end - pos >= sizeof(value))
                return TTF_ELIMIT;
            memcpy(value, text + pos, end - pos);
            value[end - pos] = 0;
            digit_mode =
                (end - pos > 1 && text[pos] == '0') || (policy && !strcmp(policy, "source-digits"));
            ttf_trace_stage(e, TTF_TRACE_POLICY, pos, end);
            if (policy) {
                status = ttf_trace_resource(e, "alnum_number", EN, "initialism", value,
                                            "english-alnum-number-policy", SIZE_MAX);
                if (status != TTF_OK)
                    return status;
            }
            status = digit_mode ? digits(e, value, out)
                                : code_number(e, value, end - pos, pos && text[pos - 1] == '-',
                                              end < length && ((text[end] >= 'a' && text[end] <= 'z') ||
                                                               (text[end] >= 'A' && text[end] <= 'Z')),
                                              out);
            if (status == TTF_OK)
                status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, EN, "decision",
                                        "alnum-number", value, value,
                                        digit_mode ? "digit-label" : "cardinal-label", SIZE_MAX);
        } else {
            int upper = 1;
            while (end < length && ((text[end] >= 'a' && text[end] <= 'z') ||
                                    (text[end] >= 'A' && text[end] <= 'Z'))) {
                if (text[end] >= 'a' && text[end] <= 'z')
                    upper = 0;
                ++end;
            }
            initialism = upper && end - pos >= 2;
            if (upper && caps_word(text + pos, end - pos) && end < length &&
                ((text[end] >= '0' && text[end] <= '9') ||
                 (text[end] == '-' && end + 1 < length && text[end + 1] >= '0' && text[end + 1] <= '9'))) {
                size_t k;
                initialism = 0;
                status = TTF_OK;
                for (k = pos; k < end && status == TTF_OK; ++k) {
                    char lower = (char)(text[k] - 'A' + 'a');
                    status = ttf_buffer_append(out, &lower, 1);
                }
            } else if (upper || end - pos == 1)
                status = ttf_en_speech_spelling(e, text + pos, end - pos, 0, out);
            else
                status = ttf_buffer_append(out, text + pos, end - pos);
        }
        if (status != TTF_OK)
            return status;
        emitted = 1;
        pos = end;
    }
    return TTF_OK;
}

static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *out);
static ttf_status_t expression_number(const ttf_engine_t *e, const char *value, ttf_buffer_t *out);
static ttf_status_t expression_letter(const ttf_engine_t *e, char letter, ttf_buffer_t *out) {
    char key[2] = {letter >= 'A' && letter <= 'Z' ? (char)(letter - 'A' + 'a') : letter, 0};
    const char *label = ttf_resource_get(e, "letter_name", "en-US", key, NULL);
    return label ? ttf_buffer_puts(out, label) : TTF_ERESOURCE;
}
/* A canonical expression ("3 - 1 = 2") read with editable operator names and a
   relation template: {L} and {R} are the two sides, and {A/B} picks A after a
   final consonant (Korean particle), otherwise B. */
static ttf_status_t expression_side(const ttf_engine_t *e, const char *value, size_t length,
                                    ttf_buffer_t *out) {
    size_t at = 0;
    ttf_status_t st;
    while (at < length) {
        char part[48];
        size_t end = at;
        while (end < length && value[end] != ' ')
            ++end;
        if (end == at || end - at >= sizeof(part))
            return TTF_EINVAL;
        memcpy(part, value + at, end - at);
        part[end - at] = 0;
        /* 3 ^2 = 9 three squared equals nine, ^10 to the tenth, ^-3 to the minus three: the power
           of the part before it, also alone after a word already read (mc ^2). The Korean pack
           writes powers so (ko:superscript-expression) and a sentence routed to English read
           them here (2026-10-26; was three ² equals nine). */
        if (part[0] == '^' && part[1]) {
            const char *power = part + 1;
            int minus = *power == '-';
            char *stop;
            unsigned long x = strtoul(power + minus, &stop, 10);
            if (*stop || stop == power + minus)
                return TTF_EINVAL;
            if ((st = ttf_buffer_puts(out, " ")) != TTF_OK)
                return st;
            if (!minus && x == 2)
                st = ttf_buffer_puts(out, "squared");
            else if (!minus && x == 3)
                st = ttf_buffer_puts(out, "cubed");
            else if ((st = ttf_buffer_puts(out, minus ? "to the minus " : "to the ")) == TTF_OK)
                st = minus ? card(e, x, out) : ordinal(e, x, out);
            if (st != TTF_OK)
                return st;
            at = end < length ? end + 1 : end;
            continue;
        }
        if (at && (st = ttf_buffer_puts(out, " ")) != TTF_OK)
            return st;
        /* 1 / 2 + 1 / 4: a proper fraction of small whole numbers reads as a fraction (en dev v1,
           2026-10-17) */
        if (part[0] >= '0' && part[0] <= '9' && !strchr(part, '.') && end + 3 < length && value[end + 1] == '/' &&
            value[end + 2] == ' ' && value[end + 3] >= '0' && value[end + 3] <= '9') {
            size_t d0 = end + 3, d1 = d0;
            char den[8];
            while (d1 < length && value[d1] >= '0' && value[d1] <= '9')
                ++d1;
            if ((d1 >= length || value[d1] == ' ') && d1 - d0 < sizeof(den) && strtoul(part, NULL, 10) >= 1 &&
                strtoul(part, NULL, 10) < strtoul(value + d0, NULL, 10) && strtoul(value + d0, NULL, 10) <= 16) {
                ttf_semantic_t frac = {0};
                frac.struct_size = sizeof(frac);
                frac.type = TTF_SEM_FRACTION;
                (void)snprintf(frac.value, sizeof(frac.value), "%s", part);
                (void)snprintf(frac.secondary, sizeof(frac.secondary), "%.*s", (int)(d1 - d0), value + d0);
                (void)snprintf(frac.render_lang, sizeof(frac.render_lang), "%s", EN);
                if ((st = verbalize(e, &frac, part, strlen(part), out)) != TTF_OK)
                    return st;
                at = d1 < length ? d1 + 1 : d1;
                continue;
            }
        }
        if ((part[0] >= '0' && part[0] <= '9') || (part[0] == '-' && part[1]))
            st = expression_number(e, part, out);
        else if (!part[1] &&
                 ((part[0] >= 'a' && part[0] <= 'z') || (part[0] >= 'A' && part[0] <= 'Z')))
            st = expression_letter(e, part[0], out);
        else {
            const char *label = ttf_resource_get(e, "math_operator", EN, part, NULL);
            if (!label)
                return TTF_ERESOURCE;
            st = ttf_trace_resource(e, "math_operator", EN, part, part, "expression-operator",
                                    SIZE_MAX);
            if (st == TTF_OK)
                st = ttf_buffer_puts(out, label);
        }
        if (st != TTF_OK)
            return st;
        at = end < length ? end + 1 : end;
    }
    return TTF_OK;
}
static int final_consonant(const ttf_buffer_t *out) {
    size_t start = out->length;
    uint32_t cp;
    size_t width;
    while (start && ((unsigned char)out->data[start - 1] & 0xc0) == 0x80)
        --start;
    if (!start || ttf_utf8_decode(out->data + start - 1, out->length - start + 1, &cp, &width) != 1)
        return 0;
    return cp >= 0xac00 && cp <= 0xd7a3 && (cp - 0xac00) % 28;
}
static ttf_status_t expression(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    const char *relation = NULL, *template, *p;
    size_t length = strlen(value), left = length, right = length;
    ttf_status_t st;
    static const char *const relations[] = {" = ", " < ", " > ", " ≤ ", " ≥ ", " ≠ ", " ≈ ", " ≡ "};
    for (size_t i = 0; i < sizeof(relations) / sizeof(relations[0]); ++i) {
        const char *hit = strstr(value, relations[i]);
        if (hit) {
            relation = relations[i];
            left = (size_t)(hit - value);
            right = left + strlen(relations[i]);
        }
    }
    if (!relation)
        return expression_side(e, value, length, out);
    {
        char key[8];
        size_t w = strlen(relation) - 2;
        memcpy(key, relation + 1, w);
        key[w] = 0;
        template = ttf_resource_get(e, "math_relation", EN, key, NULL);
        if (!template)
            return TTF_ERESOURCE;
        if ((st = ttf_trace_resource(e, "math_relation", EN, key, key, "expression-relation",
                                     SIZE_MAX)) != TTF_OK)
            return st;
    }
    for (p = template; *p;) {
        if (!strncmp(p, "{L}", 3)) {
            st = expression_side(e, value, left, out);
            p += 3;
        } else if (!strncmp(p, "{R}", 3)) {
            st = expression_side(e, value + right, length - right, out);
            p += 3;
        } else if (*p == '{' && strchr(p, '/') && strchr(p, '}') &&
                   strchr(p, '/') < strchr(p, '}')) {
            const char *slash = strchr(p, '/'), *close = strchr(p, '}');
            st = final_consonant(out)
                     ? ttf_buffer_append(out, p + 1, (size_t)(slash - p - 1))
                     : ttf_buffer_append(out, slash + 1, (size_t)(close - slash - 1));
            p = close + 1;
        } else {
            st = ttf_buffer_append(out, p, 1);
            ++p;
        }
        if (st != TTF_OK)
            return st;
    }
    return TTF_OK;
}

static ttf_status_t expression_number(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    return numeric(e, value, out);
}

/* 2.1.3 and 192.168.0.1: dotted numeric parts with an editable part reading and
   separator. A leading-zero part keeps its digits; a v prefix is spelled. */
static ttf_status_t dotted_numbers(const ttf_engine_t *e, const ttf_semantic_t *s,
                                   const char *surface, size_t length, ttf_buffer_t *out) {
    const char *kind = s->type == TTF_SEM_IP_ADDRESS ? "ip" : "version";
    const char *policy = ttf_resource_get(e, "electronic_number", EN, kind, NULL);
    const char *separator = ttf_resource_get(e, "electronic_separator", EN, kind, NULL);
    size_t at = 0;
    ttf_status_t status;
    if (!policy || !separator)
        return TTF_ERESOURCE;
    if (s->secondary[0]) {
        /* v2.1: version two point one when electronic_prefix names the letter
           (2026-10-13; the Japanese pack reads バージョン); otherwise spelled. */
        char letter[2] = {(char)tolower((unsigned char)s->secondary[0]), 0};
        const char *named = res(e, "electronic_prefix", letter);
        if (!length || surface[0] != s->secondary[0])
            return TTF_EINVAL;
        if ((status = named ? ttf_buffer_puts(out, named)
                            : ttf_en_speech_spelling(e, surface, 1, 0, out)) != TTF_OK ||
            (status = ttf_buffer_puts(out, " ")) != TTF_OK)
            return status;
        at = 1;
    }
    while (at < length) {
        ttf_semantic_t part = {0};
        size_t end = at;
        while (end < length && surface[end] >= '0' && surface[end] <= '9')
            ++end;
        if (end == at || end - at >= sizeof(part.value))
            return TTF_EINVAL;
        part.struct_size = sizeof(part);
        /* an IP octet of one or two digits reads as a number (10.0.4.17 ten dot zero dot
           four dot seventeen; owner sets, 2026-10-14); 192 stays one nine two */
        part.type = (!strcmp(policy, "digits") && !(s->type == TTF_SEM_IP_ADDRESS && end - at <= 2)) ||
                            (end - at > 1 && surface[at] == '0')
                        ? TTF_SEM_DIGIT_SEQUENCE
                        : TTF_SEM_CARDINAL;
        memcpy(part.value, surface + at, end - at);
        if ((status = verbalize(e, &part, surface + at, end - at, out)) != TTF_OK)
            return status;
        at = end;
        if (at < length) {
            if (surface[at] != '.')
                return TTF_EINVAL;
            if ((status = ttf_buffer_puts(out, " ")) != TTF_OK ||
                (status = ttf_buffer_puts(out, separator)) != TTF_OK ||
                (status = ttf_buffer_puts(out, " ")) != TTF_OK)
                return status;
            ++at;
        }
    }
    return TTF_OK;
}

static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *b) {
    ttf_status_t st;
    char key[48];
    const char *label;
    unsigned h, min, sec;
    switch (s->type) {
    case TTF_SEM_ALNUM_TERM:
        if (e->frontend_options.output_mode == TTF_OUTPUT_SPEECH && !strcmp(s->render_lang, EN))
            return spoken_alnum(e, surface, length, b);
        return ttf_buffer_append(b, surface, length);
    case TTF_SEM_SYMBOL:
        if (!strcmp(s->unit, "@feet-inches") || !strcmp(s->unit, "@ratio")) {
            /* 6'2" six foot two; 1:10 one to ten */
            const char *bar = strchr(s->value, '|');
            uint64_t a, c;
            char left[24];
            if (!bar || (size_t)(bar - s->value) >= sizeof(left))
                return TTF_EINVAL;
            (void)snprintf(left, sizeof(left), "%.*s", (int)(bar - s->value), s->value);
            if (!uint64_value(left, &a) || !uint64_value(bar + 1, &c))
                return TTF_EINVAL;
            if ((st = card(e, a, b)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            if ((st = ttf_buffer_puts(b, !strcmp(s->unit, "@ratio") ? "to" : "foot")) != TTF_OK ||
                (st = space(b)) != TTF_OK)
                return st;
            return card(e, c, b);
        }
        if (!strcmp(s->unit, "@scaled") || !strcmp(s->unit, "@seconds")) {
            /* 1.2|million one point two million; 90|s ninety seconds, 1|s one second, 90|second */
            char left[24];
            const char *bar = strchr(s->value, '|');
            if (!bar || (size_t)(bar - s->value) >= sizeof(left))
                return TTF_EINVAL;
            (void)snprintf(left, sizeof(left), "%.*s", (int)(bar - s->value), s->value);
            if ((st = numeric(e, left, b)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            if (!strcmp(bar + 1, "s"))
                return ttf_buffer_puts(b, !strcmp(left, "1") ? "second" : "seconds");
            return ttf_buffer_puts(b, bar + 1);
        }
        if (!strcmp(s->unit, "@star") || !strcmp(s->unit, "@hostport")) {
            if ((st = ttf_buffer_puts(b, !strcmp(s->unit, "@star") ? "star " : "localhost colon ")) != TTF_OK)
                return st;
            return digits(e, s->value, b);
        }
        if (!strcmp(s->unit, "@code")) {
            /* WDT750SAKZ w d t seven five zero s a k z */
            const char *c;
            for (c = s->value; *c; ++c) {
                if (c != s->value && (st = space(b)) != TTF_OK)
                    return st;
                if (*c >= '0' && *c <= '9') {
                    char d[2] = {*c, 0};
                    st = digits(e, d, b);
                } else
                    st = ttf_en_speech_spelling(e, c, 1, 0, b);
                if (st != TTF_OK)
                    return st;
            }
            return TTF_OK;
        }
        if (!strcmp(s->unit, "@mixed")) {
            /* 2|a half two and a half */
            const char *bar = strchr(s->value, '|');
            uint64_t a;
            char left[8];
            if (!bar || (size_t)(bar - s->value) >= sizeof(left))
                return TTF_EINVAL;
            (void)snprintf(left, sizeof(left), "%.*s", (int)(bar - s->value), s->value);
            if (!uint64_value(left, &a))
                return TTF_EINVAL;
            if ((st = card(e, a, b)) != TTF_OK || (st = ttf_buffer_puts(b, " and ")) != TTF_OK)
                return st;
            return ttf_buffer_puts(b, bar + 1);
        }
        if (!strcmp(s->unit, "@day-range")) {
            /* October|24|26 October twenty fourth to twenty sixth */
            const char *bar = strchr(s->value, '|'), *bar2 = bar ? strchr(bar + 1, '|') : NULL;
            if (!bar2)
                return TTF_EINVAL;
            if ((st = ttf_buffer_append(b, s->value, (size_t)(bar - s->value))) != TTF_OK ||
                (st = space(b)) != TTF_OK || (st = ordinal(e, strtoull(bar + 1, NULL, 10), b)) != TTF_OK ||
                (st = ttf_buffer_puts(b, " to ")) != TTF_OK)
                return st;
            return ordinal(e, strtoull(bar2 + 1, NULL, 10), b);
        }
        if (!strcmp(s->unit, "@cents")) {
            uint64_t a;
            if (!uint64_value(s->value, &a))
                return TTF_EINVAL;
            if ((st = card(e, a, b)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            return ttf_buffer_puts(b, a == 1 ? "cent" : "cents");
        }
        if (!strcmp(s->unit, "@inch")) {
            uint64_t a;
            if (!uint64_value(s->value, &a))
                return TTF_EINVAL;
            if ((st = card(e, a, b)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            return ttf_buffer_puts(b, "inch");
        }
        if (!strcmp(s->unit, "@inch-mark")) {
            /* 8-3/8|diameter|1 eight and three eighths inch diameter by */
            char num[24], sfx[24], by[4];
            const char *slash;
            if (sscanf(s->value, "%23[^|]|%23[^|]|%3s", num, sfx, by) < 2) {
                if (sscanf(s->value, "%23[^|]||%3s", num, by) != 2)
                    return TTF_EINVAL;
                sfx[0] = 0;
            }
            if ((slash = strchr(num, '/')) != NULL) {
                ttf_semantic_t f = *s;
                const char *join = strpbrk(num, "- ");
                f.type = TTF_SEM_FRACTION;
                (void)snprintf(f.value, sizeof(f.value), "%.*s", (int)(slash - (join ? join + 1 : num)), join ? join + 1 : num);
                (void)snprintf(f.secondary, sizeof(f.secondary), "%s", slash + 1);
                if (join)
                    (void)snprintf(f.unit, sizeof(f.unit), "mixed:%.*s", (int)(join - num), num);
                else
                    f.unit[0] = 0;
                st = verbalize(e, &f, num, strlen(num), b);
            } else
                st = numeric(e, num, b);
            if (st != TTF_OK || (st = ttf_buffer_puts(b, " inch")) != TTF_OK)
                return st;
            if (sfx[0] && ((st = space(b)) != TTF_OK || (st = ttf_buffer_puts(b, sfx)) != TTF_OK))
                return st;
            return by[0] == '1' ? ttf_buffer_puts(b, " by") : TTF_OK;
        }
        if (!strcmp(s->unit, "@dims")) {
            /* 97.6*57.1* ninety seven point six by fifty seven point one by */
            const char *at = s->value, *star;
            char num[24];
            while ((star = strchr(at, '*')) != NULL) {
                (void)snprintf(num, sizeof(num), "%.*s", (int)(star - at), at);
                if ((st = numeric(e, num, b)) != TTF_OK || (st = ttf_buffer_puts(b, " by ")) != TTF_OK)
                    return st;
                at = star + 1;
            }
            return TTF_OK;
        }
        if (!strcmp(s->unit, "@count-per")) {
            /* 10|per pack ten per pack */
            const char *bar = strchr(s->value, '|');
            char num[24];
            if (!bar || (size_t)(bar - s->value) >= sizeof(num))
                return TTF_EINVAL;
            (void)snprintf(num, sizeof(num), "%.*s", (int)(bar - s->value), s->value);
            if ((st = numeric(e, num, b)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            return ttf_buffer_puts(b, bar + 1);
        }
        if (!strcmp(s->unit, "@ord-the")) {
            if ((st = ttf_buffer_puts(b, "the ")) != TTF_OK)
                return st;
            return ordinal(e, strtoull(s->value, NULL, 10), b);
        }
        if (!strcmp(s->unit, "@money-scale")) {
            /* 53.7|million|dollars */
            char num[32], scale[16], money[32];
            if (sscanf(s->value, "%31[^|]|%15[^|]|%31[^\n]", num, scale, money) != 3)
                return TTF_EINVAL;
            if ((st = numeric(e, num, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
                (st = ttf_buffer_puts(b, scale)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            return ttf_buffer_puts(b, money);
        }
        if (!strcmp(s->unit, "@quarter")) {
            /* 4|2010 Q four twenty ten */
            if ((st = ttf_buffer_puts(b, "Q ")) != TTF_OK || (st = card(e, (uint64_t)(s->value[0] - '0'), b)) != TTF_OK ||
                (st = space(b)) != TTF_OK)
                return st;
            return date_year(e, (unsigned)atoi(s->value + 2), "year_style", b);
        }
        if (!strcmp(s->unit, "@by"))
            return ttf_buffer_puts(b, "by");
        if (!strcmp(s->unit, "@to"))
            return ttf_buffer_puts(b, "to");
        if (!strcmp(s->unit, "@word"))
            return ttf_buffer_puts(b, s->value);
        if (!strcmp(s->unit, "@plus-minus"))
            return ttf_buffer_puts(b, "plus or minus");
        if (!strcmp(s->unit, "@out-of")) {
            /* 4.5/5 four point five out of five */
            char left[24];
            const char *bar = strchr(s->value, '|');
            if (!bar || (size_t)(bar - s->value) >= sizeof(left))
                return TTF_EINVAL;
            (void)snprintf(left, sizeof(left), "%.*s", (int)(bar - s->value), s->value);
            if ((st = numeric(e, left, b)) != TTF_OK || (st = ttf_buffer_puts(b, " out of ")) != TTF_OK)
                return st;
            return numeric(e, bar + 1, b);
        }
        if (!strcmp(s->unit, "@number-sign")) {
            const char *template = ttf_resource_get(e, "number_sign", EN, "#", NULL), *p;
            ttf_status_t status;
            if (!template)
                return TTF_ERESOURCE;
            status =
                ttf_trace_resource(e, "number_sign", EN, "#", "#", "number-sign-reading", SIZE_MAX);
            if (status != TTF_OK)
                return status;
            for (p = template; *p;) {
                if (!strncmp(p, "{N}", 3)) {
                    status = numeric(e, s->value, b);
                    p += 3;
                } else {
                    status = ttf_buffer_append(b, p, 1);
                    ++p;
                }
                if (status != TTF_OK)
                    return status;
            }
            return TTF_OK;
        }
        if (!strcmp(s->unit, "@expression"))
            return expression(e, s->value, b);
        if (!strcmp(s->unit, "@math")) {
            label = res(e, "math_operator", s->value);
            if (!label)
                return TTF_ERESOURCE;
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, strlen(s->value));
            st = ttf_trace_resource(e, "math_operator", EN, s->value, s->value,
                                    "binary-operator-reading", SIZE_MAX);
            return st == TTF_OK ? ttf_buffer_puts(b, label) : st;
        }
        return ttf_buffer_append(b, surface, length);
    case TTF_SEM_CUSTOM:
        if (!strcmp(s->unit, "@alphabet"))
            return !strcmp(s->secondary, "control-surface")
                       ? ttf_en_speech_spelling(e, surface, length, 0, b)
                       : ttf_en_speech_spelling(e, s->value, strlen(s->value), 0, b);
        if (!strcmp(s->unit, "@codepoint")) {
            char point[sizeof(s->value) + 2];
            if (e->frontend_options.output_mode != TTF_OUTPUT_SPEECH)
                return ttf_buffer_append(b, surface, length);
            (void)snprintf(point, sizeof(point), "U+%s", s->value);
            return ttf_en_speech_spelling(e, point, strlen(point), 1, b);
        }
        /* @say (dev_forms): c:N cardinal, o:N ordinal, d:digits, k:label pairs, n:number, s:letters,
           w:words, y:YEAR a calendar year */
        if (!strcmp(s->unit, "@say")) {
            const char *it = s->value;
            int first = 1;
            while (*it) {
                const char *bar = strchr(it, '|');
                size_t l = bar ? (size_t)(bar - it) : strlen(it);
                char item[64];
                if (l < 2 || l - 2 >= sizeof(item) || it[1] != ':')
                    return TTF_EINVAL;
                memcpy(item, it + 2, l - 2);
                item[l - 2] = 0;
                if (!first && (st = space(b)) != TTF_OK)
                    return st;
                first = 0;
                switch (it[0]) {
                case 'c':
                    st = card(e, strtoull(item, NULL, 10), b);
                    break;
                case 'o':
                    st = ordinal(e, strtoull(item, NULL, 10), b);
                    break;
                case 'd':
                    st = digits(e, item, b);
                    break;
                case 'k': {
                    uint64_t x = strtoull(item, NULL, 10);
                    size_t il = strlen(item);
                    /* 1600 sixteen hundred; 214 two fourteen; 3120 thirty one twenty */
                    if (il == 4 && x % 100 == 0 && x % 1000) {
                        if ((st = card(e, x / 100, b)) == TTF_OK && (st = space(b)) == TTF_OK)
                            st = word(e, b, "number", "100");
                    } else
                        st = code_number(e, item, il, 0, 0, b);
                    break;
                }
                case 'n':
                    st = numeric(e, item, b);
                    break;
                case 's':
                    st = ttf_en_speech_spelling(e, item, strlen(item), 0, b);
                    break;
                case 'y':
                    st = date_year(e, (unsigned)strtoul(item, NULL, 10), "year_style", b);
                    break;
                case 'w':
                    st = ttf_buffer_puts(b, item);
                    break;
                default:
                    return TTF_EINVAL;
                }
                if (st != TTF_OK)
                    return st;
                if (!bar)
                    break;
                it = bar + 1;
            }
            return TTF_OK;
        }
        /* 1M requests: one million (count_scale) */
        if (!strcmp(s->unit, "@scaled-count")) {
            if ((st = numeric(e, s->value, b)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            return word(e, b, "number", s->secondary);
        }
        /* requests/day: per day (per_word) */
        if (!strcmp(s->unit, "@per"))
            return (st = space(b)) != TTF_OK ? st : word(e, b, "per_word", s->value);
        if (!strcmp(s->unit, "@email") || !strcmp(s->unit, "@url"))
            return ttf_electronic_control(e, s, surface, length, b);
        if (!strcmp(s->unit, "@counter"))
            return TTF_EINVAL; /* Native counted nouns are a KO pack control. */
        return ttf_semantic_control(e, s, surface, length, b);
    case TTF_SEM_VERSION:
    case TTF_SEM_IP_ADDRESS:
        return dotted_numbers(e, s, surface, length, b);
    case TTF_SEM_EMAIL:
    case TTF_SEM_URL:
        return ttf_en_speech_spelling(e, surface, length, 1, b);
    case TTF_SEM_ABBREVIATION:
    case TTF_SEM_INITIALISM:
        if (!strcmp(s->rule, "syntax.web-initialism"))
            return ttf_buffer_puts(b, s->value);
        return ttf_en_verbalize_abbreviation(e, s, surface, length, b);
    case TTF_SEM_CARDINAL:
    case TTF_SEM_NUMBER:
        return numeric(e, s->value, b);
    case TTF_SEM_ORDINAL: {
        uint64_t value;
        if (!uint64_value(s->value, &value))
            return ttf_buffer_append(b, surface, length);
        /* Henry II -> the second */
        if (!strcmp(s->secondary, "regnal") && (st = ttf_buffer_puts(b, "the ")) != TTF_OK)
            return st;
        return ordinal(e, value, b);
    }
    case TTF_SEM_DIGIT_SEQUENCE:
        if (!strcmp(s->unit, "financial_identifier"))
            return ttf_en_render_identifier(e, s->value, b);
        if (!strcmp(s->unit, "written_fallback"))
            return written(e, surface, length, b);
        if (!strcmp(s->unit, "scripture"))
            return scripture(e, s->value, b);
        return digits(e, s->value, b);
    case TTF_SEM_PHONE:
        return phone(e, s, b);
    case TTF_SEM_DATE:
        if (!strcmp(s->unit, "weekday")) {
            if (s->value[0] < '1' || s->value[0] > '7' || s->value[1])
                return TTF_EINVAL;
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, length);
            if (s->secondary[0] &&
                (st = ttf_trace_resource(e, "weekday_label", s->source_lang, s->secondary,
                                         NULL, "date-attached weekday alias", (size_t)-1)) != TTF_OK)
                return st;
            if ((st = ttf_trace_resource(e, "weekday", EN, s->value, NULL,
                                         "calendar weekday reading", (size_t)-1)) != TTF_OK)
                return st;
            return word(e, b, "weekday", s->value);
        }
        return date(e, s, b);
    case TTF_SEM_TIME: {
        int fields = sscanf(s->value, "%u:%u:%u", &h, &min, &sec);
        if (fields < 2)
            return TTF_EINVAL;
        /* Midnight is a time, not a zero-valued twelve-hour clock position.
           Keep labels and the optional seconds relation in editable resources. */
        if (!h && !min) {
            if (fields == 3 && sec) {
                if ((st = card(e, sec, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
                    (st = word(e, b, "unit", sec == 1 ? "s.one" : "s.many")) != TTF_OK ||
                    (st = space(b)) != TTF_OK || (st = word(e, b, "clock", "after")) != TTF_OK ||
                    (st = space(b)) != TTF_OK)
                    return st;
            }
            return word(e, b, "clock", "midnight");
        }
        if ((st = card(e, h, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if (!min && (!strcmp(s->rule, "syntax.hour-meridiem") || s->secondary[0])) {
            /* 4:00 a.m. four a m: the meridiem replaces o'clock (2026-10-07). */
            /* 5pm, 7 a.m.: the meridiem replaces o'clock (five p m); a written
               8:00 AM keeps its o'clock reading. */
        } else if (!min) {
            /* The project keeps 24-hour values; do not attach o'clock to 13..23. */
            st = h <= 12 ? word(e, b, "number", "oclock") : word(e, b, "clock", "h24_full_hour");
            if (st != TTF_OK)
                return st;
        } else {
            if (min < 10) {
                if ((st = word(e, b, "number", "oh")) != TTF_OK)
                    return st;
                if ((st = space(b)) != TTF_OK)
                    return st;
            }
            if ((st = card(e, min, b)) != TTF_OK)
                return st;
        }
        if (fields == 3) {
            if ((st = space(b)) != TTF_OK || (st = word(e, b, "number", "and")) != TTF_OK ||
                (st = space(b)) != TTF_OK || (st = card(e, sec, b)) != TTF_OK ||
                (st = space(b)) != TTF_OK ||
                (st = word(e, b, "unit", sec == 1 ? "s.one" : "s.many")) != TTF_OK)
                return st;
        }
        if (s->secondary[0]) {
            if ((!b->length || b->data[b->length - 1] != ' ') && (st = space(b)) != TTF_OK)
                return st;
            return word(e, b, "meridiem", s->secondary);
        }
        return TTF_OK;
    }
    case TTF_SEM_FRACTION: {
        /* 1/4 one quarter, 3/4 three quarters, 2/3 two thirds, 3/8 three eighths:
           a proper fraction names its denominator (the English references 51 : 0
           against "over", 2026-10-07); half and quarter are data
           (fraction_denominator), other denominators are ordinals with -s. Other
           fractions keep "over" (24/7). A mixed number (2 1/2) carries its whole
           part in the unit (mixed:2) and reads "two and one half". */
        char *end_num, *end_den, key[8], idiom[sizeof(s->value) + sizeof(s->secondary)];
        unsigned long num = strtoul(s->value, &end_num, 10), den = strtoul(s->secondary, &end_den, 10);
        const char *named;
        /* 24/7: twenty four seven (fraction_idiom) */
        if (!strcmp(s->unit, "idiom")) {
            (void)snprintf(idiom, sizeof(idiom), "%s/%s", s->value, s->secondary);
            return word(e, b, "fraction_idiom", idiom);
        }
        if (!strncmp(s->unit, "mixed:", 6)) {
            if ((st = numeric(e, s->unit + 6, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
                (st = word(e, b, "number", "and")) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            /* 2 1/2 two and a half (owner v2, 2026-10-15) */
            if (num == 1 && (den == 2 || den == 4) && !*end_num && !*end_den)
                return ttf_buffer_puts(b, den == 2 ? "a half" : "a quarter");
        }
        if (s->value[0] && s->secondary[0] && !*end_num && !*end_den && num >= 1 && num < den &&
            den >= 2 && den <= 16) {
            (void)snprintf(key, sizeof(key), "%lu", den);
            named = res(e, "fraction_denominator", key);
            if ((st = card(e, (unsigned)num, b)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            if (named) {
                const char *bar = strchr(named, '|');
                if (!bar)
                    return TTF_ERESOURCE;
                st = num == 1 ? ttf_buffer_append(b, named, (size_t)(bar - named))
                              : ttf_buffer_puts(b, bar + 1);
            } else if ((st = ordinal(e, (unsigned)den, b)) == TTF_OK && num != 1)
                st = ttf_buffer_puts(b, "s");
            if (st != TTF_OK)
                return st;
            if (!strncmp(s->unit, "unit:", 5)) {
                char name[32];
                (void)snprintf(name, sizeof(name), "%s.one", s->unit + 5);
                if ((st = space(b)) != TTF_OK)
                    return st;
                return word(e, b, "unit", name);
            }
            return TTF_OK;
        }
        if ((st = numeric(e, s->value, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
            (st = word(e, b, "number", "fraction_over")) != TTF_OK || (st = space(b)) != TTF_OK)
            return st;
        return numeric(e, s->secondary, b);
    }
    case TTF_SEM_CURRENCY: {
        const char *dot = strchr(s->value, '.');
        const char *minor_policy = res(e, "currency_minor_digits", s->unit);
        char whole[TTF_IR_VALUE_SIZE];
        size_t len = dot ? (size_t)(dot - s->value) : strlen(s->value);
        size_t fractional_digits = dot ? strlen(dot + 1) : 0;
        unsigned minor_digits = 2;
        int decimal_major;
        if (s->secondary[0]) {
            char coefficient[TTF_IR_VALUE_SIZE + 20], scale_key[24];
            if ((st = currency_coefficient(s, coefficient, scale_key)) != TTF_OK)
                return st;
            if ((st = numeric(e, coefficient, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
                (st = word(e, b, "number", scale_key)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            /* A scale phrase takes a plural currency noun even when a tiny
               coefficient happens to produce one expanded monetary unit. */
            (void)snprintf(key, sizeof(key), "%s.many", s->unit);
            return word(e, b, "currency", key);
        }
        if (minor_policy) {
            if (!strcmp(minor_policy, "0"))
                minor_digits = 0;
            else if (strcmp(minor_policy, "2"))
                return TTF_ERESOURCE;
        }
        decimal_major = !minor_digits || fractional_digits > minor_digits;
        memcpy(whole, s->value, len);
        whole[len] = '\0';
        if ((st = numeric(e, decimal_major ? s->value : whole, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        (void)snprintf(key, sizeof(key), "%s.%s", s->unit,
                       singular(decimal_major ? s->value : whole) ? "one" : "many");
        if ((st = word(e, b, "currency", key)) != TTF_OK)
            return st;
        if (dot && !decimal_major && fractional_digits && strtoul(dot + 1, NULL, 10)) {
            unsigned cents = (unsigned)strtoul(dot + 1, NULL, 10);
            if (!dot[2])
                cents *= 10;
            if ((st = space(b)) != TTF_OK)
                return st;
            if ((st = word(e, b, "number", "and")) != TTF_OK)
                return st;
            if ((st = space(b)) != TTF_OK)
                return st;
            if ((st = card(e, cents, b)) != TTF_OK)
                return st;
            if ((st = space(b)) != TTF_OK)
                return st;
            (void)snprintf(key, sizeof(key), "%s.%s", s->unit,
                           cents == 1 ? "minor.one" : "minor.many");
            return word(e, b, "currency", key);
        }
        return TTF_OK;
    }
    case TTF_SEM_PERCENT:
        if ((st = numeric(e, s->value, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        return word(e, b, "number", "percent");
    case TTF_SEM_MEASURE:
    case TTF_SEM_COUNTER:
        if ((st = numeric(e, s->value, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        (void)snprintf(key, sizeof(key), "%s.%s", s->unit, singular(s->value) ? "one" : "many");
        label = res(e, "unit", key);
        return ttf_buffer_puts(b, label ? label : s->unit);
    case TTF_SEM_RANGE:
        if (!strcmp(s->unit, "year")) {
            /* Each endpoint is a year, so "1988 - 93" is nineteen eighty eight
               to ninety three rather than a pair of plain cardinals. */
            ttf_semantic_t endpoint = *s;
            endpoint.type = TTF_SEM_DATE;
            endpoint.secondary[0] = '\0';
            if ((st = verbalize(e, &endpoint, surface, length, b)) != TTF_OK ||
                (st = space(b)) != TTF_OK || (st = word(e, b, "number", "range")) != TTF_OK ||
                (st = space(b)) != TTF_OK)
                return st;
            (void)snprintf(endpoint.value, sizeof(endpoint.value), "%s", s->secondary);
            if (strlen(s->secondary) != 4)
                endpoint.type = TTF_SEM_CARDINAL, endpoint.unit[0] = '\0';
            return verbalize(e, &endpoint, surface, length, b);
        }
        if (!strcmp(s->unit, "time")) {
            ttf_semantic_t endpoint = *s;
            endpoint.type = TTF_SEM_TIME;
            endpoint.secondary[0] = '\0';
            endpoint.unit[0] = '\0';
            if ((st = verbalize(e, &endpoint, surface, length, b)) != TTF_OK ||
                (st = space(b)) != TTF_OK || (st = word(e, b, "number", "range")) != TTF_OK ||
                (st = space(b)) != TTF_OK)
                return st;
            (void)snprintf(endpoint.value, sizeof(endpoint.value), "%s", s->secondary);
            return verbalize(e, &endpoint, surface, length, b);
        }
        /* Sharing "million dollars" in "two to three million dollars" can
           incorrectly imply a million-scale left endpoint. Repeat currency
           labels when only one endpoint reaches a named cardinal scale. */
        (void)snprintf(key, sizeof(key), "%s.many", s->unit);
        if (s->unit[0] && res(e, "currency", key) &&
            thousand_or_larger(s->value) != thousand_or_larger(s->secondary)) {
            ttf_semantic_t endpoint = *s;
            endpoint.type = TTF_SEM_CURRENCY;
            endpoint.secondary[0] = '\0';
            if ((st = verbalize(e, &endpoint, surface, length, b)) != TTF_OK ||
                (st = space(b)) != TTF_OK || (st = word(e, b, "number", "range")) != TTF_OK ||
                (st = space(b)) != TTF_OK)
                return st;
            (void)snprintf(endpoint.value, sizeof(endpoint.value), "%s", s->secondary);
            return verbalize(e, &endpoint, surface, length, b);
        }
        if ((st = numeric(e, s->value, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if ((st = word(e, b, "number", "range")) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if ((st = numeric(e, s->secondary, b)) != TTF_OK || !s->unit[0])
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if (!strcmp(s->unit, "percent"))
            return word(e, b, "number", "percent");
        (void)snprintf(key, sizeof(key), "%s.%s", s->unit, singular(s->secondary) ? "one" : "many");
        label = res(e, "unit", key);
        if (!label)
            label = res(e, "currency", key);
        return ttf_buffer_puts(b, label ? label : s->unit);
    default:
        return ttf_buffer_append(b, surface, length);
    }
}
static size_t phone_count(const char *text) {
    size_t count = 0;
    int in_token = 0;
    for (; *text; ++text) {
        if (*text == ' ')
            in_token = 0;
        else if (!in_token) {
            ++count;
            in_token = 1;
        }
    }
    return count;
}

static ttf_status_t pronounce(const ttf_engine_t *e, const char *word_text, size_t n,
                              ttf_lexical_class_t lexical, ttf_pronunciation_t *out) {
    char key[TTF_IR_VALUE_SIZE], variant_key_text[TTF_IR_VALUE_SIZE + 32];
    size_t i, p = 0, phone_index = 0;
    const char *phones, *dictionary_kind, *dictionary_key, *selection_reason = "default-base";
    ttf_resource_layer_t layer = TTF_LAYER_COMMON, part_layer = TTF_LAYER_COMMON;
    ttf_status_t status;
    /* Long unknown words bypass this bounded prototype and reach the target's
       streaming grapheme fallback; they must not fail the entire request. */
    if (n >= sizeof(key))
        return TTF_EINVAL;
    memset(out, 0, sizeof(*out));
    (void)snprintf(out->alphabet, sizeof(out->alphabet), "ttf-phones-v1");
    for (i = 0; i < n; ++i)
        key[i] = ascii_lower(word_text[i]);
    key[n] = '\0';
    /* A lowercase dictionary headword such as "us" is not evidence for the
       letter sequence U.S. Explicit initialism pronunciations have their own
       namespace; otherwise construct letter phones below. */
    dictionary_kind = lexical == TTF_LEX_INITIALISM ? "initialism_pronunciation" : "pronunciation";
    dictionary_key = key;
    phones = ttf_resource_get(e, dictionary_kind, EN, key, &layer);
    if (lexical != TTF_LEX_INITIALISM) {
        ttf_resource_layer_t choice_layer = TTF_LAYER_COMMON;
        const char *choice = ttf_resource_get(e, "pronunciation_choice", EN, key, &choice_layer);
        if (choice && phones && choice_layer < layer)
            selection_reason = "higher-layer-base";
        else if (choice) {
            selection_reason = !strcmp(choice, "base") ? "explicit-base" : "explicit-choice";
            if ((status = ttf_trace_resource(e, "pronunciation_choice", EN, key, choice,
                                             selection_reason, SIZE_MAX)) != TTF_OK)
                return status;
            if (strcmp(choice, "base")) {
                (void)snprintf(variant_key_text, sizeof(variant_key_text), "%s#%s", key, choice);
                dictionary_kind = "pronunciation_variant";
                dictionary_key = variant_key_text;
                phones = ttf_resource_get(e, dictionary_kind, EN, dictionary_key, &layer);
                if (!phones)
                    return TTF_ERESOURCE; /* Freeze normally reports this dependency. */
            }
        }
    }
    if (phones) {
        if (strlen(phones) >= sizeof(out->phones))
            return TTF_ELIMIT;
        (void)snprintf(out->phones, sizeof(out->phones), "%s", phones);
        (void)snprintf(out->provenance, sizeof(out->provenance),
                       "ttf-pron-v1:%s:pronunciation-dictionary", ttf_layer_name(layer));
        return ttf_trace_resource(e, dictionary_kind, EN, dictionary_key, phones, selection_reason,
                                  0);
    }
    if (lexical == TTF_LEX_INITIALISM) {
        for (i = 0; i < n; ++i) {
            char symbolic[16], spelling[2];
            int written;
            if (key[i] < 'a' || key[i] > 'z')
                continue;
            (void)snprintf(symbolic, sizeof(symbolic), "letter.%c",
                           (char)toupper((unsigned char)key[i]));
            written =
                snprintf(out->phones + p, sizeof(out->phones) - p, "%s%s", p ? " " : "", symbolic);
            if (written < 0 || (size_t)written >= sizeof(out->phones) - p)
                return TTF_ELIMIT;
            p += (size_t)written;
            spelling[0] = word_text[i];
            spelling[1] = '\0';
            if ((status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, EN,
                                         "algorithm", "initialism-letter", symbolic, spelling,
                                         "generated-letter-phone", phone_index++)) != TTF_OK)
                return status;
        }
        (void)snprintf(out->provenance, sizeof(out->provenance), "ttf-pron-v1:common:initialism");
        return p ? TTF_OK : TTF_EINVAL;
    }
    if (lexical == TTF_LEX_ALNUM_TERM) {
        size_t upper = 0;
        while (upper < n && word_text[upper] >= 'A' && word_text[upper] <= 'Z')
            ++upper;
        if (upper >= 2) {
            for (i = 0; i < n; ++i) {
                char symbolic[16], digit_key[2] = {key[i], '\0'};
                int written;
                if (word_text[i] == '-')
                    continue;
                if (key[i] >= 'a' && key[i] <= 'z') {
                    (void)snprintf(symbolic, sizeof(symbolic), "letter.%c",
                                   (char)toupper((unsigned char)key[i]));
                    phones = symbolic;
                } else if (key[i] >= '0' && key[i] <= '9') {
                    phones = ttf_resource_get(e, "digit_pronunciation", EN, digit_key, &part_layer);
                    if (phones && part_layer > layer)
                        layer = part_layer;
                    if (!phones)
                        return TTF_EINVAL;
                } else
                    return TTF_EINVAL;
                written = snprintf(out->phones + p, sizeof(out->phones) - p, "%s%s", p ? " " : "",
                                   phones);
                if (written < 0 || (size_t)written >= sizeof(out->phones) - p)
                    return TTF_EINVAL;
                p += (size_t)written;
                if (key[i] >= '0' && key[i] <= '9')
                    status = ttf_trace_resource(e, "digit_pronunciation", EN, digit_key, phones,
                                                "alnum-digit", phone_index);
                else
                    status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, EN,
                                            "algorithm", "alnum-letter", phones, digit_key,
                                            "generated-letter-phone", phone_index);
                if (status != TTF_OK)
                    return status;
                phone_index += phone_count(phones);
            }
            (void)snprintf(out->provenance, sizeof(out->provenance),
                           "ttf-pron-v1:%s:alnum-initialism", ttf_layer_name(layer));
            return p ? TTF_OK : TTF_EINVAL;
        }
    }
    for (i = 0; i < n;) {
        size_t take = n - i;
        char chunk[8];
        int written;
        /* Five letters is what an English suffix needs to be written whole
           (-ation, -reaso-); the validator holds consumers to the same bound. */
        if (take > 5)
            take = 5;
        phones = NULL;
        for (; take; --take) {
            /* Anchored keys let the data express word-initial and word-final
               context (^pre, e$) without a second matcher. */
            char anchored[10];
            const char *forms[3];
            size_t form, count_forms = 0;
            memcpy(chunk, key + i, take);
            chunk[take] = '\0';
            if (!i && i + take == n) {
                snprintf(anchored, sizeof(anchored), "^%s$", chunk);
                forms[count_forms++] = anchored;
            }
            if (i + take == n) {
                static char final_key[10];
                snprintf(final_key, sizeof(final_key), "%s$", chunk);
                forms[count_forms++] = final_key;
            }
            if (!i) {
                static char initial_key[10];
                snprintf(initial_key, sizeof(initial_key), "^%s", chunk);
                forms[count_forms++] = initial_key;
            }
            for (form = 0; form < count_forms && !phones; ++form)
                phones = ttf_resource_get(e, "g2p", EN, forms[form], &part_layer);
            if (!phones)
                phones = ttf_resource_get(e, "g2p", EN, chunk, &part_layer);
            if (phones) {
                if (part_layer > layer)
                    layer = part_layer;
                break;
            }
        }
        if (!phones)
            return TTF_EINVAL;
        /* "-" marks a silent grapheme such as a word-final e. */
        if (strcmp(phones, "-")) {
            written =
                snprintf(out->phones + p, sizeof(out->phones) - p, "%s%s", p ? " " : "", phones);
            if (written < 0 || (size_t)written >= sizeof(out->phones) - p)
                return TTF_ELIMIT;
            p += (size_t)written;
        }
        if ((status = ttf_trace_resource(e, "g2p", EN, chunk, phones, "longest-grapheme-rule",
                                         phone_index)) != TTF_OK)
            return status;
        phone_index += phone_count(phones);
        i += take;
    }
    (void)snprintf(out->provenance, sizeof(out->provenance),
                   "ttf-pron-v1:%s:grapheme-g2p-prototype", ttf_layer_name(layer));
    return p ? TTF_OK : TTF_EINVAL;
}

const ttf_language_pack_t ttf_pack_en_US = {sizeof(ttf_language_pack_t),
                                            TTF_ABI_VERSION,
                                            EN,
                                            NULL,
                                            script_score,
                                            context_score,
                                            classify,
                                            verbalize,
                                            pronounce,
                                            NULL,
                                            NULL};
