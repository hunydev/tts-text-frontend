#include "../core/internal.h"
#include "validation.h"

/* Copy a valid UTF-8 prefix; diagnostic truncation never splits a codepoint. */
void ttf_validation_copy(char *destination, size_t capacity, const char *source) {
    size_t at = 0, length = strlen(source);
    if (!capacity)
        return;
    while (at < length) {
        uint32_t cp;
        size_t width;
        if (ttf_utf8_decode(source + at, length - at, &cp, &width) != 1 || width >= capacity - at)
            break;
        at += width;
    }
    if (at)
        memmove(destination, source, at);
    destination[at] = 0;
}
static void diagnostic_sanitize(ttf_resource_diagnostic_t *d) {
#define SANITIZE(field)                                                                            \
    do {                                                                                           \
        d->field[sizeof(d->field) - 1] = 0;                                                        \
        ttf_validation_copy(d->field, sizeof(d->field), d->field);                                 \
    } while (0)
    SANITIZE(path);
    SANITIZE(kind);
    SANITIZE(language);
    SANITIZE(key);
    SANITIZE(message);
    d->struct_size = sizeof(*d);
#undef SANITIZE
}
void ttf_resource_diagnostic_init(ttf_resource_diagnostic_t *d) {
    if (d) {
        memset(d, 0, sizeof(*d));
        d->struct_size = sizeof(*d);
    }
}
void ttf_resource_validation_report_init(ttf_resource_validation_report_t *r) {
    if (r) {
        memset(r, 0, sizeof(*r));
        r->struct_size = sizeof(*r);
    }
}
int ttf_validation_integer(const char *s, long minimum, long maximum, long *parsed) {
    unsigned long value = 0, limit;
    int negative = 0;
    long result;
    if (!s || minimum > maximum)
        return 0;
    if (*s == '+' || *s == '-')
        negative = *s++ == '-';
    if (*s < '0' || *s > '9')
        return 0;
    limit = negative ? (unsigned long)LONG_MAX + 1UL : (unsigned long)LONG_MAX;
    do {
        unsigned digit = (unsigned)(*s++ - '0');
        if (value > (limit - digit) / 10UL)
            return 0;
        value = value * 10UL + digit;
    } while (*s >= '0' && *s <= '9');
    if (*s)
        return 0;
    result =
        negative ? (value == (unsigned long)LONG_MAX + 1UL ? LONG_MIN : -(long)value) : (long)value;
    if (result < minimum || result > maximum)
        return 0;
    if (parsed)
        *parsed = result;
    return 1;
}
int ttf_validation_enum(const char *value, const char *choices) {
    size_t n;
    const char *p;
    if (!value || !choices)
        return 0;
    n = strlen(value);
    for (p = choices; *p;) {
        const char *end = strchr(p, '|');
        size_t length = end ? (size_t)(end - p) : strlen(p);
        if (length == n && !memcmp(value, p, n))
            return 1;
        if (!end)
            break;
        p = end + 1;
    }
    return 0;
}
ttf_status_t ttf_validation_error(ttf_resource_diagnostic_t *d, const char *message) {
    if (d)
        ttf_validation_copy(d->message, sizeof(d->message), message);
    return TTF_ERESOURCE;
}
ttf_status_t ttf_engine_register_resource_validator(ttf_engine_t *e,
                                                    const ttf_resource_validator_t *v) {
    size_t i;
    if (!e || !v || v->struct_size != sizeof(*v) ||
        v->abi_version != TTF_RESOURCE_VALIDATOR_ABI_VERSION || !v->name || !*v->name ||
        (!v->validate_record && !v->validate_engine))
        return TTF_EINVAL;
    if (e->frozen || e->validating)
        return TTF_ESTATE;
    for (i = 0; i < e->validator_count; ++i)
        if (!strcmp(e->validators[i]->name, v->name))
            return TTF_EINVAL;
    if (e->validator_count == TTF_MAX_RESOURCE_VALIDATORS)
        return TTF_ELIMIT;
    e->validators[e->validator_count++] = v;
    return TTF_OK;
}
/* Preserve file coordinates supplied by the loader. Callback messages describe
   the failure; row identity always comes from the record being checked. */
