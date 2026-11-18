/* Validation belongs to the pack which consumes these policies. Readings are
 * checked structurally, never certified as pronunciation gold. */
#include "../../src/core/internal.h"
#include "../../src/resource/validation.h"

#define ZH "zh-CN"
#define SAME(a, b) ((a)[0] == (b)[0] && !strcmp((a), (b)))

static int label(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!*p || strlen(text) >= 64)
        return 0;
    for (; *p; ++p)
        if (*p <= 32 || *p == 127 || *p == '|')
            return 0;
    return 1;
}

/* A short key of one to `most` characters (a cue word up to four, a unit symbol
   such as mmol/L up to eight). */
static int short_key(const char *key, size_t most) {
    size_t n = strlen(key), at = 0, chars = 0;
    while (at < n) {
        uint32_t cp;
        size_t width;
        if (ttf_utf8_decode(key + at, n - at, &cp, &width) != 1 || cp <= 32)
            return 0;
        at += width;
        ++chars;
    }
    return chars >= 1 && chars <= most && n < 20;
}
/* A unit symbol: a short key, one inner space allowed (n mile). */
static int unit_key(const char *key) {
    char copy[24];
    size_t n = strlen(key), i;
    if (!n || n >= sizeof(copy) || key[0] == ' ' || key[n - 1] == ' ')
        return 0;
    for (i = 0; i <= n; ++i)
        copy[i] = key[i] == ' ' && key[i + 1] != ' ' ? '_' : key[i];
    return short_key(copy, 8);
}
static int cue(const char *key) {
    return short_key(key, 4);
}

static ttf_status_t record(const ttf_resource_record_t *r, int *handled,
                           ttf_resource_diagnostic_t *d, void *userdata) {
    const char *k = r->kind, *key = r->key, *v = r->value;
    (void)userdata;
    *handled = 0;
    if (!SAME(r->language, ZH))
        return TTF_OK;
    /* 新☆王道: a decorative star read only under the decorative policy (2026-11-11, real-text
       cycle R11) */
    if (SAME(k, "speech_decorative")) {
        *handled = 1;
        return key && *key && strlen(key) < 96 && v && *v && strlen(v) < 256
                   ? TTF_OK
                   : ttf_validation_error(d, "speech_decorative requires a symbol and a reading");
    }
    if (SAME(k, "address_symbol")) {
        /* One printable ASCII mark read inside an e-mail address or URL. */
        *handled = 1;
        return key[0] > 32 && key[0] < 127 && !key[1] &&
                       !((key[0] >= '0' && key[0] <= '9') || (key[0] >= 'a' && key[0] <= 'z') ||
                         (key[0] >= 'A' && key[0] <= 'Z')) &&
                       label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "address_symbol key is one ASCII mark with a reading");
    }
    if (SAME(k, "number")) {
        *handled = 1;
        return ((key[0] == 'd' && key[1] >= '0' && key[1] <= '9' && !key[2]) ||
                ttf_validation_enum(key, "yao|liang|10|100|1000|wan|yi|wanyi|point|minus|"
                                         "below_zero|percent|permille|fraction|range|ratio|year|"
                                         "month|day|oclock|hours|minutes|seconds|plus|subtract|"
                                         "times|multiply|divide|equals|greater|less|power|power_end|"
                                         "square|root|greater_eq|less_eq|not_equal|"
                                         "approx_equal|cube_root|angle|pi|degree|"
                                         "approx_plus|scale_plus|and_fraction|per|ordinal|double|"
                                         "factorial|triangle|colon|chi_square|coord_N|"
                                         "coord_S|coord_E|coord_W|positive|square_bare|approx|tends_to|"
                                         "infinity|hao")) &&
                       label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "number requires a digit or a named word and a reading");
    }
    if (SAME(k, "zh_unit") || SAME(k, "zh_currency") || SAME(k, "zh_vulgar") ||
        SAME(k, "zh_greek") || SAME(k, "zh_roman") || SAME(k, "zh_unit_math") ||
        SAME(k, "zh_unit_per")) {
        *handled = 1;
        return unit_key(key) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "zh_unit/zh_currency/zh_vulgar require a short "
                                             "symbol and a reading");
    }
    if (SAME(k, "liang_counter") || SAME(k, "code_cue") || SAME(k, "compute_cue") || SAME(k, "phone_cue") ||
        SAME(k, "phone_after") || SAME(k, "room_after") || SAME(k, "year_after") ||
        SAME(k, "year_not") || SAME(k, "ratio_cue") || SAME(k, "duration_cue") ||
        SAME(k, "range_unit") || SAME(k, "length_cue") || SAME(k, "ratio_after") ||
        SAME(k, "count_before") || SAME(k, "code_after") || SAME(k, "room_before") || SAME(k, "room_not") ||
        SAME(k, "holiday_after") || SAME(k, "date_cue") || SAME(k, "date_after") ||
        SAME(k, "standard_cue") || SAME(k, "score_after") || SAME(k, "status_cue") ||
        SAME(k, "proportion_cue") || SAME(k, "money_after") || SAME(k, "service_cue") || SAME(k, "service_after") ||
        SAME(k, "liang_not") || SAME(k, "schedule_word") ||
        SAME(k, "festival_digits") || SAME(k, "md_cue") || SAME(k, "md_after") ||
        SAME(k, "score_cue") || SAME(k, "short_year_after")) {
        *handled = 1;
        return cue(key) && SAME(v, "1")
                   ? TTF_OK
                   : ttf_validation_error(d, "a Chinese cue requires a word of one to four "
                                             "characters and the value 1");
    }
    if (SAME(k, "service_number")) {
        size_t i;
        *handled = 1;
        for (i = 0; key[i]; ++i)
            if (key[i] < '0' || key[i] > '9')
                break;
        return !key[i] && i >= 3 && i <= 6 && SAME(v, "1")
                   ? TTF_OK
                   : ttf_validation_error(d, "service_number requires 3 to 6 digits and 1");
    }
    if (SAME(k, "per_hanzi")) {
        *handled = 1;
        return cue(key) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "per_hanzi requires a short word and its reading");
    }
    if (SAME(k, "han_foreign")) {
        uint32_t cp;
        size_t width, n = strlen(key);
        *handled = 1;
        return ttf_utf8_decode(key, n, &cp, &width) == 1 && width == n && cp >= 0x3000 &&
                       SAME(v, "1")
                   ? TTF_OK
                   : ttf_validation_error(d, "han_foreign requires one CJK character and 1");
    }
    if (SAME(k, "context") && SAME(key, "sentence_word_weight")) {
        *handled = 1;
        return ttf_validation_integer(v, 1, 9, NULL)
                   ? TTF_OK
                   : ttf_validation_error(d, "sentence_word_weight must be an integer in 1..9");
    }
    if (SAME(k, "context")) {
        *handled = 1;
        return ttf_validation_enum(key, "declared_only|absorb_foreign|han_sentence") &&
                       ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "context requires a known flag and 0|1");
    }
    return TTF_OK;
}

