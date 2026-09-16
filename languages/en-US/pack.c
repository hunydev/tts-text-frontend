#include "tts_text_frontend/ttf_language_pack.h"
#include "../context_run.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EN "en-US"
static char ascii_lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c;
}
static const char *res(const ttf_engine_t *e, const char *kind, const char *key) {
    return ttf_resource_get(e, kind, EN, key, NULL);
}
static int weight(const char *s, int fallback) {
    char *end;
    long n;
    if (!s)
        return fallback;
    n = strtol(s, &end, 10);
    return !*end && n >= 0 && n <= 1000 ? (int)n : fallback;
}
static ttf_status_t word(const ttf_engine_t *e, ttf_buffer_t *b, const char *kind,
                         const char *key) {
    const char *v = res(e, kind, key);
    if (!v)
        return TTF_ERESOURCE;
    return ttf_buffer_puts(b, v);
}
static ttf_status_t space(ttf_buffer_t *b) {
    return ttf_buffer_puts(b, " ");
}
static ttf_status_t card(const ttf_engine_t *e, uint64_t n, ttf_buffer_t *b) {
    static const uint64_t scales[] = {
        UINT64_C(1000000000000000000), UINT64_C(1000000000000000), UINT64_C(1000000000000),
        UINT64_C(1000000000),          UINT64_C(1000000),          UINT64_C(1000)};
    size_t i;
    char key[32];
    ttf_status_t st;
    for (i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i) {
        if (n >= scales[i]) {
            st = card(e, n / scales[i], b);
            if (st != TTF_OK)
                return st;
            if ((st = space(b)) != TTF_OK)
                return st;
            (void)snprintf(key, sizeof(key), "%llu", (unsigned long long)scales[i]);
            if ((st = word(e, b, "number", key)) != TTF_OK)
                return st;
            n %= scales[i];
            if (!n)
                return TTF_OK;
            if ((st = space(b)) != TTF_OK)
                return st;
            return card(e, n, b);
        }
    }
    if (n >= 100) {
        if ((st = card(e, n / 100, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if ((st = word(e, b, "number", "100")) != TTF_OK)
            return st;
        n %= 100;
        if (!n)
            return TTF_OK;
        if ((st = space(b)) != TTF_OK)
            return st;
    }
    if (n >= 20) {
        (void)snprintf(key, sizeof(key), "%u", (unsigned)(n / 10 * 10));
        if ((st = word(e, b, "number", key)) != TTF_OK)
            return st;
        n %= 10;
        if (!n)
            return TTF_OK;
        if ((st = space(b)) != TTF_OK)
            return st;
    }
    (void)snprintf(key, sizeof(key), "%u", (unsigned)n);
    return word(e, b, "number", key);
}
static ttf_status_t digits(const ttf_engine_t *e, const char *s, ttf_buffer_t *b) {
    size_t i;
    ttf_status_t st;
    int emitted = 0;
    for (i = 0; s[i]; ++i) {
        char key[2] = {s[i], '\0'};
        if (s[i] < '0' || s[i] > '9')
            continue;
        if (emitted && (st = space(b)) != TTF_OK)
            return st;
        if ((st = word(e, b, "number", key)) != TTF_OK)
            return st;
        emitted = 1;
    }
    return TTF_OK;
}
static int uint64_value(const char *text, uint64_t *value) {
    const char *p;
    *value = 0;
    if (!*text)
        return 0;
    for (p = text; *p; ++p) {
        unsigned digit_value = (unsigned)(*p - '0');
        if (digit_value > 9 || *value > (UINT64_MAX - digit_value) / 10)
            return 0;
        *value = *value * 10 + digit_value;
    }
    return 1;
}
static int singular(const char *text) {
    if (*text == '-' || *text == '+')
        ++text;
    while (*text == '0')
        ++text;
    if (*text++ != '1')
        return 0;
    if (*text == '.') {
        ++text;
        while (*text == '0')
            ++text;
    }
    return !*text;
}
static ttf_status_t numeric(const ttf_engine_t *e, const char *s, ttf_buffer_t *b) {
    char integral[TTF_IR_VALUE_SIZE];
    const char *dot;
    size_t len;
    uint64_t value;
    ttf_status_t st;
    if (*s == '-' || *s == '+') {
        if ((st = word(e, b, "number", *s == '-' ? "minus" : "plus")) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        ++s;
    }
    dot = strchr(s, '.');
    len = dot ? (size_t)(dot - s) : strlen(s);
    if (len >= sizeof(integral))
        return TTF_ELIMIT;
    memcpy(integral, s, len);
    integral[len] = '\0';
    if (uint64_value(integral, &value))
        st = card(e, value, b);
    else
        st = digits(e, integral, b);
    if (st != TTF_OK || !dot)
        return st;
    if ((st = space(b)) != TTF_OK)
        return st;
    if ((st = word(e, b, "number", "point")) != TTF_OK)
        return st;
    if ((st = space(b)) != TTF_OK)
        return st;
    return digits(e, dot + 1, b);
}
static ttf_status_t ordinal(const ttf_engine_t *e, uint64_t n, ttf_buffer_t *b) {
    char key[32];
    const char *v;
    ttf_status_t st;
    (void)snprintf(key, sizeof(key), "%llu", (unsigned long long)n);
    v = res(e, "ordinal", key);
    if (v)
        return ttf_buffer_puts(b, v);
    if (n < 20 || (n < 100 && n % 10 == 0))
        return TTF_ERESOURCE;
    if (n < 100) {
        if ((st = card(e, n / 10 * 10, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        return ordinal(e, n % 10, b);
    }
    if (n % 100) {
        if ((st = card(e, n - n % 100, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        return ordinal(e, n % 100, b);
    }
    if ((st = card(e, n, b)) != TTF_OK)
        return st;
    return word(e, b, "ordinal", "suffix");
}
static int script_score(ttf_script_t s) {
    return s == TTF_SCRIPT_LATIN ? 10 : 0;
}
/* Units and ordinal suffixes convey semantics, not independent English words. */
static int neutral_marker(const ttf_pack_input_t *in, size_t index, const char *lower) {
    const ttf_token_t *t = &in->tokens[index];
    size_t previous = index, start, length;
    char original[96];
    /* The exponent marker inside scientific numeric syntax is not an English
       word. The universal classifier remains responsible for validation. */
    if (!strcmp(lower, "e") && index && in->tokens[index - 1].script == TTF_SCRIPT_DIGIT &&
        in->tokens[index - 1].end == t->begin) {
        size_t next = index + 1, end = t->end;
        if (next < in->token_count && in->tokens[next].begin == end &&
            in->tokens[next].end == end + 1 &&
            (in->text[end] == '+' || in->text[end] == '-'))
            end = in->tokens[next++].end;
        if (next < in->token_count && in->tokens[next].begin == end &&
            in->tokens[next].script == TTF_SCRIPT_DIGIT)
            return 1;
    }
    while (previous && (in->tokens[previous - 1].script == TTF_SCRIPT_WHITESPACE ||
                        in->tokens[previous - 1].script == TTF_SCRIPT_SYMBOL))
        --previous;
    if (!previous || in->tokens[previous - 1].script != TTF_SCRIPT_DIGIT)
        return 0;
    if (in->tokens[previous - 1].end == t->begin &&
        ttf_resource_get(in->engine, "ordinal_suffix", "*", lower, NULL))
        return 1;
    start = in->tokens[previous - 1].end;
    while (start < t->begin && (in->text[start] == ' ' || in->text[start] == '\t'))
        ++start;
    length = t->end - start;
    if (length >= sizeof(original))
        return 0;
    memcpy(original, in->text + start, length);
    original[length] = '\0';
    return ttf_resource_get(in->engine, "numeric_suffix", "*", original, NULL) != NULL;
}
/* An explicit label selects the speech language of its immediately following
   lexical run. The labels and their weights are resource data, and a zero
   weight disables an inherited label. Sentence/line boundaries end the cue. */
static int context_cue_score(const ttf_pack_input_t *in, size_t begin) {
    const ttf_token_t *token;
    char key[96];
    size_t i, n;
    int colon = 0;
    while (begin) {
        token = &in->tokens[begin - 1];
        if (token->script == TTF_SCRIPT_WHITESPACE) {
            for (i = token->begin; i < token->end; ++i)
                if (in->text[i] == '\r' || in->text[i] == '\n')
                    return 0;
            --begin;
        } else if (!colon && token->end == token->begin + 1 &&
                   in->text[token->begin] == ':') {
            colon = 1;
            --begin;
        } else
            break;
    }
    if (!begin)
        return 0;
    token = &in->tokens[begin - 1];
    n = token->end - token->begin;
    if (n >= sizeof(key))
        return 0;
    memcpy(key, in->text + token->begin, n);
    key[n] = '\0';
    return weight(res(in->engine, "context_cue", key), 0);
}
static int context_score(const ttf_pack_input_t *in) {
    size_t begin, end, i, words = 0;
    int total = 0, predicate = 0, imperative = 0, subject = 0, polite = 0;
    int ceiling = weight(res(in->engine, "context", "phrase_ceiling"), 160);
    int latin = weight(res(in->engine, "context", "latin_weight"), 8);
    if (in->index >= in->token_count || in->tokens[in->index].script == TTF_SCRIPT_HANGUL)
        return 0;
    context_run_bounds(in, &begin, &end);
    for (i = begin; i < end; ++i) {
        const ttf_token_t *t = &in->tokens[i];
        char key[96];
        const char *role;
        size_t n = t->end - t->begin, j;
        int addition;
        if (t->script != TTF_SCRIPT_LATIN || n >= sizeof(key))
            continue;
        for (j = 0; j < n; ++j)
            key[j] = ascii_lower(in->text[t->begin + j]);
        key[n] = '\0';
        if (neutral_marker(in, i, key))
            continue;
        ++words;
        addition = latin + weight(res(in->engine, "context_word", key), 0);
        /* Saturation avoids an arbitrarily long noun list defeating an attached
           particle, and prevents an integer overflow on a large input. */
        total = addition >= ceiling - total ? ceiling : total + addition;
        role = res(in->engine, "context_role", key);
        if (!role)
            continue;
        if (!strcmp(role, "finite"))
            predicate = 1;
        else if (!strcmp(role, "imperative")) {
            if (words == 1 || polite || subject)
                imperative = 1;
        } else if (!strcmp(role, "subject"))
            subject = 1;
        else if (!strcmp(role, "polite"))
            polite = 1;
    }
    /* A predicate with another word supplies a clause signal. Bare nouns and
       a conjunction alone do not. The finite/imperative vocabulary is data. */
    if ((predicate && words >= 2) ||
        (imperative && ((words >= 3) || ((polite || subject) && words >= 2))))
        total += weight(res(in->engine, "context", "clause_weight"), 500);
    /* An ordinal alone is a quantity title and keeps the surrounding speech
       language. At least one lexical word is required for a foreign title. */
    return total + (words ? context_cue_score(in, begin) : 0);
}
static size_t classify(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const ttf_token_t *t = &in->tokens[in->index];
    char key[96];
    size_t n = t->end - t->begin, j, consumed = 1, end = t->end;
    const char *kind;
    if (t->script != TTF_SCRIPT_LATIN || n >= sizeof(key))
        return 0;
    /* Treat an adjacent hyphenated word as one lexical unit so the existing
       layered lexicon can choose a pronunciation for the complete spelling.
       Spaces, doubled hyphens and numeric terms are separate grammar. */
    while (in->index + consumed + 1 < in->token_count) {
        const ttf_token_t *separator = &in->tokens[in->index + consumed];
        const ttf_token_t *next = &in->tokens[in->index + consumed + 1];
        if (separator->begin != end || separator->end != separator->begin + 1 ||
            in->text[separator->begin] != '-' || next->begin != separator->end ||
            next->script != TTF_SCRIPT_LATIN)
            break;
        /* Bound lookahead as well as the copied key. Otherwise an overlong
           chain revisited from each component would take quadratic time. */
        if (next->end - t->begin >= sizeof(key))
            return 0;
        end = next->end;
        consumed += 2;
    }
    n = end - t->begin;
    if (n >= sizeof(key))
        return 0;
    for (j = 0; j < n; ++j)
        key[j] = ascii_lower(in->text[t->begin + j]);
    key[n] = '\0';
    kind = res(in->engine, "lexical", key);
    out->type = TTF_SEM_WORD;
    out->lexical_class = TTF_LEX_UNKNOWN_LATIN;
    if (kind) {
        if (!strcmp(kind, "INITIALISM")) {
            out->type = TTF_SEM_INITIALISM;
            out->lexical_class = TTF_LEX_INITIALISM;
        } else if (!strcmp(kind, "ACRONYM")) {
            out->type = TTF_SEM_ACRONYM;
            out->lexical_class = TTF_LEX_ACRONYM;
        } else if (!strcmp(kind, "PROPER_NOUN"))
            out->lexical_class = TTF_LEX_PROPER_NOUN;
        else
            out->lexical_class = TTF_LEX_WORD;
    } else {
        int all_upper = n > 1;
        for (j = 0; j < n; ++j)
            if (in->text[t->begin + j] < 'A' || in->text[t->begin + j] > 'Z')
                all_upper = 0;
        if (all_upper) {
            out->type = TTF_SEM_INITIALISM;
            out->lexical_class = TTF_LEX_INITIALISM;
        }
    }
    if (consumed == 1 && t->end < in->length && in->text[t->end] == '.' &&
        res(in->engine, "abbreviation", key)) {
        out->type = TTF_SEM_ABBREVIATION;
        out->input_end = t->end + 1;
        (void)snprintf(out->rule, sizeof(out->rule), "pack.abbreviation");
        return in->index + 1 < in->token_count ? 2 : 1;
    }
    out->input_end = end;
    (void)snprintf(out->rule, sizeof(out->rule), "%s",
                   consumed > 1 ? "pack.hyphenated-word" : "pack.lexical");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    return consumed;
}
static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *b) {
    ttf_status_t st;
    char key[48];
    const char *label;
    unsigned y, m, d, h, min, sec;
    switch (s->type) {
    case TTF_SEM_CARDINAL:
    case TTF_SEM_NUMBER:
        return numeric(e, s->value, b);
    case TTF_SEM_ORDINAL: {
        uint64_t value;
        if (!uint64_value(s->value, &value))
            return ttf_buffer_append(b, surface, length);
        return ordinal(e, value, b);
    }
    case TTF_SEM_DIGIT_SEQUENCE:
    case TTF_SEM_PHONE:
        return digits(e, s->value, b);
    case TTF_SEM_DATE:
        if (sscanf(s->value, "%u-%u-%u", &y, &m, &d) != 3)
            return TTF_EINVAL;
        (void)snprintf(key, sizeof(key), "%u", m);
        if ((st = word(e, b, "month", key)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if ((st = ordinal(e, d, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        return card(e, y, b);
    case TTF_SEM_TIME: {
        int fields = sscanf(s->value, "%u:%u:%u", &h, &min, &sec);
        if (fields < 2)
            return TTF_EINVAL;
        if ((st = card(e, h, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if (!min) {
            if ((st = word(e, b, "number", "oclock")) != TTF_OK)
                return st;
        } else {
            if (min < 10) {
                if ((st = word(e, b, "number", "oh")) != TTF_OK)
                    return st;
                if ((st = space(b)) != TTF_OK)
                    return st;
            }
            if ((st = card(e, min, b)) != TTF_OK)
                return st;
        }
        if (fields == 3) {
            if ((st = space(b)) != TTF_OK || (st = word(e, b, "number", "and")) != TTF_OK ||
                (st = space(b)) != TTF_OK || (st = card(e, sec, b)) != TTF_OK ||
                (st = space(b)) != TTF_OK ||
                (st = word(e, b, "unit", sec == 1 ? "s.one" : "s.many")) != TTF_OK)
                return st;
        }
        if (s->secondary[0]) {
            if ((st = space(b)) != TTF_OK)
                return st;
            return word(e, b, "meridiem", s->secondary);
        }
        return TTF_OK;
    }
    case TTF_SEM_FRACTION:
        if ((st = numeric(e, s->value, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
            (st = word(e, b, "number", "fraction_over")) != TTF_OK || (st = space(b)) != TTF_OK)
            return st;
        return numeric(e, s->secondary, b);
    case TTF_SEM_CURRENCY: {
        const char *dot = strchr(s->value, '.');
        char whole[TTF_IR_VALUE_SIZE];
        size_t len = dot ? (size_t)(dot - s->value) : strlen(s->value);
        memcpy(whole, s->value, len);
        whole[len] = '\0';
        if ((st = numeric(e, dot && strlen(dot + 1) > 2 ? s->value : whole, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        (void)snprintf(key, sizeof(key), "%s.%s", s->unit, singular(whole) ? "one" : "many");
        if ((st = word(e, b, "currency", key)) != TTF_OK)
            return st;
        if (dot && dot[1] && strlen(dot + 1) <= 2 && strtoul(dot + 1, NULL, 10)) {
            unsigned cents = (unsigned)strtoul(dot + 1, NULL, 10);
            if (!dot[2])
                cents *= 10;
            if ((st = space(b)) != TTF_OK)
                return st;
            if ((st = word(e, b, "number", "and")) != TTF_OK)
                return st;
            if ((st = space(b)) != TTF_OK)
                return st;
            if ((st = card(e, cents, b)) != TTF_OK)
                return st;
            if ((st = space(b)) != TTF_OK)
                return st;
            (void)snprintf(key, sizeof(key), "%s.%s", s->unit,
                           cents == 1 ? "minor.one" : "minor.many");
            return word(e, b, "currency", key);
        }
        return TTF_OK;
    }
    case TTF_SEM_PERCENT:
        if ((st = numeric(e, s->value, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        return word(e, b, "number", "percent");
    case TTF_SEM_MEASURE:
    case TTF_SEM_COUNTER:
        if ((st = numeric(e, s->value, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        (void)snprintf(key, sizeof(key), "%s.%s", s->unit, singular(s->value) ? "one" : "many");
        label = res(e, "unit", key);
        return ttf_buffer_puts(b, label ? label : s->unit);
    case TTF_SEM_RANGE:
        if ((st = numeric(e, s->value, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if ((st = word(e, b, "number", "range")) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if ((st = numeric(e, s->secondary, b)) != TTF_OK || !s->unit[0])
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if (!strcmp(s->unit, "percent"))
            return word(e, b, "number", "percent");
        (void)snprintf(key, sizeof(key), "%s.%s", s->unit, singular(s->secondary) ? "one" : "many");
        label = res(e, "unit", key);
        if (!label)
            label = res(e, "currency", key);
        return ttf_buffer_puts(b, label ? label : s->unit);
    default:
        return ttf_buffer_append(b, surface, length);
    }
}
static ttf_status_t pronounce(const ttf_engine_t *e, const char *word_text, size_t n,
                              ttf_lexical_class_t lexical, ttf_pronunciation_t *out) {
    char key[TTF_IR_VALUE_SIZE];
    size_t i, p = 0;
    const char *phones;
    /* Long unknown words bypass this bounded prototype and reach the target's
       streaming grapheme fallback; they must not fail the entire request. */
    if (n >= sizeof(key))
        return TTF_EINVAL;
    memset(out, 0, sizeof(*out));
    (void)snprintf(out->alphabet, sizeof(out->alphabet), "ttf-phones-v1");
    for (i = 0; i < n; ++i)
        key[i] = ascii_lower(word_text[i]);
    key[n] = '\0';
    phones = res(e, "pronunciation", key);
    if (phones) {
        if (strlen(phones) >= sizeof(out->phones))
            return TTF_ELIMIT;
        (void)snprintf(out->phones, sizeof(out->phones), "%s", phones);
        (void)snprintf(out->provenance, sizeof(out->provenance), "pronunciation-dictionary");
        return TTF_OK;
    }
    if (lexical == TTF_LEX_INITIALISM) {
        for (i = 0; i < n; ++i) {
            int written;
            if (key[i] < 'a' || key[i] > 'z')
                continue;
            written = snprintf(out->phones + p, sizeof(out->phones) - p, "%sletter.%c",
                               p ? " " : "", (char)toupper((unsigned char)key[i]));
            if (written < 0 || (size_t)written >= sizeof(out->phones) - p)
                return TTF_ELIMIT;
            p += (size_t)written;
        }
        (void)snprintf(out->provenance, sizeof(out->provenance), "initialism");
        return p ? TTF_OK : TTF_EINVAL;
    }
    if (lexical == TTF_LEX_ALNUM_TERM) {
        size_t upper = 0;
        while (upper < n && word_text[upper] >= 'A' && word_text[upper] <= 'Z')
            ++upper;
        if (upper >= 2) {
            for (i = 0; i < n; ++i) {
                char symbolic[16];
                int written;
                if (word_text[i] == '-')
                    continue;
                if (key[i] >= 'a' && key[i] <= 'z') {
                    (void)snprintf(symbolic, sizeof(symbolic), "letter.%c",
                                   (char)toupper((unsigned char)key[i]));
                    phones = symbolic;
                } else if (key[i] >= '0' && key[i] <= '9') {
                    char digit_key[2] = {key[i], '\0'};
                    phones = res(e, "digit_pronunciation", digit_key);
                    if (!phones)
                        return TTF_EINVAL;
                } else
                    return TTF_EINVAL;
                written = snprintf(out->phones + p, sizeof(out->phones) - p, "%s%s", p ? " " : "",
                                   phones);
                if (written < 0 || (size_t)written >= sizeof(out->phones) - p)
                    return TTF_EINVAL;
                p += (size_t)written;
            }
            (void)snprintf(out->provenance, sizeof(out->provenance), "alnum-initialism");
            return p ? TTF_OK : TTF_EINVAL;
        }
    }
    for (i = 0; i < n;) {
        size_t take = n - i;
        char chunk[8];
        int written;
        if (take > 4)
            take = 4;
        phones = NULL;
        for (; take; --take) {
            memcpy(chunk, key + i, take);
            chunk[take] = '\0';
            phones = res(e, "g2p", chunk);
            if (phones)
                break;
        }
        if (!phones)
            return TTF_EINVAL;
        written = snprintf(out->phones + p, sizeof(out->phones) - p, "%s%s", p ? " " : "", phones);
        if (written < 0 || (size_t)written >= sizeof(out->phones) - p)
            return TTF_ELIMIT;
        p += (size_t)written;
        i += take;
    }
    (void)snprintf(out->provenance, sizeof(out->provenance), "grapheme-g2p-prototype");
    return p ? TTF_OK : TTF_EINVAL;
}

const ttf_language_pack_t ttf_pack_en_US = {sizeof(ttf_language_pack_t),
                                            TTF_ABI_VERSION,
                                            EN,
                                            NULL,
                                            script_score,
                                            context_score,
                                            classify,
                                            verbalize,
                                            pronounce,
                                            NULL,
                                            NULL};
