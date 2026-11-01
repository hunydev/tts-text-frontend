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

/* -2℃ 영하 이 도 (owner, 2026-10-14): a temperature below zero reads 영하 and
   drops 섭씨 (quantity_prefix below_zero.<unit>). NULL when the unit has none. */
static const char *below_zero(const ttf_engine_t *e, const ttf_semantic_t *s) {
    char key[sizeof(s->unit) + 16];
    if (s->value[0] != '-' || !s->unit[0])
        return NULL;
    (void)snprintf(key, sizeof(key), "below_zero.%s", s->unit);
    return ttf_resource_get(e, "quantity_prefix", "ko-KR", key, NULL);
}

static ttf_status_t cardinal(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    char integer[TTF_IR_VALUE_SIZE];
    const char *p = value, *fraction = NULL;
    size_t n = 0, start, groups, i;
    static const char *const powers[] = {"",   "만", "억", "조", "경", "해",
                                         "자", "양", "구", "간", "정"};
    const char *exponent = strchr(value, 'e');
    if (exponent) {
        /* 1.25e4 일 점 이오만: a written decimal before its scale word (written_scale) */
        char mantissa[TTF_IR_VALUE_SIZE], key[16];
        unsigned power = (unsigned)atoi(exponent + 1);
        if ((size_t)(exponent - value) >= sizeof(mantissa) || power < 4 || power > 16 || power % 4)
            return TTF_EINVAL;
        (void)snprintf(mantissa, sizeof(mantissa), "%.*s", (int)(exponent - value), value);
        TRY(cardinal(e, mantissa, out));
        (void)snprintf(key, sizeof(key), "10^%u", power);
        return ttf_buffer_puts(out, resource(e, "magnitude", key, powers[power / 4]));
    }
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
    unsigned long n;
    char key[16];
    const char *override;
    /* +9시간 플러스 아홉 시간 (owner sets, 2026-10-14; the sign was dropped) */
    if (*value == '+' || *value == '-') {
        TRY(ttf_buffer_puts(out, *value == '+' ? resource(e, "number", "positive", "플러스 ")
                                                : resource(e, "number", "negative", "마이너스 ")));
        ++value;
    }
    n = strtoul(value, &end, 10);
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
/* A chemical formula's count as an English numeral (H₂ 투, C₁₂ 트웰브, 2026-10-26): ten and below
   from english_numeral, 11-99 from the teen and tens words, longer counts digit by digit. */
static ttf_status_t formula_count(const ttf_engine_t *e, const char *digits, ttf_buffer_t *out) {
    static const char *const teens[] = {"eleven", "twelve", "thirteen", "fourteen", "fifteen",
                                        "sixteen", "seventeen", "eighteen", "nineteen"};
    static const char *const tens[] = {"twenty", "thirty", "forty", "fifty", "sixty", "seventy",
                                       "eighty", "ninety"};
    const char *word = english_numeral(e, digits), *ones = NULL;
    char key[24];
    size_t n = strlen(digits), i;
    if (word)
        return ttf_buffer_puts(out, word);
    if (n == 2 && digits[0] != '0') {
        unsigned value = (unsigned)(digits[0] - '0') * 10 + (unsigned)(digits[1] - '0');
        char unit[2] = {digits[1], '\0'};
        (void)snprintf(key, sizeof(key), "en-US:%s", value < 20 ? teens[value - 11] : tens[value / 10 - 2]);
        word = ttf_resource_get(e, "lexicon", "ko-KR", key, NULL);
        ones = value < 20 || value % 10 == 0 ? "" : english_numeral(e, unit);
        if (word && ones) {
            ttf_status_t st = ttf_buffer_puts(out, word);
            return st == TTF_OK ? ttf_buffer_puts(out, ones) : st;
        }
    }
    for (i = 0; i < n; ++i) {
        char one[2] = {digits[i], '\0'};
        if (!(word = english_numeral(e, one)))
            return TTF_EINVAL;
        ttf_status_t st = ttf_buffer_puts(out, word);
        if (st != TTF_OK)
            return st;
    }
    return TTF_OK;
}
static ttf_status_t expression_number(const ttf_engine_t *e, const char *value, ttf_buffer_t *out);
static ttf_status_t algebra_read(const ttf_engine_t *e, const char *value, ttf_buffer_t *out);
static ttf_status_t chem_read(const ttf_engine_t *e, const char *value, ttf_buffer_t *out);
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
    int letter = 0;
    while (at < length) {
        char part[48];
        size_t end = at;
        while (end < length && value[end] != ' ')
            ++end;
        if (end == at || end - at >= sizeof(part))
            return TTF_EINVAL;
        memcpy(part, value + at, end - at);
        part[end - at] = 0;
        if (part[0] == '^' && part[1]) {
            /* 3² 삼의 제곱, 10³ 십의 세제곱, 2¹⁰ 이의 십제곱, a² 에이 제곱: the power of the operand
               before it (owner request 2026-10-23; the superscript was passed through) */
            const char *power = part + 1;
            /* a power alone (ko:trailing-power) follows a word already read: 엑스 제곱 */
            TRY(ttf_buffer_puts(out, letter || !at ? " " : "의 "));
            if (!strcmp(power, "2"))
                st = ttf_buffer_puts(out, resource(e, "math_power", "2", "제곱"));
            else if (!strcmp(power, "3"))
                st = ttf_buffer_puts(out, resource(e, "math_power", "3", "세제곱"));
            /* x⁴ 엑스 네제곱: the dictionary word (2026-10-31, TN round 15; was 사제곱) */
            else if (!strcmp(power, "4"))
                st = ttf_buffer_puts(out, resource(e, "math_power", "4", "네제곱"));
            else {
                if (*power == '-') {
                    TRY(ttf_buffer_puts(out, resource(e, "number", "negative", "마이너스 ")));
                    ++power;
                }
                TRY(cardinal(e, power, out));
                st = ttf_buffer_puts(out, resource(e, "math_power", "n", "제곱"));
            }
            if (st != TTF_OK)
                return st;
            at = end < length ? end + 1 : end;
            continue;
        }
        if (at && (st = ttf_buffer_puts(out, " ")) != TTF_OK)
            return st;
        letter = !part[1] && ((part[0] >= 'a' && part[0] <= 'z') || (part[0] >= 'A' && part[0] <= 'Z'));
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
        /* 10.20.30.45 일공 점 이공: an IP address reads its zero 공, as codes do
           (project-owner decision 2026-10-23) */
        if (s->type == TTF_SEM_IP_ADDRESS && part.type == TTF_SEM_DIGIT_SEQUENCE)
            status = digits(e, part.value, 1, out);
        else
            status = verbalize(e, &part, surface + at, end - at, out);
        if (status != TTF_OK)
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
    /* 2~4시간 두 시간에서 네 시간: a native first number keeps its unit, a Sino one stays
       bare (2~4층 이에서 사 층) (project-owner decision 2026-10-23; was 둘에서 네 시간) */
    if (native_endpoint(e, endpoint) && endpoint->type == TTF_SEM_COUNTER &&
        plain_integer(s->value, &value) && value <= 99)
        return verbalize(e, endpoint, surface, length, out);
    return cardinal(e, s->value, out);
}
/* U+20A9: 유 플러스 이공에이구. The hex digits are spelled and 0 reads 공, as in
   other digit codes (2026-10-13). */
static ttf_status_t codepoint(const ttf_engine_t *e, const char *hex, ttf_buffer_t *out) {
    const char *plus = ttf_resource_get(e, "speech_symbol", "ko-KR", "+", NULL);
    const char *zero = ttf_resource_get(e, "number", "ko-KR", "digits_zero", NULL);
    ttf_status_t st;
    size_t i;
    if (!plus || !zero)
        return TTF_ERESOURCE;
    if (!*hex)
        return TTF_EINVAL;
    if ((st = ttf_ko_speech_spelling(e, "U", 1, 0, out)) != TTF_OK ||
        (st = ttf_buffer_puts(out, " ")) != TTF_OK || (st = ttf_buffer_puts(out, plus)) != TTF_OK ||
        (st = ttf_buffer_puts(out, " ")) != TTF_OK)
        return st;
    for (i = 0; hex[i]; ++i)
        if ((st = hex[i] == '0' ? ttf_buffer_puts(out, zero)
                                : ttf_ko_speech_spelling(e, hex + i, 1, 0, out)) != TTF_OK)
            return st;
    return TTF_OK;
}

static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *out) {
    switch (s->type) {
    case TTF_SEM_CUSTOM:
        if (!strcmp(s->unit, "@codepoint"))
            return e->frontend_options.output_mode == TTF_OUTPUT_SPEECH
                       ? codepoint(e, s->value, out)
                       : ttf_buffer_append(out, surface, length);
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
        if (!strcmp(s->rule, "ko:chemical-formula")) {
            /* 에이치투에스오포: each element spelled, each count an English numeral, joined */
            const char *p = s->value;
            while (*p) {
                size_t m = p[1] >= 'a' && p[1] <= 'z' ? 2 : 1, c = 0;
                char count[8];
                TRY(ttf_ko_speech_spelling(e, p, m, 0, out));
                p += m;
                while (*p >= '0' && *p <= '9' && c + 1 < sizeof(count))
                    count[c++] = *p++;
                count[c] = '\0';
                if (c)
                    TRY(formula_count(e, count, out));
            }
            return TTF_OK;
        }
        if (!strcmp(s->rule, "ko:ratio")) {
            /* 일 대 이만 오천, 이 대 이 대 일 */
            const char *term = s->value, *colon = strchr(term, ':');
            char part[24];
            if (!colon)
                return TTF_EINVAL;
            for (;;) {
                size_t len = colon ? (size_t)(colon - term) : strlen(term);
                if (!len || len >= sizeof(part))
                    return TTF_EINVAL;
                (void)snprintf(part, sizeof(part), "%.*s", (int)len, term);
                TRY(cardinal(e, part, out));
                if (!colon)
                    return TTF_OK;
                TRY(ttf_buffer_puts(out, " "));
                TRY(ttf_buffer_puts(out, resource(e, "policy", "score_separator", "대")));
                TRY(ttf_buffer_puts(out, " "));
                term = colon + 1;
                colon = strchr(term, ':');
            }
        }
        if (!strcmp(s->rule, "ko:court-case")) {
            /* 이천이십오 타경 만 사백구십이 */
            const char *colon = strchr(s->value, ':');
            char year[8];
            if (!colon || colon - s->value != 4)
                return TTF_EINVAL;
            (void)snprintf(year, sizeof(year), "%.4s", s->value);
            TRY(cardinal(e, year, out));
            TRY(ttf_buffer_puts(out, " "));
            TRY(ttf_buffer_puts(out, s->secondary));
            TRY(ttf_buffer_puts(out, " "));
            return cardinal(e, colon + 1, out);
        }
        if (!strcmp(s->rule, "ko:letters-decimal")) {
            /* 피엠 이 점 오 */
            TRY(ttf_ko_speech_spelling(e, s->secondary, strlen(s->secondary), 0, out));
            TRY(ttf_buffer_puts(out, " "));
            return cardinal(e, s->value, out);
        }
        if (!strcmp(s->rule, "ko:quarter")) {
            /* 일 사분기 */
            TRY(cardinal(e, s->value, out));
            TRY(ttf_buffer_puts(out, " "));
            return cardinal(e, "4", out);
        }
        if (!strcmp(s->rule, "ko:below-zero")) {
            if (s->secondary[0])
                TRY(ttf_buffer_puts(out, resource(e, "quantity_prefix", "below_zero.°C", "영하 ")));
            return cardinal(e, s->value, out);
        }
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
            unsigned long large;
            if ((strcmp(policy, "sino") && strcmp(policy, "mixed")) ||
                (strcmp(reading, "native") && strcmp(reading, "sino")))
                return TTF_ERESOURCE;
            /* mixed (owner sets, 2026-10-14): 101번째 백한 번째, 203번째 이백세 번째; 176번째
               stays 백칠십육 번째 */
            large = strtoul(s->value, NULL, 10);
            if (!strcmp(policy, "mixed") && !strcmp(reading, "native") && large > 100 && large % 100 &&
                large % 100 < 10 &&
                strlen(s->value) < 16) {
                char hundreds[24], rest[8];
                (void)snprintf(hundreds, sizeof(hundreds), "%lu", large - large % 100);
                (void)snprintf(rest, sizeof(rest), "%lu", large % 100);
                TRY(cardinal(e, hundreds, out));
                TRY(native_number(e, rest, out));
                TRY(ttf_buffer_puts(out, " "));
                return ttf_buffer_puts(out, resource(e, "unit", s->unit, s->unit));
            }
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
        if (!strcmp(s->unit, "spaced_groups")) {
            /* 6845 2201 3379 육팔사오 이이공일 삼삼칠구 */
            const char *g = s->value;
            char part[8];
            while (*g) {
                size_t l = strcspn(g, " ");
                if (l >= sizeof(part))
                    return TTF_EINVAL;
                memcpy(part, g, l);
                part[l] = 0;
                TRY(ttf_ko_render_identifier(e, part, out));
                g += l;
                if (*g == ' ') {
                    TRY(ttf_buffer_puts(out, " "));
                    ++g;
                }
            }
            return TTF_OK;
        }
        if (!strcmp(s->unit, "zero_number")) {
            /* 0427번 공사이칠번 (owner sets, 2026-10-14; attached like 공삼번 in ko_bench_fixes_v32) */
            TRY(ttf_ko_render_identifier(e, s->value, out));
            return ttf_buffer_puts(out, "번");
        }
        if (!strcmp(s->unit, "clothing_size")) {
            /* 66 육육, 55반 오오반, 55~77 오오에서 칠칠. */
            const char *v = s->value;
            while (*v) {
                if (*v >= '0' && *v <= '9' && v[1] >= '0' && v[1] <= '9') {
                    char pair[3] = {v[0], v[1], 0};
                    TRY(ttf_ko_render_identifier(e, pair, out));
                    v += 2;
                } else if (*v == '~') {
                    TRY(ttf_buffer_puts(out, "에서 "));
                    ++v;
                } else {
                    size_t w = ((unsigned char)*v & 0xf0) == 0xe0 ? 3 : 1; /* 반 */
                    TRY(ttf_buffer_append(out, v, w));
                    v += w;
                }
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
        const char *month = s->value, *below = below_zero(e, s);
        if (below) {
            TRY(ttf_buffer_puts(out, below));
            return quantity(e, s->value + 1, resource(e, "unit", s->unit, s->unit), 0, out);
        }
        /* -10~40℃ 영하 십 도에서 사십 도: the end after a range from below zero */
        if (strcmp(s->rule, "ko:after-below-zero"))
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
        /* 1,182.60원 천백팔십이 점 육영 원: a won amount reads every written digit. A foreign
           amount with two decimals (cents) drops their trailing zeros: €89.90 팔십구 점 구 유로,
           $15.00 십오 달러 (owner-delegated decision 2026-10-27: the owner sets and v3 accept
           it, as spoken; the Korean release references read 영 and lose about 20 sentences).
           A rate with more decimals keeps them (1.13890달러). Was: 팔십구 점 구영 유로. */
        {
            const char *dot = strchr(s->value, '.');
            if (strcmp(s->unit, "KRW") && dot && strlen(dot + 1) == 2) {
                char amount[TTF_IR_VALUE_SIZE];
                size_t len;
                (void)snprintf(amount, sizeof(amount), "%s", s->value);
                len = strlen(amount);
                while (amount[len - 1] == '0')
                    amount[--len] = '\0';
                if (amount[len - 1] == '.')
                    amount[--len] = '\0';
                return quantity(e, amount, resource(e, "currency", s->unit, s->unit), 0, out);
            }
        }
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
        if (below_zero(e, s)) {
            TRY(ttf_buffer_puts(out, below_zero(e, s)));
            return quantity(e, s->value + 1, resource(e, "unit", s->unit, s->unit), 0, out);
        }
        TRY(ttf_buffer_puts(out, resource(e, "quantity_prefix", s->unit, "")));
        if (!strcmp(s->rule, "ko:basement-floor") && s->secondary[0]) {
            /* B3F로 지하 삼 층으로: the particle the recognizer agreed (patterns.c) */
            TRY(quantity(e, s->value, resource(e, "unit", s->unit, s->unit), 0, out));
            return ttf_buffer_puts(out, s->secondary);
        }
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
        /* A prefixed unit (섭씨, 시속) keeps its prefix at both ends and its unit once:
           섭씨 육십에서 섭씨 칠십 도 (project-owner decision 2026-10-23; was 섭씨 육십 도에서
           섭씨 칠십 도). Below zero keeps both readings. */
        int prefixed = s->unit[0] && *resource(e, "quantity_prefix", s->unit, "") &&
                       range_unit_shared(surface, length) && endpoint.type != TTF_SEM_ORDINAL &&
                       !below_zero(e, s) && s->secondary[0] != '-';
        if (!range_unit_shared(surface, length) || endpoint.type == TTF_SEM_ORDINAL ||
            (s->unit[0] && *resource(e, "quantity_prefix", s->unit, "")))
            repeat_unit = 1;
        if (quantity_range && !repeat_unit)
            TRY(range_contraction(e, s, &endpoint, surface, length, out, &contracted));
        if (contracted)
            return TTF_OK;
        if (unit_repeat && !strcmp(unit_repeat, "wide"))
            repeat_unit = 1;
        if (prefixed) {
            TRY(ttf_buffer_puts(out, resource(e, "quantity_prefix", s->unit, "")));
            TRY(cardinal(e, s->value, out));
        } else if (repeat_unit || !s->unit[0] || !strcmp(s->unit, "time"))
            TRY(verbalize(e, &endpoint, surface, length, out));
        else
            TRY(range_first_bare(e, s, &endpoint, surface, length, out));
        TRY(ttf_buffer_puts(
            out,
            resource(e, "policy", quantity_range ? "quantity_range_separator" : "range_separator",
                     "에서 ")));
        if (below_zero(e, s))
            (void)snprintf(endpoint.rule, sizeof(endpoint.rule), "ko:after-below-zero");
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
        TRY(quantity(e, value, resource(e, "unit", "day", "일"), 0, out));
        if (!strcmp(s->secondary, "range-from"))
            TRY(ttf_buffer_puts(out, resource(e, "policy", "range_separator", "에서 ")));
        return TTF_OK;
    }
    case TTF_SEM_TIME: {
        unsigned hour, minute, second;
        int fields;
        char value[16];
        fields = sscanf(s->value, "%u:%u:%u", &hour, &minute, &second);
        if (fields < 2)
            return TTF_EINVAL;
        if (!strcmp(s->rule, "ko:duration")) {
            /* 3:05짜리 삼 분 오 초; 2:58:30 두 시간 오십팔 분 삼십 초 */
            unsigned part[3] = {hour, minute, second};
            static const char *const units[][2] = {{"hours", "시간"}, {"minutes", "분"}, {"seconds", "초"}};
            /* 비행 2:10 소요 두 시간 십 분 (unit hours); 0:45 길이 사십오 초: a zero leading unit is
               not said (2026-10-26) */
            int k, first = fields == 3 || !strcmp(s->unit, "hours") ? 0 : 1, emitted = 0,
                   last = first + fields - 1;
            for (k = first; k < 3 && k - first < fields; ++k) {
                if (!part[k - first] && (emitted || k < last))
                    continue;
                if (emitted)
                    TRY(ttf_buffer_puts(out, " "));
                (void)snprintf(value, sizeof(value), "%u", part[k - first]);
                TRY(quantity(e, value, resource(e, "unit", units[k][0], units[k][1]), k == 0, out));
                emitted = 1;
            }
            if (!strcmp(s->unit, "minutes-fraction") && strchr(s->value, '.')) {
                /* the hundredths after the seconds, as a number (오십팔 초 십이) */
                TRY(ttf_buffer_puts(out, " "));
                TRY(cardinal(e, strchr(s->value, '.') + 1, out));
            }
            return TTF_OK;
        }
        if (!strcmp(s->rule, "ko:clock-24")) {
            /* 이십사 시: the Sino number, as the hours past twelve */
            TRY(cardinal(e, "24", out));
            TRY(ttf_buffer_puts(out, " "));
            return ttf_buffer_puts(out, resource(e, "unit", "hour", "시"));
        }
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
        if (!strcmp(s->unit, "@algebra"))
            return algebra_read(e, s->value, out);
        if (!strcmp(s->unit, "@chem"))
            return chem_read(e, s->value, out);
        if (!strcmp(s->unit, "@word"))
            return ttf_buffer_puts(out, s->value);
        if (!strcmp(s->unit, "@times"))
            return quantity(e, s->value, resource(e, "unit", "times", "배"), !strchr(s->value, '.') && atoi(s->value) <= 10, out);
        if (!strcmp(s->unit, "@quarter"))
            return quantity(e, s->value, resource(e, "unit", "quarter", "분기"), 0, out);
        if (!strcmp(s->unit, "@quarter-year")) {
            /* q|yy: 이십삼 년 사 분기 */
            char quarter[2] = {s->value[0], 0};
            if (!s->value[0] || s->value[1] != '|')
                return TTF_EINVAL;
            TRY(cardinal(e, s->value + 2, out));
            TRY(ttf_buffer_puts(out, " 년 "));
            return quantity(e, quarter, resource(e, "unit", "quarter", "분기"), 0, out);
        }
        if (!strcmp(s->unit, "@official-number")) {
            /* [제]year|serial: 제이천이십사 다시 백이십삼 호 */
            char year[8];
            const char *v = s->value, *bar = strchr(v, '|');
            if (!strncmp(v, "제", 3)) {
                TRY(ttf_buffer_puts(out, "제"));
                v += 3;
            }
            if (!bar || bar < v || (size_t)(bar - v) >= sizeof(year))
                return TTF_EINVAL;
            (void)snprintf(year, sizeof(year), "%.*s", (int)(bar - v), v);
            TRY(cardinal(e, year, out));
            TRY(ttf_buffer_puts(out, " 다시 "));
            TRY(cardinal(e, bar + 1, out));
            return ttf_buffer_puts(out, " 호");
        }
        if (!strcmp(s->unit, "@field-mark")) {
            /* m|cc[|cm]: 팔 미터 이십이, 팔 미터 십오 센티미터 */
            char metres[8], centimetres[4];
            const char *bar = strchr(s->value, '|');
            if (!bar || (size_t)(bar - s->value) >= sizeof(metres) || strlen(bar + 1) < 2)
                return TTF_EINVAL;
            (void)snprintf(metres, sizeof(metres), "%.*s", (int)(bar - s->value), s->value);
            (void)snprintf(centimetres, sizeof(centimetres), "%.2s", bar + 1);
            TRY(quantity(e, metres, resource(e, "unit", "meter", "미터"), 0, out));
            TRY(ttf_buffer_puts(out, " "));
            if (strchr(bar + 1, '|'))
                return quantity(e, centimetres, resource(e, "unit", "centimeter", "센티미터"), 0, out);
            return cardinal(e, centimetres, out);
        }
        if (!strcmp(s->unit, "@inch"))
            return quantity(e, s->value, resource(e, "unit", "inch", "인치"), 0, out);
        if (!strcmp(s->unit, "@count-per")) {
            /* time|count: 분당 칠십이 회 */
            char label[16];
            const char *bar = strchr(s->value, '|');
            if (!bar || (size_t)(bar - s->value) >= sizeof(label))
                return TTF_EINVAL;
            (void)snprintf(label, sizeof(label), "%.*s", (int)(bar - s->value), s->value);
            TRY(ttf_buffer_puts(out, label));
            TRY(ttf_buffer_puts(out, resource(e, "unit", "per_unit", "당")));
            TRY(ttf_buffer_puts(out, " "));
            TRY(cardinal(e, bar + 1, out));
            return ttf_buffer_puts(out, " 회");
        }
        if (!strcmp(s->unit, "@countdown")) {
            TRY(ttf_buffer_puts(out, resource(e, "countdown", "D-", "디 마이너스 ")));
            return cardinal(e, s->value, out);
        }
        if (!strcmp(s->unit, "@per-unit")) {
            /* label|number|scale|particle: 킬로그램당 구만 팔천 원 */
            char label[64], number[32];
            const char *bar = strchr(s->value, '|'), *bar2 = bar ? strchr(bar + 1, '|') : NULL,
                       *bar3 = bar2 ? strchr(bar2 + 1, '|') : NULL;
            if (!bar3 || (size_t)(bar - s->value) >= sizeof(label) || (size_t)(bar2 - bar - 1) >= sizeof(number))
                return TTF_EINVAL;
            (void)snprintf(label, sizeof(label), "%.*s", (int)(bar - s->value), s->value);
            (void)snprintf(number, sizeof(number), "%.*s", (int)(bar2 - bar - 1), bar + 1);
            TRY(ttf_buffer_puts(out, label));
            TRY(ttf_buffer_puts(out, resource(e, "unit", "per_unit", "당")));
            TRY(ttf_buffer_puts(out, " "));
            TRY(cardinal(e, number, out));
            TRY(ttf_buffer_append(out, bar2 + 1, (size_t)(bar3 - bar2 - 1)));
            TRY(ttf_buffer_puts(out, " "));
            TRY(ttf_buffer_puts(out, resource(e, "currency", "KRW", "원")));
            return ttf_buffer_puts(out, bar3 + 1);
        }
        if (!strcmp(s->unit, "@mark")) {
            /* mark|number|unit reading, placed by the number_mark template */
            char mark[8], number[32], unit[64];
            const char *bar = strchr(s->value, '|'), *bar2 = bar ? strchr(bar + 1, '|') : NULL,
                       *bar3 = bar2 ? strchr(bar2 + 1, '|') : NULL, *p, *template;
            if (!bar3 || (size_t)(bar - s->value) >= sizeof(mark) ||
                (size_t)(bar2 - bar - 1) >= sizeof(number) || (size_t)(bar3 - bar2 - 1) >= sizeof(unit))
                return TTF_EINVAL;
            (void)snprintf(unit, sizeof(unit), "%.*s", (int)(bar3 - bar2 - 1), bar2 + 1);
            (void)snprintf(mark, sizeof(mark), "%.*s", (int)(bar - s->value), s->value);
            (void)snprintf(number, sizeof(number), "%.*s", (int)(bar2 - bar - 1), bar + 1);
            if (!(template = ttf_resource_get(e, "number_mark", "ko-KR", mark, NULL)))
                return TTF_ERESOURCE;
            TRY(ttf_trace_resource(e, "number_mark", "ko-KR", mark, template, "number-mark-reading",
                                   SIZE_MAX));
            for (p = template; *p;) {
                if (!strncmp(p, "{N}", 3)) {
                    TRY(cardinal(e, number, out));
                    if (unit[0]) {
                        TRY(ttf_buffer_puts(out, " "));
                        TRY(ttf_buffer_puts(out, unit));
                    }
                    p += 3;
                } else {
                    TRY(ttf_buffer_append(out, p, 1));
                    ++p;
                }
            }
            return ttf_buffer_puts(out, bar3 + 1);
        }
        if (!strcmp(s->unit, "@pressure")) {
            /* 150|95 백오십에 구십오; 140|90|mmHg 백사십에 구십 수은주밀리미터 */
            char part[8];
            const char *bar = strchr(s->value, '|'), *bar2;
            if (!bar || (size_t)(bar - s->value) >= sizeof(part))
                return TTF_EINVAL;
            bar2 = strchr(bar + 1, '|');
            (void)snprintf(part, sizeof(part), "%.*s", (int)(bar - s->value), s->value);
            TRY(cardinal(e, part, out));
            TRY(ttf_buffer_puts(out, "에 "));
            (void)snprintf(part, sizeof(part), "%.*s", (int)(bar2 ? (size_t)(bar2 - bar - 1) : strlen(bar + 1)), bar + 1);
            TRY(cardinal(e, part, out));
            return bar2 ? ttf_buffer_puts(out, " 수은주밀리미터") : TTF_OK;
        }
        if (!strcmp(s->unit, "@resolution")) {
            TRY(cardinal(e, s->value, out));
            return ttf_buffer_puts(out, " 피");
        }
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
    /* only the key lengths this kind has (built at freeze, 2026-10-24) */
    uint64_t lengths = ttf_resource_key_lengths(in->engine, kind, "ko-KR");
    for (*width = available < sizeof(key) - 1 ? available : sizeof(key) - 1; *width > 0; --*width) {
        const char *label;
        if (!((lengths >> *width) & 1))
            continue;
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
    int sign_before_prefix = 0, plus = 0;
    ttf_semantic_type_t type = TTF_SEM_CARDINAL;
    decimal_zero(&total);
    decimal_zero(&group);
    /* An adjacent sign may precede a resource-defined currency prefix.
       Keep it attached to the complete amount, including Korean scales. */
    if (p + 1 < n && (text[p] == '-' || text[p] == '+')) {
        prefix = quantity_resource_at(in, "currency_prefix", p + 1, &width);
        if (prefix) {
            sign_before_prefix = 1;
            plus = text[p] == '+';
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
            negative = text[p] == '-', plus = text[p] == '+';
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
    /* +320억 원: the plus the writer put there is read (owner v2, 2026-10-15) */
    if (valid && plus && strlen(s->value) + 1 < sizeof(s->value)) {
        memmove(s->value + 1, s->value, strlen(s->value) + 1);
        s->value[0] = '+';
    }
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

/* 1.25만 일 점 이오만, -4.5조 마이너스 사 점 오조, 3.5억 원 삼 점 오억 원: a decimal before
   one scale word is read as written, as news reading does (project-owner decision
   2026-10-23, open set v3; was 만 이천오백); 3.5만 원 and 1.001만 keep their sum. The value keeps the scale as an exponent
   (1.25e4) for cardinal(); ranges and several groups (1.5억 3천만) keep the exact sum. */
static void written_scale(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    static const char *const words[] = {"만", "억", "조", "경"};
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, a, b, c, k, w = 0;
    char v[TTF_IR_VALUE_SIZE];
    if (s->type != TTF_SEM_CARDINAL && s->type != TTF_SEM_CURRENCY && s->type != TTF_SEM_COUNTER &&
        s->type != TTF_SEM_MEASURE)
        return;
    a = p + (p < n && (t[p] == '-' || t[p] == '+'));
    for (b = a; b < n && t[b] >= '0' && t[b] <= '9'; ++b)
        ;
    if (b == a || b + 1 >= n || t[b] != '.' || !(t[b + 1] >= '0' && t[b + 1] <= '9'))
        return;
    for (c = ++b; b < n && t[b] >= '0' && t[b] <= '9'; ++b)
        ;
    if (b - c > 2)
        return; /* 1.001만 원 만 십 원: an exact amount keeps its sum */
    for (k = 0; k < 4; ++k) {
        w = strlen(words[k]);
        if (b + w <= n && !memcmp(t + b, words[k], w))
            break;
    }
    if (k == 4)
        return;
    for (c = b + w; c < n && t[c] == ' '; ++c)
        ;
    if ((c < n && t[c] >= '0' && t[c] <= '9') || b - p + 4 >= sizeof(v))
        return;
    /* 3.5만 원 삼만 오천 원: an amount of won in 만 is said as its sum */
    if (!k && c + strlen("원") <= n && !memcmp(t + c, "원", strlen("원")))
        return;
    (void)snprintf(v, sizeof(v), "%.*se%u", (int)(b - p), t + p, (unsigned)(4 * (k + 1)));
    (void)snprintf(s->value, sizeof(s->value), "%s", v);
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

/* 아침 -3도라는데 영하 삼도라는데: a negative number before 도 is a temperature
   (owner, 2026-10-14). After a written 영하 the sign is not read again. */
static size_t below_zero_degrees(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, begin = in->tokens[in->index].begin, p = begin + 1, i;
    if (t[begin] != '-' || p >= n || t[p] < '0' || t[p] > '9' ||
        (begin && ((t[begin - 1] >= '0' && t[begin - 1] <= '9') ||
                   ((t[begin - 1] | 0x20) >= 'a' && (t[begin - 1] | 0x20) <= 'z'))))
        return 0;
    while (p < n && t[p] >= '0' && t[p] <= '9')
        ++p;
    if (p + 1 < n && t[p] == '.' && t[p + 1] >= '0' && t[p + 1] <= '9')
        for (++p; p < n && t[p] >= '0' && t[p] <= '9'; ++p)
            ;
    if (p + 3 > n || memcmp(t + p, "도", 3) || p - begin - 1 >= sizeof(s->value))
        return 0;
    /* 각도는 -30도: an angle */
    for (i = begin > 24 ? begin - 24 : 0; i + 6 <= begin; ++i)
        if (!memcmp(t + i, "각도", 6) || !memcmp(t + i, "기울", 6) || !memcmp(t + i, "회전", 6))
            return 0;
    memcpy(s->value, t + begin + 1, p - begin - 1);
    s->value[p - begin - 1] = '\0';
    i = begin;
    while (i && t[i - 1] == ' ')
        --i;
    s->secondary[0] = i >= 6 && !memcmp(t + i - 6, "영하", 6) ? '\0' : '1';
    s->secondary[1] = '\0';
    s->type = TTF_SEM_CARDINAL;
    s->input_end = p;
    s->layer = TTF_LAYER_COMMON;
    (void)snprintf(s->rule, sizeof(s->rule), "ko:below-zero");
    for (i = in->index; i < in->token_count && in->tokens[i].begin < p; ++i)
        ;
    return i - in->index;
}

static int two_digits_at(const char *t, size_t n, size_t p, unsigned *v, size_t *end) {
    size_t q = p;
    *v = 0;
    while (q < n && q - p < 3 && t[q] >= '0' && t[q] <= '9')
        *v = *v * 10 + (unsigned)(t[q++] - '0');
    *end = q;
    return q > p && q - p <= 2;
}

/* '25.11.3, 26.12.24: a two-digit year, month and day joined by dots is a date
   (owner sets, 2026-10-14); the apostrophe of a shortened year is part of it. */
static int word_within(const char *t, size_t p, size_t window, const char *words);
static size_t short_dotted_date(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, begin = in->tokens[in->index].begin, p = begin, q, i;
    unsigned year, month, day;
    static const unsigned days[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (p < n && t[p] == '\'')
        ++p;
    if (p >= n || t[p] < '0' || t[p] > '9' ||
        (begin && ((t[begin - 1] >= '0' && t[begin - 1] <= '9') || t[begin - 1] == '.')))
        return 0;
    /* 카카오톡 버전 10.8.5 십 점 팔 점 오: after a version word it is a version, not a date
       (2026-10-30, TN round 10; was 십 년 팔 월 오 일) */
    if (word_within(t, begin, 12, "버전|ver|Ver|VER"))
        return 0;
    /* iOS 17.4.1 십칠 점 사 점 일: after a Latin product name, too (2026-10-31, TN round 12) */
    if (begin >= 2 && t[begin - 1] == ' ' && ((t[begin - 2] | 0x20) >= 'a' && (t[begin - 2] | 0x20) <= 'z'))
        return 0;
    if (!two_digits_at(t, n, p, &year, &q) || q - p != 2 || q >= n || t[q] != '.' ||
        !two_digits_at(t, n, q + 1, &month, &q) || q >= n || t[q] != '.' ||
        !two_digits_at(t, n, q + 1, &day, &q))
        return 0;
    if (q < n && t[q] == '.' && !(q + 1 < n && t[q + 1] >= '0' && t[q + 1] <= '9'))
        ++q;
    if ((q < n && ((t[q] >= '0' && t[q] <= '9') || t[q] == '.' || t[q] == '%')) || !year || !month ||
        month > 12 || !day || day > days[month - 1])
        return 0;
    (void)snprintf(s->value, sizeof(s->value), "%u-%u-%u", year, month, day);
    s->unit[0] = '\0';
    s->type = TTF_SEM_DATE;
    s->input_end = q;
    s->layer = TTF_LAYER_COMMON;
    (void)snprintf(s->rule, sizeof(s->rule), "ko:short-dotted-date");
    for (i = in->index; i < in->token_count && in->tokens[i].begin < q; ++i)
        ;
    return i - in->index;
}

static size_t ko_finish(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t end, ttf_semantic_type_t type,
                        const char *value, const char *unit, const char *rule) {
    size_t i;
    (void)snprintf(s->value, sizeof(s->value), "%s", value);
    (void)snprintf(s->unit, sizeof(s->unit), "%s", unit);
    (void)snprintf(s->rule, sizeof(s->rule), "%s", rule);
    s->type = type;
    s->input_end = end;
    s->layer = TTF_LAYER_COMMON;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < end; ++i)
        ;
    return i - in->index;
}
static int ko_digit(char c) {
    return c >= '0' && c <= '9';
}
/* The text at p starts with one of the |-separated words. */
static int starts_with_word(const char *t, size_t n, size_t p, const char *words) {
    const char *w = words;
    while (*w) {
        size_t m = strcspn(w, "|");
        if (p + m <= n && !memcmp(t + p, w, m))
            return 1;
        w += m + (w[m] == '|');
    }
    return 0;
}
/* One of the |-separated words ends within the window before p. */
static int word_within(const char *t, size_t p, size_t window, const char *words) {
    const char *w = words;
    size_t from = p > window ? p - window : 0;
    while (*w) {
        size_t m = strcspn(w, "|"), k;
        for (k = from; k + m <= p; ++k)
            if (!memcmp(t + k, w, m))
                return 1;
        w += m + (w[m] == '|');
    }
    return 0;
}
/* One of the |-separated words starts within `window` bytes after `end`. */
static int word_after(const char *t, size_t n, size_t end, size_t window, const char *words) {
    const char *w = words;
    size_t to = end + window < n ? end + window : n;
    while (*w) {
        size_t m = strcspn(w, "|"), k;
        for (k = end; k + m <= to; ++k)
            if (!memcmp(t + k, w, m))
                return 1;
        w += m + (w[m] == '|');
    }
    return 0;
}
/* Words around M:SS that make it a length of time, not a clock (v3 dev, 2026-10-26):
   영상 1:30 분량 일 분 삼십 초, 재생 길이는 3:45, 4:25의 기록, 스톱워치로 3:30를. */
#define KO_DURATION_WORDS "길이|분량|재생|영상|노래|음원|랩타임|스톱워치|타이머|조리|전자레인지|구간|기록|러닝타임|플레이타임"
/* Owner evaluation sets, phase 4 (2026-10-14; 24:00 stays as written, golden and
   typed-control tests):
   1:30 남았습니다 일 분 삼십 초, 3:05짜리 삼 분 오 초, 기록 2:58:30 두 시간
   오십팔 분 삼십 초; 6/6 현충일 유월 육 일; 1:25,000 일 대 이만 오천. */
/* A Korean request, or Hangul in the text: the forms below are Korean readings, not for an
   English request (24:00 and 12:30:60 stay as written there). */
static int korean_context(const ttf_pack_input_t *in) {
    size_t i;
    if (in->engine && in->engine->default_language && !strcmp(in->engine->default_language, "ko-KR"))
        return 1;
    for (i = 0; i + 2 < in->length; ++i) {
        unsigned char c = (unsigned char)in->text[i];
        if (c >= 0xEA && c <= 0xED)
            return 1;
    }
    return 0;
}
static size_t owner_forms(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q, r, z;
    char v[64];
    if (p >= n || !ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ':')))
        return 0;
    for (q = p; q < n && ko_digit(t[q]); ++q)
        ;
    if (q - p > 2 || q + 2 >= n)
        return 0;
    /* 24:00 이십사 시, 24:00:00: the end of the day (v3, 2026-10-23); the shared clock stops at 23 */
    if (q - p == 2 && q + 3 <= n && !memcmp(t + p, "24:00", 5) && !(q + 3 < n && ko_digit(t[q + 3])) &&
        korean_context(in)) {
        r = q + 3;
        if (r + 3 <= n && !memcmp(t + r, ":00", 3) && !(r + 3 < n && ko_digit(t[r + 3])))
            r += 3;
        if (!(r < n && t[r] == ':')) {
            s->secondary[0] = '\0';
            return ko_finish(in, s, r, TTF_SEM_TIME, "24:00", "time", "ko:clock-24");
        }
    }
    /* 3:58.12로 신기록 삼 분 오십팔 초 십이: minutes, seconds and hundredths of a record
       (2026-10-31, TN round 12; was left as written) */
    if (t[q] == ':' && q + 5 < n && ko_digit(t[q + 1]) && ko_digit(t[q + 2]) && t[q + 3] == '.' &&
        ko_digit(t[q + 4]) && atoi(t + q + 1) < 60) {
        size_t f = q + 4;
        while (f < n && ko_digit(t[f]) && f - q - 4 < 3)
            ++f;
        if (!(f < n && (ko_digit(t[f]) || t[f] == '.' || t[f] == ':')) && korean_context(in)) {
            (void)snprintf(v, sizeof(v), "%.*s", (int)(f - p), t + p);
            return ko_finish(in, s, f, TTF_SEM_TIME, v, "minutes-fraction", "ko:duration");
        }
    }
    /* M:SS and H:MM:SS durations */
    if (t[q] == ':' && ko_digit(t[q + 1]) && q + 2 < n && ko_digit(t[q + 2]) && !(q + 3 < n && ko_digit(t[q + 3]))) {
        r = q + 3;
        if (r + 2 < n && t[r] == ':' && ko_digit(t[r + 1]) && ko_digit(t[r + 2]) && !(r + 3 < n && ko_digit(t[r + 3]))) {
            /* 2:58:30의 기록으로, 분량이 1:15:40에, 인코딩 2:58:30 만에: before or after */
            /* 1:02:15에 결승선을 밟았다 한 시간 이 분 십오 초 (2026-10-31, TN round 20; was 한 시) */
            if (word_within(t, p, 30, "기록|완주|소요|걸렸|분량|길이|재생|녹음|영상|마라톤|레이스") ||
                word_after(t, n, r + 3, 30, "기록|완주|소요|걸렸|만에|분량|결승선|골인|주파")) {
                (void)snprintf(v, sizeof(v), "%.*s", (int)(r + 3 - p), t + p);
                return ko_finish(in, s, r + 3, TTF_SEM_TIME, v, "duration", "ko:duration");
            }
        } else if (starts_with_word(t, n, r, "짜리| 남았| 남은| 남겨| 동안")) {
            (void)snprintf(v, sizeof(v), "%.*s", (int)(r - p), t + p);
            return ko_finish(in, s, r, TTF_SEM_TIME, v, "minutes", "ko:duration");
        } else if (!starts_with_word(t, n, r, "에|부터|까지|쯤|경") &&
                   !word_within(t, p, 12, "오전|오후|새벽|아침|저녁|밤|낮") &&
                   !word_within(t, p, 30, "비율|축척|배합|시각|시점|일시") && korean_context(in)) {
            /* a clock takes 에, 부터 and 까지 and follows 오전 or 오후 or 시각 (복구 완료 시각은
               16:05로 기록되었고 열여섯 시 오 분); a length sits by a length word, or by 소요 for
               hours and minutes (비행 2:10 정도 소요 두 시간 십 분) */
            (void)snprintf(v, sizeof(v), "%.*s", (int)(r - p), t + p);
            if (word_within(t, p, 30, KO_DURATION_WORDS) || word_after(t, n, r, 30, KO_DURATION_WORDS))
                return ko_finish(in, s, r, TTF_SEM_TIME, v, "minutes", "ko:duration");
            if (atoi(t + q + 1) < 60 && (word_within(t, p, 30, "비행시간|비행 시간") || word_after(t, n, r, 24, "소요")))
                return ko_finish(in, s, r, TTF_SEM_TIME, v, "hours", "ko:duration");
        }
    }
    /* 오는 11. 18. 십일 월 십팔 일, 매월 09.15 구월 십오 일: a month and day without a year, the
       spaced form ending in a period or a month with a leading zero, after a date word (v3 dev,
       2026-10-26). A full date (2026. 11. 18.) is read from its year. */
    if (t[q] == '.' && korean_context(in) && !(p >= 2 && t[p - 1] == ' ' && t[p - 2] == '.') &&
        word_within(t, p, 24, "오는|지난|다음|이번|요일인|날짜|일정|기한|마감|매년|매월|기준일|결제일")) {
        unsigned month = (unsigned)atoi(t + p), day = 0;
        size_t d = q + 1, e2 = 0;
        int spaced = d < n && t[d] == ' ';
        if (spaced)
            ++d;
        for (r = d; r < n && ko_digit(t[r]); ++r)
            ;
        if (r > d && r - d <= 2)
            day = (unsigned)atoi(t + d);
        if (spaced && r < n && t[r] == '.' && !(r + 1 < n && (ko_digit(t[r + 1]) || t[r + 1] == '.')))
            e2 = r + 1;
        else if (!spaced && t[p] == '0' && q - p == 2 && r - d == 2 &&
                 !(r < n && (ko_digit(t[r]) || t[r] == '.' || t[r] == '%' || t[r] == ':')))
            e2 = r;
        if (e2 && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
            (void)snprintf(v, sizeof(v), "--%02u-%02u", month, day);
            return ko_finish(in, s, e2, TTF_SEM_DATE, v, "month-day", "ko:yearless-date");
        }
    }
    /* 6/6 현충일, 3/1은 삼일절, 12.25 크리스마스: a month and day before a holiday */
    if ((t[q] == '/' || t[q] == '.') && ko_digit(t[q + 1])) {
        unsigned month = (unsigned)atoi(t + p), day;
        for (r = q + 1; r < n && ko_digit(t[r]); ++r)
            ;
        day = (unsigned)atoi(t + q + 1);
        z = r;
        while (z < n && (t[z] != ' ' && (unsigned char)t[z] >= 0x80))
            ++z; /* 3/1은: a particle */
        while (z < n && t[z] == ' ')
            ++z;
        if (r - q - 1 <= 2 && month >= 1 && month <= 12 && day >= 1 && day <= 31 && !(r < n && (ko_digit(t[r]) || t[r] == '.' || t[r] == '/')) &&
            starts_with_word(t, n, z, "현충일|삼일절|크리스마스|성탄절|광복절|개천절|한글날|어린이날|어버이날|제헌절|추석|설날|발렌타인|화이트데이|빼빼로") &&
            /* 8.15 광복절 팔일오: a dotted national day keeps its event name */
            !(t[q] == '.' && starts_with_word(t, n, z, "현충일|삼일절|광복절|개천절|제헌절"))) {
            (void)snprintf(v, sizeof(v), "--%02u-%02u", month, day);
            return ko_finish(in, s, r, TTF_SEM_DATE, v, "month-day", "ko:holiday-date");
        }
    }
    /* 1:25,000 일 대 이만 오천, 1:1,000으로 희석: a ratio with a grouped second number */
    if (t[q] == ':' && ko_digit(t[q + 1])) {
        size_t used = 0;
        int grouped = 0;
        /* 2:2:1 이 대 이 대 일, 1:2:4로: three terms or more that are not a clock (whose minutes
           and seconds have two digits under 60) (v3, 2026-10-23) */
        {
            size_t a = q, b, terms = 1;
            int clock = 1;
            while (a + 1 < n && t[a] == ':' && ko_digit(t[a + 1])) {
                for (b = a + 1; b < n && ko_digit(t[b]); ++b)
                    ;
                if (b - a - 1 > 3)
                    break;
                if (b - a - 1 != 2 || atoi(t + a + 1) > 59)
                    clock = 0;
                ++terms;
                a = b;
            }
            if (terms >= 3 && !clock && !(a < n && (ko_digit(t[a]) || t[a] == '.' || t[a] == ':')) &&
                a - p < sizeof(v) && korean_context(in)) {
                (void)snprintf(v, sizeof(v), "%.*s", (int)(a - p), t + p);
                return ko_finish(in, s, a, TTF_SEM_CARDINAL, v, "", "ko:ratio");
            }
        }
        for (r = q + 1; r < n && (ko_digit(t[r]) || (t[r] == ',' && r + 3 < n && ko_digit(t[r + 1]) && ko_digit(t[r + 2]) && ko_digit(t[r + 3]))); ++r)
            grouped |= t[r] == ',';
        if (grouped && !(r < n && ko_digit(t[r])) && r - p < sizeof(v)) {
            for (z = p; z < r; ++z)
                if (t[z] != ',')
                    v[used++] = t[z];
            v[used] = '\0';
            return ko_finish(in, s, r, TTF_SEM_CARDINAL, v, "", "ko:ratio");
        }
        /* 축척 비율은 실물의 1:25 크기 일 대 이십오: a ratio word before a clock-like pair (v3 dev,
           2026-10-26) */
        if (!grouped && r - q - 1 <= 3 && !(r < n && (ko_digit(t[r]) || t[r] == '.' || t[r] == ':')) &&
            word_within(t, p, 30, "비율|축척|배합") && r - p < sizeof(v) && korean_context(in)) {
            (void)snprintf(v, sizeof(v), "%.*s", (int)(r - p), t + p);
            return ko_finish(in, s, r, TTF_SEM_CARDINAL, v, "", "ko:ratio");
        }
        /* 물 1:1.2로 일 대 일 점 이: a one-digit second number with a decimal part (owner v2,
           2026-10-15); 1:30.5 stays a time */
        if (!grouped && q - p <= 3 && r == q + 2 && r + 1 < n && t[r] == '.' && ko_digit(t[r + 1])) {
            for (z = r + 1; z < n && ko_digit(t[z]); ++z)
                ;
            if (z - r - 1 <= 2 && !(z < n && (ko_digit(t[z]) || t[z] == '.' || t[z] == ':')) && z - p < sizeof(v)) {
                (void)snprintf(v, sizeof(v), "%.*s", (int)(z - p), t + p);
                return ko_finish(in, s, z, TTF_SEM_CARDINAL, v, "", "ko:ratio");
            }
        }
    }
    return 0;
}

/* Owner evaluation sets, phase 5 (2026-10-14): 10/13(월) 시월 십삼 일 월요일,
   0427번 공사이칠번, 1080p 천팔십 피. */
static size_t owner_forms5(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q;
    char v[32];
    /* (월) after a date: a weekday mark. 2025. 1. 31.(금) and 7일 (월) too: a date's closing dot
       and one space before the mark (round 21). */
    if (p >= 1 && p + 5 <= n && t[p] == '(' && t[p + 4] == ')') {
        static const char *const days[] = {"월", "화", "수", "목", "금", "토", "일"};
        size_t k, m = p;
        if (m >= 5 && t[m - 1] == ' ' && (t[m - 2] == '.' || (!memcmp(t + m - 4, "일", 3) && ko_digit(t[m - 5]))))
            --m;
        if (m >= 2 && t[m - 1] == '.' && ko_digit(t[m - 2]))
            --m;
        for (k = 0; k < 7 && (ko_digit(t[m - 1]) || (m >= 3 && !memcmp(t + m - 3, "일", 3))); ++k)
            if (!memcmp(t + p + 1, days[k], 3)) {
                size_t z = m - (ko_digit(t[m - 1]) ? 1 : 3);
                /* a date before it: 10/13, 5/3, 21일, 2025. 1. 31. */
                while (z && (ko_digit(t[z - 1]) || t[z - 1] == '/' || t[z - 1] == '.'))
                    --z;
                if (!memchr(t + z, '/', p - z) && !memchr(t + z, '.', p - z) && memcmp(t + m - 3, "일", 3))
                    break;
                (void)snprintf(v, sizeof(v), t[p - 1] == ' ' ? "%s요일" : " %s요일", days[k]);
                return ko_finish(in, s, p + 5, TTF_SEM_SYMBOL, v, "@word", "ko:weekday-mark");
            }
    }
    /* 12.14(월) 십이 월 십사 일 월요일: a dotted month and day before a weekday mark (owner v2,
       2026-10-15); 12/14(월) is already a date */
    if (p + 8 <= n && ko_digit(t[p]) && !(p && (ko_digit(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ','))) {
        size_t a = p, b;
        while (a < n && ko_digit(t[a]) && a - p < 2)
            ++a;
        if (a < n && t[a] == '.' && a + 1 < n && ko_digit(t[a + 1])) {
            for (b = a + 1; b < n && ko_digit(t[b]) && b - a - 1 < 2; ++b)
                ;
            if (!(b < n && ko_digit(t[b])) && b + 5 <= n && t[b] == '(' && t[b + 4] == ')') {
                static const char *const wd[] = {"월", "화", "수", "목", "금", "토", "일"};
                int month = atoi(t + p), day = atoi(t + a + 1), k;
                for (k = 0; k < 7; ++k)
                    if (!memcmp(t + b + 1, wd[k], 3) && month >= 1 && month <= 12 && day >= 1 && day <= 31) {
                        (void)snprintf(v, sizeof(v), "--%02d-%02d", month, day);
                        return ko_finish(in, s, b, TTF_SEM_DATE, v, "month-day", "ko:dot-date-weekday");
                    }
            }
        }
    }
    if (p >= n || !ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ',')))
        return 0;
    for (q = p; q < n && ko_digit(t[q]); ++q)
        ;
    /* 010.3304.6671 공일공 삼삼공사 육육칠일: a mobile number written with dots */
    if (t[p] == '0' && q - p == 3 && q + 10 <= n && t[q] == '.') {
        size_t a = q + 1, b2;
        for (b2 = a; b2 < n && ko_digit(t[b2]); ++b2)
            ;
        if ((b2 - a == 3 || b2 - a == 4) && b2 + 5 <= n && t[b2] == '.' && ko_digit(t[b2 + 1])) {
            size_t c = b2 + 1, d;
            for (d = c; d < n && ko_digit(t[d]); ++d)
                ;
            if (d - c == 4 && !(d < n && (ko_digit(t[d]) || (t[d] == '.' && d + 1 < n && ko_digit(t[d + 1]))))) {
                (void)snprintf(v, sizeof(v), "%.3s-%.*s-%.4s", t + p, (int)(b2 - a), t + a, t + c);
                return ko_finish(in, s, d, TTF_SEM_PHONE, v, "", "ko:dotted-phone");
            }
        }
    }
    /* 운송장번호 6845 2201 3379: spaced digit groups after a number label read digit by digit */
    if (q - p >= 3 && q - p <= 4 && q + 4 < n && t[q] == ' ' && ko_digit(t[q + 1]) && word_within(t, p, 18, "번호") &&
        in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
        size_t z = q, groups = 1, used = (size_t)(q - p);
        memcpy(v, t + p, q - p);
        while (z + 1 < n && t[z] == ' ' && ko_digit(t[z + 1])) {
            size_t g = z + 1;
            while (g < n && ko_digit(t[g]))
                ++g;
            if (g - z - 1 < 3 || g - z - 1 > 4 || used + (g - z) + 1 >= sizeof(v))
                break;
            v[used++] = ' ';
            memcpy(v + used, t + z + 1, g - z - 1);
            used += g - z - 1;
            z = g;
            ++groups;
        }
        v[used] = 0;
        if (groups >= 3 && !(z < n && ko_digit(t[z])))
            return ko_finish(in, s, z, TTF_SEM_DIGIT_SEQUENCE, v, "spaced_groups", "ko:spaced-groups");
    }
    /* 9/28~10/1 구 월 이십팔 일에서 시월 일 일: both ends of a slash date range are dates */
    if (q - p <= 2 && q + 1 < n && t[q] == '/' && ko_digit(t[q + 1])) {
        size_t r, z;
        unsigned month = (unsigned)atoi(t + p), day = (unsigned)atoi(t + q + 1);
        for (r = q + 1; r < n && ko_digit(t[r]); ++r)
            ;
        if (r - q - 1 <= 2 && month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
            !(r < n && (ko_digit(t[r]) || t[r] == '/' || t[r] == '.'))) {
            int range = 0;
            size_t other = 0, tl = r < n && t[r] == ' ' ? r + 1 : r;
            /* the first end: ~M/D follows (11/7 ~ 11/9 with spaces too, owner v2 2026-10-15) */
            if (tl + 3 < n && t[tl] == '~') {
                size_t a2 = t[tl + 1] == ' ' ? tl + 2 : tl + 1;
                if (a2 < n && ko_digit(t[a2])) {
                    unsigned m2 = (unsigned)atoi(t + a2);
                    for (z = a2; z < n && ko_digit(t[z]); ++z)
                        ;
                    range = z - a2 <= 2 && z + 1 < n && t[z] == '/' && ko_digit(t[z + 1]) && m2 >= 1 && m2 <= 12;
                    other = a2;
                }
            }
            /* the second end: M/D~ comes before */
            if (!range) {
                size_t pe = p >= 1 && t[p - 1] == ' ' ? p - 1 : p, pt;
                if (pe >= 4 && t[pe - 1] == '~') {
                    pt = t[pe - 2] == ' ' ? pe - 2 : pe - 1;
                    if (pt >= 3 && ko_digit(t[pt - 1])) {
                        for (z = pt - 1; z && ko_digit(t[z - 1]); --z)
                            ;
                        range = z >= 2 && t[z - 1] == '/' && ko_digit(t[z - 2]);
                        for (other = z - 2; other && ko_digit(t[other - 1]); --other)
                            ;
                    }
                }
            }
            /* 1/3~1/2, 1/5~1/6: fractions with 1 on top at both ends stay fractions (Korean references) */
            if (range && month == 1 && t[other] == '1' && other + 1 < n && t[other + 1] == '/')
                range = 0;
            if (range) {
                size_t c;
                (void)snprintf(v, sizeof(v), "--%02u-%02u", month, day);
                /* 11/7 ~ 11/9 십일 월 칠 일에서 십일 월 구 일: the spaced mark is read with the
                   first end (owner v2, 2026-10-15) */
                if (other && other > r && tl > r) {
                    c = ko_finish(in, s, other, TTF_SEM_DATE, v, "month-day", "ko:date-range");
                    if (c)
                        (void)snprintf(s->secondary, sizeof(s->secondary), "range-from");
                    return c;
                }
                return ko_finish(in, s, r, TTF_SEM_DATE, v, "month-day", "ko:date-range");
            }
        }
    }
    /* 혈압 150/95 백오십에 구십오, 140/90mmHg: a blood pressure reading */
    if (q - p >= 2 && q - p <= 3 && q + 3 < n && t[q] == '/' && ko_digit(t[q + 1]) && word_within(t, p, 15, "혈압")) {
        size_t r;
        for (r = q + 1; r < n && ko_digit(t[r]); ++r)
            ;
        if (r - q - 1 >= 2 && r - q - 1 <= 3 && !(r < n && (ko_digit(t[r]) || t[r] == '/' || t[r] == '.'))) {
            int mmhg = r + 4 <= n && !memcmp(t + r, "mmHg", 4);
            (void)snprintf(v, sizeof(v), "%.*s|%.*s%s", (int)(q - p), t + p, (int)(r - q - 1), t + q + 1, mmhg ? "|mmHg" : "");
            return ko_finish(in, s, mmhg ? r + 4 : r, TTF_SEM_SYMBOL, v, "@pressure", "ko:blood-pressure");
        }
    }
    /* 0427번: a number with a leading zero before 번 reads digit by digit (01번째 stays an ordinal,
       0번지 an address) */
    if (t[p] == '0' && q - p >= 2 && q - p <= 8 && q + 3 <= n && !memcmp(t + q, "번", 3) &&
        !(q + 6 <= n && (!memcmp(t + q + 3, "째", 3) || !memcmp(t + q + 3, "지", 3)))) {
        (void)snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
        return ko_finish(in, s, q + 3, TTF_SEM_DIGIT_SEQUENCE, v, "zero_number", "ko:zero-number");
    }
    /* 1080p 천팔십 피: a video resolution */
    if (q < n && (t[q] == 'p' || t[q] == 'P') && !(q + 1 < n && ((t[q + 1] >= 'a' && t[q + 1] <= 'z') ||
                                                              (t[q + 1] >= 'A' && t[q + 1] <= 'Z') || ko_digit(t[q + 1])))) {
        static const char *const sizes[] = {"360", "480", "720", "1080", "1440", "2160", "4320"};
        size_t k;
        for (k = 0; k < sizeof(sizes) / sizeof(*sizes); ++k)
            if (strlen(sizes[k]) == q - p && !memcmp(t + p, sizes[k], q - p)) {
                (void)snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
                return ko_finish(in, s, q + 1, TTF_SEM_SYMBOL, v, "@resolution", "ko:resolution");
            }
    }
    return 0;
}

/* 1/4분기 일 사분기: a quarter written as a fraction (owner sets, 2026-10-14). */
static size_t quarter(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, begin = in->tokens[in->index].begin, i;
    if (begin + 3 + 6 > n || t[begin] < '1' || t[begin] > '4' || t[begin + 1] != '/' || t[begin + 2] != '4' ||
        memcmp(t + begin + 3, "분기", 6) || (begin && t[begin - 1] >= '0' && t[begin - 1] <= '9'))
        return 0;
    s->value[0] = t[begin];
    s->value[1] = '\0';
    s->type = TTF_SEM_CARDINAL;
    s->input_end = begin + 3;
    s->layer = TTF_LAYER_COMMON;
    (void)snprintf(s->rule, sizeof(s->rule), "ko:quarter");
    for (i = in->index; i < in->token_count && in->tokens[i].begin < begin + 3; ++i)
        ;
    return i - in->index;
}

/* A superscript digit or minus at t: its ASCII character and byte width, else 0. */
static size_t superscript_at(const char *t, size_t n, size_t q, char *c) {
    const unsigned char *u = (const unsigned char *)t + q;
    if (q + 2 <= n && u[0] == 0xC2 && (u[1] == 0xB9 || u[1] == 0xB2 || u[1] == 0xB3)) {
        *c = u[1] == 0xB9 ? '1' : u[1] == 0xB2 ? '2' : '3';
        return 2;
    }
    if (q + 3 <= n && u[0] == 0xE2 && u[1] == 0x81 && (u[2] == 0xB0 || (u[2] >= 0xB4 && u[2] <= 0xB9) || u[2] == 0xBB)) {
        *c = u[2] == 0xB0 ? '0' : u[2] == 0xBB ? '-' : (char)('4' + u[2] - 0xB4);
        return 3;
    }
    return 0;
}
/* 3²=9 삼의 제곱은 구, a²+b²=c², 2¹⁰=1024, 10³: numbers and single letters with superscript
   powers, joined by operators, read as an expression (owner request 2026-10-23). A unit (5m²,
   km²) and a letter alone (x²) are not this. */
static size_t superscript_expression(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    static const char *const ops[] = {"+", "-", "×", "÷", "*", "/", "=", "<", ">", "≤", "≥", "≠", "≈"};
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q = p, end = p, used = 0, operands = 0, k, w;
    int powers = 0, operators = 0, number_base = 0;
    char v[TTF_IR_VALUE_SIZE], c;
    if (p >= n || (p && ((t[p - 1] >= 'a' && t[p - 1] <= 'z') || (t[p - 1] >= 'A' && t[p - 1] <= 'Z') ||
                          ko_digit(t[p - 1]) || t[p - 1] == '.')))
        return 0;
    for (;;) {
        size_t a = q;
        if (q < n && ko_digit(t[q])) {
            while (q < n && ko_digit(t[q]))
                ++q;
            if (q + 1 < n && t[q] == '.' && ko_digit(t[q + 1]))
                for (++q; q < n && ko_digit(t[q]); ++q)
                    ;
            number_base |= operands == 0;
        } else if (q < n && ((t[q] >= 'a' && t[q] <= 'z') || (t[q] >= 'A' && t[q] <= 'Z')) &&
                   !(q + 1 < n && ((t[q + 1] >= 'a' && t[q + 1] <= 'z') || (t[q + 1] >= 'A' && t[q + 1] <= 'Z') ||
                                   ko_digit(t[q + 1]))))
            ++q;
        else
            return 0;
        if (used + (q - a) + 2 >= sizeof(v))
            return 0;
        if (used)
            v[used++] = ' ';
        memcpy(v + used, t + a, q - a);
        used += q - a;
        ++operands;
        if (superscript_at(t, n, q, &c)) {
            if (used + 2 >= sizeof(v))
                return 0;
            v[used++] = ' ';
            v[used++] = '^';
            while ((w = superscript_at(t, n, q, &c)) != 0 && used + 1 < sizeof(v)) {
                v[used++] = c;
                q += w;
            }
            ++powers;
        }
        end = q;
        while (q < n && t[q] == ' ')
            ++q;
        for (k = 0; k < sizeof(ops) / sizeof(*ops); ++k)
            if (q + strlen(ops[k]) <= n && !memcmp(t + q, ops[k], strlen(ops[k])))
                break;
        if (k == sizeof(ops) / sizeof(*ops))
            break;
        w = strlen(ops[k]);
        {
            size_t r = q + w;
            while (r < n && t[r] == ' ')
                ++r;
            if (!(r < n && (ko_digit(t[r]) || (t[r] >= 'a' && t[r] <= 'z') || (t[r] >= 'A' && t[r] <= 'Z'))))
                break;
            if (used + w + 2 >= sizeof(v))
                return 0;
            v[used++] = ' ';
            memcpy(v + used, ops[k], w);
            used += w;
            ++operators;
            q = r;
        }
    }
    if (!powers || (!operators && !number_base) || !korean_context(in) ||
        (end < n && ((t[end] >= 'a' && t[end] <= 'z') || (t[end] >= 'A' && t[end] <= 'Z') || ko_digit(t[end]) ||
                     (t[end] == '.' && end + 1 < n && ko_digit(t[end + 1])))))
        return 0;
    v[used] = '\0';
    return ko_finish(in, s, end, TTF_SEM_SYMBOL, v, "@expression", "ko:superscript-expression");
}

/* D-30 디 마이너스 삼십, D-7 디 마이너스 칠: a countdown to a day (2026-10-29; was 디삼십).
   The capital D opens the word and the number ends it (a letter or digit after it, or a
   letter before the D, makes it a code such as AD-30A). */
static size_t day_countdown(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q = p + 2, used = 0;
    char v[8];
    if (t[p] != 'D' || p + 2 >= n || t[p + 1] != '-' || !ko_digit(t[p + 2]) ||
        (p && (ko_digit(t[p - 1]) || ((t[p - 1] | 0x20) >= 'a' && (t[p - 1] | 0x20) <= 'z') || t[p - 1] == '-')))
        return 0;
    while (q < n && ko_digit(t[q]) && used + 1 < 4)
        v[used++] = t[q++];
    v[used] = 0;
    if ((q < n && (ko_digit(t[q]) || ((t[q] | 0x20) >= 'a' && (t[q] | 0x20) <= 'z') || t[q] == '-' || t[q] == '.')) ||
        !korean_context(in))
        return 0;
    return ko_finish(in, s, q, TTF_SEM_SYMBOL, v, "@countdown", "ko:day-countdown");
}

/* 공고 제2024-123호 제이천이십사 다시 백이십삼 호: an official document number, a year and a
   serial joined by a hyphen and closed by 호, after 제 or a document word (round 21; was read
   digit by digit). Spaces around the hyphen and – or － are taken too. */
static size_t official_number(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    static const char *const words[] = {"공고", "고시", "훈령", "예규", "지침", "예고", "조례", "규칙"};
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, start = p, q, a, b, k;
    int cue = 0, year;
    char v[32];
    if (p + 4 <= n && !memcmp(t + p, "제", 3)) /* the token may begin at 제 */
        p += 3;
    if (p + 4 > n || !ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || t[p - 1] == '-' || t[p - 1] == '.')))
        return 0;
    for (q = p; q < n && ko_digit(t[q]); ++q)
        ;
    year = atoi(t + p);
    if (q - p != 4 || year < 1900 || year > 2099)
        return 0;
    if (p >= 3 && !memcmp(t + p - 3, "제", 3))
        cue = 1;
    for (k = 0; !cue && k < sizeof(words) / sizeof(*words); ++k) {
        size_t w = strlen(words[k]);
        cue = p >= w + 1 && t[p - 1] == ' ' && !memcmp(t + p - 1 - w, words[k], w);
    }
    if (!cue)
        return 0;
    for (a = q; a < n && t[a] == ' '; ++a)
        ;
    if (a < n && t[a] == '-')
        a += 1;
    else if (a + 3 <= n && (!memcmp(t + a, "\xe2\x80\x93", 3) || !memcmp(t + a, "\xef\xbc\x8d", 3)))
        a += 3;
    else
        return 0;
    while (a < n && t[a] == ' ')
        ++a;
    for (b = a; b < n && ko_digit(t[b]) && b - a < 6; ++b)
        ;
    if (b == a || (b < n && ko_digit(t[b])) || b + 3 > n || memcmp(t + b, "호", 3) || !korean_context(in))
        return 0;
    (void)snprintf(v, sizeof(v), "%s%.4s|%.*s", start < p ? "제" : "", t + p, (int)(b - a), t + a);
    return ko_finish(in, s, b + 3, TTF_SEM_SYMBOL, v, "@official-number", "ko:official-number");
}

/* 72회/분 분당 칠십이 회: a count per hour, minute or second reads the time unit first with 당
   (round 21; was 칠십이회 슬래시 분). The unit must close the word or take a particle, so
   3회/분기 is not taken. */
static size_t count_per_time(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    static const char *const units[] = {"시간", "분", "초"};
    static const char *const particles[] = {"로", "으로", "이", "가", "을", "를", "은", "는", "의", "에", "와", "도", "만", "까지"};
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q, k, j;
    char v[32];
    if (p >= n || !ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ',')))
        return 0;
    for (q = p; q < n && ko_digit(t[q]) && q - p < 6; ++q)
        ;
    if (q + 4 > n || memcmp(t + q, "회", 3) || t[q + 3] != '/')
        return 0;
    for (k = 0; k < sizeof(units) / sizeof(*units); ++k) {
        size_t w = strlen(units[k]), e = q + 4 + w;
        int ok;
        if (e > n || memcmp(t + q + 4, units[k], w))
            continue;
        ok = e >= n || (unsigned char)t[e] < 0xea || (unsigned char)t[e] > 0xed;
        for (j = 0; !ok && j < sizeof(particles) / sizeof(*particles); ++j)
            ok = e + strlen(particles[j]) <= n && !memcmp(t + e, particles[j], strlen(particles[j]));
        if (!ok || !korean_context(in))
            return 0;
        (void)snprintf(v, sizeof(v), "%s|%.*s", units[k], (int)(q - p), t + p);
        return ko_finish(in, s, e, TTF_SEM_SYMBOL, v, "@count-per", "ko:count-per-time");
    }
    return 0;
}

/* 8m22 팔 미터 이십이, 2m 35: a field-event mark, metres then two centimetre digits with the
   centimetre unit left unwritten (round 24; was 팔엠이십이); 8m15cm 팔 미터 십오 센티미터 (was
   팔 엠 일오 씨엠). The mark must close the word or take a particle, so 5m30s and 8m 22개 are
   not taken. */
static size_t field_mark(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    static const char *const particles[] = {"로", "으로", "를", "을", "은", "는", "이", "가", "의", "에", "와", "과", "도", "만", "까지", "에서", "였", "이었", "이다", "다"};
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q, c, e, j;
    int cm;
    char v[16];
    if (p >= n || !ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || ((t[p - 1] | 0x20) >= 'a' && (t[p - 1] | 0x20) <= 'z') ||
                                           t[p - 1] == '.' || t[p - 1] == '-')))
        return 0;
    for (q = p; q < n && ko_digit(t[q]) && q - p < 3; ++q)
        ;
    if (q - p > 2 || q >= n || t[q] != 'm')
        return 0;
    c = q + 1 + (q + 1 < n && t[q + 1] == ' ');
    if (c + 2 > n || !ko_digit(t[c]) || !ko_digit(t[c + 1]))
        return 0;
    e = c + 2;
    cm = e + 1 < n && t[e] == 'c' && t[e + 1] == 'm' && !(e + 2 < n && ((t[e + 2] | 0x20) >= 'a' && (t[e + 2] | 0x20) <= 'z'));
    if (cm)
        e += 2;
    if (e < n && (ko_digit(t[e]) || ((t[e] | 0x20) >= 'a' && (t[e] | 0x20) <= 'z') || t[e] == '%' || t[e] == ':' ||
                  ((t[e] == '.' || t[e] == ',') && e + 1 < n && ko_digit(t[e + 1]))))
        return 0;
    if (e < n && (unsigned char)t[e] >= 0xea && (unsigned char)t[e] <= 0xed) {
        int ok = 0;
        for (j = 0; !ok && j < sizeof(particles) / sizeof(*particles); ++j)
            ok = e + strlen(particles[j]) <= n && !memcmp(t + e, particles[j], strlen(particles[j]));
        if (!ok)
            return 0;
    }
    if (!korean_context(in))
        return 0;
    (void)snprintf(v, sizeof(v), "%.*s|%.2s%s", (int)(q - p), t + p, t + c, cm ? "|cm" : "");
    return ko_finish(in, s, e, TTF_SEM_SYMBOL, v, "@field-mark", "ko:field-mark");
}

