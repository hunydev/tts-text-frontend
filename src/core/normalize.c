#include "internal.h"
#include "speech_output.h"
#include "../language/context_memo.h"
#include "../pronunciation/trace.h"
ttf_status_t ttf_trace_foreign_lexicon(const ttf_engine_t *, const char *, const char *,
                                       const char *, size_t, const char *);
#include "../protected/protected.h"
const ttf_entry_t *ttf_rule_match(const ttf_engine_t *, const char *, const char *, size_t,
                                  size_t *);
ttf_status_t ttf_foreign_render(const ttf_engine_t *, const ttf_language_pack_t *,
                                const ttf_language_pack_t *, const char *, size_t,
                                ttf_lexical_class_t, ttf_buffer_t *, char[64],
                                ttf_resource_layer_t *);
static void tag(char dest[TTF_LANGUAGE_TAG_SIZE], const char *source) {
    (void)snprintf(dest, TTF_LANGUAGE_TAG_SIZE, "%s", source);
}
/* Aliases share the normal layer order; an exact spelling wins ties, but a
   system exact spelling must not shadow a user's canonical pronunciation. */
static ttf_status_t pronunciation_candidate(const ttf_engine_t *engine, const char *source,
                                            const char *target, const char *text, size_t length,
                                            const char **best, ttf_resource_layer_t *layer,
                                            const char **selected_text, size_t *selected_length) {
    const char *candidate = NULL;
    ttf_resource_layer_t candidate_layer = TTF_LAYER_COMMON;
    ttf_status_t status =
        ttf_foreign_lookup(engine, source, target, text, length, &candidate, &candidate_layer);
    if (status == TTF_OK && candidate && (!*best || candidate_layer > *layer)) {
        *best = candidate;
        *layer = candidate_layer;
        *selected_text = text;
        *selected_length = length;
    }
    return status;
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
        if (tokens[i].script != TTF_SCRIPT_PUNCTUATION)
            continue;
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
/* An electronic address is neutral context evidence. Hide its internal Latin
   tokens for this one routing decision; do not mutate the shared token cache. */
static const ttf_language_pack_t *electronic_context(const ttf_engine_t *e,
                                                     const ttf_pack_input_t *in, size_t consumed,
                                                     char trace[192]) {
    ttf_pack_input_t neutral = *in;
    ttf_engine_t view = *e;
    ttf_token_t *tokens;
    const ttf_language_pack_t *pack;
    size_t after = in->index + consumed;
    if (in->token_count > SIZE_MAX / sizeof(*tokens))
        return NULL;
    tokens = malloc(in->token_count * sizeof(*tokens));
    if (!tokens)
        return NULL;
    memcpy(tokens, in->tokens, in->index * sizeof(*tokens));
    tokens[in->index] = in->tokens[in->index];
    tokens[in->index].end = in->tokens[after - 1].end;
    tokens[in->index].script = TTF_SCRIPT_OTHER;
    memcpy(tokens + in->index + 1, in->tokens + after, (in->token_count - after) * sizeof(*tokens));
    neutral.tokens = tokens;
    neutral.token_count -= consumed - 1;
    view.context_memo = NULL;
    neutral.engine = &view;
    snprintf(trace, 192, "electronic-neutral ");
    pack = context_pack(e, &neutral, trace);
    free(tokens);
    return pack;
}
static ttf_status_t render_control(const ttf_engine_t *e, const ttf_pack_input_t *in,
                                   const ttf_control_match_t *control, size_t offset,
                                   ttf_span_t *span, ttf_buffer_t *output) {
    const ttf_language_pack_t *context, *source = NULL;
    ttf_semantic_t *sem = &span->semantic;
    ttf_token_t *base_tokens = NULL;
    size_t base_count = 0, base_length = control->base_end - control->base_begin;
    ttf_buffer_t decoded = {0};
    ttf_status_t status;
    context = context_pack(e, in, span->context_trace);
    if (!context)
        return TTF_EINVAL;
    status = ttf_tokenize(in->text + control->base_begin, base_length, &base_tokens, &base_count);
    if (status != TTF_OK)
        return status;
    for (size_t k = 0; k < base_count && !source; ++k)
        source = script_pack(e, base_tokens[k].script);
    span->script = base_count ? base_tokens[0].script : TTF_SCRIPT_SYMBOL;
    free(base_tokens);
    sem->type = TTF_SEM_CUSTOM;
    sem->input_begin = span->input_begin = offset + control->begin;
    sem->input_end = span->input_end = offset + control->end;
    tag(sem->source_lang, source ? source->language_tag : "und");
    tag(sem->context_lang, context->language_tag);
    tag(sem->render_lang, context->language_tag);
    tag(span->verbalizer, context->language_tag);
    snprintf(sem->rule, sizeof(sem->rule), "control.%s",
             control->kind == TTF_CONTROL_PATTERN ? "pattern" : "literal");
    if (base_length < sizeof(sem->value))
        memcpy(sem->value, in->text + control->base_begin, base_length);
    else if (control->kind == TTF_CONTROL_PATTERN)
        return TTF_ELIMIT;
    status = ttf_control_decode(in->text, control->payload_begin, control->payload_end, &decoded);
    if (status != TTF_OK)
        goto done;
    ttf_trace_span_begin(e, span->input_begin, span->input_end);
    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, control->end - control->begin);
    if (control->kind == TTF_CONTROL_PATTERN) {
        if (decoded.length >= sizeof(sem->unit)) {
            status = TTF_ELIMIT;
            goto done;
        }
        memcpy(sem->unit, decoded.data, decoded.length);
        status = context->verbalize(e, sem, in->text + control->base_begin, base_length, output);
    } else {
        snprintf(sem->unit, sizeof(sem->unit), "@literal");
        status = ttf_buffer_append(output, decoded.data, decoded.length);
    }
    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, control->end - control->begin);
    if (status == TTF_OK)
        status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_USER, 0, context->language_tag,
                                "input-control", sem->unit, decoded.data ? decoded.data : "",
                                sem->value, "explicit-input-reading", SIZE_MAX);
