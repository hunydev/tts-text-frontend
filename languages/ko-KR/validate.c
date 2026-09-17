/* Korean policy schema. General labels remain editorial data; passing this
 * check proves structural usability, not an independently approved reading. */
#include "../../src/core/internal.h"
#include "../../src/resource/validation.h"

#define KO "ko-KR"
#define SAME(a, b) (!strcmp((a), (b)))

static int label(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!*p)
        return 0;
    for (; *p; ++p)
        if (*p < 32 || *p == 127)
            return 0;
    return 1;
}

static int decimal_key(const char *text, size_t limit) {
    size_t i;
    if (!*text)
        return 0;
    for (i = 0; text[i]; ++i)
        if (i >= limit || text[i] < '0' || text[i] > '9')
            return 0;
    return 1;
}

static int phone_key(const char *text) {
    size_t i;
    for (i = 0; text[i]; ++i)
        if (i >= 23 || text[i] < 'A' || text[i] > 'Z')
            return 0;
    return i != 0;
}

static ttf_status_t bounded_key(const ttf_resource_record_t *r, size_t capacity,
                                ttf_resource_diagnostic_t *d) {
    return *r->key && strlen(r->key) < capacity
               ? TTF_OK
               : ttf_validation_error(d, "Korean policy key exceeds its lookup capacity");
}

static ttf_status_t record(const ttf_resource_record_t *r, int *handled,
                           ttf_resource_diagnostic_t *d, void *userdata) {
    const char *k = r->kind, *key = r->key, *v = r->value;
    int wildcard = SAME(r->language, "*");
    long number;
    ttf_status_t status;
    (void)userdata;
    *handled = 0;
    if (!wildcard && !SAME(r->language, KO))
        return TTF_OK;
    if (SAME(k, "context")) {
        if (!ttf_validation_enum(key, "hangul_weight|particle_weight|clause_floor|script_weight")) {
            if (wildcard)
                return TTF_OK;
            *handled = 1;
            return ttf_validation_error(d, "unknown Korean context policy key");
        }
        *handled = 1;
        return ttf_validation_integer(v, 0, 1000, NULL)
                   ? TTF_OK
                   : ttf_validation_error(d, "context weight must be an integer in 0..1000");
    }
    if (SAME(k, "counter") || SAME(k, "native_ordinal_suffix")) {
        *handled = 1;
        if ((status = bounded_key(r, 32, d)) != TTF_OK)
            return status;
        /* A zero suffix is unsafe: the renderer tests resource presence. */
        return ttf_validation_enum(v, "native|sino")
                   ? TTF_OK
                   : ttf_validation_error(d, "counter reading must be native or sino");
    }
    if (SAME(k, "ordinal_policy")) {
        *handled = 1;
        return SAME(key, "large") && SAME(v, "sino")
                   ? TTF_OK
                   : ttf_validation_error(d, "ordinal_policy supports only large=sino");
    }
    if (SAME(k, "counter_role_before") || SAME(k, "counter_role_after")) {
        *handled = 1;
        if ((status = bounded_key(r, 96, d)) != TTF_OK)
            return status;
        if (strchr(key, ' '))
            return ttf_validation_error(d, "Korean counter role cue must be a single token");
        return ttf_validation_enum(v, "frequency|identifier|0")
                   ? TTF_OK
                   : ttf_validation_error(d, "counter role must be frequency, identifier or 0");
    }
    if (SAME(k, "counter_role_unit") || SAME(k, "ordinal_prefix_unit")) {
        *handled = 1;
        if ((status = bounded_key(r, 32, d)) != TTF_OK)
            return status;
        return ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "counter unit flag must be 0 or 1");
    }
    if (SAME(k, "phone_onset") || SAME(k, "phone_vowel") || SAME(k, "phone_coda")) {
        int maximum = SAME(k, "phone_onset") ? 18 : SAME(k, "phone_vowel") ? 20 : 27;
        *handled = 1;
        if (!phone_key(key))
            return ttf_validation_error(d, "phone map key must be 1..23 uppercase ASCII letters");
        return ttf_validation_integer(v, 0, maximum, NULL)
                   ? TTF_OK
                   : ttf_validation_error(d,
                                          "phone map index exceeds its Hangul composition bounds");
    }
    if (SAME(k, "scale")) {
        *handled = 1;
        if ((status = bounded_key(r, 32, d)) != TTF_OK)
            return status;
        return ttf_validation_integer(v, 1, 40, &number) && (number <= 3 || number % 4 == 0)
                   ? TTF_OK
                   : ttf_validation_error(
                         d, "scale exponent must be 1..3 or a multiple of 4 in 4..40");
    }
    if (SAME(k, "policy")) {
        *handled = 1;
        if (SAME(key, "range_mode"))
            return ttf_validation_enum(v, "cardinal|digits")
                       ? TTF_OK
                       : ttf_validation_error(d, "range_mode must be cardinal or digits");
        if (SAME(key, "quantity_range_repeat_unit"))
            return ttf_validation_enum(v, "0|1")
                       ? TTF_OK
                       : ttf_validation_error(d, "quantity_range_repeat_unit must be 0 or 1");
        if (!ttf_validation_enum(key,
                                 "range_separator|quantity_range_separator|quantity_range_suffix"))
            return ttf_validation_error(d, "unknown Korean policy key");
    } else if (SAME(k, "identifier_label")) {
        *handled = 1;
        if ((status = bounded_key(r, TTF_IR_VALUE_SIZE, d)) != TTF_OK)
            return status;
        return ttf_validation_enum(v, "digits|disabled")
                   ? TTF_OK
                   : ttf_validation_error(d, "identifier_label must be digits or disabled");
    } else if (SAME(k, "identifier_render")) {
        *handled = 1;
        if (!ttf_validation_enum(key, "zero|separator"))
            return ttf_validation_error(d, "unknown identifier_render key");
    } else if (SAME(k, "ordinal_marker")) {
        *handled = 1;
        if (!SAME(key, "prefix") || strlen(v) >= 96)
            return ttf_validation_error(d, "ordinal_marker requires prefix and a bounded marker");
    } else if (SAME(k, "ordinal_first")) {
        *handled = 1;
        if (!decimal_key(key, TTF_IR_VALUE_SIZE - 1) || (key[0] == '0' && key[1]))
            return ttf_validation_error(
                d, "ordinal_first key must be a canonical decimal integer without leading zeros");
    } else if (SAME(k, "particle") || SAME(k, "quantity_tail")) {
        *handled = 1;
        if ((status = bounded_key(r, SAME(k, "particle") ? 64 : 128, d)) != TTF_OK)
            return status;
        /* These consumers test presence, so zero does not disable a row. */
        return SAME(v, "1")
                   ? TTF_OK
                   : ttf_validation_error(d, "presence-based Korean marker supports 1 only");
    } else if (SAME(k, "digit")) {
        *handled = 1;
        if (strlen(key) != 1 || key[0] < '0' || key[0] > '9')
            return ttf_validation_error(d, "digit key must be one ASCII decimal digit");
    } else if (SAME(k, "native") || SAME(k, "native_tens")) {
        *handled = 1;
        if (!decimal_key(key, 2) || !ttf_validation_integer(key, 1, 99, &number) || key[0] == '0' ||
            (SAME(k, "native_tens") && number % 10))
            return ttf_validation_error(d,
                                        "native keys are 1..99; native_tens keys are tens 10..90");
    } else if (SAME(k, "phone_letter")) {
        *handled = 1;
        if (!phone_key(key) &&
            (strlen(key) != 8 || memcmp(key, "letter.", 7) || key[7] < 'A' || key[7] > 'Z'))
            return ttf_validation_error(
                d, "phone_letter key must be an uppercase phone symbol or letter.A");
    } else if (SAME(k, "grapheme")) {
        *handled = 1;
        if ((status = bounded_key(r, 4, d)) != TTF_OK)
            return status;
    } else if (ttf_validation_enum(
                   k, "number|magnitude|month|currency|unit|meridiem|symbol|quantity_prefix")) {
        *handled = 1;
    }
    return !*handled || label(v)
               ? TTF_OK
               : ttf_validation_error(
                     d, "reading label must be nonempty and contain no ASCII controls");
}