/* 27" 이십칠 인치, 32″ 삼십이 인치: a double prime or a straight double quote after a screen size
   reads 인치 when a screen or device word is near (round 24; 32″ was 더블 프라임). Without
   such a word the mark stays ambiguous (seconds, a closing quote), as unit-symbol-v21 left it;
   a size after a foot mark (5'11") and a closing quote (an odd count of " before) are not taken. */
static size_t screen_inch(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    static const char *const cues[] = {"모니터", "화면", "텔레비전", "티브이", "TV", "노트북", "디스플레이", "태블릿",
                                       "스마트폰", "휴대폰", "액정", "패널", "아이패드", "맥북", "인치"};
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q, e, a, b, k, j, quotes = 0;
    int cue = 0;
    char v[16];
    if (p >= n || !ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || t[p - 1] == '\'' || t[p - 1] == '.' ||
                                           ((t[p - 1] | 0x20) >= 'a' && (t[p - 1] | 0x20) <= 'z'))) ||
        (p >= 3 && !memcmp(t + p - 3, "\xe2\x80\xb2", 3)))
        return 0;
    for (q = p; q < n && (ko_digit(t[q]) || (t[q] == '.' && q > p && q + 1 < n && ko_digit(t[q + 1]))) && q - p < 6; ++q)
        ;
    if (q < n && t[q] == '"') {
        for (k = 0; k < p; ++k)
            quotes += t[k] == '"';
        if (quotes % 2)
            return 0;
        e = q + 1;
    } else if (q + 3 <= n && !memcmp(t + q, "\xe2\x80\xb3", 3))
        e = q + 3;
    else
        return 0;
    if (e < n && ko_digit(t[e]))
        return 0;
    a = p > 90 ? p - 90 : 0;
    b = e + 90 < n ? e + 90 : n;
    for (k = a; !cue && k < b; ++k)
        for (j = 0; !cue && j < sizeof(cues) / sizeof(*cues); ++j)
            cue = k + strlen(cues[j]) <= b && !memcmp(t + k, cues[j], strlen(cues[j]));
    if (!cue || !korean_context(in))
        return 0;
    (void)snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
    return ko_finish(in, s, e, TTF_SEM_SYMBOL, v, "@inch", "ko:screen-inch");
}

