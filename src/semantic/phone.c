#include "phone.h"

#define PHONE_SCAN_LIMIT 384
#define PHONE_MAX_GROUPS 48

static int digit(char c) {
    return c >= '0' && c <= '9';
}
static int letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int hspace(char c) {
    return c == ' ' || c == '\t';
}
static char fold(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
}
static size_t spaces(const char *s, size_t n, size_t p) {
    while (p < n && hspace(s[p]))
        ++p;
    return p;
}
static void promote(ttf_resource_layer_t *layer, ttf_resource_layer_t candidate) {
    if (candidate > *layer)
        *layer = candidate;
}
static const char *lookup(const ttf_engine_t *e, const char *kind, const char *language,
                          const char *key, ttf_resource_layer_t *layer) {
    const char *best = NULL;
    ttf_resource_layer_t best_layer = TTF_LAYER_COMMON, candidate_layer;
    size_t i;
    if (language)
        return ttf_resource_get(e, kind, language, key, layer);
    for (i = 0; i <= e->pack_count; ++i) {
        const char *candidate;
        /* A declared-only pack outside its request (ja-JP, zh-CN) owns no phone
           grammar; skipping it keeps this per-space scan from growing with packs. */
        if (i < e->pack_count && e->frozen && e->undeclared[i])
            continue;
        candidate = ttf_resource_get(
            e, kind, i < e->pack_count ? e->packs[i]->language_tag : "*", key, &candidate_layer);
        if (candidate && (!best || candidate_layer > best_layer)) {
            best = candidate;
            best_layer = candidate_layer;
        }
    }
    if (best && layer)
        *layer = best_layer;
    return best;
}
int ttf_phone_preceding_label(const ttf_pack_input_t *in, size_t begin, const char *kind,
                              const char *language, char matched[TTF_IR_VALUE_SIZE],
                              ttf_resource_layer_t *layer) {
    size_t end = begin, i;
    int found = 0, enabled = 0;
    if (!in || !in->engine || !kind || begin > in->length)
        return 0;
    if (matched)
        matched[0] = 0;
    while (end && hspace(in->text[end - 1]))
        --end;
    if (end && in->text[end - 1] == ':') {
        --end;
        while (end && hspace(in->text[end - 1]))
            --end;
    }
    for (i = in->index; i; --i) {
        const ttf_token_t *token = &in->tokens[i - 1];
        char key[TTF_IR_VALUE_SIZE];
        size_t p, used = 0, token_end = token->end < in->length ? token->end : in->length;
        int pending_space = 0;
        const char *value;
        ttf_resource_layer_t candidate_layer = TTF_LAYER_COMMON;
        if (token->begin >= end)
            continue;
        if (end - token->begin >= sizeof(key))
            break;
        if (token->script == TTF_SCRIPT_WHITESPACE) {
            for (p = token->begin; p < token_end; ++p)
                if (in->text[p] == '\n' || in->text[p] == '\r')
                    return found && enabled;
            continue;
        }
        if (token->script == TTF_SCRIPT_PUNCTUATION || token->script == TTF_SCRIPT_SYMBOL)
            break;
        for (p = token->begin; p < end; ++p) {
            char c = in->text[p];
            if (c == '\n' || c == '\r')
                return found && enabled;
            if (hspace(c)) {
                pending_space = 1;
                continue;
            }
            if (pending_space && used)
                key[used++] = ' ';
            pending_space = 0;
            key[used++] = fold(c);
        }
        key[used] = 0;
        value = lookup(in->engine, kind, language, key, &candidate_layer);
        if (value) {
            found = 1;
            enabled = !strcmp(value, !strcmp(kind, "identifier_label") ? "digits" : "1");
            if (matched)
                memcpy(matched, key, used + 1);
            if (layer)
                *layer = candidate_layer;
        }
    }
    return found && enabled;
}
static size_t policy(const ttf_engine_t *e, const char *key, size_t fallback,
                     ttf_resource_layer_t *layer) {
    ttf_resource_layer_t selected = TTF_LAYER_COMMON;
    const char *value = ttf_resource_get(e, "phone_policy", "*", key, &selected);
    if (value) {
        char *end;
        unsigned long parsed = strtoul(value, &end, 10);
        if (*value && !*end && parsed && parsed <= 95) {
            promote(layer, selected);
            return (size_t)parsed;
        }
    }
    return fallback;
}
static int marker_boundary(const char *s, size_t n, size_t p) {
    uint32_t cp;
    size_t width;
    ttf_script_t script;
    if (p == n || digit(s[p]))
        return 1;
    if (ttf_utf8_decode(s + p, n - p, &cp, &width) != 1)
        return 0;
    script = ttf_script_of(cp);
    return script == TTF_SCRIPT_WHITESPACE || script == TTF_SCRIPT_PUNCTUATION ||
           script == TTF_SCRIPT_SYMBOL;
}

