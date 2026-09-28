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
static ttf_status_t phone_digits(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    for (; *value; ++value) {
        const char *label;
        if (*value == '-') {
            label = ttf_resource_get(e, "phone_render", "ko-KR", "separator", NULL);
            if (!label)
                label = " "; /* Legacy phone group separator. */
        } else if (*value >= '0' && *value <= '9') {
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
    for (; *value; ++value) {
        if (*value >= '0' && *value <= '9')
            TRY(digit(e, (unsigned)(*value - '0'), telephone, out));
        else if (*value == '-' || *value == ' ')
            TRY(ttf_buffer_puts(out, " "));
        else if (*value == '.')
            TRY(ttf_buffer_puts(out, resource(e, "number", "decimal", " 점 ")));
    }
    return TTF_OK;
}

static ttf_status_t sino_group(const ttf_engine_t *e, unsigned value, ttf_buffer_t *out) {
    static const unsigned divisors[] = {1000, 100, 10, 1};
    static const char *const units[] = {"천", "백", "십", ""};
    size_t i;
    for (i = 0; i < 4; ++i) {
        unsigned d = value / divisors[i];
        value %= divisors[i];
        if (!d)
            continue;
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
            int separate = groups > 1 &&
                !strcmp(resource(e, "number_spacing", "group", "compact"), "space");
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
        if (strcmp(s->unit, "financial_identifier") == 0)
            return ttf_ko_render_identifier(e, s->value, out);
        if (!strcmp(s->unit, "leading_zero")) {
            const char *p = s->value;
            const char *zero = resource(e, "number", "leading_zero_zero",
                                        resource(e, "digit", "0", "영"));
            for (; *p; ++p) {
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
        return quantity(e, s->value, resource(e, "unit", s->unit, s->unit),
                        !strcmp(s->secondary, "frequency") ||
                            (strcmp(s->secondary, "identifier") &&
                             strcmp(resource(e, "counter", s->unit, "sino"), "native") == 0),
                        out);
    }
    case TTF_SEM_CURRENCY:
        return quantity(e, s->value, resource(e, "currency", s->unit, s->unit), 0, out);
    case TTF_SEM_PERCENT:
        return quantity(e, s->value, resource(e, "unit", "%", "퍼센트"), 0, out);
    case TTF_SEM_MEASURE:
        TRY(ttf_buffer_puts(out, resource(e, "quantity_prefix", s->unit, "")));
        return quantity(e, s->value, resource(e, "unit", s->unit, s->unit), 0, out);
    case TTF_SEM_RANGE: {
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
        if (s->unit[0] && ttf_resource_get(e, "range_repeat_unit", "ko-KR", s->unit, NULL) &&
            !strcmp(ttf_resource_get(e, "range_repeat_unit", "ko-KR", s->unit, NULL), "0"))
            repeat_unit = 0;
        if (repeat_unit || !s->unit[0] || !strcmp(s->unit, "time"))
            TRY(verbalize(e, &endpoint, surface, length, out));
        else
            TRY(cardinal(e, s->value, out));
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
    if (!decimal_parse(text, n, &p, &pending))
        valid = 0;
    end = p;
    for (;;) {
        q = skip_spaces(text, n, p);
        width = scale_at(in, q, &power);
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
            if (!quantity_boundary(in, end)) {
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
            size_t scale_start = skip_spaces(in->text, in->length, second.input_end);
            if (scale_at(in, scale_start, &ignored_power)) {
                size_t guard_end = scale_start, guard_index = next, suffix_width;
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
                if (t->script == TTF_SCRIPT_HANGUL && t->begin == p)
                    cue = band_cue(in, "counter_band_after", t->begin, t->end);
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
    identifier = ttf_ko_classify_identifier(in, s);
    if (identifier)
        return identifier;
    if (in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
        identifier = ttf_ko_classify_alnum_identifier(in, s);
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
    int pending = -1, glide = 0;
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
                (i + 2 == count || (strcmp(phones[i + 2], "W") && strcmp(phones[i + 2], "Y")))) {
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
            glide = 0;
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
