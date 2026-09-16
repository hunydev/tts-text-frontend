#include "../core/internal.h"

/* Syntax is universal; suffix and prefix inventories are resource data. A match
   may finish inside a script token (3개입니다); input_end retains that boundary. */
static int digit(char c) {
    return c >= '0' && c <= '9';
}
static size_t finish(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t end,
                     ttf_semantic_type_t type, const char *rule) {
    size_t i = in->index;
    s->type = type;
    s->input_end = end;
    (void)snprintf(s->rule, sizeof(s->rule), "%s", rule);
    while (i < in->token_count && in->tokens[i].begin < end)
        ++i;
    return i - in->index;
}
static int copy_value(char *dst, size_t cap, const char *s, size_t n) {
    if (n >= cap)
        return 0;
    memcpy(dst, s, n);
    dst[n] = '\0';
    return 1;
}
static size_t integer(const char *s, size_t n, size_t p) {
    while (p < n && digit(s[p]))
        ++p;
    return p;
}
/* Expand a bounded decimal exponent using decimal digits only. The IR stores
   exact base-ten values; binary floating point must not round financial input. */
static int expand_exponent(char *value, int exponent) {
    char digits[TTF_IR_VALUE_SIZE], result[TTF_IR_VALUE_SIZE];
    size_t i = 0, count = 0, fractional = 0, out = 0, start = 0;
    int point, decimal = 0;
    char sign = 0;
    if (value[i] == '+' || value[i] == '-')
        sign = value[i++];
    for (; value[i]; ++i) {
        if (value[i] == '.') {
            decimal = 1;
        } else {
            digits[count++] = value[i];
            if (decimal)
                ++fractional;
        }
    }
    point = (int)count - (int)fractional + exponent;
    while (start + 1 < count && digits[start] == '0') {
        ++start;
        --point;
    }
    count -= start;
    memmove(digits, digits + start, count);
    if (count == 1 && digits[0] == '0') {
        point = 1;
    } else {
        while (count > 1 && (int)count > point && digits[count - 1] == '0')
            --count;
    }
    if (sign)
        result[out++] = sign;
    if (point <= 0) {
        size_t zeros = (size_t)(-point);
        if (out + 2 + zeros + count >= sizeof(result))
            return 0;
        result[out++] = '0';
        result[out++] = '.';
        memset(result + out, '0', zeros);
        out += zeros;
        memcpy(result + out, digits, count);
        out += count;
    } else {
        size_t whole = (size_t)point;
        size_t required = whole < count ? count + 1 : whole;
        if (out + required >= sizeof(result))
            return 0;
        for (i = 0; i < whole; ++i)
            result[out++] = i < count ? digits[i] : '0';
        if (whole < count) {
            result[out++] = '.';
            memcpy(result + out, digits + whole, count - whole);
            out += count - whole;
        }
    }
    result[out] = '\0';
    memcpy(value, result, out + 1);
    return 1;
}
static size_t number(const char *s, size_t n, size_t p, char *value) {
    size_t start = p, k = 0, first;
    if (p < n && (s[p] == '-' || s[p] == '+')) {
        value[k++] = s[p++];
    }
    first = p;
    p = integer(s, n, p);
    if (p - start >= TTF_IR_VALUE_SIZE)
        return start;
    if (p == first) {
        if (p + 1 >= n || s[p] != '.' || !digit(s[p + 1]))
            return start;
        value[k++] = '0';
    } else {
        memcpy(value + k, s + first, p - first);
        k += p - first;
    }
    if (p < n && s[p] == ',' && p + 1 < n && digit(s[p + 1]) && p - first > 3)
        return start;
    while (p < n && s[p] == ',' && p + 1 < n && digit(s[p + 1])) {
        size_t q = integer(s, n, p + 1);
        if (q - p != 4 || k + 3 >= TTF_IR_VALUE_SIZE)
            return start;
        memcpy(value + k, s + p + 1, 3);
        k += 3;
        p = q;
    }
    if (p + 1 < n && s[p] == '.' && digit(s[p + 1])) {
        size_t q = integer(s, n, p + 1);
        if (k + q - p >= TTF_IR_VALUE_SIZE)
            return start;
        memcpy(value + k, s + p, q - p);
        k += q - p;
        p = q;
    }
    value[k] = '\0';
    if (p < n && (s[p] == 'e' || s[p] == 'E')) {
        unsigned exponent = 0;
        int negative = 0;
        size_t first_digit;
        ++p;
        if (p < n && (s[p] == '+' || s[p] == '-'))
            negative = s[p++] == '-';
        first_digit = p;
        while (p < n && digit(s[p])) {
            /* A 96-byte mantissa and result cannot require a larger shift. */
            if (exponent > TTF_IR_VALUE_SIZE ||
                exponent * 10u + (unsigned)(s[p] - '0') > TTF_IR_VALUE_SIZE)
                return start;
            exponent = exponent * 10u + (unsigned)(s[p++] - '0');
        }
        if (p == first_digit || !expand_exponent(value, negative ? -(int)exponent : (int)exponent))
            return start;
    }
    return p;
}
static unsigned small_uint(const char *s, size_t n) {
    unsigned v = 0;
    size_t i;
    for (i = 0; i < n; ++i)
        v = v * 10u + (unsigned)(s[i] - '0');
    return v;
}
static size_t skip_space(const char *s, size_t n, size_t p) {
    while (p < n && (s[p] == ' ' || s[p] == '\t'))
        ++p;
    return p;
}
static int resource_prefix(const ttf_pack_input_t *in, const char *kind, size_t p,
                           const char **value, size_t *end, ttf_resource_layer_t *layer) {
    char key[64];
    size_t n = in->length - p;
    if (n >= sizeof(key))
        n = sizeof(key) - 1;
    for (; n; --n) {
        memcpy(key, in->text + p, n);
        key[n] = '\0';
        *value = ttf_resource_get(in->engine, kind, "*", key, layer);
        if (*value) {
            *end = p + n;
            return 1;
        }
    }
    return 0;
}
static int ascii_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
/* Invalid numeric-looking objects are kept whole: emitting a cardinal for their
   prefix silently changes identifiers and malformed amounts. Punctuation at the
   end is outside the object so sentence metadata is retained. */
