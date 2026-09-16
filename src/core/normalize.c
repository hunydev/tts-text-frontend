#include "internal.h"
const ttf_entry_t *ttf_rule_match(const ttf_engine_t *, const char *, const char *, size_t,
                                  size_t *);
ttf_status_t ttf_foreign_render(const ttf_engine_t *, const ttf_language_pack_t *,
                                const ttf_language_pack_t *, const char *, size_t,
                                ttf_lexical_class_t, ttf_buffer_t *, char[64],
                                ttf_resource_layer_t *);
static void tag(char dest[TTF_LANGUAGE_TAG_SIZE], const char *source) {
    (void)snprintf(dest, TTF_LANGUAGE_TAG_SIZE, "%s", source);
}
typedef struct {
    size_t begin, end;
} context_scope_t;

/* A complete quotation is an independent context window. Numeric/lexical
   classification still sees the original token stream and byte offsets. */
static context_scope_t *quote_scopes(const char *text, size_t length, const ttf_token_t *tokens,
                                     size_t count) {
    context_scope_t *scopes;
    size_t starts[32], depth = 0, i;
    uint32_t closers[32];
    if (!count)
        return NULL;
    if (count > SIZE_MAX / sizeof(*scopes))
        return NULL;
    scopes = (context_scope_t *)calloc(count, sizeof(*scopes));
    if (!scopes)
        return NULL;
    for (i = 0; i < count; ++i) {
        uint32_t cp, closer;
        size_t width, j;
        if (ttf_utf8_decode(text + tokens[i].begin, length - tokens[i].begin, &cp, &width) != 1)
            continue;
        if (depth && cp == closers[depth - 1]) {
            size_t begin = starts[--depth] + 1;
            for (j = begin; j < i; ++j)
                if (!scopes[j].end) {
                    scopes[j].begin = begin;
                    scopes[j].end = i;
                }
        } else if ((closer = ttf_quote_closer(cp)) && depth < 32) {
            starts[depth] = i;
            closers[depth++] = closer;
        }
    }
    return scopes;
}
static const ttf_language_pack_t *script_pack(const ttf_engine_t *e, ttf_script_t script) {
    const ttf_language_pack_t *best = NULL;
    int score = 0;
    size_t j;
    for (j = 0; j < e->pack_count; j++) {
        int n = e->packs[j]->script_score ? e->packs[j]->script_score(script) : 0;
        if (n > score) {
            score = n;
            best = e->packs[j];
        }
    }
    return best;
}
static const ttf_language_pack_t *context_pack(const ttf_engine_t *e, const ttf_pack_input_t *in,
                                               char trace[192]) {
    const ttf_language_pack_t *best = ttf_find_pack(e, e->default_language);
    int score = 0;
    size_t j;
    for (j = 0; j < e->pack_count; j++) {
        int n = e->packs[j]->context_score ? e->packs[j]->context_score(in) : 0;
        size_t used = strlen(trace);
        if (used < 191)
            snprintf(trace + used, 192 - used, "%s%s=%d", used ? " " : "",
                     e->packs[j]->language_tag, n);
        if (n > score) {
            score = n;
            best = e->packs[j];
        }
    }
    return best;
}
static int foreign_type(ttf_semantic_type_t t) {
    return t == TTF_SEM_WORD || t == TTF_SEM_INITIALISM || t == TTF_SEM_ACRONYM ||
           t == TTF_SEM_ALNUM_TERM || t == TTF_SEM_ORDINAL || t == TTF_SEM_ABBREVIATION;
}
static ttf_status_t add_span(ttf_result_t *r, const ttf_span_t *span, size_t *capacity) {
    if (r->span_count == *capacity) {
        size_t cap = *capacity ? *capacity * 2 : 32;
        ttf_span_t *p;
        if (cap < *capacity || cap > SIZE_MAX / sizeof(*p))
            return TTF_ELIMIT;
        p = (ttf_span_t *)realloc(r->spans, cap * sizeof(*p));
        if (!p)
            return TTF_ENOMEM;
        r->spans = p;
        *capacity = cap;
    }
    r->spans[r->span_count++] = *span;
    return TTF_OK;
}
ttf_status_t ttf_process_unit(const ttf_engine_t *e, const char *text, size_t length, size_t offset,
                              ttf_result_t *out) {
    ttf_token_t *tokens = NULL;
    context_scope_t *scopes = NULL;
    size_t count = 0, i = 0, capacity = 0;
    ttf_buffer_t output = {0};
    ttf_status_t st;
    memset(out, 0, sizeof(*out));
    st = ttf_tokenize(text, length, &tokens, &count);
    if (st != TTF_OK)
        return st;
    scopes = quote_scopes(text, length, tokens, count);
    if (count && !scopes) {
        free(tokens);
        return TTF_ENOMEM;
    }
    while (i < count) {
        ttf_pack_input_t in;
        ttf_span_t span;
        ttf_semantic_t *sem = &span.semantic;
        size_t consumed = 0, j, end, matched = 0;
        const ttf_language_pack_t *source, *context, *renderer;
        const ttf_entry_t *rule = NULL;
        ttf_buffer_t normalized = {0};
        memset(&span, 0, sizeof(span));
        sem->struct_size = sizeof(*sem);
        sem->type = TTF_SEM_TEXT;
        sem->layer = TTF_LAYER_COMMON;
        in.text = text;
        in.length = length;
        in.tokens = tokens;
        in.token_count = count;
        in.index = i;
        in.engine = e;
        if (scopes[i].end) {
            ttf_pack_input_t scoped = in;
            scoped.tokens = tokens + scopes[i].begin;
            scoped.token_count = scopes[i].end - scopes[i].begin;
            scoped.index -= scopes[i].begin;
            snprintf(span.context_trace, sizeof(span.context_trace), "quotation");
            context = context_pack(e, &scoped, span.context_trace);
        } else
            context = context_pack(e, &in, span.context_trace);
        source = script_pack(e, tokens[i].script);
        if (!context) {
            st = TTF_EINVAL;
            goto fail;
        }
        if (tokens[i].script != TTF_SCRIPT_WHITESPACE) {
            consumed = ttf_classify_universal(&in, sem);
            for (j = 0; j < e->pack_count; j++)
                if (e->packs[j]->classify) {
                    ttf_semantic_t candidate;
                    size_t n;
                    memset(&candidate, 0, sizeof(candidate));
                    candidate.struct_size = sizeof(candidate);
                    candidate.layer = TTF_LAYER_SYSTEM;
                    n = e->packs[j]->classify(&in, &candidate);
                    /* An explicit pack identifier cue is more specific than
                       generic numeric syntax over the identical byte extent. */
                    int labeled_identifier = n && n == consumed &&
                        candidate.input_end == sem->input_end &&
                        candidate.type == TTF_SEM_DIGIT_SEQUENCE &&
                        (sem->type == TTF_SEM_CARDINAL || sem->type == TTF_SEM_NUMBER ||
                         sem->type == TTF_SEM_PHONE || sem->type == TTF_SEM_RANGE ||
                         sem->type == TTF_SEM_DATE ||
                         sem->type == TTF_SEM_TEXT);
                    if (n > consumed || (n && n == consumed && sem->type == TTF_SEM_WORD) ||
                        labeled_identifier) {
                        *sem = candidate;
                        consumed = n;
                    }
                }
            rule = ttf_rule_match(e, context->language_tag, text + tokens[i].begin,
                                  length - tokens[i].begin, &matched);
        }
        if (rule) {
            sem->type = TTF_SEM_CUSTOM;
            sem->layer = rule->layer;
            snprintf(sem->rule, sizeof(sem->rule), "%s:%.48s", rule->kind, rule->key);
            sem->input_end = tokens[i].begin + matched;
            consumed = 0;
            while (i + consumed < count && tokens[i + consumed].begin < sem->input_end)
                consumed++;
        }
        if (!consumed) {
            consumed = 1;
            sem->type =
                (tokens[i].script == TTF_SCRIPT_LATIN || tokens[i].script == TTF_SCRIPT_HANGUL ||
                 tokens[i].script == TTF_SCRIPT_OTHER)
                    ? TTF_SEM_WORD
                    : TTF_SEM_TEXT;
            if (tokens[i].script == TTF_SCRIPT_SYMBOL)
                sem->type = TTF_SEM_SYMBOL;
            sem->lexical_class =
                tokens[i].script == TTF_SCRIPT_LATIN ? TTF_LEX_UNKNOWN_LATIN : TTF_LEX_NONE;
            snprintf(sem->rule, sizeof(sem->rule), "identity");
        }
        if (consumed > count - i) {
            st = TTF_ERESOURCE;
            goto fail;
        }
        end = sem->input_end ? sem->input_end : tokens[i + consumed - 1].end;
        if (end <= tokens[i].begin || end > tokens[i + consumed - 1].end) {
            st = TTF_ERESOURCE;
            goto fail;
        }
        if (!source)
            for (j = i; j < i + consumed; j++) {
                source = script_pack(e, tokens[j].script);
                if (source)
                    break;
            }
        /* Numeric units convey semantics; an attached unit does not change the
           source language of a neutral number. Ordinal suffixes do carry source. */
        if (sem->type != TTF_SEM_ORDINAL && !foreign_type(sem->type) && !rule)
            source = script_pack(e, tokens[i].script);
        if (sem->source_lang[0])
            source = ttf_find_pack(e, sem->source_lang);
        tag(sem->source_lang, source ? source->language_tag : "und");
        tag(sem->context_lang, context->language_tag);
        renderer = context;
        if (e->config.render_policy == TTF_PRESERVE_LANGUAGE && source)
            renderer = source;
        tag(sem->render_lang, renderer->language_tag);
        sem->input_begin = tokens[i].begin + offset;
        sem->input_end = end + offset;
        span.input_begin = sem->input_begin;
        span.input_end = sem->input_end;
        span.script = tokens[i].script;
        tag(span.verbalizer, renderer->language_tag);
        if (rule)
            st = ttf_buffer_puts(&normalized, rule->value);
        else if (source && source != context && foreign_type(sem->type) &&
                 e->config.render_policy == TTF_PHONETIC_LOCALIZE) {
            ttf_buffer_t spoken = {0};
            tag(span.verbalizer, source->language_tag);
            st = source->verbalize(e, sem, text + tokens[i].begin, end - tokens[i].begin, &spoken);
            if (st == TTF_OK)
                st = ttf_foreign_render(e, source, context, spoken.data ? spoken.data : "",
                                        spoken.length, sem->lexical_class, &normalized,
                                        span.lexicon, &span.pronunciation_layer);
            ttf_buffer_free(&spoken);
            if (st == TTF_OK && !strcmp(span.lexicon, "preserved:no-renderer"))
                tag(sem->render_lang, source->language_tag);
        } else
            st = renderer->verbalize(e, sem, text + tokens[i].begin, end - tokens[i].begin,
                                     &normalized);
        if (st != TTF_OK) {
            ttf_buffer_free(&normalized);
            goto fail;
        }
        span.begin = output.length;
        st = ttf_buffer_append(&output, normalized.data, normalized.length);
        span.end = output.length;
        ttf_buffer_free(&normalized);
        if (st != TTF_OK)
            goto fail;
        st = add_span(out, &span, &capacity);
        if (st != TTF_OK)
            goto fail;
        j = i + consumed - 1;
        if (end < tokens[j].end) {
            tokens[j].begin = end;
            i = j;
        } else
            i = j + 1;
    }
    st = ttf_buffer_append(&output, "", 0);
    if (st != TTF_OK)
        goto fail;
    out->text = output.data;
    out->length = output.length;
    free(tokens);
    free(scopes);
    return TTF_OK;
fail:
    free(tokens);
    free(scopes);
    ttf_buffer_free(&output);
    ttf_result_free(out);
    return st;
}

