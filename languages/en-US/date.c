#include "date.h"
#include "../../src/core/internal.h"
#include "../../src/semantic/numeric_grouping.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
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
    /* 1899--seventeen: a doubled hyphen before a word is a dash, not a tail. */
    if (c == '-' && p + 2 < length && text[p + 1] == '-' && ascii_alpha(text[p + 2]))
        return 0;
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
    /* 21 January 19 44: a year written as its century and the rest (the
       references read nineteen forty four; 2026-10-07). */
    if (year->valid && year->digits == 2 && year->value >= 10 && year->value <= 20) {
        size_t q = skip_horizontal(in->text, in->length, year->end);
        if (q > year->end && q + 2 <= in->length && ascii_digit(in->text[q]) &&
            ascii_digit(in->text[q + 1]) && (q + 2 == in->length || !ascii_digit(in->text[q + 2]))) {
            date_number_t rest = number_at(in, q, 0);
            if (rest.valid && rest.digits == 2) {
                year->value = year->value * 100 + rest.value;
                year->digits = 4;
                year->end = rest.end;
            }
        }
    }
    return 1;
}

static int article_before(const ttf_pack_input_t *in, size_t start) {
    size_t p = start;
    while (p && horizontal(in->text[p - 1]))
        --p;
    return p != start && p >= 3 && (in->text[p - 3] | 0x20) == 't' &&
           (in->text[p - 2] | 0x20) == 'h' && (in->text[p - 1] | 0x20) == 'e' &&
           (p == 3 || !ascii_alpha(in->text[p - 4]));
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
        /* 2007 Apr 24, Epub 2014 Aug 2: year, month, day (the references read
           them in that order; 2026-10-07). */
        if (day.valid && !day.ordinal && day.digits == 4) {
            size_t r = skip_horizontal(in->text, in->length, end);
            date_number_t date_day;
            if (r > end && r < in->length && ascii_digit(in->text[r])) {
                date_day = number_at(in, r, 0);
                if (date_day.valid && date_day.digits <= 2 && date_day.value &&
                    (date_day.end == in->length || (!ascii_digit(in->text[date_day.end]) &&
                                                    !ascii_alpha(in->text[date_day.end]) &&
                                                    in->text[date_day.end] != ':')) &&
                    valid_day(day.value, month, date_day.value)) {
                    (void)snprintf(out->value, sizeof(out->value), "%04u-%02u-%02u", day.value,
                                   month, date_day.value);
                    (void)snprintf(out->secondary, sizeof(out->secondary), "ymd");
                    return finish(in, out, date_day.end, 1, layer, "en.date.named-ymd");
                }
            }
        }
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
        /* She died July 18, 453: a three-digit year after a month and day too
           (2026-10-07). */
        valid = valid && year.valid && year.value != 0 &&
                (year.digits == 4 || (year.digits == 3 && year.value >= 100));
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
        /* the 9th October: the written article is the reading's article, so it is
           not said twice (2026-10-07). */
        (void)snprintf(out->secondary, sizeof(out->secondary), "%s",
                       month_first ? "mdy" : article_before(in, start) ? "dmyn" : "dmy");
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

/* "( 1965 )" and "Press , 1992 ." are the citation shapes: the marks on both
   sides of the number introduce a year together. "( 2003 and 2005 )" and a
   quantity list keep their cardinals, because the closing mark has to match.
   Each opening mark declares its closing set, and "$" in that set means the
   number may end the text instead. */
static int enclosure_cue(const ttf_pack_input_t *in, size_t start, size_t stop,
                         ttf_resource_layer_t *layer) {
    char key[2];
    size_t p = start, q = skip_horizontal(in->text, in->length, stop);
    ttf_resource_layer_t found_layer;
    const char *value;
    while (p && horizontal(in->text[p - 1]))
        --p;
    if (!p)
        return 0;
    key[0] = in->text[p - 1];
    key[1] = '\0';
    value = ttf_resource_get(in->engine, "date_year_enclosure", EN, key, &found_layer);
    if (!value || !*value)
        return 0;
    if (q >= in->length ? !strchr(value, '$') : !strchr(value, in->text[q]))
        return 0;
    merge_layer(layer, found_layer);
    return 1;
}

static int bracket_cue(const ttf_pack_input_t *in, size_t start, size_t stop,
                       ttf_resource_layer_t *layer) {
    char key[2];
    size_t p = start;
    ttf_resource_layer_t found_layer;
    const char *value;
    if (enclosure_cue(in, start, stop, layer))
        return 1;
    while (p && horizontal(in->text[p - 1]))
        --p;
    if (!p)
        return 0;
    key[0] = in->text[p - 1];
    key[1] = '\0';
    value = ttf_resource_get(in->engine, "date_year_cue", EN, key, &found_layer);
    if (!value || !strcmp(value, "0"))
        return 0;
    merge_layer(layer, found_layer);
    return 1;
}

/* "1830 : The first Constitution": a colon after a four-digit number is the
   timeline shape, so the mark itself introduces the year. The marks are data. */
static int following_cue(const ttf_pack_input_t *in, size_t end, ttf_resource_layer_t *layer) {
    char key[2];
    size_t p = skip_horizontal(in->text, in->length, end);
    ttf_resource_layer_t found_layer;
    const char *value;
    if (p >= in->length)
        return 0;
    key[0] = in->text[p];
    key[1] = '\0';
    value = ttf_resource_get(in->engine, "date_year_cue_after", EN, key, &found_layer);
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

/* "the 9th of April, 1865, Grant", "In June, 1812, the war": a four-digit
   number right after a month name and its comma is the year of that date when
   punctuation or the end follows it. A following word keeps the quantity test
   ("In April, 1500 soldiers") and the cardinal. */
static int month_cue(const ttf_pack_input_t *in, size_t start, size_t stop,
                     ttf_resource_layer_t *layer) {
    size_t p = start, begin, q = skip_horizontal(in->text, in->length, stop);
    unsigned month = 0;
    ttf_resource_layer_t found = TTF_LAYER_COMMON;
    if (q < in->length && !strchr(",.;:)]", in->text[q]))
        return 0;
    while (p && horizontal(in->text[p - 1]))
        --p;
    if (!p || in->text[p - 1] != ',')
        return 0;
    --p;
    while (p && horizontal(in->text[p - 1]))
        --p;
    begin = p;
    while (begin && ascii_alpha(in->text[begin - 1]))
        --begin;
    if (begin == p || (begin && (ascii_alpha(in->text[begin - 1]) || ascii_digit(in->text[begin - 1]))))
        return 0;
    if (month_at(in, begin, &month, &found) != p || !month)
        return 0;
    merge_layer(layer, found);
    return 1;
}

/* Uncued years apply to English text only, judged from the same bounded window
   the cue rules read, so a stream decides like a batch: on the line of the number,
   no Hangul, Han or Kana in the 64 bytes before it, and an English word there or
   right after the number (two in a non-English default, so 1234{@break:1} stays
   Korean). A lone number keeps its cardinal (2026), and so does a number alone on
   its line (in, a newline, 1998). */
static int english_text(const ttf_pack_input_t *in, size_t start, size_t end) {
    size_t from = start > 64 ? start - 64 : 0, p, q, words = 0;
    const char *lang = in->engine->config.default_language;
    for (p = start; p > from; --p) {
        unsigned char c = (unsigned char)in->text[p - 1];
        if (c == '\n' || c == '\r')
            break;
        if (c >= 0xE3 && c <= 0xED)
            return 0; /* a lead byte of Kana, Han or Hangul */
    }
    for (q = p; q < start; ++q)
        if (ascii_alpha(in->text[q]) && (q == p || !ascii_alpha(in->text[q - 1]))) {
            size_t w = q;
            while (w < start && ascii_alpha(in->text[w]))
                ++w;
            words += w - q >= 2;
        }
    q = skip_horizontal(in->text, in->length, end);
    p = q;
    while (q < in->length && ascii_alpha(in->text[q]))
        ++q;
    words += q - p >= 2;
    return words >= (lang && !strncmp(lang, "en", 2) ? 1u : 2u);
}

/* 1716. alone on its line, in an English default: a year heading (the English
   references read 33 such lines as years; 2026 without the period stays a
   cardinal; 2026-10-07). */
/* May. 2026: a month name that ends a sentence is not this number's cue, and
   the number keeps its cardinal (the v13 guards; now that a segment sees the
   previous one, 2026-10-07). */
static int month_sentence_before(const ttf_pack_input_t *in, size_t start) {
    size_t p = start, w;
    unsigned month = 0;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    while (p && horizontal(in->text[p - 1]))
        --p;
    if (!p || in->text[p - 1] != '.')
        return 0;
    w = --p;
    while (p && ascii_alpha(in->text[p - 1]))
        --p;
    return w > p && month_at(in, p, &month, &layer) == w && month;
}

static int year_line(const ttf_pack_input_t *in, size_t start, size_t end) {
    const char *lang = in->engine->config.default_language;
    size_t p = start;
    if (!lang || strncmp(lang, "en", 2))
        return 0;
    while (p && horizontal(in->text[p - 1]))
        --p;
    if (p && in->text[p - 1] != '\n' && in->text[p - 1] != '\r')
        return 0;
    if (end >= in->length || in->text[end] != '.')
        return 0;
    p = skip_horizontal(in->text, in->length, end + 1);
    return p == in->length || in->text[p] == '\n' || in->text[p] == '\r';
}

/* The word before a number, skipping earlier list members (1200 , 1400 and), so a
   list is judged by the word that introduces it. *second gets the word before. */
static int previous_words(const ttf_pack_input_t *in, size_t start, char *first, char *second, size_t size) {
    size_t p = start, k;
    char *words[2] = {first, second};
    first[0] = second[0] = '\0';
    for (;;) {
        size_t q = p, digits = 0;
        while (q && horizontal(in->text[q - 1]))
            --q;
        if (q && in->text[q - 1] == ',')
            --q;
        else if (q >= 3 && (!memcmp(in->text + q - 3, "and", 3) || !memcmp(in->text + q - 3, " or", 3)))
            q -= 3;
        else
            break;
        while (q && horizontal(in->text[q - 1]))
            --q;
        while (q && ascii_digit(in->text[q - 1])) {
            --q;
            ++digits;
        }
        if (digits != 4)
            break;
        p = q;
    }
    for (k = 0; k < 2; ++k) {
        size_t end, begin, n;
        while (p && (horizontal(in->text[p - 1]) || in->text[p - 1] == ','))
            --p;
        end = p;
        while (p && ascii_alpha(in->text[p - 1]))
            --p;
        begin = p;
        if (begin == end || end - begin >= size)
            break;
        for (n = 0; n < end - begin; ++n)
            words[k][n] = ascii_lower(in->text[begin + n]);
        words[k][n] = '\0';
    }
    return first[0] != '\0';
}

/* page 1965, rose to 1965, paid 1865, scores were 1200 , 1400 and 1600: a label or
   a quantity word before the number keeps the cardinal (date_year_label_before,
   date_year_quantity_before). */
static int quantity_before(const ttf_pack_input_t *in, size_t start) {
    char first[32], second[32];
    if (!previous_words(in, start, first, second, sizeof(first)))
        return 0;
    /* "is 2026." opening a segment: the word that decides (The count is) is in
       an earlier segment, so a lone copula keeps the cardinal (2026-10-07). */
    if (!second[0] && (!strcmp(first, "is") || !strcmp(first, "was") || !strcmp(first, "are") ||
                       !strcmp(first, "were") || !strcmp(first, "be"))) {
        size_t p = start;
        while (p && (horizontal(in->text[p - 1]) || in->text[p - 1] == ','))
            --p;
        while (p && ascii_alpha(in->text[p - 1]))
            --p;
        while (p && horizontal(in->text[p - 1]))
            --p;
        if (!p)
            return 1;
    }
    return ttf_resource_get(in->engine, "date_year_label_before", EN, first, NULL) ||
           ttf_resource_get(in->engine, "date_year_quantity_before", EN, first, NULL) ||
           (second[0] && ttf_resource_get(in->engine, "date_year_quantity_before", EN, second, NULL));
}

static size_t contextual_year(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t start) {
    date_number_t number;
    size_t consumed;
    unsigned minimum, maximum;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    if (!ascii_digit(in->text[start]))
        return 0;
    int cued;
    number = number_at(in, start, 0);
    if (!number.valid || number.digits != 4 || following_quantity(in, number.end))
        return 0;
    cued = preceding_cue(in, start, number.end, &layer) || following_cue(in, number.end, &layer) ||
           interval_continuation(in, start, &layer) || month_cue(in, start, number.end, &layer);
    /* Without a cue, a value from uncued_min_year up is still a year (Act of 1964,
       a closing "1894."; 2026-10-07). */
    if (!cued && (number.value < policy_year(in, "uncued_min_year", 10000, &layer) ||
                  !(english_text(in, start, number.end) || year_line(in, start, number.end)) ||
                  quantity_before(in, start) || month_sentence_before(in, start)))
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

/* 1083-1093 after an issue number "37 (6)", or after p., pp., page(s): a page range. */
static int page_cue(const ttf_pack_input_t *in, size_t start) {
    size_t q = start, b;
    char word[8];
    size_t n = 0;
    while (q && (in->text[q - 1] == ' ' || in->text[q - 1] == '\t'))
        --q;
    if (q && in->text[q - 1] == ')')
        return 1;
    /* Neuropsychologia, 41, 1422 - 1429; 37:1083-1093: a volume number before the pages */
    if (q && (in->text[q - 1] == ',' || in->text[q - 1] == ':')) {
        size_t d = q - 1;
        while (d && (in->text[d - 1] == ' ' || in->text[d - 1] == '\t'))
            --d;
        if (d && ascii_digit(in->text[d - 1]))
            return 1;
    }
    b = q;
    while (b && (isalpha((unsigned char)in->text[b - 1]) || in->text[b - 1] == '.') && q - b < sizeof(word) - 1)
        --b;
    for (n = 0; b + n < q && n < sizeof(word) - 1; ++n)
        word[n] = (char)tolower((unsigned char)in->text[b + n]);
    word[n] = 0;
    return !strcmp(word, "p.") || !strcmp(word, "pp.") || !strcmp(word, "pp") || !strcmp(word, "page") ||
           !strcmp(word, "pages");
}

/* 1245 - 1251, Dec. 1987: a citation gives its year right after the page range. */
static int year_follows(const ttf_pack_input_t *in, size_t from) {
    size_t q, limit = from + 20 < in->length ? from + 20 : in->length;
    for (q = from; q + 4 <= limit; ++q)
        if (ascii_digit(in->text[q]) && ascii_digit(in->text[q + 1]) && ascii_digit(in->text[q + 2]) &&
            ascii_digit(in->text[q + 3]) && !(q && ascii_digit(in->text[q - 1])) &&
            !(q + 4 < in->length && ascii_digit(in->text[q + 4])) &&
            (in->text[q] == '1' || in->text[q] == '2'))
            return 1;
    return 0;
}

/* "from 1988 - 93", "the 1988 - 1993 period": the word form already reads both
   endpoints as years, and the hyphen form should too. The cue rule is unchanged,
   so an uncued pair and a page range keep their cardinals. */
static size_t year_range(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t start) {
    date_number_t first, second;
    size_t p, consumed;
    int slash = 0;
    unsigned minimum, maximum;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    if (!ascii_digit(in->text[start]))
        return 0;
    /* number_at treats an attached hyphen as a malformed field, which is exactly
       the shape here, so the first endpoint is read directly. */
    first.end = start;
    first.value = 0;
    first.digits = 0;
    first.valid = 1;
    first.ordinal = 0;
    while (first.end < in->length && ascii_digit(in->text[first.end])) {
        first.value = first.value * 10 + (unsigned)(in->text[first.end] - '0');
        ++first.digits;
        ++first.end;
    }
    if (first.digits != 4)
        return 0;
    /* built 1998-2004: two years of 1900-2099 in order read as years without a cue (owner
       sets, 2026-10-14); other pairs still need one (a page range keeps its cardinals). */
    {
        /* 1988 / 1989 season, 1941/42: a slash between a year and a later year reads like the
           hyphen (2026-11-10, real-text cycle R8; it was nineteen eighty eight over …) */
        size_t r = skip_horizontal(in->text, in->length, first.end), k;
        unsigned later = 0;
        if (r < in->length && in->text[r] == '/') {
            r = skip_horizontal(in->text, in->length, r + 1);
            for (k = 0; k < 4 && r + k < in->length && ascii_digit(in->text[r + k]); ++k)
                later = later * 10 + (unsigned)(in->text[r + k] - '0');
            if ((r + k < in->length && ascii_digit(in->text[r + k])) ||
                !((k == 4 && later > first.value && later <= first.value + 100) ||
                  (k == 2 && later > first.value % 100)) ||
                first.value < 1000 || first.value > 2099 ||
                !(in->engine->config.default_language &&
                  !strncmp(in->engine->config.default_language, "en", 2)) ||
                !english_text(in, start, r + k) || page_cue(in, start))
                return 0;
            slash = 1;
        }
    }
    if (!slash && !preceding_cue(in, start, first.end, &layer)) {
        size_t r = skip_horizontal(in->text, in->length, first.end), k;
        unsigned second_year = 0;
        if (first.value < 1000 || first.value > 2099 || r >= in->length || in->text[r] != '-')
            return 0;
        /* the extension below 1900 is for English requests only: 1588-1688 is a Korean
           service number */
        if (first.value < 1900 && !(in->engine->config.default_language &&
                                    !strncmp(in->engine->config.default_language, "en", 2)))
            return 0;
        r = skip_horizontal(in->text, in->length, r + 1);
        for (k = 0; k < 4 && r + k < in->length && ascii_digit(in->text[r + k]); ++k)
            second_year = second_year * 10 + (unsigned)(in->text[r + k] - '0');
        /* War Diary: 1914 - 15, 1897 - 99: an English sentence's year with the next years'
           last two digits (2026-11-10, real-text cycle R8; it was one thousand nine hundred
           fourteen to fifteen). A bare 1988 - 93 keeps its cardinals. */
        if (k == 2 && !(r + 2 < in->length && ascii_digit(in->text[r + 2])) &&
            second_year > first.value % 100 && in->engine->config.default_language &&
            !strncmp(in->engine->config.default_language, "en", 2) &&
            english_text(in, start, r + 2) && !page_cue(in, start) && !year_follows(in, r + 2)) {
            /* read below */
        } else if (k != 4 || (r + 4 < in->length && ascii_digit(in->text[r + 4])) ||
                   second_year <= first.value || second_year > 2099)
            return 0;
        /* (1857 - 1952), Infantry, 1861 - 1864: before 1900 a pair reads as years too when no
           page cue precedes it (2026-11-09, real-text cycle R1; 55% of such pairs in English
           Wikipedia text were read as cardinals). The 150-year limit was dropped in cycle R8
           (Ohio authors, 1796 - 1950; 1470 - 1900): pages rarely span centuries. */
        else if (first.value < 1900 && (page_cue(in, start) || year_follows(in, r + 4)))
            return 0;
    }
    p = skip_horizontal(in->text, in->length, first.end);
    if (p >= in->length || (in->text[p] != '-' && !(slash && in->text[p] == '/')))
        return 0;
    p = skip_horizontal(in->text, in->length, p + 1);
    second = number_at(in, p, 0);
    if (!second.valid || (second.digits != 2 && second.digits != 4) ||
        following_quantity(in, second.end))
        return 0;
    minimum = policy_year(in, "min_year", 1000, &layer);
    maximum = policy_year(in, "max_year", 2099, &layer);
    if (first.value < minimum || first.value > maximum)
        return 0;
    if (second.digits == 4 && (second.value < minimum || second.value > maximum))
        return 0;
    (void)snprintf(out->value, sizeof(out->value), "%04u", first.value);
    (void)snprintf(out->secondary, sizeof(out->secondary), "%0*u", (int)second.digits,
                   second.value);
    (void)snprintf(out->unit, sizeof(out->unit), "year");
    consumed = finish(in, out, second.end, 1, layer, "en.date.year-range");
    out->type = TTF_SEM_RANGE;
    (void)snprintf(out->source_lang, sizeof(out->source_lang), "und");
    return consumed;
}

/* A weekday spelling is only a calendar label in an adjacent date(label).
   Reuse the date classifiers on a bounded lookbehind; ordinary sun/sat/wed
   and malformed dates must not become weekdays. Keep parentheses separate
   so bracket read/preserve/skip still owns their contents. */
static size_t bracketed_weekday(const ttf_pack_input_t *in, ttf_semantic_t *out,
                                size_t start) {
    char key[32];
    const char *value;
    size_t end = start, label_end, opening, i;
    ttf_resource_layer_t layer;
    if (!start || in->text[start - 1] != '(' ||
        in->tokens[in->index].script != TTF_SCRIPT_LATIN)
        return 0;
    while (end < in->length && ascii_alpha(in->text[end]))
        ++end;
    if (end == start || end - start >= sizeof(key))
        return 0;
    label_end = end;
    if (end < in->length && in->text[end] == '.')
        ++end;
    if (end >= in->length || in->text[end] != ')')
        return 0;
    for (i = start; i < label_end; ++i)
        key[i - start] = ascii_lower(in->text[i]);
    key[label_end - start] = 0;
    value = ttf_resource_get(in->engine, "weekday_label", EN, key, &layer);
    if (!value || value[0] < '1' || value[0] > '7' || value[1])
        return 0;
    opening = start - 1;
    i = in->index;
    while (i) {
        ttf_pack_input_t probe = *in;
        ttf_semantic_t date = {0};
        size_t begin = in->tokens[--i].begin;
        if (opening - begin > 96)
            break;
        if (in->tokens[i].script != TTF_SCRIPT_DIGIT &&
            in->tokens[i].script != TTF_SCRIPT_LATIN)
            continue;
        probe.index = i;
        if (!left_boundary(&probe, begin))
            continue;
        (void)ttf_classify_universal(&probe, &date);
        if (date.type != TTF_SEM_DATE || date.input_end != opening ||
            (date.unit[0] && strcmp(date.unit, "month-day"))) {
            memset(&date, 0, sizeof(date));
            (void)named_date(&probe, &date, begin);
        }
        if (date.type != TTF_SEM_DATE || date.input_end != opening ||
            (date.unit[0] && strcmp(date.unit, "month-day")))
            continue;
        (void)snprintf(out->value, sizeof(out->value), "%s", value);
        (void)snprintf(out->unit, sizeof(out->unit), "weekday");
        (void)snprintf(out->secondary, sizeof(out->secondary), "%s", key);
        merge_layer(&layer, date.layer);
        return finish(in, out, end, 1, layer, "en.date.weekday");
    }
    return 0;
}

/* English text on this line: no Hangul, Han or Kana in the 64 bytes before or
   the 16 after, and an English default or an English word before. */
static int english_line(const ttf_pack_input_t *in, size_t start, size_t end) {
    size_t from = start > 64 ? start - 64 : 0, to = end + 16 < in->length ? end + 16 : in->length, p;
    const char *lang = in->engine->config.default_language;
    int word = 0;
    for (p = start; p > from; --p) {
        unsigned char c = (unsigned char)in->text[p - 1];
        if (c == '\n' || c == '\r')
            break;
        if (c >= 0xE3 && c <= 0xED)
            return 0;
        word |= ascii_alpha((char)c) && p >= 2 && ascii_alpha(in->text[p - 2]);
    }
    for (p = end; p < to && in->text[p] != '\n'; ++p)
        if ((unsigned char)in->text[p] >= 0xE3 && (unsigned char)in->text[p] <= 0xED)
            return 0;
    return word || (lang && !strncmp(lang, "en", 2));
}

/* 6/25/1940, 02/15/2017, 6/17/13: an English month/day/year reads month first,
   as the references do (10/02/2017 October second); a first field over 12 is the
   day (25/12/1999 the twenty fifth of December). A two-digit year reads as
   written (13 thirteen, 00 oh oh). English text only (2026-10-07). */
static size_t slash_month_day_year(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t start) {
    static const unsigned days[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    unsigned field[3] = {0, 0, 0}, digits[3] = {0, 0, 0}, month, day, swap;
    size_t p = start, k;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    for (k = 0; k < 3; ++k) {
        if (k) {
            if (p >= in->length || in->text[p] != '/')
                return 0;
            ++p;
        }
        while (p < in->length && ascii_digit(in->text[p]) && digits[k] < 5) {
            field[k] = field[k] * 10 + (unsigned)(in->text[p] - '0');
            ++digits[k];
            ++p;
        }
        if (!digits[k])
            return 0;
    }
    if (digits[0] > 2 || digits[1] > 2 || (digits[2] != 2 && digits[2] != 4))
        return 0;
    if (p < in->length &&
        (ascii_digit(in->text[p]) || ascii_alpha(in->text[p]) || in->text[p] == '/' ||
         ((in->text[p] == '.' || in->text[p] == '-' || in->text[p] == ':') && p + 1 < in->length &&
          ascii_digit(in->text[p + 1]))))
        return 0;
    month = field[0];
    day = field[1];
    swap = month > 12 && day <= 12;
    if (swap) {
        month = field[1];
        day = field[0];
    }
    if (!month || month > 12 || !day || day > days[month - 1] || !english_line(in, start, p))
        return 0;
    if (digits[2] == 4) {
        if (field[2] < policy_year(in, "min_year", 1000, &layer) ||
            field[2] > policy_year(in, "max_year", 2099, &layer))
            return 0;
        (void)snprintf(out->value, sizeof(out->value), "%04u-%02u-%02u", field[2], month, day);
    } else {
        (void)snprintf(out->value, sizeof(out->value), "%02u-%02u-%02u", field[2], month, day);
        (void)snprintf(out->unit, sizeof(out->unit), "short-year");
    }
    (void)snprintf(out->secondary, sizeof(out->secondary), "%s", swap ? "dmy" : "mdy");
    return finish(in, out, p, 1, layer, "en.date.slash");
}

/* ships 12/5, on 1/2, starts 2/1: a month/day without a year right after a date
   word (date_cue) is a date; a bare 1/2 stays a fraction (owner sets, 2026-10-14). */
static size_t slash_month_day_cue(const ttf_pack_input_t *in, ttf_semantic_t *out, size_t start) {
    static const unsigned days[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const char *t = in->text;
    size_t n = in->length, p = start, q, w, k;
    unsigned month = 0, day = 0, md = 0, dd = 0;
    char key[24];
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    while (p < n && ascii_digit(t[p]) && md < 3)
        month = month * 10 + (unsigned)(t[p++] - '0'), ++md;
    if (!md || md > 2 || p >= n || t[p] != '/')
        return 0;
    ++p;
    while (p < n && ascii_digit(t[p]) && dd < 3)
        day = day * 10 + (unsigned)(t[p++] - '0'), ++dd;
    if (!dd || dd > 2 || !month || month > 12 || !day || day > days[month - 1])
        return 0;
    if (p < n && (ascii_digit(t[p]) || ascii_alpha(t[p]) || t[p] == '/' ||
                  ((t[p] == '.' || t[p] == ',' || t[p] == '-') && p + 1 < n && ascii_digit(t[p + 1]))))
        return 0;
    for (q = start; q && t[q - 1] == ' '; --q)
        ;
    for (w = q; w && ascii_alpha(t[w - 1]); --w)
        ;
    if (w == q || q == start || q - w >= sizeof(key))
        return 0;
    for (k = 0; k < q - w; ++k)
        key[k] = t[w + k] >= 'A' && t[w + k] <= 'Z' ? (char)(t[w + k] - 'A' + 'a') : t[w + k];
    key[k] = 0;
    if (!ttf_resource_get(in->engine, "date_cue", EN, key, &layer))
        return 0;
    (void)snprintf(out->value, sizeof(out->value), "--%02u-%02u", month, day);
    (void)snprintf(out->unit, sizeof(out->unit), "month-day");
    return finish(in, out, p, 1, layer, "en.date.slash-cue");
}

size_t ttf_en_classify_date(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    size_t start, named;
    if (!in || !out || in->index >= in->token_count)
        return 0;
    start = in->tokens[in->index].begin;
    if (start >= in->length || !left_boundary(in, start))
        return 0;
    named = bracketed_weekday(in, out, start);
    if (named)
        return named;
    named = slash_month_day_year(in, out, start);
    if (named)
        return named;
    named = slash_month_day_cue(in, out, start);
    if (named)
        return named;
    named = named_date(in, out, start);
    if (named)
        return named;
    named = decade(in, out, start);
    if (named)
        return named;
    named = year_range(in, out, start);
    return named ? named : contextual_year(in, out, start);
}