/* A complete marker must not match the beginning of another word. Digits may be
   attached (ext0042, x0042); resources define the marker spellings. */
static size_t marker(const ttf_engine_t *e, const char *s, size_t n, size_t begin,
                     ttf_resource_layer_t *layer, int prefix, const char *kind) {
    size_t p = begin, best = 0;
    char key[TTF_IR_VALUE_SIZE];
    (void)prefix;
    while (p < n && p - begin + 1 < sizeof(key) && !hspace(s[p]) && s[p] != '\n' && s[p] != '\r' &&
           s[p] != ':' && !digit(s[p])) {
        ttf_resource_layer_t selected = TTF_LAYER_COMMON;
        const char *value;
        key[p - begin] = fold(s[p]);
        ++p;
        key[p - begin] = 0;
        value = lookup(e, kind, NULL, key, &selected);
        if (value && marker_boundary(s, n, p)) {
            /* The longest known marker wins even when disabled. */
            best = !strcmp(value, "1") ? p : 0;
            promote(layer, selected);
        }
    }
    return best;
}
static size_t extension(const ttf_engine_t *e, const char *s, size_t n, size_t begin,
                        ttf_resource_layer_t *layer, int prefix) {
    return marker(e, s, n, begin, layer, prefix, "phone_extension");
}
/* Expand an invalid candidate without a value buffer or digit-count truncation.
   Only phone-shaped continuation and an anchored extension may cross whitespace.
   This scanner is linear even for inputs reaching max_pending_bytes. */
