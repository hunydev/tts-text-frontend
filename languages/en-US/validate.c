/* Validation belongs to the pack which consumes these policies. Free-form
 * labels are checked structurally, never certified as pronunciation gold. */
#include "../../src/core/internal.h"
#include "../../src/resource/validation.h"

#define EN "en-US"
#define SAME(a, b) (!strcmp((a), (b)))

static int lowercase_letters(const char *text, size_t limit) {
    size_t n = 0;
    for (; text[n]; ++n)
        if (n >= limit || text[n] < 'a' || text[n] > 'z')
            return 0;
    return n != 0;
}

static int lowercase_words(const char *text, size_t limit) {
    size_t n = strlen(text), i;
    if (!n || n > limit || text[0] == ' ' || text[n - 1] == ' ')
        return 0;
    for (i = 0; i < n; ++i)
        if (!(text[i] >= 'a' && text[i] <= 'z') && !(text[i] == ' ' && i && text[i - 1] != ' '))
            return 0;
    return 1;
}

/* Fold ASCII only; dictionary apostrophes, hyphens and other punctuation stay
 * valid keys. Non-ASCII bytes are not changed by the source lookup. */
static int folded_key(const char *key) {
    for (; *key; ++key)
        if (*key >= 'A' && *key <= 'Z')
            return 0;
    return 1;
}

static int label(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!*p)
        return 0;
    for (; *p; ++p)
        if (*p < 32 || *p == 127)
            return 0;
    return 1;
}

/* Accept extensible uppercase phone symbols rather than a closed CMU inventory.
 * Stress 0..2 and the shared letter.A form are preserved. Mapping coverage is
 * a separate concern: an unknown phone can reach the target grapheme fallback. */
static int phone_sequence(const char *text) {
    size_t p = 0, count = 0;
    if (!*text || strlen(text) >= sizeof(((ttf_pronunciation_t *)0)->phones))
        return 0;
    while (text[p]) {
        size_t start, end, i;
        while (text[p] == ' ')
            ++p;
        if (!text[p])
            break;
        start = p;
        while (text[p] && text[p] != ' ')
            ++p;
        end = p;
        if (end - start >= 24)
            return 0;
        if (end - start == 8 && !memcmp(text + start, "letter.", 7) && text[start + 7] >= 'A' &&
            text[start + 7] <= 'Z') {
            if (++count > 128)
                return 0;
            continue;
        }
        if (text[end - 1] >= '0' && text[end - 1] <= '2')
            --end;
        if (end == start)
            return 0;
        for (i = start; i < end; ++i)
            if (text[i] < 'A' || text[i] > 'Z')
                return 0;
        if (++count > 128)
            return 0;
    }
    return count != 0;
}

/* Choice IDs are opaque stable labels, not frequency or POS rankings. */
static int variant_id(const char *text) {
    size_t n;
    if (!text[0] || !((text[0] >= 'a' && text[0] <= 'z') || (text[0] >= '0' && text[0] <= '9')))
        return 0;
    for (n = 0; text[n]; ++n)
        if (n >= 31 || !((text[n] >= 'a' && text[n] <= 'z') || (text[n] >= '0' && text[n] <= '9') ||
                         text[n] == '.' || text[n] == '_' || text[n] == '-'))
            return 0;
    return 1;
}

static int variant_key(const char *key) {
    const char *separator = strchr(key, '#');
    return separator && separator != key && (size_t)(separator - key) < TTF_IR_VALUE_SIZE &&
           !strchr(separator + 1, '#') && folded_key(key) && variant_id(separator + 1) &&
           !SAME(separator + 1, "base");
}

static int phone_cue_key(const char *key, int extension) {
    size_t i, n = strlen(key);
    if (!n || n >= (extension ? 32u : (size_t)TTF_IR_VALUE_SIZE) || key[0] == ' ' ||
        key[n - 1] == ' ' || (extension && key[0] == '.'))
        return 0;
    for (i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)key[i];
        if (c < 32 || c == 127 || (c >= 'A' && c <= 'Z'))
            return 0;
        if (c == ' ' && (extension || (i && key[i - 1] == ' ')))
            return 0;
        if (c < 128 && c != ' ' && !(c >= 'a' && c <= 'z') &&
            !(extension && c == '.' && i + 1 == n))
            return 0;
    }
    return 1;
}