/* 2x로 두 배, 1.5x 일 점 오 배: a number with a lowercase x after it is a multiple (speed or
   size; 2026-10-30, was 이 엑스). 1Q 일 분기, 3Q 삼 분기: a quarter of the year (was 일 큐).
   Both must close the word, and the number must not follow a letter or digit. */
static size_t times_or_quarter(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, d = p;
    char v[16];
    if (!ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || ((t[p - 1] | 0x20) >= 'a' && (t[p - 1] | 0x20) <= 'z') ||
                                  t[p - 1] == '.' || t[p - 1] == '-' || t[p - 1] == '/')))
        return 0;
    while (d < n && (ko_digit(t[d]) || (t[d] == '.' && d + 1 < n && ko_digit(t[d + 1]))) && d - p < sizeof(v) - 1)
        ++d;
    /* 4Q23 이십삼 년 사 분기, 1Q'24: a quarter with a two-digit year after it (round 21; was 사큐이십삼) */
    if (d < n && t[d] == 'Q' && d - p == 1 && t[p] >= '1' && t[p] <= '4') {
        size_t y = d + 1 + (d + 1 < n && t[d + 1] == '\'');
        if (y + 2 <= n && ko_digit(t[y]) && ko_digit(t[y + 1]) &&
            !(y + 2 < n && (ko_digit(t[y + 2]) || ((t[y + 2] | 0x20) >= 'a' && (t[y + 2] | 0x20) <= 'z'))) &&
            korean_context(in)) {
            (void)snprintf(v, sizeof(v), "%c|%.2s", t[p], t + y);
            return ko_finish(in, s, y + 2, TTF_SEM_SYMBOL, v, "@quarter-year", "ko:quarter-year");
        }
    }
    if (d >= n || (t[d] != 'x' && t[d] != 'Q') || (d + 1 < n && (ko_digit(t[d + 1]) || ((t[d + 1] | 0x20) >= 'a' && (t[d + 1] | 0x20) <= 'z'))) ||
        !korean_context(in))
        return 0;
    (void)snprintf(v, sizeof(v), "%.*s", (int)(d - p), t + p);
    if (t[d] == 'Q') {
        if (d - p != 1 || t[p] < '1' || t[p] > '4')
            return 0;
        return ko_finish(in, s, d + 1, TTF_SEM_SYMBOL, v, "@quarter", "ko:quarter-q");
    }
    return ko_finish(in, s, d + 1, TTF_SEM_SYMBOL, v, "@times", "ko:times");
}