static size_t invalid_end(const ttf_pack_input_t *in, size_t begin) {
    const char *s = in->text;
    size_t p = begin, n = in->length;
    int after_space = 0;
    while (p < n) {
        unsigned char c = (unsigned char)s[p];
        if (digit((char)c) || c == '+' || c == '-' || c == '(' || c == ')' || c == '/' ||
            c == ':' || c == '#' || c == '~') {
            ++p;
            after_space = 0;
            continue;
        }
        if (c == '.' || c == ',') {
            size_t q = p;
            while (q < n && (s[q] == '.' || s[q] == ','))
                ++q;
            if (q < n && digit(s[q])) {
                p = q;
                continue;
            }
        }
        if (hspace((char)c)) {
            size_t q = spaces(s, n, p), ext;
            ttf_resource_layer_t ignored = TTF_LAYER_COMMON;
            ext = extension(in->engine, s, n, q, &ignored, 0);
            if (q < n &&
                (digit(s[q]) || s[q] == '(' || s[q] == ')' || s[q] == '+' || s[q] == '-' || ext)) {
                p = q;
                after_space = 1;
                continue;
            }
            break;
        }
        if (c >= 128) {
            uint32_t cp;
            size_t width;
            if (ttf_utf8_decode(s + p, n - p, &cp, &width) == 1 &&
                ttf_script_of(cp) == TTF_SCRIPT_WHITESPACE && cp != 0x2028 && cp != 0x2029) {
                p += width;
                after_space = 1;
                continue;
            }
        }
        {
            ttf_resource_layer_t ignored = TTF_LAYER_COMMON;
            size_t ext = extension(in->engine, s, n, p, &ignored, 0);
            if (ext) {
                p = ext;
                after_space = 0;
                continue;
            }
        }
        if (letter((char)c) && !after_space) {
            do {
                ++p;
            } while (p < n && (letter(s[p]) || digit(s[p]) || s[p] == '_'));
        }
        break;
    }
    return p;
}
static size_t finish(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t end,
                     ttf_semantic_type_t type, const char *rule) {
    size_t count = in->index;
    if (end <= in->tokens[in->index].begin)
        return 0;
    out->type = type;
    out->input_end = end;
    snprintf(out->rule, sizeof(out->rule), "%s", rule);
    while (count < in->token_count && in->tokens[count].begin < end)
        ++count;
    return count - in->index;
}
static size_t preserve(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t end,
                       ttf_resource_layer_t layer) {
    out->value[0] = out->secondary[0] = out->unit[0] = 0;
    out->layer = layer;
    return finish(in, out, end, TTF_SEM_TEXT, "syntax.invalid-phone");
}
/* Numeric suffixes keep their existing quantity meaning, even after a long '+'. */
static int quantity_tail(const ttf_pack_input_t *in, size_t at, ttf_resource_layer_t *layer) {
    size_t p = spaces(in->text, in->length, at), begin = p;
    char key[TTF_IR_VALUE_SIZE];
    while (p < in->length && p - begin < sizeof(key) - 1 && in->text[p] != '\n' &&
           in->text[p] != '\r') {
        const char *suffix;
        size_t i;
        ++p;
        memcpy(key, in->text + begin, p - begin);
        key[p - begin] = 0;
        if (p < in->length && letter(in->text[p - 1]) &&
            (letter(in->text[p]) || digit(in->text[p])))
            continue;
        suffix = ttf_resource_get(in->engine, "numeric_suffix", "*", key, NULL);
        /* 010-1234-5678 개인 폰: a Hangul suffix only counts when its word ends
           there; 개 is not a counter in 개인 or 개월 (2026-10-02). */
        if (suffix && p + 3 <= in->length && (unsigned char)in->text[p - 1] >= 0x80 &&
            (unsigned char)in->text[p] >= 0xea && (unsigned char)in->text[p] <= 0xed)
            suffix = NULL;
        if (suffix) {
            /* A pack may use a numeric suffix as a number designator after a
               telephone. Keep it outside the PHONE span; do not confuse it
               with a physical quantity suffix. The phone shape is still
               checked below, including lengths, signs and malformed groups. */
            ttf_resource_layer_t selected = TTF_LAYER_COMMON;
            const char *tail = lookup(in->engine, "phone_number_tail", NULL, key, &selected);
            if (tail)
                promote(layer, selected);
            return !tail || strcmp(tail, "1");
        }
        for (i = 0; i < p - begin; ++i)
            key[i] = fold(key[i]);
        if (ttf_resource_get(in->engine, "currency_name", "*", key, NULL))
            return 1;
        if (!letter(in->text[p - 1]) && (unsigned char)in->text[p - 1] < 128 &&
            !hspace(in->text[p - 1]))
            break;
    }
    return 0;
}

/* Unlabelled parentheses are not a phone cue by themselves. This initial
   syntax accepts a short parenthesized area plus a substantial subscriber;
   labels opt into other bounded shapes. No numbering-plan validity is inferred. */
static int parenthesized_shape(const ttf_pack_input_t *in, size_t begin) {
    size_t end = in->length, p = begin, groups = 0, digits = 0;
    size_t first = 0, second = 0, hyphens = 0, groups_at_close = 0;
    int closed = 0;
    if (end - begin > PHONE_SCAN_LIMIT)
        end = begin + PHONE_SCAN_LIMIT;
    while (p < end) {
        if (digit(in->text[p])) {
            size_t start = p;
            while (p < end && digit(in->text[p]))
                ++p;
            if (!groups)
                first = p - start;
            else if (groups == 1)
                second = p - start;
            ++groups;
            digits += p - start;
        } else if (hspace(in->text[p]) || in->text[p] == '(' || in->text[p] == ')' ||
                   in->text[p] == '-') {
            if (in->text[p] == ')' && !closed) {
                closed = 1;
                groups_at_close = groups;
            }
            if (in->text[p] == '-')
                ++hyphens;
            ++p;
        } else
            break;
    }
    return first <= 3 && digits >= 7 && (groups >= 3 || (groups == 2 && second >= 6)) &&
           ((closed && groups_at_close == 1) || (!closed && hyphens));
}

