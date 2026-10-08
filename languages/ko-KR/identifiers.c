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
    /* 제품코드, 상품코드: a word ending in 코드 names a code too (2026-10-02). */
    if (n > 6 && !memcmp(key + n - 6, "\xec\xbd\x94\xeb\x93\x9c", 6))
        return 1;
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
    /* 이 인 입 번, 뒤 (1385뒤에 *을 누르는; 2026-10-07) and 였 (9234였다; 2026-10-08) */
    static const char *const heads[] = {"\xec\x9d\xb4", "\xec\x9d\xb8", "\xec\x9e\x85",
                                        "\xeb\xb2\x88", "\xeb\x92\xa4", "\xec\x98\x80"};
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

static int korean_neighbour(const ttf_pack_input_t *in, size_t end);

/* 비밀번호 네 자리 1834, 핀 번호 6자리 451233: a length phrase (a count and 자리)
 * between the label and the number. Returns where the phrase starts, or begin. */
static size_t length_phrase(const ttf_pack_input_t *in, size_t begin) {
    static const char *const counts[] = {"한", "두", "세", "네", "다섯", "여섯", "일곱", "여덟", "아홉", "열",
                                         "열두", "열세", "열네", "열여섯"};
    size_t p = begin, k;
    while (p && horizontal_space(in->text[p - 1]))
        --p;
    if (p < 6 || memcmp(in->text + p - 6, "자리", 6))
        return begin;
    p -= 6;
    while (p && horizontal_space(in->text[p - 1]))
        --p;
    if (p && decimal_digit(in->text[p - 1])) {
        while (p && decimal_digit(in->text[p - 1]))
            --p;
    } else {
        size_t best = 0;
        for (k = 0; k < sizeof(counts) / sizeof(counts[0]); ++k) {
            size_t n = strlen(counts[k]);
            if (n <= p && n > best && !memcmp(in->text + p - n, counts[k], n) &&
                (p == n || horizontal_space(in->text[p - n - 1])))
                best = n;
        }
        if (!best)
            return begin;
        p -= best;
    }
    return p;
}

/* 8282로 끝나는 번호, 070으로 시작하는 번호: the number is the start or end of
 * another number, so it is read digit by digit (references 18 : 0). */
static int ends_with_cue(const ttf_pack_input_t *in, size_t end) {
    static const char *const cues[] = {"로 끝나", "으로 끝나", "로 시작", "으로 시작"};
    size_t k;
    for (k = 0; k < sizeof(cues) / sizeof(cues[0]); ++k) {
        size_t n = strlen(cues[k]);
        if (end + n <= in->length && !memcmp(in->text + end, cues[k], n))
            return 1;
    }
    return 0;
}

/* A counter or unit as the next word (1000000 원): a quantity, not a code. */
static int counter_follows(const ttf_pack_input_t *in, size_t end) {
    size_t p = end, width;
    char key[16];
    while (p < in->length && horizontal_space(in->text[p]))
        ++p;
    for (width = 3; width <= 12 && p + width <= in->length; width += 3) {
        if ((unsigned char)in->text[p + width - 3] < 0xEA || (unsigned char)in->text[p + width - 3] > 0xED)
            break;
        memcpy(key, in->text + p, width);
        key[width] = '\0';
        if (ttf_resource_get(in->engine, "numeric_suffix", "*", key, NULL) ||
            ttf_resource_get(in->engine, "counter", "ko-KR", key, NULL))
            return 1;
    }
    return 0;
}

/* 112에 신고, 114에 전화해서, 전화번호가 112와 113: a declared service number
 * (service_number) with a call cue (call_cue: 전화, 신고, 상황실 …) within twenty
 * syllables either side is a phone number read digit by digit. Without a cue it
 * stays a value (114명, 119개, 113번 버스). */
static int service_number(const ttf_pack_input_t *in, size_t begin, size_t end) {
    char key[16];
    size_t from, to, p, width;
    if (end - begin >= sizeof(key))
        return 0;
    memcpy(key, in->text + begin, end - begin);
    key[end - begin] = '\0';
    if (!ttf_resource_get(in->engine, "service_number", "ko-KR", key, NULL))
        return 0;
    /* 120만 원 is an amount; 무명로 119, 무명로7길 120 are addresses. */
    if (end + 3 <= in->length && (!memcmp(in->text + end, "만", 3) || !memcmp(in->text + end, "억", 3) ||
                                  !memcmp(in->text + end, "천", 3)))
        return 0;
    for (p = begin; p && horizontal_space(in->text[p - 1]); --p) {
    }
    if (p >= 3 && (!memcmp(in->text + p - 3, "로", 3) || !memcmp(in->text + p - 3, "길", 3)))
        return 0;
    from = begin > 60 ? begin - 60 : 0;
    to = end + 60 < in->length ? end + 60 : in->length;
    for (p = from; p < to; ++p) {
        if ((unsigned char)in->text[p] < 0xEA || (unsigned char)in->text[p] > 0xED)
            continue;
        for (width = 6; width <= 9 && p + width <= to; width += 3) {
            memcpy(key, in->text + p, width);
            key[width] = '\0';
            if (ttf_resource_get(in->engine, "call_cue", "ko-KR", key, NULL))
                return 1;
        }
    }
    return 0;
}

/* A declared cue word (kind) in the window [from, to) on the line. */
static int cue_in(const ttf_pack_input_t *in, const char *kind, size_t from, size_t to) {
    size_t p, width;
    char key[16];
    for (p = from; p + 6 <= to; ++p) {
        if (in->text[p] == '\n')
            return 0;
        if ((unsigned char)in->text[p] < 0xEA || (unsigned char)in->text[p] > 0xED)
            continue;
        for (width = 6; width <= 12 && p + width <= to; width += 3) {
            memcpy(key, in->text + p, width);
            key[width] = '\0';
            if (ttf_resource_get(in->engine, kind, "ko-KR", key, NULL))
                return 1;
        }
    }
    return 0;
}

