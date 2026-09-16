#include "identifiers.h"
#include <stdio.h>
#include <string.h>

static int horizontal_space(char c) {
    return c == ' ' || c == '\t';
}

static int decimal_digit(char c) {
    return c >= '0' && c <= '9';
}

/* The tokenizer supplies starts so a cue cannot match inside a longer word.
 * Canonical keys use one ASCII space; repeated spaces and tabs are equivalent.
 * Newlines, punctuation and quotes terminate label scope. */
static int preceding_label(const ttf_pack_input_t *in, size_t begin, char *matched, size_t capacity,
                           ttf_resource_layer_t *layer) {
    size_t end = begin, i;
    int found = 0, enabled = 0;
    while (end && horizontal_space(in->text[end - 1]))
        --end;
    if (end && in->text[end - 1] == ':') {
        --end;
        while (end && horizontal_space(in->text[end - 1]))
            --end;
    }
    for (i = in->index; i > 0; --i) {
        const ttf_token_t *token = &in->tokens[i - 1];
        size_t p, used = 0;
        int pending_space = 0;
        char key[TTF_IR_VALUE_SIZE];
        const char *value;
        ttf_resource_layer_t candidate_layer;
        if (token->begin >= end)
            continue;
        if (end - token->begin >= capacity)
            break;
        if (token->script == TTF_SCRIPT_WHITESPACE) {
            for (p = token->begin; p < token->end; ++p)
                if (in->text[p] == '\n' || in->text[p] == '\r')
                    return found && enabled;
            continue;
        }
        if (token->script == TTF_SCRIPT_PUNCTUATION || token->script == TTF_SCRIPT_SYMBOL)
            break;
        for (p = token->begin; p < end; ++p) {
            char c = in->text[p];
            if (c == '\n' || c == '\r')
                return found && enabled;
            if (horizontal_space(c)) {
                pending_space = 1;
                continue;
            }
            if (pending_space && used)
                key[used++] = ' ';
            pending_space = 0;
            key[used++] = c;
        }
        key[used] = '\0';
        value = ttf_resource_get(in->engine, "identifier_label", "ko-KR", key, &candidate_layer);
        if (value) {
            /* Prefer the longest complete cue; resource layering has already
             * selected the effective record for that exact cue. */
            found = 1;
            enabled = strcmp(value, "digits") == 0;
            memcpy(matched, key, used + 1);
            *layer = candidate_layer;
        }
    }
    return found && enabled;
}

static int valid_tail(const ttf_pack_input_t *in, size_t end) {
    size_t i;
    unsigned char c;
    if (end == in->length)
        return 1;
    c = (unsigned char)in->text[end];
    if (c < 128) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || decimal_digit((char)c) ||
            c == '-' || c == '+' || c == '/' || c == ':' || c == '%')
            return 0;
        if ((c == '.' || c == ',') && end + 1 < in->length && decimal_digit(in->text[end + 1]))
            return 0;
        return 1;
    }
    for (i = in->index; i < in->token_count; ++i) {
        const ttf_token_t *token = &in->tokens[i];
        if (token->begin <= end && end < token->end) {
            char key[TTF_IR_VALUE_SIZE];
            size_t n = token->end - end;
            if (token->script != TTF_SCRIPT_HANGUL)
                return token->script == TTF_SCRIPT_PUNCTUATION ||
                       token->script == TTF_SCRIPT_WHITESPACE;
            if (n >= sizeof(key))
                return 0;
            memcpy(key, in->text + end, n);
            key[n] = '\0';
            return ttf_resource_get(in->engine, "particle", "ko-KR", key, NULL) != NULL ||
                   ttf_resource_get(in->engine, "quantity_tail", "ko-KR", key, NULL) != NULL;
        }
    }
    return 0;
}

size_t ttf_ko_classify_identifier(const ttf_pack_input_t *in, ttf_semantic_t *semantic) {
    size_t begin = in->tokens[in->index].begin, end = begin, i;
    char label[TTF_IR_VALUE_SIZE];
    ttf_resource_layer_t layer;
    if (begin == in->length || !decimal_digit(in->text[begin]) ||
        !preceding_label(in, begin, label, sizeof(label), &layer))
        return 0;
    while (end < in->length && decimal_digit(in->text[end]))
        ++end;
    while (end + 1 < in->length && in->text[end] == '-' && decimal_digit(in->text[end + 1])) {
        ++end;
        while (end < in->length && decimal_digit(in->text[end]))
            ++end;
    }
    if (!valid_tail(in, end) || end - begin >= sizeof(semantic->value))
        return 0;
    memcpy(semantic->value, in->text + begin, end - begin);
    semantic->value[end - begin] = '\0';
    (void)snprintf(semantic->secondary, sizeof(semantic->secondary), "%s", label);
    (void)snprintf(semantic->unit, sizeof(semantic->unit), "financial_identifier");
    (void)snprintf(semantic->rule, sizeof(semantic->rule), "ko:labeled-identifier");
    semantic->type = TTF_SEM_DIGIT_SEQUENCE;
    semantic->input_end = end;
    semantic->layer = layer;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < end; ++i) {
    }
    return i - in->index;
}

ttf_status_t ttf_ko_render_identifier(const ttf_engine_t *engine, const char *value,
                                      ttf_buffer_t *output) {
    static const char *const digits[] = {"공", "일", "이", "삼", "사",
                                         "오", "육", "칠", "팔", "구"};
    for (; *value; ++value) {
        const char *rendered;
        ttf_status_t status;
        if (*value == '-') {
            rendered = ttf_resource_get(engine, "identifier_render", "ko-KR", "separator", NULL);
            if (!rendered)
                rendered = " ";
        } else if (decimal_digit(*value)) {
            char key[2] = {*value, '\0'};
            rendered = *value == '0'
                           ? ttf_resource_get(engine, "identifier_render", "ko-KR", "zero", NULL)
                           : ttf_resource_get(engine, "digit", "ko-KR", key, NULL);
            if (!rendered)
                rendered = digits[(unsigned)(*value - '0')];
        } else
            return TTF_EINVAL;
        status = ttf_buffer_puts(output, rendered);
        if (status != TTF_OK)
            return status;
    }
    return TTF_OK;
}