done:
    ttf_buffer_free(&decoded);
    return status;
}
/* A declared particle written against the reading stays attached to it. */
static int following_particle(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                              const char *text, size_t n) {
    char key[32];
    size_t width = 0;
    while (width < n && width + 1 < sizeof(key)) {
        uint32_t cp;
        size_t step;
        if (ttf_utf8_decode(text + width, n - width, &cp, &step) != 1 ||
            ttf_script_of(cp) != TTF_SCRIPT_HANGUL || width + step + 1 >= sizeof(key))
            break;
        width += step;
    }
    if (!width)
        return 0;
    memcpy(key, text, width);
    key[width] = 0;
    return ttf_resource_get(e, "particle", renderer->language_tag, key, NULL) != NULL;
}

/* Korean-style particle agreement: a written particle that stands alone after a
   normalized reading takes the form the spoken text requires. The alternation
   table and forms are resources, so the mechanism stays language independent. */
static ttf_status_t particle_agreement(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                                       const char *source, size_t source_length,
                                       const ttf_buffer_t *output, ttf_buffer_t *normalized) {
    char key[32], forms[96], *vowel, *rieul;
    const char *value;
    uint32_t cp;
    size_t width, start = output->length;
    int final;
    /* Cheap shape tests first: a particle is a short non-ASCII span left as
       written, and it can only agree after a syllable of the same script. */
    if (!source_length || source_length >= sizeof(key) || !output->length ||
        !((unsigned char)source[0] & 0x80))
        return TTF_OK;
    if (normalized->length != source_length || memcmp(normalized->data, source, source_length))
        return TTF_OK;
    while (start && ((unsigned char)output->data[start - 1] & 0xc0) == 0x80)
        --start;
    if (!start ||
        ttf_utf8_decode(output->data + start - 1, output->length - start + 1, &cp, &width) != 1 ||
        cp < 0xac00 || cp > 0xd7a3)
        return TTF_OK;
    final = (int)((cp - 0xac00) % 28);
    memcpy(key, source, source_length);
    key[source_length] = 0;
    value = ttf_resource_get(e, "particle_form", renderer->language_tag, key, NULL);

    if (!value || strlen(value) >= sizeof(forms))
        return TTF_OK;
    memcpy(forms, value, strlen(value) + 1);
    vowel = strchr(forms, '|');
    if (!vowel)
        return TTF_OK;
    *vowel++ = 0;
    rieul = strchr(vowel, '|');
    if (rieul)
        *rieul++ = 0;
    value = !final ? vowel : final == 8 && rieul ? rieul : forms;
    if (!strcmp(value, key))
        return TTF_OK;
    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, source_length);
    {
        ttf_status_t status = ttf_trace_resource(e, "particle_form", renderer->language_tag, key,
                                                 value, "particle-agreement", SIZE_MAX);
        if (status != TTF_OK)
            return status;
    }
    normalized->length = 0;
    if (normalized->data)
        normalized->data[0] = 0;
    return ttf_buffer_puts(normalized, value);
}