/* 비밀번호가 … 3043을 입력, 번호키로 바꾸고 1385로, ATM에서 2501, 3412, 2014번을 입력:
 * a code of four to eight digits in a sentence about a password or keypad
 * (pin_cue within 32 syllables before, on the line) or keyed in (입력 within 14
 * syllables after) is read digit by digit (Jev review, 2026-10-07). */
static int pin_context(const ttf_pack_input_t *in, size_t begin, size_t end) {
    size_t from = begin > 96 ? begin - 96 : 0, p, to = end + 42 < in->length ? end + 42 : in->length;
    size_t near = end + 30 < in->length ? end + 30 : in->length;
    for (p = begin; p > from && in->text[p - 1] != '\n'; --p) {
    }
    if (cue_in(in, "pin_cue", p, begin))
        return 1;
    /* 5141인 비밀번호, 9112인 현관 비밀번호: the label a few syllables after (2026-10-08). */
    for (p = end; p < near && in->text[p] != '\n'; ++p) {
    }
    if (cue_in(in, "pin_cue", end, p))
        return 1;
    /* Keyed in or pressed: 입력, 눌러/누르/누른/눌렀 (2120을 눌러; 2026-10-08). */
    for (p = end; p + 6 <= to && in->text[p] != '\n'; ++p)
        if (!memcmp(in->text + p, "입력", 6) || !memcmp(in->text + p, "눌러", 6) ||
            !memcmp(in->text + p, "누르", 6) || !memcmp(in->text + p, "누른", 6) ||
            !memcmp(in->text + p, "눌렀", 6))
            return 1;
    return 0;
}

/* 외교후 112 002: a diplomatic plate, two groups of three digits after 외교 and
 * one syllable, read digit by digit (references 2 : 0). */
static size_t diplomatic_plate(const ttf_pack_input_t *in, size_t begin) {
    size_t p = begin, q;
    if (begin < 10 || in->text[begin - 1] != ' ' || memcmp(in->text + begin - 10, "외교", 6))
        return 0;
    for (q = 0; q < 3; ++q, ++p)
        if (p >= in->length || !decimal_digit(in->text[p]))
            return 0;
    if (p >= in->length || in->text[p] != ' ')
        return 0;
    for (++p, q = 0; q < 3; ++q, ++p)
        if (p >= in->length || !decimal_digit(in->text[p]))
            return 0;
    return p < in->length && decimal_digit(in->text[p]) ? 0 : p;
}

/* After N번, a frequency cue (counter_role_tail 이나/만/씩, or counter_role_after
   이상/정도/넘게 on the next word) makes it a count: 비밀번호를 5번이나 입력. */
static int frequency_after_count(const ttf_pack_input_t *in, size_t p) {
    char key[64];
    size_t q = p, width;
    const char *value;
    while (q + 3 <= in->length && (unsigned char)in->text[q] >= 0xea && (unsigned char)in->text[q] <= 0xed &&
           q - p + 3 < sizeof(key))
        q += 3;
    if (q > p) {
        memcpy(key, in->text + p, q - p);
        key[q - p] = '\0';
        value = ttf_resource_get(in->engine, "counter_role_tail", "ko-KR", key, NULL);
        return value && !strcmp(value, "frequency");
    }
    while (q < in->length && (in->text[q] == ' ' || in->text[q] == '\t'))
        ++q;
    if (q == p)
        return 0;
    /* The longest cue wins, as in the counter role (연습문제 is a number, 연습 a count). */
    const char *best = NULL;
    for (width = 3; q + width <= in->length && width < sizeof(key); width += 3) {
        if ((unsigned char)in->text[q + width - 3] < 0xea || (unsigned char)in->text[q + width - 3] > 0xed)
            break;
        memcpy(key, in->text + q, width);
        key[width] = '\0';
        value = ttf_resource_get(in->engine, "counter_role_after", "ko-KR", key, NULL);
        if (value)
            best = value;
    }
    return best && !strcmp(best, "frequency");
}

/* 231-29번지: a lot number, two groups of up to four digits before 번지, is not a
 * range; each group is a number joined by 다시 (산 12-3번지 십이 다시 삼번지;
 * Jev review, 2026-10-07). */
size_t ttf_ko_classify_lot_number(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    size_t begin = in->tokens[in->index].begin, p = begin, dash, end, i;
    if (begin && (decimal_digit(in->text[begin - 1]) || in->text[begin - 1] == '-' ||
                  ((in->text[begin - 1] | 0x20) >= 'a' && (in->text[begin - 1] | 0x20) <= 'z')))
        return 0;
    while (p < in->length && decimal_digit(in->text[p]) && p - begin < 5)
        ++p;
    if (p == begin || p - begin > 4 || p >= in->length || in->text[p] != '-' || in->text[begin] == '0')
        return 0;
    dash = p++;
    while (p < in->length && decimal_digit(in->text[p]) && p - dash <= 5)
        ++p;
    end = p;
    if (end == dash + 1 || end - dash - 1 > 4 || in->text[dash + 1] == '0' ||
        end + 6 > in->length || memcmp(in->text + end, "번지", 6))
        return 0;
    memcpy(s->value, in->text + begin, end - begin);
    s->value[end - begin] = '\0';
    (void)snprintf(s->unit, sizeof(s->unit), "financial_identifier");
    (void)snprintf(s->rule, sizeof(s->rule), "ko:lot-number");
    s->type = TTF_SEM_DIGIT_SEQUENCE;
    s->input_end = end;
    s->layer = TTF_LAYER_COMMON;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < end; ++i) {
    }
    return i - in->index;
}

/* 타율 0.212 이할 일푼 이리: a rate with three decimals after 타율, 출루율, 장타율 or
 * 승률 (rate_cue) reads 할, 푼 and 리 as Korean broadcasts do (Jev review,
 * 2026-10-07). */
