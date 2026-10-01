#include "identifiers.h"
#include "../../src/core/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int horizontal_space(char c) {
    return c == ' ' || c == '\t';
}

static int decimal_digit(char c) {
    return c >= '0' && c <= '9';
}

/* 예매번호, 주민번호, 항공 번호: a word ending in 번호 names a number to read digit by
 * digit, unless it names a phone number (전화번호, 휴대폰번호), which keeps its own
 * grammar. A declared identifier_label (also disabled ones) takes precedence. */
static int number_word(const char *key, size_t n) {
    static const char suffix[] = "\xeb\xb2\x88\xed\x98\xb8"; /* 번호 */
    /* Bare 번호 names nothing in particular; 예매번호, 항공 번호 do. */
    if (n <= 6 || memcmp(key + n - 6, suffix, 6))
        return 0;
    return !strstr(key, "\xec\xa0\x84\xed\x99\x94") /* 전화 */ &&
           !strstr(key, "\xed\x8f\xb0"); /* 폰 */
}
/* The label as written may carry a particle (주문번호를, 예매번호가). */
static const char *label_value(const ttf_pack_input_t *in, char *key, size_t used,
                               ttf_resource_layer_t *layer, int *generic) {
    const char *value = ttf_resource_get(in->engine, "identifier_label", "ko-KR", key, layer);
    size_t cut;
    *generic = 0;
    if (value)
        return value;
    for (cut = 3; cut <= 6 && cut < used; cut += 3) {
        char saved = key[used - cut];
        if (!ttf_resource_get(in->engine, "particle", "ko-KR", key + used - cut, NULL))
            continue;
        key[used - cut] = '\0';
        value = ttf_resource_get(in->engine, "identifier_label", "ko-KR", key, layer);
        if (!value && number_word(key, used - cut)) {
            value = "digits";
            *layer = TTF_LAYER_COMMON;
            *generic = 1;
        }
        key[used - cut] = saved;
        if (value)
            return value;
    }
    if (number_word(key, used)) {
        *layer = TTF_LAYER_COMMON;
        *generic = 1;
        return "digits";
    }
    return NULL;
}

/* The tokenizer supplies starts so a cue cannot match inside a longer word.
 * Canonical keys use one ASCII space; repeated spaces and tabs are equivalent.
 * Newlines, punctuation and quotes terminate label scope. */
