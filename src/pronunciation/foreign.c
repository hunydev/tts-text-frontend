#include "../core/internal.h"
ttf_status_t ttf_foreign_render(const ttf_engine_t *e, const ttf_language_pack_t *source,
                                const ttf_language_pack_t *target, const char *text, size_t n,
                                ttf_lexical_class_t lexical, ttf_buffer_t *out, char trace[64],
                                ttf_resource_layer_t *layer) {
    char *key;
    size_t a, i;
    const char *hit;
    ttf_status_t st = TTF_EINVAL;
    ttf_pronunciation_t pron;
    size_t before = out->length;
    if (!source || !target)
        return TTF_EINVAL;
    a = strlen(source->language_tag);
    if (n > SIZE_MAX - a - 2)
        return TTF_ELIMIT;
    key = (char *)malloc(a + n + 2);
    if (!key)
        return TTF_ENOMEM;
    memcpy(key, source->language_tag, a);
    key[a] = ':';
    memcpy(key + a + 1, text, n);
    key[a + n + 1] = 0;
    for (i = a + 1; i < a + n + 1; i++)
        if (key[i] >= 'A' && key[i] <= 'Z')
            key[i] += 'a' - 'A';
    hit = ttf_resource_get(e, "lexicon", target->language_tag, key, layer);
    free(key);
    if (hit) {
        snprintf(trace, 64, "lexicon:%s", ttf_layer_name(*layer));
        return ttf_buffer_puts(out, hit);
    }
    if (!target->render_phones && !target->render_graphemes) {
        snprintf(trace, 64, "preserved:no-renderer");
        return ttf_buffer_append(out, text, n);
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