static int boundary_hint(const ttf_engine_t *e, const char *key) {
    const char *v = ttf_resource_get(e, "boundary_word", "*", key, NULL);
    size_t i;
    if (v && SAME(v, "1"))
        return 1;
    for (i = 0; i < e->pack_count; ++i) {
        v = ttf_resource_get(e, "boundary_word", e->packs[i]->language_tag, key, NULL);
        if (v && SAME(v, "1"))
            return 1;
    }
    return 0;
}

static ttf_status_t cue_boundaries(const ttf_engine_t *e, const char *cue,
                                   ttf_resource_diagnostic_t *d) {
    size_t p = 0;
    while (cue[p]) {
        char key[96];
        size_t n = 0;
        while (cue[p] == ' ')
            ++p;
        if (!cue[p])
            break;
        while (cue[p] && cue[p] != ' ') {
            char c = cue[p++];
            key[n++] = c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c;
        }
        key[n] = 0;
        if (!boundary_hint(e, key)) {
            if (d) {
                snprintf(d->kind, sizeof(d->kind), "boundary_word");
                snprintf(d->language, sizeof(d->language), KO);
                snprintf(d->key, sizeof(d->key), "%s", key);
            }
            return ttf_validation_error(
                d, "enabled Korean counter cue needs boundary_word=1 for every word");
        }
    }
    return TTF_OK;
}

static ttf_status_t engine(const ttf_engine_t *e, ttf_resource_diagnostic_t *d, void *userdata) {
    size_t bucket;
    ttf_status_t status;
    const char *marker;
    (void)userdata;
    if (!ttf_find_pack(e, KO))
        return TTF_OK;
    marker = ttf_resource_get(e, "ordinal_marker", KO, "prefix", NULL);
    if (marker && (status = cue_boundaries(e, marker, d)) != TTF_OK)
        return status;
    for (bucket = 0; bucket < TTF_HASH_BUCKETS; ++bucket) {
        const ttf_entry_t *entry;
        for (entry = e->resources[bucket]; entry; entry = entry->next) {
            const char *value;
            if (!SAME(entry->kind, "counter_role_before") ||
                (!SAME(entry->lang, KO) && !SAME(entry->lang, "*")))
                continue;
            value = ttf_resource_get(e, entry->kind, KO, entry->key, NULL);
            if (value == entry->value && !SAME(value, "0") &&
                (status = cue_boundaries(e, entry->key, d)) != TTF_OK)
                return status;
        }
    }
    return TTF_OK;
}

const ttf_resource_validator_t ttf_resource_validator_ko_KR = {sizeof(ttf_resource_validator_t),
                                                               TTF_RESOURCE_VALIDATOR_ABI_VERSION,
                                                               "ko-KR",
                                                               NULL,
                                                               record,
                                                               engine};
