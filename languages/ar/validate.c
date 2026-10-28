/* Validation belongs to the pack which consumes these policies. Readings are
 * checked structurally, never certified as pronunciation gold. */
#include "../../src/core/internal.h"
#include "../../src/resource/validation.h"

#define AR "ar"
#define SAME(a, b) ((a)[0] == (b)[0] && !strcmp((a), (b)))

/* A reading: printable, no tab, at most `bars` bars, under 160 bytes (Arabic is two bytes a
   letter). */
static int label(const char *text, int bars) {
    const unsigned char *p = (const unsigned char *)text;
    int seen = 0;
    if (!*p || strlen(text) >= 160)
        return 0;
    for (; *p; ++p) {
        if (*p < 32 || *p == 127)
            return 0;
        seen += *p == '|';
    }
    return seen == bars;
}
static int short_key(const char *key, size_t most) {
    size_t n = strlen(key), i;
    if (!n || n > most)
        return 0;
    for (i = 0; i < n; ++i)
        if ((unsigned char)key[i] < 32)
            return 0;
    return 1;
}

static ttf_status_t record(const ttf_resource_record_t *r, int *handled,
                           ttf_resource_diagnostic_t *d, void *userdata) {
    const char *k = r->kind, *key = r->key, *v = r->value;
    (void)userdata;
    *handled = 0;
    if (!SAME(r->language, AR))
        return TTF_OK;
    if (SAME(k, "number") || SAME(k, "ar_month") || SAME(k, "ar_hijri") || SAME(k, "ar_letter") ||
        SAME(k, "ar_word") || SAME(k, "ar_unit")) {
        *handled = 1;
        return short_key(key, 47) && label(v, 0)
                   ? TTF_OK
                   : ttf_validation_error(d, "an Arabic number word, month, letter, word or unit needs a short key and a reading");
    }
    if (SAME(k, "speech_symbol") || SAME(k, "address_symbol")) {
        /* a symbol read in an Arabic sentence (× في, π باي), or inside an {@email} or {@url}
           (? علامة استفهام, 2026-10-27) */
        *handled = 1;
        return short_key(key, 15) && label(v, 0)
                   ? TTF_OK
                   : ttf_validation_error(d, "speech_symbol and address_symbol require a symbol and a reading");
    }
    if (SAME(k, "ar_plural")) {
        /* a written plural and the ar_noun key of its singular (أيام يوم) */
        *handled = 1;
        return short_key(key, 47) && label(v, 0)
                   ? TTF_OK
                   : ttf_validation_error(d, "ar_plural requires a plural and its singular key");
    }
    if (SAME(k, "ar_noun")) {
        /* gender|singular|dual|plural|accusative */
        *handled = 1;
        return short_key(key, 47) && label(v, 4) && (v[0] == 'm' || v[0] == 'f') && v[1] == '|'
                   ? TTF_OK
                   : ttf_validation_error(d, "ar_noun requires m|f and four forms");
    }
    if (SAME(k, "ar_currency")) {
        /* noun|minor noun|minor digits */
        *handled = 1;
        return short_key(key, 23) && label(v, 2)
                   ? TTF_OK
                   : ttf_validation_error(d, "ar_currency requires a noun, a minor noun and its digits");
    }
    if (SAME(k, "ar_gender")) {
        *handled = 1;
        return short_key(key, 47) && ttf_validation_enum(v, "m|f")
                   ? TTF_OK
                   : ttf_validation_error(d, "ar_gender requires m or f");
    }
    if (SAME(k, "ar_cue")) {
        *handled = 1;
        return short_key(key, 47) && ttf_validation_enum(v, "ordm|ordf|hour|phone|code")
                   ? TTF_OK
                   : ttf_validation_error(d, "ar_cue requires ordm|ordf|hour|phone|code");
    }
    if (SAME(k, "ar_period")) {
        *handled = 1;
        return short_key(key, 47) && ttf_validation_enum(v, "am|pm")
                   ? TTF_OK
                   : ttf_validation_error(d, "ar_period requires am or pm");
    }
    if (SAME(k, "context")) {
        *handled = 1;
        return ttf_validation_enum(key, "declared_only|arabic_sentence|native_symbol_names") && ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "context requires a known flag and 0|1");
    }
    return TTF_OK;
}

/* Every word verbalize asks for must be present. */
static ttf_status_t engine(const ttf_engine_t *e, ttf_resource_diagnostic_t *d, void *userdata) {
    static const char *const needed[] = {"0", "1m", "1f", "1fc", "2m", "2mo", "2f", "2fo", "3m", "10m", "3f", "10f",
                                         "11m", "11f", "12m", "12mo", "12f", "12fo", "teenm", "teenf", "20", "20o",
                                         "90", "90o", "100", "200", "200o", "200c", "900", "k1", "k2", "k2o", "k2c",
                                         "kp", "ka", "m1", "mp", "ma", "g1", "gp", "ga", "t1", "tp", "ta", "point",
                                         "percent", "plus", "minus", "plusminus", "to", "from", "of", "versus",
                                         "over", "dot", "at", "dash", "slash", "underscore", "version", "times",
                                         "divided", "equals", "lt", "gt", "le", "ge", "ne", "squared", "hijri",
                                         "midnight", "quarter", "halfhour", "to_quarter", "am", "pm", "noon_period",
                                         "afternoon", "minute", "minute2", "minutes", "hour", "hour2", "hours",
                                         "o1m", "o1f", "o10m", "o10f", "oc1m", "oc1f", "oteenm", "oteenf", "ot20",
                                         "ot90", "h1"};
    size_t i;
    (void)userdata;
    if (!ttf_find_pack(e, AR))
        return TTF_OK;
    for (i = 0; i < sizeof(needed) / sizeof(*needed); ++i)
        if (!ttf_resource_get(e, "number", AR, needed[i], NULL)) {
            if (d) {
                snprintf(d->kind, sizeof(d->kind), "number");
                snprintf(d->language, sizeof(d->language), "%s", AR);
                snprintf(d->key, sizeof(d->key), "%s", needed[i]);
            }
            return ttf_validation_error(d, "an Arabic number word is missing");
        }
    return TTF_OK;
}

const ttf_resource_validator_t ttf_resource_validator_ar = {sizeof(ttf_resource_validator_t),
                                                            TTF_RESOURCE_VALIDATOR_ABI_VERSION,
                                                            "ar",
                                                            NULL,
                                                            record,
                                                            engine};