static ttf_status_t phone_dependencies(const ttf_engine_t *e, ttf_resource_diagnostic_t *d) {
    static const char *const policies[] = {"international_min_digits", "international_max_digits",
                                           "national_min_digits", "national_max_digits",
                                           "extension_max_digits"};
    static const char *const labels[] = {"plus", "extension", "separator", "zero"};
    size_t i;
    int configured = 0;
    for (i = 0; i < sizeof(policies) / sizeof(policies[0]); ++i)
        if (ttf_resource_get(e, "phone_policy", "*", policies[i], NULL))
            configured = 1;
    if (!configured)
        return TTF_OK; /* Old resource roots keep their legacy phone contract. */
    for (i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i)
        if (!ttf_resource_get(e, "phone_render", EN, labels[i], NULL)) {
            if (d) {
                snprintf(d->kind, sizeof(d->kind), "phone_render");
                snprintf(d->language, sizeof(d->language), "%s", EN);
                snprintf(d->key, sizeof(d->key), "%s", labels[i]);
            }
            return ttf_validation_error(
                d, "configured phone policy requires plus/extension/separator/zero reading labels");
        }
    return TTF_OK;
}

static ttf_status_t record(const ttf_resource_record_t *r, int *handled,
                           ttf_resource_diagnostic_t *d, void *userdata) {
    const char *k = r->kind, *key = r->key, *v = r->value;
    int wildcard = SAME(r->language, "*");
    (void)userdata;
    *handled = 0;
    if (!wildcard && !SAME(r->language, EN))
        return TTF_OK;
    if (SAME(k, "phone_label") || SAME(k, "phone_extension") || SAME(k, "phone_extension_hint")) {
        *handled = 1;
        if (!phone_cue_key(key, !SAME(k, "phone_label")))
            return ttf_validation_error(d, "phone cue must use bounded canonical words with "
                                           "lowercase ASCII; extension may end in a period");
        return ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "phone cue flag must be 0 or 1");
    }
    if (SAME(k, "phone_render")) {
        *handled = 1;
        if (!ttf_validation_enum(key, "plus|extension|separator|zero"))
            return ttf_validation_error(d, "unknown phone_render key");
        return label(v) && strlen(v) < TTF_IR_VALUE_SIZE
                   ? TTF_OK
                   : ttf_validation_error(d, "phone reading label must be nonempty, bounded and "
                                             "contain no ASCII controls");
    }
    if (SAME(k, "identifier_label")) {
        *handled = 1;
        if (!phone_cue_key(key, 0))
            return ttf_validation_error(
                d, "identifier label must use bounded canonical words with lowercase ASCII");
        return ttf_validation_enum(v, "digits|disabled")
                   ? TTF_OK
                   : ttf_validation_error(d, "identifier_label must be digits or disabled");
    }
    if (SAME(k, "identifier_render")) {
        *handled = 1;
        if (!ttf_validation_enum(key, "zero|separator"))
            return ttf_validation_error(d, "unknown identifier_render key");
        return label(v) && strlen(v) < TTF_IR_VALUE_SIZE
                   ? TTF_OK
                   : ttf_validation_error(d, "identifier reading label must be nonempty, bounded "
                                             "and contain no ASCII controls");
    }
    if (SAME(k, "date_policy")) {
        *handled = 1;
        if (ttf_validation_enum(key, "named_year_style|numeric_year_style|year_style"))
            return ttf_validation_enum(v, "cardinal|calendar")
                       ? TTF_OK
                       : ttf_validation_error(d, "date year style must be cardinal or calendar");
        if (ttf_validation_enum(key, "min_year|max_year"))
            return ttf_validation_integer(v, 1, 9999, NULL)
                       ? TTF_OK
                       : ttf_validation_error(d, "date year limit must be an integer in 1..9999");
        return ttf_validation_error(d, "unknown English date_policy key");
    }
    if (SAME(k, "initialism_policy") || SAME(k, "initialism_text")) {
        *handled = 1;
        if (SAME(k, "initialism_policy") && !ttf_validation_enum(key, "dotted|known|inferred"))
            return ttf_validation_error(d, "unknown initialism_policy key");
        if (SAME(k, "initialism_text") && !lowercase_letters(key, TTF_IR_VALUE_SIZE - 1))
            return ttf_validation_error(d, "initialism_text key must be lowercase ASCII letters");
        return ttf_validation_enum(v, "preserve|letters|names")
                   ? TTF_OK
                   : ttf_validation_error(d,
                                          "initialism reading must be preserve, letters or names");
    }
    if (SAME(k, "currency_minor_digits")) {
        *handled = 1;
        if (strlen(key) >= 32)
            return ttf_validation_error(d, "currency identity exceeds semantic unit capacity");
        return ttf_validation_enum(v, "0|2")
                   ? TTF_OK
                   : ttf_validation_error(d, "currency_minor_digits must be 0 or 2");
    }
    if (SAME(k, "context")) {
        if (!ttf_validation_enum(
                key, "latin_weight|phrase_ceiling|clause_weight|numeric_label_weight")) {
            if (wildcard)
                return TTF_OK; /* A different pack may own this wildcard key. */
            *handled = 1;
            return ttf_validation_error(d, "unknown English context policy key");
        }
        *handled = 1;
        return ttf_validation_integer(v, 0, 1000, NULL)
                   ? TTF_OK
                   : ttf_validation_error(d, "context weight must be an integer in 0..1000");
    }
    if (SAME(k, "context_word") || SAME(k, "context_cue")) {
        *handled = 1;
        if (strlen(key) >= 96)
            return ttf_validation_error(d, "context cue exceeds the 95-byte lookup limit");
        if (SAME(k, "context_word") && !folded_key(key))
            return ttf_validation_error(d, "context_word key must use lowercase ASCII spelling");
        return ttf_validation_integer(v, 0, 1000, NULL)
                   ? TTF_OK
                   : ttf_validation_error(d, "context weight must be an integer in 0..1000");
    }
    if (SAME(k, "context_neutral")) {
        *handled = 1;
        if (!SAME(key, "dotted_initials"))
            return ttf_validation_error(d, "unknown English context_neutral key");
        return ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "context_neutral value must be 0 or 1");
    }
    if (SAME(k, "context_role") || SAME(k, "lexical")) {
        *handled = 1;
        if (strlen(key) >= 96)
            return ttf_validation_error(d, "English lexical key exceeds 95 bytes");
        if (!folded_key(key))
            return ttf_validation_error(d, "English lexical key must use lowercase ASCII spelling");
        if (SAME(k, "context_role"))
            return ttf_validation_enum(v, "finite|imperative|subject|polite|0")
                       ? TTF_OK
                       : ttf_validation_error(d, "unknown English context role");
        return ttf_validation_enum(v, "WORD|PROPER_NOUN|INITIALISM|ACRONYM")
                   ? TTF_OK
                   : ttf_validation_error(d, "unknown English lexical class");
    }
    if (SAME(k, "month_alias")) {
        *handled = 1;
        if (!lowercase_letters(key, 31))
            return ttf_validation_error(d, "month_alias key must be 1..31 lowercase ASCII letters");
        return ttf_validation_integer(v, 1, 12, NULL)
                   ? TTF_OK
                   : ttf_validation_error(d, "month_alias value must be an integer in 1..12");
    }
    if (SAME(k, "date_year_cue") || SAME(k, "date_year_quantity")) {
        *handled = 1;
        if (SAME(k, "date_year_cue") ? !lowercase_words(key, 64) : !lowercase_letters(key, 63))
            return ttf_validation_error(d,
                                        "date cue must be bounded canonical lowercase ASCII words");
        return ttf_validation_enum(v, "0|1")
                   ? TTF_OK
                   : ttf_validation_error(d, "date cue value must be 0 or 1");
    }
    if (SAME(k, "pronunciation_choice")) {
        *handled = 1;
        if (!*key || strlen(key) >= TTF_IR_VALUE_SIZE || strchr(key, '#') || !folded_key(key))
            return ttf_validation_error(
                d, "pronunciation choice key must be a bounded folded headword without #");
        return variant_id(v)
                   ? TTF_OK
                   : ttf_validation_error(
                         d,
                         "pronunciation choice must be base or a 1..31-byte lowercase variant ID");
    }
    if (SAME(k, "pronunciation_variant")) {
        *handled = 1;
        if (!variant_key(key))
            return ttf_validation_error(
                d, "pronunciation variant key must be headword#id; base is reserved");
        return phone_sequence(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "expected at most 128 bounded uppercase phone tokens "
                                             "with stress 0..2 or letter.A");
    }
    if (SAME(k, "pronunciation") || SAME(k, "initialism_pronunciation") ||
        SAME(k, "digit_pronunciation") || SAME(k, "g2p")) {
        *handled = 1;
        if (strlen(key) >= TTF_IR_VALUE_SIZE || (SAME(k, "g2p") && strlen(key) > 4) ||
            (SAME(k, "digit_pronunciation") && (strlen(key) != 1 || key[0] < '0' || key[0] > '9')))
            return ttf_validation_error(d, "pronunciation key exceeds its source lookup grammar");
        if (!folded_key(key))
            return ttf_validation_error(d, "pronunciation key must use lowercase ASCII spelling");
        return phone_sequence(v)
                   ? TTF_OK
                   : ttf_validation_error(d, "expected at most 128 bounded uppercase phone tokens "
                                             "with stress 0..2 or letter.A");
    }
    if (SAME(k, "letter_name")) {
        *handled = 1;
        if (!lowercase_letters(key, 1))
            return ttf_validation_error(d, "letter_name key must be one lowercase ASCII letter");
    } else if (SAME(k, "abbreviation") || SAME(k, "abbreviation_expansion")) {
        /* Common protected abbreviations use a generic marker and may include
         * periods (e.g/i.e); the core validator owns that namespace. */
        if (wildcard && SAME(k, "abbreviation"))
            return TTF_OK;
        *handled = 1;
        if (!lowercase_letters(key, 95))
            return ttf_validation_error(d,
                                        "abbreviation key must be 1..95 lowercase ASCII letters");
        if (SAME(k, "abbreviation") && !SAME(v, "preserve"))
            return ttf_validation_error(d, "abbreviation marker supports preserve only");
        if (SAME(k, "abbreviation_expansion") && strlen(v) >= TTF_IR_VALUE_SIZE)
            return ttf_validation_error(
                d, "abbreviation expansion exceeds the semantic value capacity");
    } else if (SAME(k, "date_word")) {
        *handled = 1;
        if (!ttf_validation_enum(key, "the|of"))
            return ttf_validation_error(d, "unknown English date_word key");
    } else if (SAME(k, "clock")) {
        *handled = 1;
        if (!ttf_validation_enum(key, "midnight|after|h24_full_hour"))
            return ttf_validation_error(d, "unknown English clock key");
    } else if (ttf_validation_enum(k, "number|ordinal|month|currency|unit|meridiem|symbol")) {
        *handled = 1;
    }
    return !*handled || label(v)
               ? TTF_OK
               : ttf_validation_error(
                     d, "reading label must be nonempty and contain no ASCII controls");
}