typedef struct {
    size_t end, digits, groups, hyphens;
    size_t lengths[PHONE_MAX_GROUPS];
    int international, negative, parenthesis, spaced, malformed, overflow, truncated;
    char value[TTF_IR_VALUE_SIZE];
} phone_body_t;
static void body(const ttf_pack_input_t *in, size_t begin, phone_body_t *out) {
    const char *s = in->text;
    size_t n = in->length, p = begin, used = 0;
    memset(out, 0, sizeof(*out));
    if (n - begin > PHONE_SCAN_LIMIT)
        n = begin + PHONE_SCAN_LIMIT;
    if (p < n && (s[p] == '+' || s[p] == '-')) {
        out->international = s[p] == '+';
        out->negative = s[p] == '-';
        if (out->international)
            out->value[used++] = '+';
        ++p;
        p = spaces(s, n, p);
    }
    while (p < n) {
        size_t start, q;
        int opened = s[p] == '(';
        if (opened) {
            out->parenthesis = 1;
            ++p;
            p = spaces(s, n, p);
        }
        start = p;
        while (p < n && digit(s[p]))
            ++p;
        if (p == start) {
            out->malformed = 1;
            break;
        }
        if (out->groups < PHONE_MAX_GROUPS)
            out->lengths[out->groups] = p - start;
        else
            out->overflow = 1;
        if (out->groups && used < sizeof(out->value) - 1)
            out->value[used++] = '-';
        if (p - start < sizeof(out->value) - used) {
            memcpy(out->value + used, s + start, p - start);
            used += p - start;
        } else
            out->overflow = 1;
        ++out->groups;
        out->digits += p - start;
        if (opened) {
            p = spaces(s, n, p);
            if (p == n || s[p] != ')') {
                out->malformed = 1;
                break;
            }
            ++p;
        }
        out->end = p;
        q = spaces(s, n, p);
        if (q == n && n < in->length)
            out->truncated = 1;
        /* An ordinary parenthetical word following a complete numeric field
           belongs to the next text span, not a malformed phone continuation. */
        if (q < n && s[q] == '(') {
            size_t inside = spaces(s, n, q + 1);
            if (inside < n && !digit(s[inside]) && s[inside] != '(')
                break;
        }
        if (q > p && q < n && (digit(s[q]) || s[q] == '(')) {
            out->spaced = 1;
            p = q;
            continue;
        }
        if (q < n && s[q] == '-') {
            ++out->hyphens;
            if (q > p)
                out->spaced = 1;
            p = spaces(s, n, q + 1);
            if (p > q + 1)
                out->spaced = 1;
            if (p < n && (digit(s[p]) || s[p] == '('))
                continue;
            out->end = p;
            out->malformed = 1;
            break;
        }
        if (q == p && p < n && (s[p] == '(' || (opened && digit(s[p]))))
            continue;
        break;
    }
    out->value[used] = 0;
    if (!out->end)
        out->end = p;
    if (p == n && n < in->length)
        out->truncated = 1;
}
/* "(010-1234-5678)": digits and hyphens between brackets, with no inner
   parenthesis, are one number wearing punctuation. */
static int bracketed_number(const ttf_pack_input_t *in, size_t begin) {
    size_t p = begin + 1, end = in->length, groups = 0, hyphens = 0;
    if (end - begin > PHONE_SCAN_LIMIT)
        end = begin + PHONE_SCAN_LIMIT;
    while (p < end) {
        if (digit(in->text[p])) {
            while (p < end && digit(in->text[p]))
                ++p;
            ++groups;
        } else if (in->text[p] == '-') {
            ++hyphens;
            ++p;
        } else
            break;
    }
    return p < end && in->text[p] == ')' && groups >= 2 && hyphens >= 1;
}

/* A declared service number (1588-0000) has a four-digit prefix and a
   four-digit subscriber group; every other bare hyphen pair is a range. */
