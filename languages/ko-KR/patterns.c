/* Building floor shorthand needs an explicit local building cue.
 * Temperature marks and isolated identifiers keep their own interpretation. */
#include "patterns.h"
#include <stdio.h>
#include <string.h>

static int cue(const ttf_pack_input_t *in, const char *kind, size_t index,
               ttf_resource_layer_t *layer) {
    const ttf_token_t *token = &in->tokens[index];
    char key[96];
    const char *value;
    size_t width = token->end - token->begin;
    if (!width || width >= sizeof(key))
        return 0;
    memcpy(key, in->text + token->begin, width);
    key[width] = 0;
    value = ttf_resource_get(in->engine, kind, "ko-KR", key, layer);
    if (value)
        return !strcmp(value, "1");
    if (!strcmp(kind, "floor_cue_before")) {
        /* A named building can retain its productive noun ending: the longest
           declared suffix wins, including an explicit disabled suffix. */
        for (size_t offset = 0; offset < width; ++offset) {
            if (((unsigned char)key[offset] & 0xc0u) == 0x80u)
                continue;
            value = ttf_resource_get(in->engine, "floor_cue_before_suffix", "ko-KR", key + offset,
                                     layer);
            if (value)
                return !strcmp(value, "1");
        }
    }
    return 0;
}
static int horizontal(const ttf_pack_input_t *in, size_t index) {
    const ttf_token_t *token = &in->tokens[index];
    size_t p;
    if (token->script != TTF_SCRIPT_WHITESPACE)
        return 0;
    for (p = token->begin; p < token->end; ++p)
        if (in->text[p] != ' ' && in->text[p] != '\t')
            return 0;
    return 1;
}
size_t ttf_ko_classify_floor(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    size_t start = in->tokens[in->index].begin, end = start, previous = in->index;
    size_t next, consumed;
    char alias[2];
    const char *unit;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON, cue_layer = TTF_LAYER_COMMON;
    int matched = 0;
    while (end < in->length && in->text[end] >= '0' && in->text[end] <= '9')
        ++end;
    if (end == start || end - start >= sizeof(out->value) || end == in->length)
        return 0;
    alias[0] = in->text[end];
    alias[1] = 0;
    unit = ttf_resource_get(in->engine, "floor_alias", "ko-KR", alias, &layer);
    if (!unit)
        return 0;
    ++end;
    /* The alias must finish a Latin token, not prefix F2/Foo. */
    for (next = in->index; next < in->token_count && in->tokens[next].begin < end; ++next)
        if (in->tokens[next].end > end)
            return 0;
    if (next < in->token_count && in->tokens[next].begin == end &&
        (in->tokens[next].script == TTF_SCRIPT_LATIN ||
         in->tokens[next].script == TTF_SCRIPT_DIGIT))
        return 0;
    consumed = next - in->index;
    while (previous && horizontal(in, previous - 1))
        --previous;
    if (previous)
        matched = cue(in, "floor_cue_before", previous - 1, &cue_layer);
    while (next < in->token_count && horizontal(in, next))
        ++next;
    if (!matched && next < in->token_count)
        matched = cue(in, "floor_cue_after", next, &cue_layer);
    if (!matched)
        return 0;
    memcpy(out->value, in->text + start, end - start - 1);
    out->value[end - start - 1] = 0;
    snprintf(out->unit, sizeof(out->unit), "%s", unit);
    snprintf(out->rule, sizeof(out->rule), "ko:building-floor");
    out->type = TTF_SEM_MEASURE;
    out->input_end = end;
    out->layer = layer > cue_layer ? layer : cue_layer;
    return consumed;
}
