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
        if (digits == 1 && p + 2 <= n && s[p] == 'X' && (s[p + 1] == 'L' || s[p + 1] == 'S') &&
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
