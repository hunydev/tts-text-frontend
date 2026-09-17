#ifndef TTF_NUMERIC_GROUPING_H
#define TTF_NUMERIC_GROUPING_H
#include "../core/internal.h"
/* Recognize a possible grouping separator without deciding whether a following
   group exists. Ordinary ASCII spaces/tabs and line separators never qualify.
   The inventory is supplied by resources; known typographic number spaces stay
   recognizable when disabled so malformed quantities can be preserved whole. */
static inline size_t ttf_numeric_group_separator(const ttf_engine_t *engine, const char *text,
                                                 size_t length, size_t position, int *enabled,
                                                 ttf_resource_layer_t *layer) {
    uint32_t cp;
    size_t width;
    char key[5];
    const char *policy;
    ttf_resource_layer_t matched = TTF_LAYER_COMMON;
    if (position >= length || ttf_utf8_decode(text + position, length - position, &cp, &width) != 1)
        return 0;
    if (cp != ',' &&
        (cp < 128 || cp == 0x2028 || cp == 0x2029 || ttf_script_of(cp) != TTF_SCRIPT_WHITESPACE))
        return 0;
    memcpy(key, text + position, width);
    key[width] = '\0';
    policy = ttf_resource_get(engine, "numeric_group_separator", "*", key, &matched);
    if (!policy && cp != ',' && cp != 0xa0 && cp != 0x202f && cp != 0x2009)
        return 0;
    if (enabled)
        *enabled = policy ? !strcmp(policy, "3") : cp == ',';
    if (layer)
        *layer = matched;
    return width;
}
/* A grouping candidate joins only to another digit, possibly after repeated
   candidate separators. Repetition is retained for whole-object rejection. */
static inline size_t ttf_numeric_group_link(const ttf_engine_t *engine, const char *text,
                                            size_t length, size_t position) {
    size_t p = position, width;
    while ((width = ttf_numeric_group_separator(engine, text, length, p, NULL, NULL)))
        p += width;
    return p > position && p < length && text[p] >= '0' && text[p] <= '9' ? p : 0;
}
/* Shared exact decimal grammar. The end is an original UTF-8 byte offset;
   failure returns start. The candidate scanner supports whole preservation. */
size_t ttf_numeric_value(const ttf_pack_input_t *input, size_t start, char value[TTF_IR_VALUE_SIZE],
                         ttf_resource_layer_t *layer);
size_t ttf_numeric_atom_end(const ttf_pack_input_t *input, size_t start);
#endif