static size_t atom_end(const char *s, size_t n, size_t p) {
    size_t end = p;
    while (end < n &&
           (digit(s[end]) || ascii_letter(s[end]) || s[end] == '.' || s[end] == ',' ||
            s[end] == '-' || s[end] == '+' || s[end] == ':' || s[end] == '/' || s[end] == '%'))
        ++end;
    while (end > p && (s[end - 1] == '.' || s[end - 1] == ',' || s[end - 1] == ':'))
        --end;
    return end;
}
static int structured_tail(const char *s, size_t n, size_t p) {
    size_t next = p;
    if (p >= n)
        return 0;
    while (next < n && (s[next] == '.' || s[next] == ',' || s[next] == '-'))
        ++next;
    if (next > p + 1 && next < n && digit(s[next]))
        return 1;
    return ascii_letter(s[p]) || digit(s[p]) || s[p] == ':' || s[p] == '/' ||
           (s[p] == '%' && p + 1 < n &&
            (ascii_letter(s[p + 1]) || digit(s[p + 1]) || s[p + 1] == '%')) ||
           ((s[p] == '.' || s[p] == ',' || s[p] == '-') && p + 1 < n && digit(s[p + 1]));
}
static size_t preserve(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t end,
                       const char *rule) {
    out->value[0] = out->secondary[0] = out->unit[0] = '\0';
    return finish(in, out, end, TTF_SEM_TEXT, rule);
}
/* A source pack can constrain ordinal spellings with resource tails (longest
   decimal tail first). No source-language suffix rules live in the core. */
static int valid_ordinal(const ttf_pack_input_t *in, const char *language, const char *value,
                         const char *suffix, size_t suffix_length) {
    size_t i, n = strlen(value);
    if (!n || value[0] == '+' || value[0] == '-' || strchr(value, '.'))
        return 0;
    for (i = 0; i < n; ++i) {
        const char *expected =
            ttf_resource_get(in->engine, "ordinal_tail", language, value + i, NULL);
        if (expected)
            return strlen(expected) == suffix_length && !memcmp(expected, suffix, suffix_length);
    }
    return 1;
}
static int ascii_web(char c) {
    unsigned char u = (unsigned char)c;
    return u > 32 && u < 127 && c != '<' && c != '>' && c != '"';
}
/* Separator spellings are data; arithmetic and unit identity are universal. */
static size_t quantity_separator(const ttf_pack_input_t *in, size_t position,
                                 ttf_resource_layer_t *matched_layer) {
    const char *value;
    size_t end;
    ttf_resource_layer_t layer;
    if (position < in->length &&
        resource_prefix(in, "quantity_range_separator", position, &value, &end, &layer) &&
        !strcmp(value, "1")) {
        if (matched_layer)
            *matched_layer = layer;
        return end;
    }
    return 0;
}
static int quantity_tail(const ttf_pack_input_t *in, size_t end, int allow_range) {
    return !structured_tail(in->text, in->length, end) ||
           (allow_range && quantity_separator(in, skip_space(in->text, in->length, end), NULL));
}
/* Trailing quantity units retain their semantic identity in the IR. In
   particular percentage points and basis points are not converted to percent. */
