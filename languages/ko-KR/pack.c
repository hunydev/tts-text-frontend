/* Korean reading policy and phonetic realization live in this language pack.
 * The core only dispatches through the versioned language-pack interface. */
#include "tts_text_frontend/ttf_language_pack.h"
#include "../context_run.h"
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
    } else if (*p == '+')
        ++p;
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
            start = 0;
            for (i = groups; i > 0; --i) {
                size_t width = i == groups ? (n - 1) % 4 + 1 : 4, j;
                unsigned group = 0;
                for (j = 0; j < width; ++j)
                    group = group * 10 + (unsigned)(integer[start++] - '0');
                if (!group)
                    continue;
                if (!(group == 1 && i == 2))
                    TRY(sino_group(e, group, out));
                if (i > 1) {
                    char key[16];
                    (void)snprintf(key, sizeof(key), "10^%u", (unsigned)((i - 1) * 4));
                    TRY(ttf_buffer_puts(out, resource(e, "magnitude", key, powers[i - 1])));
                }
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
                              size_t length, ttf_buffer_t *out) {
    switch (s->type) {
    case TTF_SEM_NUMBER:
    case TTF_SEM_CARDINAL:
        return cardinal(e, s->value, out);
    case TTF_SEM_ORDINAL:
        TRY(ttf_buffer_puts(out, resource(e, "number", "ordinal_prefix", "제")));
        return cardinal(e, s->value, out);
    case TTF_SEM_DIGIT_SEQUENCE:
        return digits(e, s->value, 0, out);
    case TTF_SEM_PHONE:
        return digits(e, s->value, 1, out);
    case TTF_SEM_COUNTER: {
        const char *month = s->value;
        if (*month == '+')
            ++month;
        while (*month == '0' && month[1])
            ++month;
        if (strcmp(s->unit, "월") == 0 && (!strcmp(month, "6") || !strcmp(month, "10"))) {
            TRY(ttf_buffer_puts(out,
                                resource(e, "month", month, !strcmp(month, "6") ? "유" : "시")));
            return ttf_buffer_puts(out, " 월");
        }
        return quantity(e, s->value, resource(e, "unit", s->unit, s->unit),
                        strcmp(resource(e, "counter", s->unit, "sino"), "native") == 0, out);
    }
    case TTF_SEM_CURRENCY:
        return quantity(e, s->value, resource(e, "currency", s->unit, s->unit), 0, out);
    case TTF_SEM_PERCENT:
        return quantity(e, s->value, resource(e, "unit", "%", "퍼센트"), 0, out);
    case TTF_SEM_MEASURE:
        return quantity(e, s->value, resource(e, "unit", s->unit, s->unit), 0, out);
    case TTF_SEM_RANGE: {
        int digit_mode = strcmp(resource(e, "policy", "range_mode", "cardinal"), "digits") == 0;
        if (digit_mode)
            TRY(digits(e, s->value, 0, out));
        else
            TRY(cardinal(e, s->value, out));
        TRY(ttf_buffer_puts(out, resource(e, "policy", "range_separator", " 다시 ")));
        return digit_mode ? digits(e, s->secondary, 0, out) : cardinal(e, s->secondary, out);
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
        unsigned year, month, day;
        char value[16];
        if (sscanf(s->value, "%u-%u-%u", &year, &month, &day) != 3)
            return TTF_EINVAL;
        (void)snprintf(value, sizeof(value), "%u", year);
        TRY(quantity(e, value, resource(e, "unit", "year", "년"), 0, out));
        TRY(ttf_buffer_puts(out, " "));
        if (month == 6 || month == 10) {
            TRY(ttf_buffer_puts(
                out, resource(e, "month", month == 6 ? "6" : "10", month == 6 ? "유" : "시")));
            TRY(ttf_buffer_puts(out, " "));
            TRY(ttf_buffer_puts(out, resource(e, "unit", "month", "월")));
        } else {
            (void)snprintf(value, sizeof(value), "%u", month);
            TRY(quantity(e, value, resource(e, "unit", "month", "월"), 0, out));
        }
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
        (void)snprintf(value, sizeof(value), "%u", hour);
        TRY(quantity(e, value, resource(e, "unit", "hour", "시"), 1, out));
        TRY(ttf_buffer_puts(out, " "));
        (void)snprintf(value, sizeof(value), "%u", minute);
        TRY(quantity(e, value, resource(e, "unit", "minute", "분"), 0, out));
        if (fields == 3) {
            TRY(ttf_buffer_puts(out, " "));
            (void)snprintf(value, sizeof(value), "%u", second);
            TRY(quantity(e, value, resource(e, "unit", "second", "초"), 0, out));
        }
        return TTF_OK;
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
            return ttf_resource_get(in->engine, "particle", "ko-KR", key, NULL) != NULL ||
                   ttf_resource_get(in->engine, "quantity_tail", "ko-KR", key, NULL) != NULL;
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
    ttf_semantic_type_t type = TTF_SEM_CARDINAL;
    decimal_zero(&total);
    decimal_zero(&group);
    prefix = quantity_resource_at(in, "currency_prefix", p, &width);
    if (prefix)
        p = skip_spaces(text, n, p + width);
    if (p < n && (text[p] == '-' || text[p] == '+'))
        negative = text[p++] == '-';
    if (p >= n || text[p] < '0' || text[p] > '9')
        return 0;
    valid = decimal_parse(text, n, &p, &pending);
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

/* Pack-specific grammar creates language-neutral IR. */
static size_t classify(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *text = in->text;
    size_t n = in->length, begin = in->tokens[in->index].begin, p, q;
    unsigned year, month, day;
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

static int context_score(const ttf_pack_input_t *in) {
    size_t i, hangul = 0, begin, end;
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
    if (in->index >= in->token_count)
        return score;
    if (in->tokens[in->index].script == TTF_SCRIPT_HANGUL)
        return score + resource_integer(in->engine, "context", "script_weight", 0, 1000, 1000);
    context_run_bounds(in, &begin, &end);
    /* A particle scopes over the full preceding foreign noun phrase, including
       spaces and conjunctions. A stronger source-clause signal can still win. */
    if (begin < end && end < in->token_count && in->tokens[end - 1].end == in->tokens[end].begin &&
        is_particle(in, &in->tokens[end]))
        score += resource_integer(in->engine, "context", "particle_weight", 0, 1000, 200);
    return score;
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

static ttf_status_t render_phones(const ttf_engine_t *e, const ttf_pronunciation_t *p,
                                  ttf_buffer_t *out) {
    char phones[128][24];
    size_t count = 0, pos = 0, i;
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
        int v = vowel(e, phone), c = onset(e, phone);
        const char *letter = ttf_resource_get(e, "phone_letter", "ko-KR", phone, NULL);
        if (letter) {
            if (pending >= 0)
                TRY(put_syllable(out, pending, 18, 0));
            pending = -1;
            TRY(ttf_buffer_puts(out, letter));
            continue;
        }
        if (strcmp(phone, "W") == 0 || strcmp(phone, "Y") == 0) {
            glide = strcmp(phone, "W") == 0 ? 1 : 2;
            continue;
        }
        if (v >= 0) {
            int cod = 0;
            if (glide == 1) {
                if (v == 0)
                    v = 9;
                else if (v == 4 || v == 6)
                    v = 14;
                else if (v == 1)
                    v = 10;
                else if (v == 20)
                    v = 16;
            } else if (glide == 2) {
                if (v == 0)
                    v = 2;
                else if (v == 4)
                    v = 6;
                else if (v == 8)
                    v = 12;
                else if (v == 13)
                    v = 17;
            }
            /* Nasals/laterals close syllables before a consonant or word end. */
            if (i + 1 < count && (i + 2 == count || vowel(e, phones[i + 2]) < 0)) {
                int close = resource_integer(e, "phone_coda", phones[i + 1], 0, 27, -1);
                if (close >= 0) {
                    cod = close;
                    ++i;
                }
            }
            TRY(put_syllable(out, pending >= 0 ? pending : 11, v, cod));
            if (strcmp(phone, "AY") == 0 || strcmp(phone, "EY") == 0 || strcmp(phone, "OY") == 0)
                TRY(put_syllable(out, 11, 20, 0));
            else if (strcmp(phone, "AW") == 0)
                TRY(put_syllable(out, 11, 13, 0));
            pending = -1;
            glide = 0;
        } else if (c >= 0) {
            if (strcmp(phone, "R") == 0 && i + 1 == count)
                continue;
            if (pending >= 0)
                TRY(put_syllable(out, pending, 18, 0));
            pending = c;
        } else
            return TTF_EINVAL;
    }
    if (pending >= 0)
        TRY(put_syllable(out, pending, 18, 0));
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
            i += width;
        } else if (word[i] >= '0' && word[i] <= '9') {
            TRY(digit(e, (unsigned)(word[i] - '0'), 0, out));
            ++i;
        } else {
            /* A letter-name fallback always speaks unfamiliar Latin letters. */
            char letter[16] = "letter.A";
            const char *name;
            unsigned char c = (unsigned char)word[i];
            letter[7] = (char)(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c);
            name = ttf_resource_get(e, "phone_letter", "ko-KR", letter, NULL);
            if (name)
                TRY(ttf_buffer_puts(out, name));
            else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                TRY(ttf_buffer_puts(out, "으"));
            else if (c < 128)
                TRY(ttf_buffer_append(out, word + i, 1));
            /* Accented Latin codepoints use a speakable escape when not mapped. */
            else if ((c & 0xc0) != 0x80)
                TRY(ttf_buffer_puts(out, "으"));
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
