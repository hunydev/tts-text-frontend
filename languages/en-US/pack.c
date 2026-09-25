#include "tts_text_frontend/ttf_language_pack.h"
#include "../../src/pronunciation/trace.h"
#include "date.h"
#include "abbreviation.h"
#include "identifiers.h"
#include "speech.h"
#include "../../src/semantic/phone.h"
#include "../../src/language/context_memo.h"
#include "../context_run.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EN "en-US"
static char ascii_lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c;
}
static const char *res(const ttf_engine_t *e, const char *kind, const char *key) {
    return ttf_resource_get(e, kind, EN, key, NULL);
}
static int weight(const char *s, int fallback) {
    char *end;
    long n;
    if (!s)
        return fallback;
    n = strtol(s, &end, 10);
    return !*end && n >= 0 && n <= 1000 ? (int)n : fallback;
}
static ttf_status_t word(const ttf_engine_t *e, ttf_buffer_t *b, const char *kind,
                         const char *key) {
    const char *v = res(e, kind, key);
    if (!v)
        return TTF_ERESOURCE;
    return ttf_buffer_puts(b, v);
}
static ttf_status_t space(ttf_buffer_t *b) {
    return ttf_buffer_puts(b, " ");
}
static ttf_status_t card(const ttf_engine_t *e, uint64_t n, ttf_buffer_t *b) {
    static const uint64_t scales[] = {
        UINT64_C(1000000000000000000), UINT64_C(1000000000000000), UINT64_C(1000000000000),
        UINT64_C(1000000000),          UINT64_C(1000000),          UINT64_C(1000)};
    size_t i;
    char key[32];
    ttf_status_t st;
    for (i = 0; i < sizeof(scales) / sizeof(scales[0]); ++i) {
        if (n >= scales[i]) {
            st = card(e, n / scales[i], b);
            if (st != TTF_OK)
                return st;
            if ((st = space(b)) != TTF_OK)
                return st;
            (void)snprintf(key, sizeof(key), "%llu", (unsigned long long)scales[i]);
            if ((st = word(e, b, "number", key)) != TTF_OK)
                return st;
            n %= scales[i];
            if (!n)
                return TTF_OK;
            if ((st = space(b)) != TTF_OK)
                return st;
            return card(e, n, b);
        }
    }
    if (n >= 100) {
        if ((st = card(e, n / 100, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if ((st = word(e, b, "number", "100")) != TTF_OK)
            return st;
        n %= 100;
        if (!n)
            return TTF_OK;
        if ((st = space(b)) != TTF_OK)
            return st;
    }
    if (n >= 20) {
        (void)snprintf(key, sizeof(key), "%u", (unsigned)(n / 10 * 10));
        if ((st = word(e, b, "number", key)) != TTF_OK)
            return st;
        n %= 10;
        if (!n)
            return TTF_OK;
        if ((st = space(b)) != TTF_OK)
            return st;
    }
    (void)snprintf(key, sizeof(key), "%u", (unsigned)n);
    return word(e, b, "number", key);
}
static ttf_status_t digits(const ttf_engine_t *e, const char *s, ttf_buffer_t *b) {
    size_t i;
    ttf_status_t st;
    int emitted = 0;
    for (i = 0; s[i]; ++i) {
        char key[2] = {s[i], '\0'};
        if (s[i] < '0' || s[i] > '9')
            continue;
        if (emitted && (st = space(b)) != TTF_OK)
            return st;
        if ((st = word(e, b, "number", key)) != TTF_OK)
            return st;
        emitted = 1;
    }
    return TTF_OK;
}
static int uint64_value(const char *text, uint64_t *value) {
    const char *p;
    *value = 0;
    if (!*text)
        return 0;
    for (p = text; *p; ++p) {
        unsigned digit_value = (unsigned)(*p - '0');
        if (digit_value > 9 || *value > (UINT64_MAX - digit_value) / 10)
            return 0;
        *value = *value * 10 + digit_value;
    }
    return 1;
}
static int singular(const char *text) {
    if (*text == '-' || *text == '+')
        ++text;
    while (*text == '0')
        ++text;
    if (*text++ != '1')
        return 0;
    if (*text == '.') {
        ++text;
        while (*text == '0')
            ++text;
    }
    return !*text;
}
static int thousand_or_larger(const char *text) {
    size_t integral = 0;
    if (*text == '-' || *text == '+')
        ++text;
    while (*text == '0')
        ++text;
    while (*text >= '0' && *text <= '9') {
        ++integral;
        ++text;
    }
    return integral >= 4;
}
/* A scaled currency keeps the exact expanded amount in the universal IR.
   Recover its written coefficient by moving the decimal point, never through
   floating point. Only named cardinal scales can be rendered this way. */
static ttf_status_t currency_coefficient(const ttf_semantic_t *s, char *coefficient,
                                         char *scale_key) {
    char digits_text[TTF_IR_VALUE_SIZE], raw[TTF_IR_VALUE_SIZE + 20];
    const char *p = s->secondary, *value = s->value;
    size_t n = 0, integral = 0, used = 0, i, start;
    unsigned exponent = 0;
    uint64_t scale = 1;
    int decimal = 0;
    char sign = '\0';
    if (!*p)
        return TTF_ERESOURCE;
    for (; *p; ++p) {
        if (*p < '0' || *p > '9' || exponent > 18)
            return TTF_ERESOURCE;
        exponent = exponent * 10 + (unsigned)(*p - '0');
    }
    if (exponent < 3 || exponent > 18 || exponent % 3)
        return TTF_ERESOURCE;
    if (*value == '-' || *value == '+')
        sign = *value++;
    for (p = value; *p; ++p) {
        if (*p == '.' && !decimal && n) {
            integral = n;
            decimal = 1;
        } else if (*p >= '0' && *p <= '9' && n < sizeof(digits_text) - 1) {
            digits_text[n++] = *p;
        } else
            return TTF_EINVAL;
    }
    if (!n || (decimal && integral == n))
        return TTF_EINVAL;
    if (!decimal)
        integral = n;
    if (integral <= exponent) {
        raw[used++] = '0';
        raw[used++] = '.';
        for (i = integral; i < exponent; ++i)
            raw[used++] = '0';
        memcpy(raw + used, digits_text, n);
        used += n;
    } else {
        size_t before = integral - exponent;
        memcpy(raw, digits_text, before);
        used = before;
        raw[used++] = '.';
        memcpy(raw + used, digits_text + before, n - before);
        used += n - before;
    }
    while (used && raw[used - 1] == '0')
        --used;
    if (used && raw[used - 1] == '.')
        --used;
    raw[used] = '\0';
    start = 0;
    while (raw[start] == '0' && raw[start + 1] && raw[start + 1] != '.')
        ++start;
    used = 0;
    if (sign)
        coefficient[used++] = sign;
    memcpy(coefficient + used, raw + start, strlen(raw + start) + 1);
    for (i = 0; i < exponent; ++i)
        scale *= 10;
    (void)snprintf(scale_key, 24, "%llu", (unsigned long long)scale);
    return TTF_OK;
}
static ttf_status_t numeric(const ttf_engine_t *e, const char *s, ttf_buffer_t *b) {
    char integral[TTF_IR_VALUE_SIZE];
    const char *dot;
    size_t len;
    uint64_t value;
    ttf_status_t st;
    if (*s == '-' || *s == '+') {
        if ((st = word(e, b, "number", *s == '-' ? "minus" : "plus")) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        ++s;
    }
    dot = strchr(s, '.');
    len = dot ? (size_t)(dot - s) : strlen(s);
    if (len >= sizeof(integral))
        return TTF_ELIMIT;
    memcpy(integral, s, len);
    integral[len] = '\0';
    if (uint64_value(integral, &value))
        st = card(e, value, b);
    else
        st = digits(e, integral, b);
    if (st != TTF_OK || !dot)
        return st;
    if ((st = space(b)) != TTF_OK)
        return st;
    if ((st = word(e, b, "number", "point")) != TTF_OK)
        return st;
    if ((st = space(b)) != TTF_OK)
        return st;
    return digits(e, dot + 1, b);
}
static ttf_status_t ordinal(const ttf_engine_t *e, uint64_t n, ttf_buffer_t *b) {
    char key[32];
    const char *v;
    ttf_status_t st;
    (void)snprintf(key, sizeof(key), "%llu", (unsigned long long)n);
    v = res(e, "ordinal", key);
    if (v)
        return ttf_buffer_puts(b, v);
    if (n < 20 || (n < 100 && n % 10 == 0))
        return TTF_ERESOURCE;
    if (n < 100) {
        if ((st = card(e, n / 10 * 10, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        return ordinal(e, n % 10, b);
    }
    if (n % 100) {
        if ((st = card(e, n - n % 100, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        return ordinal(e, n % 100, b);
    }
    if ((st = card(e, n, b)) != TTF_OK)
        return st;
    return word(e, b, "ordinal", "suffix");
}
/* Calendar readings are a language policy. The stored year remains the exact
   numeric value, and ISO input retains the existing cardinal default. */
static ttf_status_t date_year(const ttf_engine_t *e, unsigned year, const char *policy,
                              ttf_buffer_t *b) {
    const char *style = res(e, "date_policy", policy);
    ttf_status_t st;
    unsigned century, remainder;
    if (!style)
        style = !strcmp(policy, "numeric_year_style") ? "cardinal" : "calendar";
    if (!strcmp(style, "cardinal"))
        return card(e, year, b);
    if (strcmp(style, "calendar"))
        return TTF_ERESOURCE;
    if (year < 1000 || (year >= 2000 && year <= 2009) || year % 1000 == 0)
        return card(e, year, b);
    century = year / 100;
    remainder = year % 100;
    if ((st = card(e, century, b)) != TTF_OK || (st = space(b)) != TTF_OK)
        return st;
    if (!remainder)
        return word(e, b, "number", "100");
    if (remainder < 10) {
        if ((st = word(e, b, "number", "oh")) != TTF_OK || (st = space(b)) != TTF_OK)
            return st;
    }
    return card(e, remainder, b);
}
/* A decade is the plural of the year reading: 1960s nineteen sixties, 1900s
   nineteen hundreds, 2000s two thousands, 80s eighties. The plural words are
   data because they are English morphology, not arithmetic. */
static ttf_status_t date_decade(const ttf_engine_t *e, unsigned value, ttf_buffer_t *b) {
    char key[16];
    ttf_status_t st;
    unsigned century = value / 100, remainder = value % 100;
    if (value < 100) {
        (void)snprintf(key, sizeof(key), "%u", value);
        return word(e, b, "decade_plural", key);
    }
    if (!remainder) {
        if (!(value % 1000)) {
            if ((st = card(e, value / 1000, b)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            return word(e, b, "decade_plural", "1000");
        }
        if ((st = card(e, century, b)) != TTF_OK || (st = space(b)) != TTF_OK)
            return st;
        return word(e, b, "decade_plural", "100");
    }
    if ((st = card(e, century, b)) != TTF_OK || (st = space(b)) != TTF_OK)
        return st;
    (void)snprintf(key, sizeof(key), "%u", remainder);
    return word(e, b, "decade_plural", key);
}

static ttf_status_t date(const ttf_engine_t *e, const ttf_semantic_t *s, ttf_buffer_t *b) {
    unsigned year = 0, month = 0, day = 0;
    int consumed = 0, fields, has_year = 1, has_month = 1, has_day = 1;
    int day_first = !strcmp(s->secondary, "dmy");
    char key[16];
    const char *policy = s->secondary[0] ? "named_year_style" : "numeric_year_style";
    ttf_status_t st;
    if (s->secondary[0] && strcmp(s->secondary, "mdy") && !day_first)
        return TTF_EINVAL;
    if (!strcmp(s->unit, "decade")) {
        if (sscanf(s->value, "%u%n", &year, &consumed) != 1 || s->value[consumed])
            return TTF_EINVAL;
        return date_decade(e, year, b);
    }
    if (!strcmp(s->unit, "year")) {
        fields = sscanf(s->value, "%u%n", &year, &consumed);
        has_month = has_day = 0;
        if (fields != 1)
            return TTF_EINVAL;
        policy = "year_style";
    } else if (!strcmp(s->unit, "year-month")) {
        fields = sscanf(s->value, "%u-%u%n", &year, &month, &consumed);
        has_day = 0;
        if (fields != 2)
            return TTF_EINVAL;
    } else if (!strcmp(s->unit, "month-day")) {
        fields = sscanf(s->value, "--%u-%u%n", &month, &day, &consumed);
        has_year = 0;
        if (fields != 2)
            return TTF_EINVAL;
    } else if (!s->unit[0]) {
        if (sscanf(s->value, "%u-%u-%u%n", &year, &month, &day, &consumed) != 3)
            return TTF_EINVAL;
    } else
        return TTF_EINVAL;
    if (s->value[consumed] || (has_year && (!year || year > 9999)) ||
        (has_month && (!month || month > 12)) || (has_day && (!day || day > 31)))
        return TTF_EINVAL;
    if (!has_month)
        return date_year(e, year, policy, b);
    if (day_first && has_day) {
        if ((st = word(e, b, "date_word", "the")) != TTF_OK || (st = space(b)) != TTF_OK ||
            (st = ordinal(e, day, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
            (st = word(e, b, "date_word", "of")) != TTF_OK || (st = space(b)) != TTF_OK)
            return st;
    }
    (void)snprintf(key, sizeof(key), "%u", month);
    if ((st = word(e, b, "month", key)) != TTF_OK)
        return st;
    if (has_day && !day_first) {
        if ((st = space(b)) != TTF_OK || (st = ordinal(e, day, b)) != TTF_OK)
            return st;
    }
    if (has_year) {
        if ((st = space(b)) != TTF_OK)
            return st;
        return date_year(e, year, policy, b);
    }
    return TTF_OK;
}
static int script_score(ttf_script_t s) {
    return s == TTF_SCRIPT_LATIN ? 10 : 0;
}
/* Units, scale markers and ordinal suffixes convey semantics, not independent
   English words. */
static int neutral_marker(const ttf_pack_input_t *in, size_t index, const char *lower) {
    const ttf_token_t *t = &in->tokens[index];
    size_t previous = index, start, length;
    char original[96];
    const char *dotted_neutral = res(in->engine, "context_neutral", "dotted_initials");
    /* Letter labels do not establish an English clause by themselves. Dotted
       initials inherit the configured/surrounding language, while ordinary
       words alongside them still contribute their independent evidence. */
    if (dotted_neutral && strcmp(dotted_neutral, "0") && t->end == t->begin + 1 &&
        in->text[t->begin] >= 'A' && in->text[t->begin] <= 'Z' && t->end < in->length &&
        in->text[t->end] == '.')
        return 1;
    /* A-B, x-y: joined single letters label something; they are not the
       English article or a word of an English clause. */
    if (t->end == t->begin + 1) {
        const char *joined_neutral = res(in->engine, "context_neutral", "joined_letters");
        int after =
            index + 2 < in->token_count && in->tokens[index + 1].begin == t->end &&
            in->tokens[index + 1].end == t->end + 1 &&
            (in->text[t->end] == '-' || in->text[t->end] == '&' || in->text[t->end] == '/') &&
            in->tokens[index + 2].begin == t->end + 1 &&
            in->tokens[index + 2].script == TTF_SCRIPT_LATIN &&
            in->tokens[index + 2].end == t->end + 2;
        int before = index >= 2 && in->tokens[index - 1].end == t->begin &&
                     in->tokens[index - 1].begin + 1 == t->begin &&
                     (in->text[t->begin - 1] == '-' || in->text[t->begin - 1] == '&' ||
                      in->text[t->begin - 1] == '/') &&
                     in->tokens[index - 2].script == TTF_SCRIPT_LATIN &&
                     in->tokens[index - 2].end + 1 == t->begin &&
                     in->tokens[index - 2].begin + 2 == t->begin;
        if (joined_neutral && !strcmp(joined_neutral, "1") && (after || before))
            return 1;
    }
    if (!strcmp(lower, "www") && t->end < in->length && in->text[t->end] == '.') {
        const char *web_neutral = res(in->engine, "context_neutral", "web_initialism");
        size_t after = t->end + 1;
        if (web_neutral && !strcmp(web_neutral, "1") &&
            (after == in->length || (unsigned char)in->text[after] <= 32 ||
             (unsigned char)in->text[after] >= 127))
            return 1;
    }
    if (in->engine->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
        const char *alnum_neutral = res(in->engine, "context_neutral", "alnum_label");
        size_t next = t->end;
        int after_digit = index && in->tokens[index - 1].end == t->begin &&
                          in->tokens[index - 1].script == TTF_SCRIPT_DIGIT;
        if (next + 1 < in->length && in->text[next] == '-')
            ++next;
        if (alnum_neutral && !strcmp(alnum_neutral, "1") &&
            (after_digit || (next < in->length && in->text[next] >= '0' && in->text[next] <= '9')))
            return 1;
    }
    /* The exponent marker inside scientific numeric syntax is not an English
       word. The universal classifier remains responsible for validation. */
    if (!strcmp(lower, "e") && index && in->tokens[index - 1].script == TTF_SCRIPT_DIGIT &&
        in->tokens[index - 1].end == t->begin) {
        size_t next = index + 1, end = t->end;
        if (next < in->token_count && in->tokens[next].begin == end &&
            in->tokens[next].end == end + 1 && (in->text[end] == '+' || in->text[end] == '-'))
            end = in->tokens[next++].end;
        if (next < in->token_count && in->tokens[next].begin == end &&
            in->tokens[next].script == TTF_SCRIPT_DIGIT)
            return 1;
    }
    while (previous && (in->tokens[previous - 1].script == TTF_SCRIPT_WHITESPACE ||
                        in->tokens[previous - 1].script == TTF_SCRIPT_SYMBOL))
        --previous;
    if (!previous || in->tokens[previous - 1].script != TTF_SCRIPT_DIGIT)
        return 0;
    if (in->tokens[previous - 1].end == t->begin &&
        ttf_resource_get(in->engine, "ordinal_suffix", "*", lower, NULL))
        return 1;
    start = in->tokens[previous - 1].end;
    while (start < t->begin && (in->text[start] == ' ' || in->text[start] == '\t'))
        ++start;
    length = t->end - start;
    if (length >= sizeof(original))
        return 0;
    memcpy(original, in->text + start, length);
    original[length] = '\0';
    return ttf_resource_get(in->engine, "numeric_suffix", "*", original, NULL) != NULL ||
           ttf_resource_get(in->engine, "currency_scale", "*", original, NULL) != NULL;
}
/* An explicit label selects the speech language of its immediately following
   lexical run. The labels and their weights are resource data, and a zero
   weight disables an inherited label. Sentence/line boundaries end the cue. */
static int context_cue_score(const ttf_pack_input_t *in, size_t begin) {
    const ttf_token_t *token;
    char key[96];
    size_t i, n;
    int colon = 0;
    while (begin) {
        token = &in->tokens[begin - 1];
        if (token->script == TTF_SCRIPT_WHITESPACE) {
            for (i = token->begin; i < token->end; ++i)
                if (in->text[i] == '\r' || in->text[i] == '\n')
                    return 0;
            --begin;
        } else if (!colon && token->end == token->begin + 1 && in->text[token->begin] == ':') {
            colon = 1;
            --begin;
        } else
            break;
    }
    if (!begin)
        return 0;
    token = &in->tokens[begin - 1];
    n = token->end - token->begin;
    if (n >= sizeof(key))
        return 0;
    memcpy(key, in->text + token->begin, n);
    key[n] = '\0';
    return weight(res(in->engine, "context_cue", key), 0);
}
/* Shared run evidence is index-independent. The source order of lexical roles
 * remains significant (for example an imperative following a subject). */
static int run_context_score(const ttf_pack_input_t *in, size_t begin, size_t end) {
    size_t i, words = 0;
    int total = 0, predicate = 0, imperative = 0, subject = 0, polite = 0;
    int ceiling = weight(res(in->engine, "context", "phrase_ceiling"), 160);
    int latin = weight(res(in->engine, "context", "latin_weight"), 8);
    for (i = begin; i < end; ++i) {
        const ttf_token_t *t = &in->tokens[i];
        char key[96];
        const char *role;
        size_t n = t->end - t->begin, j;
        int addition;
        if (t->script != TTF_SCRIPT_LATIN || n >= sizeof(key))
            continue;
        for (j = 0; j < n; ++j)
            key[j] = ascii_lower(in->text[t->begin + j]);
        key[n] = '\0';
        if (neutral_marker(in, i, key))
            continue;
        ++words;
        addition = latin + weight(res(in->engine, "context_word", key), 0);
        /* Saturation avoids an arbitrarily long noun list defeating an attached
           particle, and prevents an integer overflow on a large input. */
        total = addition >= ceiling - total ? ceiling : total + addition;
        role = res(in->engine, "context_role", key);
        if (!role)
            continue;
        if (!strcmp(role, "finite"))
            predicate = 1;
        else if (!strcmp(role, "imperative")) {
            if (words == 1 || polite || subject)
                imperative = 1;
        } else if (!strcmp(role, "subject"))
            subject = 1;
        else if (!strcmp(role, "polite"))
            polite = 1;
    }
    /* A predicate with another word supplies a clause signal. Bare nouns and
       a conjunction alone do not. The finite/imperative vocabulary is data. */
    if ((predicate && words >= 2) ||
        (imperative && ((words >= 3) || ((polite || subject) && words >= 2))))
        total += weight(res(in->engine, "context", "clause_weight"), 500);
    /* An ordinal alone is a quantity title and keeps the surrounding speech
       language. At least one lexical word is required for a foreign title. */
    return total + (words ? context_cue_score(in, begin) : 0);
}

static int numeric_label_context_score(const ttf_pack_input_t *in) {
    const ttf_token_t *current = &in->tokens[in->index];
    size_t width = current->end - current->begin;
    int numeric_start =
        current->script == TTF_SCRIPT_DIGIT ||
        (width == 1 && (in->text[current->begin] == '+' || in->text[current->begin] == '-' ||
                        in->text[current->begin] == '('));
    if (numeric_start &&
        (ttf_phone_preceding_label(in, current->begin, "phone_label", EN, NULL, NULL) ||
         ttf_phone_preceding_label(in, current->begin, "identifier_label", EN, NULL, NULL)))
        return weight(res(in->engine, "context", "numeric_label_weight"), 24);
    return 0;
}

static const char context_memo_identity = 0;
static ttf_status_t build_context_scores(const ttf_pack_input_t *in, void **out) {
    ttf_pack_input_t cursor = *in;
    size_t i, last_begin = SIZE_MAX, last_end = SIZE_MAX;
    int *scores, last_score = 0;
    *out = NULL;
    if (in->token_count > SIZE_MAX / sizeof(*scores))
        return TTF_ELIMIT;
    scores = (int *)malloc(in->token_count * sizeof(*scores));
    if (!scores)
        return TTF_ENOMEM;
    for (i = 0; i < in->token_count; ++i) {
        size_t begin, end;
        if (in->tokens[i].script == TTF_SCRIPT_HANGUL) {
            scores[i] = 0;
            continue;
        }
        cursor.index = i;
        context_run_bounds(&cursor, &begin, &end);
        /* Leading/trailing whitespace can share one trimmed run. Each distinct
         * contiguous run is evaluated once, without changing its extent. */
        if (begin != last_begin || end != last_end) {
            last_score = run_context_score(&cursor, begin, end);
            last_begin = begin;
            last_end = end;
        }
        scores[i] = last_score;
    }
    *out = scores;
    return TTF_OK;
}
static void dispose_context_scores(void *scores) {
    free(scores);
}
static int context_score(const ttf_pack_input_t *in) {
    int *scores = NULL, run_score;
    size_t begin, end;
    if (in->index >= in->token_count || in->tokens[in->index].script == TTF_SCRIPT_HANGUL)
        return 0;
    if (ttf_context_memo_enabled(in))
        scores = (int *)ttf_context_memo_get(in, &context_memo_identity, build_context_scores,
                                             dispose_context_scores);
    if (scores)
        run_score = scores[in->index];
    else {
        /* Direct pack callbacks retain an uncached reference path. Memo errors
         * remain sticky on the request and are propagated by the core. */
        context_run_bounds(in, &begin, &end);
        run_score = run_context_score(in, begin, end);
    }
    return run_score + numeric_label_context_score(in);
}

static int upper_run(const ttf_pack_input_t *in, size_t index) {
    const ttf_token_t *t = &in->tokens[index];
    size_t i;
    if (t->script != TTF_SCRIPT_LATIN || t->end - t->begin > 4)
        return 0;
    for (i = t->begin; i < t->end; ++i)
        if (in->text[i] < 'A' || in->text[i] > 'Z')
            return 0;
    return 1;
}

/* R&D, AT&T: short capital runs joined by an unspaced ampersand spell their
   letters. Lowercase words (rock&roll) and longer runs keep word grammar. */
static size_t ampersand_initialism(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    size_t index = in->index, end;
    if (!upper_run(in, index) ||
        (in->tokens[index].begin && ((in->text[in->tokens[index].begin - 1] >= 'a' &&
                                      in->text[in->tokens[index].begin - 1] <= 'z') ||
                                     (in->text[in->tokens[index].begin - 1] >= '0' &&
                                      in->text[in->tokens[index].begin - 1] <= '9') ||
                                     in->text[in->tokens[index].begin - 1] == '&')))
        return 0;
    while (index + 2 < in->token_count) {
        const ttf_token_t *amp = &in->tokens[index + 1], *next = &in->tokens[index + 2];
        if (amp->begin != in->tokens[index].end || amp->end != amp->begin + 1 ||
            in->text[amp->begin] != '&' || next->begin != amp->end || !upper_run(in, index + 2))
            break;
        index += 2;
    }
    end = in->tokens[index].end;
    if (index == in->index || end - in->tokens[in->index].begin >= sizeof(out->value) ||
        (end < in->length && ((in->text[end] >= '0' && in->text[end] <= '9') ||
                              in->text[end] == '&' || in->text[end] == '_')))
        return 0;
    memcpy(out->value, in->text + in->tokens[in->index].begin, end - in->tokens[in->index].begin);
    out->value[end - in->tokens[in->index].begin] = '\0';
    out->type = TTF_SEM_INITIALISM;
    out->lexical_class = TTF_LEX_INITIALISM;
    ttf_en_initialism_policy(in->engine, out, out->value, strlen(out->value), "inferred");
    out->input_end = end;
    (void)snprintf(out->rule, sizeof(out->rule), "pack.ampersand-initialism");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    return index - in->index + 1;
}

static int single_letter(const ttf_pack_input_t *in, size_t index) {
    const ttf_token_t *t = &in->tokens[index];
    char c = in->text[t->begin];
    return t->script == TTF_SCRIPT_LATIN && t->end == t->begin + 1 &&
           ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'));
}
static int joiner_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
           c == '&' || c == '_';
}
/* x-y, A-B: single letters joined by hyphens spell one letter sequence. */
static size_t hyphen_letters(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    size_t index = in->index, used = 0, end;
    if (!single_letter(in, index) ||
        (in->tokens[index].begin && joiner_char(in->text[in->tokens[index].begin - 1])))
        return 0;
    out->value[used++] = (char)toupper((unsigned char)in->text[in->tokens[index].begin]);
    while (index + 2 < in->token_count && used + 1 < sizeof(out->value)) {
        const ttf_token_t *dash = &in->tokens[index + 1];
        if (dash->begin != in->tokens[index].end || dash->end != dash->begin + 1 ||
            (in->text[dash->begin] != '-' && in->text[dash->begin] != '/') ||
            in->tokens[index + 2].begin != dash->end || !single_letter(in, index + 2))
            break;
        index += 2;
        out->value[used++] = (char)toupper((unsigned char)in->text[in->tokens[index].begin]);
    }
    end = in->tokens[index].end;
    if (index == in->index || (end < in->length && joiner_char(in->text[end]))) {
        out->value[0] = '\0';
        return 0;
    }
    out->value[used] = '\0';
    out->type = TTF_SEM_INITIALISM;
    out->lexical_class = TTF_LEX_INITIALISM;
    ttf_en_initialism_policy(in->engine, out, out->value, used, "inferred");
    out->input_end = end;
    (void)snprintf(out->rule, sizeof(out->rule), "pack.hyphen-letters");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    return index - in->index + 1;
}

static size_t classify(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const ttf_token_t *t = &in->tokens[in->index];
    char key[96];
    size_t n = t->end - t->begin, j, consumed = 1, end = t->end;
    size_t date_tokens = ttf_en_classify_identifier(in, out);
    const char *kind;
    if (date_tokens)
        return date_tokens;
    date_tokens = ttf_en_classify_date(in, out);
    if (date_tokens)
        return date_tokens;
    date_tokens = ttf_en_classify_abbreviation(in, out);
    if (date_tokens)
        return date_tokens;
    if (t->script != TTF_SCRIPT_LATIN || n >= sizeof(key))
        return 0;
    date_tokens = ampersand_initialism(in, out);
    if (date_tokens)
        return date_tokens;
    date_tokens = hyphen_letters(in, out);
    if (date_tokens)
        return date_tokens;
    /* Treat an adjacent hyphenated word as one lexical unit so the existing
       layered lexicon can choose a pronunciation for the complete spelling.
       Spaces, doubled hyphens and numeric terms are separate grammar. */
    while (in->index + consumed + 1 < in->token_count) {
        const ttf_token_t *separator = &in->tokens[in->index + consumed];
        const ttf_token_t *next = &in->tokens[in->index + consumed + 1];
        if (separator->begin != end || separator->end != separator->begin + 1 ||
            in->text[separator->begin] != '-' || next->begin != separator->end ||
            next->script != TTF_SCRIPT_LATIN)
            break;
        /* Bound lookahead as well as the copied key. Otherwise an overlong
           chain revisited from each component would take quadratic time. */
        if (next->end - t->begin >= sizeof(key))
            return 0;
        end = next->end;
        consumed += 2;
    }
    n = end - t->begin;
    if (n >= sizeof(key))
        return 0;
    for (j = 0; j < n; ++j)
        key[j] = ascii_lower(in->text[t->begin + j]);
    key[n] = '\0';
    kind = res(in->engine, "lexical", key);
    out->type = TTF_SEM_WORD;
    out->lexical_class = TTF_LEX_UNKNOWN_LATIN;
    /* IT, AT, BE: an all-uppercase run whose spelling is also an ordinary word
       is an abbreviation under the spelling policy. A lexicon entry still wins
       later, because it is an explicit reading for the term. */
    if (kind && in->engine->initialism_policy == TTF_INITIALISM_SPELL && consumed == 1 && n > 1 &&
        strcmp(kind, "ACRONYM") && strcmp(kind, "INITIALISM") && strcmp(kind, "PROPER_NOUN")) {
        size_t upper;
        for (upper = 0; upper < n; ++upper)
            if (in->text[t->begin + upper] < 'A' || in->text[t->begin + upper] > 'Z')
                break;
        if (upper == n)
            kind = "INITIALISM";
    }
    if (kind) {
        if (!strcmp(kind, "INITIALISM")) {
            out->type = TTF_SEM_INITIALISM;
            out->lexical_class = TTF_LEX_INITIALISM;
        } else if (!strcmp(kind, "ACRONYM")) {
            out->type = TTF_SEM_ACRONYM;
            out->lexical_class = TTF_LEX_ACRONYM;
        } else if (!strcmp(kind, "PROPER_NOUN"))
            out->lexical_class = TTF_LEX_PROPER_NOUN;
        else
            out->lexical_class = TTF_LEX_WORD;
    } else {
        int all_upper = n > 1;
        for (j = 0; j < n; ++j)
            if (in->text[t->begin + j] < 'A' || in->text[t->begin + j] > 'Z')
                all_upper = 0;
        if (all_upper) {
            out->type = TTF_SEM_INITIALISM;
            out->lexical_class = TTF_LEX_INITIALISM;
        }
    }
    if (out->type == TTF_SEM_INITIALISM)
        ttf_en_initialism_policy(in->engine, out, in->text + t->begin, n,
                                 kind ? "known" : "inferred");
    if (consumed == 1 && t->end < in->length && in->text[t->end] == '.' &&
        res(in->engine, "abbreviation", key)) {
        out->type = TTF_SEM_ABBREVIATION;
        out->input_end = t->end + 1;
        (void)snprintf(out->rule, sizeof(out->rule), "pack.abbreviation");
        return in->index + 1 < in->token_count ? 2 : 1;
    }
    out->input_end = end;
    (void)snprintf(out->rule, sizeof(out->rule), "%s",
                   consumed > 1 ? "pack.hyphenated-word" : "pack.lexical");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    return consumed;
}
static ttf_status_t phone_digits(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    int previous_digit = 0;
    for (; *value; ++value) {
        const char *label;
        ttf_status_t status;
        if (*value == '-') {
            label = res(e, "phone_render", "separator");
            if (!label)
                label = " "; /* Legacy phone group separator. */
            previous_digit = 0;
        } else if (*value >= '0' && *value <= '9') {
            char key[2] = {*value, '\0'};
            label = *value == '0' ? res(e, "phone_render", "zero") : res(e, "number", key);
            if (!label && *value == '0')
                label = res(e, "number", "0");
            if (previous_digit && (status = space(out)) != TTF_OK)
                return status;
            previous_digit = 1;
        } else
            return TTF_EINVAL;
        if (!label)
            return TTF_ERESOURCE;
        if ((status = ttf_buffer_puts(out, label)) != TTF_OK)
            return status;
    }
    return TTF_OK;
}
static ttf_status_t phone(const ttf_engine_t *e, const ttf_semantic_t *s, ttf_buffer_t *out) {
    const char *value = s->value;
    ttf_status_t status;
    if (*value == '+') {
        if ((status = word(e, out, "phone_render", "plus")) != TTF_OK)
            return status;
        ++value;
    }
    if ((status = phone_digits(e, value, out)) != TTF_OK)
        return status;
    if (s->secondary[0]) {
        if ((status = word(e, out, "phone_render", "extension")) != TTF_OK)
            return status;
        return phone_digits(e, s->secondary, out);
    }
    return TTF_OK;
}

/* Same-language spoken labels keep ordinary words and spell isolated letters
   or uppercase initialisms. Source verbalization for a foreign target must
   retain its original atom so the generic composition pipeline can split it. */
static ttf_status_t spoken_alnum(const ttf_engine_t *e, const char *text, size_t length,
                                 ttf_buffer_t *out) {
    size_t pos = 0;
    int has_digit = 0, emitted = 0, initialism = 0;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c >= '0' && c <= '9')
            has_digit = 1;
        else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-'))
            return ttf_buffer_append(out, text, length);
    }
    if (!has_digit)
        return ttf_buffer_append(out, text, length);
    while (pos < length) {
        size_t end = pos;
        ttf_status_t status;
        if (text[pos] == '-') {
            ++pos;
            continue;
        }
        if (emitted && (status = space(out)) != TTF_OK)
            return status;
        if (text[pos] >= '0' && text[pos] <= '9') {
            char value[TTF_IR_VALUE_SIZE];
            const char *policy = initialism ? res(e, "alnum_number", "initialism") : NULL;
            int digit_mode;
            while (end < length && text[end] >= '0' && text[end] <= '9')
                ++end;
            if (end - pos >= sizeof(value))
                return TTF_ELIMIT;
            memcpy(value, text + pos, end - pos);
            value[end - pos] = 0;
            digit_mode =
                (end - pos > 1 && text[pos] == '0') || (policy && !strcmp(policy, "source-digits"));
            ttf_trace_stage(e, TTF_TRACE_POLICY, pos, end);
            if (policy) {
                status = ttf_trace_resource(e, "alnum_number", EN, "initialism", value,
                                            "english-alnum-number-policy", SIZE_MAX);
                if (status != TTF_OK)
                    return status;
            }
            status = digit_mode ? digits(e, value, out) : numeric(e, value, out);
            if (status == TTF_OK)
                status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, EN, "decision",
                                        "alnum-number", value, value,
                                        digit_mode ? "digit-label" : "cardinal-label", SIZE_MAX);
        } else {
            int upper = 1;
            while (end < length && ((text[end] >= 'a' && text[end] <= 'z') ||
                                    (text[end] >= 'A' && text[end] <= 'Z'))) {
                if (text[end] >= 'a' && text[end] <= 'z')
                    upper = 0;
                ++end;
            }
            initialism = upper && end - pos >= 2;
            if (upper || end - pos == 1)
                status = ttf_en_speech_spelling(e, text + pos, end - pos, 0, out);
            else
                status = ttf_buffer_append(out, text + pos, end - pos);
        }
        if (status != TTF_OK)
            return status;
        emitted = 1;
        pos = end;
    }
    return TTF_OK;
}

static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *out);
static ttf_status_t expression_number(const ttf_engine_t *e, const char *value, ttf_buffer_t *out);
static ttf_status_t expression_letter(const ttf_engine_t *e, char letter, ttf_buffer_t *out) {
    char key[2] = {letter >= 'A' && letter <= 'Z' ? (char)(letter - 'A' + 'a') : letter, 0};
    const char *label = ttf_resource_get(e, "letter_name", "en-US", key, NULL);
    return label ? ttf_buffer_puts(out, label) : TTF_ERESOURCE;
}
/* A canonical expression ("3 - 1 = 2") read with editable operator names and a
   relation template: {L} and {R} are the two sides, and {A/B} picks A after a
   final consonant (Korean particle), otherwise B. */
static ttf_status_t expression_side(const ttf_engine_t *e, const char *value, size_t length,
                                    ttf_buffer_t *out) {
    size_t at = 0;
    ttf_status_t st;
    while (at < length) {
        char part[48];
        size_t end = at;
        while (end < length && value[end] != ' ')
            ++end;
        if (end == at || end - at >= sizeof(part))
            return TTF_EINVAL;
        memcpy(part, value + at, end - at);
        part[end - at] = 0;
        if (at && (st = ttf_buffer_puts(out, " ")) != TTF_OK)
            return st;
        if ((part[0] >= '0' && part[0] <= '9') || (part[0] == '-' && part[1]))
            st = expression_number(e, part, out);
        else if (!part[1] &&
                 ((part[0] >= 'a' && part[0] <= 'z') || (part[0] >= 'A' && part[0] <= 'Z')))
            st = expression_letter(e, part[0], out);
        else {
            const char *label = ttf_resource_get(e, "math_operator", EN, part, NULL);
            if (!label)
                return TTF_ERESOURCE;
            st = ttf_trace_resource(e, "math_operator", EN, part, part, "expression-operator",
                                    SIZE_MAX);
            if (st == TTF_OK)
                st = ttf_buffer_puts(out, label);
        }
        if (st != TTF_OK)
            return st;
        at = end < length ? end + 1 : end;
    }
    return TTF_OK;
}
static int final_consonant(const ttf_buffer_t *out) {
    size_t start = out->length;
    uint32_t cp;
    size_t width;
    while (start && ((unsigned char)out->data[start - 1] & 0xc0) == 0x80)
        --start;
    if (!start || ttf_utf8_decode(out->data + start - 1, out->length - start + 1, &cp, &width) != 1)
        return 0;
    return cp >= 0xac00 && cp <= 0xd7a3 && (cp - 0xac00) % 28;
}
static ttf_status_t expression(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    const char *relation = NULL, *template, *p;
    size_t length = strlen(value), left = length, right = length;
    ttf_status_t st;
    static const char *const relations[] = {" = ", " < ", " > ", " ≤ ", " ≥ "};
    for (size_t i = 0; i < sizeof(relations) / sizeof(relations[0]); ++i) {
        const char *hit = strstr(value, relations[i]);
        if (hit) {
            relation = relations[i];
            left = (size_t)(hit - value);
            right = left + strlen(relations[i]);
        }
    }
    if (!relation)
        return expression_side(e, value, length, out);
    {
        char key[8];
        size_t w = strlen(relation) - 2;
        memcpy(key, relation + 1, w);
        key[w] = 0;
        template = ttf_resource_get(e, "math_relation", EN, key, NULL);
        if (!template)
            return TTF_ERESOURCE;
        if ((st = ttf_trace_resource(e, "math_relation", EN, key, key, "expression-relation",
                                     SIZE_MAX)) != TTF_OK)
            return st;
    }
    for (p = template; *p;) {
        if (!strncmp(p, "{L}", 3)) {
            st = expression_side(e, value, left, out);
            p += 3;
        } else if (!strncmp(p, "{R}", 3)) {
            st = expression_side(e, value + right, length - right, out);
            p += 3;
        } else if (*p == '{' && strchr(p, '/') && strchr(p, '}') &&
                   strchr(p, '/') < strchr(p, '}')) {
            const char *slash = strchr(p, '/'), *close = strchr(p, '}');
            st = final_consonant(out)
                     ? ttf_buffer_append(out, p + 1, (size_t)(slash - p - 1))
                     : ttf_buffer_append(out, slash + 1, (size_t)(close - slash - 1));
            p = close + 1;
        } else {
            st = ttf_buffer_append(out, p, 1);
            ++p;
        }
        if (st != TTF_OK)
            return st;
    }
    return TTF_OK;
}

static ttf_status_t expression_number(const ttf_engine_t *e, const char *value, ttf_buffer_t *out) {
    return numeric(e, value, out);
}

/* 2.1.3 and 192.168.0.1: dotted numeric parts with an editable part reading and
   separator. A leading-zero part keeps its digits; a v prefix is spelled. */
static ttf_status_t dotted_numbers(const ttf_engine_t *e, const ttf_semantic_t *s,
                                   const char *surface, size_t length, ttf_buffer_t *out) {
    const char *kind = s->type == TTF_SEM_IP_ADDRESS ? "ip" : "version";
    const char *policy = ttf_resource_get(e, "electronic_number", EN, kind, NULL);
    const char *separator = ttf_resource_get(e, "electronic_separator", EN, kind, NULL);
    size_t at = 0;
    ttf_status_t status;
    if (!policy || !separator)
        return TTF_ERESOURCE;
    if (s->secondary[0]) {
        if (!length || surface[0] != s->secondary[0])
            return TTF_EINVAL;
        if ((status = ttf_en_speech_spelling(e, surface, 1, 0, out)) != TTF_OK ||
            (status = ttf_buffer_puts(out, " ")) != TTF_OK)
            return status;
        at = 1;
    }
    while (at < length) {
        ttf_semantic_t part = {0};
        size_t end = at;
        while (end < length && surface[end] >= '0' && surface[end] <= '9')
            ++end;
        if (end == at || end - at >= sizeof(part.value))
            return TTF_EINVAL;
        part.struct_size = sizeof(part);
        part.type = !strcmp(policy, "digits") || (end - at > 1 && surface[at] == '0')
                        ? TTF_SEM_DIGIT_SEQUENCE
                        : TTF_SEM_CARDINAL;
        memcpy(part.value, surface + at, end - at);
        if ((status = verbalize(e, &part, surface + at, end - at, out)) != TTF_OK)
            return status;
        at = end;
        if (at < length) {
            if (surface[at] != '.')
                return TTF_EINVAL;
            if ((status = ttf_buffer_puts(out, " ")) != TTF_OK ||
                (status = ttf_buffer_puts(out, separator)) != TTF_OK ||
                (status = ttf_buffer_puts(out, " ")) != TTF_OK)
                return status;
            ++at;
        }
    }
    return TTF_OK;
}

static ttf_status_t verbalize(const ttf_engine_t *e, const ttf_semantic_t *s, const char *surface,
                              size_t length, ttf_buffer_t *b) {
    ttf_status_t st;
    char key[48];
    const char *label;
    unsigned h, min, sec;
    switch (s->type) {
    case TTF_SEM_ALNUM_TERM:
        if (e->frontend_options.output_mode == TTF_OUTPUT_SPEECH && !strcmp(s->render_lang, EN))
            return spoken_alnum(e, surface, length, b);
        return ttf_buffer_append(b, surface, length);
    case TTF_SEM_SYMBOL:
        if (!strcmp(s->unit, "@number-sign")) {
            const char *template = ttf_resource_get(e, "number_sign", EN, "#", NULL), *p;
            ttf_status_t status;
            if (!template)
                return TTF_ERESOURCE;
            status =
                ttf_trace_resource(e, "number_sign", EN, "#", "#", "number-sign-reading", SIZE_MAX);
            if (status != TTF_OK)
                return status;
            for (p = template; *p;) {
                if (!strncmp(p, "{N}", 3)) {
                    status = numeric(e, s->value, b);
                    p += 3;
                } else {
                    status = ttf_buffer_append(b, p, 1);
                    ++p;
                }
                if (status != TTF_OK)
                    return status;
            }
            return TTF_OK;
        }
        if (!strcmp(s->unit, "@expression"))
            return expression(e, s->value, b);
        if (!strcmp(s->unit, "@math")) {
            label = res(e, "math_operator", s->value);
            if (!label)
                return TTF_ERESOURCE;
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, strlen(s->value));
            st = ttf_trace_resource(e, "math_operator", EN, s->value, s->value,
                                    "binary-operator-reading", SIZE_MAX);
            return st == TTF_OK ? ttf_buffer_puts(b, label) : st;
        }
        return ttf_buffer_append(b, surface, length);
    case TTF_SEM_CUSTOM:
        if (!strcmp(s->unit, "@alphabet"))
            return ttf_en_speech_spelling(e, s->value, strlen(s->value), 0, b);
        return TTF_EINVAL;
    case TTF_SEM_VERSION:
    case TTF_SEM_IP_ADDRESS:
        return dotted_numbers(e, s, surface, length, b);
    case TTF_SEM_EMAIL:
    case TTF_SEM_URL:
        return ttf_en_speech_spelling(e, surface, length, 1, b);
    case TTF_SEM_ABBREVIATION:
    case TTF_SEM_INITIALISM:
        if (!strcmp(s->rule, "syntax.web-initialism"))
            return ttf_buffer_puts(b, s->value);
        return ttf_en_verbalize_abbreviation(e, s, surface, length, b);
    case TTF_SEM_CARDINAL:
    case TTF_SEM_NUMBER:
        return numeric(e, s->value, b);
    case TTF_SEM_ORDINAL: {
        uint64_t value;
        if (!uint64_value(s->value, &value))
            return ttf_buffer_append(b, surface, length);
        return ordinal(e, value, b);
    }
    case TTF_SEM_DIGIT_SEQUENCE:
        if (!strcmp(s->unit, "financial_identifier"))
            return ttf_en_render_identifier(e, s->value, b);
        return digits(e, s->value, b);
    case TTF_SEM_PHONE:
        return phone(e, s, b);
    case TTF_SEM_DATE:
        return date(e, s, b);
    case TTF_SEM_TIME: {
        int fields = sscanf(s->value, "%u:%u:%u", &h, &min, &sec);
        if (fields < 2)
            return TTF_EINVAL;
        /* Midnight is a time, not a zero-valued twelve-hour clock position.
           Keep labels and the optional seconds relation in editable resources. */
        if (!h && !min) {
            if (fields == 3 && sec) {
                if ((st = card(e, sec, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
                    (st = word(e, b, "unit", sec == 1 ? "s.one" : "s.many")) != TTF_OK ||
                    (st = space(b)) != TTF_OK || (st = word(e, b, "clock", "after")) != TTF_OK ||
                    (st = space(b)) != TTF_OK)
                    return st;
            }
            return word(e, b, "clock", "midnight");
        }
        if ((st = card(e, h, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if (!min) {
            /* The project keeps 24-hour values; do not attach o'clock to 13..23. */
            st = h <= 12 ? word(e, b, "number", "oclock") : word(e, b, "clock", "h24_full_hour");
            if (st != TTF_OK)
                return st;
        } else {
            if (min < 10) {
                if ((st = word(e, b, "number", "oh")) != TTF_OK)
                    return st;
                if ((st = space(b)) != TTF_OK)
                    return st;
            }
            if ((st = card(e, min, b)) != TTF_OK)
                return st;
        }
        if (fields == 3) {
            if ((st = space(b)) != TTF_OK || (st = word(e, b, "number", "and")) != TTF_OK ||
                (st = space(b)) != TTF_OK || (st = card(e, sec, b)) != TTF_OK ||
                (st = space(b)) != TTF_OK ||
                (st = word(e, b, "unit", sec == 1 ? "s.one" : "s.many")) != TTF_OK)
                return st;
        }
        if (s->secondary[0]) {
            if ((st = space(b)) != TTF_OK)
                return st;
            return word(e, b, "meridiem", s->secondary);
        }
        return TTF_OK;
    }
    case TTF_SEM_FRACTION:
        if ((st = numeric(e, s->value, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
            (st = word(e, b, "number", "fraction_over")) != TTF_OK || (st = space(b)) != TTF_OK)
            return st;
        return numeric(e, s->secondary, b);
    case TTF_SEM_CURRENCY: {
        const char *dot = strchr(s->value, '.');
        const char *minor_policy = res(e, "currency_minor_digits", s->unit);
        char whole[TTF_IR_VALUE_SIZE];
        size_t len = dot ? (size_t)(dot - s->value) : strlen(s->value);
        size_t fractional_digits = dot ? strlen(dot + 1) : 0;
        unsigned minor_digits = 2;
        int decimal_major;
        if (s->secondary[0]) {
            char coefficient[TTF_IR_VALUE_SIZE + 20], scale_key[24];
            if ((st = currency_coefficient(s, coefficient, scale_key)) != TTF_OK)
                return st;
            if ((st = numeric(e, coefficient, b)) != TTF_OK || (st = space(b)) != TTF_OK ||
                (st = word(e, b, "number", scale_key)) != TTF_OK || (st = space(b)) != TTF_OK)
                return st;
            /* A scale phrase takes a plural currency noun even when a tiny
               coefficient happens to produce one expanded monetary unit. */
            (void)snprintf(key, sizeof(key), "%s.many", s->unit);
            return word(e, b, "currency", key);
        }
        if (minor_policy) {
            if (!strcmp(minor_policy, "0"))
                minor_digits = 0;
            else if (strcmp(minor_policy, "2"))
                return TTF_ERESOURCE;
        }
        decimal_major = !minor_digits || fractional_digits > minor_digits;
        memcpy(whole, s->value, len);
        whole[len] = '\0';
        if ((st = numeric(e, decimal_major ? s->value : whole, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        (void)snprintf(key, sizeof(key), "%s.%s", s->unit,
                       singular(decimal_major ? s->value : whole) ? "one" : "many");
        if ((st = word(e, b, "currency", key)) != TTF_OK)
            return st;
        if (dot && !decimal_major && fractional_digits && strtoul(dot + 1, NULL, 10)) {
            unsigned cents = (unsigned)strtoul(dot + 1, NULL, 10);
            if (!dot[2])
                cents *= 10;
            if ((st = space(b)) != TTF_OK)
                return st;
            if ((st = word(e, b, "number", "and")) != TTF_OK)
                return st;
            if ((st = space(b)) != TTF_OK)
                return st;
            if ((st = card(e, cents, b)) != TTF_OK)
                return st;
            if ((st = space(b)) != TTF_OK)
                return st;
            (void)snprintf(key, sizeof(key), "%s.%s", s->unit,
                           cents == 1 ? "minor.one" : "minor.many");
            return word(e, b, "currency", key);
        }
        return TTF_OK;
    }
    case TTF_SEM_PERCENT:
        if ((st = numeric(e, s->value, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        return word(e, b, "number", "percent");
    case TTF_SEM_MEASURE:
    case TTF_SEM_COUNTER:
        if ((st = numeric(e, s->value, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        (void)snprintf(key, sizeof(key), "%s.%s", s->unit, singular(s->value) ? "one" : "many");
        label = res(e, "unit", key);
        return ttf_buffer_puts(b, label ? label : s->unit);
    case TTF_SEM_RANGE:
        if (!strcmp(s->unit, "time")) {
            ttf_semantic_t endpoint = *s;
            endpoint.type = TTF_SEM_TIME;
            endpoint.secondary[0] = '\0';
            endpoint.unit[0] = '\0';
            if ((st = verbalize(e, &endpoint, surface, length, b)) != TTF_OK ||
                (st = space(b)) != TTF_OK || (st = word(e, b, "number", "range")) != TTF_OK ||
                (st = space(b)) != TTF_OK)
                return st;
            (void)snprintf(endpoint.value, sizeof(endpoint.value), "%s", s->secondary);
            return verbalize(e, &endpoint, surface, length, b);
        }
        /* Sharing "million dollars" in "two to three million dollars" can
           incorrectly imply a million-scale left endpoint. Repeat currency
           labels when only one endpoint reaches a named cardinal scale. */
        (void)snprintf(key, sizeof(key), "%s.many", s->unit);
        if (s->unit[0] && res(e, "currency", key) &&
            thousand_or_larger(s->value) != thousand_or_larger(s->secondary)) {
            ttf_semantic_t endpoint = *s;
            endpoint.type = TTF_SEM_CURRENCY;
            endpoint.secondary[0] = '\0';
            if ((st = verbalize(e, &endpoint, surface, length, b)) != TTF_OK ||
                (st = space(b)) != TTF_OK || (st = word(e, b, "number", "range")) != TTF_OK ||
                (st = space(b)) != TTF_OK)
                return st;
            (void)snprintf(endpoint.value, sizeof(endpoint.value), "%s", s->secondary);
            return verbalize(e, &endpoint, surface, length, b);
        }
        if ((st = numeric(e, s->value, b)) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if ((st = word(e, b, "number", "range")) != TTF_OK)
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if ((st = numeric(e, s->secondary, b)) != TTF_OK || !s->unit[0])
            return st;
        if ((st = space(b)) != TTF_OK)
            return st;
        if (!strcmp(s->unit, "percent"))
            return word(e, b, "number", "percent");
        (void)snprintf(key, sizeof(key), "%s.%s", s->unit, singular(s->secondary) ? "one" : "many");
        label = res(e, "unit", key);
        if (!label)
            label = res(e, "currency", key);
        return ttf_buffer_puts(b, label ? label : s->unit);
    default:
        return ttf_buffer_append(b, surface, length);
    }
}
static size_t phone_count(const char *text) {
    size_t count = 0;
    int in_token = 0;
    for (; *text; ++text) {
        if (*text == ' ')
            in_token = 0;
        else if (!in_token) {
            ++count;
            in_token = 1;
        }
    }
    return count;
}

static ttf_status_t pronounce(const ttf_engine_t *e, const char *word_text, size_t n,
                              ttf_lexical_class_t lexical, ttf_pronunciation_t *out) {
    char key[TTF_IR_VALUE_SIZE], variant_key_text[TTF_IR_VALUE_SIZE + 32];
    size_t i, p = 0, phone_index = 0;
    const char *phones, *dictionary_kind, *dictionary_key, *selection_reason = "default-base";
    ttf_resource_layer_t layer = TTF_LAYER_COMMON, part_layer = TTF_LAYER_COMMON;
    ttf_status_t status;
    /* Long unknown words bypass this bounded prototype and reach the target's
       streaming grapheme fallback; they must not fail the entire request. */
    if (n >= sizeof(key))
        return TTF_EINVAL;
    memset(out, 0, sizeof(*out));
    (void)snprintf(out->alphabet, sizeof(out->alphabet), "ttf-phones-v1");
    for (i = 0; i < n; ++i)
        key[i] = ascii_lower(word_text[i]);
    key[n] = '\0';
    /* A lowercase dictionary headword such as "us" is not evidence for the
       letter sequence U.S. Explicit initialism pronunciations have their own
       namespace; otherwise construct letter phones below. */
    dictionary_kind = lexical == TTF_LEX_INITIALISM ? "initialism_pronunciation" : "pronunciation";
    dictionary_key = key;
    phones = ttf_resource_get(e, dictionary_kind, EN, key, &layer);
    if (lexical != TTF_LEX_INITIALISM) {
        ttf_resource_layer_t choice_layer = TTF_LAYER_COMMON;
        const char *choice = ttf_resource_get(e, "pronunciation_choice", EN, key, &choice_layer);
        if (choice && phones && choice_layer < layer)
            selection_reason = "higher-layer-base";
        else if (choice) {
            selection_reason = !strcmp(choice, "base") ? "explicit-base" : "explicit-choice";
            if ((status = ttf_trace_resource(e, "pronunciation_choice", EN, key, choice,
                                             selection_reason, SIZE_MAX)) != TTF_OK)
                return status;
            if (strcmp(choice, "base")) {
                (void)snprintf(variant_key_text, sizeof(variant_key_text), "%s#%s", key, choice);
                dictionary_kind = "pronunciation_variant";
                dictionary_key = variant_key_text;
                phones = ttf_resource_get(e, dictionary_kind, EN, dictionary_key, &layer);
                if (!phones)
                    return TTF_ERESOURCE; /* Freeze normally reports this dependency. */
            }
        }
    }
    if (phones) {
        if (strlen(phones) >= sizeof(out->phones))
            return TTF_ELIMIT;
        (void)snprintf(out->phones, sizeof(out->phones), "%s", phones);
        (void)snprintf(out->provenance, sizeof(out->provenance),
                       "ttf-pron-v1:%s:pronunciation-dictionary", ttf_layer_name(layer));
        return ttf_trace_resource(e, dictionary_kind, EN, dictionary_key, phones, selection_reason,
                                  0);
    }
    if (lexical == TTF_LEX_INITIALISM) {
        for (i = 0; i < n; ++i) {
            char symbolic[16], spelling[2];
            int written;
            if (key[i] < 'a' || key[i] > 'z')
                continue;
            (void)snprintf(symbolic, sizeof(symbolic), "letter.%c",
                           (char)toupper((unsigned char)key[i]));
            written =
                snprintf(out->phones + p, sizeof(out->phones) - p, "%s%s", p ? " " : "", symbolic);
            if (written < 0 || (size_t)written >= sizeof(out->phones) - p)
                return TTF_ELIMIT;
            p += (size_t)written;
            spelling[0] = word_text[i];
            spelling[1] = '\0';
            if ((status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, EN,
                                         "algorithm", "initialism-letter", symbolic, spelling,
                                         "generated-letter-phone", phone_index++)) != TTF_OK)
                return status;
        }
        (void)snprintf(out->provenance, sizeof(out->provenance), "ttf-pron-v1:common:initialism");
        return p ? TTF_OK : TTF_EINVAL;
    }
    if (lexical == TTF_LEX_ALNUM_TERM) {
        size_t upper = 0;
        while (upper < n && word_text[upper] >= 'A' && word_text[upper] <= 'Z')
            ++upper;
        if (upper >= 2) {
            for (i = 0; i < n; ++i) {
                char symbolic[16], digit_key[2] = {key[i], '\0'};
                int written;
                if (word_text[i] == '-')
                    continue;
                if (key[i] >= 'a' && key[i] <= 'z') {
                    (void)snprintf(symbolic, sizeof(symbolic), "letter.%c",
                                   (char)toupper((unsigned char)key[i]));
                    phones = symbolic;
                } else if (key[i] >= '0' && key[i] <= '9') {
                    phones = ttf_resource_get(e, "digit_pronunciation", EN, digit_key, &part_layer);
                    if (phones && part_layer > layer)
                        layer = part_layer;
                    if (!phones)
                        return TTF_EINVAL;
                } else
                    return TTF_EINVAL;
                written = snprintf(out->phones + p, sizeof(out->phones) - p, "%s%s", p ? " " : "",
                                   phones);
                if (written < 0 || (size_t)written >= sizeof(out->phones) - p)
                    return TTF_EINVAL;
                p += (size_t)written;
                if (key[i] >= '0' && key[i] <= '9')
                    status = ttf_trace_resource(e, "digit_pronunciation", EN, digit_key, phones,
                                                "alnum-digit", phone_index);
                else
                    status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, EN,
                                            "algorithm", "alnum-letter", phones, digit_key,
                                            "generated-letter-phone", phone_index);
                if (status != TTF_OK)
                    return status;
                phone_index += phone_count(phones);
            }
            (void)snprintf(out->provenance, sizeof(out->provenance),
                           "ttf-pron-v1:%s:alnum-initialism", ttf_layer_name(layer));
            return p ? TTF_OK : TTF_EINVAL;
        }
    }
    for (i = 0; i < n;) {
        size_t take = n - i;
        char chunk[8];
        int written;
        /* Five letters is what an English suffix needs to be written whole
           (-ation, -reaso-); the validator holds consumers to the same bound. */
        if (take > 5)
            take = 5;
        phones = NULL;
        for (; take; --take) {
            /* Anchored keys let the data express word-initial and word-final
               context (^pre, e$) without a second matcher. */
            char anchored[10];
            const char *forms[3];
            size_t form, count_forms = 0;
            memcpy(chunk, key + i, take);
            chunk[take] = '\0';
            if (!i && i + take == n) {
                snprintf(anchored, sizeof(anchored), "^%s$", chunk);
                forms[count_forms++] = anchored;
            }
            if (i + take == n) {
                static char final_key[10];
                snprintf(final_key, sizeof(final_key), "%s$", chunk);
                forms[count_forms++] = final_key;
            }
            if (!i) {
                static char initial_key[10];
                snprintf(initial_key, sizeof(initial_key), "^%s", chunk);
                forms[count_forms++] = initial_key;
            }
            for (form = 0; form < count_forms && !phones; ++form)
                phones = ttf_resource_get(e, "g2p", EN, forms[form], &part_layer);
            if (!phones)
                phones = ttf_resource_get(e, "g2p", EN, chunk, &part_layer);
            if (phones) {
                if (part_layer > layer)
                    layer = part_layer;
                break;
            }
        }
        if (!phones)
            return TTF_EINVAL;
        /* "-" marks a silent grapheme such as a word-final e. */
        if (strcmp(phones, "-")) {
            written =
                snprintf(out->phones + p, sizeof(out->phones) - p, "%s%s", p ? " " : "", phones);
            if (written < 0 || (size_t)written >= sizeof(out->phones) - p)
                return TTF_ELIMIT;
            p += (size_t)written;
        }
        if ((status = ttf_trace_resource(e, "g2p", EN, chunk, phones, "longest-grapheme-rule",
                                         phone_index)) != TTF_OK)
            return status;
        phone_index += phone_count(phones);
        i += take;
    }
    (void)snprintf(out->provenance, sizeof(out->provenance),
                   "ttf-pron-v1:%s:grapheme-g2p-prototype", ttf_layer_name(layer));
    return p ? TTF_OK : TTF_EINVAL;
}

const ttf_language_pack_t ttf_pack_en_US = {sizeof(ttf_language_pack_t),
                                            TTF_ABI_VERSION,
                                            EN,
                                            NULL,
                                            script_score,
                                            context_score,
                                            classify,
                                            verbalize,
                                            pronounce,
                                            NULL,
                                            NULL};
