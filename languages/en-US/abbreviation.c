#include "abbreviation.h"
#include "../../src/core/internal.h"
#include "../../src/pronunciation/trace.h"
#include <stdio.h>
#include <string.h>

#define EN "en-US"
static int upper(char c) {
    return c >= 'A' && c <= 'Z';
}
static int alpha(char c) {
    return upper(c) || (c >= 'a' && c <= 'z');
}
static char lower(char c) {
    return upper(c) ? (char)(c + 'a' - 'A') : c;
}
static int horizontal(char c) {
    return c == ' ' || c == '\t';
}
static void merge_layer(ttf_semantic_t *s, ttf_resource_layer_t layer) {
    if (layer > s->layer)
        s->layer = layer;
}
static const char *lookup(const ttf_engine_t *engine, const char *kind, const char *key,
                          ttf_semantic_t *semantic) {
    ttf_resource_layer_t layer;
    const char *value = ttf_resource_get(engine, kind, EN, key, &layer);
    if (value && semantic)
        merge_layer(semantic, layer);
    return value;
}
/* The previous visible character closes a quotation or bracket, or opens a bracket. */
static int credit_position(const ttf_pack_input_t *in, size_t start) {
    static const char *const marks[] = {"\xe2\x80\x99", "\xe2\x80\x9d", "\xe3\x80\x8b",
                                        "\xe3\x80\x89", "\xe3\x80\x8d", "\xe3\x80\x8f"};
    size_t p = start, k;
    while (p && horizontal(in->text[p - 1]))
        --p;
    if (!p)
        return 0;
    if (strchr("'\")(>", in->text[p - 1]))
        return 1;
    for (k = 0; k < sizeof(marks) / sizeof(marks[0]); ++k)
        if (p >= 3 && !memcmp(in->text + p - 3, marks[k], 3))
            return 1;
    return 0;
}
static int left_boundary(const ttf_pack_input_t *in, size_t start) {
    char c;
    if (!start)
        return 1;
    c = in->text[start - 1];
    if ((unsigned char)c >= 128)
        return in->index && in->tokens[in->index - 1].script != TTF_SCRIPT_LATIN;
    return !alpha(c) && !(c >= '0' && c <= '9') && c != '_' && c != '.' && c != '/' && c != '\\' &&
           c != '-' && c != '@';
}
static size_t token_count(const ttf_pack_input_t *in, size_t end) {
    size_t i = in->index;
    while (i < in->token_count && in->tokens[i].begin < end)
        ++i;
    return i - in->index;
}
/* "Cossette , M. , Lecomte": the period belongs to the initial, and what follows
   carries the sentence on. A period that ends a sentence is left alone, because
   dropping it would take the boundary with it. The marks are resources. */
static int initial_continuation(const ttf_pack_input_t *in, size_t end) {
    char key[2];
    size_t p = end;
    const char *value;
    while (p < in->length && horizontal(in->text[p]))
        ++p;
    if (p >= in->length || (unsigned char)in->text[p] >= 128)
        return 0;
    key[0] = in->text[p];
    key[1] = '\0';
    value = ttf_resource_get(in->engine, "initial_continuation", EN, key, NULL);
    return value && strcmp(value, "0");
}

/* A title is recognized only before an ASCII-capitalized name. The guard is
   deliberate: terminal 'Dr.' and street-address suffixes are not Doctor. */
static int following_name(const ttf_pack_input_t *in, size_t end) {
    size_t p = end;
    while (p < in->length && horizontal(in->text[p]))
        ++p;
    /* Mr. and Mrs. Lee: a title joined by "and" to another title before the name
       (2026-10-30; was Mr. kept as written) */
    if (p > end && p + 4 < in->length && !memcmp(in->text + p, "and ", 4)) {
        size_t q = p + 4, w = q;
        char key[8];
        while (w < in->length && alpha(in->text[w]) && w - q < sizeof(key) - 1)
            key[w - q] = lower(in->text[w]), ++w;
        key[w - q] = 0;
        if (w > q && w < in->length && in->text[w] == '.' && lookup(in->engine, "abbreviation_expansion", key, NULL))
            return following_name(in, w + 1);
    }
    return p > end && p < in->length && upper(in->text[p]) && p + 1 < in->length &&
           (alpha(in->text[p + 1]) || in->text[p + 1] == '.');
}
/* The capitalized word just before start (Grey St., Kansas City, MO). */
static int capitalized_before(const ttf_pack_input_t *in, size_t start, size_t gap_max) {
    size_t p = start, w;
    while (p && horizontal(in->text[p - 1]) && start - p < gap_max)
        --p;
    if (p == start && gap_max)
        return 0;
    w = p;
    while (p && alpha(in->text[p - 1]))
        --p;
    return w > p && upper(in->text[p]);
}
/* A capitalized word after a period that starts a new sentence rather than a
   name (Grey St. The fare). */
