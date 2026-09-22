#include "speech_output.h"
#include "../pronunciation/trace.h"

/* A character is spoken content when it is neither whitespace nor punctuation
   kept for prosody. Symbol names are separated only from spoken neighbors. */
static int spoken_at(const ttf_engine_t *e, const char *language, const char *s, size_t n) {
    uint32_t cp;
    size_t width;
    char key[5];
    const char *keep;
    if (!n || ttf_utf8_decode(s, n, &cp, &width) != 1)
        return 0;
    if (ttf_script_of(cp) == TTF_SCRIPT_WHITESPACE)
        return 0;
    memcpy(key, s, width);
    key[width] = 0;
    keep = ttf_resource_get(e, "speech_preserve", language, key, NULL);
    return !(keep && !strcmp(keep, "1"));
}

int ttf_speech_spoken_after(const ttf_engine_t *e, const char *language, const char *s, size_t n) {
    return spoken_at(e, language, s, n);
}

int ttf_speech_spoken_before(const ttf_engine_t *e, const char *language, const char *s, size_t n) {
    size_t start = n;
    while (start && ((unsigned char)s[start - 1] & 0xc0) == 0x80)
        --start;
    return start ? spoken_at(e, language, s + start - 1, n - start + 1) : 0;
}

static int lengthening(const ttf_engine_t *e, const char *language, const char *s, size_t n,
                       size_t *width) {
    uint32_t cp;
    char key[5];
    const char *flag;
    if (!n || ttf_utf8_decode(s, n, &cp, width) != 1)
        return 0;
    memcpy(key, s, *width);
    key[*width] = 0;
    flag = ttf_resource_get(e, "speech_lengthening", language, key, NULL);
    return flag && !strcmp(flag, "1");
}

int ttf_speech_lengthening_span(const ttf_engine_t *e, const char *language, const char *left,
                                size_t left_length, const char *span, size_t span_length,
                                const char *right, size_t right_length) {
    size_t at = 0, width;
    /* A run of marks may arrive as one token per mark. */
    while (left_length) {
        size_t start = left_length;
        while (start && ((unsigned char)left[start - 1] & 0xc0) == 0x80)
            --start;
        if (!start || !lengthening(e, language, left + start - 1, left_length - start + 1, &width))
            break;
        left_length = start - 1;
    }
    while (right_length && lengthening(e, language, right, right_length, &width)) {
        right += width;
        right_length -= width;
    }
    if (!span_length || !ttf_speech_spoken_before(e, language, left, left_length) ||
        spoken_at(e, language, right, right_length))
        return 0;
    while (at < span_length && lengthening(e, language, span + at, span_length - at, &width))
        at += width;
    return at == span_length;
}

/* `speech_join symbol.<char>` overrides `speech_join symbol`; the default
   separates a symbol name from neighboring words. */
static int symbol_spaced(const ttf_engine_t *e, const char *language, const char *key) {
    char name[16];
    const char *join;
    snprintf(name, sizeof(name), "symbol.%s", key);
    join = ttf_resource_get(e, "speech_join", language, name, NULL);
    if (!join)
        join = ttf_resource_get(e, "speech_join", language, "symbol", NULL);
    return !join || strcmp(join, "none");
}

/* This is a character safety pass, not a second TN grammar or transliterator.
   Literal overrides and intentionally protected semantic objects bypass it.
   Resource replacements are explicit output and are never recursively expanded.
   `left` is output already committed in this unit; `right` is the source after
   the span. They only decide word spacing and lengthening marks. */