size_t ttf_ko_classify_batting_average(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    size_t begin = in->tokens[in->index].begin, from = begin > 30 ? begin - 30 : 0, i;
    if (begin + 5 > in->length || in->text[begin] != '0' || in->text[begin + 1] != '.' ||
        !decimal_digit(in->text[begin + 2]) || !decimal_digit(in->text[begin + 3]) ||
        !decimal_digit(in->text[begin + 4]) ||
        (begin + 5 < in->length && (decimal_digit(in->text[begin + 5]) || in->text[begin + 5] == '.')) ||
        (begin && (decimal_digit(in->text[begin - 1]) || in->text[begin - 1] == '.')))
        return 0;
    if (!cue_in(in, "rate_cue", from, begin))
        return 0;
    memcpy(s->value, in->text + begin + 2, 3);
    s->value[3] = '\0';
    (void)snprintf(s->unit, sizeof(s->unit), "batting_average");
    (void)snprintf(s->rule, sizeof(s->rule), "ko:batting-average");
    s->type = TTF_SEM_DIGIT_SEQUENCE;
    s->input_end = begin + 5;
    /* 승률 0.576을 오할 칠푼 육리를: the reading ends in a vowel unless it is 할 or
       푼, so a following 을, 이, 은, 과 or 으로 takes its vowel form; the particle
       is part of the span and recorded in secondary. */
    {
        static const char *const pairs[][2] = {{"으로", "로"}, {"을", "를"}, {"이", "가"},
                                               {"은", "는"}, {"과", "와"}};
        size_t k;
        for (k = 0; s->value[2] != '0' && k < sizeof(pairs) / sizeof(pairs[0]); ++k) {
            size_t m = strlen(pairs[k][0]);
            if (begin + 5 + m <= in->length && !memcmp(in->text + begin + 5, pairs[k][0], m)) {
                (void)snprintf(s->secondary, sizeof(s->secondary), "%s", pairs[k][1]);
                s->input_end = begin + 5 + m;
                break;
            }
        }
    }
    s->layer = TTF_LAYER_COMMON;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < s->input_end; ++i) {
    }
    return i - in->index;
}

/* 수완 03번, 종로 08번: a route number with a leading zero reads its digits (it
 * was 세 번); 70-2번, 330-1번: a branch route, a smaller number after the hyphen,
 * reads its numbers joined by 다시 (3-5번 문제 stays a range; 2026-10-07). */
size_t ttf_ko_classify_route_number(const ttf_pack_input_t *in, ttf_semantic_t *s) {
    size_t begin = in->tokens[in->index].begin, p = begin, dash = 0, end, i;
    unsigned first = 0, second = 0;
    if (begin && (decimal_digit(in->text[begin - 1]) || in->text[begin - 1] == '-' ||
                  in->text[begin - 1] == '.' || in->text[begin - 1] == ',' ||
                  ((in->text[begin - 1] | 0x20) >= 'a' && (in->text[begin - 1] | 0x20) <= 'z')))
        return 0;
    while (p < in->length && decimal_digit(in->text[p]) && p - begin < 5)
        first = first * 10 + (unsigned)(in->text[p++] - '0');
    if (p == begin)
        return 0;
    end = p;
    if (p + 1 < in->length && in->text[p] == '-' && decimal_digit(in->text[p + 1])) {
        dash = p++;
        while (p < in->length && decimal_digit(in->text[p]) && p - dash <= 3)
            second = second * 10 + (unsigned)(in->text[p++] - '0');
        end = p;
    }
    if (end + 3 > in->length || memcmp(in->text + end, "\xeb\xb2\x88", 3) ||
        (end + 6 <= in->length && !memcmp(in->text + end, "번지", 6)))
        return 0;
    if (!dash) {
        if (end - begin < 2 || end - begin > 3 || in->text[begin] != '0')
            return 0;
        memcpy(s->value, in->text + begin, end - begin);
        s->value[end - begin] = '\0';
        (void)snprintf(s->unit, sizeof(s->unit), "leading_zero");
        (void)snprintf(s->rule, sizeof(s->rule), "ko:route-zero");
    } else {
        if (dash - begin > 3 || end - dash - 1 > 2 || in->text[begin] == '0' ||
            in->text[dash + 1] == '0' || !second || second >= first)
            return 0;
        memcpy(s->value, in->text + begin, end - begin);
        s->value[end - begin] = '\0';
        (void)snprintf(s->unit, sizeof(s->unit), "financial_identifier");
        (void)snprintf(s->rule, sizeof(s->rule), "ko:lot-number");
    }
    s->type = TTF_SEM_DIGIT_SEQUENCE;
    s->input_end = end;
    s->layer = TTF_LAYER_COMMON;
    for (i = in->index; i < in->token_count && in->tokens[i].begin < end; ++i) {
    }
    return i - in->index;
}