/* Declared bracket pairs mark an aside. The policy decides whether its content
   is read, left out of the reading, or kept exactly as written. */
static uint32_t bracket_closer(const ttf_engine_t *e, const char *text, size_t length) {
    char key[8];
    uint32_t cp, closer;
    size_t width;
    const char *value;
    if (ttf_utf8_decode(text, length, &cp, &width) != 1 || width >= sizeof(key))
        return 0;
    memcpy(key, text, width);
    key[width] = 0;
    value = ttf_resource_get(e, "bracket_pair", "*", key, NULL);
    if (!value || ttf_utf8_decode(value, strlen(value), &closer, &width) != 1)
        return 0;
    return closer;
}

static size_t bracket_region(const ttf_engine_t *e, const char *text, size_t length,
                             const ttf_token_t *tokens, size_t count, size_t index) {
    uint32_t closer = bracket_closer(e, text + tokens[index].begin, length - tokens[index].begin);
    size_t i, depth = 1;
    if (!closer || tokens[index].end != tokens[index].begin + 1)
        return 0;
    for (i = index + 1; i < count; ++i) {
        uint32_t cp;
        size_t width;
        if (ttf_utf8_decode(text + tokens[i].begin, length - tokens[i].begin, &cp, &width) != 1)
            continue;
        if (cp == closer && !--depth)
            return i;
        if (bracket_closer(e, text + tokens[i].begin, length - tokens[i].begin) == closer)
            ++depth;
    }
    return 0;
}

