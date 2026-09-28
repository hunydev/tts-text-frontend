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
    return p > end && p < in->length && upper(in->text[p]) && p + 1 < in->length &&
           (alpha(in->text[p + 1]) || in->text[p + 1] == '.');
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
    if (n < sizeof(key) && token->end < in->length && in->text[token->end] == '.') {
        for (i = 0; i < n; ++i)
            key[i] = lower(in->text[start + i]);
        key[n] = '\0';
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
    /* A few abbreviations are unambiguous without their period, so the data may
       declare an expansion that applies to the bare token: "vol 1", "Records
       ltd". A token whose bare reading is contested, such as st or dr, is not
       declared and keeps its text. */
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
        for (split = 1; split < i - run; ++split)
            if (cluster(engine, "syllable_coda", word + run, split) &&
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
        if (!strcmp(s->secondary, "bare")) {
            value = lookup(engine, "abbreviation_bare", s->value, NULL);
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
                       pronounceable(engine, key, used) ? "pronounceable" : s->secondary);
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
