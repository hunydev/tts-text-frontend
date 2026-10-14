/* Validation belongs to the pack which consumes these policies. Readings are
 * checked structurally, never certified as pronunciation gold. */
#include "../../src/core/internal.h"
#include "../../src/resource/validation.h"

#define TH "th-TH"
#define SAME(a, b) ((a)[0] == (b)[0] && !strcmp((a), (b)))

/* A reading: printable, no tab or bar, under 96 bytes (Thai is three bytes a letter). */
static int label(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!*p || strlen(text) >= 96)
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

static ttf_status_t record(const ttf_resource_record_t *r, int *handled,
                           ttf_resource_diagnostic_t *d, void *userdata) {
    const char *k = r->kind, *key = r->key, *v = r->value;
    (void)userdata;
    *handled = 0;
    if (!SAME(r->language, TH))
        return TTF_OK;
    if (SAME(k, "number")) {
        *handled = 1;
        return ((key[0] == 'd' && key[1] >= '0' && key[1] <= '9' && !key[2]) ||
                ttf_validation_enum(key, "ed|twenty|ten|hundred|thousand|tenthousand|hundredthousand|million|"
                                         "point|minus|plus|plusminus|percent|to|versus|over|oclock|minute|"
                                         "slash|dash|version|be|ce|class|half|times|equals|sharp|greater|less|"
                                         "greaterequal|lessequal|notequal|divide|root|square|cube|approx|hour|second")) &&
                       label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "number requires a digit or a named word and a reading");
    }
    if (SAME(k, "th_cue")) {
        /* the words before a number that decide its reading, |-separated */
        *handled = 1;
        return ttf_validation_enum(key, "phone|code|date|time|score|ratio|classifier|weekday|postal|address") && *v && strlen(v) < 512
                   ? TTF_OK
                   : ttf_validation_error(d, "th_cue requires phone|code|date|time|score|ratio|classifier|weekday|postal|address and words");
    }
    if (SAME(k, "th_month_key")) {
        *handled = 1;
        return short_key(key, 31) && atoi(v) >= 1 && atoi(v) <= 12
                   ? TTF_OK
                   : ttf_validation_error(d, "th_month_key requires a written month and 1-12");
    }
    if (SAME(k, "th_unit") || SAME(k, "th_money") || SAME(k, "th_month") || SAME(k, "th_abbr") || SAME(k, "th_letter") ||
        SAME(k, "address_symbol") || SAME(k, "electronic_word") || SAME(k, "speech_symbol")) {
        *handled = 1;
        return short_key(key, 31) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "a Thai unit, month, abbreviation, letter or address "
                                             "word needs a short key and a reading");
    }
    if (SAME(k, "context")) {
        *handled = 1;
        return ttf_validation_enum(key, "declared_only|absorb_foreign|thai_sentence") && ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "context requires a known flag and 0|1");
    }
    return TTF_OK;
}

/* Every digit name and every word verbalize asks for must be present. */
static ttf_status_t engine(const ttf_engine_t *e, ttf_resource_diagnostic_t *d, void *userdata) {
    static const char *const needed[] = {"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9",
                                         "ed", "twenty", "ten", "hundred", "thousand", "tenthousand",
                                         "hundredthousand", "million", "point", "minus", "plus", "plusminus",
                                         "percent", "to", "versus", "over", "oclock", "minute", "slash",
                                         "dash", "version", "be", "ce", "class"};
    size_t i;
    (void)userdata;
    if (!ttf_find_pack(e, TH))
        return TTF_OK;
    for (i = 0; i < sizeof(needed) / sizeof(*needed); ++i)
        if (!ttf_resource_get(e, "number", TH, needed[i], NULL))
            return ttf_validation_error(d, "a Thai number word is missing");
    return TTF_OK;
}

const ttf_resource_validator_t ttf_resource_validator_th_TH = {sizeof(ttf_resource_validator_t),
                                                               TTF_RESOURCE_VALIDATOR_ABI_VERSION,
                                                               "th-TH",
                                                               NULL,
                                                               record,
                                                               engine};
