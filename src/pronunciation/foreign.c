#include "../core/internal.h"
ttf_status_t ttf_foreign_lookup(const ttf_engine_t *e, const char *source,
                               const char *target, const char *text, size_t n,
                               const char **hit, ttf_resource_layer_t *layer) {
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
ttf_status_t ttf_foreign_render(const ttf_engine_t *e, const ttf_language_pack_t *source,
                                const ttf_language_pack_t *target, const char *text, size_t n,
                                ttf_lexical_class_t lexical, ttf_buffer_t *out, char trace[64],
                                ttf_resource_layer_t *layer) {
    const char *hit;
    ttf_status_t st;
    ttf_pronunciation_t pron;
    size_t before = out->length;
    if (!source || !target)
        return TTF_EINVAL;
    st = ttf_foreign_lookup(e, source->language_tag, target->language_tag, text, n, &hit, layer);
    if (st != TTF_OK)
        return st;
    if (hit) {
        snprintf(trace, 64, "lexicon:%s", ttf_layer_name(*layer));
        return ttf_buffer_puts(out, hit);
    }
    st = TTF_EINVAL;
    if (!target->render_phones && !target->render_graphemes) {
        snprintf(trace, 64, "preserved:no-renderer");
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
                st = ttf_foreign_render(e, source, target, text + at, end - at, TTF_LEX_WORD, out,
                                        part_trace, &part_layer);
                if (part_layer > *layer)
                    *layer = part_layer;
                if (strstr(part_trace, "lexicon"))
                    paths |= 1;
                else if (strstr(part_trace, "dictionary"))
                    paths |= 2;
                else if (strstr(part_trace, "grapheme"))
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
    memset(&pron, 0, sizeof(pron));
    if (source->pronounce && target->render_phones) {
        st = source->pronounce(e, text, n, lexical, &pron);
        if (st == TTF_OK)
            st = target->render_phones(e, &pron, out);
        if (st == TTF_OK && out->length > before) {
            snprintf(trace, 64, "%.63s", pron.provenance);
            return TTF_OK;
        }
        out->length = before;
        if (out->data)
            out->data[before] = 0;
        if (st == TTF_ENOMEM || st == TTF_ELIMIT)
            return st;
    }
    if (target->render_graphemes) {
        st = target->render_graphemes(e, text, n, out);
        if (st == TTF_OK && out->length > before) {
            snprintf(trace, 64, "grapheme-fallback");
            return TTF_OK;
        }
    }
    return st == TTF_OK ? TTF_EINVAL : st;
}
