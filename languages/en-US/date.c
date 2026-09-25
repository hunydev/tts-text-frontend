#include "date.h"
#include "../../src/semantic/numeric_grouping.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EN "en-US"

typedef struct {
    size_t end, digits;
    unsigned value;
    int valid, ordinal;
} date_number_t;

static int ascii_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int ascii_digit(char c) {
    return c >= '0' && c <= '9';
}

static char ascii_lower(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c;
}

static int horizontal(char c) {
    return c == ' ' || c == '\t';
}

static size_t skip_horizontal(const char *text, size_t length, size_t p) {
    while (p < length && horizontal(text[p]))
        ++p;
    return p;
}

static void merge_layer(ttf_resource_layer_t *into, ttf_resource_layer_t layer) {
    if (layer > *into)
        *into = layer;
}

static int unicode_tail(const ttf_pack_input_t *in, size_t p) {
    size_t low = in->index, high = in->token_count;
    if (p >= in->length || (unsigned char)in->text[p] < 128)
        return 0;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (in->tokens[middle].end <= p)
            low = middle + 1;
        else
            high = middle;
    }
    if (low < in->token_count && in->tokens[low].begin <= p)
        return in->tokens[low].script == TTF_SCRIPT_LATIN ||
               in->tokens[low].script == TTF_SCRIPT_DIGIT ||
               in->tokens[low].script == TTF_SCRIPT_SYMBOL;
    return 0;
}

/* Deliberately ASCII and locale independent: a name must be a complete word.
   The segmenter's shared resource marks aliases which accept a trailing dot. */
static size_t month_at(const ttf_pack_input_t *in, size_t p, unsigned *month,
                       ttf_resource_layer_t *layer) {
    char key[32], *end_value;
    const char *value;
    size_t end = p, i;
    long number;
    ttf_resource_layer_t found_layer;
    while (end < in->length && ascii_alpha(in->text[end]))
        ++end;
    if (end == p || end - p >= sizeof(key))
        return p;
    if (end < in->length &&
        (unicode_tail(in, end) || ascii_digit(in->text[end]) || in->text[end] == '_'))
        return p;
    for (i = p; i < end; ++i)
        key[i - p] = ascii_lower(in->text[i]);
    key[end - p] = '\0';
    value = ttf_resource_get(in->engine, "month_alias", EN, key, &found_layer);
    if (!value)
        return p;
    number = strtol(value, &end_value, 10);
    if (!*value || *end_value || number < 1 || number > 12)
        return p;
    *month = (unsigned)number;
    merge_layer(layer, found_layer);
    if (end < in->length && in->text[end] == '.') {
        value = ttf_resource_get(in->engine, "boundary_period", EN, key, &found_layer);
        if (value && strcmp(value, "0")) {
            merge_layer(layer, found_layer);
            ++end;
        }
    }
    return end;
}

/* Attached numeric tails are part of a malformed candidate. Sentence periods,
   commas and brackets are boundaries, except punctuation directly followed by
   another identifier character, e.g. 2026.5, 2026_foo or 2026/09. */
static int attached_tail(const ttf_pack_input_t *in, size_t p) {
    const char *text = in->text;
    size_t length = in->length;
    char c;
    if (p >= length)
        return 0;
    c = text[p];
    /* A calendar field cannot end at the first group of a typographic number.
       Commas remain the named-date day/year delimiter; only Unicode numeric
       spaces extend this malformed-field guard. */
    if ((unsigned char)c >= 128 && ttf_numeric_group_link(in->engine, text, length, p))
        return 1;
    if (ascii_alpha(c) || ascii_digit(c) || unicode_tail(in, p) || c == '_' || c == '/' ||
        c == '\\' || c == '-' || c == '+' || c == '%' || c == '$' || c == '#')
        return 1;
    return c == '.' && p + 1 < length &&
           (ascii_alpha(text[p + 1]) || ascii_digit(text[p + 1]) || text[p + 1] == '_');
}

static size_t malformed_end(const ttf_pack_input_t *in, size_t p) {
    while (p < in->length) {
        size_t linked = (unsigned char)in->text[p] >= 128
                            ? ttf_numeric_group_link(in->engine, in->text, in->length, p)
                            : 0;
        if (linked)
            p = linked;
        else if (attached_tail(in, p))
            ++p;
        else
            break;
    }
    return p;
}

static const char *ordinal_suffix(unsigned n) {
    if (n % 100 >= 11 && n % 100 <= 13)
        return "th";
    if (n % 10 == 1)
        return "st";
    if (n % 10 == 2)
        return "nd";
    if (n % 10 == 3)
        return "rd";
    return "th";
}