/* ▲8.4%로 … 상승으로: a particle that ends the word after a reading that now ends in the
   syllable cp takes that syllable's form (로/으로 를/을 가/이 는/은 와/과); it is appended to
   v and the returned end passes it. Otherwise d is returned unchanged. */
static size_t fitted_particle(const char *t, size_t n, size_t d, uint32_t cp, char *v, size_t capacity) {
    static const char *const pairs[][2] = {{"로", "으로"}, {"를", "을"}, {"가", "이"},
                                           {"는", "은"}, {"와", "과"}};
    size_t k;
    for (k = 0; cp >= 0xac00 && cp <= 0xd7a3 && k < sizeof(pairs) / sizeof(*pairs); ++k) {
        int form;
        for (form = 0; form < 2; ++form) {
            size_t width = strlen(pairs[k][form]);
            const unsigned char *u = (const unsigned char *)t + d + width;
            unsigned final;
            int consonant;
            size_t used;
            if (d + width > n || memcmp(t + d, pairs[k][form], width) ||
                (d + width + 3 <= n && u[0] >= 0xea && u[0] <= 0xed))
                continue;
            final = (unsigned)((cp - 0xac00) % 28);
            consonant = final && !(k == 0 && final == 8); /* ㄹ keeps 로 */
            used = strlen(v);
            (void)snprintf(v + used, capacity - used, "%s", pairs[k][consonant]);
            return d + width;
        }
    }
    return d;
}

/* 98,000원/kg 킬로그램당 구만 팔천 원, 520만 원/㎡ 제곱미터당 오백이십만 원: a won price
   per measure unit reads the unit first with 당 (2026-10-29; was 원 슬래시 킬로그램). */
static size_t price_per_unit(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    static const char won[] = "\xec\x9b\x90"; /* 원 */
    static const char *const scales[] = {"\xec\xa1\xb0", "\xec\x96\xb5", "\xeb\xa7\x8c", "\xec\xb2\x9c"}; /* 조 억 만 천 */
    const char *t = in->text, *suffix, *label;
    size_t n = in->length, p = in->tokens[in->index].begin, d, q, w, used = 0, k;
    char number[32], scale[16] = "", id[48], v[TTF_IR_VALUE_SIZE];
    if (!ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ',')))
        return 0;
    for (d = p; d < n && (ko_digit(t[d]) || ((t[d] == ',' || t[d] == '.') && d + 1 < n && ko_digit(t[d + 1]))); ++d)
        if (t[d] != ',' && used + 1 < sizeof(number))
            number[used++] = t[d];
    number[used] = 0;
    q = d;
    for (k = 0; k < sizeof(scales) / sizeof(*scales); ++k)
        if (q + 3 <= n && !memcmp(t + q, scales[k], 3)) {
            memcpy(scale, t + q, 3);
            scale[3] = 0;
            q += 3;
            break;
        }
    if (q < n && t[q] == ' ')
        ++q;
    if (q + 4 > n || memcmp(t + q, won, 3) || t[q + 3] != '/')
        return 0;
    q += 4;
    suffix = quantity_resource_at(in, "numeric_suffix", q, &w);
    if (!suffix || strncmp(suffix, "MEASURE:", 8) || strlen(suffix + 8) >= sizeof(id))
        return 0;
    (void)snprintf(id, sizeof(id), "%s", suffix + 8);
    label = ttf_resource_get(in->engine, "unit", "ko-KR", id, NULL);
    if (!label || (q + w < n && (ko_digit(t[q + w]) || ((t[q + w] | 0x20) >= 'a' && (t[q + w] | 0x20) <= 'z'))))
        return 0;
    (void)snprintf(v, sizeof(v), "%s|%s|%s|", label, number, scale);
    return ko_finish(in, s, fitted_particle(t, n, q + w, 0xc6d0 /* 원 */, v, sizeof(v)), TTF_SEM_SYMBOL, v,
                     "@per-unit", "ko:price-per-unit");
}

/* 원/달러 환율 원 달러: a slash between two currency words names the pair (currency_pair_word;
   2026-10-29, was 원 슬래시 달러). */
static size_t currency_pair(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, a = p, b = p + 1, k;
    char left[16], right[16];
    int found_left = 0, found_right = 0;
    if (t[p] != '/' || !p)
        return 0;
    for (k = 3; k <= 9 && k <= a && !found_left; k += 3) {
        memcpy(left, t + a - k, k);
        left[k] = 0;
        found_left = ttf_resource_get(in->engine, "currency_pair_word", "ko-KR", left, NULL) != NULL &&
                     !(a - k >= 3 && (unsigned char)t[a - k - 3] >= 0xea && (unsigned char)t[a - k - 3] <= 0xed);
    }
    for (k = 9; k >= 3 && !found_right; k -= 3) {
        if (b + k > n)
            continue;
        memcpy(right, t + b, k);
        right[k] = 0;
        found_right = ttf_resource_get(in->engine, "currency_pair_word", "ko-KR", right, NULL) != NULL;
    }
    if (!found_left || !found_right)
        return 0;
    return ko_finish(in, s, p + 1, TTF_SEM_SYMBOL, " ", "@word", "ko:currency-pair");
}

/* report_final.pdf 리포트 언더스코어 파이널 닷 피디에프: the period before a declared file
   extension (file_extension) reads with its symbol name; it was kept as written and so
   was silent (2026-10-29). Only right after a letter, digit or Hangul syllable (보고서.pdf;
   2026-10-30, was 보고서.피디에프) and before the end of the name, so a sentence period
   stays punctuation. An extension declared with a reading (exe 이엑스이) is read with it. */
static size_t file_dot(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text, *name, *reading;
    size_t n = in->length, p = in->tokens[in->index].begin, q = p + 1, k;
    char ext[8], word[64];
    int hangul = p >= 3 && (unsigned char)t[p - 3] >= 0xea && (unsigned char)t[p - 3] <= 0xed &&
                 ((unsigned char)t[p - 2] & 0xc0) == 0x80 && ((unsigned char)t[p - 1] & 0xc0) == 0x80;
    if (t[p] != '.' || !p ||
        !(hangul || ko_digit(t[p - 1]) || ((t[p - 1] | 0x20) >= 'a' && (t[p - 1] | 0x20) <= 'z')))
        return 0;
    for (k = 0; q + k < n && k + 1 < sizeof(ext) && (((t[q + k] | 0x20) >= 'a' && (t[q + k] | 0x20) <= 'z') || ko_digit(t[q + k])); ++k)
        ext[k] = (char)(t[q + k] | (ko_digit(t[q + k]) ? 0 : 0x20));
    ext[k] = 0;
    if (!k || (q + k < n && (ko_digit(t[q + k]) || ((t[q + k] | 0x20) >= 'a' && (t[q + k] | 0x20) <= 'z') ||
                             (t[q + k] == '.' && q + k + 1 < n && t[q + k + 1] != ' '))) ||
        !(reading = ttf_resource_get(in->engine, "file_extension", "ko-KR", ext, NULL)) || !korean_context(in))
        return 0;
    name = ttf_resource_get(in->engine, "speech_symbol", "ko-KR", ".", NULL);
    if (!name || strlen(name) + strlen(reading) + 3 > sizeof(word))
        return 0;
    if (strcmp(reading, "1")) {
        size_t a = strlen(name), b = strlen(reading);
        word[0] = ' ';
        memcpy(word + 1, name, a);
        word[a + 1] = ' ';
        memcpy(word + a + 2, reading, b + 1);
        return ko_finish(in, s, q + k, TTF_SEM_SYMBOL, word, "@word", "ko:file-dot");
    }
    (void)snprintf(word, sizeof(word), " %s ", name);
    return ko_finish(in, s, q, TTF_SEM_SYMBOL, word, "@word", "ko:file-dot");
}

/* 1+1 행사 원 플러스 원, 2+1이라 투 플러스 원: a one-digit sum before a sales word
   (promo_cue) is the shop offer, read with the English numbers (promo_number; 2026-10-29,
   was 일 더하기 일). 1+1은 2 stays arithmetic. */