ttf_status_t ttf_speech_output(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                               ttf_buffer_t *text, const char *left, size_t left_length,
                               const char *right, size_t right_length) {
    ttf_buffer_t out = {0};
    size_t at = 0;
    int pending_space = 0;
    ttf_status_t status = TTF_OK;
    const char *lang = renderer->language_tag;
    if (e->frontend_options.output_mode != TTF_OUTPUT_SPEECH)
        return TTF_OK;
#define PREV_SPOKEN()                                                                              \
    (out.length ? ttf_speech_spoken_before(e, lang, out.data, out.length)                          \
                : ttf_speech_spoken_before(e, lang, left, left_length))
    while (at < text->length) {
        uint32_t cp;
        size_t width, run;
        char key[5];
        const char *mapped, *keep;
        ttf_script_t script;
        int spoken;
        if (ttf_utf8_decode(text->data + at, text->length - at, &cp, &width) != 1) {
            status = TTF_EUTF8;
            break;
        }
        memcpy(key, text->data + at, width);
        key[width] = 0;
        script = ttf_script_of(cp);
        spoken = spoken_at(e, lang, key, width);
        ttf_trace_stage(e, TTF_TRACE_POLICY, at, at + width);
        mapped = ttf_resource_get(e, "speech_character", lang, key, NULL);
        keep = ttf_resource_get(e, "speech_preserve", lang, key, NULL);
        if (!mapped && lengthening(e, lang, key, width, &run) && PREV_SPOKEN()) {
            /* A lengthening mark after a word carries prosody, not a word.
               Between two words or standing alone it keeps its symbol name. */
            size_t next = at + width, mark_width;
            int followed;
            while (next < text->length &&
                   lengthening(e, lang, text->data + next, text->length - next, &mark_width))
                next += mark_width;
            followed = next < text->length
                           ? spoken_at(e, lang, text->data + next, text->length - next)
                           : spoken_at(e, lang, right, right_length);
            if (!followed) {
                status = ttf_trace_resource(e, "speech_lengthening", lang, key, key,
                                            "final-lengthening-mark", SIZE_MAX);
                if (status != TTF_OK)
                    break;
                at = next;
                continue;
            }
        }
        if (mapped) {
            status = ttf_trace_resource(e, "speech_character", lang, key, key,
                                        "final-character-mapped", SIZE_MAX);
            if (status == TTF_OK && pending_space)
                status = ttf_buffer_puts(&out, " ");
            pending_space = 0;
            if (status == TTF_OK)
                status = ttf_buffer_puts(&out, mapped);
        } else if (script == TTF_SCRIPT_WHITESPACE || (keep && !strcmp(keep, "1")) ||
                   (renderer->script_score && renderer->script_score(script) > 0)) {
            if (pending_space && spoken)
                status = ttf_buffer_puts(&out, " ");
            pending_space = 0;
            if (status == TTF_OK)
                status = ttf_buffer_append(&out, key, width);
        } else if ((mapped = ttf_resource_get(e, "speech_symbol", lang, key, NULL))) {
            int spaced = symbol_spaced(e, lang, key);
            status = ttf_trace_resource(e, "speech_symbol", lang, key, key, "final-symbol-mapped",
                                        SIZE_MAX);
            if (status == TTF_OK && spaced && PREV_SPOKEN())
                status = ttf_buffer_puts(&out, " ");
            if (status == TTF_OK)
                status = ttf_buffer_puts(&out, mapped);
            pending_space = spaced;
        } else if ((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') ||
                   (cp >= 'a' && cp <= 'z')) {
            size_t before = out.length, attempt = ttf_trace_mark(e);
            int had_pending = pending_space;
            ttf_semantic_t sem = {0};
            if (pending_space && (status = ttf_buffer_puts(&out, " ")) != TTF_OK)
                break;
            pending_space = 0;
            sem.struct_size = sizeof(sem);
            sem.type = TTF_SEM_CUSTOM;
            snprintf(sem.unit, sizeof(sem.unit), "@alphabet");
            snprintf(sem.value, sizeof(sem.value), "%s", key);
            status = renderer->verbalize(e, &sem, key, width, &out);
            ttf_trace_stage(e, TTF_TRACE_POLICY, at, at + width);
            if (status == TTF_OK)
                status =
                    ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, lang, "decision",
                                   "final-alphabet", "", key, "final-character-spelled", SIZE_MAX);
            /* A third-party pack can have no alphabet renderer. Its unsupported
               characters then follow the same explicit user policy. */
            if (status == TTF_EINVAL) {
                pending_space = had_pending;
                out.length = before;
                if (out.data)
                    out.data[before] = 0;
                ttf_trace_reject_since(e, attempt);
                goto unknown;
            }
        } else {
        unknown:
            status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, lang, "decision",
                                    "unreadable", "", key,
                                    e->frontend_options.unreadable_policy == TTF_UNREADABLE_DROP
                                        ? "final-unreadable-dropped"
                                        : "final-unreadable-preserved",
                                    SIZE_MAX);
            if (e->frontend_options.unreadable_policy == TTF_UNREADABLE_PRESERVE) {
                if (status == TTF_OK && pending_space && spoken)
                    status = ttf_buffer_puts(&out, " ");
                pending_space = 0;
                if (status == TTF_OK)
                    status = ttf_buffer_append(&out, key, width);
            } else if (PREV_SPOKEN())
                /* Removing a character between two words must not join them. */
                pending_space = 1;
        }
        if (status != TTF_OK)
            break;
        at += width;
    }
#undef PREV_SPOKEN
    if (status == TTF_OK && pending_space && spoken_at(e, lang, right, right_length))
        status = ttf_buffer_puts(&out, " ");
    if (status == TTF_OK) {
        ttf_buffer_free(text);
        *text = out;
    } else
        ttf_buffer_free(&out);
    return status;
}