static date_number_t number_at(const ttf_pack_input_t *in, size_t p, int allow_ordinal) {
    date_number_t number = {p, 0, 0, 1, 0};
    size_t letters;
    while (number.end < in->length && ascii_digit(in->text[number.end])) {
        if (number.digits < 5)
            number.value = number.value * 10 + (unsigned)(in->text[number.end] - '0');
        ++number.digits;
        ++number.end;
    }
    if (!number.digits || number.digits > 4)
        number.valid = 0;
    letters = number.end;
    while (number.end < in->length && ascii_alpha(in->text[number.end]))
        ++number.end;
    if (number.end > letters) {
        const char *suffix = ordinal_suffix(number.value);
        number.ordinal = 1;
        if (!allow_ordinal || number.end - letters != 2 ||
            ascii_lower(in->text[letters]) != suffix[0] ||
            ascii_lower(in->text[letters + 1]) != suffix[1])
            number.valid = 0;
    }
    if (attached_tail(in, number.end)) {
        number.valid = 0;
        number.end = malformed_end(in, number.end);
    }
    return number;
}

static int valid_day(unsigned year, unsigned month, unsigned day) {
    static const unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned maximum;
    if (!month || month > 12 || !day)
        return 0;
    maximum = days[month - 1];
    if (month == 2 && (!year || (!(year % 4) && (year % 100 || !(year % 400)))))
        ++maximum;
    return day <= maximum;
}

static size_t finish(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t end, int valid,
                     ttf_resource_layer_t layer, const char *rule) {
    size_t i = in->index;
    out->type = valid ? TTF_SEM_DATE : TTF_SEM_TEXT;
    out->input_end = end;
    out->layer = layer;
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "%s", EN);
    (void)snprintf(out->rule, sizeof(out->rule), "%s", valid ? rule : "en.date.invalid");
    if (!valid) {
        out->value[0] = '\0';
        out->secondary[0] = '\0';
        out->unit[0] = '\0';
    }
    while (i < in->token_count && in->tokens[i].begin < end)
        ++i;
    return i - in->index;
}

static int left_boundary(const ttf_pack_input_t *in, size_t start) {
    unsigned char c;
    if (!start)
        return 1;
    c = (unsigned char)in->text[start - 1];
    if (c >= 128)
        return in->index && in->tokens[in->index - 1].end == start &&
               in->tokens[in->index - 1].script == TTF_SCRIPT_PUNCTUATION;
    return !ascii_alpha((char)c) && !ascii_digit((char)c) && c < 128 && c != '_' && c != '-' &&
           c != '+' && c != '$' && c != '#' && c != '/' && c != '\\' && c != '.';
}

/* Returns an optional numeric year after a day or month. A comma is not part
   of the candidate unless a numeric year follows it. No newline is crossed. */
static int optional_year(const ttf_pack_input_t *in, size_t end, date_number_t *year) {
    size_t p = skip_horizontal(in->text, in->length, end);
    if (p < in->length && in->text[p] == ',')
        p = skip_horizontal(in->text, in->length, p + 1);
    else if (p == end)
        return 0;
    if (p == in->length || !ascii_digit(in->text[p]))
        return 0;
    *year = number_at(in, p, 0);
    return 1;
}

static size_t named_date(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t start) {
    unsigned month = 0, year_value = 0;
    size_t p, q, end;
    int month_first, has_year, valid;
    date_number_t day, year;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    p = month_at(in, start, &month, &layer);
    month_first = p > start;
    if (month_first) {
        q = skip_horizontal(in->text, in->length, p);
        if (q == p || q == in->length || !ascii_digit(in->text[q]))
            return 0;
        day = number_at(in, q, 1);
        if (day.digits >= 3) {
            valid = day.valid && !day.ordinal && day.digits == 4 && day.value != 0;
            if (valid) {
                (void)snprintf(out->value, sizeof(out->value), "%04u-%02u", day.value, month);
                (void)snprintf(out->unit, sizeof(out->unit), "year-month");
                (void)snprintf(out->secondary, sizeof(out->secondary), "mdy");
            }
            return finish(in, out, day.end, valid, layer, "en.date.named-month-year");
        }
        end = day.end;
    } else {
        if (!ascii_digit(in->text[start]))
            return 0;
        day = number_at(in, start, 1);
        q = skip_horizontal(in->text, in->length, day.end);
        if (q == day.end)
            return 0;
        end = month_at(in, q, &month, &layer);
        if (end == q)
            return 0;
        if (attached_tail(in, end)) {
            end = malformed_end(in, end);
            day.valid = 0;
        }
    }
    has_year = optional_year(in, end, &year);
    valid = day.valid && day.digits <= 2;
    if (has_year) {
        end = year.end;
        year_value = year.value;
        valid = valid && year.valid && year.digits == 4 && year.value != 0;
    }
    valid = valid && valid_day(year_value, month, day.value);
    if (valid) {
        if (has_year)
            (void)snprintf(out->value, sizeof(out->value), "%04u-%02u-%02u", year_value, month,
                           day.value);
        else {
            (void)snprintf(out->value, sizeof(out->value), "--%02u-%02u", month, day.value);
            (void)snprintf(out->unit, sizeof(out->unit), "month-day");
        }
        (void)snprintf(out->secondary, sizeof(out->secondary), "%s", month_first ? "mdy" : "dmy");
    }
    return finish(in, out, end, valid, layer, "en.date.named");
}