static size_t promo_plus(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text, *a, *b;
    size_t n = in->length, p = in->tokens[in->index].begin, q = p + 3, r, k;
    char key[2] = {0, 0}, v[96];
    int cue = 0;
    if (q > n || !ko_digit(t[p]) || t[p + 1] != '+' || !ko_digit(t[p + 2]) ||
        (p && (ko_digit(t[p - 1]) || t[p - 1] == '+' || t[p - 1] == '-' || t[p - 1] == '.' ||
               ((t[p - 1] | 0x20) >= 'a' && (t[p - 1] | 0x20) <= 'z'))) ||
        (q < n && (ko_digit(t[q]) || t[q] == '+' || t[q] == '=' || t[q] == '.' || t[q] == '-' ||
                   ((t[q] | 0x20) >= 'a' && (t[q] | 0x20) <= 'z'))))
        return 0;
    key[0] = t[p];
    a = ttf_resource_get(in->engine, "promo_number", "ko-KR", key, NULL);
    key[0] = t[p + 2];
    b = ttf_resource_get(in->engine, "promo_number", "ko-KR", key, NULL);
    if (!a || !b)
        return 0;
    r = q < n && t[q] == ' ' ? q + 1 : q;
    for (k = 3; k <= 15 && r + k <= n && !cue; k += 3) {
        char word[16];
        memcpy(word, t + r, k);
        word[k] = 0;
        cue = ttf_resource_get(in->engine, "promo_cue", "ko-KR", word, NULL) != NULL;
    }
    if (!cue)
        return 0;
    (void)snprintf(v, sizeof(v), "%s %s %s", a,
                   ttf_resource_get(in->engine, "speech_symbol", "ko-KR", "+", NULL)
                       ? ttf_resource_get(in->engine, "speech_symbol", "ko-KR", "+", NULL)
                       : "플러스",
                   b);
    return ko_finish(in, s, q, TTF_SEM_SYMBOL, v, "@word", "ko:promo-plus");
}

/* ▲1.2% 일 점 이 퍼센트 상승, ≥60점 육십 점 이상, ~10분 약 십 분: a mark written before a
   number reads around it (number_mark, {N} is the number and its unit; 2026-10-29, was the
   mark's name before the number). The mark opens a word, so x ≥ 5 stays a relation and 3~5
   a range. The unit is a declared word (number_mark_unit) or a measure symbol; without one
   only a particle may follow. */
static int bare_month_day(const char *t, size_t n, size_t at, size_t *end);
static size_t number_mark(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text, *unit = NULL;
    size_t n = in->length, b = in->tokens[in->index].begin, m = in->tokens[in->index].end, p, d, w = 0;
    char key[8], number[32], v[TTF_IR_VALUE_SIZE], id[48];
    size_t used = 0;
    if (m - b >= sizeof(key))
        return 0;
    memcpy(key, t + b, m - b);
    key[m - b] = 0;
    if (!ttf_resource_get(in->engine, "number_mark", "ko-KR", key, NULL) || !korean_context(in))
        return 0;
    if (b) {
        /* written apart from the word before (37.5도~38도 is a range) */
        size_t c = b;
        if (t[b - 1] != ' ' && t[b - 1] != '(')
            return 0;
        while (c && t[c - 1] == ' ')
            --c;
        if (c && (ko_digit(t[c - 1]) || (t[c - 1] >= 'a' && t[c - 1] <= 'z') ||
                  (t[c - 1] >= 'A' && t[c - 1] <= 'Z') || t[c - 1] == ')' || t[c - 1] == ']'))
            return 0;
    }
    p = m;
    if (p < n && t[p] == ' ')
        ++p;
    for (d = p; d < n && (ko_digit(t[d]) || (t[d] == ',' && d > p && d + 1 < n && ko_digit(t[d + 1])) ||
                          (t[d] == '.' && d > p && d + 1 < n && ko_digit(t[d + 1])));
         ++d)
        if (t[d] != ',' && used + 1 < sizeof(number))
            number[used++] = t[d];
    if (d == p || !ko_digit(t[p]) || used + 1 >= sizeof(number))
        return 0;
    number[used] = 0;
    {
        /* ~11.14까지 십일 월 십사 일까지: a dotted month and day before 까지 is a date, so the
           range mark means "until" and is not read (2026-10-31, TN round 11; was 약 십일 점 일사) */
        size_t e;
        if (bare_month_day(t, n, p, &e) && e == d && starts_with_word(t, n, d, "까지"))
            return ttf_resource_get(in->engine, "speech_range_mark", "ko-KR", key, NULL)
                       ? ko_finish(in, s, m, TTF_SEM_SYMBOL, "", "@word", "ko:mark-until")
                       : 0;
    }
    {
        /* the longest declared unit word, else a measure symbol (%, %p, ml) */
        size_t k;
        for (k = d + 12 < n ? 12 : n - d; k > 0 && !unit; --k)
            if (d + k == n || ((unsigned char)t[d + k] & 0xc0u) != 0x80u) {
                char word[16];
                memcpy(word, t + d, k);
                word[k] = 0;
                if ((unit = ttf_resource_get(in->engine, "number_mark_unit", "ko-KR", word, NULL)))
                    w = k;
            }
        if (!unit) {
            const char *suffix = d < n ? quantity_resource_at(in, "numeric_suffix", d, &w) : NULL;
            if (suffix && !strncmp(suffix, "MEASURE:", 8) && strlen(suffix + 8) < sizeof(id)) {
                (void)snprintf(id, sizeof(id), "%s", suffix + 8);
                unit = ttf_resource_get(in->engine, "unit", "ko-KR", id, NULL);
            }
            if (!unit)
                w = 0;
        }
    }
    d += w;
    if (d < n && (ko_digit(t[d]) || (t[d] >= 'a' && t[d] <= 'z') || (t[d] >= 'A' && t[d] <= 'Z')))
        return 0;
    /* ~10/31까지, ~12:00: a date or time after the mark is not an amount. A range mark
       there means "until" and is not read; the other marks keep their names. */
    if (!unit && d + 1 < n && (t[d] == '/' || t[d] == ':' || t[d] == '.' || t[d] == '-') && ko_digit(t[d + 1]))
        return ttf_resource_get(in->engine, "speech_range_mark", "ko-KR", key, NULL)
                   ? ko_finish(in, s, m, TTF_SEM_SYMBOL, "", "@word", "ko:mark-until")
                   : 0;
    if (!unit && d + 3 <= n && (unsigned char)t[d] >= 0xea && (unsigned char)t[d] <= 0xed) {
        /* ≥25이면: only a particle may follow a bare number */
        size_t k;
        int particle = 0;
        for (k = 3; k <= 9 && d + k <= n && !particle; k += 3) {
            char word[16];
            memcpy(word, t + d, k);
            word[k] = 0;
            particle = ttf_resource_get(in->engine, "particle", "ko-KR", word, NULL) != NULL;
        }
        if (!particle)
            return 0;
    }
    (void)snprintf(v, sizeof(v), "%s|%s|%s|", key, number, unit ? unit : "");
    {
        /* ▲8.4%로 팔 점 사 퍼센트 상승으로: a word after the number ends the reading, so a
           particle that ends the word takes the form for that word's last syllable. */
        const char *template = ttf_resource_get(in->engine, "number_mark", "ko-KR", key, NULL);
        size_t tl = strlen(template);
        uint32_t cp = 0;
        if (tl >= 3 && strcmp(template + tl - 3, "{N}") && (unsigned char)template[tl - 3] >= 0xea &&
            (unsigned char)template[tl - 3] <= 0xed)
            cp = ((uint32_t)(template[tl - 3] & 0x0f) << 12) | ((uint32_t)(template[tl - 2] & 0x3f) << 6) |
                 (uint32_t)(template[tl - 1] & 0x3f);
        d = fitted_particle(t, n, d, cp, v, sizeof(v));
    }
    return ko_finish(in, s, d, TTF_SEM_SYMBOL, v, "@mark", "ko:number-mark");
}

/* 6천여 m² 육천여 제곱미터, 100여 m 백여 미터, 500여 ㎡: a unit symbol after a number with the
   approximate 여 (and a space) reads as its unit; the grammar takes a unit only right after
   the number, and the symbol was spelled (엠 제곱) (2026-10-24). */
static size_t approximate_unit(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    static const char yeo[] = "\xec\x97\xac"; /* 여 */
    const char *t = in->text, *suffix, *label;
    size_t n = in->length, p = in->tokens[in->index].begin, b = p, width;
    char id[48];
    while (b && t[b - 1] == ' ')
        --b;
    if (b < 4 || memcmp(t + b - 3, yeo, 3) || !korean_context(in))
        return 0;
    {
        /* 여 after a digit or a Korean number word (6천여, 8,400여, 3만여) */
        size_t c = b - 3;
        int number = c && ko_digit(t[c - 1]);
        static const char *const words[] = {"\xec\xb2\x9c", "\xeb\xb0\xb1", "\xeb\xa7\x8c", "\xec\x8b\xad", "\xec\x96\xb5"}; /* 천 백 만 십 억 */
        size_t k;
        for (k = 0; !number && k < sizeof(words) / sizeof(*words); ++k)
            number = c >= 3 && !memcmp(t + c - 3, words[k], 3) && c >= 4;
        if (!number)
            return 0;
    }
    suffix = quantity_resource_at(in, "numeric_suffix", p, &width);
    if (!suffix || strncmp(suffix, "MEASURE:", 8) || strlen(suffix + 8) >= sizeof(id))
        return 0;
    if (p + width < n && ((t[p + width] >= 'a' && t[p + width] <= 'z') || (t[p + width] >= 'A' && t[p + width] <= 'Z') ||
                          ko_digit(t[p + width])))
        return 0;
    (void)snprintf(id, sizeof(id), "%s", suffix + 8);
    label = ttf_resource_get(in->engine, "unit", "ko-KR", id, NULL);
    if (!label)
        return 0;
    return ko_finish(in, s, p + width, TTF_SEM_SYMBOL, label, "@word", "ko:approximate-unit");
}

/* x², mc², km²: a superscript power right after a Latin letter or digit that no grammar took
   reads 제곱 (2026-10-23; was passed through). Superscripts written alone (¹²³, 각주¹) stay.
   After a closing bracket too: (x+1)²=0을 (엑스 더하기 일) 제곱 (2026-10-26). */
static size_t trailing_power(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q = p, w, used = 0;
    char power[8], c, v[48];
    if (!p ||
        !((t[p - 1] >= 'a' && t[p - 1] <= 'z') || (t[p - 1] >= 'A' && t[p - 1] <= 'Z') || ko_digit(t[p - 1]) ||
          t[p - 1] == ')') ||
        !korean_context(in))
        return 0;
    while ((w = superscript_at(t, n, q, &c)) != 0 && used + 1 < sizeof(power)) {
        power[used++] = c;
        q += w;
    }
    if (!used || (q < n && superscript_at(t, n, q, &c)))
        return 0;
    power[used] = '\0';
    (void)snprintf(v, sizeof(v), "^%s", power);
    return ko_finish(in, s, q, TTF_SEM_SYMBOL, v, "@expression", "ko:trailing-power");
}

/* 2025타경10492 이천이십오 타경 만 사백구십이: a court case number, a year, the case type and
   the case number read as numbers (project-owner decision 2026-10-23). */
static size_t court_case(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    static const char *const types = "타경|타채|가합|가단|가소|고합|고단|고정|구합|구단|드합|드단|카합|카단|"
                                     "헌마|헌바|헌가|노|도|누|두|나|다|머|브|스|르";
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q, z;
    char v[32];
    if (p + 4 >= n || (p && ko_digit(t[p - 1])))
        return 0;
    for (q = p; q < n && ko_digit(t[q]); ++q)
        ;
    if (q - p != 4 || !(t[p] == '1' || t[p] == '2'))
        return 0;
    {
        const char *w = types;
        size_t m = 0;
        while (*w) {
            size_t len = strcspn(w, "|");
            if (q + len < n && !memcmp(t + q, w, len) && ko_digit(t[q + len])) {
                m = len;
                break;
            }
            w += len + (w[len] == '|');
        }
        if (!m)
            return 0;
        for (z = q + m; z < n && ko_digit(t[z]); ++z)
            ;
        if (z - q - m > 7 || (z < n && (ko_digit(t[z]) || t[z] == '-' || t[z] == '.')))
            return 0;
        (void)snprintf(v, sizeof(v), "%.*s:%.*s", (int)(q - p), t + p, (int)(z - q - m), t + q + m);
        z = ko_finish(in, s, z, TTF_SEM_CARDINAL, v, "", "ko:court-case");
        (void)snprintf(s->secondary, sizeof(s->secondary), "%.*s", (int)m, t + q);
        return z;
    }
}

/* H₂O 에이치투오, H2SO4 에이치투에스오포, Fe2O3 에프이투오쓰리, C6H12O6 씨식스에이치트웰브오식스: a
   chemical formula spells its element symbols and reads each count as an English numeral, as
   Korean says it (project-owner decision 2026-10-26). A subscript count (₂, U+2080-U+2089) always
   makes a formula; in ASCII, two counts or a two-letter symbol do (CO2 and B7 already read so as
   letter-digit labels). Every symbol must be an element and every count one or two digits.
   Product codes look alike (BC40F28, P3K48 after 제품번호; release benchmark 2026-10-26), so an
   ASCII formula uses only common elements (no B, V, W, Y, U), no count starts with 0, and none
   follows 번호, 코드, 모델 or 품번. */
static int element_symbol(const char *t, size_t len) {
    static const char elements[] =
        "|H|He|Li|Be|B|C|N|O|F|Ne|Na|Mg|Al|Si|P|S|Cl|Ar|K|Ca|Sc|Ti|V|Cr|Mn|Fe|Co|Ni|Cu|Zn|Ga|Ge|As|Se|Br|Kr"
        "|Rb|Sr|Y|Zr|Nb|Mo|Tc|Ru|Rh|Pd|Ag|Cd|In|Sn|Sb|Te|I|Xe|Cs|Ba|La|Ce|Pr|Nd|Pm|Sm|Eu|Gd|Tb|Dy|Ho|Er"
        "|Tm|Yb|Lu|Hf|Ta|W|Re|Os|Ir|Pt|Au|Hg|Tl|Pb|Bi|Po|At|Rn|Fr|Ra|Ac|Th|Pa|U|Np|Pu|Am|Cm|Bk|Cf|Es"
        "|Fm|Md|No|Lr|Rf|Db|Sg|Bh|Hs|Mt|Ds|Rg|Cn|Nh|Fl|Mc|Lv|Ts|Og|";
    char key[5];
    key[0] = '|';
    memcpy(key + 1, t, len);
    key[len + 1] = '|';
    key[len + 2] = '\0';
    return len <= 2 && strstr(elements, key) != NULL;
}
/* The elements an ASCII formula may use: the ones everyday chemistry writes. */
static int common_element(const char *t, size_t len) {
    static const char common[] = "|H|C|N|O|F|P|S|Cl|Br|I|Na|K|Mg|Ca|Fe|Al|Si|Cu|Zn|Ag|Au|Pb|Mn|Cr|Ni|Co|Ba|Li|Hg|Sn|Ti|Pt|He|Ne|Ar|";
    char key[5];
    key[0] = '|';
    memcpy(key + 1, t, len);
    key[len + 1] = '|';
    key[len + 2] = '\0';
    return len <= 2 && strstr(common, key) != NULL;
}
static int subscript_digit(const char *t, size_t n, size_t p) {
    const unsigned char *u = (const unsigned char *)t + p;
    return p + 3 <= n && u[0] == 0xe2 && u[1] == 0x82 && u[2] >= 0x80 && u[2] <= 0x89 ? u[2] - 0x80 : -1;
}
static size_t chemical_formula(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q = p, used = 0;
    int groups = 0, counts = 0, subscripts = 0, two = 0, uncommon = 0, zero = 0;
    char v[TTF_IR_VALUE_SIZE];
    if (p >= n || t[p] < 'A' || t[p] > 'Z' ||
        (p && ((t[p - 1] >= 'A' && t[p - 1] <= 'Z') || (t[p - 1] >= 'a' && t[p - 1] <= 'z') || ko_digit(t[p - 1]))))
        return 0;
    while (q < n && t[q] >= 'A' && t[q] <= 'Z') {
        size_t len = q + 1 < n && t[q + 1] >= 'a' && t[q + 1] <= 'z' ? 2 : 1;
        int counted = 0, d;
        if (!element_symbol(t + q, len) || used + len + 8 >= sizeof(v))
            return 0;
        uncommon |= !common_element(t + q, len);
        memcpy(v + used, t + q, len);
        used += len;
        q += len;
        two |= len == 2;
        ++groups;
        for (;;) {
            if (q < n && ko_digit(t[q]))
                d = t[q++] - '0';
            else if ((d = subscript_digit(t, n, q)) >= 0) {
                q += 3;
                subscripts = 1;
            } else
                break;
            /* a count has one or two digits: K47YI5124 is a passport number (benchmark, 2026-10-26) */
            if (used + 2 >= sizeof(v) || counted == 2)
                return 0;
            zero |= !counted && d == 0;
            v[used++] = (char)('0' + d);
            ++counted;
        }
        counted = counted != 0;
        counts += counted;
    }
    v[used] = '\0';
    if (q < n && ((t[q] >= 'a' && t[q] <= 'z') || ko_digit(t[q]) || t[q] == '.' || subscript_digit(t, n, q) >= 0))
        return 0;
    if (!(subscripts && counts) && !(groups >= 2 && (counts >= 2 || (two && counts))))
        return 0;
    if (!subscripts) {
        size_t from = p > 16 ? p - 16 : 0;
        int labeled = 0;
        for (size_t k = from; k + 6 <= p && !labeled; ++k)
            labeled = !memcmp(t + k, "번호", 6) || !memcmp(t + k, "코드", 6) || !memcmp(t + k, "모델", 6) ||
                      !memcmp(t + k, "품번", 6);
        if (uncommon || zero || labeled)
            return 0;
    }
    if (!korean_context(in))
        return 0;
    return ko_finish(in, s, q, TTF_SEM_CARDINAL, v, "", "ko:chemical-formula");
}
/* x₁ 엑스원, a₁₂ 에이트웰브: a letter with a subscript index reads the index as an English
   numeral, like a formula count (2026-10-26). */
static size_t subscript_index(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q, used = 0;
    char v[16];
    int d;
    if (p >= n || !((t[p] >= 'a' && t[p] <= 'z') || (t[p] >= 'A' && t[p] <= 'Z')) ||
        (p && ((t[p - 1] >= 'A' && t[p - 1] <= 'Z') || (t[p - 1] >= 'a' && t[p - 1] <= 'z') || ko_digit(t[p - 1]))))
        return 0;
    v[used++] = t[p];
    for (q = p + 1; (d = subscript_digit(t, n, q)) >= 0 && used + 1 < sizeof(v); q += 3)
        v[used++] = (char)('0' + d);
    v[used] = '\0';
    if (used < 2 ||
        (q < n && ((t[q] >= 'a' && t[q] <= 'z') || (t[q] >= 'A' && t[q] <= 'Z') || ko_digit(t[q]) ||
                   subscript_digit(t, n, q) >= 0)) ||
        !korean_context(in))
        return 0;
    if (v[0] >= 'a' && v[0] <= 'z')
        v[0] = (char)(v[0] - 'a' + 'A');
    return ko_finish(in, s, q, TTF_SEM_CARDINAL, v, "", "ko:chemical-formula");
}

/* PM2.5 피엠 이 점 오: capital letters before a decimal number are spelled and the number is
   read in Korean (v3, 2026-10-23); PM10 stays a model name. */
static size_t letters_decimal(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, a, b, c, z;
    char letters[8], v[24];
    if (p >= n || t[p] < 'A' || t[p] > 'Z' || (p && ((t[p - 1] >= 'A' && t[p - 1] <= 'Z') ||
                                                    (t[p - 1] >= 'a' && t[p - 1] <= 'z') || ko_digit(t[p - 1]))))
        return 0;
    for (a = p; a < n && t[a] >= 'A' && t[a] <= 'Z' && a - p < 4; ++a)
        ;
    for (b = a; b < n && ko_digit(t[b]) && b - a < 3; ++b)
        ;
    if (b == a || b + 1 >= n || t[b] != '.' || !ko_digit(t[b + 1]))
        return 0;
    for (c = b + 1; c < n && ko_digit(t[c]) && c - b - 1 < 2; ++c)
        ;
    z = c;
    if (z < n && (ko_digit(t[z]) || t[z] == '.' || (t[z] >= 'A' && t[z] <= 'Z') || (t[z] >= 'a' && t[z] <= 'z')))
        return 0;
    if (!korean_context(in))
        return 0;
    (void)snprintf(letters, sizeof(letters), "%.*s", (int)(a - p), t + p);
    (void)snprintf(v, sizeof(v), "%.*s", (int)(z - a), t + a);
    z = ko_finish(in, s, z, TTF_SEM_CARDINAL, v, "", "ko:letters-decimal");
    (void)snprintf(s->secondary, sizeof(s->secondary), "%s", letters);
    return z;
}

/* A month and day written as M.D or M/D at `at` (1-2 digits each, a real date). */
static int bare_month_day(const char *t, size_t n, size_t at, size_t *end) {
    size_t q, r;
    unsigned month, day;
    for (q = at; q < n && ko_digit(t[q]); ++q)
        ;
    if (q == at || q - at > 2 || q + 1 >= n || (t[q] != '.' && t[q] != '/') || !ko_digit(t[q + 1]))
        return 0;
    for (r = q + 1; r < n && ko_digit(t[r]); ++r)
        ;
    if (r - q - 1 > 2)
        return 0;
    month = (unsigned)atoi(t + at);
    day = (unsigned)atoi(t + q + 1);
    *end = r;
    return month >= 1 && month <= 12 && day >= 1 && day <= 31;
}

