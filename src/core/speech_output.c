#include "speech_output.h"
#include "../unicode_fallback/fallback.h"
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

/* A range mark written against both neighbours ("서울~부산") joins them; with a
   space on either side it keeps its symbol name. */
const char *ttf_speech_range_mark(const ttf_engine_t *e, const char *language, const char *left,
                                  size_t left_length, const char *span, size_t span_length,
                                  const char *right, size_t right_length) {
    char key[8];
    size_t width;
    if (!span_length || span_length >= sizeof(key) ||
        !ttf_speech_spoken_before(e, language, left, left_length) ||
        !spoken_at(e, language, right, right_length))
        return NULL;
    memcpy(key, span, span_length);
    key[span_length] = 0;
    /* A repeated mark is a lengthening run, not a range between two words. */
    if (lengthening(e, language, right, right_length, &width) ||
        (right_length >= span_length && !memcmp(right, key, span_length)))
        return NULL;
    {
        size_t start = left_length;
        while (start && ((unsigned char)left[start - 1] & 0xc0) == 0x80)
            --start;
        if (start && left_length - start + 1 >= span_length &&
            !memcmp(left + left_length - span_length, key, span_length))
            return NULL;
    }
    /* A customer who disabled this mark as a range separator keeps its name. */
    {
        const char *separator = ttf_resource_get(e, "quantity_range_separator", "*", key, NULL);
        if (separator && !strcmp(separator, "0"))
            return NULL;
    }
    return ttf_resource_get(e, "speech_range_mark", language, key, NULL);
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

/* A word letter (Hangul or Latin) starts right after the symbol. */
static int word_follows(const char *s, size_t n, const char *right, size_t right_length) {
    uint32_t cp;
    size_t width;
    ttf_script_t script;
    if (!n) {
        s = right;
        n = right_length;
    }
    if (!n || ttf_utf8_decode(s, n, &cp, &width) != 1)
        return 0;
    script = ttf_script_of(cp);
    return script == TTF_SCRIPT_HANGUL || script == TTF_SCRIPT_LATIN;
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
/* The Unicode spoken fallback names a character, or the complete emoji
   sequence starting there, that no table above reads. *consumed is 0 when it
   has no allowed name, so the unreadable policy decides as before. */
static ttf_status_t unicode_fallback(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                                     const char *lang, const char *text, size_t length,
                                     size_t at, int prev_spoken, ttf_buffer_t *out,
                                     size_t *consumed) {
    size_t end = ttf_emoji_cluster_end(text, length, at), width;
    uint32_t cp;
    ttf_buffer_t name = {0};
    int found = 0;
    ttf_status_t status;
    char key[64];
    *consumed = 0;
    if (!end) {
        if (ttf_utf8_decode(text + at, length - at, &cp, &width) != 1)
            return TTF_OK;
        end = at + width;
    }
    status = ttf_symbol_fallback_speak(e, renderer, text + at, end - at, &name, &found);
    if (status == TTF_OK && found) {
        snprintf(key, sizeof(key), "%.*s", (int)(end - at < sizeof(key) - 1 ? end - at : sizeof(key) - 1),
                 text + at);
        status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, lang, "decision",
                                "unicode-fallback", "", key, "final-unicode-fallback", SIZE_MAX);
        if (status == TTF_OK && prev_spoken)
            status = ttf_buffer_puts(out, " ");
        if (status == TTF_OK)
            status = ttf_buffer_append(out, name.data ? name.data : "", name.length);
        if (status == TTF_OK)
            *consumed = end - at;
    }
    ttf_buffer_free(&name);
    return status;
}
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
        size_t width, run, fallback;
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
        } else if (ttf_resource_get(e, "speech_silent", lang, key, NULL) &&
                   (!strcmp(ttf_resource_get(e, "speech_silent", lang, key, NULL), "1") ||
                    /* K-팝, U-보금자리론: between two spoken words only (2026-10-07) */
                    (!strcmp(ttf_resource_get(e, "speech_silent", lang, key, NULL), "between") &&
                     PREV_SPOKEN() &&
                     word_follows(text->data + at + width, text->length - at - width, right,
                                  right_length)))) {
            /* A path or bracket mark outside an expression is not spoken; it
               still separates the words on either side. */
            status = ttf_trace_resource(e, "speech_silent", lang, key, key, "final-silent-mark",
                                        SIZE_MAX);
            /* INGREDIENTS.--To, manner:--Dissolve: after clause punctuation the
               dash still separates the words (2026-10-07). */
            {
                size_t k = left_length;
                char last = '\0';
                if (out.length)
                    last = out.data[out.length - 1];
                else {
                    while (k && left[k - 1] == '-')
                        --k;
                    last = k ? left[k - 1] : '\0';
                }
                if (PREV_SPOKEN() ||
                    (last && strchr(".,;:!?", last) &&
                     word_follows(text->data + at + width, text->length - at - width, right,
                                  right_length)))
                    pending_space = 1;
            }
        } else if ((mapped = ttf_resource_get(e, "speech_symbol", lang, key, NULL))) {
            int spaced = symbol_spaced(e, lang, key);
            const char *prefix = word_follows(text->data + at + width, text->length - at - width,
                                              right, right_length) &&
                                         !PREV_SPOKEN()
                                     ? ttf_resource_get(e, "speech_symbol_prefix", lang, key, NULL)
                                     : NULL;
            if (prefix)
                mapped = prefix;
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
            /* Spelled characters follow the declared alphabet join, so English
               reads "two zero f" while Korean keeps 이영에프 in one piece. */
            {
                const char *join = ttf_resource_get(e, "speech_join", lang, "alphabet", NULL);
                if (join && !strcmp(join, "space"))
                    pending_space = 1;
            }
        } else if (e->decorative_policy == TTF_DECORATIVE_READ &&
                   (mapped = ttf_resource_get(e, "speech_decorative", lang, key, NULL))) {
            /* A declared decorative name is spoken like any other symbol name. */
            status = ttf_trace_resource(e, "speech_decorative", lang, key, key, "decorative-name",
                                        SIZE_MAX);
            if (status == TTF_OK && PREV_SPOKEN())
                status = ttf_buffer_puts(&out, " ");
            if (status == TTF_OK)
                status = ttf_buffer_puts(&out, mapped);
            pending_space = 1;
        } else if ((status = unicode_fallback(e, renderer, lang, text->data, text->length, at,
                                              PREV_SPOKEN(), &out, &fallback)) != TTF_OK ||
                   fallback) {
            if (status != TTF_OK)
                break;
            pending_space = 1;
            at += fallback;
            continue;
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