typedef struct {
    ttf_buffer_t text;
    ttf_result_t *out;
    size_t cap;
    ttf_status_t status;
} collector_t;
static int collect(const ttf_segment_t *seg, void *userdata) {
    collector_t *c = (collector_t *)userdata;
    size_t i, base = c->text.length;
    c->status = ttf_buffer_append(&c->text, seg->text, seg->length);
    if (c->status != TTF_OK)
        return 1;
    for (i = 0; i < seg->span_count; i++) {
        ttf_span_t span = seg->spans[i];
        span.begin += base;
        span.end += base;
        c->status = add_span(c->out, &span, &c->cap);
        if (c->status != TTF_OK)
            return 1;
    }
    return 0;
}
ttf_status_t ttf_normalize(const ttf_engine_t *e, const char *text, size_t length,
                           ttf_result_t *out) {
    ttf_stream_t *s = NULL;
    collector_t c;
    ttf_status_t st;
    if (!e || !out || (!text && length))
        return TTF_EINVAL;
    memset(out, 0, sizeof(*out));
    memset(&c, 0, sizeof(c));
    c.out = out;
    st = ttf_stream_create(e, collect, &c, &s);
    if (st != TTF_OK)
        return st;
    st = ttf_stream_write(s, text, length);
    if (st == TTF_OK)
        st = ttf_stream_flush(s);
    ttf_stream_destroy(s);
    if (c.status != TTF_OK)
        st = c.status;
    if (st == TTF_OK)
        st = ttf_buffer_append(&c.text, "", 0);
    if (st != TTF_OK) {
        ttf_buffer_free(&c.text);
        ttf_result_free(out);
        return st;
    }
    out->text = c.text.data;
    out->length = c.text.length;
    return TTF_OK;
}
