#include "protected.h"

ttf_status_t ttf_tokenize_protected(const ttf_engine_t *engine, const char *text, size_t length,
                                    int final, ttf_token_t **tokens, size_t *count,
                                    ttf_protected_region_t **regions, size_t *region_count) {
    ttf_token_t *raw = NULL, *view = NULL;
    ttf_protected_region_t *protected = NULL;
    size_t raw_count = 0, i = 0, used = 0, capacity = 0, nregions = 0, region_capacity = 0;
    ttf_status_t status;
    *tokens = NULL;
    *count = 0;
    *regions = NULL;
    *region_count = 0;
    status = ttf_tokenize(text, length, &raw, &raw_count);
    if (status != TTF_OK)
        return status;
    while (i < raw_count) {
        ttf_protected_match_t match = {0};
        ttf_token_t token = raw[i];
        int found = ttf_protect_markup(engine, text, length, token.begin, final, &match) ||
                    ttf_protect_path(engine, text, length, token.begin, final, &match);
        if (!found && engine->frontend_options.input_format == TTF_INPUT_ANNOTATED) {
            status = ttf_control_parse(engine, text, length, token.begin, final, &match.control);
            if (status != TTF_OK)
                goto fail;
            if (match.control.kind != TTF_CONTROL_NONE) {
                match.end = match.control.end;
                match.kind = "control";
                match.rule = match.control.kind == TTF_CONTROL_ESCAPE    ? "control.escape"
                             : match.control.kind == TTF_CONTROL_PATTERN ? "control.pattern"
                                                                         : "control.literal";
                match.layer = TTF_LAYER_COMMON;
                match.complete = match.control.complete;
                found = 1;
            }
        }
        if (used == capacity) {
            size_t next = capacity ? capacity * 2 : 32;
            ttf_token_t *p;
            if (next < capacity || next > SIZE_MAX / sizeof(*p)) {
                status = TTF_ELIMIT;
                goto fail;
            }
            p = (ttf_token_t *)realloc(view, next * sizeof(*p));
            if (!p) {
                status = TTF_ENOMEM;
                goto fail;
            }
            view = p;
            capacity = next;
        }
        if (!found) {
            view[used++] = token;
            ++i;
            continue;
        }
        if (match.end <= token.begin || match.end > length || !match.kind || !match.rule ||
            (final && !match.complete)) {
            status = TTF_ERESOURCE;
            goto fail;
        }
        if (nregions == region_capacity) {
            size_t next = region_capacity ? region_capacity * 2 : 8;
            ttf_protected_region_t *p;
            if (next < region_capacity || next > SIZE_MAX / sizeof(*p)) {
                status = TTF_ELIMIT;
                goto fail;
            }
            p = (ttf_protected_region_t *)realloc(protected, next * sizeof(*p));
            if (!p) {
                status = TTF_ENOMEM;
                goto fail;
            }
            protected = p;
            region_capacity = next;
        }
        protected[nregions].begin = token.begin;
        protected[nregions++].match = match;
        token.end = match.end;
        token.script = TTF_SCRIPT_SYMBOL;
        view[used++] = token;
        while (i < raw_count && raw[i].end <= match.end)
            ++i;
        if (i < raw_count && raw[i].begin < match.end)
            raw[i].begin = match.end;
    }
    free(raw);
    *tokens = view;
    *count = used;
    *regions = protected;
    *region_count = nregions;
    return TTF_OK;
fail:
    free(raw);
    free(view);
    free(protected);
    return status;
}
