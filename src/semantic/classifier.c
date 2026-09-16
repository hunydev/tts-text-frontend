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
        if (valid && !structured_tail(s, n, end)) {
            if (has_seconds)
                (void)snprintf(out->value, sizeof(out->value), "%02u:%02u:%02u", hour, minute,
                               second);
            else
                (void)snprintf(out->value, sizeof(out->value), "%02u:%02u", hour, minute);
            return finish(in, out, end, TTF_SEM_TIME, "syntax.time");
        }
        return preserve(in, out, atom_end(s, n, b), "syntax.invalid-time");
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
        if (q > p && !structured_tail(s, n, q) &&
            copy_value(out->value, sizeof(out->value), s + b, a_end - b) &&
            copy_value(out->secondary, sizeof(out->secondary), s + p, q - p))
            return finish(in, out, q, TTF_SEM_RANGE, "syntax.range");
        if (q > p)
            return preserve(in, out, atom_end(s, n, b), "syntax.invalid-range");
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
    if (q < n && s[q] == '%' && !structured_tail(s, n, q + 1))
        return finish(in, out, q + 1, TTF_SEM_PERCENT, "syntax.percent");
    if (resource_prefix(in, "numeric_suffix", q, &rule_value, &end, &layer) &&
        !structured_tail(s, n, end)) {
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
    if (structured_tail(s, n, p))
        return preserve(in, out, atom_end(s, n, b), "syntax.unsupported-numeric");
    return finish(in, out, p, TTF_SEM_CARDINAL, "syntax.cardinal");
}