ttf_status_t ttf_validate_resource_record(const ttf_engine_t *e, const ttf_resource_record_t *r,
                                          int *validated, ttf_resource_diagnostic_t *d) {
    size_t i;
    *validated = 0;
    d->layer = r->layer;
    ttf_validation_copy(d->kind, sizeof(d->kind), r->kind);
    ttf_validation_copy(d->language, sizeof(d->language), r->language);
    ttf_validation_copy(d->key, sizeof(d->key), r->key);
    for (i = 0; i < e->validator_count; ++i) {
        const ttf_resource_validator_t *v = e->validators[i];
        int handled = 0;
        ttf_status_t status;
        if (!v->validate_record)
            continue;
        d->message[0] = 0;
        status = v->validate_record(r, &handled, d, v->userdata);
        /* Only a rejection exposes the diagnostic; an accepted record just keeps
           the strings terminated (the UTF-8 sanitizing pass per record and
           validator dominated engine start-up; 2026-10-03). */
        if (status == TTF_OK) {
            d->struct_size = sizeof(*d);
            d->path[sizeof(d->path) - 1] = d->kind[sizeof(d->kind) - 1] = 0;
            d->language[sizeof(d->language) - 1] = d->key[sizeof(d->key) - 1] = 0;
            d->message[sizeof(d->message) - 1] = 0;
        } else
            diagnostic_sanitize(d);
        if (handled)
            *validated = 1;
        if (status != TTF_OK) {
            if (d) {
                d->layer = r->layer;
                ttf_validation_copy(d->kind, sizeof(d->kind), r->kind);
                ttf_validation_copy(d->language, sizeof(d->language), r->language);
                ttf_validation_copy(d->key, sizeof(d->key), r->key);
                if (!d->message[0])
                    (void)snprintf(d->message, sizeof(d->message), "Rejected by validator %s",
                                   v->name);
                diagnostic_sanitize(d);
            }
            return status;
        }
    }
    return TTF_OK;
}
static int declaration_order(const void *left, const void *right) {
    const ttf_entry_t *a = *(const ttf_entry_t *const *)left;
    const ttf_entry_t *b = *(const ttf_entry_t *const *)right;
    return a->order < b->order ? -1 : a->order > b->order;
}
ttf_status_t ttf_engine_validate_resources(const ttf_engine_t *e,
                                           ttf_resource_validation_report_t *report,
                                           ttf_resource_diagnostic_t *diagnostic) {
    size_t count = 0, bucket, i, table;
    const ttf_entry_t **rows;
    ttf_resource_diagnostic_t local;
    ttf_status_t status = TTF_OK;
    ttf_engine_t *setup;
    if (!e || (report && report->struct_size != sizeof(*report)) ||
        (diagnostic && diagnostic->struct_size != sizeof(*diagnostic)))
        return TTF_EINVAL;
    if (e->validating)
        return TTF_ESTATE;
    setup = e->frozen ? NULL : (ttf_engine_t *)e;
    if (report)
        ttf_resource_validation_report_init(report);
    if (!diagnostic)
        diagnostic = &local;
    ttf_resource_diagnostic_init(diagnostic);
    for (table = 0; table < (e->cjk ? 2u : 1u); ++table) for (bucket = 0; bucket < TTF_HASH_BUCKETS; ++bucket) {
        const ttf_entry_t *entry;
        for (entry = table ? e->cjk->resources[bucket] : e->resources[bucket]; entry; entry = entry->next)
            ++count;
    }
    if (count > SIZE_MAX / sizeof(*rows))
        return TTF_ELIMIT;
    rows = count ? malloc(count * sizeof(*rows)) : NULL;
    if (count && !rows)
        return TTF_ENOMEM;
    i = 0;
    for (table = 0; table < (e->cjk ? 2u : 1u); ++table) for (bucket = 0; bucket < TTF_HASH_BUCKETS; ++bucket) {
        const ttf_entry_t *entry;
        for (entry = table ? e->cjk->resources[bucket] : e->resources[bucket]; entry; entry = entry->next)
            rows[i++] = entry;
    }
    if (count > 1)
        qsort(rows, count, sizeof(*rows), declaration_order);
    if (setup)
        setup->validating = 1;
    for (i = 0; i < count; ++i) {
        const ttf_entry_t *entry = rows[i];
        ttf_resource_record_t record = {sizeof(record), entry->kind,  entry->lang,
                                        entry->key,     entry->value, entry->layer};
        int validated;
        /* A registered custom validator may depend on outside state: check again. */
        if (entry->checked && entry->checked_validators == e->validator_count &&
            e->validator_count == e->builtin_validators)
            validated = entry->checked == 1; /* validated when loaded, same validators */
        else {
            status = ttf_validate_resource_record(e, &record, &validated, diagnostic);
            if (status != TTF_OK)
                break;
            if (setup && e->validator_count == e->builtin_validators) {
                ((ttf_entry_t *)entry)->checked = validated ? 1 : 2;
                ((ttf_entry_t *)entry)->checked_validators = e->validator_count;
            }
        }
        if (report) {
            ++report->records;
            if (validated)
                ++report->validated_records;
            else
                ++report->unvalidated_records;
        }
    }
    free(rows);
    if (status != TTF_OK) {
        if (setup)
            setup->validating = 0;
        return status;
    }
    for (i = 0; i < e->validator_count; ++i) {
        const ttf_resource_validator_t *v = e->validators[i];
        if (!v->validate_engine)
            continue;
        ttf_resource_diagnostic_init(diagnostic);
        status = v->validate_engine(e, diagnostic, v->userdata);
        diagnostic_sanitize(diagnostic);
        if (status != TTF_OK) {
            if (!diagnostic->message[0])
                (void)snprintf(diagnostic->message, sizeof(diagnostic->message),
                               "Rejected by validator %s", v->name);
            diagnostic_sanitize(diagnostic);
            if (setup)
                setup->validating = 0;
            return status;
        }
    }
    if (setup)
        setup->validating = 0;
    ttf_resource_diagnostic_init(diagnostic);
    return TTF_OK;
}
static int ascii_alnum(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
static int identifier(const char *s, size_t limit, int language) {
    size_t i, n = strlen(s);
    if (!n || n >= limit || !((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= 'A' && s[0] <= 'Z')))
        return 0;
    for (i = 1; i < n; ++i)
        if (!ascii_alnum((unsigned char)s[i]) && s[i] != '-' && (language || s[i] != '_'))
            return 0;
    return 1;
}
static int lower_words(const char *s, size_t limit) {
    size_t i, n = strlen(s);
    if (!n || n >= limit || s[0] == ' ' || s[n - 1] == ' ')
        return 0;
    for (i = 0; i < n; ++i)
        if (!(s[i] >= 'a' && s[i] <= 'z') && !(s[i] == ' ' && i && s[i - 1] != ' '))
            return 0;
    return 1;
}
static int typed_value(const char *value, const char *types, size_t limit, int language) {
    const char *colon = strchr(value, ':');
    char type[32];
    if (!colon || colon == value || (size_t)(colon - value) >= sizeof(type))
        return 0;
    memcpy(type, value, (size_t)(colon - value));
    type[colon - value] = 0;
    if (!ttf_validation_enum(type, types))
        return 0;
    /* Unit IDs can be Unicode (e.g. Korean counters); language tags cannot. */
    return language ? identifier(colon + 1, limit, 1)
                    : colon[1] && strlen(colon + 1) < limit && !strchr(colon + 1, ':');
}
static ttf_status_t core_record(const ttf_resource_record_t *r, int *handled,
                                ttf_resource_diagnostic_t *d, void *userdata) {
    const char *kind = r->kind, *key = r->key, *value = r->value;
    long number;
    (void)userdata;
    *handled = 1;
    if (!strcmp(kind, "classifier_priority")) {
        if (strlen(key) >= sizeof(((ttf_semantic_t *)0)->rule) ||
            !ttf_validation_integer(value, -1000, 1000, NULL))
            return ttf_validation_error(
                d, "classifier_priority requires a rule ID under 64 bytes and integer -1000..1000");
    } else if (!strcmp(kind, "phone_policy")) {
        long maximum = !strcmp(key, "extension_max_digits") ? TTF_IR_VALUE_SIZE - 1 : 15;
        if (strcmp(r->language, "*") ||
            !ttf_validation_enum(key, "international_min_digits|international_max_digits|"
                                      "national_min_digits|national_max_digits|extension_max_digits") ||
            !ttf_validation_integer(value, 1, maximum, NULL))
            return ttf_validation_error(d, "phone_policy requires common '*' namespace, a known "
                                           "digit limit and integer 1..15 (extension 1..95)");
    } else if (!strcmp(kind, "numeric_policy")) {
        if (strcmp(r->language, "*") || strcmp(key, "leading_zero_min_digits") ||
            !ttf_validation_integer(value, 2, TTF_IR_VALUE_SIZE - 1, NULL))
            return ttf_validation_error(d, "numeric_policy requires common '*' namespace, "
                                           "leading_zero_min_digits and integer 2..95");
    } else if (!strcmp(kind, "numeric_group_separator")) {
        uint32_t cp;
        size_t width, n = strlen(key);
        if (ttf_utf8_decode(key, n, &cp, &width) != 1 || width != n ||
            (cp != ',' && (cp < 128 || cp == 0x2028 || cp == 0x2029 ||
                           ttf_script_of(cp) != TTF_SCRIPT_WHITESPACE)) ||
            !ttf_validation_enum(value, "0|3"))
            return ttf_validation_error(d, "numeric_group_separator requires one supported "
                                           "horizontal Unicode space or comma, and value 0 or 3");
    } else if (!strcmp(kind, "currency_scale")) {
        if (strlen(key) >= 64 || !ttf_validation_integer(value, 3, 18, &number) || number % 3)
            return ttf_validation_error(
                d,
                "currency_scale requires a key under 64 bytes and an integer power 3,6,9,12,15,18");
    } else if (!strcmp(kind, "numeric_suffix")) {
        if (strlen(key) >= 64 || !typed_value(value, "COUNTER|CURRENCY|MEASURE", 32, 0))
            return ttf_validation_error(d, "numeric_suffix requires COUNTER:unit, CURRENCY:unit or "
                                           "MEASURE:unit with unit under 32 bytes");
    } else if (!strcmp(kind, "numeric_suffix_word")) {
        if (strlen(key) >= 64 || strcmp(value, "1"))
            return ttf_validation_error(d, "numeric_suffix_word requires a word under 64 bytes and value 1");
    } else if (!strcmp(kind, "currency_name")) {
        const char *colon = strchr(value, ':');
        char currency[32];
        if (!lower_words(key, 96) || !colon || colon == value ||
            (size_t)(colon - value) >= sizeof(currency) ||
            !identifier(colon + 1, TTF_LANGUAGE_TAG_SIZE, 1))
            return ttf_validation_error(
                d, "currency_name requires canonical lowercase ASCII words and currency:language");
        memcpy(currency, value, (size_t)(colon - value));
        currency[colon - value] = 0;
        if (!identifier(currency, sizeof(currency), 0))
            return ttf_validation_error(d, "currency_name has an invalid currency ID");
    } else if (!strcmp(kind, "currency_prefix")) {
        if (strlen(key) >= 64 || !identifier(value, 32, 0))
            return ttf_validation_error(
                d, "currency_prefix requires a key under 64 bytes and currency ID under 32 bytes");
    } else if (!strcmp(kind, "currency_name_cue")) {
        if (!lower_words(key, 64) || strchr(key, ' ') ||
            (strcmp(value, "1") && !identifier(value, 32, 0)))
            return ttf_validation_error(
                d, "currency_name_cue requires one lowercase ASCII word and 1 or a currency ID");
    } else if (!strcmp(kind, "currency_name_ambiguous") ||
               !strcmp(kind, "currency_name_mass_tail")) {
        if (!lower_words(key, !strcmp(kind, "currency_name_ambiguous") ? 96 : 64) ||
            (!strcmp(kind, "currency_name_mass_tail") && strchr(key, ' ')) ||
            !ttf_validation_enum(value, "0|1"))
            return ttf_validation_error(
                d, "currency name guard requires canonical lowercase ASCII key and value 0 or 1");
    } else if (!strcmp(kind, "ordinal_suffix")) {
        if (strlen(key) >= 64 || !identifier(value, TTF_LANGUAGE_TAG_SIZE, 1))
            return ttf_validation_error(
                d, "ordinal_suffix requires a key under 64 bytes and a source language tag");
    } else if (!strcmp(kind, "ordinal_tail")) {
        size_t i;
        for (i = 0; key[i] >= '0' && key[i] <= '9'; ++i) {
        }
        if (!i || key[i] || i >= TTF_IR_VALUE_SIZE || strlen(value) >= 64)
            return ttf_validation_error(
                d, "ordinal_tail requires a decimal tail and suffix under 64 bytes");
    } else if (!strcmp(kind, "semantic_term")) {
        if (strlen(key) >= 64 ||
            !typed_value(value, "WORD|INITIALISM|ACRONYM|ALNUM_TERM", TTF_LANGUAGE_TAG_SIZE, 1))
            return ttf_validation_error(d, "semantic_term requires WORD, INITIALISM, ACRONYM or "
                                           "ALNUM_TERM followed by :source-language");
    } else if (!strcmp(kind, "time_suffix")) {
        if (strlen(key) >= 64 || !ttf_validation_enum(value, "AM|PM"))
            return ttf_validation_error(d,
                                        "time_suffix requires a key under 64 bytes and AM or PM");
    } else if (!strcmp(kind, "quantity_range_separator") || !strcmp(kind, "numeric_continuation") ||
               !strcmp(kind, "boundary_word") || !strcmp(kind, "boundary_period") ||
               !strcmp(kind, "quantity_lookahead_tail")) {
        size_t limit = (!strcmp(kind, "boundary_word") || !strcmp(kind, "boundary_period") ||
                        !strcmp(kind, "quantity_lookahead_tail"))
                           ? 96
                           : 64;
        if (strlen(key) >= limit || !ttf_validation_enum(value, "0|1"))
            return ttf_validation_error(
                d, "boundary/continuation flag requires a bounded key and value 0 or 1");
    } else if (!strcmp(kind, "context_connector")) {
        uint32_t cp;
        size_t width, n = strlen(key);
        if (ttf_utf8_decode(key, n, &cp, &width) != 1 || width != n ||
            (ttf_script_of(cp) != TTF_SCRIPT_SYMBOL &&
             ttf_script_of(cp) != TTF_SCRIPT_PUNCTUATION) ||
            strcmp(value, "number"))
            return ttf_validation_error(d, "context_connector requires one punctuation/symbol "
                                           "codepoint and the number marker");
    } else if (!strcmp(kind, "boundary_syntax")) {
        if (strcmp(key, "dotted_initials"))
            *handled = 0;
        else if (!ttf_validation_enum(value, "0|1"))
            return ttf_validation_error(d, "dotted_initials boundary flag must be 0 or 1");
    } else if (!strcmp(kind, "protection")) {
        if (!ttf_validation_enum(key, "code|markup|path"))
            *handled = 0;
        else if (!ttf_validation_enum(value, "0|1"))
            return ttf_validation_error(d, "protection policy must be 0 or 1");
    } else if (!strcmp(kind, "protected_tag")) {
        size_t i;
        if (strlen(key) >= 64 || key[0] < 'a' || key[0] > 'z')
            return ttf_validation_error(
                d, "protected_tag requires a lowercase ASCII tag name under 64 bytes");
        for (i = 1; key[i]; ++i)
            if (!(key[i] >= 'a' && key[i] <= 'z') && !(key[i] >= '0' && key[i] <= '9') &&
                key[i] != ':' && key[i] != '_' && key[i] != '-')
                return ttf_validation_error(d, "protected_tag has an invalid tag name");
        if (!ttf_validation_enum(value, "opaque|transparent"))
            return ttf_validation_error(d, "protected_tag policy must be opaque or transparent");
    } else if (!strcmp(kind, "abbreviation") && !strcmp(r->language, "*")) {
        if (strlen(key) >= 64 || strcmp(value, "protected"))
            return ttf_validation_error(
                d, "common abbreviation requires a key under 64 bytes and protected value");
    } else if (!strcmp(kind, "lexicon")) {
        char *canonical = NULL;
        const char *colon = strchr(key, ':');
        char source[TTF_LANGUAGE_TAG_SIZE];
        ttf_status_t status;
        if (!colon || (size_t)(colon - key) >= sizeof(source))
            return ttf_validation_error(d, "lexicon key requires a bounded source language prefix");
        memcpy(source, key, (size_t)(colon - key));
        source[colon - key] = 0;
        if (!identifier(source, sizeof(source), 1))
            return ttf_validation_error(d, "lexicon source language prefix is invalid");
        status = ttf_lexicon_canonical_key(key, &canonical);
        free(canonical);
        if (status != TTF_OK)
            return status == TTF_ENOMEM
                       ? status
                       : ttf_validation_error(d, "lexicon key requires source-language:word or a "
                                                 "supported bounded phrase");
    } else if (!strcmp(kind, "exact")) {
        /* Literal key and literal replacement are intentionally unrestricted. */
    } else if (!strcmp(kind, "pattern")) {
        const char *p = key;
        while (*p) {
            if (!strncmp(p, "{digits}", 8))
                p += 8;
            else if (*p == '{' || *p == '}')
                return ttf_validation_error(
                    d,
                    "pattern supports only the {digits} placeholder; use exact for literal braces");
            else
                ++p;
        }
    } else
        *handled = 0;
    return TTF_OK;
}
static ttf_status_t core_engine(const ttf_engine_t *e, ttf_resource_diagnostic_t *d,
                                void *userdata) {
    static const char *const minima[] = {"international_min_digits", "national_min_digits"};
    static const char *const maxima[] = {"international_max_digits", "national_max_digits"};
    size_t i;
    (void)userdata;
    for (i = 0; i < 2; ++i) {
        long minimum = 7, maximum = 15;
        ttf_resource_layer_t low_layer = TTF_LAYER_COMMON, high_layer = TTF_LAYER_COMMON;
        const char *low = ttf_resource_get(e, "phone_policy", "*", minima[i], &low_layer);
        const char *high = ttf_resource_get(e, "phone_policy", "*", maxima[i], &high_layer);
        if (low)
            (void)ttf_validation_integer(low, 1, 15, &minimum);
        if (high)
            (void)ttf_validation_integer(high, 1, 15, &maximum);
        if (minimum > maximum) {
            int report_low = low && (!high || low_layer >= high_layer);
            ttf_validation_copy(d->kind, sizeof(d->kind), "phone_policy");
            ttf_validation_copy(d->language, sizeof(d->language), "*");
            ttf_validation_copy(d->key, sizeof(d->key), report_low ? minima[i] : maxima[i]);
            d->layer = report_low ? low_layer : high_layer;
            return ttf_validation_error(d, "effective phone minimum exceeds maximum");
        }
    }
    return TTF_OK;
}
const ttf_resource_validator_t ttf_resource_validator_core = {sizeof(ttf_resource_validator_t),
                                                              TTF_RESOURCE_VALIDATOR_ABI_VERSION,
                                                              "ttf.core",
                                                              NULL,
                                                              core_record,
                                                              core_engine};
