/* Korean reading policy and phonetic realization live in this language pack.
 * The core only dispatches through the versioned language-pack interface. */
#include "tts_text_frontend/ttf_language_pack.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRY(expr)                                                                                  \
    do {                                                                                           \
        ttf_status_t status_ = (expr);                                                             \
        if (status_ != TTF_OK)                                                                     \
            return status_;                                                                        \
    } while (0)

static const char *resource(const ttf_engine_t *e, const char *kind, const char *key,
                            const char *fallback) {
    const char *value = ttf_resource_get(e, kind, "ko-KR", key, NULL);
    return value ? value : fallback;
}

static int resource_integer(const ttf_engine_t *e, const char *kind, const char *key, int minimum,
                            int maximum, int fallback) {
    const char *text = ttf_resource_get(e, kind, "ko-KR", key, NULL);
    char *end;
    long value;
    if (!text)
        return fallback;
    value = strtol(text, &end, 10);
    if (*end || value < minimum || value > maximum)
        return fallback;
    return (int)value;
}

static ttf_status_t digit(const ttf_engine_t *e, unsigned value, int telephone, ttf_buffer_t *out) {
    char key[2] = {(char)('0' + value), '\0'};
    static const char *const standard[] = {"영", "일", "이", "삼", "사",
                                           "오", "육", "칠", "팔", "구"};
    if (telephone && value == 0)
        return ttf_buffer_puts(out, resource(e, "number", "phone_zero", "공"));
    return ttf_buffer_puts(out, resource(e, "digit", key, standard[value]));
}

static ttf_status_t digits(const ttf_engine_t *e, const char *value, int telephone,
                           ttf_buffer_t *out) {
    for (; *value; ++value) {
        if (*value >= '0' && *value <= '9')
            TRY(digit(e, (unsigned)(*value - '0'), telephone, out));
        else if (*value == '-' || *value == ' ')
            TRY(ttf_buffer_puts(out, " "));
        else if (*value == '.')
            TRY(ttf_buffer_puts(out, resource(e, "number", "decimal", " 점 ")));
    }
    return TTF_OK;
}

static ttf_status_t sino_group(const ttf_engine_t *e, unsigned value, ttf_buffer_t *out) {
    static const unsigned divisors[] = {1000, 100, 10, 1};
    static const char *const units[] = {"천", "백", "십", ""};
    size_t i;
    for (i = 0; i < 4; ++i) {
        unsigned d = value / divisors[i];
        value %= divisors[i];
        if (!d)
            continue;
        if (d != 1 || i == 3)
            TRY(digit(e, d, 0, out));
        if (i != 3) {
            char key[16];
            (void)snprintf(key, sizeof(key), "%u", divisors[i]);
            TRY(ttf_buffer_puts(out, resource(e, "magnitude", key, units[i])));
        }
    }
    return TTF_OK;
}

static ttf_status_t cardinal(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    char integer[TTF_IR_VALUE_SIZE];
    const char *p = value, *fraction = NULL;
    size_t n = 0, start, groups, i;
    static const char *const powers[] = {"",   "만", "억", "조", "경", "해",
                                         "자", "양", "구", "간", "정"};
    if (*p == '-') {
        TRY(ttf_buffer_puts(out, resource(e, "number", "negative", "마이너스 ")));
        ++p;
    } else if (*p == '+')
        ++p;
    while (*p && *p != '.') {
        if (*p >= '0' && *p <= '9') {
            if (n + 1 >= sizeof(integer))
                return TTF_ELIMIT;
            integer[n++] = *p;
        } else if (*p != ',')
            return TTF_EINVAL;
        ++p;
    }
    if (*p == '.')
        fraction = p + 1;
    if (!n)
        return TTF_EINVAL;
    integer[n] = '\0';
    for (start = 0; start + 1 < n && integer[start] == '0'; ++start) {
    }
    n -= start;
    memmove(integer, integer + start, n + 1);
    if (n == 1 && integer[0] == '0')
        TRY(digit(e, 0, 0, out));
    else {
        groups = (n + 3) / 4;
        /* Arbitrary large identifiers still yield pronounceable output. */
        if (groups > sizeof(powers) / sizeof(powers[0])) {
            TRY(digits(e, integer, 0, out));
        } else {
            start = 0;
            for (i = groups; i > 0; --i) {
                size_t width = i == groups ? (n - 1) % 4 + 1 : 4, j;
                unsigned group = 0;
                for (j = 0; j < width; ++j)
                    group = group * 10 + (unsigned)(integer[start++] - '0');
                if (!group)
                    continue;
                if (!(group == 1 && i == 2))
                    TRY(sino_group(e, group, out));
                if (i > 1) {
                    char key[16];
                    (void)snprintf(key, sizeof(key), "10^%u", (unsigned)((i - 1) * 4));
                    TRY(ttf_buffer_puts(out, resource(e, "magnitude", key, powers[i - 1])));
                }
            }
        }
    }
    if (fraction && *fraction) {
        TRY(ttf_buffer_puts(out, resource(e, "number", "decimal", " 점 ")));
        TRY(digits(e, fraction, 0, out));
    }
    return TTF_OK;
}