static int date_tail(const char *t, size_t n, size_t r) {
    return r < n && (ko_digit(t[r]) || t[r] == '.' || t[r] == '/' || t[r] == '%' || t[r] == ':' || t[r] == '~' ||
                     t[r] == '-');
}

/* After a month and day pair: the end, punctuation, a space before a word, a particle, or a
   copula (입니다). A unit in Latin letters or a symbol (2.4~3.6km, 1.5~2.0℃) keeps it a number. */
static int date_follow_ok(const ttf_pack_input_t *in, size_t at) {
    const char *t = in->text;
    size_t n = in->length, k, a = at;
    while (a < n && t[a] == ' ')
        ++a;
    if (a >= n)
        return 1;
    if ((unsigned char)t[a] < 0x80)
        return !(((t[a] | 0x20) >= 'a' && (t[a] | 0x20) <= 'z') || ko_digit(t[a]) || t[a] == '%');
    if ((unsigned char)t[a] < 0xea || (unsigned char)t[a] > 0xed)
        return 0;
    if (a > at)
        return 1;
    /* 1.5~2.5만원: a number scale is not the particle 만 */
    if (starts_with_word(t, n, at, "만|억|천|조|배"))
        return 0;
    if (starts_with_word(t, n, at, "입니|이다|이며|이고|이었|였다"))
        return 1;
    for (k = 3; k <= 9 && at + k <= n; k += 3) {
        char word[16];
        memcpy(word, t + at, k);
        word[k] = 0;
        if (ttf_resource_get(in->engine, "particle", "ko-KR", word, NULL))
            return 1;
    }
    return 0;
}

/* 1.5~2.5 정도: an amount word after the pair (정도, 가량, 내외 …) makes it a number range */
static int approx_after(const char *t, size_t n, size_t at) {
    while (at < n && t[at] == ' ')
        ++at;
    return starts_with_word(t, n, at, "정도|가량|내외|안팎|수준|이상|이하|미만|초과");
}

/* 2024.1.1~12.31 이천이십사 년 일 월 일 일에서 십이 월 삼십일 일, 11/1~ 시작 십일 월 일 일부터
   시작: a month and day without a year reads as a date right after a full dotted date and
   its range mark, or right before an open range mark (~ then a space and a word, or the
   end) (2026-10-30, TN round 9; was 십이 점 삼일, 일 분의 십일). Two bare pairs (8.5~8.9 휴관)
   are dates unless a numeric word (수치, 비율, 금리 …) comes shortly before without a period
   word, or a unit follows (1.2~1.8배, 2.4~3.6km); a pair before a clock time (12.6 13:00~15:00)
   and one between ~ and 까지 (~11.14까지) are dates too; an amount word after the pair
   (1.5~2.5 정도) keeps numbers (2026-10-31, TN round 11; was 팔 점 오에서
   팔 점 구, 십이 점 육 열세 시, 약 십일 점 일사). 3.5~ 4.0 stays a number. */
static size_t range_month_day(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q, r, k;
    unsigned month, day;
    int after_date = 0, open = 0;
    char v[16];
    if (p >= n || !ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == '/')))
        return 0;
    for (q = p; q < n && ko_digit(t[q]); ++q)
        ;
    if (q - p > 2 || q + 1 >= n || (t[q] != '.' && t[q] != '/') || !ko_digit(t[q + 1]))
        return 0;
    for (r = q + 1; r < n && ko_digit(t[r]); ++r)
        ;
    if (r - q - 1 > 2)
        return 0;
    month = (unsigned)atoi(t + p);
    day = (unsigned)atoi(t + q + 1);
    if (month < 1 || month > 12 || day < 1 || day > 31)
        return 0;
    if (p >= 6 && t[p - 1] == '~' && ko_digit(t[p - 2])) {
        /* the left side is Y.M.D: four year digits, or 'YY */
        size_t dots = 0, lead = 0;
        for (k = p - 1; k > 0 && p - k < 14 && (ko_digit(t[k - 1]) || t[k - 1] == '.'); --k)
            dots += t[k - 1] == '.';
        while (k + lead < p && ko_digit(t[k + lead]))
            ++lead;
        after_date = dots == 2 && (lead == 4 || (lead == 2 && k && t[k - 1] == '\'')) &&
                     !(k && (ko_digit(t[k - 1]) || ((t[k - 1] | 0x20) >= 'a' && (t[k - 1] | 0x20) <= 'z')));
    }
    if (r < n && t[r] == '~' && (r + 1 == n || (t[r + 1] == ' ' && r + 2 < n && !ko_digit(t[r + 2]) &&
                                                t[r + 2] != '~' && t[r + 2] != '.')))
        open = 1;
    /* 12.6 13:00~15:00: a clock time after one space */
    if (!after_date && !open && r + 5 < n && t[r] == ' ' && ko_digit(t[r + 1])) {
        size_t c = r + 1;
        while (c < n && c - r - 1 < 2 && ko_digit(t[c]))
            ++c;
        if (c + 2 < n && t[c] == ':' && ko_digit(t[c + 1]) && ko_digit(t[c + 2]) &&
            !(c + 3 < n && ko_digit(t[c + 3])))
            open = 1;
    }
    /* ~11.14까지 */
    if (!after_date && !open && p && t[p - 1] == '~' && !(p >= 2 && (ko_digit(t[p - 2]) || t[p - 2] == '.')) &&
        starts_with_word(t, n, r, "까지"))
        open = 1;
    if (!after_date && !open) {
        /* 여름 성수기는 7.1~8.31: both sides of the range are month and day pairs */
        size_t e2, from;
        int pair = 0;
        if (r < n && t[r] == '~' && bare_month_day(t, n, r + 1, &e2) && !date_tail(t, n, e2) &&
            date_follow_ok(in, e2) && !approx_after(t, n, e2))
            pair = 1;
        else if (p >= 4 && t[p - 1] == '~' && !date_tail(t, n, r) && date_follow_ok(in, r) && !approx_after(t, n, r)) {
            for (from = p - 1; from > 0 && (ko_digit(t[from - 1]) || t[from - 1] == '.' || t[from - 1] == '/');
                 --from)
                ;
            pair = bare_month_day(t, n, from, &e2) && e2 == p - 1 &&
                   !(from && (((t[from - 1] | 0x20) >= 'a' && (t[from - 1] | 0x20) <= 'z') || t[from - 1] == '\''));
        }
        /* a bare pair needs Hangul words around it: 1.5~2.5 alone stays numbers */
        for (k = 0; pair && k < n; ++k)
            if ((unsigned char)t[k] >= 0xea && (unsigned char)t[k] <= 0xed)
                break;
        if (k == n)
            pair = 0;
        if (!pair || (word_within(t, p, 40, "수치|비율|지수|점수|평점|금리|성장률|확률|농도|배율|환율|지표|수익률|증가율|감소율|오차|버전") &&
                      !word_within(t, p, 40, "기간|성수기|일정|시즌|휴가|방학|연휴|행사")))
            return 0;
        open = 1; /* the right side and the range mark are checked above */
    }
    if (!open && date_tail(t, n, r))
        return 0;
    if (!korean_context(in))
        return 0;
    (void)snprintf(v, sizeof(v), "--%02u-%02u", month, day);
    return ko_finish(in, s, r, TTF_SEM_DATE, v, "month-day", "ko:range-month-day");
}

/* 월~금 운영 월요일부터 금요일까지 운영: a range between two one-syllable weekday names
   standing alone reads the full names (owner set ko-KR-0776 accepts 월요일부터 금요일까지;
   2026-10-30, TN round 9; was 월에서 금). Before a particle (월~금에는) it stays a plain
   range. */
static size_t weekday_range(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, end = p + 7;
    char a[4], b[4], v[64];
    if (end > n || t[p + 3] != '~' || (unsigned char)t[p] < 0xea || (unsigned char)t[p] > 0xed ||
        (unsigned char)t[p + 4] < 0xea || (unsigned char)t[p + 4] > 0xed)
        return 0;
    if (p && ((unsigned char)t[p - 1] >= 0x80 || ko_digit(t[p - 1]) || ((t[p - 1] | 0x20) >= 'a' && (t[p - 1] | 0x20) <= 'z')))
        return 0;
    if (end < n && ((unsigned char)t[end] >= 0x80 || ko_digit(t[end]) || ((t[end] | 0x20) >= 'a' && (t[end] | 0x20) <= 'z') ||
                    t[end] == '~' || t[end] == '-'))
        return 0;
    memcpy(a, t + p, 3);
    a[3] = 0;
    memcpy(b, t + p + 4, 3);
    b[3] = 0;
    if (!strcmp(a, b) || !ttf_resource_get(in->engine, "weekday_label", "ko-KR", a, NULL) ||
        !ttf_resource_get(in->engine, "weekday_label", "ko-KR", b, NULL) || !korean_context(in))
        return 0;
    (void)snprintf(v, sizeof(v), "%s요일부터 %s요일까지", a, b);
    return ko_finish(in, s, end, TTF_SEM_SYMBOL, v, "@word", "ko:weekday-range");
}

/* 4,000K 조명 사천 켈빈, 7.1ch 칠 점 일 채널, 2R 이 라운드: a number with K near a temperature or
   light word (4K 해상도 stays 포케이), with ch, or with R (2026-10-31, TN round 12; was 케이, 씨에이치,
   알). */
static size_t letter_unit(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q = p, w = 0, k;
    char v[32];
    const char *unit = NULL;
    if (p >= n || !ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || t[p - 1] == '.' || t[p - 1] == ',' ||
                                            ((t[p - 1] | 0x20) >= 'a' && (t[p - 1] | 0x20) <= 'z'))))
        return 0;
    while (q < n && (ko_digit(t[q]) || (t[q] == ',' && q + 1 < n && ko_digit(t[q + 1])) ||
                     (t[q] == '.' && q + 1 < n && ko_digit(t[q + 1]))))
        if (t[q++] != ',' && w + 1 < sizeof(v))
            v[w++] = t[q - 1];
    v[w] = 0;
    if (q >= n || w + 1 >= sizeof(v))
        return 0;
    /* 273 K 이백칠십삼 켈빈: one space may stand before the K (2026-10-31, TN round 17) */
    if ((t[q] == 'K' || (t[q] == ' ' && q + 1 < n && t[q + 1] == 'K')) && !strchr(v, '.') &&
        (word_within(t, p, 40, "온도|끓는점|녹는점|어는점|절대영도|색온도|조명|빛|광|냉각|극저온|표면|항성|태양|켈빈") ||
         word_after(t, n, q + 1, 16, "색온도|조명|광|빛")))
        unit = "kelvin", k = t[q] == ' ' ? 2 : 1;
    else if (q + 1 < n && t[q] == 'c' && t[q + 1] == 'h')
        unit = "channel", k = 2;
    else if (t[q] == 'R' && w <= 2 && !strchr(v, '.'))
        unit = "round", k = 1;
    else
        return 0;
    if (q + k < n && (ko_digit(t[q + k]) || ((t[q + k] | 0x20) >= 'a' && (t[q + k] | 0x20) <= 'z') || t[q + k] == '-'))
        return 0;
    if (!korean_context(in))
        return 0;
    return ko_finish(in, s, q + k, TTF_SEM_MEASURE, v, unit, "ko:letter-unit");
}

/* 마지막 세트에서 15:13으로 이기며 십오 대 십삼: a colon pair in a sentence with a result word
   (이기, 졌, 꺾, 역전 …), with a particle after it and not after a clock word, is a score
   (2026-10-31, TN round 12; was 열다섯 시 십삼 분). */
static size_t colon_score(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q, r, k;
    static const char *const cues[] = {"이기", "이겼", "졌", "꺾", "역전", "승리", "패배", "완승", "완패", "제압",
                                       "완파", "격파", "눌렀", "누르", "뒤집", "무승부", "동점", "스코어"};
    int cue = 0;
    if (p >= n || !ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || t[p - 1] == ':' || t[p - 1] == '.')))
        return 0;
    for (q = p; q < n && ko_digit(t[q]); ++q)
        ;
    if (q - p > 2 || q + 1 >= n || t[q] != ':' || !ko_digit(t[q + 1]))
        return 0;
    for (r = q + 1; r < n && ko_digit(t[r]); ++r)
        ;
    if (r - q - 1 > 2 || (r < n && (ko_digit(t[r]) || t[r] == ':' || (t[r] == '.' && r + 1 < n && ko_digit(t[r + 1])))) ||
        starts_with_word(t, n, r, "에|부터|까지|쯤|경") || word_within(t, p, 12, "오전|오후|새벽|아침|저녁|밤|낮"))
        return 0;
    for (k = 0; k < sizeof(cues) / sizeof(cues[0]) && !cue; ++k) {
        size_t m = strlen(cues[k]), i;
        for (i = 0; i + m <= n && !cue; ++i)
            cue = !memcmp(t + i, cues[k], m);
    }
    if (!cue || !korean_context(in))
        return 0;
    /* 15:13으로: a particle right after it; at the end or before a mark only when the minutes
       could not be a round clock time (결승 15:30 킥오프 stays a time) */
    if (!starts_with_word(t, n, r, "으로|로|의|이|가") &&
        !((r == n || t[r] == '.' || t[r] == ',' || t[r] == ')') && !(t[q + 1] == '0' && t[q + 2] == '0') &&
          !(t[q + 1] == '3' && t[q + 2] == '0') && r - q - 1 == 2))
        return 0;
    {
        char a[4], b[4];
        size_t z;
        (void)snprintf(a, sizeof(a), "%.*s", (int)(q - p), t + p);
        (void)snprintf(b, sizeof(b), "%.*s", (int)(r - q - 1), t + q + 1);
        z = ko_finish(in, s, r, TTF_SEM_RANGE, a, "", "ko:score");
        (void)snprintf(s->secondary, sizeof(s->secondary), "%s", b);
        return z;
    }
}

/* GPT-4.5 지피티 사 점 오, 라마-3.1 라마 삼 점 일, HTTP/2 에이치티티피 이, H.264 에이치 이육사: a name and
   its version joined by a hyphen (a decimal version), a slash after an acronym, or a period after one
   capital (a three-digit codec number) reads the name and then the number, the joining mark silent
   (2026-10-31, TN round 13; was 지피티포영 점 오, 라마마이너스 삼 점 일, 에이치티티피 슬래시 이,
   에이치영 점 이육사). A capitalized or lowercase name needs a lexicon reading (Gemini 제미나이);
   hyphenated integers (KF-21, COVID-19) keep their own readings. */
static size_t name_version(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text, *label;
    size_t n = in->length, p = in->tokens[in->index].begin, q = p, r, k, caps = 0, letters = 0, used = 0;
    char key[80], num[16], v[TTF_IR_VALUE_SIZE];
    ttf_buffer_t buf = {0};
    int hangul = 0, codec = 0, ok;
    char sep;
    if (p >= n || (p && t[p - 1] != ' ' && t[p - 1] != '(' && t[p - 1] != '"' && t[p - 1] != '\''))
        return 0;
    if ((unsigned char)t[p] >= 0xea && (unsigned char)t[p] <= 0xed) {
        while (q + 3 <= n && (unsigned char)t[q] >= 0xea && (unsigned char)t[q] <= 0xed && q - p < 15)
            q += 3;
        hangul = 1;
    } else {
        while (q < n && ((t[q] | 0x20) >= 'a' && (t[q] | 0x20) <= 'z') && q - p < 12) {
            caps += t[q] >= 'A' && t[q] <= 'Z';
            ++q;
        }
        letters = q - p;
    }
    if (q == p || q + 1 >= n || !ko_digit(t[q + 1]))
        return 0;
    sep = t[q];
    for (r = q + 1; r < n && ko_digit(t[r]); ++r)
        ;
    if (sep == '.') {
        codec = !hangul && letters == 1 && caps == 1 && r - q - 1 == 3;
        if (!codec)
            return 0;
    } else if (sep == '-' || (sep == '/' && !hangul && letters >= 3 && caps == letters)) {
        if (r - q - 1 > 2)
            return 0;
        if (r + 1 < n && t[r] == '.' && ko_digit(t[r + 1])) {
            for (++r; r < n && ko_digit(t[r]); ++r)
                ;
        } else if (sep == '-')
            return 0; /* KF-21, COVID-19: an integer after a hyphen keeps its reading */
    } else
        return 0;
    if (r < n && (ko_digit(t[r]) || ((t[r] | 0x20) >= 'a' && (t[r] | 0x20) <= 'z') || t[r] == '-' || t[r] == '/' ||
                  (t[r] == '.' && r + 1 < n && ko_digit(t[r + 1]))))
        return 0;
    if (hangul && r < n && (unsigned char)t[r] >= 0x80)
        return 0; /* 기온-3.5도: a Hangul name needs a space or mark after the version */
    if (r - q - 1 >= sizeof(num) || !korean_context(in))
        return 0;
    /* the name */
    if (hangul) {
        (void)snprintf(v, sizeof(v), "%.*s", (int)(q - p), t + p);
        used = q - p;
    } else if (caps == letters && letters <= 6) {
        for (k = p; k < q; ++k) {
            (void)snprintf(key, sizeof(key), "letter.%c", t[k]);
            label = ttf_resource_get(in->engine, "phone_letter", "ko-KR", key, NULL);
            if (!label || used + strlen(label) + 1 >= sizeof(v))
                return 0;
            used += (size_t)snprintf(v + used, sizeof(v) - used, "%s", label);
        }
    } else {
        size_t m = (size_t)snprintf(key, sizeof(key), "en-US:");
        for (k = p; k < q && m + 1 < sizeof(key); ++k)
            key[m++] = (char)(t[k] | 0x20);
        key[m] = 0;
        label = ttf_resource_get(in->engine, "lexicon", "ko-KR", key, NULL);
        if (!label || strlen(label) + 1 >= sizeof(v))
            return 0;
        used = (size_t)snprintf(v, sizeof(v), "%s", label);
    }
    /* the number: a codec number digit by digit, a version as a Sino number */
    (void)snprintf(num, sizeof(num), "%.*s", (int)(r - q - 1), t + q + 1);
    if (codec) {
        for (k = 0; num[k]; ++k) {
            char d[2] = {num[k], 0};
            label = ttf_resource_get(in->engine, "digit", "ko-KR", d, NULL);
            if (!label || used + strlen(label) + 2 >= sizeof(v))
                return 0;
            used += (size_t)snprintf(v + used, sizeof(v) - used, "%s%s", k ? "" : " ", label);
        }
    } else {
        ok = cardinal(in->engine, num, &buf) == TTF_OK && buf.data && used + buf.length + 2 < sizeof(v);
        if (ok)
            used += (size_t)snprintf(v + used, sizeof(v) - used, " %.*s", (int)buf.length, buf.data);
        ttf_buffer_free(&buf);
        if (!ok)
            return 0;
    }
    return ko_finish(in, s, r, TTF_SEM_SYMBOL, v, "@word", "ko:name-version");
}

/* 37.5665°N 북위 삼십칠 점 오육육오 도, 37°33′59″N 북위 삼십칠 도 삼십삼 분 오십구 초, 131°52′E 동경
   백삼십일 도 오십이 분: a latitude or longitude with its hemisphere letter, or degrees with minutes
   (′ or ') and seconds (″ or "), reads the hemisphere word first and the minutes and seconds as
   분 and 초 (2026-10-31, TN round 14; was 도 엔, 프라임, 더블 프라임). */
