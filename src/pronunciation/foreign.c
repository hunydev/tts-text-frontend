#include "../core/internal.h"
#include "trace.h"

/* ABI v1 keeps provenance in the existing fixed-size field. The reserved
   envelope reports SOURCE phone resources, never the target phone mapper. */
static ttf_status_t pronunciation_provenance(const ttf_pronunciation_t *pron, char trace[64],
                                             ttf_resource_layer_t *layer) {
    static const char reserved[] = "ttf-pron-";
    static const char version[] = "ttf-pron-v1:";
    const char *path = pron->provenance;
    const char *separator;
    size_t i, length;
    int found = 0;
    *layer = TTF_LAYER_COMMON;
    if (!memchr(pron->provenance, 0, sizeof(pron->provenance)) ||
        !memchr(pron->phones, 0, sizeof(pron->phones)) ||
        !memchr(pron->alphabet, 0, sizeof(pron->alphabet)))
        return TTF_ERESOURCE;
    if (!strncmp(path, reserved, sizeof(reserved) - 1)) {
        if (strncmp(path, version, sizeof(version) - 1))
            return TTF_ERESOURCE;
        path += sizeof(version) - 1;
        separator = strchr(path, ':');
        if (!separator)
            return TTF_ERESOURCE;
        length = (size_t)(separator - path);
        for (i = TTF_LAYER_COMMON; i <= TTF_LAYER_USER; ++i) {
            const char *name = ttf_layer_name((ttf_resource_layer_t)i);
            if (strlen(name) == length && !memcmp(name, path, length)) {
                *layer = (ttf_resource_layer_t)i;
                found = 1;
                break;
            }
        }
        if (!found)
            return TTF_ERESOURCE;
        path = separator + 1;
        if (!*path)
            return TTF_ERESOURCE;
        /* Paths are stable identifiers, not another metadata serialization. */
        for (i = 0; path[i]; ++i)
            if (!((path[i] >= 'a' && path[i] <= 'z') || (path[i] >= '0' && path[i] <= '9') ||
                  (i && (path[i] == '-' || path[i] == '_' || path[i] == '.'))))
                return TTF_ERESOURCE;
    }
    (void)snprintf(trace, 64, "%s", path);
    return TTF_OK;
}
ttf_status_t ttf_foreign_lookup(const ttf_engine_t *e, const char *source, const char *target,
                                const char *text, size_t n, const char **hit,
                                ttf_resource_layer_t *layer) {
    char *key;
    size_t a, i;
    if (!e || !source || !target || !hit || !layer || (!text && n))
        return TTF_EINVAL;
    *hit = NULL;
    a = strlen(source);
    if (n > SIZE_MAX - a - 2)
        return TTF_ELIMIT;
    key = (char *)malloc(a + n + 2);
    if (!key)
        return TTF_ENOMEM;
    memcpy(key, source, a);
    key[a] = ':';
    if (n)
        memcpy(key + a + 1, text, n);
    key[a + n + 1] = 0;
    for (i = a + 1; i < a + n + 1; i++)
        if (key[i] >= 'A' && key[i] <= 'Z')
            key[i] += 'a' - 'A';
    *hit = ttf_resource_get(e, "lexicon", target, key, layer);
    free(key);
    return TTF_OK;
}
ttf_status_t ttf_trace_foreign_lexicon(const ttf_engine_t *e, const char *source,
                                       const char *target, const char *text, size_t n,
                                       const char *detail) {
    ttf_status_t status;
    char *word, *key, *canonical = NULL;
    size_t prefix = strlen(source), i;
    if (!e->trace_state)
        return TTF_OK;
    if (n > SIZE_MAX - prefix - 2)
        return TTF_ELIMIT;
    word = ttf_strndup(text, n);
    key = malloc(prefix + n + 2);
    if (!word || !key) {
        free(word);
        free(key);
        return TTF_ENOMEM;
    }
    memcpy(key, source, prefix);
    key[prefix] = ':';
    memcpy(key + prefix + 1, word, n + 1);
    for (i = prefix + 1; i < prefix + n + 1; ++i)
        if (key[i] >= 'A' && key[i] <= 'Z')
            key[i] += 'a' - 'A';
    status = ttf_lexicon_canonical_key(key, &canonical);
    if (status == TTF_OK)
        status = ttf_trace_resource(e, "lexicon", target, canonical, word, detail, SIZE_MAX);
    free(canonical);
    free(key);
    free(word);
    return status;
}
static ttf_status_t render_at(const ttf_engine_t *e, const ttf_language_pack_t *source,
                              const ttf_language_pack_t *target, const char *text, size_t n,
                              ttf_lexical_class_t lexical, ttf_buffer_t *out, char trace[64],
                              ttf_resource_layer_t *layer, size_t part_begin) {
    const char *hit;
    ttf_status_t st;
    ttf_pronunciation_t pron;
    size_t before = out->length, attempt;
    char *trace_text = NULL;
    if (!source || !target)
        return TTF_EINVAL;
    ttf_trace_stage(e, TTF_TRACE_LEXICON, part_begin, part_begin + n);
    st = ttf_foreign_lookup(e, source->language_tag, target->language_tag, text, n, &hit, layer);
    if (st != TTF_OK)
        return st;
    if (hit) {
        snprintf(trace, 64, "lexicon:%s", ttf_layer_name(*layer));
        st = ttf_trace_foreign_lexicon(e, source->language_tag, target->language_tag, text, n,
                                       "whole-part-lexicon");
        return st == TTF_OK ? ttf_buffer_puts(out, hit) : st;
    }
    *layer = TTF_LAYER_COMMON;
    st = TTF_EINVAL;
    if (!target->render_phones && !target->render_graphemes) {
        snprintf(trace, 64, "preserved:no-renderer");
        ttf_trace_stage(e, TTF_TRACE_POLICY, part_begin, part_begin + n);
        (void)ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, target->language_tag,
                             "decision", "preserve", "", "", "target-has-no-renderer", SIZE_MAX);
        return ttf_buffer_append(out, text, n);
    }
    /* A source verbalizer can produce several words (e.g. an ordinal). Resolve
       the complete phrase above first, then preserve its whitespace and apply
       the same layered pipeline to individual words. */
    if (memchr(text, ' ', n) || memchr(text, '\t', n) || memchr(text, '\n', n)) {
        unsigned paths = 0;
        size_t at = 0;
        *layer = TTF_LAYER_COMMON;
        while (at < n) {
            size_t end = at;
            int whitespace = text[at] == ' ' || text[at] == '\t' || text[at] == '\n';
            while (end < n &&
                   ((text[end] == ' ' || text[end] == '\t' || text[end] == '\n') == whitespace))
                end++;
            if (whitespace)
                st = ttf_buffer_append(out, text + at, end - at);
            else {
                char part_trace[64] = {0};
                ttf_resource_layer_t part_layer = TTF_LAYER_COMMON;
                st = render_at(e, source, target, text + at, end - at, TTF_LEX_WORD, out,
                               part_trace, &part_layer, part_begin + at);
                if (part_layer > *layer)
                    *layer = part_layer;
                if (strstr(part_trace, "lexicon"))
                    paths |= 1;
                else if (strstr(part_trace, "dictionary"))
                    paths |= 2;
                else if (strstr(part_trace, "grapheme-fallback"))
                    paths |= 8;
                else
                    paths |= 4;
            }
            if (st != TTF_OK)
                return st;
            at = end;
        }
        snprintf(trace, 64, "phrase:%s%s%s%s", paths & 1 ? "lexicon " : "",
                 paths & 2 ? "dictionary " : "", paths & 4 ? "g2p " : "",
                 paths & 8 ? "grapheme-fallback" : "");
        return TTF_OK;
    }
    if (e->trace_state) {
        trace_text = ttf_strndup(text, n);
        if (!trace_text)
            return TTF_ENOMEM;
    }
    memset(&pron, 0, sizeof(pron));
    attempt = ttf_trace_mark(e);
    if (source->pronounce && target->render_phones) {
        char source_trace[64];
        int target_attempted = 0;
        ttf_resource_layer_t source_layer = TTF_LAYER_COMMON;
        ttf_trace_stage(e, TTF_TRACE_SOURCE_PRONUNCIATION, part_begin, part_begin + n);
        st = source->pronounce(e, text, n, lexical, &pron);
        if (st == TTF_OK) {
            st = pronunciation_provenance(&pron, source_trace, &source_layer);
            if (st != TTF_OK) {
                free(trace_text);
                return st;
            }
            ttf_trace_stage(e, TTF_TRACE_SOURCE_PRONUNCIATION, part_begin, part_begin + n);
            (void)ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, source->language_tag,
                                 "pronunciation-ir", pron.alphabet, pron.phones, trace_text,
                                 source_trace, SIZE_MAX);
            ttf_trace_stage(e, TTF_TRACE_TARGET_PHONETIC, part_begin, part_begin + n);
            target_attempted = 1;
            st = target->render_phones(e, &pron, out);
        }
        if (st == TTF_OK && out->length > before) {
            snprintf(trace, 64, "%s", source_trace);
            *layer = source_layer;
            (void)ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, target->language_tag,
                                 "decision", "phone-renderer", "", trace_text,
                                 "phone-renderer-success", SIZE_MAX);
            free(trace_text);
            return ttf_trace_status(e);
        }
        out->length = before;
        if (out->data)
            out->data[before] = 0;
        ttf_trace_reject_since(e, attempt);
        ttf_trace_stage(e, TTF_TRACE_POLICY, part_begin, part_begin + n);
        (void)ttf_trace_emit(e, TTF_TRACE_REJECTED, TTF_LAYER_COMMON, 0,
                             target_attempted ? target->language_tag : source->language_tag,
                             "decision", "phone-pipeline", ttf_status_string(st), trace_text,
                             !target_attempted ? "source-pronunciation-failed"
                             : st == TTF_OK    ? "empty-phonetic-output"
                                               : "target-phonetic-failed",
                             SIZE_MAX);
        if (st == TTF_ENOMEM || st == TTF_ELIMIT) {
            free(trace_text);
            return st;
        }
    }
    if (target->render_graphemes) {
        ttf_trace_stage(e, TTF_TRACE_TARGET_GRAPHEME, part_begin, part_begin + n);
        st = target->render_graphemes(e, text, n, out);
        if (st == TTF_OK && out->length > before) {
            snprintf(trace, 64, "grapheme-fallback");
            (void)ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, target->language_tag,
                                 "decision", "grapheme-renderer", "", trace_text,
                                 "grapheme-renderer-success", SIZE_MAX);
            free(trace_text);
            return ttf_trace_status(e);
        }
    }
    free(trace_text);
    return st == TTF_OK ? TTF_EINVAL : st;
}
ttf_status_t ttf_foreign_render(const ttf_engine_t *e, const ttf_language_pack_t *source,
                                const ttf_language_pack_t *target, const char *text, size_t n,
                                ttf_lexical_class_t lexical, ttf_buffer_t *out, char trace[64],
                                ttf_resource_layer_t *layer) {
    return render_at(e, source, target, text, n, lexical, out, trace, layer, 0);
}
