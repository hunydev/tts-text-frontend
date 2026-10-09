/* Validation belongs to the pack which consumes these policies. Readings are
 * checked structurally, never certified as pronunciation gold. */
#include "../../src/core/internal.h"
#include "../../src/resource/validation.h"

#define JA "ja-JP"
#define SAME(a, b) ((a)[0] == (b)[0] && !strcmp((a), (b)))

static int label(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!*p || strlen(text) >= 64)
        return 0;
    for (; *p; ++p)
        if (*p <= 32 || *p == 127 || *p == ':')
            return 0;
    return 1;
}

/* One character: a symbol, punctuation, enclosed form or kanji. */
static int symbol_key(const char *key) {
    uint32_t cp;
    size_t width, length = strlen(key);
    return length && ttf_utf8_decode(key, length, &cp, &width) == 1 && width == length && cp > 32 &&
           cp != 127 && !(cp >= '0' && cp <= '9') && !(cp >= 'A' && cp <= 'Z') &&
           !(cp >= 'a' && cp <= 'z');
}

static int alpha_ascii(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int decimal(const char *text, size_t limit) {
    size_t n = 0;
    for (; text[n]; ++n)
        if (n >= limit || text[n] < '0' || text[n] > '9')
            return 0;
    return n != 0;
}

/* reading:class[:base], the class one of the sound-change families. */
static int counter_spec(const char *v) {
    const char *colon = strchr(v, ':'), *colon2;
    char reading[64], cls[8];
    size_t n;
    if (!colon || colon == v || (size_t)(colon - v) >= sizeof(reading))
        return 0;
    memcpy(reading, v, (size_t)(colon - v));
    reading[colon - v] = 0;
    colon2 = strchr(colon + 1, ':');
    n = colon2 ? (size_t)(colon2 - colon - 1) : strlen(colon + 1);
    if (!n || n >= sizeof(cls))
        return 0;
    memcpy(cls, colon + 1, n);
    cls[n] = 0;
    if (colon2 && !label(colon2 + 1))
        return 0;
    return label(reading) && ttf_validation_enum(cls, "-|h|f|k|k3|s|s3|t|p10|w");
}

/* counter:value or counter:*d */
static int reading_key(const char *key) {
    const char *colon = strrchr(key, ':');
    if (!colon || colon == key || strlen(key) >= 48)
        return 0;
    if (colon[1] == '*')
        return colon[2] >= '1' && colon[2] <= '9' && !colon[3];
    return decimal(colon + 1, 20);
}

static ttf_status_t record(const ttf_resource_record_t *r, int *handled,
                           ttf_resource_diagnostic_t *d, void *userdata) {
    const char *k = r->kind, *key = r->key, *v = r->value;
    (void)userdata;
    *handled = 0;
    if (!SAME(r->language, JA))
        return TTF_OK;
    if (SAME(k, "number")) {
        *handled = 1;
        return (decimal(key, 1) ||
                ttf_validation_enum(key, "zero_integer|point|minus|range|fraction|"
                                         "phone_separator|version_separator|version_prefix|"
                                         "score_separator|power|postal_prefix|basement|"
                                         "plus|mixed|"
                                         "ordinal_prefix")) &&
                       label(v)
                   ? TTF_OK
                   : ttf_validation_error(d,
                                          "number requires a digit or a named word and a reading");
    }
    if (SAME(k, "number_place")) {
        long place;
        *handled = 1;
        place = decimal(key, 4) ? strtol(key, NULL, 10) : 0;
        return place >= 10 && place <= 9000 &&
                       (place % 1000 == 0 || (place < 1000 && place % 100 == 0) ||
                        (place < 100 && place % 10 == 0)) &&
                       label(v)
                   ? TTF_OK
                   : ttf_validation_error(
                         d, "number_place requires 10..90, 100..900 or 1000..9000 and a reading");
    }
    if (SAME(k, "number_scale")) {
        *handled = 1;
        return ttf_validation_enum(key, "4|8|12|16") && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "number_scale requires 4|8|12|16 and a reading");
    }
    if (SAME(k, "number_scale_mark")) {
        *handled = 1;
        return strlen(key) == 3 && ttf_validation_enum(v, "3|4|8|12")
                   ? TTF_OK
                   : ttf_validation_error(d, "number_scale_mark requires one kanji and 3|4|8|12");
    }
    if (SAME(k, "counter") || SAME(k, "unit")) {
        *handled = 1;
        return strlen(key) < 32 && label(key) && counter_spec(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "counter/unit requires reading:class[:base]");
    }
    if (SAME(k, "counter_reading")) {
        *handled = 1;
        return reading_key(key) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(
                         d, "counter_reading requires counter:value or counter:*d and a reading");
    }
    if (SAME(k, "currency")) {
        *handled = 1;
        return strlen(key) == 3 && key[0] >= 'A' && key[0] <= 'Z' && label(v) && strlen(v) < 32
                   ? TTF_OK
                   : ttf_validation_error(d, "currency requires an ISO code and a counter");
    }
    if (SAME(k, "half_suffix")) {
        *handled = 1;
        return label(key) && strlen(key) < 32 && ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "half_suffix requires a counter and 0|1");
    }
    if (SAME(k, "speech_kana_lengthening")) {
        *handled = 1;
        return symbol_key(key) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "speech_kana_lengthening requires one mark and a reading");
    }
    if (SAME(k, "speech_range_mark")) {
        *handled = 1;
        return symbol_key(key) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "speech_range_mark requires one mark and a reading");
    }
    if (SAME(k, "long_suffix")) {
        *handled = 1;
        return label(key) && strlen(key) < 32 && ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "long_suffix requires a counter and 0|1");
    }
    if (SAME(k, "kanji_word")) {
        *handled = 1;
        return label(key) && strlen(key) < 32 && ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "kanji_word requires a word under 32 bytes and 0|1");
    }
    if (SAME(k, "digit_cue")) {
        *handled = 1;
        return label(key) && strlen(key) < 16 && ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "digit_cue requires a label under 16 bytes and 0|1");
    }
    if (SAME(k, "digit")) {
        *handled = 1;
        return decimal(key, 1) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "digit requires 0..9 and a reading");
    }
    if (SAME(k, "latin_word")) {
        size_t i;
        *handled = 1;
        for (i = 0; key[i]; ++i)
            if (key[i] < 'A' || key[i] > 'Z')
                break;
        return !key[i] && i >= 2 && i <= 10 && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "latin_word requires 2-10 capital letters and a reading");
    }
    if (SAME(k, "latin_abbreviation")) {
        size_t i;
        *handled = 1;
        for (i = 0; alpha_ascii(key[i]); ++i)
            ;
        return i && i <= 8 && (!key[i] || (key[i] == '.' && !key[i + 1])) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "latin_abbreviation requires letters, an optional period and a reading");
    }
    if (SAME(k, "latin_ordinal")) {
        *handled = 1;
        return decimal(key, 3) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "latin_ordinal requires a number and a reading");
    }
    if (SAME(k, "counter_block") || SAME(k, "digit_after_cue") || SAME(k, "english_counter") ||
        SAME(k, "rate_cue")) {
        *handled = 1;
        return label(key) && strlen(key) <= 18 && SAME(v, "1")
                   ? TTF_OK
                   : ttf_validation_error(d, "a cue or block word requires a word and 1");
    }
    if (SAME(k, "english_number")) {
        *handled = 1;
        return decimal(key, 2) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "english_number requires a number and a reading");
    }
    if (SAME(k, "grade_prefix") || SAME(k, "alnum_word")) {
        *handled = 1;
        return label(key) && strlen(key) < 16 && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "grade_prefix/alnum_word requires a key and a reading");
    }
    if (SAME(k, "speech_range_open")) {
        *handled = 1;
        return symbol_key(key) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "speech_range_open requires one mark and a reading");
    }
    if (SAME(k, "score_cue") || SAME(k, "ratio_cue") || SAME(k, "date_cue") ||
        SAME(k, "fraction_cue")) {
        *handled = 1;
        return label(key) && strlen(key) <= 12 && SAME(v, "1")
                   ? TTF_OK
                   : ttf_validation_error(d, "score_cue/ratio_cue/date_cue/fraction_cue requires a word and 1");
    }
    if (SAME(k, "math_operator")) {
        *handled = 1;
        return symbol_key(key) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "math_operator requires one operator and a reading");
    }
    if (SAME(k, "letter_name")) {
        *handled = 1;
        return key[0] >= 'A' && key[0] <= 'Z' && !key[1] && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "letter_name requires A..Z and a reading");
    }
    if (SAME(k, "meridiem")) {
        *handled = 1;
        return ttf_validation_enum(key, "AM|PM") && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "meridiem requires AM|PM and a reading");
    }
    if (SAME(k, "speech_symbol") || SAME(k, "weekday_mark")) {
        *handled = 1;
        return symbol_key(key) && label(v)
                   ? TTF_OK
                   : ttf_validation_error(
                         d, "speech_symbol/weekday_mark requires one character and a reading");
    }
    if (SAME(k, "speech_silent")) {
        *handled = 1;
        return symbol_key(key) && ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "speech_silent requires one symbol and 0|1");
    }
    if (SAME(k, "speech_join")) {
        *handled = 1;
        return (SAME(key, "alphabet") || SAME(key, "symbol")) &&
                       ttf_validation_enum(v, "none|space")
                   ? TTF_OK
                   : ttf_validation_error(d, "speech_join supports alphabet|symbol=none|space");
    }
    if (SAME(k, "electronic_word")) {
        size_t i;
        *handled = 1;
        for (i = 0; key[i]; ++i)
            if (key[i] < 'a' || key[i] > 'z')
                return ttf_validation_error(d, "electronic_word requires a lowercase ASCII key");
        return i && i < 64 && label(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "electronic_word requires a reading");
    }
    if (SAME(k, "context")) {
        *handled = 1;
        return ttf_validation_enum(key,
                                   "declared_only|absorb_foreign|attached_grammar|kana_sentence") &&
                       ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "context requires a known flag and 0|1");
    }
    return TTF_OK;
}