static ttf_status_t native_number(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    static const char *const ones[] = {"",     "한",   "두",   "세",   "네",
                                       "다섯", "여섯", "일곱", "여덟", "아홉"};
    static const char *const tens[] = {"",   "열",   "스물", "서른", "마흔",
                                       "쉰", "예순", "일흔", "여든", "아흔"};
    char *end;
    unsigned long n = strtoul(value, &end, 10);
    char key[16];
    const char *override;
    if (*end || n == 0 || n > 99)
        return cardinal(e, value, out);
    (void)snprintf(key, sizeof(key), "%lu", n);
    override = ttf_resource_get(e, "native", "ko-KR", key, NULL);
    if (override)
        return ttf_buffer_puts(out, override);
    if (n == 20)
        return ttf_buffer_puts(out, resource(e, "native", "20", "스무"));
    if (n >= 10) {
        (void)snprintf(key, sizeof(key), "%lu", n / 10 * 10);
        TRY(ttf_buffer_puts(out, resource(e, "native_tens", key, tens[n / 10])));
    }
    if (n % 10) {
        (void)snprintf(key, sizeof(key), "%lu", n % 10);
        TRY(ttf_buffer_puts(out, resource(e, "native", key, ones[n % 10])));
    }
    return TTF_OK;
}

static ttf_status_t quantity(const ttf_engine_t *e, const char *value, const char *unit, int native,
                             ttf_buffer_t *out) {
    if (native)
        TRY(native_number(e, value, out));
    else
        TRY(cardinal(e, value, out));
    TRY(ttf_buffer_puts(out, " "));
    return ttf_buffer_puts(out, unit);
}

static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *out) {
    switch (s->type) {
    case TTF_SEM_NUMBER:
    case TTF_SEM_CARDINAL:
        return cardinal(e, s->value, out);
    case TTF_SEM_ORDINAL:
        TRY(ttf_buffer_puts(out, resource(e, "number", "ordinal_prefix", "제")));
        return cardinal(e, s->value, out);
    case TTF_SEM_DIGIT_SEQUENCE:
        return digits(e, s->value, 0, out);
    case TTF_SEM_PHONE:
        return digits(e, s->value, 1, out);
    case TTF_SEM_COUNTER:
        return quantity(e, s->value, resource(e, "unit", s->unit, s->unit),
                        strcmp(resource(e, "counter", s->unit, "sino"), "native") == 0, out);
    case TTF_SEM_CURRENCY:
        return quantity(e, s->value, resource(e, "currency", s->unit, s->unit), 0, out);
    case TTF_SEM_PERCENT:
        return quantity(e, s->value, resource(e, "unit", "%", "퍼센트"), 0, out);
    case TTF_SEM_MEASURE:
        return quantity(e, s->value, resource(e, "unit", s->unit, s->unit), 0, out);
    case TTF_SEM_RANGE: {
        int digit_mode = strcmp(resource(e, "policy", "range_mode", "cardinal"), "digits") == 0;
        if (digit_mode)
            TRY(digits(e, s->value, 0, out));
        else
            TRY(cardinal(e, s->value, out));
        TRY(ttf_buffer_puts(out, resource(e, "policy", "range_separator", " 다시 ")));
        return digit_mode ? digits(e, s->secondary, 0, out) : cardinal(e, s->secondary, out);
    }
    case TTF_SEM_DATE: {
        unsigned year, month, day;
        char value[16];
        if (sscanf(s->value, "%u-%u-%u", &year, &month, &day) != 3)
            return TTF_EINVAL;
        (void)snprintf(value, sizeof(value), "%u", year);
        TRY(quantity(e, value, resource(e, "unit", "year", "년"), 0, out));
        TRY(ttf_buffer_puts(out, " "));
        if (month == 6 || month == 10) {
            TRY(ttf_buffer_puts(
                out, resource(e, "month", month == 6 ? "6" : "10", month == 6 ? "유" : "시")));
            TRY(ttf_buffer_puts(out, " "));
            TRY(ttf_buffer_puts(out, resource(e, "unit", "month", "월")));
        } else {
            (void)snprintf(value, sizeof(value), "%u", month);
            TRY(quantity(e, value, resource(e, "unit", "month", "월"), 0, out));
        }
        TRY(ttf_buffer_puts(out, " "));
        (void)snprintf(value, sizeof(value), "%u", day);
        return quantity(e, value, resource(e, "unit", "day", "일"), 0, out);
    }
    case TTF_SEM_TIME: {
        unsigned hour, minute;
        char value[16];
        if (sscanf(s->value, "%u:%u", &hour, &minute) != 2)
            return TTF_EINVAL;
        (void)snprintf(value, sizeof(value), "%u", hour);
        TRY(quantity(e, value, resource(e, "unit", "hour", "시"), 1, out));
        TRY(ttf_buffer_puts(out, " "));
        (void)snprintf(value, sizeof(value), "%u", minute);
        return quantity(e, value, resource(e, "unit", "minute", "분"), 0, out);
    }
    default:
        return ttf_buffer_append(out, surface, length);
    }
}

