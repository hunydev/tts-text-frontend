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
static size_t number(const char *s, size_t n, size_t p, char *value) {
    size_t start = p, k = 0, first;
    if (p < n && (s[p] == '-' || s[p] == '+')) {
        value[k++] = s[p++];
    }
    first = p;
    p = integer(s, n, p);
    if (p == first || p - start >= TTF_IR_VALUE_SIZE)
        return start;
    memcpy(value + k, s + first, p - first);
    k += p - first;
    while (p < n && s[p] == ',') {
        size_t q = integer(s, n, p + 1);
        if (q - p != 4 || k + 3 >= TTF_IR_VALUE_SIZE)
            break;
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
static int ascii_web(char c) {
    unsigned char u = (unsigned char)c;
    return u > 32 && u < 127 && c != '<' && c != '>' && c != '"';
}

size_t ttf_classify_universal(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *s = in->text, *rule_value = NULL;
    size_t n = in->length, b = in->tokens[in->index].begin;
    size_t p = b, q, r, end, a_end;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    if (b >= n)
        return 0;

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

    if (resource_prefix(in, "currency_prefix", b, &rule_value, &end, &layer)) {
        p = number(s, n, end, out->value);
        if (p > end) {
            (void)snprintf(out->unit, sizeof(out->unit), "%s", rule_value);
            out->layer = layer;
            return finish(in, out, p, TTF_SEM_CURRENCY, "syntax.currency-prefix");
        }
    }
    if (!digit(s[b]) && !((s[b] == '-' || s[b] == '+') && b + 1 < n && digit(s[b + 1])))
        return 0;

    a_end = integer(s, n, b);
    /* Calendar date, with bounded fields and a Gregorian day check. */
    if (a_end - b == 4 && a_end < n && (s[a_end] == '-' || s[a_end] == '.')) {
        char separator = s[a_end];
        unsigned year, month, day, maxday;
        p = skip_space(s, n, a_end + 1);
        q = integer(s, n, p);
        if (q > p && q - p <= 2 && q < n && s[q] == separator) {
            r = skip_space(s, n, q + 1);
            end = integer(s, n, r);
            if (end > r && end - r <= 2) {
                static const unsigned days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                year = small_uint(s + b, 4);
                month = small_uint(s + p, q - p);
                day = small_uint(s + r, end - r);
                maxday = (month >= 1 && month <= 12) ? days[month - 1] : 0;
                if (month == 2 && (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)))
                    ++maxday;
                if (year && day >= 1 && day <= maxday) {
                    (void)snprintf(out->value, sizeof(out->value), "%04u-%02u-%02u", year, month,
                                   day);
                    if (separator == '.' && end < n && s[end] == '.')
                        ++end;
                    return finish(in, out, end, TTF_SEM_DATE, "syntax.date");
                }
            }
        }
    }
    if (a_end > b && a_end - b <= 2 && a_end < n && s[a_end] == ':') {
        p = a_end + 1;
        q = integer(s, n, p);
        if (q - p == 2 && small_uint(s + b, a_end - b) < 24 && small_uint(s + p, 2) < 60) {
            (void)snprintf(out->value, sizeof(out->value), "%02u:%02u",
                           small_uint(s + b, a_end - b), small_uint(s + p, 2));
            return finish(in, out, q, TTF_SEM_TIME, "syntax.time");
        }
    }
    if (a_end > b && a_end < n && s[a_end] == '-') {
        p = a_end + 1;
        q = integer(s, n, p);
        if (q > p && q < n && s[q] == '-') {
            r = q + 1;
            end = integer(s, n, r);
            if (a_end - b >= 2 && a_end - b <= 4 && q - p >= 2 && q - p <= 4 && end - r >= 3 &&
                end - r <= 4 && copy_value(out->value, sizeof(out->value), s + b, end - b))
                return finish(in, out, end, TTF_SEM_PHONE, "syntax.phone");
        }
        if (q > p && copy_value(out->value, sizeof(out->value), s + b, a_end - b) &&
            copy_value(out->secondary, sizeof(out->secondary), s + p, q - p))
            return finish(in, out, q, TTF_SEM_RANGE, "syntax.range");
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
        return 0;
    if (resource_prefix(in, "ordinal_suffix", p, &rule_value, &end, &layer)) {
        if (!strchr(out->value, '.') &&
            (end == n || !((s[end] >= 'a' && s[end] <= 'z') || (s[end] >= 'A' && s[end] <= 'Z')))) {
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", rule_value);
            out->layer = layer;
            return finish(in, out, end, TTF_SEM_ORDINAL, "syntax.ordinal");
        }
    }
    if (p < n && s[p] == '%')
        return finish(in, out, p + 1, TTF_SEM_PERCENT, "syntax.percent");
    if (resource_prefix(in, "numeric_suffix", p, &rule_value, &end, &layer)) {
        const char *colon = strchr(rule_value, ':');
        ttf_semantic_type_t type = TTF_SEM_MEASURE;
        if (!strncmp(rule_value, "COUNTER:", 8))
            type = TTF_SEM_COUNTER;
        else if (!strncmp(rule_value, "CURRENCY:", 9))
            type = TTF_SEM_CURRENCY;
        else if (strncmp(rule_value, "MEASURE:", 8))
            colon = NULL;
        if (colon) {
            (void)snprintf(out->unit, sizeof(out->unit), "%s", colon + 1);
            out->layer = layer;
            return finish(in, out, end, type, "syntax.numeric-suffix");
        }
    }
    return finish(in, out, p, TTF_SEM_CARDINAL, "syntax.cardinal");
}
