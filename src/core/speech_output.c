#include "speech_output.h"
#include "../pronunciation/trace.h"

/* This is a character safety pass, not a second TN grammar or transliterator.
   Literal overrides and intentionally protected semantic objects bypass it.
   Resource replacements are explicit output and are never recursively expanded. */
ttf_status_t ttf_speech_output(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                               ttf_buffer_t *text) {
    ttf_buffer_t out = {0};
    size_t at = 0;
    ttf_status_t status = TTF_OK;
    if (e->frontend_options.output_mode != TTF_OUTPUT_SPEECH)
        return TTF_OK;
    while (at < text->length) {
        uint32_t cp;
        size_t width;
        char key[5];
        const char *mapped, *keep;
        ttf_script_t script;
        if (ttf_utf8_decode(text->data + at, text->length - at, &cp, &width) != 1) {
            status = TTF_EUTF8;
            break;
        }
        memcpy(key, text->data + at, width);
        key[width] = 0;
        script = ttf_script_of(cp);
        ttf_trace_stage(e, TTF_TRACE_POLICY, at, at + width);
        mapped = ttf_resource_get(e, "speech_character", renderer->language_tag, key, NULL);
        keep = ttf_resource_get(e, "speech_preserve", renderer->language_tag, key, NULL);
        if (mapped) {
            status = ttf_trace_resource(e, "speech_character", renderer->language_tag, key, key,
                                        "final-character-mapped", SIZE_MAX);
            if (status == TTF_OK)
                status = ttf_buffer_puts(&out, mapped);
        } else if (script == TTF_SCRIPT_WHITESPACE || (keep && !strcmp(keep, "1")) ||
                   (renderer->script_score && renderer->script_score(script) > 0)) {
            status = ttf_buffer_append(&out, key, width);
        } else if ((mapped =
                        ttf_resource_get(e, "speech_symbol", renderer->language_tag, key, NULL))) {
            status = ttf_trace_resource(e, "speech_symbol", renderer->language_tag, key, key,
                                        "final-symbol-mapped", SIZE_MAX);
            if (status == TTF_OK)
                status = ttf_buffer_puts(&out, mapped);
        } else if ((cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') ||
                   (cp >= 'a' && cp <= 'z')) {
            size_t before = out.length, attempt = ttf_trace_mark(e);
            ttf_semantic_t sem = {0};
            sem.struct_size = sizeof(sem);
            sem.type = TTF_SEM_CUSTOM;
            snprintf(sem.unit, sizeof(sem.unit), "@alphabet");
            snprintf(sem.value, sizeof(sem.value), "%s", key);
            status = renderer->verbalize(e, &sem, key, width, &out);
            ttf_trace_stage(e, TTF_TRACE_POLICY, at, at + width);
            if (status == TTF_OK)
                status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0,
                                        renderer->language_tag, "decision", "final-alphabet", "",
                                        key, "final-character-spelled", SIZE_MAX);
            /* A third-party pack can have no alphabet renderer. Its unsupported
               characters then follow the same explicit user policy. */
            if (status == TTF_EINVAL) {
                out.length = before;
                if (out.data)
                    out.data[before] = 0;
                ttf_trace_reject_since(e, attempt);
                goto unknown;
            }
        } else {
        unknown:
            status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0,
                                    renderer->language_tag, "decision", "unreadable", "", key,
                                    e->frontend_options.unreadable_policy == TTF_UNREADABLE_DROP
                                        ? "final-unreadable-dropped"
                                        : "final-unreadable-preserved",
                                    SIZE_MAX);
            if (status == TTF_OK &&
                e->frontend_options.unreadable_policy == TTF_UNREADABLE_PRESERVE)
                status = ttf_buffer_append(&out, key, width);
        }
        if (status != TTF_OK)
            break;
        at += width;
    }
    if (status == TTF_OK) {
        ttf_buffer_free(text);
        *text = out;
    } else
        ttf_buffer_free(&out);
    return status;
}