static int service_number(const ttf_pack_input_t *in, const phone_body_t *parsed, size_t begin,
                          ttf_resource_layer_t *layer) {
    char key[8];
    const char *value;
    ttf_resource_layer_t found = TTF_LAYER_COMMON;
    if (parsed->lengths[0] != 4 || parsed->lengths[1] != 4 || parsed->spaced ||
        parsed->international || parsed->negative || begin + 4 > in->length)
        return 0;
    memcpy(key, in->text + begin, 4);
    key[4] = 0;
    value = lookup(in->engine, "phone_service_prefix", NULL, key, &found);
    if (!value || strcmp(value, "1"))
        return 0;
    promote(layer, found);
    return 1;
}

static size_t classify_phone(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *s;
    size_t begin, end, ext_end, p, ext_digits = 0, minimum, maximum;
    int labeled, legacy, candidate, service;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    phone_body_t parsed;
    if (!in || !out || in->index >= in->token_count)
        return 0;
    s = in->text;
    begin = in->tokens[in->index].begin;
    if (begin >= in->length ||
        !(digit(s[begin]) || s[begin] == '+' || s[begin] == '-' || s[begin] == '('))
        return 0;
    if (ttf_phone_preceding_label(in, begin, "identifier_label", NULL, NULL, NULL))
        return 0;
    labeled = ttf_phone_preceding_label(in, begin, "phone_label", NULL, NULL, &layer);
    if (s[begin] == '(' && !parenthesized_shape(in, begin)) {
        /* Brackets around a complete number are punctuation, not an area code:
           the number classifies on its own and the brackets stay as text. */
        if (bracketed_number(in, begin) || !labeled)
            return 0;
    }
    body(in, begin, &parsed);
    service = service_number(in, &parsed, begin, &layer);
    /* Keep valid and invalid year-first dates under the established date grammar. */
    if (!parsed.international && !parsed.negative && !parsed.parenthesis && parsed.groups == 3 &&
        parsed.hyphens == 2 && parsed.lengths[0] == 4 && s[begin] != '0')
        return 0; /* 0120-456-789 is a phone number: no year starts with 0 (owner sets, 2026-10-14) */
    /* Two hyphen endpoints are a numeric range unless explicit phone context
       disambiguates them; a leading '+' alone is not a telephone label. */
    if (!labeled && !parsed.parenthesis && parsed.groups == 2 && parsed.hyphens == 1 && !service)
        return 0;
    legacy = !parsed.international && !parsed.negative && !parsed.parenthesis && !parsed.spaced &&
             parsed.groups == 3 && parsed.hyphens == 2;
    candidate = labeled || parsed.international || s[begin] == '(' || legacy || service ||
                (parsed.hyphens >= 2 && !parsed.spaced && !parsed.parenthesis) ||
                /* 888 555 3232: the North American 3-3-4 layout with spaces is a phone number
                   without a cue (owner v2, 2026-10-15) */
                (parsed.spaced && !parsed.hyphens && !parsed.parenthesis && parsed.groups == 3 &&
                 parsed.lengths[0] == 3 && parsed.lengths[1] == 3 && parsed.lengths[2] == 4 &&
                 s[begin] >= '2' && s[begin] <= '9');
    if (!candidate)
        return 0;
    if (parsed.truncated) {
        size_t whole = invalid_end(in, begin);
        size_t after = spaces(s, in->length, parsed.end);
        ttf_resource_layer_t ignored = TTF_LAYER_COMMON;
        /* A capped whitespace lookahead need not invalidate a completed body.
           Expanding the candidate is permitted only after a strong phone cue. */
        if (whole > parsed.end && !extension(in->engine, s, in->length, after, &ignored, 0))
            parsed.overflow = 1;
    }
    if (!parsed.digits) {
        size_t whole = invalid_end(in, begin), i, digits = 0;
        for (i = begin; i < whole; ++i)
            if (digit(s[i]))
                ++digits;
        if (digits >= 7 && (parsed.parenthesis || parsed.international || labeled))
            return preserve(in, out, whole, layer);
        return 0;
    }
    /* An unlabelled parenthesized number without subscriber groups is arithmetic/list syntax. */
    if (!labeled && !parsed.international && parsed.parenthesis && parsed.groups == 1 &&
        !parsed.malformed) {
        if (parsed.end < in->length && s[parsed.end] == ')')
            parsed.malformed = 1;
        else
            return 0;
    }
    /* Additional bare hyphen groups can be bibliographic or account identifiers.
       Broader group layouts require an explicit phone cue. */
    if (!labeled && !parsed.international && !parsed.parenthesis && parsed.groups > 3 &&
        !(parsed.groups == 4 && parsed.hyphens == 3 && !parsed.spaced && s[begin] == '1' &&
          parsed.lengths[0] == 1 && parsed.lengths[1] == 3 && parsed.lengths[2] == 3 && parsed.lengths[3] == 4))
        /* 1-800-555-0132: the North American 1-3-3-4 layout is a phone number without a cue
           (owner sets, 2026-10-14) */
        parsed.malformed = 1;
    minimum = policy(in->engine,
                     parsed.international ? "international_min_digits" : "national_min_digits", 7,
                     &layer);
    maximum = policy(in->engine,
                     parsed.international ? "international_max_digits" : "national_max_digits", 15,
                     &layer);
    end = parsed.end;
    if (quantity_tail(in, end, &layer)) {
        if (parsed.groups == 1 && !parsed.parenthesis)
            return 0;
        parsed.malformed = 1;
    }
    /* Dotted, fractional, range and clock continuations belong to numeric grammars. */
    if (end < in->length &&
        (s[end] == '~' || s[end] == ':' || s[end] == '/' ||
         ((s[end] == '.' || s[end] == ',') && end + 1 < in->length && digit(s[end + 1])))) {
        if (parsed.groups == 1 && !parsed.parenthesis)
            return 0;
        parsed.malformed = 1;
    }
    p = spaces(s, in->length, end);
    ext_end = extension(in->engine, s, in->length, p, &layer, 0);
    /* A bare signed integer is financial/numeric text. Only a label or an
       anchored extension disambiguates compact international spelling. */
    if (!labeled && parsed.international && parsed.groups == 1 && !parsed.parenthesis &&
        !parsed.hyphens && !parsed.spaced &&
        (!ext_end ||
         marker(in->engine, s, in->length, p, &layer, 0, "phone_extension_hint") != ext_end))
        return 0;
    if (ext_end) {
        size_t ext_begin = spaces(s, in->length, ext_end);
        if (ext_begin < in->length && s[ext_begin] == ':')
            ext_begin = spaces(s, in->length, ext_begin + 1);
        end = ext_begin;
        while (end < in->length && digit(s[end]))
            ++end;
        ext_digits = end - ext_begin;
        if (!ext_digits || ext_digits > policy(in->engine, "extension_max_digits", 8, &layer) ||
            ext_digits >= sizeof(out->secondary))
            parsed.malformed = 1;
        else {
            memcpy(out->secondary, s + ext_begin, ext_digits);
            out->secondary[ext_digits] = 0;
        }
        if (end < in->length &&
            (letter(s[end]) || s[end] == '-' || s[end] == '(' || s[end] == ')' ||
             (hspace(s[end]) && spaces(s, in->length, end) < in->length &&
              digit(s[spaces(s, in->length, end)]))))
            parsed.malformed = 1;
    } else if (end < in->length) {
        uint32_t cp;
        size_t width;
        if (letter(s[end]) || (s[end] == ')' && !(begin && s[begin - 1] == '(')) || s[end] == '+' ||
            (ttf_utf8_decode(s + end, in->length - end, &cp, &width) == 1 && cp >= 128 &&
             ttf_script_of(cp) == TTF_SCRIPT_WHITESPACE && cp != 0x2028 && cp != 0x2029 &&
             end + width < in->length && digit(s[end + width])))
            parsed.malformed = 1;
    }

    if (parsed.negative || parsed.malformed || parsed.overflow || parsed.digits < minimum ||
        parsed.digits > maximum ||
        (legacy && !(parsed.lengths[0] >= 2 && parsed.lengths[0] <= 4 && parsed.lengths[1] >= 2 &&
                     parsed.lengths[1] <= 4 && parsed.lengths[2] >= 3 && parsed.lengths[2] <= 4))) {
        size_t whole = invalid_end(in, begin);
        /* A cue does not make a short number a phone number: "문의 12" is 십이 and
           "문의 3-5" is a range. Preservation is for something that has the digits
           of a real phone number and a broken shape. */
        if (!parsed.negative && !parsed.overflow && !parsed.international && !parsed.parenthesis &&
            parsed.digits < minimum)
            return 0;
        if (whole < end)
            whole = end;
        return preserve(in, out, whole, layer);
    }
    memcpy(out->value, parsed.value, strlen(parsed.value) + 1);
    if (!ext_end)
        out->secondary[0] = 0;
    snprintf(out->unit, sizeof(out->unit), "%s",
             legacy && !ext_end     ? ""
             : parsed.international ? "international"
                                    : "national");
    out->layer = layer;
    return finish(in, out, end, TTF_SEM_PHONE,
                  legacy && !ext_end ? "syntax.phone" : "syntax.phone-extended");
}