/* A counter naming a base counter needs that base. */
static ttf_status_t engine(const ttf_engine_t *e, ttf_resource_diagnostic_t *d, void *userdata) {
    size_t bucket;
    (void)userdata;
    if (!ttf_find_pack(e, JA))
        return TTF_OK;
    for (bucket = 0; bucket < TTF_HASH_BUCKETS; ++bucket) {
        const ttf_entry_t *entry;
        for (entry = e->resources[bucket]; entry; entry = entry->next) {
            const char *base;
            if (!SAME(entry->kind, "counter") || !SAME(entry->lang, JA))
                continue;
            base = strchr(entry->value, ':');
            base = base ? strchr(base + 1, ':') : NULL;
            if (base && !ttf_resource_get(e, "counter", JA, base + 1, NULL))
                return ttf_validation_error(d, "a Japanese counter base must be a counter");
        }
        for (entry = e->resources[bucket]; entry; entry = entry->next)
            if (SAME(entry->kind, "currency") && SAME(entry->lang, JA) &&
                !ttf_resource_get(e, "counter", JA, entry->value, NULL))
                return ttf_validation_error(d, "a Japanese currency must name a counter");
    }
    return TTF_OK;
}

const ttf_resource_validator_t ttf_resource_validator_ja_JP = {sizeof(ttf_resource_validator_t),
                                                               TTF_RESOURCE_VALIDATOR_ABI_VERSION,
                                                               "ja-JP",
                                                               NULL,
                                                               record,
                                                               engine};