static unsigned policy_year(const ttf_pack_input_t *in, const char *key, unsigned fallback,
                            ttf_resource_layer_t *layer) {
    ttf_resource_layer_t found_layer;
    char *end;
    const char *text = ttf_resource_get(in->engine, "date_policy", EN, key, &found_layer);
    long value;
    if (!text)
        return fallback;
    value = strtol(text, &end, 10);
    if (!*text || *end || value < 1 || value > 9999)
        return fallback;
    merge_layer(layer, found_layer);
    return (unsigned)value;
}

/* "( 1965 )" is the citation shape: a bracket holding the number and nothing
   else introduces a year. "( 2003 and 2005 )" is an ordinary parenthesis and
   keeps its cardinals, so the closing bracket has to follow the number. */
static int bracket_cue(const ttf_pack_input_t *in, size_t start, size_t stop,
                       ttf_resource_layer_t *layer) {
    static const char *const pairs[] = {"()", "[]", "{}"};
    char key[2];
    size_t p = start, q = skip_horizontal(in->text, in->length, stop), i;
    ttf_resource_layer_t found_layer;
    const char *value;
    while (p && horizontal(in->text[p - 1]))
        --p;
    if (!p || q >= in->length)
        return 0;
    key[0] = in->text[p - 1];
    key[1] = '\0';
    for (i = 0; i < sizeof(pairs) / sizeof(pairs[0]); ++i)
        if (key[0] == pairs[i][0] && in->text[q] == pairs[i][1])
            break;
    if (i == sizeof(pairs) / sizeof(pairs[0]))
        return 0;
    value = ttf_resource_get(in->engine, "date_year_cue", EN, key, &found_layer);
    if (!value || !strcmp(value, "0"))
        return 0;
    merge_layer(layer, found_layer);
    return 1;
}

static int preceding_cue(const ttf_pack_input_t *in, size_t start, size_t stop,
                         ttf_resource_layer_t *layer) {
    char key[65];
    size_t end = start, begin, p, used;
    ttf_resource_layer_t found_layer;
    const char *value;
    if (bracket_cue(in, start, stop, layer))
        return 1;
    if (!start || !horizontal(in->text[start - 1]))
        return 0;
    while (end && horizontal(in->text[end - 1]))
        --end;
    begin = end;
    while (begin && end - begin < 64 &&
           (ascii_alpha(in->text[begin - 1]) || horizontal(in->text[begin - 1])))
        --begin;
    for (; begin < end; ++begin) {
        if (!ascii_alpha(in->text[begin]) ||
            (begin && (ascii_alpha(in->text[begin - 1]) || ascii_digit(in->text[begin - 1]) ||
                       in->text[begin - 1] == '_' || in->text[begin - 1] == '-')))
            continue;
        used = 0;
        for (p = begin; p < end; ++p) {
            if (horizontal(in->text[p])) {
                if (used && key[used - 1] != ' ')
                    key[used++] = ' ';
            } else
                key[used++] = ascii_lower(in->text[p]);
        }
        key[used] = '\0';
        value = ttf_resource_get(in->engine, "date_year_cue", EN, key, &found_layer);
        if (value && strcmp(value, "0")) {
            merge_layer(layer, found_layer);
            return 1;
        }
    }
    return 0;
}

static int following_quantity(const ttf_pack_input_t *in, size_t end) {
    size_t p = skip_horizontal(in->text, in->length, end), used = 0;
    char key[64];
    const char *value;
    while (p < in->length && ascii_alpha(in->text[p])) {
        if (used + 1 == sizeof(key))
            return 1;
        key[used++] = ascii_lower(in->text[p++]);
    }
    if (!used)
        return 0;
    key[used] = '\0';
    value = ttf_resource_get(in->engine, "date_year_quantity", EN, key, NULL);
    return value && strcmp(value, "0");
}