/* Every digit name and every word verbalize asks for must be present. */
static ttf_status_t engine(const ttf_engine_t *e, ttf_resource_diagnostic_t *d, void *userdata) {
    static const char *const needed[] = {
        "d0",       "d1",      "d2",      "d3",       "d4",     "d5",     "d6",        "d7",
        "d8",       "d9",      "yao",     "liang",    "10",     "100",    "1000",      "wan",
        "yi",       "wanyi",   "point",   "minus",    "below_zero", "percent", "permille",
        "fraction", "range",   "ratio",   "year",     "month",  "day",    "oclock",    "hours",
        "minutes",  "seconds", "plus",    "subtract", "times",  "multiply", "divide", "equals",    "greater",
        "less",     "power",   "power_end", "square", "root", "greater_eq", "less_eq",
        "not_equal", "approx_equal", "cube_root", "angle", "pi", "degree", "approx_plus",
        "and_fraction", "per", "ordinal", "double", "factorial", "triangle"};
    size_t i;
    (void)userdata;
    if (!ttf_find_pack(e, ZH))
        return TTF_OK;
    for (i = 0; i < sizeof(needed) / sizeof(*needed); ++i)
        if (!ttf_resource_get(e, "number", ZH, needed[i], NULL))
            return ttf_validation_error(d, "a Chinese number word is missing");
    return TTF_OK;
}

const ttf_resource_validator_t ttf_resource_validator_zh_CN = {sizeof(ttf_resource_validator_t),
                                                               TTF_RESOURCE_VALIDATOR_ABI_VERSION,
                                                               "zh-CN",
                                                               NULL,
                                                               record,
                                                               engine};