size_t ttf_ko_classify_identifier(const ttf_pack_input_t *in, ttf_semantic_t *semantic) {
    size_t begin = in->tokens[in->index].begin, end = begin, i;
    char label[TTF_IR_VALUE_SIZE];
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    int before, cue = 0;
    size_t plate;
    if (begin == in->length || !decimal_digit(in->text[begin]))
        return 0;
    if ((plate = diplomatic_plate(in, begin)) != 0 && labelled_tail(in, plate)) {
        memcpy(semantic->value, in->text + begin, plate - begin);
        semantic->value[plate - begin] = '\0';
        (void)snprintf(semantic->unit, sizeof(semantic->unit), "financial_identifier");
        (void)snprintf(semantic->rule, sizeof(semantic->rule), "ko:diplomatic-plate");
        semantic->type = TTF_SEM_DIGIT_SEQUENCE;
        semantic->input_end = plate;
        semantic->layer = TTF_LAYER_COMMON;
        for (i = in->index; i < in->token_count && in->tokens[i].begin < plate; ++i) {
        }
        return i - in->index;
    }
    before = preceding_label(in, begin, label, sizeof(label), &layer);
    if (!before) {
        size_t phrase = length_phrase(in, begin);
        if (phrase != begin)
            before = preceding_label(in, phrase, label, sizeof(label), &layer);
    }
    while (end < in->length && decimal_digit(in->text[end]))
        ++end;
    /* A label after a code of four or more digits (8469를 비밀번호로; references
       54 : 0, 2026-10-06; it was five or more). */
    if (!before && !(end - begin >= 4 && !(end < in->length && in->text[end] == '-') &&
                     following_label(in, end, label, sizeof(label)))) {
        /* 2501, 3412: a comma before a space ends a list item, not a group. */
        int joined = end < in->length &&
                     (in->text[end] == '-' || in->text[end] == '.' || in->text[end] == ':' ||
                      (in->text[end] == ',' && end + 1 < in->length && decimal_digit(in->text[end + 1])));
        /* Without a label (2026-10-06, project-owner judgements and references):
           the start or end of a number (8282로 끝나는), a bare run of seven or more
           digits (200487201 조은희 학생은; 1354 : 0), or a service number with a call
           cue (119에 신고). */
        /* Only in a Korean sentence; a run starting with 0 is left to the phone
           grammar (01012345678). */
        /* ATM에서 2501, 3412, 2014번을 입력: a list member keyed in has no Korean
           neighbour of its own. */
        if (joined || (!korean_neighbour(in, end) &&
                       !(end - begin >= 4 && end - begin <= 8 && pin_context(in, begin, end))))
            return 0;
        if (end - begin >= 2 && ends_with_cue(in, end))
            cue = 1;
        else if (end - begin >= 7 && in->text[begin] != '0' && !counter_follows(in, end))
            cue = 2;
        else if (service_number(in, begin, end))
            cue = 3;
        /* A short code may start with 0 (생일인 0714번; 2026-10-08): phone
           numbers are longer than six digits. */
        else if (end - begin >= 4 && end - begin <= 8 &&
                 (in->text[begin] != '0' || end - begin <= 6) &&
                 (!counter_follows(in, end) ||
                  (end + 3 <= in->length && !memcmp(in->text + end, "\xeb\xb2\x88", 3))) &&
                 pin_context(in, begin, end))
            cue = 4;
        else
            return 0;
        label[0] = '\0';
        /* A short code keeps 번 with it (비밀번호 7537번 칠오삼칠번). */
        if (cue == 4)
            (void)snprintf(label, sizeof(label), "%s", "비밀번호");
    }
    /* A word ending in 번호 that is not a declared label only marks a code of
       four or more digits (번호 3번을 keeps its counter reading). */
    if (!cue && end - begin < 4 && !ttf_resource_get(in->engine, "identifier_label", "ko-KR", label, NULL))
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
        if ((digits < 5 && !short_code_label(in, label)) ||
            (digits <= 2 && frequency_after_count(in, end + 3)))
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

/* 300000p, 72000P: the letters after the digits are a declared unit (포인트),
 * read by the quantity grammar, not a code. */
static int unit_letters(const ttf_pack_input_t *in, size_t begin, size_t end) {
    char key[16];
    if (end <= begin || end - begin >= sizeof(key))
        return 0;
    memcpy(key, in->text + begin, end - begin);
    key[end - begin] = 0;
    return (ttf_resource_get(in->engine, "numeric_suffix", "*", key, NULL) &&
            ttf_numeric_suffix_cued(in->engine, key, in->text, in->length)) ||
           ttf_resource_get(in->engine, "letter_unit", "ko-KR", key, NULL);
}

/* 38A번, 31B번 버스: a short digit-first label before the counter 번 is a route
 * number, read as a cardinal with letter names (삼십팔에이번; 2026-10-02). */
static int route_label(const ttf_pack_input_t *in, size_t end) {
    return end + 3 <= in->length && !memcmp(in->text + end, "\xeb\xb2\x88", 3);
}

/* 20F, 470G, 1234AB: up to four digits then one to three letters, not a unit
 * (5A, 500M keep the unit grammar) and not a meridiem (2PM): read as the number
 * and the letter names, 이십 에프 (project-owner decision 2026-10-02; it was
 * left unread under unreadable=preserve). */
static int short_label(const ttf_pack_input_t *in, size_t begin, size_t digits, size_t end) {
    size_t letters = end - begin - digits, i, lead = 0;
    char key[8];
    if (!digits || digits > 4 || !letters || letters > 3 || !decimal_digit(in->text[begin]) ||
        in->text[begin] == '0')
        return 0;
    while (begin + lead < end && decimal_digit(in->text[begin + lead]))
        ++lead;
    /* 2v4: digits, letters, digits (two changes of kind) read run by run. */
    if (lead < digits) {
        size_t p = begin + lead;
        while (p < end && !decimal_digit(in->text[p]))
            ++p;
        while (p < end && decimal_digit(in->text[p]))
            ++p;
        return p == end && korean_neighbour(in, end);
    }
    memcpy(key, in->text + begin + digits, letters);
    key[letters] = '\0';
    for (i = 0; i < letters; ++i)
        key[i] = (char)(key[i] >= 'a' && key[i] <= 'z' ? key[i] - 32 : key[i]);
    /* Units, meridiems, English ordinals (11st) and decades (90s) keep their grammar. */
    if (unit_letters(in, begin + digits, end) ||
        ttf_resource_get(in->engine, "time_suffix", "*", key, NULL) || !strcmp(key, "ST") ||
        !strcmp(key, "ND") || !strcmp(key, "RD") || !strcmp(key, "TH") || !strcmp(key, "S"))
        return 0;
    /* A Korean sentence, or no other Latin word around it (I wear 3XL shirts. stays English). */
    if (!korean_neighbour(in, end)) {
        /* 44, 45A, 45B의: any Hangul in the sentence makes it Korean; with none
           (I wear 3XL shirts.) only a lone label in a Korean-default engine reads. */
        int hangul = 0;
        for (i = 0; i < in->token_count; ++i)
            if (in->tokens[i].script == TTF_SCRIPT_HANGUL)
                hangul = 1;
        if (!hangul) {
            for (i = 0; i < in->token_count; ++i)
                if (i != in->index && in->tokens[i].script == TTF_SCRIPT_LATIN &&
                    !(in->tokens[i].begin >= begin && in->tokens[i].end <= end))
                    return 0;
            if (in->engine->config.default_language && strcmp(in->engine->config.default_language, "ko-KR"))
                return 0;
        }
    }
    return 1;
}

size_t ttf_ko_classify_alnum_identifier(const ttf_pack_input_t *in, ttf_semantic_t *semantic) {
    int short_form = 0;
    char label_word[TTF_IR_VALUE_SIZE];
    ttf_resource_layer_t label_layer = TTF_LAYER_COMMON;
    size_t begin = in->tokens[in->index].begin, end = begin, i, run = 0;
    size_t letters = 0, digits = 0, hyphens = 0, transitions = 0;
    int kind = 0, upper = 0, lower = 0, long_run, wordlike = 0; /* kind: 1 letter, 2 digit */
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
                /* A mixed-case run is a word (iPhone15); a long one is too unless it
                   has no vowel or follows 아이디 (lkdfs392, checked below). */
                if (upper && lower)
                    return 0;
                if (run > 4)
                    wordlike = 1;
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
    /* 아이디 sun38, 아이디 abc123: a label before lowercase letters with digits makes
       them a code, spelled even when the letters are a word (references 538
       spelled against 16 words inside e-mail addresses; 2026-10-06). Upper-case
       words keep their reading (쿠폰 코드 SAVE20 세이브 트웬티). */
    int labelled = ascii_letter(in->text[begin]) && transitions == 1 && !hyphens &&
                   in->text[begin] >= 'a' && in->text[begin] <= 'z' &&
                   preceding_label(in, begin, label_word, sizeof(label_word), &label_layer);
    /* csk01: three or more lowercase letters without a vowel cannot be a word
       (크스크); a declared whole word keeps its reading (mp4 엠피포). */
    int vowelless = 0;
    if (ascii_letter(in->text[begin]) && transitions == 1 && !hyphens && letters >= 3) {
        char whole[TTF_IR_VALUE_SIZE + 8];
        size_t used = (size_t)snprintf(whole, sizeof(whole), "en-US:");
        vowelless = 1;
        for (i = begin; i < end && ascii_letter(in->text[i]); ++i)
            if (!(in->text[i] >= 'a' && in->text[i] <= 'z') || strchr("aeiouy", in->text[i]))
                vowelless = 0;
        for (i = begin; i < end && used + 1 < sizeof(whole); ++i)
            whole[used++] = in->text[i];
        whole[used] = '\0';
        if (vowelless && ttf_resource_get(in->engine, "lexicon", "ko-KR", whole, NULL))
            vowelless = 0;
    }
    /* Boeing747 is a word with a number; lkdfs392 and 아이디 hello123 are codes. */
    if (wordlike && !vowelless && !labelled)
        return 0;
    after = end < in->length ? in->text[end] : ' ';
    if (after == '@' || after == '_' || after == '%' ||
        ((after == '.' || after == ',' || after == '/' || after == ':') && end + 1 < in->length &&
         alnum(in->text[end + 1])))
        return 0;
    /* Codes, not words or quantities: two letter/digit changes (K37PA1798,
     * 50QNA90), letters then at least three digits (M4137, kip214), or
     * hyphenated groups with at least three digits (C5135-12, AP-1019D). */
    /* Digits then letters with five or more digits (516123813ZC) is a code too,
     * like a plain run of five or more digits; shorter ones (20F, 5A, 470G) stay
     * ambiguous labels. */
    if (!(labelled || vowelless || (transitions >= 2 && end - begin >= 5) ||
          (ascii_letter(in->text[begin]) && transitions == 1 && !hyphens && digits >= 3) ||
          (decimal_digit(in->text[begin]) && transitions == 1 && !hyphens && digits >= 5 &&
           !unit_letters(in, begin + digits, end)) ||
          (decimal_digit(in->text[begin]) && transitions == 1 && !hyphens &&
           !unit_letters(in, begin + digits, end) &&
           (route_label(in, end) || preceding_label(in, begin, label_word, sizeof(label_word), &label_layer))) ||
          ((transitions == 1 || transitions == 2) && !hyphens &&
           (short_form = short_label(in, begin, digits, end)) != 0) ||
          (decimal_digit(in->text[begin]) && hyphens && route_label(in, end) && digits <= 8) ||
          (hyphens && digits >= 3)))
        return 0;
    /* A letter run that is a Korean loanword is a word with a number
     * (top100, TOP100 탑백), not a code; two-letter runs stay codes. A run of
     * five or more digits keeps the code (you501073), and an all-capital run
     * whose entry is lexicon_case lower is letters (EBS8102, AIR4930;
     * 2026-10-03). */
    for (i = begin, long_run = labelled; i < end && !long_run;) {
        size_t stop = i;
        while (stop < end && decimal_digit(in->text[stop]))
            ++stop;
        long_run = stop - i >= 5;
        i = stop > i ? stop : i + 1;
    }
    for (i = begin; i < end && !long_run;) {
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
            if (ttf_resource_get(in->engine, "lexicon", "ko-KR", key, NULL)) {
                const char *rule = ttf_resource_get(in->engine, "lexicon_case", "ko-KR", key, NULL);
                if (!(rule && !strcmp(rule, "lower") && !lowercase))
                    return 0;
            }
        }
        if (i == start)
            ++i;
    }
    for (i = in->index; i < in->token_count && in->tokens[i].begin < end; ++i) {
    }
    if (in->tokens[i - 1].end != end || (!short_form && !korean_neighbour(in, end)))
        return 0;
    memcpy(semantic->value, in->text + begin, end - begin);
    semantic->value[end - begin] = '\0';
    (void)snprintf(semantic->unit, sizeof(semantic->unit), "financial_identifier");
    (void)snprintf(semantic->rule, sizeof(semantic->rule), "%s",
                   decimal_digit(in->text[begin]) && route_label(in, end) &&
                           ((digits < 5 && transitions == 1) || (hyphens && digits <= 8))
                       ? "ko:route-label"
                   : short_form && !preceding_label(in, begin, label_word, sizeof(label_word), &label_layer)
                       ? "ko:short-label" : "ko:alnum-identifier");
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
    /* 6900번 카드로: 번 after the card number names it (2026-10-07). */
    if ((!valid_tail(in, p) && !(p + 3 <= in->length && !memcmp(in->text + p, "\xeb\xb2\x88", 3))) ||
        p - begin >= sizeof(semantic->value))
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
/* A counter or unit word as this token (66세, 55명): not a size. */
static int raw_counter_word(const ttf_pack_input_t *in, const ttf_token_t *t) {
    char key[16];
    size_t width;
    for (width = 3; width <= 9 && t->begin + width <= t->end; width += 3) {
        const char *value;
        memcpy(key, in->text + t->begin, width);
        key[width] = '\0';
        value = ttf_resource_get(in->engine, "numeric_suffix", "*", key, NULL);
        if (value && strcmp(key, "반"))
            return 1;
    }
    return 0;
}

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
            break;
    /* 펄블랙 66을, 55라면, 44, 55, 66 등, 55반~66사이즈, 55~77사이즈: without 사이즈
       right after, a doubled size in a clothing sentence (clothing_cue within
       30 syllables) is a size too, unless a counter follows (66세, 55명;
       2026-10-07). */
    {
        size_t from = begin > 90 ? begin - 90 : 0, to = p + 90 < in->length ? p + 90 : in->length;
        char after = p < in->length ? in->text[p] : ' ';
        int half = p + 3 <= in->length && !memcmp(in->text + p, "반", 3);
        int digit_next = p + 1 < in->length && decimal_digit(in->text[p + 1]);
        int scale = p + 3 <= in->length && (!memcmp(in->text + p, "만", 3) || !memcmp(in->text + p, "천", 3) ||
                                            !memcmp(in->text + p, "억", 3) || !memcmp(in->text + p, "백", 3));
        int tail_ok = !scale && (after == ' ' || ((after == ',' || after == '.') && !digit_next) ||
                                 after == '~' || half || valid_tail(in, p) || p == in->length);
        /* 88, 91, 94: a list with a number that is not a doubled size is a list of values. */
        if (tail_ok) {
            size_t q = p;
            while (q < in->length && (in->text[q] == ',' || in->text[q] == ' ' || in->text[q] == '\''))
                ++q;
            if (q > p && q + 1 < in->length && decimal_digit(in->text[q]) &&
                (in->text[q + 1] != in->text[q] || in->text[q] < '4' ||
                 (q + 2 < in->length && decimal_digit(in->text[q + 2]))))
                tail_ok = 0;
        }
        /* '82, 85, 88': the list before matters too. */
        if (tail_ok && begin >= 2) {
            size_t q = begin;
            while (q && (in->text[q - 1] == ' ' || in->text[q - 1] == '\''))
                --q;
            if (q && in->text[q - 1] == ',' && q >= 3 && decimal_digit(in->text[q - 2]) &&
                (in->text[q - 2] != in->text[q - 3] || (q >= 4 && decimal_digit(in->text[q - 4]))))
                tail_ok = 0;
        }
        if (tail_ok && !(next && next->begin == p && !half && !valid_tail(in, p)) &&
            !(next && raw_counter_word(in, next)) && cue_in(in, "clothing_cue", from, to)) {
            size_t q = p + (half ? 3 : 0), r;
            memcpy(semantic->value, in->text + begin, 2);
            semantic->value[2] = '\0';
            if (half)
                (void)snprintf(semantic->value + 2, sizeof(semantic->value) - 2, "%s", "반");
            /* 55~77: the second size joins with 에서. */
            if (q + 3 <= in->length && in->text[q] == '~' && decimal_digit(in->text[q + 1]) &&
                in->text[q + 1] == in->text[q + 2] && in->text[q + 1] >= '4' &&
                !(q + 3 < in->length && decimal_digit(in->text[q + 3]))) {
                r = strlen(semantic->value);
                (void)snprintf(semantic->value + r, sizeof(semantic->value) - r, "~%.2s", in->text + q + 1);
                q += 3;
            }
            (void)snprintf(semantic->unit, sizeof(semantic->unit), "clothing_size");
            (void)snprintf(semantic->rule, sizeof(semantic->rule), "ko:clothing-size");
            semantic->type = TTF_SEM_DIGIT_SEQUENCE;
            semantic->input_end = q;
            semantic->layer = TTF_LAYER_COMMON;
            for (i = in->index; i < in->token_count && in->tokens[i].begin < q; ++i) {
            }
            return i - in->index;
        }
    }
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
    {
        size_t l = e;
        while (l < n && ((t[l] >= 'A' && t[l] <= 'Z') || (t[l] >= 'a' && t[l] <= 'z')) && l - e < 2)
            ++l;
        if (l > e && !(l < n && alnum(t[l])))
            e = l; /* 6S, 13X, 아이폰 6s (2026-10-02) */
    }
    if (e - b > 4 || (e < n && (alnum(t[e]) || t[e] == '.' || t[e] == ',' || t[e] == '-' ||
                                t[e] == ':' || t[e] == '/')))
        return 0;
    {
        size_t d = b;
        while (d < e && decimal_digit(t[d]))
            ++d;
        if (d - b > 2)
            return 0;
    }
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
    /* 48X160cm, 21x14.8cm: dimensions with a unit read with 곱하기; 1/8g: a
       fraction with a unit (팔 분의 일 그램); 3.3T: a decimal with one letter
       (삼 점 삼 티); 3Bay, 2Room: a digit before an English word reads an
       English numeral (쓰리베이). 2026-10-02. */
    {
        size_t p = a_end, c, c_end, u;
        const char *unit;
        char key[16];
        if (p < n && (t[p] == 'x' || t[p] == 'X' || t[p] == '/') && p + 1 < n && decimal_digit(t[p + 1])) {
            c = p + 1;
            c_end = digit_run(t, n, c);
            if (t[p] != '/' && c_end + 1 < n && t[c_end] == '.' && decimal_digit(t[c_end + 1]))
                c_end = digit_run(t, n, c_end + 1);
            for (u = c_end; u < n && latin_letter(t[u]) && u - c_end < 8; ++u) {
            }
            if (u > c_end && u - c_end < sizeof(key) && !(u < n && alnum(t[u]))) {
                memcpy(key, t + c_end, u - c_end);
                key[u - c_end] = '\0';
                unit = ttf_resource_get(in->engine, "numeric_suffix", "*", key, NULL);
                if (unit && !strncmp(unit, "MEASURE:", 8) && u - b < sizeof(s->value)) {
                    memcpy(s->value, t + b, c_end - b);
                    s->value[c_end - b] = '\0';
                    (void)snprintf(s->unit, sizeof(s->unit), "%s", unit + 8);
                    s->secondary[0] = '\0';
                    return classified(in, s, u, TTF_SEM_MEASURE, t[p] == '/' ? "ko:fraction-measure" : "ko:dimensions");
                }
            }
        }
        if (p + 1 < n && t[p] == '.' && decimal_digit(t[p + 1])) {
            c_end = digit_run(t, n, p + 1);
            if (c_end < n && t[c_end] >= 'A' && t[c_end] <= 'Z' && !(c_end + 1 < n && alnum(t[c_end + 1])) &&
                !unit_letters(in, c_end, c_end + 1) && c_end + 1 - b < sizeof(s->value)) {
                memcpy(s->value, t + b, c_end + 1 - b);
                s->value[c_end + 1 - b] = '\0';
                (void)snprintf(s->unit, sizeof(s->unit), "financial_identifier");
                return classified(in, s, c_end + 1, TTF_SEM_DIGIT_SEQUENCE, "ko:short-label");
            }
        }
        /* 3days, 1day, 2way: a lowercase word too, when it has a vowel and the
           shared grammar has no unit for it (5min, 3kcal stay quantities). */
        if (a_end - b == 1 && p + 2 < n && t[p] >= 'a' && t[p] <= 'z' && t[p + 1] >= 'a' && t[p + 1] <= 'z') {
            ttf_semantic_t shared = {0};
            int vowel = 0;
            u = p;
            while (u < n && t[u] >= 'a' && t[u] <= 'z')
                vowel |= strchr("aeiouy", t[u++]) != NULL;
            shared.struct_size = sizeof(shared);
            if (u - p >= 3 && vowel && !(u < n && alnum(t[u])) && u - b < sizeof(s->value) &&
                !unit_letters(in, p, u) && ttf_classify_universal(in, &shared) &&
                shared.type == TTF_SEM_TEXT && !strcmp(shared.rule, "syntax.unsupported-numeric") &&
                shared.input_end == u) {
                memcpy(s->value, t + b, u - b);
                s->value[u - b] = '\0';
                s->unit[0] = s->secondary[0] = '\0';
                (void)snprintf(s->source_lang, sizeof(s->source_lang), "en-US");
                s->lexical_class = TTF_LEX_ALNUM_TERM;
                return classified(in, s, u, TTF_SEM_ALNUM_TERM, "ko:numeral-word");
            }
        }
        if (a_end - b == 1 && p + 3 < n && t[p] >= 'A' && t[p] <= 'Z' && t[p + 1] >= 'a' && t[p + 1] <= 'z') {
            u = p + 1;
            while (u < n && t[u] >= 'a' && t[u] <= 'z')
                ++u;
            if (u - p >= 3 && !(u < n && alnum(t[u])) && u - b < sizeof(s->value)) {
                memcpy(s->value, t + b, u - b);
                s->value[u - b] = '\0';
                s->unit[0] = s->secondary[0] = '\0';
                (void)snprintf(s->source_lang, sizeof(s->source_lang), "en-US");
                s->lexical_class = TTF_LEX_ALNUM_TERM;
                return classified(in, s, u, TTF_SEM_ALNUM_TERM, "ko:numeral-word");
            }
        }
    }
    /* 3x3 농구 삼 대 삼: two small numbers joined by x (2026-10-02). */
    if (a_end + 1 < n && t[a_end] == 'x' && decimal_digit(t[a_end + 1]) && a_end - b <= 2) {
        size_t c = a_end + 1, c_end = digit_run(t, n, c);
        if (c_end - c <= 2 && !(c_end < n && (alnum(t[c_end]) || t[c_end] == '.'))) {
            memcpy(s->value, t + b, a_end - b);
            s->value[a_end - b] = '\0';
            memcpy(s->secondary, t + c, c_end - c);
            s->secondary[c_end - c] = '\0';
            s->unit[0] = '\0';
            return classified(in, s, c_end, TTF_SEM_RANGE, "ko:score");
        }
    }
    /* 3 대 2로, 6 대 1이: a score or ratio written with 대 between spaced numbers
       reads Sino like 3:2 (benchmark references 85 : 0; 2026-10-05). */
    if (a_end - b <= 3 && a_end + 6 <= n && !memcmp(t + a_end, " \xeb\x8c\x80 ", 5) &&
        decimal_digit(t[a_end + 5])) {
        size_t c = a_end + 5, c_end = digit_run(t, n, c);
        if (c_end - c <= 3 && !(c_end < n && (alnum(t[c_end]) || t[c_end] == '.' || t[c_end] == ',')) &&
            !(a_end - b > 1 && t[b] == '0') && !(c_end - c > 1 && t[c] == '0')) {
            memcpy(s->value, t + b, a_end - b);
            s->value[a_end - b] = '\0';
            memcpy(s->secondary, t + c, c_end - c);
            s->secondary[c_end - c] = '\0';
            s->unit[0] = '\0';
            return classified(in, s, c_end, TTF_SEM_RANGE, "ko:score");
        }
    }
    /* 35대 1로: the same score with 대 against the first number. The second
       number takes only a particle, so 35대 1년 stays a count and a year. */
    if (a_end - b <= 3 && a_end + 5 <= n && !memcmp(t + a_end, "\xeb\x8c\x80 ", 4) &&
        decimal_digit(t[a_end + 4])) {
        static const char *const particles[] = {"로", "으", "의", "이", "을", "를", "은", "는", "에", "과", "와", "도", "까"};
        size_t c = a_end + 4, c_end = digit_run(t, n, c), k;
        int ok = c_end - c <= 3 && !(c_end < n && (alnum(t[c_end]) || t[c_end] == '.' || t[c_end] == ',')) &&
                 !(c_end - c > 1 && t[c] == '0');
        if (ok && c_end + 3 <= n && (unsigned char)t[c_end] >= 0xea && (unsigned char)t[c_end] <= 0xed) {
            ok = 0;
            for (k = 0; k < sizeof(particles) / sizeof(particles[0]); ++k)
                ok |= !memcmp(t + c_end, particles[k], 3);
        }
        if (ok) {
            memcpy(s->value, t + b, a_end - b);
            s->value[a_end - b] = '\0';
            memcpy(s->secondary, t + c, c_end - c);
            s->secondary[c_end - c] = '\0';
            s->unit[0] = '\0';
            return classified(in, s, c_end, TTF_SEM_RANGE, "ko:score");
        }
    }
    /* 0~3M 영에서 삼 엠: a range ending in a short label that is not a unit. */
    if (a_end + 1 < n && t[a_end] == '~' && decimal_digit(t[a_end + 1]) && a_end - b <= 4) {
        size_t c = a_end + 1, c_end = digit_run(t, n, c), l = c_end;
        while (l < n && latin_letter(t[l]) && l - c_end < 4)
            ++l;
        if (l > c_end && l - c_end <= 3 && c_end - c <= 4 && !(l < n && alnum(t[l])) &&
            !unit_letters(in, c_end, l) && l - b < sizeof(s->value)) {
            memcpy(s->value, t + b, l - b);
            s->value[l - b] = '\0';
            s->secondary[0] = '\0';
            (void)snprintf(s->unit, sizeof(s->unit), "financial_identifier");
            return classified(in, s, l, TTF_SEM_DIGIT_SEQUENCE, "ko:label-range");
        }
    }
    /* 3:2로 승리, 40:34, 1:1 질문: a colon pair that is not a valid time (the
       shared grammar rejects it as syntax.invalid-time) is a score or ratio read
       with 대 (project-owner decision 2026-10-02). A two-digit group with a
       leading zero (24:00) still looks like a time and stays preserved. */
    if (a_end + 1 < n && t[a_end] == ':' && decimal_digit(t[a_end + 1])) {
        size_t c = a_end + 1, c_end = digit_run(t, n, c), la = a_end - b, lb = c_end - c;
        ttf_semantic_t shared = {0};
        size_t consumed;
        if (la > 3 || lb > 3 || (la > 1 && t[b] == '0') || (lb > 1 && t[c] == '0') ||
            (c_end < n && (t[c_end] == ':' || t[c_end] == '.' || t[c_end] == '-' || alnum(t[c_end]))))
            return 0;
        shared.struct_size = sizeof(shared);
        consumed = ttf_classify_universal(in, &shared);
        if (!consumed || shared.type != TTF_SEM_TEXT || strcmp(shared.rule, "syntax.invalid-time") ||
            shared.input_end != c_end)
            return 0;
        memcpy(s->value, t + b, la);
        s->value[la] = '\0';
        memcpy(s->secondary, t + c, lb);
        s->secondary[lb] = '\0';
        s->unit[0] = '\0';
        return classified(in, s, c_end, TTF_SEM_RANGE, "ko:score");
    }
    /* 1,2라인: small numbers joined by commas without thousands grouping are a
       list read one by one (일, 이라인). A counter or unit after it keeps the
       shared grammar. */
    if (a_end + 1 < n && t[a_end] == ',' && decimal_digit(t[a_end + 1]) && a_end - b <= 2) {
        size_t p = a_end, used = a_end - b;
        while (p + 1 < n && t[p] == ',' && decimal_digit(t[p + 1])) {
            size_t q = digit_run(t, n, p + 1);
            if (q - (p + 1) > 2)
                return 0;
            p = q;
        }
        {
            int kind = quantity_word(in, p, 0);
            /* 1,2호, 3,4번 출구: a numbering counter keeps the list (일, 이호). */
            if (p - b >= sizeof(s->value) || (p < n && (alnum(t[p]) || t[p] == '.')) || (kind && kind != 'L'))
                return 0;
        }
        (void)used;
        memcpy(s->value, t + b, p - b);
        s->value[p - b] = '\0';
        s->secondary[0] = s->unit[0] = '\0';
        return classified(in, s, p, TTF_SEM_CARDINAL, "ko:number-list");
    }
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