ttf_status_t ttf_process_unit(const ttf_engine_t *e, const char *text, size_t length, size_t offset,
                              ttf_result_t *out) {
    ttf_context_memo_t memo = {0};
    ttf_engine_t context_view = *e;
    unsigned context_invalidations = 0;
    ttf_token_t *tokens = NULL;
    context_scope_t *scopes = NULL;
    ttf_protected_region_t *regions = NULL;
    size_t region_count = 0, region_index = 0;
    size_t count = 0, i = 0, capacity = 0;
    int removed_before = 0;
    ttf_buffer_t output = {0};
    ttf_status_t st;
    context_view.context_memo = &memo;
    memset(out, 0, sizeof(*out));
    st = ttf_tokenize_protected(e, text, length, 1, &tokens, &count, &regions, &region_count);
    if (st != TTF_OK)
        return st;
    scopes = quote_scopes(text, length, tokens, count);
    if (count && !scopes) {
        free(tokens);
        free(regions);
        return TTF_ENOMEM;
    }
    while (i < count) {
        ttf_pack_input_t in;
        ttf_span_t span;
        ttf_semantic_t *sem = &span.semantic;
        size_t consumed = 0, j, end, matched = 0, semantic_limit = length, phrase_count = count;
        long classifier_priority = 0;
        const ttf_language_pack_t *source, *context, *renderer;
        const char *reading = NULL;
        const ttf_entry_t *rule = NULL;
        const char *phrase_hit = NULL;
        ttf_buffer_t normalized = {0};
        memset(&span, 0, sizeof(span));
        sem->struct_size = sizeof(*sem);
        sem->type = TTF_SEM_TEXT;
        sem->layer = TTF_LAYER_COMMON;
        if (e->bracket_policy != TTF_BRACKET_READ) {
            size_t close = bracket_region(e, text, length, tokens, count, i);
            if (close) {
                size_t end_offset = tokens[close].end;
                span.input_begin = tokens[i].begin + offset;
                span.input_end = end_offset + offset;
                sem->input_begin = span.input_begin;
                sem->input_end = span.input_end;
                snprintf(sem->rule, sizeof(sem->rule), "%s",
                         e->bracket_policy == TTF_BRACKET_SKIP ? "policy.bracket-skipped"
                                                               : "policy.bracket-preserved");
                snprintf(sem->unit, sizeof(sem->unit), "@bracket");
                tag(sem->source_lang, "und");
                tag(sem->context_lang, "und");
                tag(sem->render_lang, "und");
                tag(span.verbalizer, "identity");
                span.script = TTF_SCRIPT_PUNCTUATION;
                span.begin = output.length;
                ttf_trace_span_begin(e, span.input_begin, span.input_end);
                ttf_trace_stage(e, TTF_TRACE_POLICY, 0, end_offset - tokens[i].begin);
                st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, "und", "decision",
                                    "bracket", "", sem->rule, sem->rule, SIZE_MAX);
                if (st == TTF_OK && e->bracket_policy == TTF_BRACKET_PRESERVE)
                    st = ttf_buffer_append(&output, text + tokens[i].begin,
                                           end_offset - tokens[i].begin);
                span.end = output.length;
                ttf_trace_span_finish(e, span.begin, span.end);
                if (st == TTF_OK)
                    st = add_span(out, &span, &capacity);
                if (st != TTF_OK)
                    goto fail;
                removed_before = e->bracket_policy == TTF_BRACKET_SKIP;
                while (region_index < region_count && regions[region_index].begin < end_offset)
                    ++region_index;
                i = close + 1;
                continue;
            }
        }
        if (region_index < region_count && regions[region_index].begin == tokens[i].begin) {
            const ttf_protected_match_t *literal = &regions[region_index++].match;
            if (literal->control.kind != TTF_CONTROL_NONE) {
                ttf_pack_input_t control_input = {text, length, tokens, count, i, e};
                if (scopes[i].end) {
                    control_input.tokens = tokens + scopes[i].begin;
                    control_input.token_count = scopes[i].end - scopes[i].begin;
                    control_input.index -= scopes[i].begin;
                    snprintf(span.context_trace, sizeof(span.context_trace), "quotation");
                }
                span.begin = output.length;
                st = render_control(e, &control_input, &literal->control, offset, &span, &output);
                span.end = output.length;
                ttf_trace_span_finish(e, span.begin, span.end);
                if (st == TTF_OK)
                    st = add_span(out, &span, &capacity);
                if (st != TTF_OK)
                    goto fail;
                removed_before = 0;
                ++i;
                continue;
            }
            sem->input_begin = span.input_begin = tokens[i].begin + offset;
            sem->input_end = span.input_end = literal->end + offset;
            sem->layer = literal->layer;
            snprintf(sem->unit, sizeof(sem->unit), "%s", literal->kind);
            snprintf(sem->rule, sizeof(sem->rule), "%s", literal->rule);
            tag(sem->source_lang, "und");
            tag(sem->context_lang, "und");
            tag(sem->render_lang, "und");
            tag(span.verbalizer, "identity");
            snprintf(span.context_trace, sizeof(span.context_trace), "protected:excluded");
            span.script = TTF_SCRIPT_SYMBOL;
            span.begin = output.length;
            st = ttf_buffer_append(&output, text + tokens[i].begin, literal->end - tokens[i].begin);
            span.end = output.length;
            if (st == TTF_OK)
                st = add_span(out, &span, &capacity);
            if (st != TTF_OK)
                goto fail;
            removed_before = 0;
            ++i;
            continue;
        }
        in.text = text;
        in.length = length;
        in.tokens = tokens;
        in.token_count = count;
        in.index = i;
        in.engine = e;
        /* Only context callbacks receive this borrowed request view. Other pack
           callbacks retain their original engine and the public ABI is unchanged. */
        if (scopes[i].end) {
            ttf_pack_input_t scoped = in;
            scoped.engine = &context_view;
            scoped.tokens = tokens + scopes[i].begin;
            scoped.token_count = scopes[i].end - scopes[i].begin;
            scoped.index -= scopes[i].begin;
            snprintf(span.context_trace, sizeof(span.context_trace), "quotation");
            context = context_pack(e, &scoped, span.context_trace);
        } else {
            ttf_pack_input_t scoped = in;
            scoped.engine = &context_view;
            context = context_pack(e, &scoped, span.context_trace);
        }
        if (memo.status != TTF_OK) {
            st = memo.status;
            goto fail;
        }
        source = script_pack(e, tokens[i].script);
        if (!context) {
            st = TTF_EINVAL;
            goto fail;
        }
        if (tokens[i].script != TTF_SCRIPT_WHITESPACE) {
            ttf_pack_input_t classification = in;
            if (region_index < region_count) {
                classification.length = regions[region_index].begin;
                classification.token_count = i;
                while (classification.token_count < count &&
                       tokens[classification.token_count].begin < classification.length)
                    ++classification.token_count;
            }
            semantic_limit = classification.length;
            phrase_count = classification.token_count;
            consumed = ttf_classify_universal(&classification, sem);
            for (j = 0; j < e->pack_count; j++)
                if (e->packs[j]->classify && strcmp(sem->unit, "@math")) {
                    ttf_semantic_t candidate;
                    size_t n;
                    long candidate_priority = 0;
                    memset(&candidate, 0, sizeof(candidate));
                    candidate.struct_size = sizeof(candidate);
                    candidate.layer = TTF_LAYER_SYSTEM;
                    n = e->packs[j]->classify(&classification, &candidate);
                    if (n > classification.token_count - i ||
                        (n && candidate.input_end > classification.length)) {
                        st = TTF_ERESOURCE;
                        goto fail;
                    }
                    if (n) {
                        const char *priority =
                            ttf_resource_get(e, "classifier_priority", e->packs[j]->language_tag,
                                             candidate.rule, NULL);
                        if (priority) {
                            char *end_priority;
                            candidate_priority = strtol(priority, &end_priority, 10);
                            if (!*priority || *end_priority || candidate_priority < -1000 ||
                                candidate_priority > 1000) {
                                st = TTF_ERESOURCE;
                                goto fail;
                            }
                        }
                    }
                    /* An explicit pack identifier cue is more specific than
                       generic numeric syntax over the identical byte extent. */
                    int labeled_identifier =
                        n && n == consumed && candidate.input_end == sem->input_end &&
                        candidate.type == TTF_SEM_DIGIT_SEQUENCE &&
                        (sem->type == TTF_SEM_CARDINAL || sem->type == TTF_SEM_NUMBER ||
                         sem->type == TTF_SEM_PHONE || sem->type == TTF_SEM_RANGE ||
                         sem->type == TTF_SEM_DATE || sem->type == TTF_SEM_TEXT);
                    /* A pack may establish a calendar meaning using explicit
                       context. A bare generic cardinal has no such evidence. */
                    int contextual_date =
                        n && n == consumed && candidate.input_end == sem->input_end &&
                        candidate.type == TTF_SEM_DATE &&
                        (sem->type == TTF_SEM_CARDINAL || sem->type == TTF_SEM_NUMBER);
                    if (n > consumed ||
                        (n && n == consumed &&
                         (candidate.input_end > sem->input_end ||
                          (candidate.input_end == sem->input_end &&
                           candidate_priority > classifier_priority))) ||
                        (n && n == consumed && sem->type == TTF_SEM_WORD) || labeled_identifier ||
                        contextual_date) {
                        *sem = candidate;
                        consumed = n;
                        classifier_priority = candidate_priority;
                    }
                }
            rule = ttf_rule_match(e, context->language_tag, text + tokens[i].begin,
                                  classification.length - tokens[i].begin, &matched);
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
        if (!rule && consumed == 1 && source && source != context &&
            e->config.render_policy == TTF_PHONETIC_LOCALIZE &&
            (sem->type == TTF_SEM_WORD || sem->type == TTF_SEM_INITIALISM ||
             sem->type == TTF_SEM_ACRONYM)) {
            size_t phrase_consumed = 0;
            char phrase_value[TTF_IR_VALUE_SIZE];
            ttf_resource_layer_t phrase_layer = TTF_LAYER_COMMON;
            phrase_hit =
                ttf_phrase_match(e, source->language_tag, context->language_tag, text, tokens,
                                 phrase_count, i, &phrase_consumed, phrase_value, &phrase_layer);
            if (phrase_hit) {
                sem->type = TTF_SEM_WORD;
                sem->lexical_class = TTF_LEX_WORD;
                sem->layer = phrase_layer;
                sem->input_end = tokens[i + phrase_consumed - 1].end;
                memcpy(sem->value, phrase_value, strlen(phrase_value) + 1);
                snprintf(sem->rule, sizeof(sem->rule), "lexicon.phrase");
                consumed = phrase_consumed;
                snprintf(span.lexicon, sizeof(span.lexicon), "lexicon:%s",
                         ttf_layer_name(phrase_layer));
                span.pronunciation_layer = phrase_layer;
            }
        }
        if (consumed > count - i) {
            st = TTF_ERESOURCE;
            goto fail;
        }
        end = sem->input_end ? sem->input_end : tokens[i + consumed - 1].end;
        if (end <= tokens[i].begin || end > tokens[i + consumed - 1].end || end > semantic_limit) {
            st = TTF_ERESOURCE;
            goto fail;
        }
        if (!source)
            for (j = i; j < i + consumed; j++) {
                source = script_pack(e, tokens[j].script);
                if (source)
                    break;
            }
        /* An address or an expression's own letters are not language evidence. */
        if (!rule && ((e->frontend_options.output_mode == TTF_OUTPUT_SPEECH &&
                       (sem->type == TTF_SEM_EMAIL || sem->type == TTF_SEM_URL ||
                        !strcmp(sem->unit, "@handle"))) ||
                      (sem->type == TTF_SEM_SYMBOL && !strcmp(sem->unit, "@expression")))) {
            ttf_pack_input_t electronic_input = in;
            if (scopes[i].end && consumed <= scopes[i].end - i) {
                electronic_input.tokens = tokens + scopes[i].begin;
                electronic_input.token_count = scopes[i].end - scopes[i].begin;
                electronic_input.index -= scopes[i].begin;
            }
            context = electronic_context(e, &electronic_input, consumed, span.context_trace);
            if (!context) {
                st = TTF_ENOMEM;
                goto fail;
            }
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
        ttf_trace_span_begin(e, span.input_begin, span.input_end);
        if (rule)
            st = ttf_buffer_puts(&normalized, rule->value);
        else if (phrase_hit) {
            tag(span.verbalizer, source->language_tag);
            ttf_trace_stage(e, TTF_TRACE_LEXICON, 0, strlen(sem->value));
            st = ttf_trace_foreign_lexicon(e, source->language_tag, context->language_tag,
                                           sem->value, strlen(sem->value), "multiword-lexicon");
            if (st == TTF_OK)
                st = ttf_buffer_puts(&normalized, phrase_hit);
        } else if (source && source != context && foreign_type(sem->type) &&
                   e->config.render_policy == TTF_PHONETIC_LOCALIZE) {
            ttf_buffer_t spoken = {0};
            const char *original_hit = NULL, *selected_text = NULL;
            size_t selected_length = 0;
            tag(span.verbalizer, source->language_tag);
            /* An abbreviation may share its final period with sentence
               punctuation. Keep punctuation as its own output span while
               still honoring the user's complete dotted spelling. */
            st = TTF_OK;
            if ((sem->type == TTF_SEM_INITIALISM || sem->type == TTF_SEM_ABBREVIATION) &&
                end < length && text[end] == '.')
                st = pronunciation_candidate(e, source->language_tag, context->language_tag,
                                             text + tokens[i].begin, end + 1 - tokens[i].begin,
                                             &original_hit, &span.pronunciation_layer,
                                             &selected_text, &selected_length);
            if (st == TTF_OK)
                st = pronunciation_candidate(e, source->language_tag, context->language_tag,
                                             text + tokens[i].begin, end - tokens[i].begin,
                                             &original_hit, &span.pronunciation_layer,
                                             &selected_text, &selected_length);
            if (st == TTF_OK && (!original_hit || span.pronunciation_layer < TTF_LAYER_USER)) {
                ttf_status_t verbalized = source->verbalize(e, sem, text + tokens[i].begin,
                                                            end - tokens[i].begin, &spoken);
                if (verbalized == TTF_OK)
                    st = pronunciation_candidate(e, source->language_tag, context->language_tag,
                                                 spoken.data ? spoken.data : "", spoken.length,
                                                 &original_hit, &span.pronunciation_layer,
                                                 &selected_text, &selected_length);
                else if (!original_hit || verbalized == TTF_ENOMEM || verbalized == TTF_ELIMIT)
                    st = verbalized;
            }
            if (st == TTF_OK && original_hit) {
                snprintf(span.lexicon, sizeof(span.lexicon), "lexicon:%s",
                         ttf_layer_name(span.pronunciation_layer));
                ttf_trace_stage(e, TTF_TRACE_LEXICON, 0, selected_length);
                st = ttf_trace_foreign_lexicon(
                    e, source->language_tag, context->language_tag, selected_text, selected_length,
                    selected_text == spoken.data ? "verbalized-alias" : "surface-alias");
                if (st == TTF_OK)
                    st = ttf_buffer_puts(&normalized, original_hit);
            } else if (st == TTF_OK) {
                st = ttf_foreign_render(e, source, context, spoken.data ? spoken.data : "",
                                        spoken.length, sem->lexical_class, &normalized,
                                        span.lexicon, &span.pronunciation_layer);
            }
            ttf_buffer_free(&spoken);
            if (st == TTF_OK && !strcmp(span.lexicon, "preserved:no-renderer"))
                tag(sem->render_lang, source->language_tag);
        } else if ((sem->type == TTF_SEM_EMAIL || sem->type == TTF_SEM_URL ||
                    !strcmp(sem->unit, "@handle")) &&
                   e->frontend_options.output_mode == TTF_OUTPUT_SPEECH)
            st = ttf_electronic_render(e, source, renderer, text + tokens[i].begin,
                                       end - tokens[i].begin, &normalized);
        else if ((sem->type == TTF_SEM_EMAIL || sem->type == TTF_SEM_URL ||
                  sem->type == TTF_SEM_VERSION || sem->type == TTF_SEM_IP_ADDRESS ||
                  !strcmp(sem->unit, "@number-sign") || !strcmp(sem->unit, "@handle")) &&
                 e->frontend_options.output_mode == TTF_OUTPUT_TEXT)
            st = ttf_buffer_append(&normalized, text + tokens[i].begin, end - tokens[i].begin);
        else if ((sem->type == TTF_SEM_SYMBOL || sem->type == TTF_SEM_TEXT ||
                  sem->type == TTF_SEM_WORD) &&
                 !sem->unit[0] &&
                 (reading = ttf_speech_range_mark(
                      e, renderer->language_tag, output.data, output.length, text + tokens[i].begin,
                      end - tokens[i].begin, text + end, length - end))) {
            char mark[8];
            size_t mark_length = end - tokens[i].begin;
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, mark_length);
            if (mark_length >= sizeof(mark))
                mark_length = sizeof(mark) - 1;
            memcpy(mark, text + tokens[i].begin, mark_length);
            mark[mark_length] = 0;
            st = ttf_trace_resource(e, "speech_range_mark", renderer->language_tag, mark, reading,
                                    "range-mark-reading", SIZE_MAX);
            if (st == TTF_OK)
                st = ttf_buffer_puts(&normalized, reading);
        } else if (sem->type == TTF_SEM_SYMBOL && !sem->unit[0] &&
                   ttf_speech_lengthening_span(e, renderer->language_tag, output.data,
                                               output.length, text + tokens[i].begin,
                                               end - tokens[i].begin, text + end, length - end)) {
            /* Text keeps the written mark; speech drops it as prosody. */
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, end - tokens[i].begin);
            st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, renderer->language_tag,
                                "decision", "lengthening-mark", "", "", "word-final-lengthening",
                                SIZE_MAX);
            if (st == TTF_OK && e->frontend_options.output_mode == TTF_OUTPUT_TEXT)
                st = ttf_buffer_append(&normalized, text + tokens[i].begin, end - tokens[i].begin);
        } else
            st = renderer->verbalize(e, sem, text + tokens[i].begin, end - tokens[i].begin,
                                     &normalized);
        if (st == TTF_OK && (!strcmp(sem->unit, "@math") || !strcmp(sem->unit, "@number-sign"))) {
            ttf_buffer_t spaced = {0};
            size_t begin = tokens[i].begin;
            if (begin && text[begin - 1] != ' ' && text[begin - 1] != '\t' &&
                text[begin - 1] != '\n')
                st = ttf_buffer_puts(&spaced, " ");
            if (st == TTF_OK)
                st = ttf_buffer_append(&spaced, normalized.data, normalized.length);
            /* Punctuation after the reading needs no separating space. */
            if (st == TTF_OK && end < length &&
                ttf_speech_spoken_after(e, renderer->language_tag, text + end, length - end) &&
                !following_particle(e, renderer, text + end, length - end))
                st = ttf_buffer_puts(&spaced, " ");
            ttf_buffer_free(&normalized);
            normalized = spaced;
        }
        if (st == TTF_OK && e->frontend_options.output_mode == TTF_OUTPUT_SPEECH && !rule &&
            strncmp(span.lexicon, "lexicon:", 8) &&
            strcmp(span.lexicon, "unknown-foreign-preserved")) {
            if (sem->type == TTF_SEM_TEXT && strcmp(sem->rule, "identity")) {
                ttf_trace_stage(e, TTF_TRACE_POLICY, 0, normalized.length);
                st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0,
                                    renderer->language_tag, "decision", "semantic-guard", "",
                                    sem->value, "ambiguous-semantic-preserved", SIZE_MAX);
            } else if (!strcmp(sem->unit, "@unreadable-cluster")) {
                ttf_trace_stage(e, TTF_TRACE_POLICY, 0, normalized.length);
                const char *cluster = normalized.data ? ttf_resource_get(e, "speech_character",
                                                                         renderer->language_tag,
                                                                         normalized.data, NULL)
                                                      : NULL;
                if (!cluster && e->decorative_policy == TTF_DECORATIVE_READ && normalized.data)
                    cluster = ttf_resource_get(e, "speech_decorative", renderer->language_tag,
                                               normalized.data, NULL);
                if (cluster) {
                    st = ttf_trace_resource(e, "speech_character", renderer->language_tag,
                                            normalized.data, normalized.data,
                                            "final-cluster-mapped", SIZE_MAX);
                    ttf_buffer_free(&normalized);
                    if (st == TTF_OK)
                        st = ttf_buffer_puts(&normalized, cluster);
                } else {
                    st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0,
                                        renderer->language_tag, "decision", "unreadable-cluster",
                                        "", normalized.data,
                                        e->frontend_options.unreadable_policy == TTF_UNREADABLE_DROP
                                            ? "final-unreadable-dropped"
                                            : "final-unreadable-preserved",
                                        SIZE_MAX);
                    if (e->frontend_options.unreadable_policy == TTF_UNREADABLE_DROP) {
                        /* Removing a cluster between two words must not join them. */
                        int gap = ttf_speech_spoken_before(e, renderer->language_tag, output.data,
                                                           output.length) &&
                                  ttf_speech_spoken_after(e, renderer->language_tag, text + end,
                                                          length - end);
                        normalized.length = 0;
                        if (normalized.data)
                            normalized.data[0] = 0;
                        if (gap)
                            st = ttf_buffer_puts(&normalized, " ");
                    }
                }
            } else
                st = ttf_speech_output(e, renderer, &normalized, output.data, output.length,
                                       text + end, length - end);
        }
        /* Only a particle written directly against the previous reading, with no
           space, is part of that phrase; a separate word keeps its own text. */
        if (st == TTF_OK && output.length && tokens[i].begin &&
            !(text[tokens[i].begin - 1] == ' ' || text[tokens[i].begin - 1] == '\t' ||
              text[tokens[i].begin - 1] == '\n'))
            st = particle_agreement(e, renderer, text + tokens[i].begin, end - tokens[i].begin,
                                    &output, &normalized);
        if (st == TTF_OK && e->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
            /* A removed span leaves its neighboring spaces behind. Keep one. */
            size_t b = 0;
            while (b < normalized.length &&
                   (normalized.data[b] == ' ' || normalized.data[b] == '\t'))
                ++b;
            if (!normalized.length)
                removed_before = end > tokens[i].begin;
            else if (b == normalized.length && removed_before &&
                     (!output.length || output.data[output.length - 1] == ' ' ||
                      output.data[output.length - 1] == '\t')) {
                normalized.length = 0;
                normalized.data[0] = 0;
            } else
                removed_before = 0;
        }
        if (st == TTF_OK)
            st = ttf_trace_status(e);
        if (st != TTF_OK) {
            ttf_buffer_free(&normalized);
            goto fail;
        }
        span.begin = output.length;
        st = ttf_buffer_append(&output, normalized.data, normalized.length);
        span.end = output.length;
        ttf_trace_span_finish(e, span.begin, span.end);
        ttf_buffer_free(&normalized);
        if (st != TTF_OK)
            goto fail;
        st = add_span(out, &span, &capacity);
        if (st != TTF_OK)
            goto fail;
        j = i + consumed - 1;
        if (end < tokens[j].end) {
            tokens[j].begin = end;
            /* A partial semantic match changes the evidence seen by later
               context callbacks, even though the token array address is stable. */
            if (context_view.context_memo) {
                ttf_context_memo_clear(&memo);
                /* Repeated partial matches would rebuild every scope for each
                   suffix. Bound this extra work to two memo generations, then
                   use the same uncached pack policy for the rest of this unit. */
                if (++context_invalidations >= 2)
                    context_view.context_memo = NULL;
            }
            i = j;
        } else
            i = j + 1;
    }
    st = ttf_buffer_append(&output, "", 0);
    if (st != TTF_OK)
        goto fail;
    out->text = output.data;
    out->length = output.length;
    ttf_context_memo_clear(&memo);
    free(tokens);
    free(scopes);
    free(regions);
    return TTF_OK;
fail:
    ttf_context_memo_clear(&memo);
    free(tokens);
    free(scopes);
    free(regions);
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
