/* Korean reading policy and phonetic realization live in this language pack.
 * The core only dispatches through the versioned language-pack interface. */
#include "tts_text_frontend/ttf_language_pack.h"
#include "../../src/pronunciation/trace.h"
#include "../../src/core/internal.h" /* Builtin scalar grammar reuse; not public ABI. */
#include "../../src/semantic/phone.h"
#include "identifiers.h"
#include "speech.h"
#include "address.h"
#include "patterns.h"
#include "counters.h"
#include "../context_run.h"
#include "../../src/language/context_memo.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRY(expr)                                                                                  \
    do {                                                                                           \
        ttf_status_t status_ = (expr);                                                             \
        if (status_ != TTF_OK)                                                                     \
            return status_;                                                                        \
    } while (0)

static const char *resource(const ttf_engine_t *e, const char *kind, const char *key,
                            const char *fallback) {
    const char *value = ttf_resource_get(e, kind, "ko-KR", key, NULL);
    return value ? value : fallback;
}

static int resource_integer(const ttf_engine_t *e, const char *kind, const char *key, int minimum,
                            int maximum, int fallback) {
    const char *text = ttf_resource_get(e, kind, "ko-KR", key, NULL);
    char *end;
    long value;
    if (!text)
        return fallback;
    value = strtol(text, &end, 10);
    if (*end || value < minimum || value > maximum)
        return fallback;
    return (int)value;
}

static ttf_status_t digit(const ttf_engine_t *e, unsigned value, int telephone, ttf_buffer_t *out) {
    char key[2] = {(char)('0' + value), '\0'};
    static const char *const standard[] = {"영", "일", "이", "삼", "사",
                                           "오", "육", "칠", "팔", "구"};
    if (telephone && value == 0)
        return ttf_buffer_puts(out, resource(e, "number", "phone_zero", "공"));
    return ttf_buffer_puts(out, resource(e, "digit", key, standard[value]));
}

static const char *phone_zero(const ttf_engine_t *e) {
    ttf_resource_layer_t current_layer = TTF_LAYER_COMMON, legacy_layer = TTF_LAYER_COMMON;
    const char *current = ttf_resource_get(e, "phone_render", "ko-KR", "zero", &current_layer);
    const char *legacy = ttf_resource_get(e, "number", "ko-KR", "phone_zero", &legacy_layer);
    /* Keep higher-layer pre-v0.18 phone_zero overrides effective. */
    if (legacy && (!current || legacy_layer > current_layer))
        return legacy;
    return current ? current : "공"; /* Existing phone digit fallback. */
}
/* number_spacing digits spaced (reading-unit profile): one word per digit in a
   digit reading, 공 일 공; the default joins a run, 공일공. */
/* The spacing=reading-unit option selects the same style as the reading-unit
   resource profile; otherwise the resources decide (default article 44). */
static const char *group_spacing(const ttf_engine_t *e) {
    return e->reading_unit_spacing ? "unit" : resource(e, "number_spacing", "group", "compact");
}
static int spaced_digits(const ttf_engine_t *e) {
    return e->reading_unit_spacing ||
           !strcmp(resource(e, "number_spacing", "digits", "joined"), "spaced");
}
static ttf_status_t digit_gap(const ttf_engine_t *e, int *previous, ttf_buffer_t *out) {
    if (*previous && spaced_digits(e))
        TRY(ttf_buffer_puts(out, " "));
    *previous = 1;
    return TTF_OK;
}
static ttf_status_t phone_digits(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    int previous = 0;
    for (; *value; ++value) {
        const char *label;
        if (*value == '-') {
            label = ttf_resource_get(e, "phone_render", "ko-KR", "separator", NULL);
            if (!label)
                label = " "; /* Legacy phone group separator. */
            previous = 0;
        } else if (*value >= '0' && *value <= '9') {
            TRY(digit_gap(e, &previous, out));
            if (*value != '0') {
                TRY(digit(e, (unsigned)(*value - '0'), 0, out));
                continue;
            }
            label = phone_zero(e);
        } else
            return TTF_EINVAL;
        if (!label)
            return TTF_ERESOURCE;
        TRY(ttf_buffer_puts(out, label));
    }
    return TTF_OK;
}
static ttf_status_t phone(const ttf_engine_t *e, const ttf_semantic_t *s, ttf_buffer_t *out) {
    const char *value = s->value, *label;
    if (*value == '+') {
        label = ttf_resource_get(e, "phone_render", "ko-KR", "plus", NULL);
        if (!label)
            return TTF_ERESOURCE;
        TRY(ttf_buffer_puts(out, label));
        ++value;
    }
    TRY(phone_digits(e, value, out));
    if (s->secondary[0]) {
        label = ttf_resource_get(e, "phone_render", "ko-KR", "extension", NULL);
        if (!label)
            return TTF_ERESOURCE;
        TRY(ttf_buffer_puts(out, label));
        TRY(phone_digits(e, s->secondary, out));
    }
    return TTF_OK;
}

static ttf_status_t digits(const ttf_engine_t *e, const char *value, int telephone,
                           ttf_buffer_t *out) {
    int previous = 0;
    for (; *value; ++value) {
        if (*value >= '0' && *value <= '9') {
            TRY(digit_gap(e, &previous, out));
            TRY(digit(e, (unsigned)(*value - '0'), telephone, out));
        } else if (*value == '-' || *value == ' ') {
            TRY(ttf_buffer_puts(out, " "));
            previous = 0;
        } else if (*value == '.') {
            TRY(ttf_buffer_puts(out, resource(e, "number", "decimal", " 점 ")));
            previous = 0;
        }
    }
    return TTF_OK;
}

static ttf_status_t sino_group(const ttf_engine_t *e, unsigned value, ttf_buffer_t *out) {
    static const unsigned divisors[] = {1000, 100, 10, 1};
    static const char *const units[] = {"천", "백", "십", ""};
    size_t i;
    /* number_spacing group unit (reading-unit profile): 사천 삼백 육십 칠. */
    int spaced = !strcmp(group_spacing(e), "unit"), emitted = 0;
    for (i = 0; i < 4; ++i) {
        unsigned d = value / divisors[i];
        value %= divisors[i];
        if (!d)
            continue;
        if (emitted && spaced)
            TRY(ttf_buffer_puts(out, " "));
        emitted = 1;
        if (d != 1 || i == 3)
            TRY(digit(e, d, 0, out));
        if (i != 3) {
            char key[16];
            (void)snprintf(key, sizeof(key), "%u", divisors[i]);
            TRY(ttf_buffer_puts(out, resource(e, "magnitude", key, units[i])));
        }
    }
    return TTF_OK;
}

static ttf_status_t cardinal(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    char integer[TTF_IR_VALUE_SIZE];
    const char *p = value, *fraction = NULL;
    size_t n = 0, start, groups, i;
    static const char *const powers[] = {"",   "만", "억", "조", "경", "해",
                                         "자", "양", "구", "간", "정"};
    if (*p == '-') {
        TRY(ttf_buffer_puts(out, resource(e, "number", "negative", "마이너스 ")));
        ++p;
    } else if (*p == '+') {
        TRY(ttf_buffer_puts(out, resource(e, "number", "positive", "플러스 ")));
        ++p;
    }
    while (*p && *p != '.') {
        if (*p >= '0' && *p <= '9') {
            if (n + 1 >= sizeof(integer))
                return TTF_ELIMIT;
            integer[n++] = *p;
        } else if (*p != ',')
            return TTF_EINVAL;
        ++p;
    }
    if (*p == '.')
        fraction = p + 1;
    if (!n)
        return TTF_EINVAL;
    integer[n] = '\0';
    for (start = 0; start + 1 < n && integer[start] == '0'; ++start) {
    }
    n -= start;
    memmove(integer, integer + start, n + 1);
    if (n == 1 && integer[0] == '0')
        TRY(digit(e, 0, 0, out));
    else {
        groups = (n + 3) / 4;
        /* Arbitrary large identifiers still yield pronounceable output. */
        if (groups > sizeof(powers) / sizeof(powers[0])) {
            TRY(digits(e, integer, 0, out));
        } else {
            int emitted = 0;
            const char *style = group_spacing(e);
            int separate = groups > 1 && (!strcmp(style, "space") || !strcmp(style, "unit"));
            start = 0;
            for (i = groups; i > 0; --i) {
                size_t width = i == groups ? (n - 1) % 4 + 1 : 4, j;
                unsigned group = 0;
                for (j = 0; j < width; ++j)
                    group = group * 10 + (unsigned)(integer[start++] - '0');
                if (!group)
                    continue;
                if (emitted && separate)
                    TRY(ttf_buffer_puts(out, " "));
                if (!(group == 1 && i == 2))
                    TRY(sino_group(e, group, out));
                if (i > 1) {
                    char key[16];
                    (void)snprintf(key, sizeof(key), "10^%u", (unsigned)((i - 1) * 4));
                    TRY(ttf_buffer_puts(out, resource(e, "magnitude", key, powers[i - 1])));
                }
                emitted = 1;
            }
        }
    }
    if (fraction && *fraction) {
        TRY(ttf_buffer_puts(out, resource(e, "number", "decimal", " 점 ")));
        TRY(digits(e, fraction, 0, out));
    }
    return TTF_OK;
}

static ttf_status_t native_number(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    static const char *const ones[] = {"",     "한",   "두",   "세",   "네",
                                       "다섯", "여섯", "일곱", "여덟", "아홉"};
    static const char *const tens[] = {"",   "열",   "스물", "서른", "마흔",
                                       "쉰", "예순", "일흔", "여든", "아흔"};
    char *end;
    unsigned long n = strtoul(value, &end, 10);
    char key[16];
    const char *override;
    if (*end || n == 0 || n > 99)
        return cardinal(e, value, out);
    (void)snprintf(key, sizeof(key), "%lu", n);
    override = ttf_resource_get(e, "native", "ko-KR", key, NULL);
    if (override)
        return ttf_buffer_puts(out, override);
    if (n == 20)
        return ttf_buffer_puts(out, resource(e, "native", "20", "스무"));
    if (n >= 10) {
        (void)snprintf(key, sizeof(key), "%lu", n / 10 * 10);
        TRY(ttf_buffer_puts(out, resource(e, "native_tens", key, tens[n / 10])));
    }
    if (n % 10) {
        (void)snprintf(key, sizeof(key), "%lu", n % 10);
        TRY(ttf_buffer_puts(out, resource(e, "native", key, ones[n % 10])));
    }
    return TTF_OK;
}

/* Explicit controls are deliberately narrower than general TN syntax. */
static int controlled_decimal(const char *value) {
    size_t i = 0, digits = 0, first;
    if (value[i] == '+' || value[i] == '-')
        ++i;
    first = i;
    while (value[i] >= '0' && value[i] <= '9') {
        ++i;
        ++digits;
    }
    if (!digits)
        return 0;
    while (first + 1 < i && value[first] == '0')
        ++first;
    if (i - first > 44)
        return 0;
    if (value[i] == '.') {
        size_t first = ++i;
        while (value[i] >= '0' && value[i] <= '9')
            ++i;
        if (i == first)
            return 0;
    }
    return !value[i] && i < TTF_IR_VALUE_SIZE;
}
static ttf_status_t standalone_native(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    unsigned number = 0;
    size_t i;
    char key[8];
    const char *label;
    if (!*value || strlen(value) > 2)
        return TTF_EINVAL;
    for (i = 0; value[i]; ++i) {
        if (value[i] < '0' || value[i] > '9')
            return TTF_EINVAL;
        number = number * 10 + (unsigned)(value[i] - '0');
    }
    if (number > 99)
        return TTF_EINVAL;
    snprintf(key, sizeof(key), "%u", number);
    label = ttf_resource_get(e, "native_standalone", "ko-KR", key, NULL);
    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, strlen(value));
    if (label) {
        TRY(ttf_trace_resource(e, "native_standalone", "ko-KR", key, value,
                               "standalone-native-exact", SIZE_MAX));
        return ttf_buffer_puts(out, label);
    }
    if (!number)
        return TTF_ERESOURCE;
    if (number >= 10) {
        snprintf(key, sizeof(key), "%u", number / 10 * 10);
        label = ttf_resource_get(e, "native_standalone", "ko-KR", key, NULL);
        if (!label)
            return TTF_ERESOURCE;
        TRY(ttf_trace_resource(e, "native_standalone", "ko-KR", key, value,
                               "standalone-native-tens", SIZE_MAX));
        TRY(ttf_buffer_puts(out, label));
    }
    if (number % 10) {
        snprintf(key, sizeof(key), "%u", number % 10);
        label = ttf_resource_get(e, "native_standalone", "ko-KR", key, NULL);
        if (!label)
            return TTF_ERESOURCE;
        TRY(ttf_trace_resource(e, "native_standalone", "ko-KR", key, value,
                               "standalone-native-ones", SIZE_MAX));
        TRY(ttf_buffer_puts(out, label));
    }
    return TTF_OK;
}

static ttf_status_t quantity(const ttf_engine_t *e, const char *value, const char *unit, int native,
                             ttf_buffer_t *out) {
    if (native)
        TRY(native_number(e, value, out));
    else
        TRY(cardinal(e, value, out));
    TRY(ttf_buffer_puts(out, " "));
    return ttf_buffer_puts(out, unit);
}

/* A label read run by run: digits as a cardinal, letters by name, a hyphen as
   다시; spaced puts a space between a number and letters (20F 이십 에프). */
static ttf_status_t render_label(const ttf_engine_t *e, const char *v, size_t n, int spaced,
                                 ttf_buffer_t *out) {
    size_t k = 0;
    while (k < n) {
        if (v[k] >= '0' && v[k] <= '9') {
            char number[16];
            size_t m = 0;
            while (k < n && ((v[k] >= '0' && v[k] <= '9') ||
                             (v[k] == '.' && m && k + 1 < n && v[k + 1] >= '0' && v[k + 1] <= '9')) &&
                   m + 1 < sizeof(number))
                number[m++] = v[k++]; /* 3.3T 삼 점 삼 티 */
            number[m] = '\0';
            TRY(cardinal(e, number, out));
            if (spaced && k < n && v[k] != '-')
                TRY(ttf_buffer_puts(out, " "));
        } else if (v[k] == '-') {
            TRY(ttf_buffer_puts(out, resource(e, "identifier_render", "separator", " 다시 ")));
            ++k;
        } else {
            char key[] = "letter.X";
            const char *name;
            key[7] = (char)(v[k] >= 'a' && v[k] <= 'z' ? v[k] - 32 : v[k]);
            name = ttf_resource_get(e, "phone_letter", "ko-KR", key, NULL);
            if (!name)
                return TTF_ERESOURCE;
            TRY(ttf_buffer_puts(out, name));
            ++k;
            if (spaced && k < n && v[k] >= '0' && v[k] <= '9')
                TRY(ttf_buffer_puts(out, " "));
        }
    }
    return TTF_OK;
}

/* A run of five or more digits in a code is read in equal groups of five,
   four or three, the largest that divides it (5, 6 3-3, 8 4-4, 9 3-3-3, 10 5-5,
   12 4-4-4); otherwise five first, or four when only seven are left, and the
   rest again (7 4-3, 11 5-3-3, 13 5-4-4). Letters and shorter runs are copied
   (516123813ZC 오일육 일이삼 팔일삼 제트씨). Returns 0 when it does not fit. */