static int quantity_suffix(const ttf_pack_input_t *in, size_t p, ttf_semantic_t *out, size_t *end,
                           ttf_semantic_type_t *type, int allow_range) {
    const char *value;
    ttf_resource_layer_t layer;
    if (resource_prefix(in, "numeric_suffix", p, &value, end, &layer) &&
        quantity_tail(in, *end, allow_range)) {
        const char *colon = strchr(value, ':');
        if (!strncmp(value, "COUNTER:", 8))
            *type = TTF_SEM_COUNTER;
        else if (!strncmp(value, "CURRENCY:", 9))
            *type = TTF_SEM_CURRENCY;
        else if (!strncmp(value, "MEASURE:", 8))
            *type = TTF_SEM_MEASURE;
        else
            colon = NULL;
        if (colon && colon[1] && strlen(colon + 1) < sizeof(out->unit)) {
            (void)snprintf(out->unit, sizeof(out->unit), "%s", colon + 1);
            out->layer = layer;
            return 1;
        }
    }
    if (p < in->length && in->text[p] == '%' && (p + 1 == in->length || in->text[p + 1] != '%') &&
        quantity_tail(in, p + 1, allow_range)) {
        *end = p + 1;
        *type = TTF_SEM_PERCENT;
        return 1;
    }
    return 0;
}

/* Clock interval endpoints deliberately exclude meridiem/offset metadata:
   RANGE.secondary is already occupied by the second exact endpoint. */
static size_t clock_endpoint(const ttf_pack_input_t *in, size_t start,
                             ttf_semantic_t *out, int allow_range) {
    const char *s = in->text, *suffix;
    size_t n = in->length, hour_end = integer(s, n, start), p, end, suffix_end;
    unsigned hour, minute, second = 0;
    int seconds = 0, valid;
    ttf_resource_layer_t layer;
    if (hour_end == start || hour_end >= n || s[hour_end] != ':')
        return 0;
    hour = hour_end - start <= 2 ? small_uint(s + start, hour_end - start) : 24;
    p = hour_end + 1;
    end = integer(s, n, p);
    minute = end - p == 2 ? small_uint(s + p, 2) : 60;
    valid = hour < 24 && minute < 60;
    if (end < n && s[end] == ':') {
        seconds = 1;
        p = end + 1;
        end = integer(s, n, p);
        second = end - p == 2 ? small_uint(s + p, 2) : 60;
        valid = valid && second < 60;
    }
    if (resource_prefix(in, "time_suffix", skip_space(s, n, end), &suffix, &suffix_end, &layer)) {
        end = suffix_end;
        valid = 0;
    }
    if (!quantity_tail(in, end, allow_range)) {
        valid = 0;
        end = atom_end(s, n, end);
    }
    out->type = valid ? TTF_SEM_TIME : TTF_SEM_TEXT;
    (void)snprintf(out->unit, sizeof(out->unit), "time");
    if (seconds)
        (void)snprintf(out->value, sizeof(out->value), "%02u:%02u:%02u", hour, minute, second);
    else
        (void)snprintf(out->value, sizeof(out->value), "%02u:%02u", hour, minute);
    return end;
}

/* Explicitly repeated currency/rate/measure/counter/clock units participate. A missing
   or different second unit must never be inferred as an exchange conversion. */