static int preceding_label(const ttf_pack_input_t *in, size_t begin, char *matched, size_t capacity,
                           ttf_resource_layer_t *layer) {
    size_t end = begin, i;
    int found = 0, enabled = 0, declared = 0;
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
        int generic;
        value = label_value(in, key, used, &candidate_layer, &generic);
        /* A declared cue (참조 번호는) is not extended by a longer generic 번호
           word around it (거래 참조 번호는). */
        if (value && generic && declared)
            value = NULL;
        if (value) {
            declared |= !generic;
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

static size_t classified(const ttf_pack_input_t *in, ttf_semantic_t *s, size_t end,
                         ttf_semantic_type_t type, const char *rule) {
    size_t consumed = 0;
    while (in->index + consumed < in->token_count && in->tokens[in->index + consumed].begin < end)
        ++consumed;
    s->type = type;
    s->input_end = end;
    (void)snprintf(s->rule, sizeof(s->rule), "%s", rule);
    return consumed;
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

/* 비밀번호 7537번: a secret or ticket code (short_code_label) is read digit by
 * digit even when short; other labels keep a short queue number as a count
 * (예약번호 89번 팔십구 번). The last word of the label is tried as written and
 * without one or two final syllables (비밀번호가, 비밀번호인). */
static int short_code_label(const ttf_pack_input_t *in, const char *label) {
    const char *word = strrchr(label, ' ');
    char key[64];
    size_t n, cut;
    word = word ? word + 1 : label;
    n = strlen(word);
    if (n >= sizeof(key))
        return 0;
    for (cut = 0; cut <= 6 && cut < n; cut += 3) {
        memcpy(key, word, n - cut);
        key[n - cut] = '\0';
        if (ttf_resource_get(in->engine, "short_code_label", "ko-KR", key, NULL))
            return 1;
    }
    return 0;
}

/* After a labelled number: a particle, the copula (2021034인, 210547이다) or the
 * counter 번 (63107508번이). */
static int labelled_tail(const ttf_pack_input_t *in, size_t end) {
    static const char *const heads[] = {"\xec\x9d\xb4", "\xec\x9d\xb8", "\xec\x9e\x85",
                                        "\xeb\xb2\x88"}; /* 이 인 입 번 */
    size_t k;
    if (valid_tail(in, end))
        return 1;
    for (k = 0; k < sizeof(heads) / sizeof(heads[0]); ++k)
        if (end + 3 <= in->length && !memcmp(in->text + end, heads[k], 3))
            return 1;
    return 0;
}
/* 56478910이 예약번호이며: a code of five or more digits, a particle, then a
 * word that names a number (number_word) right after it. */
static int following_label(const ttf_pack_input_t *in, size_t end, char *matched, size_t capacity) {
    size_t i, p;
    for (i = in->index; i < in->token_count && in->tokens[i].end <= end; ++i) {
    }
    if (i < in->token_count && in->tokens[i].begin == end &&
        in->tokens[i].script != TTF_SCRIPT_WHITESPACE) {
        const ttf_token_t *t = &in->tokens[i];
        char key[64];
        if (t->script != TTF_SCRIPT_HANGUL || t->end - end >= sizeof(key))
            return 0;
        memcpy(key, in->text + end, t->end - end);
        key[t->end - end] = '\0';
        if (!ttf_resource_get(in->engine, "particle", "ko-KR", key, NULL))
            return 0;
        ++i;
    }
    if (i >= in->token_count || in->tokens[i].script != TTF_SCRIPT_WHITESPACE ||
        in->tokens[i].end - in->tokens[i].begin != 1)
        return 0;
    ++i;
    if (i >= in->token_count || in->tokens[i].script != TTF_SCRIPT_HANGUL)
        return 0;
    for (p = in->tokens[i].begin + 6; p <= in->tokens[i].end; p += 3) {
        size_t n = p - in->tokens[i].begin;
        if (n < capacity && n < 64) {
            char key[64];
            memcpy(key, in->text + in->tokens[i].begin, n);
            key[n] = '\0';
            if (number_word(key, n)) {
                memcpy(matched, key, n + 1);
                return 1;
            }
        }
    }
    return 0;
}

size_t ttf_ko_classify_identifier(const ttf_pack_input_t *in, ttf_semantic_t *semantic) {
    size_t begin = in->tokens[in->index].begin, end = begin, i;
    char label[TTF_IR_VALUE_SIZE];
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    int before;
    if (begin == in->length || !decimal_digit(in->text[begin]))
        return 0;
    before = preceding_label(in, begin, label, sizeof(label), &layer);
    while (end < in->length && decimal_digit(in->text[end]))
        ++end;
    if (!before && !(end - begin >= 5 && !(end < in->length && in->text[end] == '-') &&
                     following_label(in, end, label, sizeof(label))))
        return 0;
    /* A word ending in 번호 that is not a declared label only marks a code of
       four or more digits (번호 3번을 keeps its counter reading). */
    if (end - begin < 4 && !ttf_resource_get(in->engine, "identifier_label", "ko-KR", label, NULL))
        return 0;
    while (end + 1 < in->length && in->text[end] == '-' && decimal_digit(in->text[end + 1])) {
        ++end;
        while (end < in->length && decimal_digit(in->text[end]))
            ++end;
    }
    if (!labelled_tail(in, end) || end - begin >= sizeof(semantic->value))
        return 0;
    memcpy(semantic->value, in->text + begin, end - begin);
    semantic->value[end - begin] = '\0';
    /* 63107508번이: the counter joins a code of five or more digits, so the
       shared counter reading (육천삼백십만… 번) does not take a longer span; it
       is read back after it. A short queue number keeps its counter reading
       (예약번호 89번 팔십구 번). */
    if (end + 3 <= in->length && !memcmp(in->text + end, "\xeb\xb2\x88", 3)) {
        size_t digits = 0;
        for (const char *c = semantic->value; *c; ++c)
            digits += decimal_digit(*c);
        if (digits < 5 && !short_code_label(in, label))
            return 0;
        end += 3;
    }
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

/* A vehicle plate (25후 5748): two or three digits, one declared plate syllable,
 * an optional space and a four-digit serial. The serial is read digit by digit
 * (오칠사팔); the front number keeps its value reading. A counter after the
 * serial (34가 1234명) is a quantity, not a plate. Speech output only. */
static int plate_tail(const ttf_pack_input_t *in, size_t end) {
    size_t i;
    if (valid_tail(in, end))
        return 1;
    for (i = in->index; i < in->token_count; ++i)
        if (in->tokens[i].begin == end && in->tokens[i].script == TTF_SCRIPT_HANGUL) {
            size_t n = in->tokens[i].end - end;
            char key[TTF_IR_VALUE_SIZE];
            if (n < 3 || memcmp(in->text + end, "번", 3) || n - 3 >= sizeof(key))
                return 0;
            if (n == 3)
                return 1;
            memcpy(key, in->text + end + 3, n - 3);
            key[n - 3] = '\0';
            return ttf_resource_get(in->engine, "particle", "ko-KR", key, NULL) != NULL;
        }
    return 0;
}

size_t ttf_ko_classify_plate(const ttf_pack_input_t *in, ttf_semantic_t *semantic) {
    size_t begin = in->tokens[in->index].begin, p = begin, front, serial, i, used;
    char before, letter[4];
    const char *flag;
    if (begin == in->length || !decimal_digit(in->text[begin]))
        return 0;
    before = begin ? in->text[begin - 1] : ' ';
    if (alnum(before) || before == '-' || before == '.' || before == ',' || before == '+')
        return 0;
    while (p < in->length && decimal_digit(in->text[p]))
        ++p;
    front = p - begin;
    if (front < 2 || front > 3 || p + 3 > in->length ||
        (unsigned char)in->text[p] < 0xEA || (unsigned char)in->text[p] > 0xED)
        return 0;
    memcpy(letter, in->text + p, 3);
    letter[3] = '\0';
    flag = ttf_resource_get(in->engine, "vehicle_plate_letter", "ko-KR", letter, NULL);
    if (!flag || strcmp(flag, "1"))
        return 0;
    p += 3;
    if (p < in->length && in->text[p] == ' ')
        ++p;
    for (serial = p; p < in->length && decimal_digit(in->text[p]); ++p) {
    }
    if (p - serial != 4 || (p < in->length && (alnum(in->text[p]) || in->text[p] == '-')) ||
        !plate_tail(in, p))
        return 0;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < p; ++i) {
    }
    if (in->tokens[i - 1].end != p)
        return 0;
    used = 0;
    memcpy(semantic->value, in->text + begin, front + 3);
    used = front + 3;
    memcpy(semantic->value + used, in->text + serial, 4);
    semantic->value[used + 4] = '\0';
    (void)snprintf(semantic->unit, sizeof(semantic->unit), "vehicle_plate");
    (void)snprintf(semantic->rule, sizeof(semantic->rule), "ko:vehicle-plate");
    semantic->type = TTF_SEM_DIGIT_SEQUENCE;
    semantic->input_end = p;
    semantic->layer = TTF_LAYER_COMMON;
    return i - in->index;
}

/* A number with an attached Latin unit that no shared grammar reads (75t,
 * 3,000P, 4Km, 2,000cc) reached TTS as raw digits. Speech output reads a
 * declared Korean letter unit (letter_unit: t -> t, P -> point) as a measure
 * next to Korean text. Other letters are labels and keep their own policy. */
size_t ttf_ko_classify_number_letters(const ttf_pack_input_t *in, ttf_semantic_t *semantic) {
    size_t begin = in->tokens[in->index].begin, p = begin, letters, i, used = 0, digits = 0;
    char before, key[8];
    const char *unit;
    int decimal = 0;
    if (begin == in->length || !decimal_digit(in->text[begin]))
        return 0;
    before = begin ? in->text[begin - 1] : ' ';
    if (alnum(before) || before == '-' || before == '.' || before == ',' || before == '+' ||
        before == '@' || before == '/' || before == ':')
        return 0;
    while (p < in->length) {
        char c = in->text[p];
        if (decimal_digit(c)) {
            if (used + 1 >= sizeof(semantic->value))
                return 0;
            semantic->value[used++] = c;
            ++digits;
            ++p;
        } else if ((c == ',' || (c == '.' && !decimal)) && p + 1 < in->length &&
                   decimal_digit(in->text[p + 1]) && p > begin) {
            if (c == '.') {
                decimal = 1;
                semantic->value[used++] = c;
            }
            ++p;
        } else
            break;
    }
    semantic->value[used] = '\0';
    for (letters = 0; p + letters < in->length && ascii_letter(in->text[p + letters]); ++letters) {
    }
    if (!letters || letters >= sizeof(key) || (p + letters < in->length &&
        (decimal_digit(in->text[p + letters]) || in->text[p + letters] == '@' ||
         in->text[p + letters] == '_' || in->text[p + letters] == '-' ||
         ((in->text[p + letters] == '.' || in->text[p + letters] == '/') &&
          p + letters + 1 < in->length && alnum(in->text[p + letters + 1])))))
        return 0;
    memcpy(key, in->text + p, letters);
    key[letters] = '\0';
    /* An English ordinal (1st, 22nd) keeps its reading. */
    if (letters == 2 && (!strcmp(key, "st") || !strcmp(key, "nd") || !strcmp(key, "rd") ||
                         !strcmp(key, "th") || !strcmp(key, "ST") || !strcmp(key, "ND") ||
                         !strcmp(key, "RD") || !strcmp(key, "TH")))
        return 0;
    /* Shared units (10kg) already read as measures; a lexicon term (3D -> 쓰리디)
       keeps its word reading. */
    if (ttf_resource_get(in->engine, "numeric_suffix", "*", key, NULL))
        return 0;
    if (!decimal && p + letters - begin < 40) {
        char term[48];
        size_t k, t = (size_t)snprintf(term, sizeof(term), "en-US:");
        for (k = begin; k < p + letters; ++k)
            if (in->text[k] != ',')
                term[t++] = (char)(in->text[k] >= 'A' && in->text[k] <= 'Z' ? in->text[k] + 32 : in->text[k]);
        term[t] = '\0';
        if (ttf_resource_get(in->engine, "lexicon", "ko-KR", term, NULL))
            return 0;
    }
    unit = ttf_resource_get(in->engine, "letter_unit", "ko-KR", key, NULL);
    /* Only declared units: other letters (20F, 4K, 45A) are ambiguous labels
       with their own preservation and unreadable-read policy. */
    if (!unit)
        return 0;
    /* A declared value list (G: 8|16|...|2048) limits a letter to its values. */
    {
        const char *values = ttf_resource_get(in->engine, "letter_unit_values", "ko-KR", key, NULL);
        if (values) {
            size_t length = strlen(semantic->value);
            const char *at = values;
            int listed = 0;
            while (*at && !listed) {
                size_t width = strcspn(at, "|");
                listed = width == length && !strncmp(at, semantic->value, length);
                at += width + (at[width] == '|');
            }
            if (!listed)
                return 0;
        }
    }
    p += letters;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < p; ++i) {
    }
    if (in->tokens[i - 1].end != p || !korean_neighbour(in, p))
        return 0;
    semantic->type = TTF_SEM_MEASURE;
    (void)snprintf(semantic->unit, sizeof(semantic->unit), "%s", unit);
    (void)snprintf(semantic->rule, sizeof(semantic->rule), "ko:letter-unit");
    semantic->input_end = p;
    semantic->layer = TTF_LAYER_COMMON;
    return i - in->index;
}

/* Some Hangul in the unit: an apparel size belongs to a Korean sentence. */
static int unit_has_hangul(const ttf_pack_input_t *in) {
    size_t i;
    for (i = 0; i < in->token_count; ++i)
        if (in->tokens[i].script == TTF_SCRIPT_HANGUL)
            return 1;
    return 0;
}
/* Apparel sizes in a Korean sentence (AI Hub 159 size references):
 *   3XL 쓰리 엑스라지, 2XS 투 엑스스몰: a multiplier 2..6 (apparel_size_multiplier)
 *   before XL/XS; XL XS XXL XXS XXXL XXXS read as apparel_size words;
 *   75AA 칠십오 에이에이: a declared bra band (bra_band) and cup (bra_cup).
 * The size must stand alone: no letter or digit on either side (particles may
 * follow). The general number+letter policy (20F, 45A) is unchanged. */
size_t ttf_ko_classify_apparel_size(const ttf_pack_input_t *in, ttf_semantic_t *semantic) {
    const char *s = in->text;
    size_t n = in->length, begin = in->tokens[in->index].begin, p = begin, i, used = 0;
    char key[16], before = begin ? s[begin - 1] : ' ';
    const char *unit = NULL;
    if (alnum(before) || !unit_has_hangul(in))
        return 0;
    if (decimal_digit(s[p])) {
        size_t digits = 0;
        while (p < n && decimal_digit(s[p]) && digits < 4) {
            ++p;
            ++digits;
        }
        if (p < n && decimal_digit(s[p]))
            return 0;
        /* 3XL: one multiplier digit, then a size word. */
        /* 2xl reads like 2XL. */
        if (digits == 1 && p + 2 <= n && (s[p] == 'X' || s[p] == 'x') &&
            (s[p + 1] == 'L' || s[p + 1] == 'S' || s[p + 1] == 'l' || s[p + 1] == 's') &&
            (p + 2 == n || !alnum(s[p + 2]))) {
            key[0] = s[begin];
            key[1] = 0;
            if (!ttf_resource_get(in->engine, "apparel_size_multiplier", "ko-KR", key, NULL))
                return 0;
            p += 2;
            unit = "apparel_size";
        } else {
            size_t cup = p;
            if (p - begin >= sizeof(key))
                return 0;
            memcpy(key, s + begin, p - begin);
            key[p - begin] = 0;
            if (!ttf_resource_get(in->engine, "bra_band", "ko-KR", key, NULL))
                return 0;
            while (cup < n && s[cup] >= 'A' && s[cup] <= 'Z' && cup - p < 3)
                ++cup;
            if (cup == p || (cup < n && alnum(s[cup])) || cup - p >= sizeof(key))
                return 0;
            memcpy(key, s + p, cup - p);
            key[cup - p] = 0;
            if (!ttf_resource_get(in->engine, "bra_cup", "ko-KR", key, NULL))
                return 0;
            p = cup;
            unit = "bra_size";
        }
    } else {
        while (p < n && ascii_letter(s[p]) && p - begin < 5)
            ++p;
        if (p - begin >= sizeof(key) || (p < n && alnum(s[p])))
            return 0;
        memcpy(key, s + begin, p - begin);
        key[p - begin] = 0;
        if (!ttf_resource_get(in->engine, "apparel_size", "ko-KR", key, NULL))
            return 0;
        unit = "apparel_size";
    }
    if (p - begin >= TTF_IR_VALUE_SIZE)
        return 0;
    memcpy(semantic->value, s + begin, p - begin);
    semantic->value[p - begin] = 0;
    if (decimal_digit(semantic->value[0]) && !strcmp(unit, "apparel_size"))
        for (i = 1; semantic->value[i]; ++i)
            if (semantic->value[i] >= 'a' && semantic->value[i] <= 'z')
                semantic->value[i] = (char)(semantic->value[i] - 'a' + 'A');
    (void)snprintf(semantic->unit, sizeof(semantic->unit), "%s", unit);
    (void)snprintf(semantic->rule, sizeof(semantic->rule), "ko:%s", !strcmp(unit, "bra_size")
                                                                    ? "bra-size" : "apparel-size");
    semantic->type = TTF_SEM_DIGIT_SEQUENCE;
    semantic->input_end = p;
    semantic->layer = TTF_LAYER_COMMON;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < p; ++i)
        ++used;
    return used;
}

/* Korean women's clothing sizes 44..99 (two equal digits) before 사이즈 are
 * read digit by digit: 55사이즈 -> 오오사이즈. Men's sizes (95, 100) read as
 * values. The word after the number is a resource (clothing_size_word). */
size_t ttf_ko_classify_clothing_size(const ttf_pack_input_t *in, ttf_semantic_t *semantic) {
    size_t begin = in->tokens[in->index].begin, p = begin + 2, i;
    const ttf_token_t *next = NULL;
    char word[TTF_IR_VALUE_SIZE];
    char before = begin ? in->text[begin - 1] : ' ';
    if (p > in->length || in->tokens[in->index].end != p || in->text[begin] != in->text[begin + 1] ||
        in->text[begin] < '4' || in->text[begin] > '9' || alnum(before) || before == '.' ||
        before == ',' || before == '-')
        return 0;
    for (i = in->index + 1; i < in->token_count && i <= in->index + 2; ++i)
        if (in->tokens[i].script == TTF_SCRIPT_HANGUL) {
            next = &in->tokens[i];
            break;
        } else if (in->tokens[i].script != TTF_SCRIPT_WHITESPACE ||
                   in->tokens[i].end - in->tokens[i].begin != 1)
            return 0;
    if (!next)
        return 0;
    for (i = 1; i <= next->end - next->begin && i < sizeof(word); ++i) {
        const char *flag;
        memcpy(word, in->text + next->begin, i);
        word[i] = '\0';
        flag = ttf_resource_get(in->engine, "clothing_size_word", "ko-KR", word, NULL);
        if (flag && !strcmp(flag, "1")) {
            memcpy(semantic->value, in->text + begin, 2);
            semantic->value[2] = '\0';
            (void)snprintf(semantic->unit, sizeof(semantic->unit), "financial_identifier");
            (void)snprintf(semantic->rule, sizeof(semantic->rule), "ko:clothing-size");
            semantic->type = TTF_SEM_DIGIT_SEQUENCE;
            semantic->input_end = p;
            semantic->layer = TTF_LAYER_COMMON;
            return 1;
        }
    }
    return 0;
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

static int latin_letter(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static size_t digit_run(const char *text, size_t n, size_t p) {
    while (p < n && decimal_digit(text[p]))
        ++p;
    return p;
}

static const char *korean_policy(const ttf_pack_input_t *in, const char *key) {
    return ttf_resource_get(in->engine, "policy", "ko-KR", key, NULL);
}

/* The quantity word right after a number, if it really ends there (a particle
 * or a word boundary follows): 'U' unit, currency, percent or scale word
 * (100-200만 원), 'C' counter, 'L' a counter that labels identifiers
 * (digit_label_counter: 1234-5678번, 2020-1234호), 0 none. After a space the
 * word must be complete, so 10-20 시즌 and 27022-8080 통장 take no counter.
 * 'S' is a scale word after the number (만, 억). */
static int hangul_syllable_at(const ttf_pack_input_t *in, size_t p) {
    const unsigned char *u = (const unsigned char *)in->text + p;
    unsigned cp;
    if (p + 3 > in->length || (u[0] & 0xf0) != 0xe0)
        return 0;
    cp = ((unsigned)(u[0] & 0x0f) << 12) | ((unsigned)(u[1] & 0x3f) << 6) | (unsigned)(u[2] & 0x3f);
    return cp >= 0xac00 && cp <= 0xd7a3;
}

static int quantity_word(const ttf_pack_input_t *in, size_t p, int attached) {
    size_t q = p, n;
    char key[64];
    while (q < in->length && horizontal_space(in->text[q]))
        ++q;
    if (q < in->length && in->text[q] == '%')
        return valid_tail(in, q + 1) ? 'U' : 0;
    n = in->length - q;
    if (n >= sizeof(key))
        n = sizeof(key) - 1;
    /* 12000-15000 사이: a word that itself means an interval. */
    for (size_t m = n; m; --m) {
        memcpy(key, in->text + q, m);
        key[m] = '\0';
        if (ttf_resource_get(in->engine, "digit_range_cue", "ko-KR", key, NULL))
            return valid_tail(in, q + m) ? 'U' : 0;
    }
    if (n >= sizeof(key))
        n = sizeof(key) - 1;
    for (; n; --n) {
        const char *value;
        memcpy(key, in->text + q, n);
        key[n] = '\0';
        value = ttf_resource_get(in->engine, "numeric_suffix", "*", key, NULL);
        if (!value && ttf_resource_get(in->engine, "digit_range_scale", "ko-KR", key, NULL))
            return 'S'; /* 100-200만 원, 01-02만원: the shared-scale grammar reads it */
        if (!value)
            continue;
        if (!valid_tail(in, q + n)) {
            if (q > p)
                return 0;
            break; /* 7-10일간: an attached word, judged below */
        }
        if (!strncmp(value, "COUNTER:", 8))
            return ttf_resource_get(in->engine, "digit_label_counter", "ko-KR", value + 8, NULL) ? 'L'
                                                                                               : 'C';
        return 'U';
    }
    /* A Hangul word attached to the number that is not a particle is a counter
     * or unit the shared grammar knows (3-5세, 2-3회, 5-10위안, 2-32점); a
     * particle (12-34를) is not. Only for hyphen pairs: after a long code an
     * attached ending (326253620012더라) is not a counter. */
    if (attached && q == p && hangul_syllable_at(in, q) && !valid_tail(in, q))
        return 'C';
    return 0;
}

/* Whether the shared grammar already rejects a bare hyphen pair as a range
 * (classifier.c identifier_pair) and something follows that it could take as a
 * counter or unit; then that path, and the labelled identifier cue after it
 * (계좌번호 3765-38264), keep the pair. */
static int shared_identifier_pair(size_t la, size_t lb, const char *a, const char *b) {
    if ((la > 1 && a[0] == '0') || (lb > 1 && b[0] == '0') || la >= 6 || lb >= 6)
        return 1;
    return la != lb && (la >= 5 || lb >= 5);
}

static int raw_suffix(const ttf_pack_input_t *in, size_t p) {
    size_t q = p, n;
    char key[64];
    while (q < in->length && horizontal_space(in->text[q]))
        ++q;
    if (q < in->length && in->text[q] == '%')
        return 1;
    n = in->length - q;
    if (n >= sizeof(key))
        n = sizeof(key) - 1;
    for (; n; --n) {
        memcpy(key, in->text + q, n);
        key[n] = '\0';
        if (ttf_resource_get(in->engine, "numeric_suffix", "*", key, NULL))
            return 1;
    }
    return 0;
}

/* A game result in the sentence (score_cue: a word prefix such as 승리, 졌, 비겼,
 * 무승부, 동점, 꺾): a short hyphen pair then is a score, 25-18 이십오 대 십팔. */
static int score_sentence(const ttf_pack_input_t *in) {
    size_t i;
    for (i = 0; i < in->token_count; ++i) {
        const ttf_token_t *token = &in->tokens[i];
        size_t width;
        char key[64];
        if (token->script != TTF_SCRIPT_HANGUL)
            continue;
        for (width = token->end - token->begin; width > 0; --width) {
            if (width >= sizeof(key))
                continue;
            memcpy(key, in->text + token->begin, width);
            key[width] = '\0';
            if (ttf_resource_get(in->engine, "score_cue", "ko-KR", key, NULL))
                return 1;
        }
    }
    return 0;
}

/* 아이폰6 대란, 갤럭시노트10, 아이폰 14: a number of one or two digits right
 * after a declared product name (model_name), alone or one space apart, is a
 * model number. Up to ten it is read with English numerals (아이폰식스,
 * 노트텐), from eleven as a Sino-Korean number (아이폰 십사). A counter or
 * unit attached to the number (아이폰 2대) makes it a quantity. */
size_t ttf_ko_classify_model_number(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text;
    size_t n = in->length, b = in->tokens[in->index].begin, e = b, word;
    char key[64];
    if (in->index == 0 || !decimal_digit(t[b]))
        return 0;
    while (e < n && decimal_digit(t[e]))
        ++e;
    if (e - b > 2 || (e < n && (alnum(t[e]) || t[e] == '.' || t[e] == ',' || t[e] == '-' ||
                                t[e] == ':' || t[e] == '/')))
        return 0;
    word = in->index - 1;
    if (in->tokens[word].script == TTF_SCRIPT_WHITESPACE) {
        if (word == 0 || in->tokens[word].end - in->tokens[word].begin != 1 || t[in->tokens[word].begin] != ' ')
            return 0;
        --word;
    }
    if (in->tokens[word].script != TTF_SCRIPT_HANGUL ||
        in->tokens[word].end - in->tokens[word].begin >= sizeof(key))
        return 0;
    memcpy(key, t + in->tokens[word].begin, in->tokens[word].end - in->tokens[word].begin);
    key[in->tokens[word].end - in->tokens[word].begin] = '\0';
    if (!ttf_resource_get(in->engine, "model_name", "ko-KR", key, NULL) || quantity_word(in, e, 1))
        return 0;
    memcpy(s->value, t + b, e - b);
    s->value[e - b] = '\0';
    s->secondary[0] = s->unit[0] = '\0';
    return classified(in, s, e, TTF_SEM_CARDINAL, "ko:model-number");
}

/* Speech only. Two digit groups joined by a hyphen are an identifier (account,
 * phone, resident or order number), read
 * as a digit sequence with 다시: 27022-8080 이칠공이이 다시 팔공팔공. A range
 * (에서) is the fallback: ascending groups followed by a unit (up to five
 * digits), a counter (up to four) or a labelling counter (up to two), or two
 * four-digit years (2019-2020). Policy hyphen_digits=range restores the old
 * reading. A plain run of long_digit_identifier_min or more digits with no comma
 * grouping and no unit or quantity counter is a code read digit by digit
 * (874456932101, 2017345678번); 1,234,567,890원 and 1234567890명 stay amounts
 * and 874456932101{@sino} still reads one. Three or more groups, decimals,
 * signs and alphanumeric codes keep their own grammar, a recognized phone number
 * (1588-0000 일오팔팔 공공공공) stays a phone number, and the classifier only acts
 * in Korean text (default ko-KR or a Hangul token in the input). */
size_t ttf_ko_classify_digit_identifier(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    const char *t = in->text, *policy;
    size_t n = in->length, b = in->tokens[in->index].begin, a_end, minimum = 0, i;
    int korean = in->engine->default_language && !strcmp(in->engine->default_language, "ko-KR");
    if (b >= n || !decimal_digit(t[b]))
        return 0;
    /* Korean text only: pages 3-5 in an English sentence keeps its range. */
    for (i = 0; !korean && i < in->token_count; ++i)
        korean = in->tokens[i].script == TTF_SCRIPT_HANGUL;
    if (!korean)
        return 0;
    if (b && (decimal_digit(t[b - 1]) || latin_letter(t[b - 1]) || t[b - 1] == '-' ||
              t[b - 1] == '+' || t[b - 1] == '.' || t[b - 1] == ','))
        return 0;
    a_end = digit_run(t, n, b);
    if (a_end + 1 < n && (t[a_end] == '.' || t[a_end] == ',' || t[a_end] == ':' || t[a_end] == '/') &&
        decimal_digit(t[a_end + 1]))
        return 0;
    if (a_end < n && latin_letter(t[a_end]))
        return 0;
    policy = korean_policy(in, "hyphen_digits");
    {
        /* A domain that reads every hyphen pair digit by digit (finance:
           range_mode digits, 123-456 일이삼 사오육) keeps its own reading. */
        const char *range_mode = korean_policy(in, "range_mode");
        if (range_mode && !strcmp(range_mode, "digits"))
            policy = "range";
    }
    if (a_end + 1 < n && t[a_end] == '-' && decimal_digit(t[a_end + 1]) &&
        (!policy || strcmp(policy, "range"))) {
        size_t c = a_end + 1, c_end = digit_run(t, n, c), la = a_end - b, lb = c_end - c;
        if (c_end < n && (t[c_end] == '-' || t[c_end] == ':' || t[c_end] == '/' || latin_letter(t[c_end]) ||
                          ((t[c_end] == '.' || t[c_end] == ',') && c_end + 1 < n &&
                           decimal_digit(t[c_end + 1]))))
            return 0;
        {
            int ahead = quantity_word(in, c_end, 1);
            /* 2-0 승리, 25-18로 졌다: a score, read with 대 */
            if (!ahead && la <= 3 && lb <= 3 && !(la > 1 && t[b] == '0') && !(lb > 1 && t[c] == '0') &&
                score_sentence(in)) {
                memcpy(s->value, t + b, la);
                s->value[la] = '\0';
                memcpy(s->secondary, t + c, lb);
                s->secondary[lb] = '\0';
                s->unit[0] = '\0';
                return classified(in, s, c_end, TTF_SEM_RANGE, "ko:score");
            }
            if ((ahead == 'S' && la <= 4 && lb <= 4) ||
                (shared_identifier_pair(la, lb, t + b, t + c) && !raw_suffix(in, c_end)))
                return 0;
        }
        if (la <= 9 && lb <= 9 && !(la > 1 && t[b] == '0') && !(lb > 1 && t[c] == '0')) {
            unsigned long x = 0, y = 0;
            size_t limit;
            int kind;
            for (i = b; i < a_end; ++i)
                x = x * 10 + (unsigned long)(t[i] - '0');
            for (i = c; i < c_end; ++i)
                y = y * 10 + (unsigned long)(t[i] - '0');
            kind = quantity_word(in, c_end, 1);
            limit = kind == 'U' ? 5 : kind == 'C' ? 4 : kind == 'L' ? 2 : 0;
            if (x < y && limit && la <= limit && lb <= limit)
                return 0; /* a quantity range: the shared range grammar reads it */
            /* 24-5개월: a shortened second number (24 to 25) before a counter. */
            if (x >= y && kind && kind != 'L' && lb < la && la <= 4)
                return 0;
            if (x < y && !kind && la == 4 && lb == 4 && x >= 1000 && y <= 2999 && y - x <= 100 &&
                valid_tail(in, c_end)) {
                memcpy(s->value, t + b, la);
                s->value[la] = '\0';
                memcpy(s->secondary, t + c, lb);
                s->secondary[lb] = '\0';
                s->unit[0] = '\0';
                return classified(in, s, c_end, TTF_SEM_RANGE, "ko:year-range");
            }
        }
        if (c_end - b >= sizeof(s->value))
            return 0;
        memcpy(s->value, t + b, c_end - b);
        s->value[c_end - b] = '\0';
        s->secondary[0] = '\0';
        (void)snprintf(s->unit, sizeof(s->unit), "financial_identifier");
        return classified(in, s, c_end, TTF_SEM_DIGIT_SEQUENCE, "ko:digit-identifier");
    }
    policy = korean_policy(in, "long_digit_identifier_min");
    if (policy)
        minimum = (size_t)strtoul(policy, NULL, 10);
    if (minimum && a_end - b >= minimum && !(a_end < n && t[a_end] == '-')) {
        int kind = quantity_word(in, a_end, 0);
        if (kind == 'U' || kind == 'C' || kind == 'S')
            return 0;
        if (a_end - b >= sizeof(s->value))
            return 0;
        memcpy(s->value, t + b, a_end - b);
        s->value[a_end - b] = '\0';
        s->secondary[0] = '\0';
        (void)snprintf(s->unit, sizeof(s->unit), "financial_identifier");
        return classified(in, s, a_end, TTF_SEM_DIGIT_SEQUENCE, "ko:long-digit-identifier");
    }
    return 0;
}

