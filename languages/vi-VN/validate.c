/* Validation belongs to the pack which consumes these policies. Readings are
 * checked structurally, never certified as pronunciation gold. */
#include "../../src/core/internal.h"
#include "../../src/resource/validation.h"

#define VI "vi-VN"
#define SAME(a, b) ((a)[0] == (b)[0] && !strcmp((a), (b)))

/* A reading: printable, no tab or bar, under 64 bytes. */
static int label(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!*p || strlen(text) >= 64)
        return 0;
    for (; *p; ++p)
        if (*p < 32 || *p == 127 || *p == '|')
            return 0;
    return 1;
}
static int short_key(const char *key, size_t most) {
    size_t n = strlen(key), i;
    if (!n || n > most)
        return 0;
    for (i = 0; i < n; ++i)
        if ((unsigned char)key[i] <= 32)
            return 0;
    return 1;
}
static int lowercase_ascii(const char *key) {
    size_t i;
    for (i = 0; key[i]; ++i)
        if (key[i] < 'a' || key[i] > 'z')
            return 0;
    return i > 0 && i < 16;
}

static ttf_status_t record(const ttf_resource_record_t *r, int *handled,
                           ttf_resource_diagnostic_t *d, void *userdata) {
    const char *k = r->kind, *key = r->key, *v = r->value;
    (void)userdata;
    *handled = 0;
    if (!SAME(r->language, VI))
        return TTF_OK;
    if (SAME(k, "number")) {
        *handled = 1;
        return ((key[0] == 'd' && key[1] >= '0' && key[1] <= '9' && !key[2]) ||
                ttf_validation_enum(key, "hundred|ten|tens|zero_tens|one_after_tens|"
                                         "five_after_tens|four_after_tens|thousand|million|"
                                         "billion|point|minus|plus_sign|percent|to|over|approx|day|month|"
                                         "year|month4|hour|minute|second|ordinal1|ordinal4|denominator4|math_eq|math_approx|math_ne|math_gt|math_lt|math_ge|math_le|math_plus|math_minus|math_negative|math_times|math_div|math_root|math_square|math_cube|math_open|math_close|math_factorial|math_pi|math_delta|math_degree|plus_minus|morning|afternoon|prize1|prize2|dot|per|per_amount|night|times|weekday|sunday|addr_room|addr_ward|addr_district")) &&
                       label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "number requires a digit or a named word and a reading");
    }
    if (SAME(k, "vi_unit") || SAME(k, "vi_currency") || SAME(k, "vi_scale") || SAME(k, "vi_road") ||
        SAME(k, "address_symbol") || SAME(k, "vi_idiom") || SAME(k, "vi_prefix")) {
        *handled = 1;
        return short_key(key, 15) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "a Vietnamese unit, currency, scale or address "
                                             "symbol needs a short key and a reading");
    }
    if (SAME(k, "vi_cue")) {
        /* the words before a number that decide its reading, |-separated */
        *handled = 1;
        return ttf_validation_enum(key, "day|date|month|ordinal|roman|code|call|version|holiday|noday|daypart|sms|code_not|id5|id4|daywide|clock|deadline|expiry|sunday|sunday_after|resolution|score") && strlen(v) < 512 && *v
                   ? TTF_OK
                   : ttf_validation_error(d, "vi_cue requires day|date|month|ordinal and words");
    }
    if (SAME(k, "vi_scale_word") || SAME(k, "vi_service")) {
        *handled = 1;
        return short_key(key, 15) && SAME(v, "1")
                   ? TTF_OK
                   : ttf_validation_error(d, "vi_scale_word and vi_service need a short key and 1");
    }
    if (SAME(k, "english_cue") || SAME(k, "latin_foreign")) {
        *handled = 1;
        return lowercase_ascii(key) && SAME(v, "1")
                   ? TTF_OK
                   : ttf_validation_error(d, "english_cue and latin_foreign require a lowercase word and 1");
    }
    if (SAME(k, "context")) {
        *handled = 1;
        if (SAME(key, "latin_marks") || SAME(key, "latin_weak") || SAME(key, "latin_veto"))
            return *v && strlen(v) < 512 ? TTF_OK
                                         : ttf_validation_error(d, "latin_marks lists letters");
        return ttf_validation_enum(key, "declared_only|absorb_foreign|latin_sentence") &&
                       ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "context requires a known flag and 0|1");
    }
    return TTF_OK;
}

/* Every digit name and every word verbalize asks for must be present. */
static ttf_status_t engine(const ttf_engine_t *e, ttf_resource_diagnostic_t *d, void *userdata) {
    static const char *const needed[] = {
        "d0",        "d1",       "d2",      "d3",     "d4",       "d5",     "d6",
        "d7",        "d8",       "d9",      "hundred", "ten",     "tens",   "zero_tens",
        "one_after_tens", "five_after_tens", "four_after_tens", "thousand", "million",
        "billion",   "point",    "minus",   "percent", "to",      "over",   "approx",
        "day",       "month",    "year",    "month4", "hour",     "minute", "second",
        "ordinal1",  "ordinal4", "denominator4", "dot", "per", "per_amount", "night", "times", "weekday", "sunday", "addr_room", "addr_ward", "addr_district"};
    size_t i;
    (void)userdata;
    if (!ttf_find_pack(e, VI))
        return TTF_OK;
    for (i = 0; i < sizeof(needed) / sizeof(*needed); ++i)
        if (!ttf_resource_get(e, "number", VI, needed[i], NULL))
            return ttf_validation_error(d, "a Vietnamese number word is missing");
    return TTF_OK;
}

const ttf_resource_validator_t ttf_resource_validator_vi_VN = {sizeof(ttf_resource_validator_t),
                                                               TTF_RESOURCE_VALIDATOR_ABI_VERSION,
                                                               "vi-VN",
                                                               NULL,
                                                               record,
                                                               engine};