/* "from 1965 to 1970", "between 1914 and 1918": the joining word alone is not
   evidence, but a four-digit year immediately before it is. */
static int interval_continuation(const ttf_pack_input_t *in, size_t start,
                                 ttf_resource_layer_t *layer) {
    size_t end = start, begin, digits = 0, p;
    char key[32];
    ttf_resource_layer_t found = TTF_LAYER_COMMON;
    const char *value;
    while (end && horizontal(in->text[end - 1]))
        --end;
    begin = end;
    while (begin && ascii_alpha(in->text[begin - 1]) && end - begin < sizeof(key) - 1)
        --begin;
    if (begin == end || end - begin >= sizeof(key))
        return 0;
    for (p = begin; p < end; ++p)
        key[p - begin] = ascii_lower(in->text[p]);
    key[end - begin] = 0;
    value = ttf_resource_get(in->engine, "date_year_interval", EN, key, &found);
    if (!value || strcmp(value, "1"))
        return 0;
    end = begin;
    while (end && horizontal(in->text[end - 1]))
        --end;
    begin = end;
    while (begin && ascii_digit(in->text[begin - 1])) {
        --begin;
        ++digits;
    }
    if (digits != 4 || (begin && (ascii_alpha(in->text[begin - 1]) || in->text[begin - 1] == '.' ||
                                  in->text[begin - 1] == ',')))
        return 0;
    /* The first endpoint must itself be a cued year; two bare numbers in a list
       stay cardinals. */
    if (!preceding_cue(in, begin, end, layer))
        return 0;
    merge_layer(layer, found);
    return 1;
}

/* "the 1960s", "the 80s", "the '90s": a decade is a plural year. The digits end
   in a zero, so 1965s and a model-number plural such as 787s are left alone. */
static size_t decade(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t start) {
    size_t p = start, digits = 0;
    unsigned value = 0, minimum, maximum;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    if (p < in->length && in->text[p] == '\'')
        ++p;
    while (p + digits < in->length && ascii_digit(in->text[p + digits])) {
        value = value * 10 + (unsigned)(in->text[p + digits] - '0');
        ++digits;
    }
    if ((digits != 4 && digits != 2) || value % 10 || p + digits >= in->length ||
        in->text[p + digits] != 's')
        return 0;
    if (p + digits + 1 < in->length &&
        (ascii_alpha(in->text[p + digits + 1]) || ascii_digit(in->text[p + digits + 1])))
        return 0;
    if (digits == 4) {
        minimum = policy_year(in, "min_year", 1000, &layer);
        maximum = policy_year(in, "max_year", 2099, &layer);
        if (value < minimum || value > maximum)
            return 0;
    }
    (void)snprintf(out->value, sizeof(out->value), "%u", value);
    (void)snprintf(out->unit, sizeof(out->unit), "decade");
    return finish(in, out, p + digits + 1, 1, layer, "en.date.decade");
}

static size_t contextual_year(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t start) {
    date_number_t number;
    size_t consumed;
    unsigned minimum, maximum;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    if (!ascii_digit(in->text[start]))
        return 0;
    number = number_at(in, start, 0);
    if (!number.valid || number.digits != 4 || following_quantity(in, number.end) ||
        (!preceding_cue(in, start, number.end, &layer) &&
         !interval_continuation(in, start, &layer)))
        return 0;
    if (number.end < in->length && in->text[number.end] == ',' && number.end + 1 < in->length &&
        ascii_digit(in->text[number.end + 1]))
        return 0;
    minimum = policy_year(in, "min_year", 1000, &layer);
    maximum = policy_year(in, "max_year", 2099, &layer);
    if (number.value < minimum || number.value > maximum)
        return 0;
    (void)snprintf(out->value, sizeof(out->value), "%04u", number.value);
    (void)snprintf(out->unit, sizeof(out->unit), "year");
    consumed = finish(in, out, number.end, 1, layer, "en.date.context-year");
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "und");
    return consumed;
}

size_t ttf_en_classify_date(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    size_t start, named;
    if (!in || !out || in->index >= in->token_count)
        return 0;
    start = in->tokens[in->index].begin;
    if (start >= in->length || !left_boundary(in, start))
        return 0;
    named = named_date(in, out, start);
    if (named)
        return named;
    named = decade(in, out, start);
    return named ? named : contextual_year(in, out, start);
}