static int group_digit_runs(const char *value, char *out, size_t size) {
    size_t used = 0, at = 0, n = strlen(value);
    while (at < n) {
        size_t run = at;
        while (run < n && value[run] >= '0' && value[run] <= '9')
            ++run;
        if (run == at) {
            if (used + 2 > size)
                return 0;
            out[used++] = value[at++];
            continue;
        }
        if (run - at < 5) {
            if (used + (run - at) + 1 > size)
                return 0;
            memcpy(out + used, value + at, run - at);
            used += run - at;
            at = run;
            continue;
        }
        {
            size_t group = 0, first = 1;
            while (at < run) {
                size_t left = run - at, take;
                if (!group)
                    group = !(left % 5) ? 5 : !(left % 4) ? 4 : !(left % 3) ? 3 : 0;
                take = group ? group : left >= 8 ? 5 : left == 7 ? 4 : left;
                if (used + take + 2 > size)
                    return 0;
                if (!first)
                    out[used++] = ' ';
                memcpy(out + used, value + at, take);
                used += take;
                at += take;
                first = 0;
            }
        }
    }
    out[used] = '\0';
    return 1;
}
/* English numerals up to ten from the shared lexicon words (6 식스, 10 텐),
   the same words letter labels use (K5 케이파이브). NULL above ten. */
static const char *english_numeral(const ttf_engine_t *e, const char *value) {
    static const char *const names[] = {"zero", "one", "two", "three", "four", "five",
                                        "six",  "seven", "eight", "nine", "ten"};
    char key[16];
    unsigned number = 0;
    if (!*value || strlen(value) > 2)
        return NULL;
    for (const char *p = value; *p; ++p) {
        if (*p < '0' || *p > '9')
            return NULL;
        number = number * 10 + (unsigned)(*p - '0');
    }
    if (number > 10)
        return NULL;
    (void)snprintf(key, sizeof(key), "en-US:%s", names[number]);
    return ttf_resource_get(e, "lexicon", "ko-KR", key, NULL);
}
static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *out);
static ttf_status_t expression_number(const ttf_engine_t *e, const char *value, ttf_buffer_t *out);
static ttf_status_t expression_letter(const ttf_engine_t *e, char letter, ttf_buffer_t *out) {
    char key[16];
    const char *label;
    snprintf(key, sizeof(key), "letter.%c",
             letter >= 'a' && letter <= 'z' ? letter - 'a' + 'A' : letter);
    label = ttf_resource_get(e, "phone_letter", "ko-KR", key, NULL);
    return label ? ttf_buffer_puts(out, label) : TTF_ERESOURCE;
}

/* Written fallback (project-owner decision 2026-10-02): in Korean speech a span
   the grammars rejected is read as written instead of kept (27만 천 원 이십칠만 천
   원, 1970년 2월 29일, 4~5억 사에서 오억, 9번째라 아홉번째라, 52,7% 오십이 점 칠
   퍼센트). Numbers read as cardinals, native before a native counter; a code
   (a leading zero, five or more digits, hyphen groups, three or more letters,
   letters before digits) is read run by run like an identifier (088-696-8173,
   9034MDSAP); letters read as a unit or by name; ~ 에서, % 퍼센트, : 대; Hangul
   and other text are copied. */
static int fb_digit(char c) { return c >= '0' && c <= '9'; }
static int fb_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int fb_chunk_char(char c) { return fb_digit(c) || fb_letter(c) || (c && strchr(".,:/~-+%", c)); }
/* The longest counter written right after the number decides native (9번째,
   4시간) or Sino (2월, 38분). */