static int sentence_word(const ttf_pack_input_t *in, size_t p) {
    static const char *const words[] = {"The", "A", "An", "It", "He", "She", "We", "They", "I",
                                        "This", "That", "There", "These", "Those", "In", "On",
                                        "At", "But", "And", "His", "Her", "Our", "Their", "You",
                                        "My", "If", "When", "Then", "So", "As"};
    size_t i, w = p;
    while (w < in->length && alpha(in->text[w]))
        ++w;
    for (i = 0; i < sizeof(words) / sizeof(words[0]); ++i)
        if (strlen(words[i]) == w - p && !memcmp(words[i], in->text + p, w - p))
            return 1;
    return 0;
}
static int following_digit(const ttf_pack_input_t *in, size_t end) {
    size_t p = end;
    while (p < in->length && horizontal(in->text[p]))
        ++p;
    return p < in->length && in->text[p] >= '0' && in->text[p] <= '9';
}
/* Preserve a terminal punctuation mark at a complete unit's end, newline, or
   closing quotation. The segmenter protects initial periods until safe input
   is available, so this does not depend on arbitrary input chunk boundaries. */
static int terminal_period(const ttf_pack_input_t *in, size_t end) {
    static const char *const closing_quotes[] = {"’", "”", "»", "」", "』"};
    size_t p = end, i;
    while (p < in->length && horizontal(in->text[p]))
        ++p;
    if (p == in->length || in->text[p] == '\r' || in->text[p] == '\n' || in->text[p] == '"' ||
        in->text[p] == '\'' || in->text[p] == ')' || in->text[p] == ']' || in->text[p] == '}')
        return 1;
    for (i = 0; i < sizeof(closing_quotes) / sizeof(closing_quotes[0]); ++i) {
        size_t n = strlen(closing_quotes[i]);
        if (in->length - p >= n && !memcmp(in->text + p, closing_quotes[i], n))
            return 1;
    }
    return 0;
}

/* A word immediately following the dot is an identifier, not an abbreviation
   boundary (Vol.3, ltd.example). A Korean particle remains a valid boundary. */
static int dotted_boundary(const ttf_pack_input_t *in, size_t end) {
    char c;
    uint32_t cp;
    size_t width;
    if (end == in->length) return 1;
    c = in->text[end];
    if ((unsigned char)c >= 128) {
        if (ttf_utf8_decode(in->text + end, in->length - end, &cp, &width) != 1)
            return 0;
        return ttf_script_of(cp) != TTF_SCRIPT_LATIN && ttf_script_of(cp) != TTF_SCRIPT_DIGIT;
    }
    return !alpha(c) && !(c >= '0' && c <= '9') &&
           c != '_' && c != '-' && c != '/' && c != '\\' && c != '@';
}
static int dotted_keeps_period(const ttf_pack_input_t *in, size_t end, const char *key) {
    size_t p = end;
    const char *policy;
    if (terminal_period(in, end) || (end < in->length && in->text[end] == '.'))
        return 1;
    while (p < in->length && horizontal(in->text[p])) ++p;
    policy = lookup(in->engine, "abbreviation_dotted_period", key, NULL);
    /* A capitalized following word may start a sentence. Preserve that dot,
       unless the resource declares a connective such as 'vs. Beta'. */
    return p > end && p < in->length && upper(in->text[p]) &&
           (!policy || strcmp(policy, "continuation"));
}

