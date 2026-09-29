#include "identifiers.h"
#include "../../src/core/internal.h"
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
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
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

static int ascii_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int alnum(char c) {
    return ascii_letter(c) || decimal_digit(c);
}

/* The previous word (skipping spaces and opening quotes) or the text right
 * after the code is Hangul: the code is part of a Korean sentence. */
static int korean_neighbour(const ttf_pack_input_t *in, size_t end) {
    size_t i;
    for (i = in->index + 1; i < in->token_count; ++i) {
        const ttf_token_t *token = &in->tokens[i];
        size_t p;
        if (token->begin < end)
            continue;
        if (token->script == TTF_SCRIPT_WHITESPACE) {
            for (p = token->begin; p < token->end; ++p)
                if (in->text[p] == '\n' || in->text[p] == '\r')
                    break;
            if (p < token->end)
                break;
            continue; /* A380 항공기 */
        }
        if (token->script == TTF_SCRIPT_HANGUL)
            return 1;
        if (token->script == TTF_SCRIPT_PUNCTUATION && i + 1 < in->token_count &&
            in->tokens[i + 1].script == TTF_SCRIPT_HANGUL &&
            in->tokens[i + 1].begin == token->end)
            return 1; /* 'MU32V1'을 */
        break;
    }
    for (i = in->index; i > 0; --i) {
        const ttf_token_t *token = &in->tokens[i - 1];
        size_t p;
        if (token->script == TTF_SCRIPT_WHITESPACE) {
            for (p = token->begin; p < token->end; ++p)
                if (in->text[p] == '\n' || in->text[p] == '\r')
                    return 0;
            continue;
        }
        if (token->script == TTF_SCRIPT_PUNCTUATION && token->end - token->begin <= 3 &&
            in->text[token->begin] != '-' && in->text[token->begin] != '.' &&
            in->text[token->begin] != '@')
            continue; /* an opening quote or bracket */
        return token->script == TTF_SCRIPT_HANGUL;
    }
    return 0;
}

size_t ttf_ko_classify_alnum_identifier(const ttf_pack_input_t *in, ttf_semantic_t *semantic) {
    size_t begin = in->tokens[in->index].begin, end = begin, i, run = 0;
    size_t letters = 0, digits = 0, hyphens = 0, transitions = 0;
    int kind = 0, upper = 0, lower = 0; /* kind: 1 letter, 2 digit */
    char before, after;
    if (begin == in->length || !alnum(in->text[begin]))
        return 0;
    before = begin ? in->text[begin - 1] : ' ';
    if (alnum(before) || before == '-' || before == '.' || before == '@' || before == '_' ||
        before == '/' || before == ':' || before == '#' || before == '+' || before == ',' ||
        before == '&')
        return 0;
    for (;;) {
        char c = end < in->length ? in->text[end] : '\0';
        if (c && alnum(c)) {
            int now = ascii_letter(c) ? 1 : 2;
            if (kind && now != kind)
                ++transitions;
            if (now != kind || !run) {
                upper = lower = 0;
                run = 0;
            }
            kind = now;
            ++run;
            if (now == 1) {
                ++letters;
                upper |= c >= 'A' && c <= 'Z';
                lower |= c >= 'a' && c <= 'z';
                /* A mixed-case or long letter run is a word (iPhone15, Boeing747). */
                if ((upper && lower) || run > 4)
                    return 0;
            } else
                ++digits;
            ++end;
        } else if (end + 1 < in->length && c == '-' && alnum(in->text[end + 1]) && end > begin) {
            ++hyphens;
            run = 0;
            ++end;
        } else
            break;
    }
    if (!letters || !digits || end - begin >= sizeof(semantic->value))
        return 0;
    after = end < in->length ? in->text[end] : ' ';
    if (after == '@' || after == '_' || after == '%' ||
        ((after == '.' || after == ',' || after == '/' || after == ':') && end + 1 < in->length &&
         alnum(in->text[end + 1])))
        return 0;
    /* Codes, not words or quantities: two letter/digit changes (K37PA1798,
     * 50QNA90), letters then at least three digits (M4137, kip214), or
     * hyphenated groups with at least three digits (C5135-12, AP-1019D). */
    if (!((transitions >= 2 && end - begin >= 5) ||
          (ascii_letter(in->text[begin]) && transitions == 1 && !hyphens && digits >= 3) ||
          (hyphens && digits >= 3)))
        return 0;
    /* A letter run that is a Korean loanword is a word with a number
     * (top100, TOP100 탑백), not a code; two-letter runs stay codes. */
    for (i = begin; i < end;) {
        size_t start = i;
        int lowercase = 1;
        char key[TTF_IR_VALUE_SIZE + 8];
        while (i < end && ascii_letter(in->text[i])) {
            lowercase &= in->text[i] >= 'a' && in->text[i] <= 'z';
            ++i;
        }
        if (i > start && (lowercase || i - start >= 3) && i - start < TTF_IR_VALUE_SIZE) {
            size_t k, used = (size_t)snprintf(key, sizeof(key), "en-US:");
            for (k = start; k < i; ++k)
                key[used++] = (char)(in->text[k] >= 'A' && in->text[k] <= 'Z' ? in->text[k] + 32
                                                                                   : in->text[k]);
            key[used] = 0;
            if (ttf_resource_get(in->engine, "lexicon", "ko-KR", key, NULL))
                return 0;
        }
        if (i == start)
            ++i;
    }
    for (i = in->index; i < in->token_count && in->tokens[i].begin < end; ++i) {
    }
    if (in->tokens[i - 1].end != end || !korean_neighbour(in, end))
        return 0;
    memcpy(semantic->value, in->text + begin, end - begin);
    semantic->value[end - begin] = '\0';
    (void)snprintf(semantic->unit, sizeof(semantic->unit), "financial_identifier");
    (void)snprintf(semantic->rule, sizeof(semantic->rule), "ko:alnum-identifier");
    semantic->type = TTF_SEM_DIGIT_SEQUENCE;
    semantic->input_end = end;
    semantic->layer = TTF_LAYER_COMMON;
    return i - in->index;
}

