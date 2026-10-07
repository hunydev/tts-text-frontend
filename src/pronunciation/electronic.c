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
    sem.struct_size = sizeof(sem);
    sem.type = TTF_SEM_CUSTOM;
    snprintf(sem.unit, sizeof(sem.unit), "@alphabet");
    if (n < sizeof(sem.value)) {
        memcpy(sem.value, text, n);
        return target->verbalize(e, &sem, sem.value, n, out);
    }
    snprintf(sem.secondary, sizeof(sem.secondary), "control-surface");
    return target->verbalize(e, &sem, text, n, out);
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
    /* Scrum.com, reasonradionetwork.com: in English a host label that is a
       capitalized word or a long lowercase run, with a vowel and no four
       consonants in a row, reads as a word (the English references; wetext
       too). Short lowercase labels (jdoe), capitals (BBC) and digits stay
       spelled (2026-10-07). */
    if (word && !strcmp(target->language_tag, "en-US") && n >= 4 && n < 40 &&
        ((text[0] >= 'A' && text[0] <= 'Z' && text[1] >= 'a' && text[1] <= 'z') || n >= 6)) {
        size_t run = 0, longest = 0, vowels = 0, upper = 0;
        for (i = 0; i < n; ++i) {
            int vowel = strchr("aeiouy", key[i]) != NULL;
            vowels += vowel;
            upper += i && text[i] >= 'A' && text[i] <= 'Z';
            run = vowel ? 0 : run + 1;
            if (run > longest)
                longest = run;
        }
        if (vowels && longest <= 3 && !upper) {
            ttf_trace_stage(e, TTF_TRACE_POLICY, part, part + n);
            return ttf_buffer_append(out, text, n);
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

/* These controls assert the object's type; they do not assert DNS existence,
   mailbox deliverability, RFC completeness or permission to fetch a URL.
   URLs are bounded printable ASCII references (relative paths are allowed).
   Email uses an unquoted dot-atom and hostname; internal hosts need no TLD. */
ttf_status_t ttf_electronic_control(const ttf_engine_t *e, const ttf_semantic_t *s,
                                    const char *text, size_t n, ttf_buffer_t *out) {
    const ttf_language_pack_t *source = ttf_find_pack(e, s->source_lang);
    const ttf_language_pack_t *target = ttf_find_pack(e, s->render_lang);
    size_t at = SIZE_MAX, i, label;
    int email = !strcmp(s->unit, "@email");
    if ((!email && strcmp(s->unit, "@url")) || !text || !n || !target)
        return TTF_EINVAL;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c <= 32 || c >= 127)
            return TTF_EINVAL;
        if (email && c == '@') {
            if (at != SIZE_MAX)
                return TTF_EINVAL;
            at = i;
        }
        if (!email && (c == '<' || c == '>' || c == '"' || c == '\\' || c == '`' ||
                       c == '{' || c == '}'))
            return TTF_EINVAL;
        if (!email && c == '%' && (i + 2 >= n ||
            !strchr("0123456789abcdefABCDEF", text[i + 1]) ||
            !strchr("0123456789abcdefABCDEF", text[i + 2])))
            return TTF_EINVAL;
    }
    if (email) {
        if (at == SIZE_MAX || !at || at + 1 == n || text[0] == '.' || text[at - 1] == '.')
            return TTF_EINVAL;
        for (i = 0; i < at; ++i) {
            if (!alnum(text[i]) && !strchr(".!#$%&'*+-/=?^_`{|}~", text[i]))
                return TTF_EINVAL;
            if (text[i] == '.' && i && text[i - 1] == '.')
                return TTF_EINVAL;
        }
        label = at + 1;
        for (i = label; i <= n; ++i) {
            if (i == n || text[i] == '.') {
                if (i == label || !alnum(text[label]) || !alnum(text[i - 1]))
                    return TTF_EINVAL;
                label = i + 1;
            } else if (!alnum(text[i]) && text[i] != '-')
                return TTF_EINVAL;
        }
    }
    return ttf_electronic_render(e, source, target, text, n, out);
}