static size_t geo_coordinate(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text, *hemi = NULL;
    size_t n = in->length, p = in->tokens[in->index].begin, q, r, m0 = 0, m1 = 0, s0 = 0, s1 = 0, end;
    char v[TTF_IR_VALUE_SIZE], num[24];
    ttf_buffer_t buf = {0};
    int ok = 1;
    if (p >= n || !ko_digit(t[p]) || (p && (ko_digit(t[p - 1]) || t[p - 1] == '.')))
        return 0;
    for (q = p; q < n && ko_digit(t[q]); ++q)
        ;
    if (q - p > 3)
        return 0;
    if (q + 1 < n && t[q] == '.' && ko_digit(t[q + 1]))
        for (++q; q < n && ko_digit(t[q]); ++q)
            ;
    /* the degree sign U+00B0 */
    if (!(q + 2 <= n && (unsigned char)t[q] == 0xc2 && (unsigned char)t[q + 1] == 0xb0))
        return 0;
    r = q + 2;
    /* minutes: digits then U+2032 or ' */
    if (r < n && ko_digit(t[r])) {
        size_t a = r, b;
        for (b = a; b < n && ko_digit(t[b]) && b - a < 2; ++b)
            ;
        if (b + 3 <= n && (unsigned char)t[b] == 0xe2 && (unsigned char)t[b + 1] == 0x80 && (unsigned char)t[b + 2] == 0xb2)
            m0 = a, m1 = b, r = b + 3;
        else if (b < n && t[b] == '\'')
            m0 = a, m1 = b, r = b + 1;
        else
            return 0;
        if (r < n && ko_digit(t[r]) && m1) {
            a = r;
            for (b = a; b < n && ko_digit(t[b]) && b - a < 2; ++b)
                ;
            if (b + 3 <= n && (unsigned char)t[b] == 0xe2 && (unsigned char)t[b + 1] == 0x80 && (unsigned char)t[b + 2] == 0xb3)
                s0 = a, s1 = b, r = b + 3;
            else if (b < n && t[b] == '"')
                s0 = a, s1 = b, r = b + 1;
            else
                return 0;
        }
    }
    end = r;
    if (r < n && (t[r] == 'N' || t[r] == 'S' || t[r] == 'E' || t[r] == 'W') &&
        !(r + 1 < n && ((t[r + 1] | 0x20) >= 'a' && (t[r + 1] | 0x20) <= 'z'))) {
        hemi = t[r] == 'N' ? "북위 " : t[r] == 'S' ? "남위 " : t[r] == 'E' ? "동경 " : "서경 ";
        end = r + 1;
    }
    if (!hemi && !m1)
        return 0; /* 45° alone keeps the shared degree reading */
    if ((end < n && (ko_digit(t[end]) || t[end] == '.')) || !korean_context(in))
        return 0;
    (void)snprintf(num, sizeof(num), "%.*s", (int)(q - p), t + p);
    ok = ttf_buffer_puts(&buf, hemi ? hemi : "") == TTF_OK && cardinal(in->engine, num, &buf) == TTF_OK &&
         ttf_buffer_puts(&buf, " 도") == TTF_OK;
    if (ok && m1) {
        (void)snprintf(num, sizeof(num), "%d", atoi(t + m0));
        ok = ttf_buffer_puts(&buf, " ") == TTF_OK && cardinal(in->engine, num, &buf) == TTF_OK &&
             ttf_buffer_puts(&buf, " 분") == TTF_OK;
    }
    if (ok && s1) {
        (void)snprintf(num, sizeof(num), "%d", atoi(t + s0));
        ok = ttf_buffer_puts(&buf, " ") == TTF_OK && cardinal(in->engine, num, &buf) == TTF_OK &&
             ttf_buffer_puts(&buf, " 초") == TTF_OK;
    }
    ok = ok && buf.data && buf.length < sizeof(v);
    if (ok)
        (void)snprintf(v, sizeof(v), "%.*s", (int)buf.length, buf.data);
    ttf_buffer_free(&buf);
    if (!ok)
        return 0;
    return ko_finish(in, s, end, TTF_SEM_SYMBOL, v, "@word", "ko:geo-coordinate");
}

/* Algebra with a parenthesized group, a superscript power or a variable fraction:
   2(x-3)=8 이 괄호 엑스 빼기 삼 괄호 닫고는 팔, (x+1)² 괄호 엑스 더하기 일 괄호 닫고 제곱,
   x/3=4 삼 분의 엑스는 사, (a+b)/2 이 분의 에이 더하기 비, 2x²+3x 이 엑스 제곱 더하기 삼 엑스,
   -3ab² 마이너스 삼 에이 비 제곱 (2026-10-31, TN round 15; was 이(엑스쓰리)이퀄 팔, (엑스 더하기 일)
   제곱, 엑스 나누기 삼은 사, 두 배 제곱). Variables are single lowercase letters that are not
   units (5m², km/h, m/s stay units); letters stand together only after a coefficient (3ab);
   a fraction needs a number or a group on one side; a group needs an operator inside or a
   power after it (3/4(월), (a) stay as they are). The written form is kept as the value and
   read again by algebra_read. */
typedef struct {
    const ttf_engine_t *e;
    const char *t;
    size_t n, p;
    int vars, groups, powers, fractions, numbers, operators, chains;
} algebra_t;
static int algebra_var(char c) { return c >= 'a' && c <= 'z' && strchr("abcdknpqrtuvwxyz", c) != NULL; }
static int algebra_side(algebra_t *a, ttf_buffer_t *out);
static int algebra_put(ttf_buffer_t *out, const char *text) { return ttf_buffer_puts(out, text) == TTF_OK; }
static int algebra_power(algebra_t *a, ttf_buffer_t *out, int number_base) {
    char c, power[8];
    size_t w, used = 0;
    while ((w = superscript_at(a->t, a->n, a->p, &c)) != 0 && used + 1 < sizeof(power)) {
        power[used++] = c;
        a->p += w;
    }
    if (!used)
        return 1;
    power[used] = 0;
    ++a->powers;
    if (power[0] == '-' || !algebra_put(out, number_base ? "의 " : " "))
        return 0;
    if (!strcmp(power, "2"))
        return algebra_put(out, resource(a->e, "math_power", "2", "제곱"));
    if (!strcmp(power, "3"))
        return algebra_put(out, resource(a->e, "math_power", "3", "세제곱"));
    if (!strcmp(power, "4"))
        return algebra_put(out, resource(a->e, "math_power", "4", "네제곱"));
    return cardinal(a->e, power, out) == TTF_OK && algebra_put(out, resource(a->e, "math_power", "n", "제곱"));
}
/* Factors written together (3x, 2(x-3), a(b+c), (2a)²). bare gets the inner reading of a lone
   group without a power, which a fraction reads without 괄호. */
static int algebra_factors(algebra_t *a, ttf_buffer_t *out, ttf_buffer_t *bare) {
    const char *t = a->t;
    size_t n = a->n, count = 0;
    int coefficient = 0, letter = 0, lone = 0, ok = 1;
    ttf_buffer_t inner = {0};
    for (; ok; ++count) {
        size_t p = a->p;
        if (count && !algebra_put(out, " "))
            ok = 0;
        else if (p < n && ko_digit(t[p]) && !count) {
            char num[24];
            size_t q = p;
            while (q < n && ko_digit(t[q]))
                ++q;
            if (q + 1 < n && t[q] == '.' && ko_digit(t[q + 1]))
                for (++q; q < n && ko_digit(t[q]); ++q)
                    ;
            if (q - p >= sizeof(num))
                return 0;
            memcpy(num, t + p, q - p);
            num[q - p] = 0;
            a->p = q;
            ++a->numbers;
            coefficient = 1;
            ok = cardinal(a->e, num, out) == TTF_OK && algebra_power(a, out, 1);
        } else if (p < n && algebra_var(t[p]) && !(letter && !coefficient)) {
            a->p = p + 1;
            ++a->vars;
            letter = 1;
            ok = expression_letter(a->e, t[p], out) == TTF_OK && algebra_power(a, out, 0);
        } else if (p < n && t[p] == '(') {
            int operators = a->operators, powers;
            ttf_buffer_free(&inner);
            a->p = p + 1;
            if (!algebra_side(a, &inner) || a->p >= n || t[a->p] != ')') {
                ok = 0;
                break;
            }
            ++a->p;
            ++a->groups;
            powers = a->powers;
            ok = algebra_put(out, resource(a->e, "math_group", "open", "괄호")) && algebra_put(out, " ") &&
                 ttf_buffer_append(out, inner.data, inner.length) == TTF_OK && algebra_put(out, " ") &&
                 algebra_put(out, resource(a->e, "math_group", "close", "괄호 닫고")) && algebra_power(a, out, 0);
            if (a->operators == operators && a->powers == powers)
                ok = 0;
            lone = !count && a->powers == powers;
            letter = 0;
        } else {
            /* the separator written for a factor that is not there */
            if (count)
                out->length -= 1;
            break;
        }
    }
    if (ok && count) {
        if (lone && count == 1)
            ok = ttf_buffer_append(bare, inner.data, inner.length) == TTF_OK;
        else
            ok = ttf_buffer_append(bare, out->data, out->length) == TTF_OK;
    }
    ttf_buffer_free(&inner);
    return ok && count;
}
static int algebra_term(algebra_t *a, ttf_buffer_t *out) {
    ttf_buffer_t num = {0}, num_bare = {0}, den = {0}, den_bare = {0};
    int numbers = a->numbers, groups = a->groups, ok = algebra_factors(a, &num, &num_bare);
    if (ok && a->p < a->n && a->t[a->p] == '/') {
        ++a->p;
        ok = algebra_factors(a, &den, &den_bare) && (a->numbers > numbers || a->groups > groups);
        ok = ok && ttf_buffer_append(out, den_bare.data, den_bare.length) == TTF_OK && algebra_put(out, " ") &&
             algebra_put(out, resource(a->e, "math_fraction", "of", "분의")) && algebra_put(out, " ") &&
             ttf_buffer_append(out, num_bare.data, num_bare.length) == TTF_OK;
        ++a->fractions;
    } else if (ok)
        ok = ttf_buffer_append(out, num.data, num.length) == TTF_OK;
    ttf_buffer_free(&num);
    ttf_buffer_free(&num_bare);
    ttf_buffer_free(&den);
    ttf_buffer_free(&den_bare);
    return ok;
}
/* An operator at p: its canonical math_operator key and byte width, else 0. */
static size_t algebra_operator(const algebra_t *a, size_t p, const char **key) {
    const char *t = a->t;
    if (p < a->n && (t[p] == '+' || t[p] == '-' || t[p] == '*')) {
        *key = t[p] == '+' ? "+" : t[p] == '-' ? "-" : "*";
        return 1;
    }
    if (p + 2 <= a->n && (unsigned char)t[p] == 0xC3 && ((unsigned char)t[p + 1] == 0x97 || (unsigned char)t[p + 1] == 0xB7)) {
        *key = (unsigned char)t[p + 1] == 0x97 ? "×" : "÷";
        return 2;
    }
    if (p + 3 <= a->n && !memcmp(t + p, "\xe2\x88\x92", 3)) {
        *key = "-";
        return 3;
    }
    return 0;
}
static int algebra_side(algebra_t *a, ttf_buffer_t *out) {
    const char *key;
    size_t w;
    if (algebra_operator(a, a->p, &key) == 1 && !strcmp(key, "-")) {
        ++a->p;
        if (!algebra_put(out, resource(a->e, "number", "negative", "마이너스 ")))
            return 0;
    }
    for (;;) {
        const char *label;
        if (!algebra_term(a, out))
            return 0;
        if (!(w = algebra_operator(a, a->p, &key)))
            return 1;
        if (!(label = ttf_resource_get(a->e, "math_operator", "ko-KR", key, NULL)))
            return 0;
        a->p += w;
        ++a->operators;
        if (!algebra_put(out, " ") || !algebra_put(out, label) || !algebra_put(out, " "))
            return 0;
    }
}
static int algebra_expression(algebra_t *a, ttf_buffer_t *out) {
    static const struct {
        const char *text, *key;
    } relations[] = {{"=", "="}, {"<", "<"}, {">", ">"}, {"\xe2\x89\xa4", "≤"}, {"\xe2\x89\xa5", "≥"}, {"\xe2\x89\xa0", "≠"}};
    ttf_buffer_t left = {0}, right = {0};
    const char *template = NULL, *q;
    size_t k;
    int ok = algebra_side(a, &left);
    for (k = 0; ok && k < sizeof(relations) / sizeof(*relations); ++k)
        if (a->p + strlen(relations[k].text) <= a->n && !memcmp(a->t + a->p, relations[k].text, strlen(relations[k].text)))
            break;
    if (!ok || k == sizeof(relations) / sizeof(*relations)) {
        ok = ok && ttf_buffer_append(out, left.data, left.length) == TTF_OK;
        ttf_buffer_free(&left);
        return ok;
    }
    a->p += strlen(relations[k].text);
    ok = algebra_side(a, &right);
    /* 1<x<5 엑스는 일보다 크고 오보다 작다, -2≤x≤3 …보다 크거나 같고 …보다 작거나 같다: a value between two
       bounds, read from the middle (2026-10-31, TN round 16; was 일 엑스 오) */
    if (ok && (k == 1 || k == 3) && a->p < a->n) {
        size_t k2, w2 = 0;
        for (k2 = 0; k2 < sizeof(relations) / sizeof(*relations); ++k2)
            if (a->p + strlen(relations[k2].text) <= a->n &&
                !memcmp(a->t + a->p, relations[k2].text, (w2 = strlen(relations[k2].text))))
                break;
        if (k2 == 1 || k2 == 3) {
            ttf_buffer_t high = {0};
            a->p += w2;
            ++a->chains;
            ok = algebra_side(a, &high) && ttf_buffer_append(out, right.data, right.length) == TTF_OK &&
                 algebra_put(out, final_consonant(out) ? "은 " : "는 ") &&
                 ttf_buffer_append(out, left.data, left.length) == TTF_OK &&
                 algebra_put(out, resource(a->e, "math_chain", k == 1 ? "<.low" : "≤.low", k == 1 ? "보다 크고 " : "보다 크거나 같고 ")) &&
                 ttf_buffer_append(out, high.data, high.length) == TTF_OK &&
                 algebra_put(out, resource(a->e, "math_chain", k2 == 1 ? "<.high" : "≤.high", k2 == 1 ? "보다 작다" : "보다 작거나 같다"));
            ttf_buffer_free(&left);
            ttf_buffer_free(&right);
            ttf_buffer_free(&high);
            return ok;
        }
    }
    ok = ok && (template = ttf_resource_get(a->e, "math_relation", "ko-KR", relations[k].key, NULL));
    for (q = template; ok && *q;) {
        if (!strncmp(q, "{L}", 3)) {
            ok = ttf_buffer_append(out, left.data, left.length) == TTF_OK;
            q += 3;
        } else if (!strncmp(q, "{R}", 3)) {
            ok = ttf_buffer_append(out, right.data, right.length) == TTF_OK;
            q += 3;
        } else if (*q == '{' && strchr(q, '/') && strchr(q, '}') && strchr(q, '/') < strchr(q, '}')) {
            const char *slash = strchr(q, '/'), *close = strchr(q, '}');
            ok = (final_consonant(out) ? ttf_buffer_append(out, q + 1, (size_t)(slash - q - 1))
                                       : ttf_buffer_append(out, slash + 1, (size_t)(close - slash - 1))) == TTF_OK;
            q = close + 1;
        } else
            ok = ttf_buffer_append(out, q++, 1) == TTF_OK;
    }
    ttf_buffer_free(&left);
    ttf_buffer_free(&right);
    return ok;
}
/* (a+b)² alone keeps its written brackets, (에이 더하기 비) 제곱 (contract superscript-power-v119,
   2026-10-26; on the owner decision list with the 괄호 reading used inside a longer expression); so does
   a lone group, √(2x-1) 루트(이 엑스 빼기 일) (2026-10-31, TN round 16; was 이 엑스 다시 일). */
static int algebra_lone_power(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    size_t n = strlen(value), open = value[0] == '-', close, depth = 0, q;
    char c;
    if (open >= n || value[open] != '(')
        return 0;
    for (close = open; close < n; ++close) {
        depth += value[close] == '(';
        depth -= value[close] == ')';
        if (!depth)
            break;
    }
    if (close >= n)
        return 0;
    for (q = close + 1; q < n && superscript_at(value, n, q, &c); q += superscript_at(value, n, q, &c))
        ;
    if (q != n)
        return 0;
    {
        algebra_t inner = {e, value + open + 1, close - open - 1, 0, 0, 0, 0, 0, 0, 0, 0};
        algebra_t power = {e, value, n, close + 1, 0, 0, 0, 0, 0, 0, 0};
        return (!open || algebra_put(out, resource(e, "number", "negative", "마이너스 "))) && algebra_put(out, "(") &&
               algebra_side(&inner, out) && inner.p == inner.n && algebra_put(out, ")") && algebra_power(&power, out, 0) &&
               power.p == n;
    }
}
static ttf_status_t algebra_read(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    algebra_t a = {e, value, strlen(value), 0, 0, 0, 0, 0, 0, 0, 0};
    size_t mark = out->length;
    if (algebra_lone_power(e, value, out))
        return TTF_OK;
    out->length = mark;
    return algebra_expression(&a, out) && a.p == a.n ? TTF_OK : TTF_EINVAL;
}
static size_t algebra(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text, *key;
    size_t n = in->length, p = in->tokens[in->index].begin, end;
    algebra_t a = {in->engine, t, n, p, 0, 0, 0, 0, 0, 0, 0};
    ttf_buffer_t check = {0};
    char v[TTF_IR_VALUE_SIZE], c;
    int ok;
    if (p >= n || !(ko_digit(t[p]) || algebra_var(t[p]) || t[p] == '(' || t[p] == '-') ||
        (p && ((t[p - 1] >= 'a' && t[p - 1] <= 'z') || (t[p - 1] >= 'A' && t[p - 1] <= 'Z') || ko_digit(t[p - 1]) ||
               t[p - 1] == '.' || t[p - 1] == ')' || t[p - 1] == '/' || t[p - 1] == '-' || t[p - 1] == '+')))
        return 0;
    ok = algebra_expression(&a, &check);
    ttf_buffer_free(&check);
    end = a.p;
    if (!ok || !a.vars || !(a.groups || a.powers || a.fractions || a.chains) || end - p >= sizeof(v) ||
        (end < n && ((t[end] >= 'a' && t[end] <= 'z') || (t[end] >= 'A' && t[end] <= 'Z') || ko_digit(t[end]) ||
                     t[end] == '(' || t[end] == '/' || t[end] == '_' || t[end] == '^' || t[end] == '=' ||
                     t[end] == '<' || t[end] == '>' || (t[end] == '.' && end + 1 < n && ko_digit(t[end + 1])) ||
                     superscript_at(t, n, end, &c) || algebra_operator(&a, end, &key))) ||
        !korean_context(in))
        return 0;
    (void)snprintf(v, sizeof(v), "%.*s", (int)(end - p), t + p);
    return ko_finish(in, s, end, TTF_SEM_SYMBOL, v, "@algebra", "ko:algebra");
}

/* sin 30° 사인 삼십 도, cos x 코사인 엑스, tan²θ 탄젠트 제곱 세타: a trigonometric function name before
   its argument (a number, θ, a variable letter or a bracket) (2026-10-31, TN round 16; was 신, 코스, 탠).
   cosplay and a tan coat stay words. */
static size_t trig_function(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text, *name;
    size_t n = in->length, p = in->tokens[in->index].begin, q, r;
    char v[64], c;
    if (p + 3 > n || (p && ((t[p - 1] >= 'a' && t[p - 1] <= 'z') || (t[p - 1] >= 'A' && t[p - 1] <= 'Z'))))
        return 0;
    if (!memcmp(t + p, "sin", 3))
        name = resource(in->engine, "math_function", "sin", "사인");
    else if (!memcmp(t + p, "cos", 3))
        name = resource(in->engine, "math_function", "cos", "코사인");
    else if (!memcmp(t + p, "tan", 3))
        name = resource(in->engine, "math_function", "tan", "탄젠트");
    else
        return 0;
    q = p + 3;
    if (q < n && ((t[q] >= 'a' && t[q] <= 'z') || (t[q] >= 'A' && t[q] <= 'Z')))
        return 0;
    (void)snprintf(v, sizeof(v), "%s", name);
    if (superscript_at(t, n, q, &c) && (c == '2' || c == '3')) {
        (void)snprintf(v + strlen(v), sizeof(v) - strlen(v), " %s",
                       c == '2' ? resource(in->engine, "math_power", "2", "제곱") : resource(in->engine, "math_power", "3", "세제곱"));
        q += superscript_at(t, n, q, &c);
    }
    r = q < n && t[q] == ' ' ? q + 1 : q;
    if (!(r < n && (ko_digit(t[r]) || t[r] == '(' || (r + 1 < n && (unsigned char)t[r] == 0xCE && (unsigned char)t[r + 1] == 0xB8) ||
                    (algebra_var(t[r]) && !(r + 1 < n && ((t[r + 1] >= 'a' && t[r + 1] <= 'z') || (t[r + 1] >= 'A' && t[r + 1] <= 'Z')))))) ||
        !korean_context(in))
        return 0;
    return ko_finish(in, s, q, TTF_SEM_SYMBOL, v, "@word", "ko:trig-function");
}

/* log₂8 로그 이의 팔, log₁₀1000 로그 십의 천, log₂x 로그 이의 엑스: a logarithm with a subscript base
   (2026-10-31, TN round 16; was 로그₂팔). */
static size_t log_base(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q, r, used = 0;
    char base[8], arg[24], v[TTF_IR_VALUE_SIZE];
    ttf_buffer_t buf = {0};
    int ok;
    if (p + 6 > n || memcmp(t + p, "log", 3) || (p && ((t[p - 1] >= 'a' && t[p - 1] <= 'z') || (t[p - 1] >= 'A' && t[p - 1] <= 'Z'))))
        return 0;
    for (q = p + 3; q + 3 <= n && (unsigned char)t[q] == 0xE2 && (unsigned char)t[q + 1] == 0x82 &&
                    (unsigned char)t[q + 2] >= 0x80 && (unsigned char)t[q + 2] <= 0x89 && used + 1 < sizeof(base);
         q += 3)
        base[used++] = (char)('0' + (unsigned char)t[q + 2] - 0x80);
    base[used] = 0;
    if (!used || q >= n)
        return 0;
    if (ko_digit(t[q])) {
        for (r = q; r < n && ko_digit(t[r]) && r - q + 1 < sizeof(arg); ++r)
            ;
    } else if (algebra_var(t[q]))
        r = q + 1;
    else
        return 0;
    if ((r < n && ((t[r] >= 'a' && t[r] <= 'z') || (t[r] >= 'A' && t[r] <= 'Z') || ko_digit(t[r]) || t[r] == '.')) ||
        !korean_context(in))
        return 0;
    (void)snprintf(arg, sizeof(arg), "%.*s", (int)(r - q), t + q);
    ok = ttf_buffer_puts(&buf, resource(in->engine, "math_function", "log", "로그")) == TTF_OK && ttf_buffer_puts(&buf, " ") == TTF_OK &&
         cardinal(in->engine, base, &buf) == TTF_OK && ttf_buffer_puts(&buf, "의 ") == TTF_OK &&
         (ko_digit(arg[0]) ? cardinal(in->engine, arg, &buf) : expression_letter(in->engine, arg[0], &buf)) == TTF_OK &&
         buf.length < sizeof(v);
    if (ok)
        (void)snprintf(v, sizeof(v), "%.*s", (int)buf.length, buf.data);
    ttf_buffer_free(&buf);
    if (!ok)
        return 0;
    return ko_finish(in, s, r, TTF_SEM_SYMBOL, v, "@word", "ko:log-base");
}