static int script_score(ttf_script_t script) {
    return script == TTF_SCRIPT_HANGUL ? 100 : 0;
}

static int is_particle(const ttf_pack_input_t *in, const ttf_token_t *token) {
    size_t width;
    if (token->script != TTF_SCRIPT_HANGUL)
        return 0;
    /* Longest-prefix lookup permits particles attached to following predicates. */
    for (width = token->end - token->begin; width > 0; --width) {
        char key[64];
        if (width >= sizeof(key))
            continue;
        memcpy(key, in->text + token->begin, width);
        key[width] = '\0';
        if (ttf_resource_get(in->engine, "particle", "ko-KR", key, NULL))
            return 1;
    }
    return 0;
}

static int context_score(const ttf_pack_input_t *in) {
    size_t i, hangul = 0;
    int score;
    for (i = 0; i < in->token_count; ++i)
        if (in->tokens[i].script == TTF_SCRIPT_HANGUL)
            ++hangul;
    score = (int)(hangul < 3 ? hangul : 3) *
            resource_integer(in->engine, "context", "hangul_weight", 0, 1000, 20);
    if (hangul) {
        int floor = resource_integer(in->engine, "context", "clause_floor", 0, 1000, 60);
        if (score < floor)
            score = floor;
    }
    if (in->index < in->token_count) {
        const ttf_token_t *current = &in->tokens[in->index];
        if (current->script == TTF_SCRIPT_HANGUL)
            score += resource_integer(in->engine, "context", "script_weight", 0, 1000, 1000);
        if (in->index + 1 < in->token_count) {
            const ttf_token_t *next = &in->tokens[in->index + 1];
            if (current->end == next->begin && is_particle(in, next))
                score += resource_integer(in->engine, "context", "particle_weight", 0, 1000, 200);
        }
        /* ORDINAL and ALNUM surfaces span multiple lexical tokens. */
        for (i = in->index + 1; i < in->token_count && i <= in->index + 5; ++i) {
            const ttf_token_t *previous = &in->tokens[i - 1];
            const ttf_token_t *next = &in->tokens[i];
            if (previous->end != next->begin || next->script == TTF_SCRIPT_WHITESPACE)
                break;
            if (is_particle(in, next)) {
                score += resource_integer(in->engine, "context", "particle_weight", 0, 1000, 200);
                break;
            }
            if (next->script == TTF_SCRIPT_HANGUL)
                break;
        }
    }
    return score;
}

static ttf_status_t put_syllable(ttf_buffer_t *out, int onset, int vowel, int coda) {
    uint32_t cp = 0xac00u + (uint32_t)((onset * 21 + vowel) * 28 + coda);
    char utf8[3];
    utf8[0] = (char)(0xe0u | (cp >> 12));
    utf8[1] = (char)(0x80u | ((cp >> 6) & 0x3fu));
    utf8[2] = (char)(0x80u | (cp & 0x3fu));
    return ttf_buffer_append(out, utf8, sizeof(utf8));
}

static int onset(const ttf_engine_t *e, const char *phone) {
    return resource_integer(e, "phone_onset", phone, 0, 18, -1);
}

static int vowel(const ttf_engine_t *e, const char *phone) {
    return resource_integer(e, "phone_vowel", phone, 0, 20, -1);
}