static size_t quantity_endpoint(const ttf_pack_input_t *in, size_t start,
                                ttf_semantic_t *out, int allow_range) {
    const char *prefix = NULL;
    size_t number_start = start, p, suffix_end;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    ttf_semantic_type_t type;
    p = clock_endpoint(in, start, out, allow_range);
    if (p)
        return p;
    if (resource_prefix(in, "currency_prefix", start, &prefix, &p, &layer))
        number_start = skip_space(in->text, in->length, p);
    p = number(in->text, in->length, number_start, out->value);
    if (p == number_start)
        return 0;
    if (quantity_suffix(in, skip_space(in->text, in->length, p), out, &suffix_end, &type,
                        allow_range)) {
        if (type != TTF_SEM_CURRENCY && type != TTF_SEM_MEASURE && type != TTF_SEM_PERCENT &&
            type != TTF_SEM_COUNTER)
            return 0;
        if (type == TTF_SEM_PERCENT)
            (void)snprintf(out->unit, sizeof(out->unit), "percent");
        out->type = prefix && (type != TTF_SEM_CURRENCY || strcmp(prefix, out->unit))
                        ? TTF_SEM_TEXT : type;
        return suffix_end;
    }
    if (prefix && quantity_tail(in, p, allow_range)) {
        out->type = TTF_SEM_CURRENCY;
        out->layer = layer;
        (void)snprintf(out->unit, sizeof(out->unit), "%s", prefix);
        return p;
    }
    return 0;
}
/* Preserve unsupported chains through each numeric endpoint, rather than
   verbalizing their last number after retaining only an invalid prefix. */
static size_t range_chain_end(const ttf_pack_input_t *in, size_t end) {
    size_t separator;
    while ((separator = quantity_separator(in, skip_space(in->text, in->length, end), NULL))) {
        ttf_semantic_t next = {0};
        size_t start = skip_space(in->text, in->length, separator);
        size_t next_end = quantity_endpoint(in, start, &next, 1);
        if (!next_end) {
            ttf_semantic_type_t type;
            size_t suffix_end;
            next_end = number(in->text, in->length, start, next.value);
            if (next_end == start) {
                end = separator;
                break;
            }
            if (quantity_suffix(in, skip_space(in->text, in->length, next_end), &next,
                                &suffix_end, &type, 1))
                next_end = suffix_end;
        }
        if (next_end <= end)
            break;
        end = next_end;
    }
    return end;
}
static size_t repeated_quantity_range(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    ttf_semantic_t first = {0}, second = {0};
    ttf_resource_layer_t delimiter_layer = TTF_LAYER_COMMON;
    size_t start = in->tokens[in->index].begin;
    size_t p = quantity_endpoint(in, start, &first, 1), separator, end;
    if (!p)
        return 0;
    separator = skip_space(in->text, in->length, p);
    p = quantity_separator(in, separator, &delimiter_layer);
    if (!p)
        return 0;
    start = skip_space(in->text, in->length, p);
    if (start >= in->length)
        return 0;
    end = quantity_endpoint(in, start, &second, 1);
    if (!end) {
        /* A numeric malformed endpoint remains visible as one TEXT object. */
        if (!digit(in->text[start]) && in->text[start] != '+' && in->text[start] != '-')
            return 0;
        end = range_chain_end(in, atom_end(in->text, in->length, start));
        return preserve(in, out, end, "syntax.invalid-quantity-range");
    }
    if (quantity_separator(in, skip_space(in->text, in->length, end), NULL))
        return preserve(in, out, range_chain_end(in, end), "syntax.invalid-quantity-range");
    if (first.type == TTF_SEM_TEXT || second.type == TTF_SEM_TEXT ||
        first.type != second.type || strcmp(first.unit, second.unit) ||
        (in->text[separator] == '-' && p == start &&
         (in->text[start] == '-' || in->text[start] == '+')))
        return preserve(in, out, end, "syntax.invalid-quantity-range");
    memcpy(out->value, first.value, sizeof(out->value));
    memcpy(out->secondary, second.value, sizeof(out->secondary));
    memcpy(out->unit, first.unit, sizeof(out->unit));
    out->layer = first.layer > second.layer ? first.layer : second.layer;
    if (delimiter_layer > out->layer)
        out->layer = delimiter_layer;
    return finish(in, out, end, TTF_SEM_RANGE,
                  first.type == TTF_SEM_TIME ? "syntax.time-range" : "syntax.repeated-quantity-range");
}