/* Chemistry notation (2026-10-31, TN round 17): H₂O 에이치투오, C₆H₁₂O₆ 씨식스에이치트웰브오식스,
   Fe³⁺ 에프이쓰리플러스, SO₄²⁻ 에스오포투마이너스, 2H₂+O₂→2H₂O 이 에이치투 플러스 오투 화살표 이 에이치투오
   (was 에이치₂, 페이 세제곱⁺, 오투에서). Element symbols are spelled by letter, a subscript or charge
   count is read with the English number words classrooms use, a coefficient with Sino-Korean; one
   formula is written together as contract ko-styled-accent-v117 has it (에이치투오, 씨오투). A formula needs a subscript, a charge or an arrow; NaCl, B1, A4 stay as they
   are. ¹⁴C 탄소 십사, U-235 우라늄 이백삼십오: an isotope reads the element name and its mass
   number (a hyphen form only within the element's mass range, and for a light element only
   beside a nuclear word, so F-16 and B-52 stay names). */
static const struct {
    const char *symbol, *name;
    int z;
} chem_elements[] = {{"He", "헬륨", 2},      {"Li", "리튬", 3},      {"Be", "베릴륨", 4},     {"Ne", "네온", 10},
                     {"Na", "나트륨", 11},   {"Mg", "마그네슘", 12}, {"Al", "알루미늄", 13},  {"Si", "규소", 14},
                     {"Cl", "염소", 17},     {"Ar", "아르곤", 18},   {"Ca", "칼슘", 20},      {"Ti", "타이타늄", 22},
                     {"Cr", "크로뮴", 24},   {"Mn", "망가니즈", 25}, {"Fe", "철", 26},        {"Co", "코발트", 27},
                     {"Ni", "니켈", 28},     {"Cu", "구리", 29},     {"Zn", "아연", 30},      {"Br", "브로민", 35},
                     {"Kr", "크립톤", 36},   {"Sr", "스트론튬", 38}, {"Tc", "테크네튬", 43},  {"Ag", "은", 47},
                     {"Sn", "주석", 50},     {"Xe", "제논", 54},     {"Cs", "세슘", 55},      {"Ba", "바륨", 56},
                     {"Pt", "백금", 78},     {"Au", "금", 79},       {"Hg", "수은", 80},      {"Pb", "납", 82},
                     {"Rn", "라돈", 86},     {"Ra", "라듐", 88},     {"Th", "토륨", 90},      {"Pu", "플루토늄", 94},
                     {"Am", "아메리슘", 95}, {"H", "수소", 1},       {"B", "붕소", 5},        {"C", "탄소", 6},
                     {"N", "질소", 7},       {"O", "산소", 8},       {"F", "플루오린", 9},    {"P", "인", 15},
                     {"S", "황", 16},        {"K", "칼륨", 19},      {"I", "아이오딘", 53},   {"U", "우라늄", 92}};
static int chem_element_at(const char *t, size_t n, size_t p, size_t *width) {
    size_t i, w;
    if (p >= n || t[p] < 'A' || t[p] > 'Z')
        return -1;
    for (i = 0; i < sizeof(chem_elements) / sizeof(*chem_elements); ++i) {
        w = strlen(chem_elements[i].symbol);
        if (p + w <= n && !memcmp(t + p, chem_elements[i].symbol, w) && !(w == 1 && p + 1 < n && t[p + 1] >= 'a' && t[p + 1] <= 'z')) {
            *width = w;
            return (int)i;
        }
    }
    return -1;
}
/* A subscript digit (U+2080..2089) at p: its value, else -1. */
static int chem_subscript(const char *t, size_t n, size_t p) {
    return p + 3 <= n && (unsigned char)t[p] == 0xE2 && (unsigned char)t[p + 1] == 0x82 && (unsigned char)t[p + 2] >= 0x80 &&
                   (unsigned char)t[p + 2] <= 0x89
               ? (unsigned char)t[p + 2] - 0x80
               : -1;
}
static int chem_count(const ttf_engine_t *e, const char *digits, ttf_buffer_t *out) {
    static const char *const words[] = {"", "원", "투", "쓰리", "포", "파이브", "식스", "세븐", "에잇", "나인", "텐", "일레븐", "트웰브"};
    int value = atoi(digits);
    char key[16];
    if (value >= 1 && value <= 12 && strlen(digits) <= 2) {
        (void)snprintf(key, sizeof(key), "%d", value);
        return algebra_put(out, resource(e, "chem_count", key, words[value]));
    }
    return cardinal(e, digits, out) == TTF_OK;
}
static int chem_sep(ttf_buffer_t *out) { return !out->length || algebra_put(out, " "); }
/* One formula from p: [coefficient] (element [subscript])+ [charge]. Returns its end, or p. */
static size_t chem_formula(const ttf_engine_t *e, const char *t, size_t n, size_t p, ttf_buffer_t *out, int *marks) {
    size_t q = p, w, k;
    int elements = 0, d;
    char digits[8];
    if (!chem_sep(out))
        return p;
    if (q < n && ko_digit(t[q])) {
        for (k = 0; q < n && ko_digit(t[q]) && k + 1 < sizeof(digits); ++q)
            digits[k++] = t[q];
        digits[k] = 0;
        if (cardinal(e, digits, out) != TTF_OK || !algebra_put(out, " "))
            return p;
    }
    while (chem_element_at(t, n, q, &w) >= 0) {
        for (k = 0; k < w; ++k)
            if (expression_letter(e, t[q + k], out) != TTF_OK)
                return p;
        q += w;
        ++elements;
        for (k = 0; (d = chem_subscript(t, n, q)) >= 0 && k + 1 < sizeof(digits); q += 3)
            digits[k++] = (char)('0' + d);
        if (k) {
            digits[k] = 0;
            ++*marks;
            if (!chem_count(e, digits, out))
                return p;
        }
    }
    if (!elements)
        return p;
    {
        char c;
        size_t r = q, sw;
        for (k = 0; (sw = superscript_at(t, n, r, &c)) != 0 && c != '-' && k + 1 < sizeof(digits); r += sw)
            digits[k++] = c;
        digits[k] = 0;
        if (r + 3 <= n && (unsigned char)t[r] == 0xE2 && (unsigned char)t[r + 1] == 0x81 &&
            ((unsigned char)t[r + 2] == 0xBA || (unsigned char)t[r + 2] == 0xBB)) {
            if ((k && !chem_count(e, digits, out)) ||
                !algebra_put(out, (unsigned char)t[r + 2] == 0xBA ? resource(e, "chem_charge", "+", "플러스")
                                                                  : resource(e, "chem_charge", "-", "마이너스")))
                return p;
            ++*marks;
            q = r + 3;
        }
    }
    return q;
}
/* A formula or an equation of formulas joined by + and →. Returns its end, or p. */
static size_t chem_equation(const ttf_engine_t *e, const char *t, size_t n, size_t p, ttf_buffer_t *out, int *marks) {
    size_t q = chem_formula(e, t, n, p, out, marks), r;
    while (q > p && q < n) {
        const char *label;
        if (t[q] == '+' && !(q + 1 < n && t[q + 1] == ' '))
            label = resource(e, "chem_operator", "+", "플러스"), r = q + 1;
        else if (q + 3 <= n && !memcmp(t + q, "\xe2\x86\x92", 3))
            label = resource(e, "chem_operator", "→", "화살표"), r = q + 3, ++*marks;
        else
            break;
        {
            size_t mark = out->length, next;
            if (!chem_sep(out) || !algebra_put(out, label) || (next = chem_formula(e, t, n, r, out, marks)) == r) {
                out->length = mark;
                break;
            }
            q = next;
        }
    }
    return q;
}
static ttf_status_t chem_read(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    ttf_buffer_t buf = {0};
    size_t n = strlen(value);
    int marks = 0, ok = chem_equation(e, value, n, 0, &buf, &marks) == n &&
                        ttf_buffer_append(out, buf.data, buf.length) == TTF_OK;
    ttf_buffer_free(&buf);
    return ok ? TTF_OK : TTF_EINVAL;
}
static size_t chem_formula_rule(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, end, w;
    ttf_buffer_t check = {0};
    char v[TTF_IR_VALUE_SIZE];
    int marks = 0;
    if (p >= n || !(ko_digit(t[p]) || (t[p] >= 'A' && t[p] <= 'Z')) ||
        (p && ((t[p - 1] >= 'a' && t[p - 1] <= 'z') || (t[p - 1] >= 'A' && t[p - 1] <= 'Z') || ko_digit(t[p - 1]) ||
               t[p - 1] == '.' || t[p - 1] == '-' || t[p - 1] == '+')))
        return 0;
    if (ko_digit(t[p])) {
        size_t q = p;
        while (q < n && ko_digit(t[q]))
            ++q;
        if (chem_element_at(t, n, q, &w) < 0)
            return 0;
    }
    end = chem_equation(in->engine, t, n, p, &check, &marks);
    ttf_buffer_free(&check);
    /* 소금의 주성분인 NaCl 엔에이씨엘: two or more elements without marks are a formula only in a
       chemistry sentence */
    if (!marks && end > p) {
        static const char *const cues[] = {"화학", "이온", "분자", "결합", "성분", "용액", "반응", "원소", "화합물"};
        size_t i, q, elements = 0, mixed = 0;
        /* ...with a two-letter symbol among them (KBO, NC, SOS are initialisms) */
        for (q = p; q < end && chem_element_at(t, n, q, &w) >= 0; q += w)
            ++elements, mixed += w == 2;
        for (i = 0; elements >= 2 && mixed && q == end && i < sizeof(cues) / sizeof(*cues); ++i)
            if (strstr(t, cues[i]))
                marks = 1;
    }
    if (end == p || !marks || end - p >= sizeof(v) ||
        (end < n && ((t[end] >= 'a' && t[end] <= 'z') || (t[end] >= 'A' && t[end] <= 'Z') || ko_digit(t[end]) ||
                     chem_subscript(t, n, end) >= 0)) ||
        !korean_context(in))
        return 0;
    (void)snprintf(v, sizeof(v), "%.*s", (int)(end - p), t + p);
    return ko_finish(in, s, end, TTF_SEM_SYMBOL, v, "@chem", "ko:chem-formula");
}
static int chem_nuclear_cue(const ttf_pack_input_t *in) {
    static const char *const cues[] = {"동위", "방사", "반감기", "원자", "연대", "감마선", "핵", "피폭", "검출", "측정법", "원소"};
    size_t i;
    for (i = 0; i < sizeof(cues) / sizeof(*cues); ++i)
        if (strstr(in->text, cues[i]))
            return 1;
    return 0;
}
static size_t chem_isotope(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, q = p, w, sw, k = 0, end;
    char mass[8], c, v[TTF_IR_VALUE_SIZE];
    int el, a;
    ttf_buffer_t buf = {0};
    if (p >= n || (p && ((t[p - 1] >= 'a' && t[p - 1] <= 'z') || (t[p - 1] >= 'A' && t[p - 1] <= 'Z') || ko_digit(t[p - 1]))))
        return 0;
    /* ¹⁴C: a mass number written before the symbol */
    while ((sw = superscript_at(t, n, q, &c)) != 0 && c != '-' && k + 1 < sizeof(mass)) {
        mass[k++] = c;
        q += sw;
    }
    mass[k] = 0;
    if (k) {
        if ((el = chem_element_at(t, n, q, &w)) < 0)
            return 0;
        end = q + w;
    } else {
        /* U-235, Co-60: the symbol, a hyphen and the mass number */
        if ((el = chem_element_at(t, n, q, &w)) < 0 || q + w + 1 >= n || t[q + w] != '-')
            return 0;
        for (end = q + w + 1; end < n && ko_digit(t[end]) && k + 1 < sizeof(mass); ++end)
            mass[k++] = t[end];
        mass[k] = 0;
        a = atoi(mass);
        if (!k || k > 3 || a < chem_elements[el].z || a * 10 > chem_elements[el].z * 26 + 40 ||
            (chem_elements[el].z < 20 && !chem_nuclear_cue(in)))
            return 0;
    }
    if ((end < n && ((t[end] >= 'a' && t[end] <= 'z') || (t[end] >= 'A' && t[end] <= 'Z') || ko_digit(t[end]) ||
                     t[end] == '-' || chem_subscript(t, n, end) >= 0)) ||
        !korean_context(in))
        return 0;
    if (ttf_buffer_puts(&buf, chem_elements[el].name) != TTF_OK || ttf_buffer_puts(&buf, " ") != TTF_OK ||
        cardinal(in->engine, mass, &buf) != TTF_OK || buf.length >= sizeof(v)) {
        ttf_buffer_free(&buf);
        return 0;
    }
    (void)snprintf(v, sizeof(v), "%.*s", (int)buf.length, buf.data);
    ttf_buffer_free(&buf);
    return ko_finish(in, s, end, TTF_SEM_SYMBOL, v, "@word", "ko:chem-isotope");
}

/* Wi-Fi 6E 와이파이 식스 이, Thunderbolt 4 선더볼트 포: the generation number of these standards is read
   in English (owner set ko-KR-0099, 0462, 0967; 2026-10-31, TN round 19; was 와이파이 육 이, 선더볼트 사). */
static size_t english_generation(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    static const struct {
        const char *written, *key, *reading;
    } names[] = {{"Wi-Fi", "wifi", "와이파이"}, {"WiFi", "wifi", "와이파이"}, {"Thunderbolt", "thunderbolt", "선더볼트"}};
    static const char *const numbers[] = {"", "원", "투", "쓰리", "포", "파이브", "식스", "세븐", "에잇", "나인", "텐"};
    const char *t = in->text;
    size_t n = in->length, p = in->tokens[in->index].begin, i, w, q, r;
    char v[TTF_IR_VALUE_SIZE];
    ttf_buffer_t buf = {0};
    int value, ok;
    if (p >= n || (p && ((t[p - 1] >= 'a' && t[p - 1] <= 'z') || (t[p - 1] >= 'A' && t[p - 1] <= 'Z'))))
        return 0;
    for (i = 0; i < sizeof(names) / sizeof(*names); ++i) {
        w = strlen(names[i].written);
        if (p + w + 1 < n && !memcmp(t + p, names[i].written, w) && t[p + w] == ' ' && ko_digit(t[p + w + 1]))
            break;
    }
    if (i == sizeof(names) / sizeof(*names))
        return 0;
    for (q = r = p + w + 1; r < n && ko_digit(t[r]) && r - q < 2; ++r)
        ;
    value = atoi(t + q);
    if (value < 1 || value > 10 || (r < n && ko_digit(t[r])))
        return 0;
    if (r < n && t[r] >= 'A' && t[r] <= 'Z' && !(r + 1 < n && ((t[r + 1] >= 'a' && t[r + 1] <= 'z') || (t[r + 1] >= 'A' && t[r + 1] <= 'Z') || ko_digit(t[r + 1]))))
        ++r;
    if ((r < n && ((t[r] >= 'a' && t[r] <= 'z') || (t[r] >= 'A' && t[r] <= 'Z') || ko_digit(t[r]) || (t[r] == '.' && r + 1 < n && ko_digit(t[r + 1])))) ||
        !korean_context(in))
        return 0;
    ok = ttf_buffer_puts(&buf, resource(in->engine, "english_generation", names[i].key, names[i].reading)) == TTF_OK &&
         ttf_buffer_puts(&buf, " ") == TTF_OK && ttf_buffer_puts(&buf, numbers[value]) == TTF_OK;
    if (ok && t[r - 1] >= 'A' && t[r - 1] <= 'Z')
        ok = ttf_buffer_puts(&buf, " ") == TTF_OK && expression_letter(in->engine, t[r - 1], &buf) == TTF_OK;
    ok = ok && buf.length < sizeof(v);
    if (ok)
        (void)snprintf(v, sizeof(v), "%.*s", (int)buf.length, buf.data);
    ttf_buffer_free(&buf);
    if (!ok)
        return 0;
    return ko_finish(in, s, r, TTF_SEM_SYMBOL, v, "@word", "ko:english-generation");
}

static size_t classify_grammar(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *text = in->text;
    size_t n = in->length, begin = in->tokens[in->index].begin, p, q;
    unsigned year, month, day;
    size_t identifier = below_zero_degrees(in, s);
    if (identifier)
        return identifier;
    if ((identifier = official_number(in, s)) != 0 || (identifier = count_per_time(in, s)) != 0 || (identifier = field_mark(in, s)) != 0 || (identifier = screen_inch(in, s)) != 0 || (identifier = algebra(in, s)) != 0 || (identifier = english_generation(in, s)) != 0 || (identifier = chem_isotope(in, s)) != 0 || (identifier = chem_formula_rule(in, s)) != 0 || (identifier = trig_function(in, s)) != 0 || (identifier = log_base(in, s)) != 0 || (identifier = number_mark(in, s)) != 0 || (identifier = day_countdown(in, s)) != 0 || (identifier = times_or_quarter(in, s)) != 0 || (identifier = promo_plus(in, s)) != 0 || (identifier = file_dot(in, s)) != 0 || (identifier = range_month_day(in, s)) != 0 || (identifier = weekday_range(in, s)) != 0 || (identifier = letter_unit(in, s)) != 0 || (identifier = name_version(in, s)) != 0 || (identifier = geo_coordinate(in, s)) != 0 || (identifier = colon_score(in, s)) != 0 || (identifier = price_per_unit(in, s)) != 0 ||
        (identifier = currency_pair(in, s)) != 0 || (identifier = short_dotted_date(in, s)) != 0 || (identifier = quarter(in, s)) != 0 ||
        (identifier = owner_forms(in, s)) != 0 || (identifier = owner_forms5(in, s)) != 0 ||
        (identifier = chemical_formula(in, s)) != 0 || (identifier = subscript_index(in, s)) != 0 ||
        (identifier = letters_decimal(in, s)) != 0 || (identifier = court_case(in, s)) != 0 ||
        (identifier = superscript_expression(in, s)) != 0 || (identifier = trailing_power(in, s)) != 0 ||
        (identifier = approximate_unit(in, s)) != 0)
        return identifier;
    identifier = ttf_ko_classify_basement(in, s);
    if (identifier)
        return identifier;
    identifier = ttf_ko_classify_floor(in, s);
    if (identifier)
        return identifier;
    if (in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
        identifier = ttf_ko_classify_lot_number(in, s);
        if (identifier)
            return identifier;
        identifier = ttf_ko_classify_route_number(in, s);
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
    p = compound_amount(in, s);
    if (p)
        written_scale(in, s);
    return p;
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

/* We need 5개, Thank you for coming 3명, 1,000원: a number written against a Hangul
   counter or particle belongs to the Korean grammar, whatever the clause before it
   (2026-10-28; was the English run's context, five 개). Only a run with Latin words
   needs it; a Korean run keeps its scores. */
static int attached_hangul_quantity(const ttf_pack_input_t *in, size_t i, size_t begin, size_t end) {
    size_t k;
    int latin = 0;
    if (in->tokens[i].script != TTF_SCRIPT_DIGIT)
        return 0;
    for (k = begin; k < end && !latin; ++k)
        latin = in->tokens[k].script == TTF_SCRIPT_LATIN;
    if (!latin)
        return 0;
    while (i + 1 < in->token_count && in->tokens[i].end == in->tokens[i + 1].begin) {
        const ttf_token_t *next = &in->tokens[i + 1];
        char c = in->text[next->begin];
        if (next->script == TTF_SCRIPT_HANGUL)
            return 1;
        if (next->script != TTF_SCRIPT_DIGIT &&
            !(next->end == next->begin + 1 && (c == ',' || c == '.') && i + 2 < in->token_count &&
              in->tokens[i + 2].script == TTF_SCRIPT_DIGIT))
            return 0;
        ++i;
    }
    return 0;
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
        if (attached_hangul_quantity(in, i, begin, end)) {
            scores->scores[i] = scores->base + script_bonus;
            continue;
        }
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
    if (attached_hangul_quantity(in, in->index, begin, end))
        return score + resource_integer(in->engine, "context", "script_weight", 0, 1000, 1000);
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
    {
        size_t letters = 0, axes = 0;
        for (i = 0; i < count; ++i) {
            letters += !strncmp(phones[i], "letter.", 7);
            axes += !strncmp(phones[i], "letter.", 7) && strlen(phones[i]) == 8 && strchr("XYZ", phones[i][7]);
        }
        /* DMZ 디엠지, MZ 엠지: a letter may have another name inside a run of two or more
           letters (phone_letter letter.Z.run; 2026-10-28, was 디엠제트). A lone Z stays 제트,
           and so do axis names written together (XYZ 엑스와이제트, XZ). */
        if (letters >= 2 && axes < letters)
            for (i = 0; i < count; ++i)
                if (!strncmp(phones[i], "letter.", 7) && strlen(phones[i]) == 8) {
                    char run[16];
                    snprintf(run, sizeof(run), "%s.run", phones[i]);
                    if (ttf_resource_get(e, "phone_letter", "ko-KR", run, NULL))
                        memcpy(phones[i], run, strlen(run) + 1);
                }
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
