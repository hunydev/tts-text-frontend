/* Korean ordinal morphology and explicitly cued count/identifier roles.
 * Numeric values stay decimal strings; all lexical cues are resources. */
#include "counters.h"
#include "../../src/core/internal.h"
#include "../../src/semantic/numeric_grouping.h"

static size_t spaces(const ttf_pack_input_t *in, size_t p) {
    while (p < in->length && (in->text[p] == ' ' || in->text[p] == '\t'))
        ++p;
    return p;
}

/* Match effective resources at complete UTF-8 boundaries, longest key first. */
static size_t match(const ttf_pack_input_t *in, const char *kind, size_t p, char *key,
                    size_t capacity, ttf_resource_layer_t *layer) {
    size_t width, best = 0;
    ttf_resource_layer_t selected_layer = TTF_LAYER_COMMON;
    char candidate[96];
    for (width = 1; width < sizeof(candidate) && width < capacity && p + width <= in->length;
         ++width) {
        const char *value;
        ttf_resource_layer_t found;
        if (p + width < in->length && ((unsigned char)in->text[p + width] & 0xc0u) == 0x80u)
            continue;
        memcpy(candidate, in->text + p, width);
        candidate[width] = 0;
        value = ttf_resource_get(in->engine, kind, "ko-KR", candidate, &found);
        if (value && strcmp(value, "0")) {
            best = width;
            memcpy(key, candidate, width + 1);
            selected_layer = found;
        }
    }
    if (best && selected_layer > *layer)
        *layer = selected_layer;
    return best;
}

static int tail(const ttf_pack_input_t *in, size_t end) {
    size_t i;
    if (end == in->length)
        return 1;
    for (i = in->index; i < in->token_count; ++i) {
        const ttf_token_t *token = &in->tokens[i];
        if (token->begin <= end && end < token->end) {
            char key[128];
            size_t n = token->end - end;
            if (token->script == TTF_SCRIPT_HANGUL) {
                if (n >= sizeof(key))
                    return 0;
                memcpy(key, in->text + end, n);
                key[n] = 0;
                return ttf_resource_get(in->engine, "particle", "ko-KR", key, NULL) ||
                       ttf_resource_get(in->engine, "quantity_tail", "ko-KR", key, NULL);
            }
            return token->script == TTF_SCRIPT_WHITESPACE ||
                   token->script == TTF_SCRIPT_PUNCTUATION || token->script == TTF_SCRIPT_SYMBOL;
        }
    }
    return 0;
}

static size_t number(const ttf_pack_input_t *in, size_t p, char *value, int *valid,
                     ttf_resource_layer_t *layer) {
    size_t start = p, i;
    int positive = 0;
    *valid = 0;
    if (p >= in->length || !((in->text[p] >= '0' && in->text[p] <= '9') || in->text[p] == '+' ||
                             in->text[p] == '-' || in->text[p] == '.'))
        return p;
    p = ttf_numeric_value(in, start, value, layer);
    *valid = p != start;
    if (p == start)
        return ttf_numeric_atom_end(in, start);
    for (i = 0; value[i]; ++i) {
        if (value[i] < '0' || value[i] > '9')
            *valid = 0;
        else
            positive |= value[i] != '0';
    }
    /* Ordinal spelling supports positive integers, not exponent/decimal forms
       which happen to expand to an integer. Keep those forms intact. */
    for (i = start; i < p; ++i)
        if (in->text[i] == '.' || in->text[i] == 'e' || in->text[i] == 'E')
            *valid = 0;
    if (!positive)
        *valid = 0;
    if (p + 1 < in->length && in->text[p] == '.' && in->text[p + 1] >= '0' &&
        in->text[p + 1] <= '9') {
        p = ttf_numeric_atom_end(in, start);
        *valid = 0;
    }
    return p;
}

static int role_value(const char *value) {
    return value && !strcmp(value, "identifier") ? 2 : value && !strcmp(value, "frequency") ? 1 : 0;
}