static ttf_status_t render_phones(const ttf_engine_t *e, const ttf_pronunciation_t *p,
                                  ttf_buffer_t *out) {
    char phones[128][24];
    size_t count = 0, pos = 0, i;
    int pending = -1, glide = 0;
    if (strcmp(p->alphabet, "ttf-phones-v1") != 0 && strcmp(p->alphabet, "ARPABET") != 0 &&
        strcmp(p->alphabet, "arpabet") != 0)
        return TTF_EINVAL;
    while (p->phones[pos]) {
        size_t n = 0;
        while (p->phones[pos] == ' ')
            ++pos;
        if (!p->phones[pos])
            break;
        if (count == 128)
            return TTF_ELIMIT;
        while (p->phones[pos] && p->phones[pos] != ' ') {
            if (n + 1 >= sizeof(phones[0]))
                return TTF_ELIMIT;
            phones[count][n++] = p->phones[pos++];
        }
        if (n && phones[count][n - 1] >= '0' && phones[count][n - 1] <= '9')
            --n;
        phones[count++][n] = '\0';
    }
    for (i = 0; i < count; ++i) {
        const char *phone = phones[i];
        int v = vowel(e, phone), c = onset(e, phone);
        const char *letter = ttf_resource_get(e, "phone_letter", "ko-KR", phone, NULL);
        if (letter) {
            if (pending >= 0)
                TRY(put_syllable(out, pending, 18, 0));
            pending = -1;
            TRY(ttf_buffer_puts(out, letter));
            continue;
        }
        if (strcmp(phone, "W") == 0 || strcmp(phone, "Y") == 0) {
            glide = strcmp(phone, "W") == 0 ? 1 : 2;
            continue;
        }
        if (v >= 0) {
            int cod = 0;
            if (glide == 1) {
                if (v == 0)
                    v = 9;
                else if (v == 4 || v == 6)
                    v = 14;
                else if (v == 1)
                    v = 10;
                else if (v == 20)
                    v = 16;
            } else if (glide == 2) {
                if (v == 0)
                    v = 2;
                else if (v == 4)
                    v = 6;
                else if (v == 8)
                    v = 12;
                else if (v == 13)
                    v = 17;
            }
            /* Nasals/laterals close syllables before a consonant or word end. */
            if (i + 1 < count && (i + 2 == count || vowel(e, phones[i + 2]) < 0)) {
                int close = resource_integer(e, "phone_coda", phones[i + 1], 0, 27, -1);
                if (close >= 0) {
                    cod = close;
                    ++i;
                }
            }
            TRY(put_syllable(out, pending >= 0 ? pending : 11, v, cod));
            if (strcmp(phone, "AY") == 0 || strcmp(phone, "EY") == 0 || strcmp(phone, "OY") == 0)
                TRY(put_syllable(out, 11, 20, 0));
            else if (strcmp(phone, "AW") == 0)
                TRY(put_syllable(out, 11, 13, 0));
            pending = -1;
            glide = 0;
        } else if (c >= 0) {
            if (strcmp(phone, "R") == 0 && i + 1 == count)
                continue;
            if (pending >= 0)
                TRY(put_syllable(out, pending, 18, 0));
            pending = c;
        } else
            return TTF_EINVAL;
    }
    if (pending >= 0)
        TRY(put_syllable(out, pending, 18, 0));
    return out->length ? TTF_OK : TTF_EINVAL;
}

static ttf_status_t render_graphemes(const ttf_engine_t *e, const char *word, size_t length,
                                     ttf_buffer_t *out) {
    size_t i = 0;
    while (i < length) {
        char key[8];
        const char *mapped = NULL;
        size_t width, max_width = length - i < 3 ? length - i : 3;
        for (width = max_width; width > 0; --width) {
            size_t j;
            for (j = 0; j < width; ++j) {
                unsigned char c = (unsigned char)word[i + j];
                key[j] = (char)(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
            }
            key[width] = '\0';
            mapped = ttf_resource_get(e, "grapheme", "ko-KR", key, NULL);
            if (mapped)
                break;
        }
        if (mapped) {
            TRY(ttf_buffer_puts(out, mapped));
            i += width;
        } else if (word[i] >= '0' && word[i] <= '9') {
            TRY(digit(e, (unsigned)(word[i] - '0'), 0, out));
            ++i;
        } else {
            /* A letter-name fallback always speaks unfamiliar Latin letters. */
            char letter[16] = "letter.A";
            const char *name;
            unsigned char c = (unsigned char)word[i];
            letter[7] = (char)(c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c);
            name = ttf_resource_get(e, "phone_letter", "ko-KR", letter, NULL);
            if (name)
                TRY(ttf_buffer_puts(out, name));
            else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                TRY(ttf_buffer_puts(out, "으"));
            else if (c < 128)
                TRY(ttf_buffer_append(out, word + i, 1));
            /* Accented Latin codepoints use a speakable escape when not mapped. */
            else if ((c & 0xc0) != 0x80)
                TRY(ttf_buffer_puts(out, "으"));
            ++i;
        }
    }
    return TTF_OK;
}

const ttf_language_pack_t ttf_pack_ko_KR = {sizeof(ttf_language_pack_t),
                                            TTF_ABI_VERSION,
                                            "ko-KR",
                                            NULL,
                                            script_score,
                                            context_score,
                                            NULL,
                                            verbalize,
                                            NULL,
                                            render_phones,
                                            render_graphemes};
