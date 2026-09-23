#include "../core/internal.h"
#include "numeric_grouping.h"
#include "phone.h"

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
size_t ttf_numeric_value(const ttf_pack_input_t *in, size_t p, char value[TTF_IR_VALUE_SIZE],
                         ttf_resource_layer_t *matched_layer) {
    const char *s = in->text;
    size_t n = in->length, start = p, k = 0, first, separator_width = 0;
    char separator[5] = {0};
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
    while (ttf_numeric_group_link(in->engine, s, n, p)) {
        int enabled = 0;
        ttf_resource_layer_t layer = TTF_LAYER_COMMON;
        size_t width = ttf_numeric_group_separator(in->engine, s, n, p, &enabled, &layer);
        size_t q = integer(s, n, p + width);
        if (matched_layer && layer > *matched_layer)
            *matched_layer = layer;
        if (!enabled || (!separator_width && (p - first == 0 || p - first > 3)) ||
            q - p - width != 3 || k + 3 >= TTF_IR_VALUE_SIZE ||
            (separator_width && (separator_width != width || memcmp(separator, s + p, width))))
            return start;
        if (!separator_width) {
            separator_width = width;
            memcpy(separator, s + p, width);
        }
        memcpy(value + k, s + p + width, 3);
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
    /* Grouping the fraction or exponent is outside this integer-group grammar.
       Preserve the entire numeric object instead of interpreting its prefix. */
    if (ttf_numeric_group_link(in->engine, s, n, p))
        return start;
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
static size_t skip_quantity_space(const ttf_pack_input_t *in, size_t p) {
    size_t q;
    for (;;) {
        q = skip_space(in->text, in->length, p);
        if (q < in->length && (unsigned char)in->text[q] >= 128) {
            size_t width =
                ttf_numeric_group_separator(in->engine, in->text, in->length, q, NULL, NULL);
            if (width) {
                p = q + width;
                continue;
            }
        }
        return q;
    }
}
static int quantity_space_before(const ttf_pack_input_t *in, size_t position) {
    size_t p = position;
    if (!p)
        return 0;
    if (in->text[p - 1] == ' ' || in->text[p - 1] == '\t')
        return 1;
    do {
        --p;
    } while (p && ((unsigned char)in->text[p] & 0xc0) == 0x80);
    return (unsigned char)in->text[p] >= 128 &&
           ttf_numeric_group_separator(in->engine, in->text, position, p, NULL, NULL) ==
               position - p;
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
size_t ttf_numeric_atom_end(const ttf_pack_input_t *in, size_t p) {
    const char *s = in->text;
    size_t n = in->length, end = p;
    while (end < n) {
        size_t linked = ttf_numeric_group_link(in->engine, s, n, end);
        if (linked) {
            end = linked;
            continue;
        }
        if (!(digit(s[end]) || ascii_letter(s[end]) || s[end] == '.' || s[end] == ',' ||
              s[end] == '-' || s[end] == '+' || s[end] == ':' || s[end] == '/' || s[end] == '%' ||
              s[end] == '_'))
            break;
        ++end;
    }
    while (end > p && (s[end - 1] == '.' || s[end - 1] == ',' || s[end - 1] == ':'))
        --end;
    return end;
}
static int structured_tail(const ttf_pack_input_t *in, size_t p) {
    const char *s = in->text;
    size_t n = in->length, next = p;
    if (ttf_numeric_group_link(in->engine, s, n, p))
        return 1;
    if (p >= n)
        return 0;
    while (next < n && (s[next] == '.' || s[next] == ',' || s[next] == '-'))
        ++next;
    if (next > p + 1 && next < n && digit(s[next]))
        return 1;
    return ascii_letter(s[p]) || digit(s[p]) || s[p] == ':' || s[p] == '/' || s[p] == '_' ||
           (s[p] == '%' && p + 1 < n &&
            (ascii_letter(s[p + 1]) || digit(s[p + 1]) || s[p + 1] == '%')) ||
           ((s[p] == '.' || s[p] == ',' || s[p] == '-') && p + 1 < n && digit(s[p + 1]));
}
static size_t invalid_numeric_end(const ttf_pack_input_t *in, size_t start);
static size_t preserve(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t end,
                       const char *rule) {
    size_t p = in->tokens[in->index].begin;
    /* A malformed grouped amount is one object including known units and
       subsequent range endpoints, not a partially verbalized number sequence. */
    int expanded = 0;
    while (p < end) {
        size_t width = ttf_numeric_group_separator(in->engine, in->text, in->length, p, NULL, NULL);
        size_t linked = width ? ttf_numeric_group_link(in->engine, in->text, in->length, p) : 0;
        if (linked) {
            if (!expanded && (unsigned char)in->text[p] >= 128) {
                size_t whole = invalid_numeric_end(in, in->tokens[in->index].begin);
                if (whole > end)
                    end = whole;
                expanded = 1;
            }
            /* Walk a repeated separator run once. Rechecking its full suffix
               at every character would be quadratic on malformed long input. */
            while (p < linked) {
                ttf_resource_layer_t layer = TTF_LAYER_COMMON;
                width =
                    ttf_numeric_group_separator(in->engine, in->text, in->length, p, NULL, &layer);
                if (layer > out->layer)
                    out->layer = layer;
                p += width;
            }
        } else
            p += width ? width : 1;
    }
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
/* Math operators and relations: ASCII or the UTF-8 signs × ÷ − ≤ ≥. Returns
   the byte width and writes the canonical operator, or 0 when absent. */
static size_t math_operator_at(const char *s, size_t n, size_t p, const char **op) {
    static const struct {
        const char *text, *canonical;
    } table[] = {{"\xc3\x97", "×"},
                 {"\xc3\xb7", "÷"},
                 {"\xe2\x88\x92", "-"},
                 {"\xe2\x89\xa4", "≤"},
                 {"\xe2\x89\xa5", "≥"},
                 {"+", "+"},
                 {"-", "-"},
                 {"*", "*"},
                 {"/", "/"},
                 {"=", "="},
                 {"<", "<"},
                 {">", ">"}};
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); ++i) {
        size_t w = strlen(table[i].text);
        if (n - p >= w && !memcmp(s + p, table[i].text, w)) {
            *op = table[i].canonical;
            return w;
        }
    }
    return 0;
}
static int math_relation(const char *op) {
    return !strcmp(op, "=") || !strcmp(op, "<") || !strcmp(op, ">") || !strcmp(op, "≤") ||
           !strcmp(op, "≥");
}
/* 3-1=2, 2×3, a+b=c: operands are numbers or single ASCII letters. A plain
   hyphen or slash is also a range, date or fraction, so without a relation only
   ×, ÷, * or a letter operand with + make an expression. The canonical form is
   stored in the value with single spaces between operands and operators. */
static size_t math_expression_end(const ttf_pack_input_t *in, size_t b, ttf_semantic_t *out) {
    const char *s = in->text, *op = NULL;
    size_t n = in->length, p = b, end = b, used = 0, operands = 0, width;
    int relations = 0, letters = 0, strong = 0, weak = 0;
    char value[TTF_IR_VALUE_SIZE];
    if (b && (ascii_letter(s[b - 1]) || digit(s[b - 1]) || s[b - 1] == '.' ||
              math_operator_at(s, n, b - 1, &op)))
        return 0;
    for (;;) {
        size_t start = p;
        char operand[48];
        size_t length = 0;
        if (p < n && (width = math_operator_at(s, n, p, &op)) && !strcmp(op, "-") &&
            p + width < n && digit(s[p + width])) {
            operand[length++] = '-';
            p += width;
        }
        if (p < n && digit(s[p])) {
            size_t q = integer(s, n, p);
            if (q < n && s[q] == '.' && q + 1 < n && digit(s[q + 1]))
                q = integer(s, n, q + 1);
            if (q - p + length >= sizeof(operand))
                return 0;
            memcpy(operand + length, s + p, q - p);
            length += q - p;
            p = q;
        } else if (length == 0 && p < n && ascii_letter(s[p]) &&
                   !(p + 1 < n && (ascii_letter(s[p + 1]) || digit(s[p + 1])))) {
            operand[length++] = s[p++];
            letters = 1;
        } else
            return 0;
        (void)start;
        if (used + length + 2 >= sizeof(value))
            return 0;
        if (used)
            value[used++] = ' ';
        memcpy(value + used, operand, length);
        used += length;
        ++operands;
        end = p;
        p = skip_space(s, n, p);
        if (p >= n || !(width = math_operator_at(s, n, p, &op)))
            break;
        {
            size_t next = skip_space(s, n, p + width);
            const char *second = NULL;
            if (next >= n || (math_operator_at(s, n, next, &second) &&
                              !(!strcmp(second, "-") && next + 1 < n && digit(s[next + 1]))))
                break;
            if (!digit(s[next]) && !ascii_letter(s[next]) && strcmp(second ? second : "", "-"))
                break;
            if (math_relation(op))
                ++relations;
            else if (!strcmp(op, "*") || !strcmp(op, "×") || !strcmp(op, "÷"))
                strong = 1;
            else if (!strcmp(op, "-") || !strcmp(op, "/"))
                weak = 1;
            if (used + strlen(op) + 2 >= sizeof(value))
                return 0;
            value[used++] = ' ';
            memcpy(value + used, op, strlen(op));
            used += strlen(op);
            p = next;
        }
    }
    if (operands < 2 || relations > 1)
        return 0;
    if (!relations && !(strong && !weak) && !(letters && !weak))
        return 0;
    if (end < n &&
        (ascii_letter(s[end]) || digit(s[end]) || s[end] == '%' || s[end] == '$' || s[end] == '_' ||
         s[end] == '@' || (s[end] == '.' && end + 1 < n && digit(s[end + 1])) ||
         math_operator_at(s, n, end, &op)))
        return 0;
    value[used] = '\0';
    memcpy(out->value, value, used + 1);
    return end;
}
/* A bare host such as naver.com: ASCII labels whose final label is a declared
   lowercase top-level domain. Returns the end offset, or `begin` when absent. */
static size_t bare_domain_end(const ttf_pack_input_t *in, size_t begin) {
    const char *s = in->text;
    size_t n = in->length, p = begin, label = begin, labels = 0;
    char tld[32];
    if (begin &&
        (ascii_letter(s[begin - 1]) || digit(s[begin - 1]) || s[begin - 1] == '.' ||
         s[begin - 1] == '@' || s[begin - 1] == '_' || s[begin - 1] == '-' || s[begin - 1] == '/'))
        return begin;
    for (;;) {
        label = p;
        while (p < n && (ascii_letter(s[p]) || digit(s[p]) || s[p] == '-'))
            ++p;
        if (p == label || s[label] == '-' || s[p - 1] == '-')
            return begin;
        ++labels;
        if (p + 1 < n && s[p] == '.' && (ascii_letter(s[p + 1]) || digit(s[p + 1])))
            ++p;
        else
            break;
    }
    if (labels < 2 || p - label >= sizeof(tld) || p - label < 2)
        return begin;
    for (size_t i = label; i < p; ++i) {
        if (s[i] < 'a' || s[i] > 'z')
            return begin;
        tld[i - label] = s[i];
    }
    tld[p - label] = '\0';
    if (!ttf_resource_get(in->engine, "url_tld", "*", tld, NULL) ||
        strcmp(ttf_resource_get(in->engine, "url_tld", "*", tld, NULL), "1"))
        return begin;
    if (p < n && (s[p] == '_' || s[p] == '@'))
        return begin;
    if (p < n && s[p] == '/')
        while (p < n && ascii_web(s[p]))
            ++p;
    while (p > begin && (s[p - 1] == '.' || s[p - 1] == ',' || s[p - 1] == '!' || s[p - 1] == '?' ||
                         s[p - 1] == ')'))
        --p;
    return p;
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
    return !structured_tail(in, end) ||
           (allow_range && quantity_separator(in, skip_quantity_space(in, end), NULL));
}
/* Written unit names are words, rather than adjacent identifier suffixes.
   Resource keys are lowercase ASCII words separated by one space. Matching
   folds ASCII case and horizontal whitespace, but never crosses a line. */
static int currency_name_tail(const ttf_pack_input_t *in, size_t end, int allow_range) {
    uint32_t cp;
    size_t width;
    if (!quantity_tail(in, end, allow_range))
        return 0;
    if (end == in->length)
        return 1;
    if ((in->text[end] == '-' || in->text[end] == '+') &&
        !(allow_range && quantity_separator(in, end, NULL)))
        return 0;
    if ((unsigned char)in->text[end] >= 128 &&
        ttf_utf8_decode(in->text + end, in->length - end, &cp, &width) == 1 &&
        (ttf_script_of(cp) == TTF_SCRIPT_LATIN || ttf_script_of(cp) == TTF_SCRIPT_OTHER))
        return 0;
    if (ttf_utf8_decode(in->text + end, in->length - end, &cp, &width) == 1 &&
        ttf_script_of(cp) == TTF_SCRIPT_SYMBOL &&
        !(allow_range && quantity_separator(in, end, NULL)))
        return 0;
    return 1;
}
static int currency_name_context(const ttf_pack_input_t *in, size_t end, const char *unit,
                                 ttf_resource_layer_t *layer) {
    const char *value;
    char key[64];
    size_t start = in->tokens[in->index].begin, p = start, begin, i, prefix_end;
    ttf_resource_layer_t matched;
    /* Any explicit monetary prefix supplies a currency interpretation. The
       endpoint parser separately rejects conflicting prefix/suffix identities. */
    if (resource_prefix(in, "currency_prefix", start, &value, &prefix_end, &matched) ||
        (start + 1 < in->length && (in->text[start] == '+' || in->text[start] == '-') &&
         resource_prefix(in, "currency_prefix", start + 1, &value, &prefix_end, &matched))) {
        if (matched > *layer)
            *layer = matched;
        return 1;
    }
    while (p && (in->text[p - 1] == ' ' || in->text[p - 1] == '\t'))
        --p;
    begin = p;
    while (begin && ascii_letter(in->text[begin - 1]))
        --begin;
    if (begin == p || p - begin >= sizeof(key) ||
        (begin &&
         (ascii_letter(in->text[begin - 1]) || digit(in->text[begin - 1]) ||
          in->text[begin - 1] == '_' || in->text[begin - 1] == '-' || in->text[begin - 1] == '/')))
        return 0;
    for (i = 0; i < p - begin; ++i) {
        char c = in->text[begin + i];
        key[i] = c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
    }
    key[i] = '\0';
    value = ttf_resource_get(in->engine, "currency_name_cue", "*", key, &matched);
    if (!value || (strcmp(value, "1") && strcmp(value, unit)))
        return 0;
    if (matched > *layer)
        *layer = matched;
    /* A mass complement vetoes a weak money cue: "paid 3 pounds of flour"
       must not acquire a currency identity from the verb alone. */
    begin = skip_quantity_space(in, end);
    p = begin;
    while (p < in->length && ascii_letter(in->text[p]))
        ++p;
    if (p - begin < sizeof(key)) {
        for (i = 0; i < p - begin; ++i) {
            char c = in->text[begin + i];
            key[i] = c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
        }
        key[i] = '\0';
        value = ttf_resource_get(in->engine, "currency_name_mass_tail", "*", key, NULL);
        if (value && !strcmp(value, "1"))
            return 0;
    }
    return 1;
}
static size_t currency_name_invalid_end(const ttf_pack_input_t *in, size_t end) {
    /* Retain a malformed attached identifier with a recognizable unit name;
       do not emit the prefix-only amount and then repeat the broken unit. */
    while (end < in->length) {
        uint32_t cp;
        size_t width, next = ttf_numeric_atom_end(in, end);
        ttf_script_t script;
        if (next > end) {
            end = next;
            continue;
        }
        if (ttf_utf8_decode(in->text + end, in->length - end, &cp, &width) != 1)
            break;
        script = ttf_script_of(cp);
        if (script != TTF_SCRIPT_LATIN && script != TTF_SCRIPT_OTHER && script != TTF_SCRIPT_SYMBOL)
            break;
        end += width;
    }
    return end;
}
static int currency_name_suffix(const ttf_pack_input_t *in, size_t start, ttf_semantic_t *out,
                                size_t *end, int allow_range) {
    char key[96];
    size_t p = start, k = 0;
    int found = 0;
    if (!quantity_space_before(in, start))
        return 0;
    while (p < in->length && k + 1 < sizeof(key)) {
        const char *value, *colon, *ambiguous;
        ttf_resource_layer_t layer, guard_layer;
        if (!ascii_letter(in->text[p]))
            break;
        do {
            char c = in->text[p++];
            if (k + 1 >= sizeof(key))
                return found;
            key[k++] = c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
        } while (p < in->length && ascii_letter(in->text[p]));
        key[k] = '\0';
        value = ttf_resource_get(in->engine, "currency_name", "*", key, &layer);
        colon = value ? strchr(value, ':') : NULL;
        if (colon && colon > value && (size_t)(colon - value) < sizeof(out->unit) && colon[1] &&
            strlen(colon + 1) < sizeof(out->source_lang)) {
            char unit[sizeof(out->unit)];
            memcpy(unit, value, (size_t)(colon - value));
            unit[colon - value] = '\0';
            if (!currency_name_tail(in, p, allow_range)) {
                memcpy(out->unit, unit, strlen(unit) + 1);
                out->layer = layer > out->layer ? layer : out->layer;
                *end = currency_name_invalid_end(in, p);
                return -1;
            }
            ambiguous =
                ttf_resource_get(in->engine, "currency_name_ambiguous", "*", key, &guard_layer);
            if (!ambiguous || strcmp(ambiguous, "1") ||
                currency_name_context(in, p, unit, &layer)) {
                if (ambiguous && guard_layer > layer)
                    layer = guard_layer;
                memcpy(out->unit, unit, strlen(unit) + 1);
                memcpy(out->source_lang, colon + 1, strlen(colon + 1) + 1);
                out->layer = layer > out->layer ? layer : out->layer;
                *end = p;
                found = 1;
            }
        }
        if (skip_quantity_space(in, p) == p)
            break;
        p = skip_quantity_space(in, p);
        key[k++] = ' ';
    }
    return found;
}
/* Trailing quantity units retain their semantic identity in the IR. In
   particular percentage points and basis points are not converted to percent. */
static int quantity_suffix(const ttf_pack_input_t *in, size_t p, ttf_semantic_t *out, size_t *end,
                           ttf_semantic_type_t *type, int allow_range) {
    const char *value;
    ttf_resource_layer_t layer;
    int name = currency_name_suffix(in, p, out, end, allow_range);
    if (name) {
        *type = name > 0 ? TTF_SEM_CURRENCY : TTF_SEM_TEXT;
        return 1;
    }
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
            out->layer = layer > out->layer ? layer : out->layer;
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
static size_t clock_endpoint(const ttf_pack_input_t *in, size_t start, ttf_semantic_t *out,
                             int allow_range) {
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
    if (resource_prefix(in, "time_suffix", skip_quantity_space(in, end), &suffix, &suffix_end,
                        &layer)) {
        end = suffix_end;
        valid = 0;
    }
    if (!quantity_tail(in, end, allow_range)) {
        valid = 0;
        end = ttf_numeric_atom_end(in, end);
    }
    out->type = valid ? TTF_SEM_TIME : TTF_SEM_TEXT;
    (void)snprintf(out->unit, sizeof(out->unit), "time");
    if (seconds)
        (void)snprintf(out->value, sizeof(out->value), "%02u:%02u:%02u", hour, minute, second);
    else
        (void)snprintf(out->value, sizeof(out->value), "%02u:%02u", hour, minute);
    return end;
}

/* Scaling inventories are data. The bounded decimal shift is shared with
   scientific notation; no floating-point approximation reaches Semantic IR. */
static int scale_power(const char *value) {
    char *tail;
    long power = strtol(value, &tail, 10);
    return *value && !*tail && power >= 3 && power <= 18 && power % 3 == 0 ? (int)power : 0;
}
static size_t quantity_endpoint(const ttf_pack_input_t *in, size_t start, ttf_semantic_t *out,
                                int allow_range) {
    const char *prefix = NULL, *scale = NULL;
    const char *s = in->text;
    size_t n = in->length, number_start = start, p, suffix_end, scale_end = 0, q;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON, scale_layer = TTF_LAYER_COMMON;
    ttf_semantic_type_t type;
    int power = 0, scaled = 0, valid = 1;
    char leading_sign = 0;
    p = clock_endpoint(in, start, out, allow_range);
    if (p)
        return p;
    if (resource_prefix(in, "currency_prefix", start, &prefix, &p, &layer))
        number_start = skip_quantity_space(in, p);
    else if (start + 1 < n && (s[start] == '-' || s[start] == '+') &&
             resource_prefix(in, "currency_prefix", start + 1, &prefix, &p, &layer)) {
        leading_sign = s[start];
        number_start = skip_quantity_space(in, p);
    }
    p = ttf_numeric_value(in, number_start, out->value, &out->layer);
    if (p == number_start)
        return 0;
    if (leading_sign) {
        size_t length = strlen(out->value);
        if (out->value[0] == '-' || out->value[0] == '+' || length + 1 >= sizeof(out->value))
            valid = 0;
        else {
            memmove(out->value + 1, out->value, length + 1);
            out->value[0] = leading_sign;
        }
    }
    q = skip_quantity_space(in, p);
    if (resource_prefix(in, "currency_scale", q, &scale, &scale_end, &scale_layer)) {
        ttf_semantic_t probe = {0};
        ttf_semantic_type_t probe_type;
        size_t probe_end;
        int has_suffix = quantity_suffix(in, skip_quantity_space(in, scale_end), &probe, &probe_end,
                                         &probe_type, allow_range);
        int currency_suffix = has_suffix && probe_type == TTF_SEM_CURRENCY;
        /* Bare m remains a meter, including mixed-unit ranges. A scale only
           takes precedence when a currency prefix or trailing currency exists. */
        int ordinary_unit = !prefix && !currency_suffix &&
                            quantity_suffix(in, q, &probe, &probe_end, &probe_type, allow_range);
        /* Across whitespace a scale cannot be the initial letters of prose:
           t in "to" and b in "bcm" remain ordinary text. A marker followed by
           structured punctuation still enters whole-object preservation. */
        int scale_boundary =
            q == p || scale_end == n || !ascii_letter(s[scale_end]) || currency_suffix;
        if (!ordinary_unit && scale_boundary && (prefix || currency_suffix || allow_range)) {
            scaled = 1;
            power = scale_power(scale);
            valid = valid && power && (quantity_tail(in, scale_end, allow_range) || has_suffix);
            p = scale_end;
            q = skip_quantity_space(in, p);
        }
    }
    if (quantity_suffix(in, q, out, &suffix_end, &type, allow_range)) {
        if (type == TTF_SEM_PERCENT)
            (void)snprintf(out->unit, sizeof(out->unit), "percent");
        valid = valid && (!prefix || (type == TTF_SEM_CURRENCY && !strcmp(prefix, out->unit)));
        /* A monetary scale must not reinterpret a physical unit. */
        if (scaled && type != TTF_SEM_CURRENCY)
            valid = 0;
        out->type = type;
        p = suffix_end;
    } else if (prefix && (quantity_tail(in, p, allow_range) || scaled)) {
        out->type = TTF_SEM_CURRENCY;
        (void)snprintf(out->unit, sizeof(out->unit), "%s", prefix);
    } else if (allow_range && quantity_tail(in, p, allow_range)) {
        /* Internal provisional endpoint only. The range planner must establish
           an explicit currency before such a bare endpoint can be promoted. */
        out->type = TTF_SEM_CARDINAL;
    } else
        return 0;
    if (layer > out->layer)
        out->layer = layer;
    if (scaled) {
        const char *next_scale;
        size_t next_end, next = skip_quantity_space(in, p);
        ttf_resource_layer_t next_layer;
        if (scale_layer > out->layer)
            out->layer = scale_layer;
        /* Multiple scale multipliers are not this grammar. Keep the entire
           recognized chain rather than reading only a misleading prefix. */
        while (resource_prefix(in, "currency_scale", next, &next_scale, &next_end, &next_layer) &&
               quantity_tail(in, next_end, allow_range)) {
            valid = 0;
            p = next_end;
            next = skip_quantity_space(in, p);
        }
        if ((structured_tail(in, p) || (p < n && (s[p] == '+' || s[p] == '-'))) &&
            !(allow_range && quantity_separator(in, p, NULL))) {
            valid = 0;
            p = ttf_numeric_atom_end(in, p);
        }
        if (valid && !expand_exponent(out->value, power))
            valid = 0;
        if (valid)
            (void)snprintf(out->secondary, sizeof(out->secondary), "%d", power);
    }
    if (!valid)
        out->type = TTF_SEM_TEXT;
    return p;
}
/* Preserve unsupported chains through each numeric endpoint, rather than
   verbalizing their last number after retaining only an invalid prefix. */
static size_t range_chain_end(const ttf_pack_input_t *in, size_t end) {
    size_t separator;
    while ((separator = quantity_separator(in, skip_quantity_space(in, end), NULL))) {
        ttf_semantic_t next = {0};
        size_t start = skip_quantity_space(in, separator);
        size_t next_end = quantity_endpoint(in, start, &next, 1);
        if (!next_end) {
            ttf_semantic_type_t type;
            size_t suffix_end;
            next_end = ttf_numeric_value(in, start, next.value, &next.layer);
            if (next_end == start) {
                end = separator;
                break;
            }
            if (quantity_suffix(in, skip_quantity_space(in, next_end), &next, &suffix_end, &type,
                                1))
                next_end = suffix_end;
        }
        if (next_end <= end)
            break;
        end = next_end;
    }
    return end;
}
/* This scanner preserves only a numeric candidate plus declared quantity
   suffixes/range delimiters. It does not absorb arbitrary following prose. */
static size_t invalid_numeric_end(const ttf_pack_input_t *in, size_t start) {
    size_t end = start;
    for (;;) {
        const char *prefix;
        size_t p = start, prefix_end, suffix_end, separator;
        ttf_resource_layer_t layer;
        ttf_semantic_t suffix = {0};
        ttf_semantic_type_t type;
        if (p < in->length && (in->text[p] == '+' || in->text[p] == '-'))
            ++p;
        if (resource_prefix(in, "currency_prefix", p, &prefix, &prefix_end, &layer))
            p = skip_quantity_space(in, prefix_end);
        if (p >= in->length ||
            (!digit(in->text[p]) && in->text[p] != '+' && in->text[p] != '-' && in->text[p] != '.'))
            return end;
        p = ttf_numeric_atom_end(in, p);
        if (p <= end)
            return end;
        end = p;
        if (quantity_suffix(in, skip_quantity_space(in, p), &suffix, &suffix_end, &type, 1))
            end = suffix_end;
        separator = quantity_separator(in, skip_quantity_space(in, end), NULL);
        if (!separator)
            return end;
        start = skip_quantity_space(in, separator);
    }
}
/* Units declared in the same `range_unit_family` (calendar, clock) describe one
   scale written across several tokens rather than two comparable quantities. */
static int same_unit_family(const ttf_pack_input_t *in, const char *left, const char *right) {
    const char *first = NULL, *second = NULL;
    size_t pack;
    if (!left[0] || !right[0])
        return 0;
    for (pack = 0; pack < in->engine->pack_count && !first; ++pack)
        first = ttf_resource_get(in->engine, "range_unit_family",
                                 in->engine->packs[pack]->language_tag, left, NULL);
    for (pack = 0; pack < in->engine->pack_count && !second; ++pack)
        second = ttf_resource_get(in->engine, "range_unit_family",
                                  in->engine->packs[pack]->language_tag, right, NULL);
    return first && second && !strcmp(first, second);
}

/* Evidence that this endpoint continues a larger written scale: the word just
   before it is a unit of the same family (10시 30분~11시, 3월 1일~3월 5일). */
static int preceding_family_unit(const ttf_pack_input_t *in, size_t begin, const char *unit) {
    size_t p = begin, end, start;
    char key[16];
    while (p && (in->text[p - 1] == ' ' || in->text[p - 1] == '\t'))
        --p;
    end = p;
    while (p && (unsigned char)in->text[p - 1] >= 128 && end - p + 1 < sizeof(key))
        --p;
    while (p < end && ((unsigned char)in->text[p] & 0xc0) == 0x80)
        ++p;
    if (p == end || end - p >= sizeof(key))
        return 0;
    for (start = p; start < end; ++start) {
        if (((unsigned char)in->text[start] & 0xc0) == 0x80)
            continue;
        memcpy(key, in->text + start, end - start);
        key[end - start] = 0;
        if (same_unit_family(in, key, unit))
            return 1;
    }
    return 0;
}

static size_t repeated_quantity_range(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    ttf_semantic_t first = {0}, second = {0};
    ttf_resource_layer_t delimiter_layer = TTF_LAYER_COMMON;
    size_t start = in->tokens[in->index].begin;
    size_t p = quantity_endpoint(in, start, &first, 1), separator, end;
    if (!p)
        return 0;
    separator = skip_quantity_space(in, p);
    p = quantity_separator(in, separator, &delimiter_layer);
    if (!p)
        return 0;
    start = skip_quantity_space(in, p);
    if (start >= in->length)
        return 0;
    end = quantity_endpoint(in, start, &second, 1);
    /* Preserve the established non-currency range grammar. Bare provisional
       endpoints are only promoted by explicit currency evidence. */
    if (first.type == TTF_SEM_CARDINAL &&
        (!end || second.type != TTF_SEM_CURRENCY || (!first.secondary[0] && !second.secondary[0])))
        return 0;
    if (!end) {
        /* A numeric malformed endpoint remains visible as one TEXT object. */
        if (!digit(in->text[start]) && in->text[start] != '+' && in->text[start] != '-')
            return 0;
        end = range_chain_end(in, ttf_numeric_atom_end(in, start));
        return preserve(in, out, end, "syntax.invalid-quantity-range");
    }
    if (quantity_separator(in, skip_quantity_space(in, end), NULL))
        return preserve(in, out, range_chain_end(in, end), "syntax.invalid-quantity-range");
    if ((first.type == TTF_SEM_CURRENCY && second.type == TTF_SEM_CARDINAL) ||
        (first.type == TTF_SEM_CARDINAL && second.type == TTF_SEM_CURRENCY)) {
        ttf_semantic_t *bare = first.type == TTF_SEM_CARDINAL ? &first : &second;
        ttf_semantic_t *money = first.type == TTF_SEM_CURRENCY ? &first : &second;
        /* A trailing scale shared by an unscaled first endpoint applies to both
           values. Explicit currency on both endpoints prevents this inference. */
        if (!first.secondary[0] && second.secondary[0]) {
            if (!expand_exponent(first.value, scale_power(second.secondary)))
                return preserve(in, out, end, "syntax.invalid-quantity-range");
        } else if (first.secondary[0] && !second.secondary[0])
            return preserve(in, out, end, "syntax.invalid-quantity-range");
        memcpy(bare->unit, money->unit, sizeof(bare->unit));
        bare->type = TTF_SEM_CURRENCY;
    }
    /* 3월 1일~3월 5일: neighbouring units of one declared family belong to
       different objects, so each side is read on its own instead of the whole
       span staying verbatim. Unrelated units keep the preserved contract. */
    if (first.type == second.type && first.type != TTF_SEM_TEXT &&
        strcmp(first.unit, second.unit) && same_unit_family(in, first.unit, second.unit) &&
        preceding_family_unit(in, in->tokens[in->index].begin, first.unit))
        return 0;
    if (first.type == TTF_SEM_TEXT || second.type == TTF_SEM_TEXT || first.type != second.type ||
        strcmp(first.unit, second.unit) ||
        (in->text[separator] == '-' && p == start &&
         (in->text[start] == '-' || in->text[start] == '+')))
        return preserve(in, out, end, "syntax.invalid-quantity-range");
    memcpy(out->value, first.value, sizeof(out->value));
    memcpy(out->secondary, second.value, sizeof(out->secondary));
    memcpy(out->unit, first.unit, sizeof(out->unit));
    if (first.source_lang[0] || second.source_lang[0])
        memcpy(out->source_lang, first.source_lang[0] ? first.source_lang : second.source_lang,
               sizeof(out->source_lang));
    out->layer = first.layer > second.layer ? first.layer : second.layer;
    if (delimiter_layer > out->layer)
        out->layer = delimiter_layer;
    return finish(in, out, end, TTF_SEM_RANGE,
                  first.type == TTF_SEM_TIME ? "syntax.time-range"
                                             : "syntax.repeated-quantity-range");
}

/* 9/25, 2026/9/25: a slash date is only unambiguous when the day cannot be a
   month, a four-digit year leads, or a declared weekday follows in brackets.
   Everything else stays the fraction grammar. */
static int calendar_day(unsigned month, unsigned day) {
    static const unsigned days[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month >= 1 && month <= 12 && day >= 1 && day <= days[month - 1];
}
static int weekday_bracket(const ttf_pack_input_t *in, size_t p) {
    const char *s = in->text;
    size_t n = in->length, end;
    char key[32];
    if (p >= n || s[p] != '(')
        return 0;
    end = ++p;
    while (end < n && s[end] != ')' && end - p + 1 < sizeof(key))
        ++end;
    if (end >= n || s[end] != ')' || end == p)
        return 0;
    memcpy(key, s + p, end - p);
    key[end - p] = 0;
    for (size_t pack = 0; pack < in->engine->pack_count; ++pack) {
        const char *value = ttf_resource_get(in->engine, "weekday_label",
                                             in->engine->packs[pack]->language_tag, key, NULL);
        if (value && strcmp(value, "0"))
            return 1;
    }
    return 0;
}
static size_t slash_date(const ttf_pack_input_t *in, size_t b, ttf_semantic_t *out) {
    const char *s = in->text;
    size_t n = in->length, p = b, first_end, second_end, third_end;
    unsigned first, second, third;
    if (b && (digit(s[b - 1]) || s[b - 1] == '/' || s[b - 1] == '.' || ascii_letter(s[b - 1])))
        return 0;
    first_end = integer(s, n, p);
    if (first_end == p || first_end - p > 4 || first_end >= n || s[first_end] != '/')
        return 0;
    first = small_uint(s + p, first_end - p);
    p = first_end + 1;
    second_end = integer(s, n, p);
    if (second_end == p || second_end - p > 2)
        return 0;
    second = small_uint(s + p, second_end - p);
    if (first_end - b == 4) {
        if (second_end >= n || s[second_end] != '/')
            return 0;
        p = second_end + 1;
        third_end = integer(s, n, p);
        if (third_end == p || third_end - p > 2 || !first || first > 9999 ||
            !calendar_day(second, third = small_uint(s + p, third_end - p)))
            return 0;
        if (third_end < n && (digit(s[third_end]) || s[third_end] == '/' ||
                              ascii_letter(s[third_end]) || s[third_end] == '.'))
            return 0;
        snprintf(out->value, sizeof(out->value), "%04u-%02u-%02u", first, second, third);
        return finish(in, out, third_end, TTF_SEM_DATE, "syntax.slash-date");
    }
    if (first_end - b > 2 || !calendar_day(first, second))
        return 0;
    if (second_end < n && (digit(s[second_end]) || s[second_end] == '/' ||
                           ascii_letter(s[second_end]) || s[second_end] == '.'))
        return 0;
    if (second <= 12 && !weekday_bracket(in, second_end))
        return 0;
    snprintf(out->value, sizeof(out->value), "--%02u-%02u", first, second);
    snprintf(out->unit, sizeof(out->unit), "month-day");
    return finish(in, out, second_end, TTF_SEM_DATE, "syntax.slash-date");
}

size_t ttf_classify_universal(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *s = in->text, *rule_value = NULL;
    size_t n = in->length, b = in->tokens[in->index].begin;
    size_t p = b, q, r, end, a_end;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    if (b >= n)
        return 0;

    /* A numeric-looking keycap is one pictographic object, not a cardinal.
       Preserve its complete source bytes for the configurable character pass. */
    if (digit(s[b]) || s[b] == '#' || s[b] == '*') {
        size_t cluster_end = b + 1;
        if (n - cluster_end >= 3 && !memcmp(s + cluster_end, "\xef\xb8\x8f", 3))
            cluster_end += 3;
        if (n - cluster_end >= 3 && !memcmp(s + cluster_end, "\xe2\x83\xa3", 3)) {
            cluster_end += 3;
            snprintf(out->unit, sizeof(out->unit), "@unreadable-cluster");
            copy_value(out->value, sizeof(out->value), s + b, cluster_end - b);
            return finish(in, out, cluster_end, TTF_SEM_SYMBOL, "syntax.keycap-cluster");
        }
    }
    /* abc_123, @daily_log: an identifier is transmitted like an address, so the
       target reads declared words and spells the rest. A bare word without an
       underscore or handle mark keeps ordinary lexical grammar. */
    {
        size_t p = b, letters = 0, digits_seen = 0, underscores = 0;
        int handle = s[b] == '@' && (!b || !(ascii_letter(s[b - 1]) || digit(s[b - 1]) ||
                                             s[b - 1] == '_' || s[b - 1] == '@'));
        if (handle)
            ++p;
        if ((handle || !b ||
             !(ascii_letter(s[b - 1]) || digit(s[b - 1]) || s[b - 1] == '_' || s[b - 1] == '@' ||
               s[b - 1] == '.')) &&
            p < n && (ascii_letter(s[p]) || s[p] == '_')) {
            size_t start = p;
            while (p < n && (ascii_letter(s[p]) || digit(s[p]) || s[p] == '_')) {
                if (s[p] == '_')
                    ++underscores;
                else if (digit(s[p]))
                    ++digits_seen;
                else
                    ++letters;
                ++p;
            }
            (void)digits_seen;
            if (underscores && letters && p - start < TTF_IR_VALUE_SIZE &&
                !(p < n && (s[p] == '.' || s[p] == '@' || s[p] == '-'))) {
                copy_value(out->value, sizeof(out->value), s + start, p - start);
                snprintf(out->unit, sizeof(out->unit), "@handle");
                return finish(in, out, p, TTF_SEM_CUSTOM,
                              handle ? "syntax.handle" : "syntax.identifier");
            }
        }
    }
    /* #1: a number sign before digits names an ordinal position. The target
       pack supplies the word order; a hashtag or C# keeps its symbol name. */
    if (s[b] == '#' && b + 1 < n && digit(s[b + 1]) && s[b + 1] != '0' &&
        (!b || !(ascii_letter(s[b - 1]) || digit(s[b - 1]) || s[b - 1] == '#'))) {
        size_t value_end = integer(s, n, b + 1);
        uint32_t cp;
        size_t width;
        int tail_ok = value_end == n;
        if (!tail_ok && ttf_utf8_decode(s + value_end, n - value_end, &cp, &width) == 1) {
            ttf_script_t script = ttf_script_of(cp);
            tail_ok = script != TTF_SCRIPT_HANGUL && script != TTF_SCRIPT_LATIN &&
                      script != TTF_SCRIPT_DIGIT;
            /* A declared particle attached to the position is still the tail. */
            if (!tail_ok && script == TTF_SCRIPT_HANGUL) {
                size_t token = in->index;
                while (token < in->token_count && in->tokens[token].begin < value_end)
                    ++token;
                if (token < in->token_count && in->tokens[token].begin == value_end &&
                    in->tokens[token].end - value_end < 32) {
                    char tail[32];
                    size_t width_tail = in->tokens[token].end - value_end;
                    memcpy(tail, s + value_end, width_tail);
                    tail[width_tail] = 0;
                    for (size_t pack = 0; pack < in->engine->pack_count; ++pack)
                        if (ttf_resource_get(in->engine, "particle",
                                             in->engine->packs[pack]->language_tag, tail, NULL)) {
                            tail_ok = 1;
                            break;
                        }
                }
            }
        }
        if (value_end - b - 1 < sizeof(out->value) && tail_ok) {
            copy_value(out->value, sizeof(out->value), s + b + 1, value_end - b - 1);
            snprintf(out->unit, sizeof(out->unit), "@number-sign");
            return finish(in, out, value_end, TTF_SEM_SYMBOL, "syntax.number-sign");
        }
    }
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
            out->layer = layer > out->layer ? layer : out->layer;
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", colon + 1);
            return finish(in, out, end, type, "resource.semantic-term");
        }
    }
    /* A bare web prefix is an initialism. A following ASCII host still takes
       the complete URL path below. Its dot is an abbreviation marker before an
       attached declared particle, otherwise it remains sentence punctuation. */
    if (n - b >= 4 && (s[b] == 'w' || s[b] == 'W') && (s[b + 1] == 'w' || s[b + 1] == 'W') &&
        (s[b + 2] == 'w' || s[b + 2] == 'W') && s[b + 3] == '.' &&
        (b + 4 == n || !ascii_web(s[b + 4]))) {
        size_t consumed_end = b + 3, token = in->index;
        while (token < in->token_count && in->tokens[token].begin < b + 4)
            ++token;
        if (token < in->token_count && in->tokens[token].begin == b + 4 &&
            in->tokens[token].end - in->tokens[token].begin < 64) {
            char tail[64];
            size_t width = in->tokens[token].end - in->tokens[token].begin;
            memcpy(tail, s + b + 4, width);
            tail[width] = 0;
            for (size_t pack = 0; pack < in->engine->pack_count; ++pack)
                if (ttf_resource_get(in->engine, "particle", in->engine->packs[pack]->language_tag,
                                     tail, NULL)) {
                    consumed_end = b + 4;
                    break;
                }
        }
        snprintf(out->value, sizeof(out->value), "www");
        out->lexical_class = TTF_LEX_INITIALISM;
        return finish(in, out, consumed_end, TTF_SEM_INITIALISM, "syntax.web-initialism");
    }
    if ((q = math_expression_end(in, b, out)) > b) {
        snprintf(out->unit, sizeof(out->unit), "@expression");
        return finish(in, out, q, TTF_SEM_SYMBOL, "syntax.math-expression");
    }
    out->value[0] = '\0';
    /* Binary plus is distinct from a unary sign. Keep the operator as its
       own IR span; target pack resources decide its reading. */
    if (s[b] == '+') {
        size_t left = b, right = skip_space(s, n, b + 1);
        while (left && (s[left - 1] == ' ' || s[left - 1] == '\t'))
            --left;
        if (right < n && (s[right] == '+' || s[right] == '-'))
            ++right;
        if (left && digit(s[left - 1]) && right < n &&
            (digit(s[right]) || (s[right] == '.' && right + 1 < n && digit(s[right + 1])))) {
            snprintf(out->value, sizeof(out->value), "+");
            snprintf(out->unit, sizeof(out->unit), "@math");
            return finish(in, out, b + 1, TTF_SEM_SYMBOL, "syntax.binary-operator");
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
    /* Declared currency prefixes may begin with letters (for example US$).
       Let the common numeric grammar consume the whole resource-defined prefix
       before the Latin lexical fast path; unknown spellings still stay lexical. */
    if (in->tokens[in->index].script == TTF_SCRIPT_LATIN &&
        !resource_prefix(in, "currency_prefix", b, &rule_value, &end, &layer)) {
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
        if (!at && (q = bare_domain_end(in, b)) > b)
            return finish(in, out, q, TTF_SEM_URL, "syntax.bare-domain");
        if ((s[b] == 'v' || s[b] == 'V') && in->tokens[in->index].end == b + 1 &&
            (!b || !(ascii_letter(s[b - 1]) || digit(s[b - 1]) || s[b - 1] == '.'))) {
            unsigned parts = 0;
            q = b + 1;
            while (q < n && digit(s[q])) {
                q = integer(s, n, q);
                ++parts;
                if (q + 1 < n && s[q] == '.' && digit(s[q + 1]))
                    ++q;
                else
                    break;
            }
            if (parts >= 2 && !(q < n && (ascii_letter(s[q]) || s[q] == '_' || s[q] == '-' ||
                                          (s[q] == '.' && q + 1 < n && ascii_letter(s[q + 1]))))) {
                out->secondary[0] = s[b];
                out->secondary[1] = '\0';
                return finish(in, out, q, TTF_SEM_VERSION, "syntax.prefixed-version");
            }
        }
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

    if (resource_prefix(in, "currency_prefix", b, &rule_value, &end, &layer) ||
        (b + 1 < n && (s[b] == '-' || s[b] == '+') &&
         resource_prefix(in, "currency_prefix", b + 1, &rule_value, &end, &layer))) {
        size_t start = skip_quantity_space(in, end);
        ttf_semantic_t quantity = {0};
        p = quantity_endpoint(in, b, &quantity, 0);
        if (p) {
            /* Prefix and suffix must describe the same currency. Reuse the
               interval endpoint contract for single amounts too, so a written
               currency name is neither duplicated nor silently contradicted. */
            if (quantity.type == TTF_SEM_TEXT)
                return preserve(in, out, p, "syntax.invalid-currency");
            memcpy(out->value, quantity.value, sizeof(out->value));
            memcpy(out->unit, quantity.unit, sizeof(out->unit));
            memcpy(out->secondary, quantity.secondary, sizeof(out->secondary));
            memcpy(out->source_lang, quantity.source_lang, sizeof(out->source_lang));
            out->layer = quantity.layer;
            return finish(in, out, p, TTF_SEM_CURRENCY,
                          out->secondary[0] ? "syntax.scaled-currency" : "syntax.currency-prefix");
        }
        if (start < n && (digit(s[start]) || s[start] == '+' || s[start] == '-'))
            return preserve(in, out, ttf_numeric_atom_end(in, start), "syntax.invalid-currency");
    }
    {
        ttf_semantic_t quantity = {0};
        p = quantity_endpoint(in, b, &quantity, 0);
        if (p && quantity.type == TTF_SEM_CURRENCY) {
            memcpy(out->value, quantity.value, sizeof(out->value));
            memcpy(out->unit, quantity.unit, sizeof(out->unit));
            memcpy(out->secondary, quantity.secondary, sizeof(out->secondary));
            memcpy(out->source_lang, quantity.source_lang, sizeof(out->source_lang));
            out->layer = quantity.layer;
            return finish(in, out, p, TTF_SEM_CURRENCY,
                          quantity.secondary[0] ? "syntax.scaled-currency"
                                                : "syntax.numeric-suffix");
        }
        if (p && quantity.type == TTF_SEM_TEXT && quantity.unit[0] && strcmp(quantity.unit, "time"))
            return preserve(in, out, p, "syntax.invalid-currency");
    }
    /* Phone syntax and its pending-prefix grammar share one bounded scanner. */
    {
        size_t phone = ttf_classify_phone(in, out);
        if (phone)
            return phone;
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
                if (year && day >= 1 && day <= maxday && !structured_tail(in, end)) {
                    (void)snprintf(out->value, sizeof(out->value), "%04u-%02u-%02u", year, month,
                                   day);
                    if (separator == '.' && end < n && s[end] == '.')
                        ++end;
                    return finish(in, out, end, TTF_SEM_DATE, "syntax.date");
                }
                return preserve(in, out, ttf_numeric_atom_end(in, end), "syntax.invalid-date");
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
                !structured_tail(in, suffix_end)) {
                valid = valid && hour >= 1 && hour <= 12 &&
                        copy_value(out->secondary, sizeof(out->secondary), rule_value,
                                   strlen(rule_value));
                end = suffix_end;
                out->layer = layer > out->layer ? layer : out->layer;
            }
        }
        if (valid && !structured_tail(in, end)) {
            if (has_seconds)
                (void)snprintf(out->value, sizeof(out->value), "%02u:%02u:%02u", hour, minute,
                               second);
            else
                (void)snprintf(out->value, sizeof(out->value), "%02u:%02u", hour, minute);
            return finish(in, out, end, TTF_SEM_TIME, "syntax.time");
        }
        return preserve(in, out, ttf_numeric_atom_end(in, end), "syntax.invalid-time");
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
        if (parts >= 3) {
            if (ttf_numeric_group_link(in->engine, s, n, p))
                return preserve(in, out, ttf_numeric_atom_end(in, b), "syntax.invalid-version");
            return finish(in, out, p, (parts == 4 && ip) ? TTF_SEM_IP_ADDRESS : TTF_SEM_VERSION,
                          (parts == 4 && ip) ? "syntax.ip" : "syntax.version");
        }
    }
    p = ttf_numeric_value(in, b, out->value, &out->layer);
    if (p == b)
        return preserve(in, out, ttf_numeric_atom_end(in, b), "syntax.invalid-number");
    q = skip_quantity_space(in, p);
    {
        ttf_resource_layer_t separator_layer = TTF_LAYER_COMMON;
        size_t separator_end = quantity_separator(in, q, &separator_layer);
        int legacy_hyphen = q < n && s[q] == '-';
        /* Keep the established bare-hyphen compatibility grammar; resource
           delimiters additionally provide explicit ranges such as 1~2. */
        if (!separator_end && legacy_hyphen)
            separator_end = q + 1;
        if (separator_end) {
            size_t start = skip_quantity_space(in, separator_end);
            if (legacy_hyphen && start == separator_end && start < n &&
                (s[start] == '-' || s[start] == '+'))
                return preserve(in, out, ttf_numeric_atom_end(in, start), "syntax.invalid-range");
            end = ttf_numeric_value(in, start, out->secondary, &out->layer);
            if (end > start) {
                size_t suffix_start = skip_quantity_space(in, end), suffix_end;
                ttf_semantic_type_t suffix_type;
                if (quantity_suffix(in, suffix_start, out, &suffix_end, &suffix_type, 1)) {
                    if (suffix_type == TTF_SEM_TEXT)
                        return preserve(in, out, suffix_end, "syntax.invalid-range");
                    if (suffix_type == TTF_SEM_PERCENT)
                        (void)snprintf(out->unit, sizeof(out->unit), "percent");
                    if (quantity_separator(in, skip_quantity_space(in, suffix_end), NULL))
                        return preserve(in, out, range_chain_end(in, suffix_end),
                                        "syntax.invalid-range");
                    if (separator_layer > out->layer)
                        out->layer = separator_layer;
                    return finish(in, out, suffix_end, TTF_SEM_RANGE, "syntax.quantity-range");
                }
                if (quantity_separator(in, skip_quantity_space(in, end), NULL))
                    return preserve(in, out, range_chain_end(in, end), "syntax.invalid-range");
                if (!structured_tail(in, end)) {
                    out->layer = separator_layer > out->layer ? separator_layer : out->layer;
                    return finish(in, out, end, TTF_SEM_RANGE,
                                  legacy_hyphen ? "syntax.range" : "syntax.explicit-range");
                }
                return preserve(in, out, ttf_numeric_atom_end(in, end), "syntax.invalid-range");
            }
            if ((!legacy_hyphen && start == n) ||
                (start < n &&
                 (digit(s[start]) || s[start] == '+' || s[start] == '-' || s[start] == '.')))
                return preserve(in, out, ttf_numeric_atom_end(in, start), "syntax.invalid-range");
        }
    }
    if (q < n && s[q] == '/') {
        int nonzero = 0;
        size_t dated = slash_date(in, b, out);
        if (dated)
            return dated;
        size_t start = skip_quantity_space(in, q + 1);
        end = integer(s, n, start);
        for (r = start; r < end; ++r)
            nonzero |= s[r] != '0';
        if (!strchr(out->value, '.') && end > start && nonzero && !structured_tail(in, end) &&
            copy_value(out->secondary, sizeof(out->secondary), s + start, end - start))
            return finish(in, out, end, TTF_SEM_FRACTION, "syntax.fraction");
        if (end > start || q == p)
            return preserve(in, out, ttf_numeric_atom_end(in, end), "syntax.invalid-fraction");
    }
    if (resource_prefix(in, "ordinal_suffix", p, &rule_value, &end, &layer)) {
        if (!structured_tail(in, end) &&
            valid_ordinal(in, rule_value, out->value, s + p, end - p)) {
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", rule_value);
            out->layer = layer > out->layer ? layer : out->layer;
            return finish(in, out, end, TTF_SEM_ORDINAL, "syntax.ordinal");
        }
        return preserve(in, out, ttf_numeric_atom_end(in, b), "syntax.invalid-ordinal");
    }
    q = skip_quantity_space(in, p);
    {
        ttf_semantic_type_t type;
        if (quantity_suffix(in, q, out, &end, &type, 0))
            return finish(in, out, end, type,
                          type == TTF_SEM_PERCENT ? "syntax.percent" : "syntax.numeric-suffix");
    }
    if (structured_tail(in, p))
        return preserve(in, out, ttf_numeric_atom_end(in, b), "syntax.unsupported-numeric");
    return finish(in, out, p, TTF_SEM_CARDINAL, "syntax.cardinal");
}