static int role(const ttf_pack_input_t *in, size_t begin, size_t end, ttf_resource_layer_t *layer) {
    size_t i, p = begin, q;
    int found = 0;
    ttf_resource_layer_t right_layer = TTF_LAYER_COMMON;
    char key[96];
    while (p && (in->text[p - 1] == ' ' || in->text[p - 1] == '\t'))
        --p;
    for (i = in->index; i > 0; --i) {
        const ttf_token_t *token = &in->tokens[i - 1];
        ttf_resource_layer_t source;
        const char *value;
        int current;
        if (token->begin >= p)
            continue;
        if (token->end != p || p - token->begin >= sizeof(key))
            break;
        memcpy(key, in->text + token->begin, p - token->begin);
        key[p - token->begin] = 0;
        value = ttf_resource_get(in->engine, "counter_role_before", "ko-KR", key, &source);
        current = role_value(value);
        if (current) {
            found = current;
            if (source > *layer)
                *layer = source;
        }
        break;
    }
    p = end;
    for (i = in->index; i < in->token_count; ++i) {
        const ttf_token_t *token = &in->tokens[i];
        if (token->script == TTF_SCRIPT_HANGUL && token->begin <= p && p < token->end &&
            token->end - p < sizeof(key)) {
            memcpy(key, in->text + p, token->end - p);
            key[token->end - p] = 0;
            if (ttf_resource_get(in->engine, "particle", "ko-KR", key, NULL) ||
                ttf_resource_get(in->engine, "quantity_tail", "ko-KR", key, NULL))
                p = token->end;
            break;
        }
    }
    q = p;
    p = spaces(in, p);
    /* The right cue must be a following word, never an attached unrelated noun. */
    if (p == q)
        return found;
    q = match(in, "counter_role_after", p, key, sizeof(key), &right_layer);
    if (q) {
        int current =
            role_value(ttf_resource_get(in->engine, "counter_role_after", "ko-KR", key, NULL));
        if (current >= found && current) {
            found = current;
            if (right_layer > *layer)
                *layer = right_layer;
        }
    }
    return found;
}

static size_t finish(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t end,
                     ttf_semantic_type_t type, const char *rule) {
    size_t count = 0;
    while (in->index + count < in->token_count && in->tokens[in->index + count].begin < end)
        ++count;
    s->input_end = end;
    s->type = type;
    snprintf(s->source_lang, sizeof(s->source_lang), "ko-KR");
    snprintf(s->rule, sizeof(s->rule), "%s", rule);
    if (type == TTF_SEM_TEXT)
        s->value[0] = s->secondary[0] = s->unit[0] = 0;
    return count;
}

/* Keep an unsupported ordinal with its attached word instead of reading only
   a prefix of a malformed identifier. Punctuation/whitespace remain outside. */
static size_t attached_end(const ttf_pack_input_t *in, size_t end) {
    size_t i;
    for (i = in->index; i < in->token_count; ++i) {
        const ttf_token_t *token = &in->tokens[i];
        if (token->end <= end)
            continue;
        if (token->begin > end || token->script == TTF_SCRIPT_WHITESPACE ||
            token->script == TTF_SCRIPT_PUNCTUATION || token->script == TTF_SCRIPT_SYMBOL)
            break;
        end = token->end;
    }
    return end;
}