static int fb_native_after(const ttf_engine_t *e, const char *p, size_t n) {
    char key[16];
    size_t len = 0;
    int native = 0;
    while (len + 3 <= n && len + 3 < sizeof(key) && (unsigned char)p[len] >= 0xEA &&
           (unsigned char)p[len] <= 0xED) {
        const char *kind;
        len += 3;
        memcpy(key, p, len);
        key[len] = '\0';
        if ((kind = ttf_resource_get(e, "counter", "ko-KR", key, NULL)) ||
            (kind = ttf_resource_get(e, "native_ordinal_suffix", "ko-KR", key, NULL)))
            native = !strcmp(kind, "native");
    }
    return native;
}
static ttf_status_t fb_letters(const ttf_engine_t *e, const char *v, size_t n, ttf_buffer_t *out) {
    char key[40];
    const char *got;
    size_t k;
    if (n + 7 < sizeof(key)) {
        memcpy(key, v, n);
        key[n] = '\0';
        /* 500ml 오백 밀리리터, 1,5L 일 점 오 리터, 2m 이 미터; other single capitals
           are names (470G 사백칠십 지). */
        got = n >= 2 || !strcmp(key, "L") || (v[0] >= 'a' && v[0] <= 'z')
                  ? ttf_resource_get(e, "numeric_suffix", "*", key, NULL) : NULL;
        if (got && !strncmp(got, "MEASURE:", 8) &&
            (got = ttf_resource_get(e, "unit", "ko-KR", got + 8, NULL)))
            return ttf_buffer_puts(out, got);
        if (n >= 3) {
            (void)snprintf(key, sizeof(key), "en-US:");
            for (k = 0; k < n; ++k)
                key[6 + k] = (char)(v[k] >= 'A' && v[k] <= 'Z' ? v[k] + 32 : v[k]);
            key[6 + n] = '\0';
            if ((got = ttf_resource_get(e, "lexicon", "ko-KR", key, NULL)))
                return ttf_buffer_puts(out, got);
        }
    }
    for (k = 0; k < n; ++k)
        TRY(expression_letter(e, v[k], out));
    return TTF_OK;
}
static int fb_is_code(const char *v, size_t n) {
    size_t k, run = 0, caps = 0;
    int letter_digit = 0, comma = 0, dot = 0, dots = 0;
    for (k = 0; k < n; ++k) {
        comma |= v[k] == ',';
        dot |= v[k] == '.';
        dots += v[k] == '.' && k + 1 < n && fb_digit(v[k + 1]);
    }
    for (k = 0; k < n; ++k) {
        char c = v[k];
        if (fb_digit(c)) {
            if (!run && c == '0' && k + 1 < n && fb_digit(v[k + 1]) && (!k || (v[k - 1] != '.' && v[k - 1] != ',')))
                return 1; /* 088, 0620G */
            if (++run >= 5 && !comma)
                return 1;
            if (k && fb_letter(v[k - 1]))
                letter_digit = 1;
            caps = 0;
        } else {
            run = 0;
            if (fb_letter(c)) {
                if (c >= 'A' && c <= 'Z' && ++caps >= 3)
                    return 1; /* 9034MDSAP */
            } else
                caps = 0;
            if (c == '-' && k && k + 1 < n && (fb_digit(v[k - 1]) || fb_letter(v[k - 1])))
                return 1; /* 83714L-63446, 8922-0918--3434 */
        }
    }
    return dots >= 2 || (letter_digit && !comma && !dot) || (n && v[n - 1] == '-');
}
static ttf_status_t fb_code(const ttf_engine_t *e, const char *v, size_t n, ttf_buffer_t *out) {
    size_t k = 0;
    const char *separator = NULL;
    while (k < n) {
        char piece[TTF_IR_VALUE_SIZE], grouped[TTF_IR_VALUE_SIZE * 2];
        size_t m = 0;
        while (k < n && !strchr(".,:/~", v[k])) {
            char c = v[k++];
            if (c == '-' && (!m || piece[m - 1] == '-' || k == n || strchr(".,:/~", v[k])))
                continue; /* 015- and -- read one 다시 or none */
            if (m + 1 < sizeof(piece))
                piece[m++] = (char)(c >= 'a' && c <= 'z' ? c - 32 : c);
        }
        piece[m] = '\0';
        if (m) {
            if (separator)
                TRY(ttf_buffer_puts(out, separator));
            TRY(ttf_ko_render_identifier(e, group_digit_runs(piece, grouped, sizeof(grouped)) ? grouped : piece, out));
        }
        if (k < n) {
            separator = v[k] == '.' ? resource(e, "number", "decimal", " 점 ") : v[k] == ':' ? " 대 " : v[k] == '~' ? "에서 " : " ";
            ++k;
        }
    }
    return TTF_OK;
}
static ttf_status_t fb_plain(const ttf_engine_t *e, const char *v, size_t n, const char *after, size_t after_n,
                             ttf_buffer_t *out) {
    size_t k = 0;
    int number_before = 0;
    while (k < n) {
        char c = v[k];
        if (fb_digit(c) || (!k && (c == '-' || c == '+' || c == '.') && k + 1 < n && fb_digit(v[k + 1]))) {
            char number[64];
            size_t m = 0;
            int decimal = 0;
            if (c == '-' || c == '+')
                number[m++] = v[k++];
            while (k < n && m + 2 < sizeof(number)) {
                if (fb_digit(v[k]))
                    number[m++] = v[k++];
                else if (v[k] == ',' && !decimal && k + 3 < n && fb_digit(v[k + 1]) &&
                         fb_digit(v[k + 2]) && fb_digit(v[k + 3]) && (k + 4 >= n || !fb_digit(v[k + 4])))
                    number[m++] = v[k++]; /* 25,000 */
                else if (v[k] == ',' && !decimal && k + 1 < n && fb_digit(v[k + 1])) {
                    size_t q = k + 1;
                    while (q < n && fb_digit(v[q]))
                        ++q;
                    if (q - k - 1 > 2 || q >= n || !(v[q] == '%' || fb_letter(v[q])))
                        break; /* 7,8월 is a list */
                    number[m++] = '.'; /* 52,7% 오십이 점 칠 */
                    ++k;
                    decimal = 1;
                } else if (v[k] == '.' && !decimal && k + 1 < n && fb_digit(v[k + 1])) {
                    number[m++] = v[k++];
                    decimal = 1;
                } else
                    break;
            }
            number[m] = '\0';
            if (k == n && !decimal && m <= 2 && fb_digit(number[0]) && strcmp(number, "0") &&
                fb_native_after(e, after, after_n))
                TRY(native_number(e, number, out));
            else
                TRY(cardinal(e, number, out));
            number_before = 1;
            continue;
        }
        if (fb_letter(c)) {
            size_t s = k;
            while (k < n && fb_letter(v[k]))
                ++k;
            if (number_before)
                TRY(ttf_buffer_puts(out, " "));
            TRY(fb_letters(e, v + s, k - s, out));
            number_before = 0;
            continue;
        }
        number_before = 0;
        ++k;
        if (c == '%') {
            int point = k < n && (v[k] == 'P' || v[k] == 'p');
            TRY(ttf_buffer_puts(out, " "));
            TRY(ttf_buffer_puts(out, point ? resource(e, "unit", "percentage_point", "퍼센트포인트")
                                           : resource(e, "unit", "percent", "퍼센트")));
            k += point;
        } else if (c == '~')
            TRY(ttf_buffer_puts(out, resource(e, "policy", "range_separator", "에서 ")));
        else if (c == ':')
            TRY(ttf_buffer_puts(out, " 대 "));
        else if (c == ',')
            TRY(ttf_buffer_puts(out, ", "));
        else if (c == '/')
            TRY(ttf_buffer_puts(out, " 슬래시 "));
        else if (c == '-')
            TRY(ttf_buffer_puts(out, resource(e, "identifier_render", "separator", " 다시 ")));
        else if (c == '+')
            TRY(ttf_buffer_puts(out, " 플러스 "));
        else
            TRY(ttf_buffer_append(out, &c, 1));
    }
    return TTF_OK;
}
/* ０９：０５ 아홉 시 오 분: a chunk shaped like a valid clock time reads as one. */
static int fb_time(const ttf_engine_t *e, const char *v, size_t n, ttf_buffer_t *out, ttf_status_t *st) {
    unsigned part[3] = {0, 0, 0}, count = 0, width = 0;
    size_t k;
    char number[8];
    for (k = 0; k < n; ++k) {
        if (fb_digit(v[k])) {
            if (++width > 2)
                return 0;
            part[count] = part[count] * 10 + (unsigned)(v[k] - '0');
        } else if (v[k] == ':' && width && (count || width <= 2) && count < 2) {
            if (count && width != 2)
                return 0;
            ++count;
            width = 0;
        } else
            return 0;
    }
    if (!count || width != 2 || part[0] > 23 || part[1] > 59 || part[2] > 59)
        return 0;
    (void)snprintf(number, sizeof(number), "%u", part[0]);
    *st = part[0] >= 1 && part[0] <= 12 ? native_number(e, number, out) : cardinal(e, number, out);
    if (*st == TTF_OK)
        *st = ttf_buffer_puts(out, " 시");
    for (k = 1; *st == TTF_OK && k <= count; ++k)
        if (part[k]) {
            (void)snprintf(number, sizeof(number), " ");
            *st = ttf_buffer_puts(out, number);
            (void)snprintf(number, sizeof(number), "%u", part[k]);
            if (*st == TTF_OK)
                *st = cardinal(e, number, out);
            if (*st == TTF_OK)
                *st = ttf_buffer_puts(out, k == 1 ? " 분" : " 초");
        }
    return 1;
}
static ttf_status_t written_read(const ttf_engine_t *e, const char *text, size_t length, ttf_buffer_t *out) {
    char *v = (char *)malloc(length + 1);
    size_t n = 0, k = 0;
    ttf_status_t st = TTF_OK;
    if (!v)
        return TTF_ENOMEM;
    while (k < length) {
        const unsigned char *u = (const unsigned char *)text + k;
        if (k + 3 <= length && u[0] == 0xEF && ((u[1] == 0xBC && u[2] >= 0x81) || (u[1] == 0xBD && u[2] <= 0x9E))) {
            /* Fullwidth forms U+FF01..U+FF5E (１２，５００％) read like ASCII. */
            v[n++] = (char)(u[1] == 0xBC ? u[2] - 0x60 : u[2] - 0x20);
            k += 3;
        } else if (k + 3 <= length && !memcmp(text + k, "\xe2\x88\xbc", 3)) {
            v[n++] = '~'; /* ∼ */
            k += 3;
        } else if (k + 3 <= length && (!memcmp(text + k, "\xe2\x80\x89", 3) || !memcmp(text + k, "\xe2\x80\xaf", 3))) {
            v[n++] = ' '; /* thin and narrow no-break spaces */
            k += 3;
        } else if (k + 2 <= length && !memcmp(text + k, "\xc2\xa0", 2)) {
            v[n++] = ' '; /* no-break space */
            k += 2;
        } else
            v[n++] = text[k++];
    }
    k = 0;
    while (st == TTF_OK && k < n) {
        size_t s = k, w;
        int alnum = 0;
        const char *currency = NULL;
        /* $12,500 만 이천오백 달러: a currency prefix is read after its amount. */
        for (w = 1; w <= 3 && !currency && k + w < n; w += 2) {
            char key[8];
            const char *code;
            memcpy(key, v + k, w);
            key[w] = '\0';
            if (fb_digit(v[k + w]) && (code = ttf_resource_get(e, "currency_prefix", "*", key, NULL)) &&
                (currency = ttf_resource_get(e, "currency", "ko-KR", code, NULL)))
                s = k = k + w;
        }
        while (k < n && fb_chunk_char(v[k])) {
            alnum |= fb_digit(v[k]) || fb_letter(v[k]);
            ++k;
        }
        if (k > s && alnum && fb_time(e, v + s, k - s, out, &st)) {
        } else if (k > s && alnum) {
            st = fb_is_code(v + s, k - s) ? fb_code(e, v + s, k - s, out)
                                         : fb_plain(e, v + s, k - s, v + k, n - k, out);
            if (st == TTF_OK && currency && (st = ttf_buffer_puts(out, " ")) == TTF_OK)
                st = ttf_buffer_puts(out, currency);
        } else if (k > s)
            st = ttf_buffer_append(out, v + s, k - s);
        else
            st = ttf_buffer_append(out, v + k++, 1);
    }
    free(v);
    return st;
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
        if (at && (st = ttf_buffer_puts(out, " ")) != TTF_OK)
            return st;
        if ((part[0] >= '0' && part[0] <= '9') || (part[0] == '-' && part[1]))
            st = expression_number(e, part, out);
        else if (!part[1] &&
                 ((part[0] >= 'a' && part[0] <= 'z') || (part[0] >= 'A' && part[0] <= 'Z')))
            st = expression_letter(e, part[0], out);
        else {
            const char *label = ttf_resource_get(e, "math_operator", "ko-KR", part, NULL);
            if (!label)
                return TTF_ERESOURCE;
            st = ttf_trace_resource(e, "math_operator", "ko-KR", part, part, "expression-operator",
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
        template = ttf_resource_get(e, "math_relation", "ko-KR", key, NULL);
        if (!template)
            return TTF_ERESOURCE;
        if ((st = ttf_trace_resource(e, "math_relation", "ko-KR", key, key, "expression-relation",
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
    return cardinal(e, value, out);
}

/* 2.1.3 and 192.168.0.1: dotted numeric parts with an editable part reading and
   separator. A leading-zero part keeps its digits; a v prefix is spelled. */
static ttf_status_t dotted_numbers(const ttf_engine_t *e, const ttf_semantic_t *s,
                                   const char *surface, size_t length, ttf_buffer_t *out) {
    const char *kind = s->type == TTF_SEM_IP_ADDRESS ? "ip" : "version";
    const char *policy = ttf_resource_get(e, "electronic_number", "ko-KR", kind, NULL);
    const char *separator = ttf_resource_get(e, "electronic_separator", "ko-KR", kind, NULL);
    size_t at = 0;
    ttf_status_t status;
    if (!policy || !separator)
        return TTF_ERESOURCE;
    if (s->secondary[0]) {
        if (!length || surface[0] != s->secondary[0])
            return TTF_EINVAL;
        if ((status = ttf_ko_speech_spelling(e, surface, 1, 0, out)) != TTF_OK ||
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
        part.type = !strcmp(policy, "digits") || (end - at > 1 && surface[at] == '0')
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

/* Bytes of a range mark at text: ~, -, or U+223C/U+FF5E/U+301C; 0 otherwise. */
static size_t range_mark_width(const char *text, size_t length) {
    const unsigned char *u = (const unsigned char *)text;
    if (length && (text[0] == '~' || text[0] == '-'))
        return 1;
    if (length >= 3 && ((u[0] == 0xe2 && u[1] == 0x88 && u[2] == 0xbc) ||
                        (u[0] == 0xef && u[1] == 0xbd && u[2] == 0x9e) ||
                        (u[0] == 0xe3 && u[1] == 0x80 && u[2] == 0x9c)))
        return 3;
    return 0;
}
/* Whether the written first endpoint ends at the range mark: its digits and an
   optional scale word (1만~3만 원) are followed by ~ or a tilde variant, not by
   a unit of its own (3개~4개). Unknown surfaces (full-width digits) share. */
static int range_unit_shared(const char *surface, size_t length) {
    static const char *const scales[] = {"만", "억", "조", "천"};
    size_t i = 0, k;
    if (!surface || !length)
        return 1;
    while (i < length) {
        const unsigned char *u = (const unsigned char *)surface + i;
        if ((surface[i] >= '0' && surface[i] <= '9') || surface[i] == ',' || surface[i] == '.')
            ++i;
        else if (length - i >= 3 && u[0] == 0xef && u[1] == 0xbc && u[2] >= 0x90 && u[2] <= 0x99)
            i += 3;
        else
            break;
    }
    if (!i)
        return 0; /* ₩1,000~₩2,000, B1~B3층: the first end carries its own unit. */
    for (k = 0; k < sizeof(scales) / sizeof(scales[0]); ++k)
        if (length - i >= strlen(scales[k]) && !strncmp(surface + i, scales[k], strlen(scales[k]))) {
            i += strlen(scales[k]);
            break;
        }
    while (i < length && surface[i] == ' ')
        ++i;
    return range_mark_width(surface + i, length - i) > 0;
}
/* A unit declared as a measure suffix (kg, km, cm, L) is a metric measure. */
static int metric_unit(const ttf_engine_t *e, const char *unit) {
    const char *suffix = unit[0] ? ttf_resource_get(e, "numeric_suffix", "*", unit, NULL) : NULL;
    return suffix && !strncmp(suffix, "MEASURE:", 8);
}
/* Range endpoints read with native numerals: native counters, frequency and
   whole clock hours (두세 시). */
static int native_endpoint(const ttf_engine_t *e, const ttf_semantic_t *endpoint) {
    const char *max;
    if (endpoint->type == TTF_SEM_TIME)
        return 1;
    /* counter_native_max: a range ending in a decimal or a count above it is Sino
       (2~4.1배 이에서 사 점 일 배, 20~30배 이십에서 삼십 배; 2026-10-04). */
    if (endpoint->type == TTF_SEM_COUNTER &&
        (max = ttf_resource_get(e, "counter_native_max", "ko-KR", endpoint->unit, NULL)) &&
        (strchr(endpoint->value, '.') || strlen(endpoint->value) > 2 || atoi(endpoint->value) > atoi(max)))
        return 0;
    return endpoint->type == TTF_SEM_COUNTER &&
           (!strcmp(endpoint->secondary, "frequency") ||
            (strcmp(endpoint->secondary, "identifier") &&
             !strcmp(resource(e, "counter", endpoint->unit, "sino"), "native")));
}
static int plain_integer(const char *value, unsigned long *out) {
    char *end;
    if (!value[0] || value[0] < '1' || value[0] > '9' || strlen(value) > 12)
        return 0;
    *out = strtoul(value, &end, 10);
    return !*end;
}
/* 2~3만 원 이삼만 원, 3~4개 서너 개, 20~30명 이삼십 명: two adjacent approximate
   numbers k·10^n and (k+1)·10^n read as one contracted numeral (policy
   range_contract adjacent; the words are range_contract resources). The second
   endpoint keeps its unit reading; ten and above with k = 1 (10~20, 1~2만) and
   numbers the renderer does not start with the plain numeral are not contracted. */
static ttf_status_t range_contraction(const ttf_engine_t *e, const ttf_semantic_t *s,
                                      const ttf_semantic_t *endpoint, const char *surface,
                                      size_t length, ttf_buffer_t *out, int *done) {
    unsigned long a, b, scale = 1;
    unsigned k, n = 0;
    int native;
    char key[16], place[24];
    const char *pair;
    ttf_semantic_t second = *endpoint;
    ttf_buffer_t tail = {0}, lead = {0}, unit_place = {0};
    ttf_status_t status = TTF_OK;
    *done = 0;
    /* A metric measure reads 일에서 이 킬로그램 (references 20:17); counters,
       periods, money and percent contract. */
    if (strcmp(resource(e, "policy", "range_contract", "adjacent"), "adjacent") || !s->unit[0] ||
        !strcmp(s->rule, "ko:identifier-range") || endpoint->type == TTF_SEM_ORDINAL ||
        endpoint->type == TTF_SEM_MEASURE || metric_unit(e, s->unit) ||
        !plain_integer(s->value, &a) || !plain_integer(s->secondary, &b) || b <= a)
        return TTF_OK;
    while (a % (scale * 10) == 0 && b % (scale * 10) == 0) {
        scale *= 10;
        ++n;
    }
    if (a % scale || a / scale < 1 || a / scale > 8 || b != a + scale || (n && a / scale == 1))
        return TTF_OK;
    k = (unsigned)(a / scale);
    native = native_endpoint(e, endpoint) && n == 0;
    snprintf(key, sizeof(key), "%s:%u", native ? "native" : "sino", k);
    pair = ttf_resource_get(e, "range_contract", "ko-KR", key, NULL);
    if (!pair)
        return TTF_OK;
    memcpy(second.value, s->secondary, sizeof(second.value));
    status = verbalize(e, &second, surface, length, &tail);
    /* The second reading must begin with its own numeral (삼만, 세, 서른). */
    if (status == TTF_OK)
        status = endpoint->type == TTF_SEM_COUNTER && native_endpoint(e, endpoint)
                     ? native_number(e, s->secondary, &lead)
                 : endpoint->type == TTF_SEM_TIME ? native_number(e, s->secondary, &lead)
                                                  : cardinal(e, s->secondary, &lead);
    if (status == TTF_OK && n) {
        snprintf(place, sizeof(place), "1%0*u", (int)n, 0u);
        status = cardinal(e, place, &unit_place);
    }
    if (status == TTF_OK) {
        const char *rest = NULL;
        if (tail.data && lead.data && !strncmp(tail.data, lead.data, lead.length))
            rest = tail.data + lead.length;
        else if (!strcmp(s->unit, "월") && tail.data &&
                 (!strcmp(tail.data, "유월") || !strcmp(tail.data, "시월")))
            rest = " 월"; /* 5~6월 오륙 월: the contraction keeps plain numerals. */
        if (rest) {
            status = ttf_buffer_puts(out, pair);
            /* 10^8 reads 일억 alone; after a numeral it is 억 (사오억). */
            if (status == TTF_OK && unit_place.data)
                status = ttf_buffer_puts(out, !strncmp(unit_place.data, "일", strlen("일"))
                                                  ? unit_place.data + strlen("일")
                                                  : unit_place.data);
            if (status == TTF_OK)
                status = ttf_buffer_puts(out, rest);
            if (status == TTF_OK) {
                *done = 1;
                ttf_trace_stage(e, TTF_TRACE_POLICY, 0, length);
                status = ttf_trace_resource(e, "range_contract", "ko-KR", key, surface,
                                            "adjacent-range-contracted", SIZE_MAX);
            }
        }
    }
    ttf_buffer_free(&tail);
    ttf_buffer_free(&lead);
    ttf_buffer_free(&unit_place);
    return status;
}
/* After the range mark: whether the second number is written with a decimal
   point (1~1.5만 원). */
static int second_written_decimal(const char *mark, size_t length) {
    size_t i = range_mark_width(mark, length);
    while (i < length && mark[i] == ' ')
        ++i;
    while (i < length && ((mark[i] >= '0' && mark[i] <= '9') || mark[i] == ','))
        ++i;
    return i < length && mark[i] == '.';
}
/* A wider range reads its first number without the unit (3~5kg 삼에서 오
   킬로그램). A first number written without the shared scale (1~3만 원) is read
   as written; a native counter uses the standalone numeral (2~5개 둘에서). */
static ttf_status_t range_first_bare(const ttf_engine_t *e, const ttf_semantic_t *s,
                                     const ttf_semantic_t *endpoint, const char *surface,
                                     size_t length, ttf_buffer_t *out) {
    char written[TTF_IR_VALUE_SIZE];
    size_t i = 0, w = 0;
    unsigned long value, as_written;
    while (surface && i < length && w + 1 < sizeof(written) &&
           ((surface[i] >= '0' && surface[i] <= '9') || surface[i] == ',')) {
        if (surface[i] != ',')
            written[w++] = surface[i];
        ++i;
    }
    written[w] = 0;
    while (i < length && surface[i] == ' ')
        ++i;
    /* Only digits directly before the range mark share the second's scale, and
       only when the second is written as a plain integer too. */
    if (w && range_mark_width(surface + i, length - i) &&
        !second_written_decimal(surface + i, length - i) && plain_integer(s->value, &value) &&
        plain_integer(written, &as_written) && as_written < value)
        return cardinal(e, written, out);
    const char *max = ttf_resource_get(e, "counter_native_max", "ko-KR", s->unit, NULL);
    /* 2~4.1배 이에서 사 점 일 배: the second endpoint past the native limit makes the range Sino. */
    if (max && (strchr(s->secondary, '.') || strlen(s->secondary) > 2 || atoi(s->secondary) > atoi(max)))
        return cardinal(e, s->value, out);
    if (native_endpoint(e, endpoint) && endpoint->type == TTF_SEM_COUNTER &&
        plain_integer(s->value, &value) && value <= 99 &&
        standalone_native(e, s->value, out) == TTF_OK)
        return TTF_OK;
    return cardinal(e, s->value, out);
}
static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *out) {
    switch (s->type) {
    case TTF_SEM_CUSTOM:
        if (!strcmp(s->unit, "@native"))
            return !s->value[0] && length ? TTF_ELIMIT : standalone_native(e, s->value, out);
        if (!strcmp(s->unit, "@sino"))
            return !s->value[0] && length ? TTF_ELIMIT :
                (controlled_decimal(s->value) ? cardinal(e, s->value, out) : TTF_EINVAL);
        if (!strcmp(s->unit, "@alphabet"))
            return !strcmp(s->secondary, "control-surface")
                       ? ttf_ko_speech_spelling(e, surface, length, 0, out)
                       : ttf_ko_speech_spelling(e, s->value, strlen(s->value), 0, out);
        if (!strcmp(s->unit, "@email") || !strcmp(s->unit, "@url"))
            return ttf_electronic_control(e, s, surface, length, out);
        if (!strcmp(s->unit, "@address"))
            return ttf_ko_address(e, surface, length, out);
        return ttf_semantic_control(e, s, surface, length, out);
    case TTF_SEM_VERSION:
    case TTF_SEM_IP_ADDRESS:
        return dotted_numbers(e, s, surface, length, out);
    case TTF_SEM_EMAIL:
    case TTF_SEM_URL:
        return ttf_ko_speech_spelling(e, surface, length, 1, out);
    case TTF_SEM_NUMBER:
    case TTF_SEM_CARDINAL:
        if (!strcmp(s->rule, "ko:number-list")) {
            /* 1,2 일, 이 */
            const char *p = s->value;
            char part[8];
            while (*p) {
                size_t k = 0;
                while (p[k] >= '0' && p[k] <= '9' && k + 1 < sizeof(part)) {
                    part[k] = p[k];
                    ++k;
                }
                part[k] = '\0';
                TRY(cardinal(e, part, out));
                p += k;
                if (*p == ',') {
                    TRY(ttf_buffer_puts(out, ", "));
                    ++p;
                } else if (*p)
                    return TTF_EINVAL;
            }
            return TTF_OK;
        }
        if (!strcmp(s->rule, "ko:model-number")) {
            char digits[4];
            size_t k = 0;
            const char *word;
            while (s->value[k] >= '0' && s->value[k] <= '9' && k + 1 < sizeof(digits)) {
                digits[k] = s->value[k];
                ++k;
            }
            digits[k] = '\0';
            word = english_numeral(e, digits);
            if (word || !s->value[k]) {
                TRY(word ? ttf_buffer_puts(out, word) : cardinal(e, digits, out));
                return s->value[k] ? render_label(e, s->value + k, strlen(s->value + k), 0, out) : TTF_OK;
            }
        }
        return cardinal(e, s->value, out);
    case TTF_SEM_ORDINAL:
        if (ttf_resource_get(e, "native_ordinal_suffix", "ko-KR", s->unit, NULL)) {
            const char *value = s->value, *first;
            const char *policy = resource(e, "ordinal_policy", "large", "sino");
            const char *reading = resource(e, "native_ordinal_suffix", s->unit, "native");
            if (strcmp(policy, "sino") || (strcmp(reading, "native") && strcmp(reading, "sino")))
                return TTF_ERESOURCE;
            while (*value == '0' && value[1])
                ++value;
            first = ttf_resource_get(e, "ordinal_first", "ko-KR", value, NULL);
            if (first)
                TRY(ttf_buffer_puts(out, first));
            else if (!strcmp(reading, "native"))
                TRY(native_number(e, value, out));
            else
                TRY(cardinal(e, value, out));
            TRY(ttf_buffer_puts(out, " "));
            return ttf_buffer_puts(out, resource(e, "unit", s->unit, s->unit));
        }
        TRY(ttf_buffer_puts(out, resource(e, "number", "ordinal_prefix", "제")));
        TRY(cardinal(e, s->value, out));
        if (!strcmp(s->secondary, "prefix") && s->unit[0]) {
            TRY(ttf_buffer_puts(out, " "));
            TRY(ttf_buffer_puts(out, resource(e, "unit", s->unit, s->unit)));
        }
        return TTF_OK;
    case TTF_SEM_DIGIT_SEQUENCE:
        if (!strcmp(s->unit, "written_fallback"))
            return written_read(e, surface, length, out);
        if (!strcmp(s->unit, "event_date")) {
            /* 6·25 육이오, 6·10 육십: the month as a number, the day digit by
               digit unless it is round or the month has two digits (12·12
               십이십이, 12.16 십이십육); a declared reading wins. */
            const char *reading = ttf_resource_get(e, "event_date", "ko-KR", s->value, NULL);
            const char *dot = strstr(s->value, "\xc2\xb7");
            char part[4];
            size_t k;
            if (reading && strcmp(reading, "1") && strcmp(reading, "0"))
                return ttf_buffer_puts(out, reading);
            if (!dot || dot == s->value || dot - s->value > 2)
                return TTF_EINVAL;
            memcpy(part, s->value, (size_t)(dot - s->value));
            part[dot - s->value] = '\0';
            TRY(cardinal(e, part, out));
            dot += 2;
            if (strlen(dot) < 2 || dot[1] == '0' || dot - 2 - s->value == 2)
                return cardinal(e, dot, out);
            for (k = 0; dot[k]; ++k) {
                part[0] = dot[k];
                part[1] = '\0';
                TRY(cardinal(e, part, out));
            }
            return TTF_OK;
        }
        if (!strcmp(s->unit, "batting_average")) {
            /* 0.212 이할 일푼 이리, 0.091 구푼 일리, 0.300 삼할. */
            static const char *const names[] = {"할", "푼", "리"};
            int any = 0;
            size_t k;
            if (strlen(s->value) != 3 || strspn(s->value, "0123456789") != 3)
                return TTF_EINVAL;
            for (k = 0; k < 3; ++k) {
                char digit[2] = {s->value[k], 0};
                if (s->value[k] == '0')
                    continue;
                if (any)
                    TRY(ttf_buffer_puts(out, " "));
                TRY(cardinal(e, digit, out));
                TRY(ttf_buffer_puts(out, names[k]));
                any = 1;
            }
            if (!any)
                TRY(cardinal(e, "0", out));
            return s->secondary[0] ? ttf_buffer_puts(out, s->secondary) : TTF_OK;
        }
        if (!strcmp(s->unit, "financial_identifier") &&
            (!strcmp(s->rule, "ko:route-label") || !strcmp(s->rule, "ko:short-label") ||
             !strcmp(s->rule, "ko:lot-number"))) {
            /* 38A번 삼십팔에이번, 500-1A번 오백 다시 일에이번; 20F 이십 에프 (one
               change of kind keeps a space), 2v4 이브이사. */
            size_t runs = 0, k;
#define LABEL_DIGIT(c) (((c) >= '0' && (c) <= '9') || (c) == '.')
            for (k = 0; s->value[k]; ++k)
                if (!k || LABEL_DIGIT(s->value[k]) != LABEL_DIGIT(s->value[k - 1]))
                    ++runs;
#undef LABEL_DIGIT
            return render_label(e, s->value, strlen(s->value),
                                !strcmp(s->rule, "ko:short-label") && runs == 2, out);
        }
        if (!strcmp(s->unit, "financial_identifier") && !strcmp(s->rule, "ko:label-range")) {
            /* 0~3M 영에서 삼 엠 */
            const char *tilde = strchr(s->value, '~');
            char left[16];
            size_t ln = tilde ? (size_t)(tilde - s->value) : 0;
            if (!tilde || !ln || ln >= sizeof(left))
                return TTF_EINVAL;
            memcpy(left, s->value, ln);
            left[ln] = '\0';
            TRY(cardinal(e, left, out));
            TRY(ttf_buffer_puts(out, resource(e, "policy", "range_separator", "에서 ")));
            return render_label(e, tilde + 1, strlen(tilde + 1), 1, out);
        }
        if (!strcmp(s->unit, "financial_identifier") &&
            (!strcmp(s->rule, "ko:long-digit-identifier") || !strcmp(s->rule, "ko:alnum-identifier") ||
             (!strcmp(s->rule, "ko:labeled-identifier") && !strchr(s->value, '-') &&
              !(surface && length >= 3 && !memcmp(surface + length - 3, "\xeb\xb2\x88", 3))))) {
            char grouped[TTF_IR_VALUE_SIZE * 2];
            if (group_digit_runs(s->value, grouped, sizeof(grouped)))
                return ttf_ko_render_identifier(e, grouped, out);
            return ttf_ko_render_identifier(e, s->value, out);
        }
        if (strcmp(s->unit, "financial_identifier") == 0) {
            TRY(ttf_ko_render_identifier(e, s->value, out));
            /* A labelled code with its counter (63107508번) keeps the 번. */
            if (!strcmp(s->rule, "ko:labeled-identifier") && length >= 3 &&
                !memcmp(surface + length - 3, "\xeb\xb2\x88", 3))
                return ttf_buffer_puts(out, "\xeb\xb2\x88");
            return TTF_OK;
        }
        if (!strcmp(s->unit, "apparel_size")) {
            /* 3XL 쓰리 엑스라지; XL 엑스라지. */
            if (s->value[0] >= '0' && s->value[0] <= '9') {
                char key[2] = {s->value[0], 0};
                const char *multiplier =
                    ttf_resource_get(e, "apparel_size_multiplier", "ko-KR", key, NULL);
                const char *word = ttf_resource_get(e, "apparel_size", "ko-KR", s->value + 1, NULL);
                if (!multiplier || !word)
                    return TTF_ERESOURCE;
                TRY(ttf_buffer_puts(out, multiplier));
                TRY(ttf_buffer_puts(out, " "));
                return ttf_buffer_puts(out, word);
            }
            return ttf_buffer_puts(out, resource(e, "apparel_size", s->value, s->value));
        }
        if (!strcmp(s->unit, "bra_size")) {
            /* 75AA 칠십오 에이에이: band value, then the cup letters by name. */
            char band[8] = {0};
            size_t n = strspn(s->value, "0123456789");
            if (!n || n >= sizeof(band) || !s->value[n])
                return TTF_EINVAL;
            memcpy(band, s->value, n);
            TRY(cardinal(e, band, out));
            TRY(ttf_buffer_puts(out, " "));
            for (const char *c = s->value + n; *c; ++c) {
                char key[] = "letter.X";
                const char *name;
                key[7] = *c;
                name = ttf_resource_get(e, "phone_letter", "ko-KR", key, NULL);
                if (!name)
                    return TTF_ERESOURCE;
                TRY(ttf_buffer_puts(out, name));
            }
            return TTF_OK;
        }
        if (!strcmp(s->unit, "vehicle_plate")) {
            /* 25후5748: front value, plate syllable, serial digit by digit. */
            char front[4] = {0};
            size_t n = strspn(s->value, "0123456789");
            if (n < 2 || n > 3 || strlen(s->value) != n + 3 + 4)
                return TTF_EINVAL;
            memcpy(front, s->value, n);
            /* 01후: a leading zero keeps the digit reading (공일후). */
            TRY(front[0] == '0' ? ttf_ko_render_identifier(e, front, out) : cardinal(e, front, out));
            TRY(ttf_buffer_append(out, s->value + n, 3));
            TRY(ttf_buffer_puts(out, " "));
            return ttf_ko_render_identifier(e, s->value + n + 3, out);
        }
        if (!strcmp(s->unit, "leading_zero") || !strcmp(s->unit, "@digits")) {
            /* {@digits} reads 0 like a card or phone digit (number digits_zero,
               공 since 2026-10-02; it was 영). */
            const char *p = s->value;
            const char *zero = !strcmp(s->unit, "@digits")
                                   ? resource(e, "number", "digits_zero", "공")
                                   : resource(e, "number", "leading_zero_zero",
                                              resource(e, "digit", "0", "영"));
            int previous = 0;
            for (; *p; ++p) {
                if (*p >= '0' && *p <= '9')
                    TRY(digit_gap(e, &previous, out));
                if (*p == '0')
                    TRY(ttf_buffer_puts(out, zero));
                else if (*p >= '1' && *p <= '9')
                    TRY(digit(e, (unsigned)(*p - '0'), 0, out));
            }
            return TTF_OK;
        }
        return digits(e, s->value, 0, out);
    case TTF_SEM_PHONE:
        return phone(e, s, out);
    case TTF_SEM_COUNTER: {
        const char *month = s->value;
        TRY(ttf_buffer_puts(out, resource(e, "quantity_prefix", s->unit, "")));
        if (*month == '+')
            ++month;
        while (*month == '0' && month[1])
            ++month;
        if (strcmp(s->unit, "월") == 0 && (!strcmp(month, "6") || !strcmp(month, "10"))) {
            TRY(ttf_buffer_puts(out,
                                resource(e, "month", month, !strcmp(month, "6") ? "유" : "시")));
            return ttf_buffer_puts(out, "월");
        }
        /* Baseball counts (counter english): 원 아웃, 투 스트라이크. */
        if (!strcmp(resource(e, "counter", s->unit, "sino"), "english")) {
            const char *word = english_numeral(e, month);
            if (word) {
                TRY(ttf_buffer_puts(out, word));
                TRY(ttf_buffer_puts(out, " "));
                return ttf_buffer_puts(out, resource(e, "unit", s->unit, s->unit));
            }
        }
        {
            int native = !strcmp(s->secondary, "frequency") ||
                         (strcmp(s->secondary, "identifier") &&
                          strcmp(resource(e, "counter", s->unit, "sino"), "native") == 0);
            /* counter_native_max: 2배 두 배 up to 19배 열아홉 배; a larger count or a
               decimal is Sino (20배 이십 배, 1.5배 일 점 오 배; 2026-10-04). */
            const char *max = ttf_resource_get(e, "counter_native_max", "ko-KR", s->unit, NULL);
            if (native && max && (strchr(month, '.') || strlen(month) > 2 || atoi(month) > atoi(max)))
                native = 0;
            return quantity(e, s->value, resource(e, "unit", s->unit, s->unit), native, out);
        }
    }
    case TTF_SEM_CURRENCY:
        return quantity(e, s->value, resource(e, "currency", s->unit, s->unit), 0, out);
    case TTF_SEM_PERCENT:
        return quantity(e, s->value, resource(e, "unit", "%", "퍼센트"), 0, out);
    case TTF_SEM_MEASURE:
        if (!strcmp(s->rule, "ko:dimensions") || !strcmp(s->rule, "ko:fraction-measure")) {
            /* 48X160cm 사십팔 곱하기 백육십 센티미터; 1/8g 팔 분의 일 그램 */
            const char *v = s->value;
            char left[48], right[48];
            size_t k = strcspn(v, "xX/");
            if (!v[k] || k >= sizeof(left) || strlen(v + k + 1) >= sizeof(right))
                return TTF_EINVAL;
            memcpy(left, v, k);
            left[k] = '\0';
            strcpy(right, v + k + 1);
            if (v[k] == '/') {
                TRY(cardinal(e, right, out));
                TRY(ttf_buffer_puts(out, " "));
                TRY(ttf_buffer_puts(out, resource(e, "number", "fraction_of", "분의")));
                TRY(ttf_buffer_puts(out, " "));
                TRY(cardinal(e, left, out));
            } else {
                TRY(cardinal(e, left, out));
                TRY(ttf_buffer_puts(out, " "));
                TRY(ttf_buffer_puts(out, resource(e, "math_operator", "*", "곱하기")));
                TRY(ttf_buffer_puts(out, " "));
                TRY(cardinal(e, right, out));
            }
            TRY(ttf_buffer_puts(out, " "));
            return ttf_buffer_puts(out, resource(e, "unit", s->unit, s->unit));
        }
        TRY(ttf_buffer_puts(out, resource(e, "quantity_prefix", s->unit, "")));
        return quantity(e, s->value, resource(e, "unit", s->unit, s->unit), 0, out);
    case TTF_SEM_RANGE: {
        int contracted = 0;
        /* A score (identifiers.c ko:score): 이십오 대 십팔. */
        if (!strcmp(s->rule, "ko:score") || !strcmp(s->rule, "syntax.score")) {
            TRY(cardinal(e, s->value, out));
            TRY(ttf_buffer_puts(out, " "));
            TRY(ttf_buffer_puts(out, resource(e, "policy", "score_separator", "대")));
            TRY(ttf_buffer_puts(out, " "));
            return cardinal(e, s->secondary, out);
        }
        int quantity_range = strcmp(s->rule, "syntax.range") != 0 || s->unit[0] ||
                             strchr(s->value, '.') || strchr(s->secondary, '.') ||
                             s->value[0] == '-' || s->value[0] == '+' || s->secondary[0] == '-' ||
                             s->secondary[0] == '+';
        int digit_mode = !quantity_range && !strcmp(s->rule, "syntax.range") &&
                         strcmp(resource(e, "policy", "range_mode", "cardinal"), "digits") == 0;
        const char *suffix = resource(e, "policy", "quantity_range_suffix", "none");
        int repeat_unit = strcmp(resource(e, "policy", "quantity_range_repeat_unit", "1"), "0");
        ttf_semantic_t endpoint = *s;
        endpoint.secondary[0] = '\0';
        if (ttf_resource_get(e, "native_ordinal_suffix", "ko-KR", s->unit, NULL))
            endpoint.type = TTF_SEM_ORDINAL;
        else if (!strcmp(s->unit, "time"))
            endpoint.type = TTF_SEM_TIME;
        else if (ttf_resource_get(e, "currency", "ko-KR", s->unit, NULL))
            endpoint.type = TTF_SEM_CURRENCY;
        else if (s->unit[0])
            endpoint.type = TTF_SEM_COUNTER;
        else
            endpoint.type = digit_mode ? TTF_SEM_DIGIT_SEQUENCE : TTF_SEM_CARDINAL;
        if (!strcmp(s->rule, "ko:frequency-range"))
            snprintf(endpoint.secondary, sizeof(endpoint.secondary), "frequency");
        else if (!strcmp(s->rule, "ko:identifier-range"))
            snprintf(endpoint.secondary, sizeof(endpoint.secondary), "identifier");
        /* Per unit: 0 never repeats, 1 always repeats, wide repeats only a range
           that does not contract (2~3시 두세 시, 2~4시 두 시에서 네 시). */
        const char *unit_repeat =
            s->unit[0] ? ttf_resource_get(e, "range_repeat_unit", "ko-KR", s->unit, NULL) : NULL;
        if (unit_repeat && !strcmp(unit_repeat, "0"))
            repeat_unit = 0;
        else if (unit_repeat && !strcmp(unit_repeat, "1"))
            repeat_unit = 1;
        else if (unit_repeat && !strcmp(unit_repeat, "wide"))
            repeat_unit = 0;
        /* 3개~4개 keeps both written units (세 개에서 네 개); only a unit written
           once, after the second number, is shared by the range. */
        /* A prefixed unit (시속 삼 마일) keeps its reading at both ends. */
        if (!range_unit_shared(surface, length) || endpoint.type == TTF_SEM_ORDINAL ||
            (s->unit[0] && *resource(e, "quantity_prefix", s->unit, "")))
            repeat_unit = 1;
        if (quantity_range && !repeat_unit)
            TRY(range_contraction(e, s, &endpoint, surface, length, out, &contracted));
        if (contracted)
            return TTF_OK;
        if (unit_repeat && !strcmp(unit_repeat, "wide"))
            repeat_unit = 1;
        if (repeat_unit || !s->unit[0] || !strcmp(s->unit, "time"))
            TRY(verbalize(e, &endpoint, surface, length, out));
        else
            TRY(range_first_bare(e, s, &endpoint, surface, length, out));
        TRY(ttf_buffer_puts(
            out,
            resource(e, "policy", quantity_range ? "quantity_range_separator" : "range_separator",
                     "에서 ")));
        memcpy(endpoint.value, s->secondary, sizeof(endpoint.value));
        TRY(verbalize(e, &endpoint, surface, length, out));
        if (quantity_range && strcmp(suffix, "none"))
            TRY(ttf_buffer_puts(out, suffix));
        return TTF_OK;
    }
    case TTF_SEM_FRACTION: {
        const char *numerator = s->value;
        if (*numerator == '-') {
            TRY(ttf_buffer_puts(out, resource(e, "number", "negative", "마이너스 ")));
            ++numerator;
        }
        TRY(cardinal(e, s->secondary, out));
        TRY(ttf_buffer_puts(out, resource(e, "number", "fraction_separator", " 분의 ")));
        return cardinal(e, numerator, out);
    }
    case TTF_SEM_DATE: {
        unsigned year = 0, month = 0, day = 0;
        int consumed = 0, has_year = 1, has_month = 1, has_day = 1;
        char value[16];
        if (!strcmp(s->unit, "weekday")) {
            const char *label;
            if (s->value[0] < '1' || s->value[0] > '7' || s->value[1])
                return TTF_EINVAL;
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, length);
            if (s->secondary[0])
                TRY(ttf_trace_resource(e, "weekday_label", s->source_lang, s->secondary,
                                       NULL, "date-attached weekday alias", (size_t)-1));
            TRY(ttf_trace_resource(e, "weekday", "ko-KR", s->value, NULL,
                                   "calendar weekday reading", (size_t)-1));
            label = ttf_resource_get(e, "weekday", "ko-KR", s->value, NULL);
            return label ? ttf_buffer_puts(out, label) : TTF_ERESOURCE;
        }
        if (!strcmp(s->unit, "year")) {
            has_month = has_day = 0;
            if (sscanf(s->value, "%u%n", &year, &consumed) != 1)
                return TTF_EINVAL;
        } else if (!strcmp(s->unit, "year-month")) {
            has_day = 0;
            if (sscanf(s->value, "%u-%u%n", &year, &month, &consumed) != 2)
                return TTF_EINVAL;
        } else if (!strcmp(s->unit, "month-day")) {
            has_year = 0;
            if (sscanf(s->value, "--%u-%u%n", &month, &day, &consumed) != 2)
                return TTF_EINVAL;
        } else if (!s->unit[0]) {
            if (sscanf(s->value, "%u-%u-%u%n", &year, &month, &day, &consumed) != 3)
                return TTF_EINVAL;
        } else
            return TTF_EINVAL;
        if (s->value[consumed] || (has_year && (!year || year > 9999)) ||
            (has_month && (!month || month > 12)) || (has_day && (!day || day > 31)))
            return TTF_EINVAL;
        if (has_year) {
            (void)snprintf(value, sizeof(value), "%u", year);
            TRY(quantity(e, value, resource(e, "unit", "year", "년"), 0, out));
            if (!has_month)
                return TTF_OK;
            TRY(ttf_buffer_puts(out, " "));
        }
        if (month == 6 || month == 10) {
            TRY(ttf_buffer_puts(
                out, resource(e, "month", month == 6 ? "6" : "10", month == 6 ? "유" : "시")));
            TRY(ttf_buffer_puts(out, resource(e, "unit", "month", "월")));
        } else {
            (void)snprintf(value, sizeof(value), "%u", month);
            TRY(quantity(e, value, resource(e, "unit", "month", "월"), 0, out));
        }
        if (!has_day)
            return TTF_OK;
        TRY(ttf_buffer_puts(out, " "));
        (void)snprintf(value, sizeof(value), "%u", day);
        return quantity(e, value, resource(e, "unit", "day", "일"), 0, out);
    }
    case TTF_SEM_TIME: {
        unsigned hour, minute, second;
        int fields;
        char value[16];
        fields = sscanf(s->value, "%u:%u:%u", &hour, &minute, &second);
        if (fields < 2)
            return TTF_EINVAL;
        if (s->secondary[0]) {
            TRY(ttf_buffer_puts(out, resource(e, "meridiem", s->secondary, s->secondary)));
            TRY(ttf_buffer_puts(out, " "));
        }
        (void)snprintf(value, sizeof(value), "%u", hour);
        TRY(quantity(e, value, resource(e, "unit", "hour", "시"), 1, out));
        /* An exact hour is spoken without its zero minutes unless a resource
           asks for them; seconds still need the minute they belong to. */
        if (minute || (fields == 3 && second) ||
            strcmp(resource(e, "clock", "zero_minutes", "omit"), "omit")) {
            TRY(ttf_buffer_puts(out, " "));
            (void)snprintf(value, sizeof(value), "%u", minute);
            TRY(quantity(e, value, resource(e, "unit", "minute", "분"), 0, out));
        }
        if (fields == 3 && second) {
            TRY(ttf_buffer_puts(out, " "));
            (void)snprintf(value, sizeof(value), "%u", second);
            TRY(quantity(e, value, resource(e, "unit", "second", "초"), 0, out));
        }
        return TTF_OK;
    }
    case TTF_SEM_SYMBOL: {
        char key[16];
        if (!strcmp(s->unit, "@number-sign")) {
            const char *template = ttf_resource_get(e, "number_sign", "ko-KR", "#", NULL), *p;
            ttf_status_t status;
            if (!template)
                return TTF_ERESOURCE;
            status = ttf_trace_resource(e, "number_sign", "ko-KR", "#", "#", "number-sign-reading",
                                        SIZE_MAX);
            if (status != TTF_OK)
                return status;
            for (p = template; *p;) {
                if (!strncmp(p, "{N}", 3)) {
                    status = cardinal(e, s->value, out);
                    p += 3;
                } else {
                    status = ttf_buffer_append(out, p, 1);
                    ++p;
                }
                if (status != TTF_OK)
                    return status;
            }
            return TTF_OK;
        }
        if (!strcmp(s->unit, "@expression"))
            return expression(e, s->value, out);
        if (!strcmp(s->unit, "@math")) {
            const char *label = ttf_resource_get(e, "math_operator", "ko-KR", s->value, NULL);
            if (!label)
                return TTF_ERESOURCE;
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, strlen(s->value));
            TRY(ttf_trace_resource(e, "math_operator", "ko-KR", s->value, s->value,
                                   "binary-operator-reading", SIZE_MAX));
            return ttf_buffer_puts(out, label);
        }
        if (length < sizeof(key)) {
            memcpy(key, surface, length);
            key[length] = '\0';
            return ttf_buffer_puts(out, resource(e, "symbol", key, key));
        }
        return ttf_buffer_append(out, surface, length);
    }
    default:
        return ttf_buffer_append(out, surface, length);
    }
}

static size_t skip_spaces(const char *text, size_t n, size_t pos) {
    while (pos < n && (text[pos] == ' ' || text[pos] == '\t'))
        ++pos;
    return pos;
}

static int horizontal_space(const char *text, size_t begin, size_t end) {
    return skip_spaces(text, end, begin) == end;
}

static size_t unsigned_value(const char *text, size_t n, size_t pos, unsigned *value) {
    size_t start = pos;
    *value = 0;
    while (pos < n && text[pos] >= '0' && text[pos] <= '9') {
        if (pos - start == 4)
            return start;
        *value = *value * 10 + (unsigned)(text[pos++] - '0');
    }
    return pos;
}

static int literal_at(const char *text, size_t n, size_t pos, const char *literal) {
    size_t width = strlen(literal);
    return pos <= n && width <= n - pos && !memcmp(text + pos, literal, width);
}

static size_t classified(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t end,
                         ttf_semantic_type_t type, const char *rule) {
    size_t consumed = 0;
    while (in->index + consumed < in->token_count && in->tokens[in->index + consumed].begin < end)
        ++consumed;
    s->type = type;
    s->input_end = end;
    (void)snprintf(s->rule, sizeof(s->rule), "%s", rule);
    return consumed;
}

/* Decimal magnitude arithmetic deliberately avoids binary floating point and
 * integer overflow. Digits are little endian, with an explicit decimal scale. */
typedef struct {
    unsigned char digits[TTF_IR_VALUE_SIZE];
    size_t count;
    size_t fraction;
} ko_decimal_t;

static void decimal_trim(ko_decimal_t *value) {
    while (value->count > 1 && !value->digits[value->count - 1])
        --value->count;
    while (value->fraction && value->count > 1 && !value->digits[0]) {
        memmove(value->digits, value->digits + 1, --value->count);
        --value->fraction;
    }
    if (value->count == 1 && !value->digits[0])
        value->fraction = 0;
}

static void decimal_zero(ko_decimal_t *value) {
    memset(value, 0, sizeof(*value));
    value->count = 1;
}

static int decimal_shift(ko_decimal_t *value, size_t power) {
    size_t append;
    if (value->fraction >= power) {
        value->fraction -= power;
        return 1;
    }
    append = power - value->fraction;
    value->fraction = 0;
    if (value->count == 1 && !value->digits[0])
        return 1;
    if (append > sizeof(value->digits) - value->count)
        return 0;
    memmove(value->digits + append, value->digits, value->count);
    memset(value->digits, 0, append);
    value->count += append;
    return 1;
}

static int decimal_add(ko_decimal_t *left, const ko_decimal_t *right) {
    ko_decimal_t sum;
    size_t scale = left->fraction > right->fraction ? left->fraction : right->fraction;
    size_t left_offset = scale - left->fraction, right_offset = scale - right->fraction;
    size_t left_count = left->count + left_offset, right_count = right->count + right_offset;
    size_t count = left_count > right_count ? left_count : right_count, i;
    unsigned carry = 0;
    if (count > sizeof(sum.digits))
        return 0;
    decimal_zero(&sum);
    sum.fraction = scale;
    for (i = 0; i < count; ++i) {
        unsigned digit = carry;
        if (i >= left_offset && i - left_offset < left->count)
            digit += left->digits[i - left_offset];
        if (i >= right_offset && i - right_offset < right->count)
            digit += right->digits[i - right_offset];
        sum.digits[i] = (unsigned char)(digit % 10);
        carry = digit / 10;
    }
    if (carry) {
        if (count == sizeof(sum.digits))
            return 0;
        sum.digits[count++] = (unsigned char)carry;
    }
    sum.count = count;
    decimal_trim(&sum);
    *left = sum;
    return 1;
}

/* Strictly below 10^power, used to reject overlapping lower components. */
static int decimal_below_power(const ko_decimal_t *value, size_t power) {
    return (value->count == 1 && !value->digits[0]) || value->count <= value->fraction + power;
}

static int decimal_string(const ko_decimal_t *value, int negative, char *out, size_t capacity) {
    size_t pos = 0, i, width = value->count > value->fraction ? value->count : value->fraction + 1;
    size_t needed = width + (value->fraction ? 1 : 0) + (negative ? 1 : 0);
    if (needed >= capacity)
        return 0;
    if (negative)
        out[pos++] = '-';
    for (i = width; i > 0; --i) {
        if (i == value->fraction)
            out[pos++] = '.';
        out[pos++] = i <= value->count ? (char)('0' + value->digits[i - 1]) : '0';
    }
    out[pos] = '\0';
    return 1;
}

/* Returns zero for a malformed coefficient while still consuming its numeric
 * spelling. That lets the caller protect the entire invalid compound surface. */
static int decimal_parse(const char *text, size_t n, size_t *position, ko_decimal_t *value) {
    unsigned char forward[TTF_IR_VALUE_SIZE];
    size_t p = *position, count = 0, group = 0, fraction = 0, i;
    int valid = 1, grouped = 0, dot = 0, have_digit = 0;
    decimal_zero(value);
    if (p < n && (text[p] == '+' || text[p] == '-')) {
        valid = 0; /* A sign belongs only to the whole compound. */
        ++p;
    }
    while (p < n) {
        if (text[p] >= '0' && text[p] <= '9') {
            have_digit = 1;
            if (count < sizeof(forward))
                forward[count] = (unsigned char)(text[p] - '0');
            else
                valid = 0;
            ++count;
            if (dot)
                ++fraction;
            else
                ++group;
            ++p;
        } else if (text[p] == ',') {
            if (dot || (!grouped && (!group || group > 3)) || (grouped && group != 3))
                valid = 0;
            grouped = 1;
            group = 0;
            ++p;
        } else if (text[p] == '.' && p + 1 < n &&
                   ((text[p + 1] >= '0' && text[p + 1] <= '9') || text[p + 1] == '.')) {
            if (dot || (grouped && group != 3))
                valid = 0;
            dot = 1;
            ++p;
        } else
            break;
    }
    if (!have_digit || (grouped && group != 3) || (dot && !fraction) || count > sizeof(forward))
        valid = 0;
    *position = p;
    if (!valid)
        return 0;
    value->count = count;
    value->fraction = fraction;
    for (i = 0; i < count; ++i)
        value->digits[i] = forward[count - 1 - i];
    decimal_trim(value);
    return 1;
}

/* Scale records are atomic exponents (십=1, 백=2, 천=3, 만=4, ...).
 * A sequence such as 천만 is parsed as a minor group followed by a major scale. */
static size_t scale_at(const ttf_pack_input_t *in, size_t position, unsigned *power) {
    char key[32];
    size_t width, available = in->length - position;
    for (width = 1; width < sizeof(key) && width <= available; ++width) {
        const char *label;
        char *end;
        long parsed;
        memcpy(key, in->text + position, width);
        key[width] = '\0';
        label = ttf_resource_get(in->engine, "scale", "ko-KR", key, NULL);
        if (!label)
            continue;
        parsed = strtol(label, &end, 10);
        if (!*end && parsed > 0 && parsed <= 40 && (parsed < 4 || parsed % 4 == 0)) {
            *power = (unsigned)parsed;
            return width;
        }
    }
    return 0;
}

static const char *quantity_resource_at(const ttf_pack_input_t *in, const char *kind,
                                        size_t position, size_t *width) {
    char key[64];
    size_t available = in->length - position;
    for (*width = available < sizeof(key) - 1 ? available : sizeof(key) - 1; *width > 0; --*width) {
        const char *label;
        memcpy(key, in->text + position, *width);
        key[*width] = '\0';
        label = ttf_resource_get(in->engine, kind, "ko-KR", key, NULL);
        if (label)
            return label;
    }
    return NULL;
}

/* Numeric morphology may end before an attached particle. Ordinary Hangul
 * words beginning with a scale (천국, 만들기) do not become numeric multipliers. */
static int quantity_boundary(const ttf_pack_input_t *in, size_t position) {
    size_t i, width;
    char key[64];
    for (i = in->index; i < in->token_count; ++i) {
        const ttf_token_t *token = &in->tokens[i];
        if (token->begin <= position && position < token->end) {
            if (token->script != TTF_SCRIPT_HANGUL)
                return token->begin == position && (token->script == TTF_SCRIPT_WHITESPACE ||
                                                    token->script == TTF_SCRIPT_PUNCTUATION ||
                                                    token->script == TTF_SCRIPT_SYMBOL);
            width = token->end - position;
            if (width >= sizeof(key))
                return 0;
            memcpy(key, in->text + position, width);
            key[width] = '\0';
            if (ttf_resource_get(in->engine, "particle", "ko-KR", key, NULL) ||
                ttf_resource_get(in->engine, "quantity_tail", "ko-KR", key, NULL))
                return 1;
            /* Productive tails, e.g. price-band 대 + 면/는/입니다, are data.
               Both parts must be declared; arbitrary word prefixes never qualify. */
            for (size_t split = 1; split < width; ++split) {
                char head[64];
                memcpy(head, key, split);
                head[split] = 0;
                if (ttf_resource_get(in->engine, "quantity_tail_head", "ko-KR", head, NULL) &&
                    (ttf_resource_get(in->engine, "particle", "ko-KR", key + split, NULL) ||
                     ttf_resource_get(in->engine, "quantity_tail", "ko-KR", key + split, NULL)))
                    return 1;
            }
            return 0;
        }
        if (token->begin >= position)
            break;
    }
    return 1;
}

static size_t compound_amount(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *text = in->text, *suffix = NULL, *prefix = NULL;
    size_t begin = in->tokens[in->index].begin, n = in->length, p = begin, q, width, end;
    ko_decimal_t total, group, pending, term;
    unsigned power, last_minor = 4, last_major = 41;
    size_t scales = 0, coefficients = 1;
    int negative = 0, valid = 1, have_pending = 1, have_group = 0;
    int sign_before_prefix = 0;
    ttf_semantic_type_t type = TTF_SEM_CARDINAL;
    decimal_zero(&total);
    decimal_zero(&group);
    /* An adjacent sign may precede a resource-defined currency prefix.
       Keep it attached to the complete amount, including Korean scales. */
    if (p + 1 < n && (text[p] == '-' || text[p] == '+')) {
        prefix = quantity_resource_at(in, "currency_prefix", p + 1, &width);
        if (prefix) {
            sign_before_prefix = 1;
            negative = text[p++] == '-';
        }
    }
    if (!prefix)
        prefix = quantity_resource_at(in, "currency_prefix", p, &width);
    if (prefix)
        p = skip_spaces(text, n, p + width);
    if (p < n && (text[p] == '-' || text[p] == '+')) {
        if (sign_before_prefix)
            valid = 0;
        else
            negative = text[p] == '-';
        ++p;
        /* Consume an invalid sign run so it cannot expose a valid-looking
           unsigned tail to another classifier. */
        while (prefix && p < n && (text[p] == '-' || text[p] == '+')) {
            valid = 0;
            ++p;
        }
    }
    if (p >= n || text[p] < '0' || text[p] > '9')
        return 0;
    /* 1백만, 1천만: a written 1 before 십, 백 or 천 and then directly 만, 억 or 조
       reads 일 (일백만), which the shared number grammar keeps; the compound
       total would drop it. 1천5백만 and 1천571 stay 천오백만, 천오백칠십일. */
    if (text[p] == '1' && (p + 1 >= n || text[p + 1] < '0' || text[p + 1] > '9') &&
        in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
        unsigned minor, major;
        size_t w = scale_at(in, p + 1, &minor);
        if (w && minor < 4 && scale_at(in, p + 1 + w, &major) && major >= 4)
            return 0;
    }
    if (!decimal_parse(text, n, &p, &pending))
        valid = 0;
    end = p;
    for (;;) {
        q = skip_spaces(text, n, p);
        width = scale_at(in, q, &power);
        /* 920만 조회: 조 that begins the word 조회 after a space is not a scale. */
        if (width && q > p && !quantity_boundary(in, q + width) &&
            !quantity_resource_at(in, "numeric_suffix", q + width, &end) &&
            !(q + width < n && text[q + width] >= '0' && text[q + width] <= '9')) {
            unsigned next_power;
            if (!scale_at(in, q + width, &next_power))
                width = 0;
        }
        if (width) {
            ++scales;
            if (power < 4) {
                if (!have_pending || power >= last_minor)
                    valid = 0;
                if (valid) {
                    term = pending;
                    if (!decimal_shift(&term, power) ||
                        (have_group && !decimal_below_power(&term, last_minor)) ||
                        !decimal_add(&group, &term))
                        valid = 0;
                }
                have_pending = 0;
                have_group = 1;
                last_minor = power;
            } else {
                if ((!have_pending && !have_group) || power >= last_major)
                    valid = 0;
                if (valid && have_pending) {
                    if ((have_group && !decimal_below_power(&pending, last_minor)) ||
                        !decimal_add(&group, &pending))
                        valid = 0;
                }
                if (valid && (!decimal_shift(&group, power) ||
                              (last_major != 41 && !decimal_below_power(&group, last_major)) ||
                              !decimal_add(&total, &group)))
                    valid = 0;
                decimal_zero(&group);
                have_pending = 0;
                have_group = 0;
                last_minor = 4;
                last_major = power;
            }
            p = q + width;
            end = p;
            continue;
        }
        if (q < n && ((text[q] >= '0' && text[q] <= '9') || text[q] == '+' || text[q] == '-' ||
                      (text[q] == '.' && q + 1 < n && text[q + 1] >= '0' && text[q + 1] <= '9'))) {
            size_t number_end = q;
            ko_decimal_t next;
            int number_valid = decimal_parse(text, n, &number_end, &next);
            /* A separate number without any preceding scale is not compound. */
            if (!scales)
                return 0;
            if (number_end == q)
                break;
            if (have_pending || !number_valid || text[q] == '.')
                valid = 0;
            pending = next;
            have_pending = 1;
            ++coefficients;
            p = number_end;
            end = p;
            continue;
        }
        suffix = quantity_resource_at(in, "numeric_suffix", q, &width);
        if (suffix && (!strncmp(suffix, "CURRENCY:", 9) || !strncmp(suffix, "COUNTER:", 8) ||
                       !strncmp(suffix, "MEASURE:", 8))) {
            end = q + width;
            if (prefix && (strncmp(suffix, "CURRENCY:", 9) || strcmp(prefix, suffix + 9)))
                valid = 0;
            /* Speech: 4만 2000km인, 1만 2,547km였다 keep the quantity; the attached
               ending stays text (as for an unknown suffix below). */
            /* 5억 1,317만 원어치의, 3백2십만 원하는: currency and counters too. */
            if (!quantity_boundary(in, end) &&
                !(valid && coefficients > 1 && strncmp(suffix, "MEASURE:", 8) == 0 &&
                  in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH &&
                  ((unsigned char)in->text[end] & 0xe0) == 0xe0) &&
                !(valid && coefficients > 1 &&
                  (!strncmp(suffix, "CURRENCY:", 9) || !strncmp(suffix, "COUNTER:", 8)) &&
                  in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH &&
                  ((unsigned char)in->text[end] & 0xe0) == 0xe0)) {
                if (valid && coefficients == 1)
                    return 0;
                valid = 0;
            }
        } else
            suffix = NULL;
        break;
    }
    if (!scales)
        return 0;
    if (!suffix && !quantity_boundary(in, end)) {
        if (valid && coefficients == 1)
            return 0;
        /* Speech: a valid compound before an unknown attached suffix (1만
           5,000본, 1만 5,000여) is still that number; the suffix stays text.
           Text output keeps the rejected object literally. */
        if (!(valid && in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH))
            valid = 0;
    }
    if (valid && have_pending) {
        if ((have_group && !decimal_below_power(&pending, last_minor)) ||
            !decimal_add(&group, &pending))
            valid = 0;
    }
    if (valid && ((last_major != 41 && !decimal_below_power(&group, last_major)) ||
                  !decimal_add(&total, &group) ||
                  !decimal_string(&total, negative, s->value, sizeof(s->value))))
        valid = 0;
    if (!valid) {
        /* Extend through an attached nonnumeric Hangul remainder as well; do
         * not leave an invalid compound open to a second partial conversion. */
        size_t i;
        for (i = in->index; i < in->token_count; ++i)
            if (in->tokens[i].begin < end && end < in->tokens[i].end) {
                end = in->tokens[i].end;
                break;
            }
        s->value[0] = '\0';
        return classified(in, s, end, TTF_SEM_TEXT, "ko:invalid-compound-number");
    }
    if (suffix) {
        if (!strncmp(suffix, "CURRENCY:", 9)) {
            type = TTF_SEM_CURRENCY;
            (void)snprintf(s->unit, sizeof(s->unit), "%s", suffix + 9);
        } else {
            type = !strncmp(suffix, "COUNTER:", 8) ? TTF_SEM_COUNTER : TTF_SEM_MEASURE;
            (void)snprintf(s->unit, sizeof(s->unit), "%s", suffix + 8);
        }
    } else if (prefix) {
        type = TTF_SEM_CURRENCY;
        (void)snprintf(s->unit, sizeof(s->unit), "%s", prefix);
    }
    return classified(in, s, end, type,
                      type == TTF_SEM_CURRENCY ? "ko:compound-currency" : "ko:compound-number");
}

/* Scaled Korean currency endpoints reuse the exact compound arithmetic above.
 * A bounded first-endpoint view stops the scale grammar at a resource delimiter;
 * the resulting RANGE remains language-neutral and can use any pack renderer. */
/* Unsigned integer strings: left < right. A descending pair such as
   1억-2천만 is not an interval and must stay verbatim. */
static int ascending_integers(const char *left, const char *right) {
    size_t a = strlen(left), b = strlen(right);
    for (const char *p = left; *p; ++p)
        if (*p < '0' || *p > '9')
            return 0;
    for (const char *p = right; *p; ++p)
        if (*p < '0' || *p > '9')
            return 0;
    return a < b || (a == b && strcmp(left, right) < 0);
}

/* A scalar probed in a shortened endpoint view can look like a bare identifier.
   Inside a confirmed quantity grammar, its original digits are the coefficient. */
static void quantity_endpoint(ttf_semantic_t *s) {
    if (s->type == TTF_SEM_DIGIT_SEQUENCE && !strcmp(s->unit, "leading_zero")) {
        s->type = TTF_SEM_CARDINAL;
        s->unit[0] = '\0';
    }
}

static size_t compound_currency_range(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    size_t begin = in->tokens[in->index].begin, i;
    if (in->tokens[in->index].script != TTF_SCRIPT_DIGIT &&
        in->tokens[in->index].script != TTF_SCRIPT_SYMBOL && in->text[begin] != '-' &&
        in->text[begin] != '+')
        return 0;
    if (in->tokens[in->index].end < in->length &&
        in->text[in->tokens[in->index].end] == '-') {
        ttf_semantic_t telephone = {0};
        /* A complete phone is not a range coefficient, even before a particle
           or name that starts with a scale character. Use the shared scanner. */
        if (ttf_classify_phone(in, &telephone) && telephone.type == TTF_SEM_PHONE)
            return 0;
    }
    for (i = in->index + 1; i < in->token_count; ++i) {
        const ttf_token_t *token = &in->tokens[i];
        char key[8];
        size_t width = token->end - token->begin, next, first_count, second_count;
        int first_scaled = 0, second_scaled = 0, shared_scale = 0;
        const char *enabled;
        ttf_resource_layer_t delimiter_layer;
        ttf_pack_input_t left = *in, right = *in;
        ttf_semantic_t first = {0}, second = {0};
        /* Two 96-byte coefficients plus written scale/unit syntax are bounded.
           Larger constructs continue through ordinary scalar classification. */
        if (token->begin - begin > 512)
            break;
        if (token->script == TTF_SCRIPT_WHITESPACE &&
            !horizontal_space(in->text, token->begin, token->end))
            break;
        if (width >= sizeof(key))
            continue;
        memcpy(key, in->text + token->begin, width);
        key[width] = '\0';
        enabled =
            ttf_resource_get(in->engine, "quantity_range_separator", "*", key, &delimiter_layer);
        if (!enabled || strcmp(enabled, "1"))
            continue;
        left.length = token->begin;
        left.token_count = i;
        first_count = compound_amount(&left, &first);
        first_scaled = first_count != 0;
        if (!first_count)
            first_count = ttf_classify_universal(&left, &first);
        quantity_endpoint(&first);
        if (!first_count ||
            (first.type != TTF_SEM_CARDINAL && first.type != TTF_SEM_NUMBER &&
             first.type != TTF_SEM_CURRENCY && first.type != TTF_SEM_COUNTER &&
             first.type != TTF_SEM_MEASURE && first.type != TTF_SEM_PERCENT) ||
            skip_spaces(in->text, in->length, first.input_end) != token->begin)
            continue;
        next = i + 1;
        while (next < in->token_count && in->tokens[next].script == TTF_SCRIPT_WHITESPACE) {
            if (!horizontal_space(in->text, in->tokens[next].begin, in->tokens[next].end))
                return 0;
            ++next;
        }
        if (next == in->token_count)
            return 0;
        right.index = next;
        second_count = compound_amount(&right, &second);
        second_scaled = second_count != 0;
        if (!second_count)
            second_count = ttf_classify_universal(&right, &second);
        quantity_endpoint(&second);
        if (!second_scaled && second_count) {
            unsigned ignored_power;
            size_t scale_start = skip_spaces(in->text, in->length, second.input_end), suffix_width;
            size_t scale_width = scale_at(in, scale_start, &ignored_power);
            /* 33-53-666846-99만: after a code of three or more hyphen groups or a
               group of five or more digits, 만 is the particle (only). */
            if (scale_width) {
                size_t back = scale_start, hyphens = 0, run = 0, longest = 0;
                while (back > in->tokens[in->index].begin &&
                       ((in->text[back - 1] >= '0' && in->text[back - 1] <= '9') || in->text[back - 1] == '-')) {
                    --back;
                    if (in->text[back] == '-') {
                        ++hyphens;
                        run = 0;
                    } else if (++run > longest)
                        longest = run;
                }
                if (hyphens >= 2 || longest >= 5)
                    scale_width = 0;
            }
            /* 1시간 만에, 6년 만에: after an endpoint with its own unit and a space,
               만 is the bound noun (2026-10-02). */
            if (scale_width &&
                (second.type == TTF_SEM_COUNTER || second.type == TTF_SEM_MEASURE ||
                 second.type == TTF_SEM_CURRENCY || second.type == TTF_SEM_TIME ||
                 second.type == TTF_SEM_PERCENT))
                scale_width = 0;
            /* 조회, 억지: a word that only begins with a scale is not one. */
            if (scale_width && !quantity_boundary(in, scale_start + scale_width) &&
                !quantity_resource_at(in, "numeric_suffix", scale_start + scale_width,
                                      &suffix_width) &&
                !scale_at(in, scale_start + scale_width, &ignored_power))
                scale_width = 0;
            if (scale_width) {
                size_t guard_end = scale_start, guard_index = next;
                const char *suffix;
                while (guard_index < in->token_count &&
                       in->tokens[guard_index].begin <= scale_start) {
                    guard_end = in->tokens[guard_index].end;
                    ++guard_index;
                }
                suffix = quantity_resource_at(in, "numeric_suffix",
                                              skip_spaces(in->text, in->length, guard_end),
                                              &suffix_width);
                if (suffix)
                    guard_end = skip_spaces(in->text, in->length, guard_end) + suffix_width;
                out->value[0] = out->secondary[0] = out->unit[0] = '\0';
                return classified(in, out, guard_end, TTF_SEM_TEXT,
                                  "ko:unsupported-shared-scale-range");
            }
        }
        if ((!first_scaled && !second_scaled) || !second_count)
            continue;
        /* An omitted first unit inherits one unambiguous trailing scale:
           1~2만 원 = 1만 원~2만 원. Mixed additive tails (2억3천만)
           cannot supply a single shared multiplier and stay verbatim. */
        if (!first_scaled && second_scaled &&
            (first.type == TTF_SEM_CARDINAL || first.type == TTF_SEM_NUMBER)) {
            size_t p = in->tokens[next].begin, scale_width, parsed = 0;
            /* 254412-774521만 있으면: a coefficient of five or more digits does not
               share a scale; 만 there is the particle (only). */
            {
                size_t a = 0, z = p;
                for (const char *c = first.value; *c; ++c)
                    a += *c >= '0' && *c <= '9';
                while (z < in->length && in->text[z] >= '0' && in->text[z] <= '9')
                    ++z;
                if (a > 4 || z - p > 4)
                    return 0;
            }
            unsigned exponent = 0, power;
            int negative = first.value[0] == '-';
            ko_decimal_t coefficient, ignored;
            const char *first_number =
                first.value + (first.value[0] == '-' || first.value[0] == '+');
            if (in->text[p] == '-' || in->text[p] == '+')
                ++p;
            if (!decimal_parse(in->text, in->length, &p, &ignored))
                continue;
            for (;;) {
                p = skip_spaces(in->text, in->length, p);
                scale_width = scale_at(in, p, &power);
                if (!scale_width)
                    break;
                exponent += power;
                p += scale_width;
            }
            if (!exponent || (p < in->length && in->text[p] >= '0' && in->text[p] <= '9') ||
                !decimal_parse(first_number, strlen(first_number), &parsed, &coefficient) ||
                !decimal_shift(&coefficient, exponent) ||
                !decimal_string(&coefficient, negative, first.value, sizeof(first.value)) ||
                (second.type != TTF_SEM_CURRENCY && second.type != TTF_SEM_COUNTER &&
                 second.type != TTF_SEM_MEASURE && second.type != TTF_SEM_CARDINAL)) {
                out->value[0] = out->secondary[0] = out->unit[0] = '\0';
                return classified(in, out, second.input_end, TTF_SEM_TEXT,
                                  "ko:ambiguous-shared-scale-range");
            }
            first.type = second.type;
            memcpy(first.unit, second.unit, sizeof(first.unit));
            shared_scale = 1;
        } else if (first_scaled && second_scaled && ascending_integers(first.value, second.value) &&
                   (first.type == TTF_SEM_CARDINAL || first.type == TTF_SEM_NUMBER) &&
                   (second.type == TTF_SEM_CURRENCY || second.type == TTF_SEM_COUNTER ||
                    second.type == TTF_SEM_MEASURE)) {
            /* 1억~2억 원: both endpoints carry a scale and only the last carries
               the unit, which then applies to the whole interval. */
            first.type = second.type;
            memcpy(first.unit, second.unit, sizeof(first.unit));
            shared_scale = 1;
        }
        /* Retain a chained expression intact; two endpoints are the supported
           interval contract, even when every written currency matches. */
        {
            size_t chain_index = next, chain_end = second.input_end;
            int chained = 0;
            while (chain_index < in->token_count) {
                const char *chain_enabled;
                size_t chain_width;
                ttf_semantic_t tail = {0};
                while (chain_index < in->token_count &&
                       (in->tokens[chain_index].begin < chain_end ||
                        in->tokens[chain_index].script == TTF_SCRIPT_WHITESPACE))
                    ++chain_index;
                if (chain_index == in->token_count)
                    break;
                chain_width = in->tokens[chain_index].end - in->tokens[chain_index].begin;
                if (chain_width >= sizeof(key))
                    break;
                memcpy(key, in->text + in->tokens[chain_index].begin, chain_width);
                key[chain_width] = '\0';
                chain_enabled =
                    ttf_resource_get(in->engine, "quantity_range_separator", "*", key, NULL);
                if (!chain_enabled || strcmp(chain_enabled, "1"))
                    break;
                chained = 1;
                chain_end = in->tokens[chain_index].end;
                ++chain_index;
                while (chain_index < in->token_count &&
                       in->tokens[chain_index].script == TTF_SCRIPT_WHITESPACE) {
                    if (!horizontal_space(in->text, in->tokens[chain_index].begin,
                                          in->tokens[chain_index].end))
                        break;
                    ++chain_index;
                }
                if (chain_index == in->token_count)
                    break;
                right.index = chain_index;
                if (!compound_amount(&right, &tail) && !ttf_classify_universal(&right, &tail))
                    break;
                if (tail.input_end <= chain_end)
                    break;
                chain_end = tail.input_end;
            }
            if (chained) {
                out->value[0] = out->secondary[0] = out->unit[0] = '\0';
                return classified(in, out, chain_end, TTF_SEM_TEXT, "ko:invalid-compound-range");
            }
        }
        if (first.type != second.type || strcmp(first.unit, second.unit) ||
            (token->end == token->begin + 1 && in->text[token->begin] == '-' &&
             in->tokens[next].begin == token->end &&
             (in->text[token->end] == '-' || in->text[token->end] == '+'))) {
            out->value[0] = out->secondary[0] = out->unit[0] = '\0';
            return classified(in, out, second.input_end, TTF_SEM_TEXT, "ko:invalid-compound-range");
        }
        memcpy(out->value, first.value, sizeof(out->value));
        memcpy(out->secondary, second.value, sizeof(out->secondary));
        memcpy(out->unit, first.unit, sizeof(out->unit));
        out->layer = first.layer > second.layer ? first.layer : second.layer;
        if (delimiter_layer > out->layer)
            out->layer = delimiter_layer;
        return classified(in, out, second.input_end, TTF_SEM_RANGE,
                          shared_scale                     ? "ko:shared-scale-range"
                          : first.type == TTF_SEM_CURRENCY ? "ko:compound-currency-range"
                                                           : "ko:compound-quantity-range");
    }
    return 0;
}

/* Longest resource key that is a prefix of the Hangul word at `p`. */
static const char *band_cue(const ttf_pack_input_t *in, const char *kind, size_t begin,
                            size_t end) {
    char key[64];
    const char *best = NULL;
    for (size_t width = 1; width <= end - begin && width < sizeof(key); ++width) {
        const char *value;
        if (begin + width < end && ((unsigned char)in->text[begin + width] & 0xc0u) == 0x80u)
            continue;
        memcpy(key, in->text + begin, width);
        key[width] = 0;
        value = ttf_resource_get(in->engine, kind, "ko-KR", key, NULL);
        if (value)
            best = value;
    }
    return best;
}
static int band_value(const char *value) {
    size_t n = strlen(value);
    return (n == 2 && value[0] >= '1' && value[0] <= '9' && value[1] == '0') || n >= 5;
}
/* 대 counts machines, but after a decade number (20대) or a large scaled
   amount (5억대) it names a band. Adjacent resource cues decide first. */
static int band_role(const ttf_pack_input_t *in, const ttf_semantic_t *s) {
    size_t begin = in->tokens[in->index].begin, end = s->input_end, i;
    const char *cue = NULL;
    for (i = in->index; i > 0 && !cue; --i) {
        const ttf_token_t *t = &in->tokens[i - 1];
        if (t->script == TTF_SCRIPT_WHITESPACE && t->end == begin &&
            horizontal_space(in->text, t->begin, t->end)) {
            begin = t->begin;
            continue;
        }
        if (t->end == begin && t->script == TTF_SCRIPT_HANGUL)
            cue = band_cue(in, "counter_band_before", t->begin, t->end);
        break;
    }
    if (!cue) {
        size_t p = skip_spaces(in->text, in->length, end);
        for (i = in->index; i < in->token_count; ++i) {
            const ttf_token_t *t = &in->tokens[i];
            if (t->begin <= p && p < t->end) {
                /* The word may start inside the counter's own token (4대째, 2대나). */
                if (t->script == TTF_SCRIPT_HANGUL)
                    cue = band_cue(in, "counter_band_after", p, t->end);
                break;
            }
        }
    }
    if (cue)
        return !strcmp(cue, "band");
    return band_value(s->value) && (s->type != TTF_SEM_RANGE || band_value(s->secondary));
}

/* Re-running the shared classifier is only worth it when a 대 can still be
   part of this numeric object; a bare number never becomes a band. */
static int band_unit_ahead(const ttf_pack_input_t *in) {
    const char *unit = ttf_resource_get(in->engine, "unit", "ko-KR", "band", NULL);
    size_t p = in->tokens[in->index].begin, end = p + 32, width;
    if (!unit)
        return 0;
    width = strlen(unit);
    if (end > in->length)
        end = in->length;
    while (p < end) {
        if (in->text[p] == ' ' || in->text[p] == '\t' || in->text[p] == '\n')
            return 0;
        if (p + width <= end && !memcmp(in->text + p, unit, width))
            return 1;
        ++p;
    }
    return 0;
}

static size_t classify_grammar(const ttf_pack_input_t *in, ttf_semantic_t *s);
/* Pack-specific grammar creates language-neutral IR. */
static size_t classify(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    size_t n = classify_grammar(in, s);
    int universal = 0;
    char c = in->text[in->tokens[in->index].begin];
    if (!n && c >= '0' && c <= '9' && band_unit_ahead(in)) {
        ttf_semantic_t candidate = *s;
        n = ttf_classify_universal(in, &candidate);
        if (n) {
            *s = candidate;
            universal = 1;
        }
    }
    if (n && (s->type == TTF_SEM_COUNTER || s->type == TTF_SEM_RANGE) && !strcmp(s->unit, "대") &&
        band_role(in, s)) {
        snprintf(s->unit, sizeof(s->unit), "band");
        snprintf(s->rule, sizeof(s->rule), "%s",
                 s->type == TTF_SEM_RANGE ? "ko:band-range" : "ko:band-counter");
        return n;
    }
    if (universal) {
        memset(s, 0, sizeof(*s));
        s->struct_size = sizeof(*s);
        s->layer = TTF_LAYER_SYSTEM;
        return 0;
    }
    return n;
}

static size_t classify_grammar(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *text = in->text;
    size_t n = in->length, begin = in->tokens[in->index].begin, p, q;
    unsigned year, month, day;
    size_t identifier = ttf_ko_classify_basement(in, s);
    if (identifier)
        return identifier;
    identifier = ttf_ko_classify_floor(in, s);
    if (identifier)
        return identifier;
    if (in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
        identifier = ttf_ko_classify_lot_number(in, s);
        if (identifier)
            return identifier;
        identifier = ttf_ko_classify_batting_average(in, s);
        if (identifier)
            return identifier;
        identifier = ttf_ko_classify_event_date(in, s);
        if (identifier)
            return identifier;
        identifier = ttf_ko_classify_card(in, s);
        if (identifier)
            return identifier;
        identifier = ttf_ko_classify_plate(in, s);
        if (identifier)
            return identifier;
        identifier = ttf_ko_classify_digit_identifier(in, s);
        if (identifier)
            return identifier;
        identifier = ttf_ko_classify_model_number(in, s);
        if (identifier)
            return identifier;
    }
    identifier = ttf_ko_classify_identifier(in, s);
    if (identifier)
        return identifier;
    if (in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
        identifier = ttf_ko_classify_apparel_size(in, s);
        if (identifier)
            return identifier;
        identifier = ttf_ko_classify_alnum_identifier(in, s);
        if (identifier)
            return identifier;
        identifier = ttf_ko_classify_number_letters(in, s);
        if (identifier)
            return identifier;
        identifier = ttf_ko_classify_clothing_size(in, s);
        if (identifier)
            return identifier;
    }
    identifier = ttf_ko_classify_counter(in, s);
    if (identifier)
        return identifier;
    identifier = ttf_ko_classify_counted_noun(in, s);
    if (identifier)
        return identifier;
    p = unsigned_value(text, n, begin, &year);
    q = skip_spaces(text, n, p);
    if (p - begin == 4 && literal_at(text, n, q, "년")) {
        p = skip_spaces(text, n, q + strlen("년"));
        q = unsigned_value(text, n, p, &month);
        if (q != p && q - p <= 2) {
            q = skip_spaces(text, n, q);
            if (literal_at(text, n, q, "월")) {
                p = skip_spaces(text, n, q + strlen("월"));
                q = unsigned_value(text, n, p, &day);
                if (q != p && q - p <= 2) {
                    static const unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                    q = skip_spaces(text, n, q);
                    if (literal_at(text, n, q, "일")) {
                        unsigned max_day = month >= 1 && month <= 12 ? days[month - 1] : 0;
                        if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100)))
                            ++max_day;
                        if (!year || !month || month > 12 || !day || day > max_day)
                            return classified(in, s, q + strlen("일"), TTF_SEM_TEXT,
                                              "ko:invalid-calendar-date");
                        (void)snprintf(s->value, sizeof(s->value), "%04u-%02u-%02u", year, month,
                                       day);
                        return classified(in, s, q + strlen("일"), TTF_SEM_DATE,
                                          "ko:calendar-date");
                    }
                }
            }
        }
    }
    p = compound_currency_range(in, s);
    if (p)
        return p;
    return compound_amount(in, s);
}

static int script_score(ttf_script_t script) {
    return script == TTF_SCRIPT_HANGUL ? 100 : 0;
}

static int is_particle(const ttf_pack_input_t *in, const ttf_token_t *token) {
    size_t width;
    if (token->script != TTF_SCRIPT_HANGUL)
        return 0;
    /* Longest-prefix lookup permits particles attached to following predicates. */
    for (width = token->end - token->begin; width > 0; --width) {
        char key[64];
        if (width >= sizeof(key))
            continue;
        memcpy(key, in->text + token->begin, width);
        key[width] = '\0';
        if (ttf_resource_get(in->engine, "particle", "ko-KR", key, NULL))
            return 1;
    }
    return 0;
}

static int scope_context_score(const ttf_pack_input_t *in) {
    size_t i, hangul = 0;
    int score;
    for (i = 0; i < in->token_count; ++i)
        if (in->tokens[i].script == TTF_SCRIPT_HANGUL)
            ++hangul;
    score = (int)(hangul < 3 ? hangul : 3) *
            resource_integer(in->engine, "context", "hangul_weight", 0, 1000, 20);
    if (hangul) {
        int floor = resource_integer(in->engine, "context", "clause_floor", 0, 1000, 60);
        if (score < floor)
            score = floor;
    }
    return score;
}
static int run_particle_score(const ttf_pack_input_t *in, size_t begin, size_t end) {
    /* A particle scopes over the full preceding foreign noun phrase, including
       spaces and conjunctions. A stronger source-clause signal can still win. */
    return begin < end && end < in->token_count &&
                   in->tokens[end - 1].end == in->tokens[end].begin &&
                   is_particle(in, &in->tokens[end])
               ? resource_integer(in->engine, "context", "particle_weight", 0, 1000, 200)
               : 0;
}

typedef struct {
    int base;
    int scores[];
} context_scores_t;
static const char context_memo_identity = 0;
static ttf_status_t build_context_scores(const ttf_pack_input_t *in, void **out) {
    ttf_pack_input_t cursor = *in;
    context_scores_t *scores;
    size_t i, last_begin = SIZE_MAX, last_end = SIZE_MAX;
    int particle = 0;
    int script_bonus = resource_integer(in->engine, "context", "script_weight", 0, 1000, 1000);
    *out = NULL;
    if (in->token_count > (SIZE_MAX - sizeof(*scores)) / sizeof(scores->scores[0]))
        return TTF_ELIMIT;
    scores =
        (context_scores_t *)malloc(sizeof(*scores) + in->token_count * sizeof(scores->scores[0]));
    if (!scores)
        return TTF_ENOMEM;
    scores->base = scope_context_score(in);
    for (i = 0; i < in->token_count; ++i) {
        size_t begin, end;
        if (in->tokens[i].script == TTF_SCRIPT_HANGUL) {
            scores->scores[i] = scores->base + script_bonus;
            continue;
        }
        cursor.index = i;
        context_run_bounds(&cursor, &begin, &end);
        if (begin != last_begin || end != last_end) {
            particle = run_particle_score(&cursor, begin, end);
            last_begin = begin;
            last_end = end;
        }
        scores->scores[i] = scores->base + particle;
    }
    *out = scores;
    return TTF_OK;
}
static void dispose_context_scores(void *scores) {
    free(scores);
}
static int context_score(const ttf_pack_input_t *in) {
    context_scores_t *scores = NULL;
    size_t begin, end;
    int score;
    if (ttf_context_memo_enabled(in))
        scores = (context_scores_t *)ttf_context_memo_get(
            in, &context_memo_identity, build_context_scores, dispose_context_scores);
    if (scores)
        return in->index < in->token_count ? scores->scores[in->index] : scores->base;
    /* Direct callback users and reference tests retain the uncached policy. */
    score = scope_context_score(in);
    if (in->index >= in->token_count)
        return score;
    if (in->tokens[in->index].script == TTF_SCRIPT_HANGUL)
        return score + resource_integer(in->engine, "context", "script_weight", 0, 1000, 1000);
    context_run_bounds(in, &begin, &end);
    return score + run_particle_score(in, begin, end);
}

static ttf_status_t put_syllable(ttf_buffer_t *out, int onset, int vowel, int coda) {
    uint32_t cp = 0xac00u + (uint32_t)((onset * 21 + vowel) * 28 + coda);
    char utf8[3];
    utf8[0] = (char)(0xe0u | (cp >> 12));
    utf8[1] = (char)(0x80u | ((cp >> 6) & 0x3fu));
    utf8[2] = (char)(0x80u | (cp & 0x3fu));
    return ttf_buffer_append(out, utf8, sizeof(utf8));
}

static int onset(const ttf_engine_t *e, const char *phone) {
    return resource_integer(e, "phone_onset", phone, 0, 18, -1);
}

static int vowel(const ttf_engine_t *e, const char *phone) {
    return resource_integer(e, "phone_vowel", phone, 0, 20, -1);
}

static int short_vowel(const ttf_engine_t *e, const char *phone) {
    const char *length = ttf_resource_get(e, "phone_vowel_length", "ko-KR", phone, NULL);
    return length && !strcmp(length, "short");
}

/* 플랫폼, 딜리버리: an l after a vowel is written twice, as the coda of the
   syllable before it and again as its own onset. */
static int double_previous_l(ttf_buffer_t *out, size_t end, size_t word_begin) {
    unsigned char *tail;
    uint32_t cp;
    if (end < word_begin + 3)
        return 0;
    tail = (unsigned char *)out->data + end - 3;
    if ((tail[0] & 0xf0u) != 0xe0u || (tail[1] & 0xc0u) != 0x80u || (tail[2] & 0xc0u) != 0x80u)
        return 0;
    cp = ((uint32_t)(tail[0] & 0x0fu) << 12) | ((uint32_t)(tail[1] & 0x3fu) << 6) |
         (uint32_t)(tail[2] & 0x3fu);
    if (cp < 0xac00u || cp > 0xd7a3u || (cp - 0xac00u) % 28u)
        return 0;
    cp += 8u;
    tail[0] = (unsigned char)(0xe0u | (cp >> 12));
    tail[1] = (unsigned char)(0x80u | ((cp >> 6) & 0x3fu));
    tail[2] = (unsigned char)(0x80u | (cp & 0x3fu));
    return 1;
}

/* A trace is evidence of a consumed decision, not every resource lookup probe. */
static ttf_status_t traced_algorithm(const ttf_engine_t *e, const char *key, const char *text,
                                     const char *detail, size_t phone_index) {
    return ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, "ko-KR", "algorithm", key, "",
                          text, detail, phone_index);
}

static ttf_status_t flush_onset(const ttf_engine_t *e, ttf_buffer_t *out, int pending,
                                const char *phone, size_t phone_index) {
    size_t begin = out->length;
    /* Korean writes 이 rather than 으 after a palatal: package 패키지. */
    int filler =
        !strcmp(phone, "JH") || !strcmp(phone, "CH") || !strcmp(phone, "SH") || !strcmp(phone, "ZH")
            ? 20
            : 18;
    TRY(put_syllable(out, pending, filler, 0));
    TRY(ttf_trace_resource(e, "phone_onset", "ko-KR", phone, out->data + begin, "consumed-onset",
                           phone_index));
    return traced_algorithm(e, "epenthetic-vowel", out->data + begin, "eu-without-following-vowel",
                            phone_index);
}

static ttf_status_t render_phones(const ttf_engine_t *e, const ttf_pronunciation_t *p,
                                  ttf_buffer_t *out) {
    char phones[128][24];
    size_t count = 0, pos = 0, i, pending_index = SIZE_MAX, glide_index = SIZE_MAX;
    size_t word_begin = out->length;
    int pending = -1, glide = 0, glide_next = 0;
    if (strcmp(p->alphabet, "ttf-phones-v1") != 0 && strcmp(p->alphabet, "ARPABET") != 0 &&
        strcmp(p->alphabet, "arpabet") != 0)
        return TTF_EINVAL;
    while (p->phones[pos]) {
        size_t n = 0;
        while (p->phones[pos] == ' ')
            ++pos;
        if (!p->phones[pos])
            break;
        if (count == 128)
            return TTF_ELIMIT;
        while (p->phones[pos] && p->phones[pos] != ' ') {
            if (n + 1 >= sizeof(phones[0]))
                return TTF_ELIMIT;
            phones[count][n++] = p->phones[pos++];
        }
        if (n && phones[count][n - 1] >= '0' && phones[count][n - 1] <= '9')
            --n;
        phones[count++][n] = '\0';
    }
    for (i = 0; i < count; ++i) {
        const char *phone = phones[i];
        int v, c;
        if (!strcmp(phone, ".")) {
            if (pending >= 0)
                TRY(flush_onset(e, out, pending, phones[pending_index], pending_index));
            pending = -1;
            continue;
        }
        v = vowel(e, phone);
        c = onset(e, phone);
        const char *letter = ttf_resource_get(e, "phone_letter", "ko-KR", phone, NULL);
        if (letter) {
            if (pending >= 0)
                TRY(flush_onset(e, out, pending, phones[pending_index], pending_index));
            pending = -1;
            TRY(ttf_buffer_puts(out, letter));
            TRY(ttf_trace_resource(e, "phone_letter", "ko-KR", phone, letter, "direct-letter-name",
                                   i));
            continue;
        }
        if (!strcmp(phone, "SH") && i + 1 < count && vowel(e, phones[i + 1]) >= 0) {
            /* 셔, 쇼: a palatal fricative palatalises the following vowel. */
            glide = 2;
            glide_index = i;
        }
        if (strcmp(phone, "W") == 0 || strcmp(phone, "Y") == 0) {
            /* 스위트, 트윈: a glide starts its own syllable after most consonants.
               A velar keeps it (퀵), so the pack asks the data which it is. */
            if (pending >= 0 && !strcmp(phone, "W") &&
                strcmp(resource(e, "phone_glide", phones[pending_index], "split"), "merge")) {
                TRY(flush_onset(e, out, pending, phones[pending_index], pending_index));
                pending = -1;
            }
            glide = strcmp(phone, "W") == 0 ? 1 : 2;
            glide_index = i;
            continue;
        }
        if (v >= 0) {
            size_t phone_index = i, coda_index = SIZE_MAX, begin = out->length;
            int cod = 0, lengthened = 0;
            if (glide == 1) {
                if (v == 0)
                    v = 9;
                else if (v == 4 || v == 6 || v == 8)
                    v = 14;
                else if (v == 1)
                    v = 10;
                else if (v == 20)
                    v = 16;
                else if (v == 5)
                    v = 15;
            } else if (glide == 2) {
                if (v == 0)
                    v = 2;
                else if (v == 4)
                    v = 6;
                else if (v == 8)
                    v = 12;
                else if (v == 13)
                    v = 17;
                else if (v == 5)
                    v = 7;
                else if (v == 1)
                    v = 3;
            }
            /* Korean loanword spelling drops the r-colouring after a vowel:
               market is 마켓, not 마르케트. The policy is a resource flag. */
            if (i + 1 < count && !strcmp(phones[i + 1], "R") &&
                (i + 2 == count || vowel(e, phones[i + 2]) < 0) &&
                !strcmp(resource(e, "phone_mapping", "coda_r", "drop"), "drop")) {
                TRY(traced_algorithm(e, "coda-r", "", "dropped-coda-r", i + 1));
                ++i;
                /* The vowel keeps the length of the r it absorbed, so the stop
                   after it is written with its own vowel: smart 스마트. */
                lengthened = 1;
            }
            /* Nasals/laterals close syllables before a consonant or word end. */
            if (i + 1 < count && (i + 2 == count || vowel(e, phones[i + 2]) < 0) &&
                (i + 2 == count || strcmp(phones[i + 2], "Y")) &&
                (i + 2 == count || strcmp(phones[i + 2], "W") || !strcmp(phones[i + 1], "M") ||
                 !strcmp(phones[i + 1], "N") || !strcmp(phones[i + 1], "NG") ||
                 !strcmp(phones[i + 1], "L"))) {
                /* An explicit boundary from the data closes the syllable whatever
                   the phonological default is: up.loading 업로딩. */
                int boundary = i + 2 < count && !strcmp(phones[i + 2], ".");
                int close = resource_integer(e, "phone_coda", phones[i + 1], 0, 27, -1);
                int stop = !strcmp(phones[i + 1], "P") || !strcmp(phones[i + 1], "T") ||
                           !strcmp(phones[i + 1], "K");
                /* A stop closes the syllable only after a short vowel (market 마켓,
                   not 업데잇) and never before a nasal or liquid (partner 파트너). */
                if (!boundary && stop &&
                    (lengthened || !short_vowel(e, phone) ||
                     (i + 2 < count && onset(e, phones[i + 2]) >= 0 &&
                      vowel(e, phones[i + 2]) < 0 &&
                      (!strcmp(phones[i + 2], "N") || !strcmp(phones[i + 2], "M") ||
                       !strcmp(phones[i + 2], "NG") || !strcmp(phones[i + 2], "L") ||
                       !strcmp(phones[i + 2], "R")))))
                    close = -1;
                if (close >= 0) {
                    cod = close;
                    coda_index = ++i;
                }
            }
            /* 파워, 타워: AW before ER gives its u-tail to the next syllable as w. */
            int aw_glide = !strcmp(phone, "AW") && i + 1 < count && !strcmp(phones[i + 1], "ER");
            {
                int tail = !strcmp(phone, "AY") || !strcmp(phone, "EY") || !strcmp(phone, "OY") ||
                           !strcmp(phone, "AW");
                /* 포인트: the coda closes the tail syllable, not the nucleus. */
                TRY(put_syllable(out, pending >= 0 ? pending : 11, v, tail ? 0 : cod));
            }
            if (pending == onset(e, "L") && pending_index != SIZE_MAX &&
                !strcmp(phones[pending_index], "L") &&
                !strcmp(resource(e, "phone_mapping", "onset_l", "single"), "double") &&
                double_previous_l(out, begin, word_begin))
                TRY(traced_algorithm(e, "doubled-l", "", "l-after-vowel", pending_index));
            if (pending >= 0)
                TRY(ttf_trace_resource(e, "phone_onset", "ko-KR", phones[pending_index],
                                       out->data + begin, "consumed-onset", pending_index));
            else
                TRY(traced_algorithm(e, "vowel-initial-onset", out->data + begin,
                                     "silent-ieung-onset", phone_index));
            TRY(ttf_trace_resource(e, "phone_vowel", "ko-KR", phone, out->data + begin,
                                   "consumed-vowel", phone_index));
            if (glide)
                TRY(traced_algorithm(e, "glide", out->data + begin,
                                     glide == 1 ? "w-before-vowel" : "y-before-vowel",
                                     glide_index));
            if (coda_index != SIZE_MAX)
                TRY(ttf_trace_resource(e, "phone_coda", "ko-KR", phones[coda_index],
                                       out->data + begin, "consumed-coda", coda_index));
            begin = out->length;
            if (strcmp(phone, "AY") == 0 || strcmp(phone, "EY") == 0 || strcmp(phone, "OY") == 0) {
                TRY(put_syllable(out, 11, 20, cod));
                TRY(traced_algorithm(e, "diphthong-tail", out->data + begin, "i-tail",
                                     phone_index));
            } else if (aw_glide) {
                glide_next = 1;
                TRY(traced_algorithm(e, "diphthong-tail", out->data + begin, "u-tail-as-w",
                                     phone_index));
            } else if (strcmp(phone, "AW") == 0) {
                TRY(put_syllable(out, 11, 13, cod));
                TRY(traced_algorithm(e, "diphthong-tail", out->data + begin, "u-tail",
                                     phone_index));
            }
            /* 딜리버리, 배터리: the r-colouring of ER becomes the onset of the
               following syllable rather than disappearing with the vowel. */
            if (!strcmp(phone, "ER") && i + 1 < count && vowel(e, phones[i + 1]) >= 0 &&
                !strcmp(resource(e, "phone_mapping", "linking_r", "drop"), "link")) {
                pending = onset(e, "R");
                pending_index = i;
                TRY(traced_algorithm(e, "linking-r", "", "r-before-vowel", i));
            } else
                pending = -1;
            glide = glide_next;
            glide_next = 0;
        } else if (c >= 0) {
            /* 심플, 프리즘: a sonorant that ends a word after another consonant is
               the coda of the inserted vowel, not a syllable of its own. */
            if (pending >= 0 && i + 1 == count) {
                int close = resource_integer(e, "phone_coda", phone, 0, 27, -1);
                if (close >= 0 && (!strcmp(phone, "L") || !strcmp(phone, "M") ||
                                   !strcmp(phone, "N") || !strcmp(phone, "NG"))) {
                    size_t begin = out->length;
                    TRY(put_syllable(out, pending, 18, close));
                    TRY(ttf_trace_resource(e, "phone_coda", "ko-KR", phone, out->data + begin,
                                           "syllabic-sonorant", i));
                    pending = -1;
                    continue;
                }
            }
            if (strcmp(phone, "R") == 0 && i + 1 == count) {
                TRY(traced_algorithm(e, "trailing-r", "", "prototype-r-suppression", i));
                continue;
            }
            if (pending >= 0)
                TRY(flush_onset(e, out, pending, phones[pending_index], pending_index));
            pending = c;
            pending_index = i;
        } else
            return TTF_EINVAL;
    }
    if (pending >= 0)
        TRY(flush_onset(e, out, pending, phones[pending_index], pending_index));
    return out->length ? TTF_OK : TTF_EINVAL;
}

static ttf_status_t render_graphemes(const ttf_engine_t *e, const char *word, size_t length,
                                     ttf_buffer_t *out) {
    size_t i = 0;
    while (i < length) {
        char key[8];
        const char *mapped = NULL;
        size_t width, max_width = length - i < 3 ? length - i : 3;
        for (width = max_width; width > 0; --width) {
            size_t j;
            for (j = 0; j < width; ++j) {
                unsigned char c = (unsigned char)word[i + j];
                key[j] = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
            }
            key[width] = '\0';
            mapped = ttf_resource_get(e, "grapheme", "ko-KR", key, NULL);
            if (mapped)
                break;
        }
        if (mapped) {
            TRY(ttf_buffer_puts(out, mapped));
            TRY(ttf_trace_resource(e, "grapheme", "ko-KR", key, mapped, "longest-grapheme",
                                   SIZE_MAX));
            i += width;
        } else if (word[i] >= '0' && word[i] <= '9') {
            char digit_key[2] = {word[i], '\0'};
            size_t begin = out->length;
            TRY(digit(e, (unsigned)(word[i] - '0'), 0, out));
            if (ttf_resource_get(e, "digit", "ko-KR", digit_key, NULL))
                TRY(ttf_trace_resource(e, "digit", "ko-KR", digit_key, out->data + begin,
                                       "digit-fallback", SIZE_MAX));
            else
                TRY(traced_algorithm(e, "digit-fallback", out->data + begin, "builtin-digit",
                                     SIZE_MAX));
            ++i;
        } else {
            /* A letter-name fallback always speaks unfamiliar Latin letters. */
            char letter[16] = "letter.A";
            const char *name;
            unsigned char c = (unsigned char)word[i];
            size_t begin = out->length;
            letter[7] = (char)(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c);
            name = ttf_resource_get(e, "phone_letter", "ko-KR", letter, NULL);
            if (name) {
                TRY(ttf_buffer_puts(out, name));
                TRY(ttf_trace_resource(e, "phone_letter", "ko-KR", letter, name,
                                       "letter-name-fallback", SIZE_MAX));
            } else {
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                    TRY(ttf_buffer_puts(out, "으"));
                else if (c < 128)
                    TRY(ttf_buffer_append(out, word + i, 1));
                /* Accented Latin codepoints use a speakable escape when not mapped. */
                else if ((c & 0xc0) != 0x80)
                    TRY(ttf_buffer_puts(out, "으"));
                if (out->length != begin)
                    TRY(traced_algorithm(e, "unmapped-grapheme", out->data + begin,
                                         c < 128 &&
                                                 !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                                             ? "preserved-ascii"
                                             : "speakable-escape",
                                         SIZE_MAX));
            }
            ++i;
        }
    }
    return TTF_OK;
}

const ttf_language_pack_t ttf_pack_ko_KR = {sizeof(ttf_language_pack_t),
                                            TTF_ABI_VERSION,
                                            "ko-KR",
                                            NULL,
                                            script_score,
                                            context_score,
                                            classify,
                                            verbalize,
                                            NULL,
                                            render_phones,
                                            render_graphemes};