size_t ttf_classify_universal(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *s = in->text, *rule_value = NULL;
    size_t n = in->length, b = in->tokens[in->index].begin;
    size_t p = b, q, r, end, a_end;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    if (b >= n)
        return 0;

    /* Resource-declared technical spellings may cross script/punctuation
       boundaries. The inventory and source tag belong to language data. */
    if (resource_prefix(in, "semantic_term", b, &rule_value, &end, &layer) &&
        (end == n || !(ascii_letter(s[end]) || digit(s[end]) || s[end] == '_' || s[end] == '+' ||
                       s[end] == '-'))) {
        const char *colon = strchr(rule_value, ':');
        ttf_semantic_type_t type = TTF_SEM_TEXT;
        ttf_lexical_class_t lexical = TTF_LEX_NONE;
        if (!strncmp(rule_value, "ALNUM_TERM:", 11)) {
            type = TTF_SEM_ALNUM_TERM;
            lexical = TTF_LEX_ALNUM_TERM;
        } else if (!strncmp(rule_value, "WORD:", 5)) {
            type = TTF_SEM_WORD;
            lexical = TTF_LEX_WORD;
        } else if (!strncmp(rule_value, "ACRONYM:", 8)) {
            type = TTF_SEM_ACRONYM;
            lexical = TTF_LEX_ACRONYM;
        } else if (!strncmp(rule_value, "INITIALISM:", 11)) {
            type = TTF_SEM_INITIALISM;
            lexical = TTF_LEX_INITIALISM;
        }
        if (colon && type != TTF_SEM_TEXT && colon[1]) {
            out->lexical_class = lexical;
            out->layer = layer;
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", colon + 1);
            return finish(in, out, end, type, "resource.semantic-term");
        }
    }
    /* Protected lexical objects: retaining their entire surface also prevents
       punctuation inside them from becoming a sentence boundary. */
    if ((n - b >= 7 && !memcmp(s + b, "http://", 7)) ||
        (n - b >= 8 && !memcmp(s + b, "https://", 8)) ||
        (n - b >= 4 && !memcmp(s + b, "www.", 4))) {
        q = b;
        while (q < n && ascii_web(s[q]))
            ++q;
        while (q > b && (s[q - 1] == '.' || s[q - 1] == ',' || s[q - 1] == '!' || s[q - 1] == '?' ||
                         s[q - 1] == ')'))
            --q;
        return finish(in, out, q, TTF_SEM_URL, "syntax.url");
    }
    if (in->tokens[in->index].script == TTF_SCRIPT_LATIN) {
        int at = 0, dot = 0;
        q = b;
        while (q < n &&
               ((s[q] >= 'a' && s[q] <= 'z') || (s[q] >= 'A' && s[q] <= 'Z') || digit(s[q]) ||
                s[q] == '.' || s[q] == '-' || s[q] == '_' || s[q] == '+' || s[q] == '@')) {
            if (s[q] == '@')
                at = 1;
            if (at && s[q] == '.')
                dot = 1;
            ++q;
        }
        while (q > b && s[q - 1] == '.')
            --q;
        if (at && dot)
            return finish(in, out, q, TTF_SEM_EMAIL, "syntax.email");
        q = in->tokens[in->index].end;
        if (q < n && s[q] == '-' && q + 1 < n && digit(s[q + 1])) {
            q = integer(s, n, q + 1);
            out->lexical_class = TTF_LEX_ALNUM_TERM;
            return finish(in, out, q, TTF_SEM_ALNUM_TERM, "syntax.alnum");
        }
        if (q < n && digit(s[q])) {
            q = integer(s, n, q);
            out->lexical_class = TTF_LEX_ALNUM_TERM;
            return finish(in, out, q, TTF_SEM_ALNUM_TERM, "syntax.alnum");
        }
        return 0;
    }

    p = repeated_quantity_range(in, out);
    if (p)
        return p;

    if (resource_prefix(in, "currency_prefix", b, &rule_value, &end, &layer)) {
        size_t start = skip_space(s, n, end);
        p = number(s, n, start, out->value);
        if (p > start && !structured_tail(s, n, p)) {
            (void)snprintf(out->unit, sizeof(out->unit), "%s", rule_value);
            out->layer = layer;
            return finish(in, out, p, TTF_SEM_CURRENCY, "syntax.currency-prefix");
        }
        if (start < n && (digit(s[start]) || s[start] == '+' || s[start] == '-'))
            return preserve(in, out, atom_end(s, n, start), "syntax.invalid-currency");
    }
    p = b;
    if (s[p] == '-' || s[p] == '+')
        ++p;
    if (p >= n || (!digit(s[p]) && !(s[p] == '.' && p + 1 < n && digit(s[p + 1]))))
        return 0;

    a_end = integer(s, n, b);
    /* Calendar date, with bounded fields and a Gregorian day check. */
    if (a_end - b == 4 && a_end < n && (s[a_end] == '-' || s[a_end] == '.')) {
        char separator = s[a_end];
        unsigned year, month, day, maxday;
        p = skip_space(s, n, a_end + 1);
        q = integer(s, n, p);
        if (q > p && q < n && s[q] == separator) {
            r = skip_space(s, n, q + 1);
            end = integer(s, n, r);
            if (end > r) {
                static const unsigned days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                year = small_uint(s + b, 4);
                month = q - p <= 2 ? small_uint(s + p, q - p) : 0;
                day = end - r <= 2 ? small_uint(s + r, end - r) : 0;
                maxday = (month >= 1 && month <= 12) ? days[month - 1] : 0;
                if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
                    ++maxday;
                if (year && day >= 1 && day <= maxday && !structured_tail(s, n, end)) {
                    (void)snprintf(out->value, sizeof(out->value), "%04u-%02u-%02u", year, month,
                                   day);
                    if (separator == '.' && end < n && s[end] == '.')
                        ++end;
                    return finish(in, out, end, TTF_SEM_DATE, "syntax.date");
                }
                return preserve(in, out, atom_end(s, n, end), "syntax.invalid-date");
            }
        }
    }
    if (a_end > b && a_end < n && s[a_end] == ':') {
        unsigned hour = a_end - b <= 2 ? small_uint(s + b, a_end - b) : 24;
        unsigned minute, second = 0;
        int has_seconds = 0, valid;
        p = a_end + 1;
        q = integer(s, n, p);
        minute = q - p == 2 ? small_uint(s + p, 2) : 60;
        end = q;
        valid = hour < 24 && minute < 60;
        if (q < n && s[q] == ':') {
            has_seconds = 1;
            r = q + 1;
            end = integer(s, n, r);
            second = end - r == 2 ? small_uint(s + r, 2) : 60;
            valid = valid && second < 60;
        }
        {
            size_t suffix_start = skip_space(s, n, end), suffix_end;
            if (resource_prefix(in, "time_suffix", suffix_start, &rule_value, &suffix_end,
                                &layer) &&
                !structured_tail(s, n, suffix_end)) {
                valid = valid && hour >= 1 && hour <= 12 &&
                        copy_value(out->secondary, sizeof(out->secondary), rule_value,
                                   strlen(rule_value));
                end = suffix_end;
                out->layer = layer;
            }
        }
        if (valid && !structured_tail(s, n, end)) {
            if (has_seconds)
                (void)snprintf(out->value, sizeof(out->value), "%02u:%02u:%02u", hour, minute,
                               second);
            else
                (void)snprintf(out->value, sizeof(out->value), "%02u:%02u", hour, minute);
            return finish(in, out, end, TTF_SEM_TIME, "syntax.time");
        }
        return preserve(in, out, atom_end(s, n, end), "syntax.invalid-time");
    }
    if (a_end > b && a_end < n && s[a_end] == '-') {
        p = a_end + 1;
        q = integer(s, n, p);
        if (q > p && q < n && s[q] == '-') {
            r = q + 1;
            end = integer(s, n, r);
            if (a_end - b >= 2 && a_end - b <= 4 && q - p >= 2 && q - p <= 4 && end - r >= 3 &&
                end - r <= 4 && !structured_tail(s, n, end) &&
                copy_value(out->value, sizeof(out->value), s + b, end - b))
                return finish(in, out, end, TTF_SEM_PHONE, "syntax.phone");
            return preserve(in, out, atom_end(s, n, b), "syntax.invalid-phone");
        }
    }
    if (a_end > b && a_end < n && s[a_end] == '.') {
        unsigned parts = 1;
        int ip = (a_end - b <= 3 && small_uint(s + b, a_end - b) <= 255);
        p = a_end;
        while (p + 1 < n && s[p] == '.' && digit(s[p + 1])) {
            q = integer(s, n, p + 1);
            if (q - p - 1 > 3 || small_uint(s + p + 1, q - p - 1) > 255)
                ip = 0;
            ++parts;
            p = q;
        }
        if (parts >= 3)
            return finish(in, out, p, (parts == 4 && ip) ? TTF_SEM_IP_ADDRESS : TTF_SEM_VERSION,
                          (parts == 4 && ip) ? "syntax.ip" : "syntax.version");
    }
    p = number(s, n, b, out->value);
    if (p == b)
        return preserve(in, out, atom_end(s, n, b), "syntax.invalid-number");
    q = skip_space(s, n, p);
    {
        ttf_resource_layer_t separator_layer = TTF_LAYER_COMMON;
        size_t separator_end = quantity_separator(in, q, &separator_layer);
        int legacy_hyphen = q < n && s[q] == '-';
        /* Keep the established bare-hyphen compatibility grammar; resource
           delimiters additionally provide explicit ranges such as 1~2. */
        if (!separator_end && legacy_hyphen)
            separator_end = q + 1;
        if (separator_end) {
            size_t start = skip_space(s, n, separator_end);
            if (legacy_hyphen && start == separator_end && start < n &&
                (s[start] == '-' || s[start] == '+'))
                return preserve(in, out, atom_end(s, n, start), "syntax.invalid-range");
            end = number(s, n, start, out->secondary);
            if (end > start) {
                size_t suffix_start = skip_space(s, n, end), suffix_end;
                ttf_semantic_type_t suffix_type;
                if (quantity_suffix(in, suffix_start, out, &suffix_end, &suffix_type, 1)) {
                    if (suffix_type == TTF_SEM_PERCENT)
                        (void)snprintf(out->unit, sizeof(out->unit), "percent");
                    if (quantity_separator(in, skip_space(s, n, suffix_end), NULL))
                        return preserve(in, out, range_chain_end(in, suffix_end),
                                        "syntax.invalid-range");
                    if (separator_layer > out->layer)
                        out->layer = separator_layer;
                    return finish(in, out, suffix_end, TTF_SEM_RANGE, "syntax.quantity-range");
                }
                if (quantity_separator(in, skip_space(s, n, end), NULL))
                    return preserve(in, out, range_chain_end(in, end), "syntax.invalid-range");
                if (!structured_tail(s, n, end)) {
                    out->layer = separator_layer;
                    return finish(in, out, end, TTF_SEM_RANGE,
                                  legacy_hyphen ? "syntax.range" : "syntax.explicit-range");
                }
                return preserve(in, out, atom_end(s, n, end), "syntax.invalid-range");
            }
            if ((!legacy_hyphen && start == n) ||
                (start < n && (digit(s[start]) || s[start] == '+' || s[start] == '-' || s[start] == '.')))
                return preserve(in, out, atom_end(s, n, start), "syntax.invalid-range");
        }
    }
    if (q < n && s[q] == '/') {
        int nonzero = 0;
        size_t start = skip_space(s, n, q + 1);
        end = integer(s, n, start);
        for (r = start; r < end; ++r)
            nonzero |= s[r] != '0';
        if (!strchr(out->value, '.') && end > start && nonzero && !structured_tail(s, n, end) &&
            copy_value(out->secondary, sizeof(out->secondary), s + start, end - start))
            return finish(in, out, end, TTF_SEM_FRACTION, "syntax.fraction");
        if (end > start || q == p)
            return preserve(in, out, atom_end(s, n, end), "syntax.invalid-fraction");
    }
    if (resource_prefix(in, "ordinal_suffix", p, &rule_value, &end, &layer)) {
        if (!structured_tail(s, n, end) &&
            valid_ordinal(in, rule_value, out->value, s + p, end - p)) {
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", rule_value);
            out->layer = layer;
            return finish(in, out, end, TTF_SEM_ORDINAL, "syntax.ordinal");
        }
        return preserve(in, out, atom_end(s, n, b), "syntax.invalid-ordinal");
    }
    q = skip_space(s, n, p);
    {
        ttf_semantic_type_t type;
        if (quantity_suffix(in, q, out, &end, &type, 0))
            return finish(in, out, end, type,
                          type == TTF_SEM_PERCENT ? "syntax.percent" : "syntax.numeric-suffix");
    }
    if (structured_tail(s, n, p))
        return preserve(in, out, atom_end(s, n, b), "syntax.unsupported-numeric");
    return finish(in, out, p, TTF_SEM_CARDINAL, "syntax.cardinal");
}