size_t ttf_ko_classify_counter(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    size_t begin = in->tokens[in->index].begin, start = begin, p, q, width, end;
    int prefix = 0, valid, right_valid = 1, is_range = 0, detected_role = 0;
    char unit[32] = "", right_unit[32] = "", separator[32];
    const char *prefix_key =
        ttf_resource_get(in->engine, "ordinal_marker", "ko-KR", "prefix", NULL);
    s->layer = TTF_LAYER_SYSTEM;
    if (prefix_key && *prefix_key && in->length - begin >= strlen(prefix_key) &&
        !memcmp(in->text + begin, prefix_key, strlen(prefix_key))) {
        prefix = 1;
        start = spaces(in, begin + strlen(prefix_key));
    }
    p = number(in, start, s->value, &valid, &s->layer);
    if (p == start)
        return 0;
    end = p;
    q = spaces(in, p);
    if (prefix) {
        /* 제 written against the number is the ordinal prefix ("제0" stays as
           written); 제 with a space may be the ordinary word, so a candidate
           there is only kept when it actually met an ordinal unit. */
        int attached = start == begin + strlen(prefix_key), had_unit;
        width = match(in, "ordinal_prefix_unit", q, unit, sizeof(unit), &s->layer);
        had_unit = width != 0;
        if (width)
            end = q + width;
        p = spaces(in, end);
        width = match(in, "quantity_range_separator", p, separator, sizeof(separator), &s->layer);
        if (width) {
            size_t rhs = spaces(in, p + width);
            if (prefix_key && in->length - rhs >= strlen(prefix_key) &&
                !memcmp(in->text + rhs, prefix_key, strlen(prefix_key)))
                rhs = spaces(in, rhs + strlen(prefix_key));
            p = number(in, rhs, s->secondary, &right_valid, &s->layer);
            if (p != rhs) {
                q = spaces(in, p);
                width =
                    match(in, "ordinal_prefix_unit", q, right_unit, sizeof(right_unit), &s->layer);
                /* 제 is also an ordinary word: "제 3-5" is 제 plus a range. Only a
                   candidate that met an ordinal unit is preserved. */
                if (!had_unit && !width && !attached)
                    return 0;
                return finish(in, s, attached_end(in, width ? q + width : p), TTF_SEM_TEXT,
                              "ko:invalid-ordinal");
            }
        }
        /* 제 is also an ordinary word, so a number after it that carries a
           measure is not a malformed ordinal: "제 10kg" is 제 plus 십 킬로그램.
           Only a candidate that already matched an ordinal unit is preserved. */
        if (!tail(in, end))
            return had_unit || attached
                       ? finish(in, s, attached_end(in, end), TTF_SEM_TEXT, "ko:invalid-ordinal")
                       : 0;
        if (!valid && !had_unit && !attached)
            return 0;
        snprintf(s->unit, sizeof(s->unit), "%s", unit);
        snprintf(s->secondary, sizeof(s->secondary), "prefix");
        return finish(in, s, end, valid ? TTF_SEM_ORDINAL : TTF_SEM_TEXT,
                      valid ? "ko:prefix-ordinal" : "ko:invalid-ordinal");
    }
    width = match(in, "native_ordinal_suffix", q, unit, sizeof(unit), &s->layer);
    if (!width)
        width = match(in, "counter_role_unit", q, unit, sizeof(unit), &s->layer);
    if (width)
        end = q + width;
    p = spaces(in, end);
    width = match(in, "quantity_range_separator", p, separator, sizeof(separator), &s->layer);
    if (width) {
        q = spaces(in, p + width);
        p = number(in, q, s->secondary, &right_valid, &s->layer);
        if (p != q) {
            q = spaces(in, p);
            width =
                match(in, "native_ordinal_suffix", q, right_unit, sizeof(right_unit), &s->layer);
            if (!width)
                width =
                    match(in, "counter_role_unit", q, right_unit, sizeof(right_unit), &s->layer);
            if (*unit || width) {
                is_range = 1;
                end = width ? q + width : p;
                if (width && *unit && strcmp(unit, right_unit))
                    right_valid = 0;
                if (width && !*unit)
                    snprintf(unit, sizeof(unit), "%s", right_unit);
                if (!tail(in, end)) {
                    right_valid = 0;
                    end = attached_end(in, end);
                }
            }
        }
    }
    if (!is_range) {
        right_valid = 1;
        s->secondary[0] = 0;
    }
    if (!*unit)
        return 0;
    if (!tail(in, end)) {
        if (ttf_resource_get(in->engine, "native_ordinal_suffix", "ko-KR", unit, NULL))
            return finish(in, s, attached_end(in, end), TTF_SEM_TEXT, "ko:invalid-ordinal");
        return 0;
    }
    snprintf(s->unit, sizeof(s->unit), "%s", unit);
    if (ttf_resource_get(in->engine, "native_ordinal_suffix", "ko-KR", unit, NULL)) {
        if (!is_range)
            s->secondary[0] = 0;
        return finish(in, s, end,
                      !valid || !right_valid ? TTF_SEM_TEXT
                      : is_range             ? TTF_SEM_RANGE
                                             : TTF_SEM_ORDINAL,
                      !valid || !right_valid ? "ko:invalid-ordinal"
                      : is_range             ? "ko:ordinal-range"
                                             : "ko:suffix-ordinal");
    }
    detected_role = role(in, begin, end, &s->layer);
    if (!detected_role || !valid || !right_valid)
        return 0;
    if (!is_range)
        snprintf(s->secondary, sizeof(s->secondary), "%s",
                 detected_role == 2 ? "identifier" : "frequency");
    return finish(in, s, end, is_range ? TTF_SEM_RANGE : TTF_SEM_COUNTER,
                  is_range ? (detected_role == 2 ? "ko:identifier-range" : "ko:frequency-range")
                  : detected_role == 2 ? "ko:identifier-counter"
                                       : "ko:frequency-counter");
}
