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

/* B1층, B1F and B1~B3층: a declared basement prefix, digits and an explicit
   floor unit (a floor alias or the unit word). Bare B1 stays an identifier. */
static size_t basement_endpoint(const ttf_pack_input_t *in, size_t p, char *value, size_t capacity,
                                int *unit) {
    char prefix[2] = {0, 0}, alias[2] = {0, 0};
    const char *word = ttf_resource_get(in->engine, "unit", "ko-KR", "floor", NULL);
    size_t start;
    *unit = 0;
    if (p >= in->length)
        return 0;
    prefix[0] = in->text[p];
    if (!ttf_resource_get(in->engine, "floor_basement_prefix", "ko-KR", prefix, NULL))
        return 0;
    start = ++p;
    while (p < in->length && in->text[p] >= '0' && in->text[p] <= '9')
        ++p;
    if (p == start || p - start >= capacity || in->text[start] == '0')
        return 0;
    memcpy(value, in->text + start, p - start);
    value[p - start] = 0;
    if (p < in->length) {
        alias[0] = in->text[p];
        if (ttf_resource_get(in->engine, "floor_alias", "ko-KR", alias, NULL) &&
            !(p + 1 < in->length && ((in->text[p + 1] >= 'A' && in->text[p + 1] <= 'Z') ||
                                     (in->text[p + 1] >= 'a' && in->text[p + 1] <= 'z') ||
                                     (in->text[p + 1] >= '0' && in->text[p + 1] <= '9')))) {
            *unit = 1;
            return p + 1;
        }
        if (word && !strncmp(in->text + p, word, strlen(word))) {
            *unit = 1;
            return p + strlen(word);
        }
    }
    if (p < in->length &&
        ((in->text[p] >= 'A' && in->text[p] <= 'Z') || (in->text[p] >= 'a' && in->text[p] <= 'z')))
        return 0;
    return p;
}

size_t ttf_ko_classify_basement(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    size_t start = in->tokens[in->index].begin, end, second_end = 0, next;
    char first[32], second[32];
    int first_unit, second_unit = 0;
    if (in->tokens[in->index].end != start + 1 ||
        (start && ((in->text[start - 1] >= 'A' && in->text[start - 1] <= 'Z') ||
                   (in->text[start - 1] >= 'a' && in->text[start - 1] <= 'z') ||
                   (in->text[start - 1] >= '0' && in->text[start - 1] <= '9'))))
        return 0;
    end = basement_endpoint(in, start, first, sizeof(first), &first_unit);
    if (!end)
        return 0;
    if (end < in->length) {
        size_t width = 0;
        if (in->text[end] == '~' || in->text[end] == '-')
            width = 1;
        else if (in->length - end >= 3 && !memcmp(in->text + end, "\xef\xbd\x9e", 3))
            width = 3;
        if (width)
            second_end = basement_endpoint(in, end + width, second, sizeof(second), &second_unit);
    }
    if (second_end && second_unit) {
        snprintf(out->value, sizeof(out->value), "%s", first);
        snprintf(out->secondary, sizeof(out->secondary), "%s", second);
        snprintf(out->unit, sizeof(out->unit), "basement_floor");
        snprintf(out->rule, sizeof(out->rule), "ko:basement-floor-range");
        out->type = TTF_SEM_RANGE;
        end = second_end;
    } else if (first_unit) {
        snprintf(out->value, sizeof(out->value), "%s", first);
        snprintf(out->unit, sizeof(out->unit), "basement_floor");
        snprintf(out->rule, sizeof(out->rule), "ko:basement-floor");
        out->type = TTF_SEM_MEASURE;
    } else
        return 0;
    out->input_end = end;
    for (next = in->index; next < in->token_count && in->tokens[next].begin < end; ++next)
        if (in->tokens[next].end > end)
            return 0;
    return next - in->index;
}