static ttf_status_t final_error(ttf_resource_diagnostic_t *d, const char *kind, const char *key,
                                const char *message) {
    if (d) {
        snprintf(d->kind, sizeof(d->kind), "%s", kind);
        snprintf(d->language, sizeof(d->language), "%s", EN);
        snprintf(d->key, sizeof(d->key), "%s", key);
    }
    return ttf_validation_error(d, message);
}

static ttf_status_t require(const ttf_engine_t *e, const char *kind, const char *key,
                            ttf_resource_diagnostic_t *d) {
    return ttf_resource_get(e, kind, EN, key, NULL)
               ? TTF_OK
               : final_error(d, kind, key, "required English verbalizer label is missing");
}

static ttf_status_t currency_labels(const ttf_engine_t *e, const char *identity,
                                    ttf_resource_diagnostic_t *d) {
    static const char *const suffixes[] = {"one", "many", "minor.one", "minor.many"};
    const char *minor = ttf_resource_get(e, "currency_minor_digits", EN, identity, NULL);
    size_t i, count = minor && SAME(minor, "0") ? 2 : 4;
    char key[48];
    ttf_status_t status;
    for (i = 0; i < count; ++i) {
        snprintf(key, sizeof(key), "%s.%s", identity, suffixes[i]);
        status = require(e, "currency", key, d);
        if (status != TTF_OK)
            return status;
    }
    return TTF_OK;
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

/* A multiword cue needs every intermediate word kept pending. Checking only
 * the last word would still let a small segment target cut inside the cue. */
static ttf_status_t cue_boundaries(const ttf_engine_t *e, const char *cue,
                                   ttf_resource_diagnostic_t *d) {
    size_t p = 0;
    while (cue[p]) {
        char key[96];
        size_t n = 0;
        while (cue[p] && cue[p] != ' ')
            key[n++] = cue[p++];
        key[n] = 0;
        if (!boundary_hint(e, key))
            return final_error(d, "boundary_word", key,
                               "enabled English date cue needs boundary_word=1 for every word");
        if (cue[p])
            ++p;
    }
    return TTF_OK;
}

static ttf_status_t engine(const ttf_engine_t *e, ttf_resource_diagnostic_t *d, void *userdata) {
    static const char *const numbers[] = {"100",
                                          "1000",
                                          "1000000",
                                          "1000000000",
                                          "1000000000000",
                                          "1000000000000000",
                                          "1000000000000000000",
                                          "minus",
                                          "plus",
                                          "point",
                                          "percent",
                                          "and",
                                          "oh",
                                          "oclock",
                                          "range",
                                          "fraction_over"};
    static const struct {
        const char *kind, *key;
    } labels[] = {{"ordinal", "suffix"}, {"date_word", "the"}, {"date_word", "of"},
                  {"clock", "midnight"}, {"clock", "after"},   {"clock", "h24_full_hour"},
                  {"unit", "s.one"},     {"unit", "s.many"},   {"meridiem", "AM"},
                  {"meridiem", "PM"}};
    static const char *const policies[] = {"dotted", "known", "inferred"};
    const char *minimum, *maximum;
    long low = 1000, high = 2099;
    size_t i, bucket;
    char key[32];
    ttf_status_t status;
    int names = 0;
    (void)userdata;
    if (!ttf_find_pack(e, EN))
        return TTF_OK;
    if ((status = phone_dependencies(e, d)) != TTF_OK)
        return status;
    minimum = ttf_resource_get(e, "date_policy", EN, "min_year", NULL);
    maximum = ttf_resource_get(e, "date_policy", EN, "max_year", NULL);
    if (minimum)
        (void)ttf_validation_integer(minimum, 1, 9999, &low);
    if (maximum)
        (void)ttf_validation_integer(maximum, 1, 9999, &high);
    if (low > high)
        return final_error(d, "date_policy", "min_year",
                           "effective min_year must not exceed max_year");
    for (i = 0; i < 100; ++i) {
        if (i >= 20 && i % 10)
            continue;
        snprintf(key, sizeof(key), "%u", (unsigned)i);
        if ((status = require(e, "number", key, d)) != TTF_OK ||
            (status = require(e, "ordinal", key, d)) != TTF_OK)
            return status;
    }
    for (i = 0; i < sizeof(numbers) / sizeof(numbers[0]); ++i)
        if ((status = require(e, "number", numbers[i], d)) != TTF_OK)
            return status;
    for (i = 1; i <= 12; ++i) {
        snprintf(key, sizeof(key), "%u", (unsigned)i);
        if ((status = require(e, "month", key, d)) != TTF_OK)
            return status;
    }
    for (i = 0; i < sizeof(labels) / sizeof(labels[0]); ++i)
        if ((status = require(e, labels[i].kind, labels[i].key, d)) != TTF_OK)
            return status;
    for (i = 0; i < sizeof(policies) / sizeof(policies[0]); ++i) {
        const char *v = ttf_resource_get(e, "initialism_policy", EN, policies[i], NULL);
        if (v && SAME(v, "names"))
            names = 1;
    }
    if (names)
        for (i = 0; i < 26; ++i) {
            key[0] = (char)('a' + i);
            key[1] = 0;
            if ((status = require(e, "letter_name", key, d)) != TTF_OK)
                return status;
        }
    for (bucket = 0; bucket < TTF_HASH_BUCKETS; ++bucket) {
        const ttf_entry_t *entry;
        for (entry = e->resources[bucket]; entry; entry = entry->next) {
            const char *value;
            char identity[32];
            if (SAME(entry->kind, "pronunciation_choice") &&
                (SAME(entry->lang, EN) || SAME(entry->lang, "*"))) {
                ttf_resource_layer_t choice_layer = TTF_LAYER_COMMON, base_layer = TTF_LAYER_COMMON;
                const char *base;
                char variant[TTF_IR_VALUE_SIZE + 32];
                value = ttf_resource_get(e, entry->kind, EN, entry->key, &choice_layer);
                if (value != entry->value || SAME(value, "base"))
                    continue;
                base = ttf_resource_get(e, "pronunciation", EN, entry->key, &base_layer);
                /* A lower policy cannot redirect an explicit higher base. */
                if (base && base_layer > choice_layer)
                    continue;
                snprintf(variant, sizeof(variant), "%s#%s", entry->key, value);
                if (!ttf_resource_get(e, "pronunciation_variant", EN, variant, NULL))
                    return final_error(d, "pronunciation_choice", entry->key,
                                       "selected pronunciation variant is missing");
            }
            if (SAME(entry->kind, "abbreviation_expansion") &&
                (SAME(entry->lang, EN) || SAME(entry->lang, "*"))) {
                value = ttf_resource_get(e, entry->kind, EN, entry->key, NULL);
                /* A common protected marker also protects the same period.
                 * Do not invent dependencies for disabled/shadowed expansions. */
                if (value == entry->value && !SAME(value, "preserve") &&
                    !ttf_resource_get(e, "abbreviation", EN, entry->key, NULL))
                    return final_error(
                        d, "abbreviation", entry->key,
                        "enabled title expansion needs an abbreviation boundary marker");
            }
            if ((SAME(entry->kind, "month_alias") || SAME(entry->kind, "date_year_cue")) &&
                (SAME(entry->lang, EN) || SAME(entry->lang, "*"))) {
                value = ttf_resource_get(e, entry->kind, EN, entry->key, NULL);
                if (value == entry->value && !SAME(value, "0") &&
                    (status = cue_boundaries(e, entry->key, d)) != TTF_OK)
                    return status;
            }
            if (SAME(entry->kind, "initialism_text") &&
                (SAME(entry->lang, EN) || SAME(entry->lang, "*"))) {
                value = ttf_resource_get(e, entry->kind, EN, entry->key, NULL);
                if (value != entry->value || !SAME(value, "names"))
                    continue;
                for (i = 0; entry->key[i]; ++i) {
                    key[0] = entry->key[i];
                    key[1] = 0;
                    if ((status = require(e, "letter_name", key, d)) != TTF_OK)
                        return status;
                }
            }
            /* Universal currency classifiers read the '*' namespace. Shadowed
             * rows must not invent dependencies for an unreachable identity. */
            if (!SAME(entry->lang, "*"))
                continue;
            value = ttf_resource_get(e, entry->kind, "*", entry->key, NULL);
            if (value != entry->value)
                continue;
            if (SAME(entry->kind, "currency_prefix"))
                snprintf(identity, sizeof(identity), "%s", value);
            else if (SAME(entry->kind, "numeric_suffix") && !strncmp(value, "CURRENCY:", 9))
                snprintf(identity, sizeof(identity), "%s", value + 9);
            else if (SAME(entry->kind, "currency_name")) {
                const char *colon = strchr(value, ':');
                size_t n = colon ? (size_t)(colon - value) : 0;
                if (!n || n >= sizeof(identity))
                    continue; /* The generic row validator reports bad shapes. */
                memcpy(identity, value, n);
                identity[n] = 0;
            } else
                continue;
            if ((status = currency_labels(e, identity, d)) != TTF_OK)
                return status;
        }
    }
    return TTF_OK;
}

const ttf_resource_validator_t ttf_resource_validator_en_US = {sizeof(ttf_resource_validator_t),
                                                               TTF_RESOURCE_VALIDATOR_ABI_VERSION,
                                                               "en-US",
                                                               NULL,
                                                               record,
                                                               engine};