/* Four space-separated groups of four digits next to Korean text are a card
 * number (0066 4144 0087 5301): read digit by digit like an identifier. A
 * fifth group, a unit or counter after it, or an attached number rejects it. */
size_t ttf_ko_classify_card(const ttf_pack_input_t *in, ttf_semantic_t *semantic) {
    size_t begin = in->tokens[in->index].begin, p = begin, group, i;
    char before;
    if (begin == in->length || !decimal_digit(in->text[begin]))
        return 0;
    before = begin ? in->text[begin - 1] : ' ';
    if (alnum(before) || before == '-' || before == '.' || before == ',' || before == '+' ||
        before == '/' || before == ':')
        return 0;
    for (group = 0; group < 4; ++group) {
        size_t digits = 0;
        if (group) {
            if (p >= in->length || in->text[p] != ' ')
                return 0;
            ++p;
        }
        while (p < in->length && decimal_digit(in->text[p]) && digits < 5) {
            ++p;
            ++digits;
        }
        if (digits != 4)
            return 0;
    }
    if (p + 1 < in->length && in->text[p] == ' ' && decimal_digit(in->text[p + 1]))
        return 0;
    if (!valid_tail(in, p) || p - begin >= sizeof(semantic->value))
        return 0;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < p; ++i) {
    }
    if (in->tokens[i - 1].end != p || !korean_neighbour(in, p))
        return 0;
    memcpy(semantic->value, in->text + begin, p - begin);
    semantic->value[p - begin] = '\0';
    (void)snprintf(semantic->unit, sizeof(semantic->unit), "financial_identifier");
    (void)snprintf(semantic->rule, sizeof(semantic->rule), "ko:card-number");
    semantic->type = TTF_SEM_DIGIT_SEQUENCE;
    semantic->input_end = p;
    semantic->layer = TTF_LAYER_COMMON;
    return i - in->index;
}

/* Digits one by one, Latin letters by their Korean names; a letter run and a
 * digit run are separated by a space. */
ttf_status_t ttf_ko_render_identifier(const ttf_engine_t *engine, const char *value,
                                      ttf_buffer_t *output) {
    static const char *const digits[] = {"공", "일", "이", "삼", "사",
                                         "오", "육", "칠", "팔", "구"};
    int previous = 0; /* 1 letter, 2 digit */
    const char *style = ttf_resource_get(engine, "number_spacing", "ko-KR", "digits", NULL);
    int spaced = engine->reading_unit_spacing || (style && !strcmp(style, "spaced")); /* 케이 삼 칠 */
    for (; *value; ++value) {
        const char *rendered;
        ttf_status_t status;
        int now = ascii_letter(*value) ? 1 : decimal_digit(*value) ? 2 : 0;
        if (now && previous && (now != previous || spaced) &&
            (status = ttf_buffer_puts(output, " ")) != TTF_OK)
            return status;
        previous = now;
        if (now == 1) {
            char key[] = "letter.X";
            key[7] = (char)(*value >= 'a' ? *value - 'a' + 'A' : *value);
            rendered = ttf_resource_get(engine, "phone_letter", "ko-KR", key, NULL);
            if (!rendered)
                return TTF_ERESOURCE;
        } else if (*value == ' ') {
            rendered = " "; /* card number group boundary */
        } else if (*value == '-') {
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
