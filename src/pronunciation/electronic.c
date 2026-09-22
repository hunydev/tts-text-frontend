#include "../core/internal.h"
#include "trace.h"

ttf_status_t ttf_trace_foreign_lexicon(const ttf_engine_t *, const char *, const char *,
                                       const char *, size_t, const char *);

static int alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int alnum(char c) {
    return alpha(c) || (c >= '0' && c <= '9');
}

static ttf_status_t spell(const ttf_engine_t *e, const ttf_language_pack_t *target,
                          const char *text, size_t n, ttf_buffer_t *out) {
    ttf_semantic_t sem = {0};
    if (n >= sizeof(sem.value))
        return TTF_ELIMIT;
    sem.struct_size = sizeof(sem);
    sem.type = TTF_SEM_CUSTOM;
    snprintf(sem.unit, sizeof(sem.unit), "@alphabet");
    memcpy(sem.value, text, n);
    return target->verbalize(e, &sem, sem.value, n, out);
}

/* An alphabetic address label is read as a word only with resource evidence:
   an `electronic_word` reading, then a pronunciation lexicon entry of the
   source language. Every other label is spelled, because account names and
   unknown hosts must survive exactly. Digits are always spelled. */
static ttf_status_t chunk(const ttf_engine_t *e, const ttf_language_pack_t *source,
                          const ttf_language_pack_t *target, const char *text, size_t n,
                          size_t part, ttf_buffer_t *out) {
    char key[64];
    const char *hit = NULL;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    size_t i;
    int word = n < sizeof(key);
    ttf_status_t st;
    for (i = 0; word && i < n; ++i) {
        if (!alpha(text[i]))
            word = 0;
        else
            key[i] = (char)(text[i] >= 'A' && text[i] <= 'Z' ? text[i] - 'A' + 'a' : text[i]);
    }
    if (word) {
        key[n] = 0;
        ttf_trace_stage(e, TTF_TRACE_LEXICON, part, part + n);
        hit = ttf_resource_get(e, "electronic_word", target->language_tag, key, NULL);
        if (hit) {
            st = ttf_trace_resource(e, "electronic_word", target->language_tag, key, key,
                                    "address-word", SIZE_MAX);
            return st == TTF_OK ? ttf_buffer_puts(out, hit) : st;
        }
        if (source && source != target) {
            st = ttf_foreign_lookup(e, source->language_tag, target->language_tag, key, n, &hit,
                                    &layer);
            if (st != TTF_OK)
                return st;
            if (hit) {
                st = ttf_trace_foreign_lexicon(e, source->language_tag, target->language_tag, key,
                                               n, "address-lexicon");
                return st == TTF_OK ? ttf_buffer_puts(out, hit) : st;
            }
        }
    }
    ttf_trace_stage(e, TTF_TRACE_POLICY, part, part + n);
    return spell(e, target, text, n, out);
}

ttf_status_t ttf_electronic_render(const ttf_engine_t *e, const ttf_language_pack_t *source,
                                   const ttf_language_pack_t *target, const char *text, size_t n,
                                   ttf_buffer_t *out) {
    size_t at = 0;
    ttf_status_t st = TTF_OK;
    while (at < n && st == TTF_OK) {
        size_t end = at;
        if (at && (st = ttf_buffer_puts(out, " ")) != TTF_OK)
            break;
        if (alnum(text[at])) {
            while (end < n && alnum(text[end]))
                ++end;
            st = chunk(e, source, target, text + at, end - at, at, out);
        } else {
            char key[2] = {text[at], 0};
            const char *label;
            end = at + 1;
            label = ttf_resource_get(e, "speech_symbol", target->language_tag, key, NULL);
            if ((unsigned char)text[at] <= 32 || (unsigned char)text[at] >= 127)
                return TTF_EINVAL;
            if (!label)
                return TTF_ERESOURCE;
            ttf_trace_stage(e, TTF_TRACE_POLICY, at, end);
            st = ttf_trace_resource(e, "speech_symbol", target->language_tag, key, key,
                                    "address-spelling", SIZE_MAX);
            if (st == TTF_OK)
                st = ttf_buffer_puts(out, label);
        }
        at = end;
    }
    return st;
}