void ttf_en_initialism_policy(const ttf_engine_t *engine, ttf_semantic_t *s, const char *surface,
                              size_t length, const char *kind) {
    char key[TTF_IR_VALUE_SIZE];
    size_t i, used = 0;
    for (i = 0; i < length && used + 1 < sizeof(key); ++i)
        if (alpha(surface[i]))
            key[used++] = lower(surface[i]);
    key[used] = '\0';
    (void)snprintf(s->secondary, sizeof(s->secondary), "%s", kind);
    if (!s->value[0] && length < sizeof(s->value)) {
        for (i = 0; i < used; ++i)
            s->value[i] = (char)(key[i] - 'a' + 'A');
        s->value[used] = '\0';
    }
    if (!lookup(engine, "initialism_text", key, s))
        (void)lookup(engine, "initialism_policy", kind, s);
}

size_t ttf_en_classify_abbreviation(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const ttf_token_t *token;
    char key[96];
    size_t start, end, p, used = 0, n, i;
    const char *expansion;
    if (!in || !out || in->index >= in->token_count)
        return 0;
    token = &in->tokens[in->index];
    start = token->begin;
    if (token->script != TTF_SCRIPT_LATIN || !left_boundary(in, start))
        return 0;
    n = token->end - start;
    /* Kansas City, MO. / NEW YORK, N.Y. / Austin, TX in: a state after a city and
       a comma reads its name (us_state; 2026-10-07). */
    if (start >= 3 && in->text[start - 1] == ' ' && in->text[start - 2] == ',' &&
        capitalized_before(in, start - 2, 0) &&
        ((in->text[start - 3] >= 'a' && in->text[start - 3] <= 'z') ||
         (n == 1 && start + 1 < in->length && in->text[start + 1] == '.')) &&
        upper(in->text[start])) {
        char state[3] = {0, 0, 0};
        size_t end = 0;
        if (n == 2 && upper(in->text[start + 1]))
            end = token->end;
        else if (n == 1 && start + 3 < in->length && in->text[start + 1] == '.' &&
                 upper(in->text[start + 2]) && in->text[start + 3] == '.' &&
                 (start + 4 == in->length || !alpha(in->text[start + 4])))
            end = start + 3;
        if (end && (end == in->length || !alpha(in->text[end + (in->text[end] == '.' ? 1 : 0)]) ||
                    in->text[end] == ' ')) {
            state[0] = lower(in->text[start]);
            state[1] = lower(in->text[end - 1]);
            expansion = lookup(in->engine, "us_state", state, NULL);
            if (expansion && strlen(expansion) < sizeof(out->value) &&
                (end == in->length || in->text[end] == ' ' || in->text[end] == '.' ||
                 in->text[end] == ',' || in->text[end] == ';' || in->text[end] == ')')) {
                out->type = TTF_SEM_ABBREVIATION;
                out->lexical_class = TTF_LEX_WORD;
                /* N.Y. at a sentence end keeps its last period as the full stop. */
                out->input_end = end;
                (void)lookup(in->engine, "us_state", state, out);
                (void)snprintf(out->value, sizeof(out->value), "%s", state);
                (void)snprintf(out->secondary, sizeof(out->secondary), "state");
                (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
                (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.state");
                return token_count(in, out->input_end);
            }
        }
    }
    if (n < sizeof(key) && token->end < in->length && in->text[token->end] == '.') {
        for (i = 0; i < n; ++i)
            key[i] = lower(in->text[start + i]);
        key[n] = '\0';
        /* Grey St. The fare, Main St. right now, Circle Dr., Santa Maria: after a
           capitalized name and before no other name, St. is Street (it was Grey
           Saint The fare; 2026-10-07). The period stays when a sentence follows. */
        expansion = lookup(in->engine, "abbreviation_street", key, NULL);
        if (expansion && strlen(expansion) < sizeof(out->value) && capitalized_before(in, start, 4) &&
            !(n > 1 && upper(in->text[start + 1]))) {
            size_t q = token->end + 1;
            int next_name, new_sentence;
            while (q < in->length && horizontal(in->text[q]))
                ++q;
            new_sentence = q == in->length || (q > token->end + 1 && upper(in->text[q]) && sentence_word(in, q));
            next_name = q > token->end + 1 && q < in->length && upper(in->text[q]) && !new_sentence;
            if (!next_name) {
                out->type = TTF_SEM_ABBREVIATION;
                out->lexical_class = TTF_LEX_WORD;
                out->input_end = new_sentence ? token->end : token->end + 1;
                (void)lookup(in->engine, "abbreviation_street", key, out);
                (void)snprintf(out->value, sizeof(out->value), "%s", key);
                (void)snprintf(out->secondary, sizeof(out->secondary), "street");
                (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
                (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.street");
                return token_count(in, out->input_end);
            }
        }
        /* 'Girls Like You' ft. Cardi B, (ft. Justin Bieber): right after a title's closing
           quote or bracket, or opening a bracket, ft. credits a featured artist; elsewhere it
           stays Fort (Ft. Worth; 2026-10-29 owner request: English song titles). */
        expansion = lookup(in->engine, "abbreviation_credit", key, NULL);
        if (expansion && credit_position(in, start)) {
            out->type = TTF_SEM_ABBREVIATION;
            out->lexical_class = TTF_LEX_WORD;
            out->input_end = token->end + 1;
            (void)lookup(in->engine, "abbreviation_credit", key, out);
            (void)snprintf(out->value, sizeof(out->value), "%s", key);
            (void)snprintf(out->secondary, sizeof(out->secondary), "credit");
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
            (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.credit");
            return token_count(in, out->input_end);
        }
        expansion = lookup(in->engine, "abbreviation_expansion", key, NULL);
        if (expansion && following_name(in, token->end + 1)) {
            if (strlen(expansion) >= sizeof(out->value))
                return 0;
            out->type = TTF_SEM_ABBREVIATION;
            out->lexical_class = TTF_LEX_WORD;
            out->input_end = token->end + 1;
            (void)lookup(in->engine, "abbreviation_expansion", key, out);
            (void)snprintf(out->value, sizeof(out->value), "%s", key);
            (void)snprintf(out->secondary, sizeof(out->secondary), "title");
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
            (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.title");
            return token_count(in, out->input_end);
        }
        /* the 12th c. twelfth century, the 5th c. BC: after an ordinal number a dotted c
           reads abbreviation_ordinal (2026-11-01, round 23; was the twelfth c.) */
        expansion = lookup(in->engine, "abbreviation_ordinal", key, NULL);
        if (expansion && strlen(expansion) < sizeof(out->value)) {
            size_t p = start;
            char a, b;
            while (p && horizontal(in->text[p - 1]))
                --p;
            a = p >= 3 ? lower(in->text[p - 2]) : 0;
            b = p >= 3 ? lower(in->text[p - 1]) : 0;
            if (p != start && p >= 3 && in->text[p - 3] >= '0' && in->text[p - 3] <= '9' &&
                ((a == 's' && b == 't') || (a == 'n' && b == 'd') || (a == 'r' && b == 'd') || (a == 't' && b == 'h'))) {
                out->type = TTF_SEM_ABBREVIATION;
                out->lexical_class = TTF_LEX_WORD;
                out->input_end = token->end + 1;
                (void)lookup(in->engine, "abbreviation_ordinal", key, out);
                (void)snprintf(out->value, sizeof(out->value), "%s", key);
                (void)snprintf(out->secondary, sizeof(out->secondary), "ordinal");
                (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
                (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.ordinal");
                return token_count(in, out->input_end);
            }
        }
        /* No. 5, no. 7: a number abbreviation before digits reads its word
           (number five); without a number it keeps its text (2026-10-07). */
        expansion = lookup(in->engine, "abbreviation_number", key, NULL);
        /* Vol.2: a number written against the period reads the number word; Vol. 3 with a
           space keeps the dotted reading and its overrides (2026-10-29) */
        /* EP.1 (a record) is letters while Ep.4 is an episode: a key marked
           abbreviation_number_case mixed skips an all-capital spelling; NO.1 still reads
           number (2026-10-29). A one-letter key in capitals is all-capital too: Section C. 5
           is a letter label, c. 1850 circa (round 25) */
        if (expansion && strcmp(expansion, "preserve") && following_digit(in, token->end + 1) &&
            !(upper(in->text[start]) && (n == 1 || upper(in->text[start + 1])) &&
              lookup(in->engine, "abbreviation_number_case", key, NULL)) &&
            !(lookup(in->engine, "abbreviation_dotted", key, NULL) && token->end + 1 < in->length &&
              horizontal(in->text[token->end + 1]))) {
            if (strlen(expansion) >= sizeof(out->value))
                return 0;
            out->type = TTF_SEM_ABBREVIATION;
            out->lexical_class = TTF_LEX_WORD;
            out->input_end = token->end + 1;
            (void)lookup(in->engine, "abbreviation_number", key, out);
            (void)snprintf(out->value, sizeof(out->value), "%s", key);
            (void)snprintf(out->secondary, sizeof(out->secondary), "number");
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
            (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.number");
            return token_count(in, out->input_end);
        }
        expansion = lookup(in->engine, "abbreviation_dotted", key, NULL);
        if (expansion && strcmp(expansion, "preserve") &&
            dotted_boundary(in, token->end + 1)) {
            out->type = TTF_SEM_ABBREVIATION;
            out->lexical_class = TTF_LEX_WORD;
            out->input_end = dotted_keeps_period(in, token->end + 1, key)
                                 ? token->end : token->end + 1;
            (void)lookup(in->engine, "abbreviation_dotted", key, out);
            (void)lookup(in->engine, "abbreviation_dotted_period", key, out);
            (void)snprintf(out->value, sizeof(out->value), "%s", key);
            (void)snprintf(out->secondary, sizeof(out->secondary), "dotted");
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
            (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.dotted");
            return token_count(in, out->input_end);
        }
    }
    /* York st, McKinley ave, st Kilda, dr Smith, mt Eden: without a period a
       street word after a capitalized name and before no other name reads as a
       street, and a title word (bare_title) before a name reads as a title, as
       with the period (the English references, 2026-10-08). */
    if (n >= 2 && n < 5 && n < sizeof(key) &&
        (token->end == in->length || strchr(" ,;:)", in->text[token->end])) &&
        !upper(in->text[start + 1])) {
        for (i = 0; i < n; ++i)
            key[i] = lower(in->text[start + i]);
        key[n] = '\0';
        expansion = lookup(in->engine, "abbreviation_street", key, NULL);
        if (expansion && strlen(expansion) < sizeof(out->value) && capitalized_before(in, start, 4) &&
            !following_name(in, token->end)) {
            out->type = TTF_SEM_ABBREVIATION;
            out->lexical_class = TTF_LEX_WORD;
            out->input_end = token->end;
            (void)lookup(in->engine, "abbreviation_street", key, out);
            (void)snprintf(out->value, sizeof(out->value), "%s", key);
            (void)snprintf(out->secondary, sizeof(out->secondary), "street");
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
            (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.street-bare");
            return token_count(in, out->input_end);
        }
        expansion = lookup(in->engine, "abbreviation_expansion", key, NULL);
        if (expansion && lookup(in->engine, "bare_title", key, NULL) && following_name(in, token->end) &&
            strlen(expansion) < sizeof(out->value)) {
            out->type = TTF_SEM_ABBREVIATION;
            out->lexical_class = TTF_LEX_WORD;
            out->input_end = token->end;
            (void)lookup(in->engine, "abbreviation_expansion", key, out);
            (void)snprintf(out->value, sizeof(out->value), "%s", key);
            (void)snprintf(out->secondary, sizeof(out->secondary), "title");
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
            (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.title-bare");
            return token_count(in, out->input_end);
        }
        /* Smith sr, Warner bros: lower-case only, so SR (a company) stays. */
        expansion = lookup(in->engine, "abbreviation_bare_lower", key, NULL);
        if (expansion && strlen(expansion) < sizeof(out->value)) {
            out->type = TTF_SEM_ABBREVIATION;
            out->lexical_class = TTF_LEX_WORD;
            out->input_end = token->end;
            (void)lookup(in->engine, "abbreviation_bare_lower", key, out);
            (void)snprintf(out->value, sizeof(out->value), "%s", key);
            (void)snprintf(out->secondary, sizeof(out->secondary), "bare_lower");
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
            (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.bare-lower");
            return token_count(in, out->input_end);
        }
    }
    /* A few abbreviations are unambiguous without their period, so the data may
       declare an expansion that applies to the bare token: "vol 1", "Records
       ltd". */
    if (n < sizeof(key) && (token->end == in->length || in->text[token->end] != '.')) {
        for (i = 0; i < n; ++i)
            key[i] = lower(in->text[start + i]);
        key[n] = '\0';
        expansion = lookup(in->engine, "abbreviation_bare", key, NULL);
        if (expansion && strlen(expansion) < sizeof(out->value)) {
            out->type = TTF_SEM_ABBREVIATION;
            out->lexical_class = TTF_LEX_WORD;
            out->input_end = token->end;
            (void)lookup(in->engine, "abbreviation_bare", key, out);
            (void)snprintf(out->value, sizeof(out->value), "%s", key);
            (void)snprintf(out->secondary, sizeof(out->secondary), "bare");
            (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
            (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.bare");
            return token_count(in, out->input_end);
        }
    }
    /* Route NO 128, Route No 138: a capital No inside a sentence before a number
       reads its number word without the period too; a lowercase no stays a word
       (2026-10-07). */
    if (n == 2 && upper(in->text[start]) && (in->text[start + 1] | 0x20) == 'o' &&
        (in->text[start] | 0x20) == 'n' && token->end < in->length && in->text[token->end] == ' ' &&
        following_digit(in, token->end)) {
        size_t p = start;
        while (p && horizontal(in->text[p - 1]))
            --p;
        if (p != start && p && (alpha(in->text[p - 1]) || in->text[p - 1] == '(' ||
                                in->text[p - 1] == ',' ||
                                (in->text[p - 1] >= '0' && in->text[p - 1] <= '9'))) {
            expansion = lookup(in->engine, "abbreviation_number", "no", NULL);
            if (expansion && strcmp(expansion, "preserve") && strlen(expansion) < sizeof(out->value)) {
                out->type = TTF_SEM_ABBREVIATION;
                out->lexical_class = TTF_LEX_WORD;
                out->input_end = token->end;
                (void)lookup(in->engine, "abbreviation_number", "no", out);
                (void)snprintf(out->value, sizeof(out->value), "no");
                (void)snprintf(out->secondary, sizeof(out->secondary), "number");
                (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
                (void)snprintf(out->rule, sizeof(out->rule), "en.abbreviation.number");
                return token_count(in, out->input_end);
            }
        }
    }
    if (n != 1 || !upper(in->text[start]))
        return 0;
    p = start;
    end = start;
    while (p + 1 < in->length && upper(in->text[p]) && in->text[p + 1] == '.') {
        if (used + 1 >= sizeof(out->value)) {
            out->value[0] = '\0';
            return 0;
        }
        out->value[used++] = in->text[p];
        end = p + 2;
        p = end;
        while (p < in->length && horizontal(in->text[p]))
            ++p;
    }
    if (!used || (used == 1 && !following_name(in, end) && !initial_continuation(in, end))) {
        out->value[0] = '\0';
        return 0;
    }
    /* Do not consume the prefix of an identifier such as U.S.Beta, U.S.2,
       or U.S.-C. Bare Korean particles are a normal code-mixed boundary. */
    if (end < in->length &&
        (alpha(in->text[end]) || (in->text[end] >= '0' && in->text[end] <= '9') ||
         in->text[end] == '_' || in->text[end] == '-' || in->text[end] == '/' ||
         in->text[end] == '\\' || in->text[end] == '@')) {
        out->value[0] = '\0';
        return 0;
    }
    out->value[used] = '\0';
    out->type = TTF_SEM_INITIALISM;
    out->lexical_class = TTF_LEX_INITIALISM;
    /* A sentence-final dot remains its own punctuation token so every
       render language preserves it after pronunciation. */
    out->input_end = terminal_period(in, end) ? end - 1 : end;
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    (void)snprintf(out->rule, sizeof(out->rule), "en.initialism.dotted");
    ttf_en_initialism_policy(in->engine, out, in->text + start, end - start, "dotted");
    return token_count(in, out->input_end);
}

/* An uppercase run that reads as an English word keeps its written form in
   speech (NASA, FIFA, EXCEL); one that cannot be pronounced is spelled (FBI).
   The clusters that may start or end a syllable are resources. */
static int cluster(const ttf_engine_t *engine, const char *kind, const char *text, size_t length) {
    char key[8];
    if (length == 1)
        return 1;
    if (length >= sizeof(key))
        return 0;
    memcpy(key, text, length);
    key[length] = '\0';
    return lookup(engine, kind, key, NULL) != NULL;
}

static int pronounceable(const ttf_engine_t *engine, const char *word, size_t length) {
    size_t i = 0, vowels = 0, split;
    if (length < 3)
        return 0;
    /* USA, CEO, EPA, UFO: an unknown three-letter capital word ending in a vowel
       is letters (the English references, 2026-10-08), unless it is a common
       short word (short_word: ONE, TWO, SHE); END, NEW keep the word test. */
    if (length == 3 && strchr("aeiou", word[2])) {
        char key[4] = {word[0], word[1], word[2], 0};
        if (!lookup(engine, "short_word", key, NULL))
            return 0;
    }
    for (i = 0; i < length; ++i)
        if (strchr("aeiouy", word[i]))
            ++vowels;
    if (!vowels)
        return 0;
    for (i = 0; i < length;) {
        size_t run = i;
        if (strchr("aeiouy", word[i])) {
            ++i;
            continue;
        }
        while (i < length && !strchr("aeiouy", word[i]))
            ++i;
        if (i - run == 1 && run && i != length)
            continue; /* a single consonant between vowels always divides */
        if (!run && cluster(engine, "syllable_onset", word + run, i - run))
            continue;
        if (i == length && cluster(engine, "syllable_coda", word + run, i - run))
            continue;
        /* A plural or third-person s after a final coda (IMPRESSIONS, PLANTS).
           Short runs stay initialisms: IRS, ALS, APS. */
        if (i == length && length >= 5 && i - run > 1 && word[i - 1] == 's' &&
            cluster(engine, "syllable_coda", word + run, i - run - 1))
            continue;
        if (!run || i == length)
            return 0;
        /* Inside a word an s may follow the coda (THURS-DAY, 2026-10-07); a
           final rs stays letters (IRS). */
        for (split = 1; split < i - run; ++split)
            if ((cluster(engine, "syllable_coda", word + run, split) ||
                 (split > 1 && word[run + split - 1] == 's' &&
                  cluster(engine, "syllable_coda", word + run, split - 1))) &&
                cluster(engine, "syllable_onset", word + run + split, i - run - split))
                break;
        if (split >= i - run)
            return 0;
    }
    return 1;
}

ttf_status_t ttf_en_verbalize_abbreviation(const ttf_engine_t *engine, const ttf_semantic_t *s,
                                           const char *surface, size_t length, ttf_buffer_t *out) {
    const char *value, *style;
    char key[TTF_IR_VALUE_SIZE];
    size_t i, used = 0;
    ttf_status_t status;
    if (s->type == TTF_SEM_ABBREVIATION) {
        if (!strcmp(s->secondary, "dotted")) {
            value = lookup(engine, "abbreviation_dotted", s->value, NULL);
            if (!value || !strcmp(value, "preserve"))
                return ttf_buffer_append(out, surface, length);
            ttf_trace_stage(engine, TTF_TRACE_POLICY, 0, length);
            status = ttf_trace_resource(engine, "abbreviation_dotted", EN, s->value,
                                        s->value, "dotted-abbreviation-reading", SIZE_MAX);
            if (status == TTF_OK && lookup(engine, "abbreviation_dotted_period", s->value, NULL))
                status = ttf_trace_resource(engine, "abbreviation_dotted_period", EN, s->value,
                                            s->value, "dotted-abbreviation-period", SIZE_MAX);
            return status == TTF_OK ? ttf_buffer_puts(out, value) : status;
        }
        if (!strcmp(s->secondary, "street") || !strcmp(s->secondary, "state")) {
            const char *kind = !strcmp(s->secondary, "street") ? "abbreviation_street" : "us_state";
            value = lookup(engine, kind, s->value, NULL);
            if (!value)
                return ttf_buffer_append(out, surface, length);
            status = ttf_trace_resource(engine, kind, EN, s->value, s->value, "abbreviation-reading",
                                        SIZE_MAX);
            return status == TTF_OK ? ttf_buffer_puts(out, value) : status;
        }
        if (!strcmp(s->secondary, "credit")) {
            value = lookup(engine, "abbreviation_credit", s->value, NULL);
            return value ? ttf_buffer_puts(out, value) : ttf_buffer_append(out, surface, length);
        }
        if (!strcmp(s->secondary, "ordinal")) {
            value = lookup(engine, "abbreviation_ordinal", s->value, NULL);
            return value ? ttf_buffer_puts(out, value) : ttf_buffer_append(out, surface, length);
        }
        if (!strcmp(s->secondary, "number")) {
            value = lookup(engine, "abbreviation_number", s->value, NULL);
            if (!value || !strcmp(value, "preserve"))
                return ttf_buffer_append(out, surface, length);
            status = ttf_trace_resource(engine, "abbreviation_number", EN, s->value, s->value,
                                        "number-abbreviation-reading", SIZE_MAX);
            return status == TTF_OK ? ttf_buffer_puts(out, value) : status;
        }
        if (!strcmp(s->secondary, "bare") || !strcmp(s->secondary, "bare_lower")) {
            value = lookup(engine, !strcmp(s->secondary, "bare") ? "abbreviation_bare"
                                                                  : "abbreviation_bare_lower",
                           s->value, NULL);
            return value && strcmp(value, "preserve") ? ttf_buffer_puts(out, value)
                                                      : ttf_buffer_append(out, surface, length);
        }
        if (strcmp(s->secondary, "title"))
            return ttf_buffer_append(out, surface, length);
        value = lookup(engine, "abbreviation_expansion", s->value, NULL);
        if (!value || !strcmp(value, "preserve"))
            return ttf_buffer_append(out, surface, length);
        return ttf_buffer_puts(out, value);
    }
    if (s->type != TTF_SEM_INITIALISM)
        return TTF_EINVAL;
    /* Foreign pronunciation receives the canonical spelling, not English
       letter-name words. This keeps letter phones and Korean lexicon hits. */
    if (strcmp(s->render_lang, EN))
        return s->value[0] ? ttf_buffer_puts(out, s->value)
                           : ttf_buffer_append(out, surface, length);
    for (i = 0; s->value[i] && used + 1 < sizeof(key); ++i)
        if (alpha(s->value[i]))
            key[used++] = lower(s->value[i]);
    key[used] = '\0';
    style = lookup(engine, "initialism_text", key, NULL);
    /* A value that is not letters alone carries its own reading, such as the
       ampersand of R&D; the speech default must not swallow it. */
    if (!style && used == strlen(s->value) &&
        engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
        char speech_key[TTF_IR_VALUE_SIZE + 24];
        (void)snprintf(speech_key, sizeof(speech_key), "%s_speech",
                       !strncmp(s->secondary, "article_", 8) ? s->secondary
                       : pronounceable(engine, key, used) ? "pronounceable" : s->secondary);
        style = lookup(engine, "initialism_policy", speech_key, NULL);
    }
    if (!style)
        style = lookup(engine, "initialism_policy", s->secondary, NULL);
    if (!style || !strcmp(style, "preserve"))
        return ttf_buffer_append(out, surface, length);
    if (strcmp(style, "letters") && strcmp(style, "names"))
        return TTF_ERESOURCE;
    for (i = 0; i < used; ++i) {
        char letter[2] = {key[i], '\0'};
        if (i && (status = ttf_buffer_puts(out, " ")) != TTF_OK)
            return status;
        value = !strcmp(style, "names") ? lookup(engine, "letter_name", letter, NULL) : letter;
        if (!value)
            return TTF_ERESOURCE;
        if ((status = ttf_buffer_puts(out, value)) != TTF_OK)
            return status;
    }
    return TTF_OK;
}
