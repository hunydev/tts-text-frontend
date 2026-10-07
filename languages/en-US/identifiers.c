#include "identifiers.h"
#include "../../src/semantic/phone.h"
#include <stdio.h>
#include <string.h>

static int decimal_digit(char c) {
    return c >= '0' && c <= '9';
}
static int ascii_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static int valid_tail(const ttf_pack_input_t *in, size_t end) {
    size_t i;
    unsigned char c;
    if (end == in->length)
        return 1;
    c = (unsigned char)in->text[end];
    if (c < 128) {
        if (ascii_letter((char)c) || decimal_digit((char)c) || c == '-' || c == '+' || c == '/' ||
            c == ':' || c == '%' || c == '@')
            return 0;
        if ((c == '.' || c == ',') && end + 1 < in->length &&
            (decimal_digit(in->text[end + 1]) || ascii_letter(in->text[end + 1])))
            return 0;
        return 1;
    }
    for (i = in->index; i < in->token_count; ++i) {
        const ttf_token_t *token = &in->tokens[i];
        if (token->begin <= end && end < token->end) {
            char key[TTF_IR_VALUE_SIZE];
            size_t j, n = token->end - end;
            if (token->script == TTF_SCRIPT_PUNCTUATION || token->script == TTF_SCRIPT_WHITESPACE)
                return 1;
            if (n >= sizeof(key))
                return 0;
            memcpy(key, in->text + end, n);
            key[n] = '\0';
            /* A registered target pack can declare attached grammatical tails.
             * This keeps code-mixed identifiers without a source/target pair branch. */
            for (j = 0; j < in->engine->pack_count; ++j) {
                const char *language = in->engine->packs[j]->language_tag;
                if (ttf_resource_get(in->engine, "particle", language, key, NULL) ||
                    ttf_resource_get(in->engine, "quantity_tail", language, key, NULL))
                    return 1;
            }
            return 0;
        }
    }
    return 0;
}
size_t ttf_en_classify_identifier(const ttf_pack_input_t *in, ttf_semantic_t *semantic) {
    size_t begin = in->tokens[in->index].begin, end = begin, i;
    char label[TTF_IR_VALUE_SIZE];
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    if (begin == in->length || !decimal_digit(in->text[begin]) ||
        !ttf_phone_preceding_label(in, begin, "identifier_label", "en-US", label, &layer))
        return 0;
    while (end < in->length && decimal_digit(in->text[end]))
        ++end;
    while (end + 1 < in->length && in->text[end] == '-' && decimal_digit(in->text[end + 1])) {
        ++end;
        while (end < in->length && decimal_digit(in->text[end]))
            ++end;
    }
    /* ISBN 1-86508-123-X: an ISBN-10 check digit may be X (2026-10-07). */
    if (!strcmp(label, "isbn") && end < in->length) {
        size_t x = end + (in->text[end] == '-');
        if (x < in->length && (in->text[x] == 'X' || in->text[x] == 'x') &&
            valid_tail(in, x + 1))
            end = x + 1;
    }
    if (!valid_tail(in, end) || end - begin >= sizeof(semantic->value))
        return 0;
    memcpy(semantic->value, in->text + begin, end - begin);
    semantic->value[end - begin] = '\0';
    snprintf(semantic->secondary, sizeof(semantic->secondary), "%s", label);
    snprintf(semantic->unit, sizeof(semantic->unit), "financial_identifier");
    snprintf(semantic->rule, sizeof(semantic->rule), "en:labeled-identifier");
    semantic->type = TTF_SEM_DIGIT_SEQUENCE;
    semantic->input_end = end;
    semantic->layer = layer;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < end; ++i) {
    }
    return i - in->index;
}

ttf_status_t ttf_en_render_identifier(const ttf_engine_t *engine, const char *value,
                                      ttf_buffer_t *output) {
    int previous_digit = 0;
    for (; *value; ++value) {
        const char *rendered;
        ttf_status_t status;
        if (*value == '-') {
            rendered = ttf_resource_get(engine, "identifier_render", "en-US", "separator", NULL);
            if (!rendered)
                rendered = " ";
            previous_digit = 0;
        } else if (decimal_digit(*value)) {
            char key[2] = {*value, '\0'};
            rendered = *value == '0'
                           ? ttf_resource_get(engine, "identifier_render", "en-US", "zero", NULL)
                           : ttf_resource_get(engine, "number", "en-US", key, NULL);
            if (!rendered && *value == '0')
                rendered = ttf_resource_get(engine, "number", "en-US", "0", NULL);
            if (previous_digit && (status = ttf_buffer_puts(output, " ")) != TTF_OK)
                return status;
            previous_digit = 1;
        } else if (*value == 'X' || *value == 'x') {
            rendered = "X";
            if (previous_digit && (status = ttf_buffer_puts(output, " ")) != TTF_OK)
                return status;
            previous_digit = 1;
        } else
            return TTF_EINVAL;
        if (!rendered)
            return TTF_ERESOURCE;
        status = ttf_buffer_puts(output, rendered);
        if (status != TTF_OK)
            return status;
    }
    return TTF_OK;
}