size_t ttf_classify_phone(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    ttf_semantic_t candidate;
    size_t consumed;
    if (!out)
        return 0;
    memset(&candidate, 0, sizeof(candidate));
    candidate.struct_size = sizeof(candidate);
    consumed = classify_phone(in, &candidate);
    if (consumed)
        *out = candidate;
    return consumed;
}

int ttf_phone_boundary_pending(const ttf_engine_t *e, const char *s, size_t length,
                               const ttf_token_t *tokens, size_t count) {
    size_t i, end = length;
    if (!e || !s || !tokens || !count)
        return 0;
    while (end && hspace(s[end - 1]))
        --end;
    /* An ordinary newline remains a commitment boundary. */
    if (!end || s[end - 1] == '\n' || s[end - 1] == '\r')
        return 0;
    for (i = count; i; --i) {
        size_t begin = tokens[i - 1].begin, after, marker;
        phone_body_t parsed;
        ttf_pack_input_t in;
        ttf_resource_layer_t ignored = TTF_LAYER_COMMON;
        int labeled;
        if (begin >= end)
            continue;
        if (end - begin > PHONE_SCAN_LIMIT)
            break;
        if (memchr(s + begin, '\n', end - begin) || memchr(s + begin, '\r', end - begin))
            break;
        if (!(digit(s[begin]) || s[begin] == '+' || s[begin] == '-' || s[begin] == '('))
            continue;
        memset(&in, 0, sizeof(in));
        in.engine = e;
        in.text = s;
        in.length = end;
        in.tokens = tokens;
        in.token_count = count;
        in.index = i - 1;
        if (ttf_phone_preceding_label(&in, begin, "identifier_label", NULL, NULL, NULL))
            continue;
        labeled = ttf_phone_preceding_label(&in, begin, "phone_label", NULL, NULL, &ignored);
        body(&in, begin, &parsed);
        if (!parsed.digits)
            continue;
        if (!(labeled || parsed.international || parsed.parenthesis || parsed.hyphens >= 2))
            continue;
        /* Malformed numeric-shaped prefixes still belong to one candidate.
           A nested/unclosed parenthesis must not expose its subscriber tail to
           a later independent RANGE classification after a tiny phrase cut. */
        if (invalid_end(&in, begin) == end)
            return 1;
        /* Protect ') ' even when its subscriber has not arrived, and numeric
           continuations even if a later byte will prove the candidate invalid. */
        after = spaces(s, end, parsed.end);
        if (after == end && (s[end - 1] == ')' || digit(s[end - 1]) || s[end - 1] == '-'))
            return 1;
        marker = extension(e, s, end, after, &ignored, 1);
        if (marker) {
            size_t p = spaces(s, end, marker);
            if (p < end && s[p] == ':')
                p = spaces(s, end, p + 1);
            while (p < end && digit(s[p]))
                ++p;
            if (p == end)
                return 1;
        }
    }
    return 0;
}
