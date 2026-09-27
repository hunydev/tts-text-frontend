/* Public setup-validation contract. Fixtures are original policy data, not a
 * mirror of validator implementation. Both source and compiled transports must
 * reject the same invalid policy without changing the engine. */
#include "tts_text_frontend/ttf_resource_validation.h"
#include "tts_text_frontend/ttf_language_pack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#define MAKE_DIRECTORY(path) _mkdir(path)
#else
#define MAKE_DIRECTORY(path) mkdir(path, 0700)
#endif

static int checks, failures;
static const char *resource_root, *scratch_root;
#define EXPECT(condition)                                                                          \
    do {                                                                                           \
        ++checks;                                                                                  \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition);                        \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)
#define STATUS(call, expected)                                                                     \
    do {                                                                                           \
        ttf_status_t observed_ = (call);                                                           \
        ++checks;                                                                                  \
        if (observed_ != (expected)) {                                                             \
            fprintf(stderr, "%s:%d: %s: expected %s, got %s\n", __FILE__, __LINE__, #call,         \
                    ttf_status_string(expected), ttf_status_string(observed_));                    \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)

static void path_for(char path[1024], const char *name) {
    int written = snprintf(path, 1024, "%s/%s", scratch_root, name);
    EXPECT(written >= 0 && written < 1024);
}
static void little_endian(unsigned char *output, uint32_t value) {
    size_t i;
    for (i = 0; i < 4; ++i)
        output[i] = (unsigned char)(value >> (8 * i));
}
static int write_file(const char *path, const char *text, int compiled) {
    FILE *file = fopen(path, "wb");
    size_t length = strlen(text), i;
    int ok;
    EXPECT(file != NULL);
    if (!file)
        return 0;
    if (compiled) {
        unsigned char header[16] = {'T', 'T', 'F', 'B', 1, 0, 0, 0};
        uint32_t hash = 2166136261u;
        for (i = 0; i < length; ++i) {
            hash ^= (unsigned char)text[i];
            hash *= 16777619u;
        }
        little_endian(header + 8, (uint32_t)length);
        little_endian(header + 12, hash);
        EXPECT(fwrite(header, 1, sizeof(header), file) == sizeof(header));
    }
    ok = fwrite(text, 1, length, file) == length;
    EXPECT(ok);
    EXPECT(fclose(file) == 0);
    return ok;
}
static ttf_engine_t *create_engine(const char *language) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_resource_diagnostic_t diagnostic;
    ttf_config_init(&config);
    config.resource_root = resource_root;
    config.default_language = language;
    ttf_resource_diagnostic_init(&diagnostic);
    STATUS(ttf_engine_create_ex(&config, &engine, &diagnostic), TTF_OK);
    if (!engine)
        fprintf(stderr, "create diagnostic: %s:%zu: %s\n", diagnostic.path, diagnostic.line,
                diagnostic.message);
    return engine;
}
static ttf_status_t load_text(ttf_engine_t *engine, const char *text, ttf_resource_layer_t layer,
                              int compiled, ttf_resource_diagnostic_t *diagnostic) {
    char path[1024];
    path_for(path, compiled ? "validation-resource.bin" : "validation-resource.tsv");
    if (!write_file(path, text, compiled))
        return TTF_EIO;
    return ttf_resource_load_ex(engine, path, layer, diagnostic);
}
static void expect_value(const ttf_engine_t *engine, const char *kind, const char *language,
                         const char *key, const char *expected,
                         ttf_resource_layer_t expected_layer) {
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    const char *value = ttf_resource_get(engine, kind, language, key, &layer);
    EXPECT(value != NULL);
    if (value) {
        EXPECT(!strcmp(value, expected));
        EXPECT(layer == expected_layer);
    }
}
static ttf_resource_validation_report_t report_for(const ttf_engine_t *engine) {
    ttf_resource_validation_report_t report;
    ttf_resource_validation_report_init(&report);
    STATUS(ttf_engine_validate_resources(engine, &report, NULL), TTF_OK);
    EXPECT(report.records == report.validated_records + report.unvalidated_records);
    return report;
}

static ttf_status_t third_verbalizer(const ttf_engine_t *engine, const ttf_semantic_t *semantic,
                                     const char *surface, size_t length, ttf_buffer_t *out) {
    (void)engine;
    if (semantic->type == TTF_SEM_CARDINAL)
        return ttf_buffer_puts(out, "third-pack-number");
    return ttf_buffer_append(out, surface, length);
}
/* The original language-pack ABI is sufficient: no validation fields were added. */
static const ttf_language_pack_t third_pack = {sizeof(ttf_language_pack_t),
                                               TTF_ABI_VERSION,
                                               "ja-Test",
                                               NULL,
                                               NULL,
                                               NULL,
                                               NULL,
                                               third_verbalizer,
                                               NULL,
                                               NULL,
                                               NULL};
typedef struct {
    size_t record_calls, final_calls;
    int reject_final, test_reentrancy;
    ttf_engine_t *engine;
    const ttf_resource_validator_t *validator;
    const char *load_path;
} custom_state_t;

static void check_read_only(custom_state_t *state) {
    ttf_resource_diagnostic_t diagnostic;
    ttf_resource_validation_report_t report;
    ttf_resource_diagnostic_init(&diagnostic);
    ttf_resource_validation_report_init(&report);
    STATUS(ttf_resource_load_ex(state->engine, state->load_path, TTF_LAYER_USER, &diagnostic),
           TTF_ESTATE);
    STATUS(ttf_engine_register_resource_validator(state->engine, state->validator), TTF_ESTATE);
    STATUS(ttf_engine_register_pack(state->engine, &third_pack), TTF_ESTATE);
    STATUS(ttf_lexicon_add(state->engine, TTF_LAYER_USER, "en-US", "ko-KR", "validation-mutation",
                           "변경"),
           TTF_ESTATE);
    STATUS(
        ttf_lexicon_remove(state->engine, TTF_LAYER_USER, "en-US", "ko-KR", "validation-mutation"),
        TTF_ESTATE);
    STATUS(ttf_engine_freeze(state->engine), TTF_ESTATE);
    STATUS(ttf_engine_validate_resources(state->engine, &report, &diagnostic), TTF_ESTATE);
    EXPECT(ttf_resource_get(state->engine, "date_policy", "en-US", "min_year", NULL) != NULL);
}
static ttf_status_t custom_record(const ttf_resource_record_t *record, int *handled,
                                  ttf_resource_diagnostic_t *diagnostic, void *userdata) {
    custom_state_t *state = (custom_state_t *)userdata;
    ++state->record_calls;
    EXPECT(record->struct_size == sizeof(*record));
    EXPECT(record->kind && record->language && record->key && record->value);
    EXPECT((unsigned)record->layer <= TTF_LAYER_USER);
    *handled = 0;
    if (strcmp(record->kind, "test.ja.policy") ||
        (strcmp(record->language, "ja-Test") && strcmp(record->language, "*")))
        return TTF_OK;
    *handled = 1;
    if (state->test_reentrancy)
        check_read_only(state);
    if (!strcmp(record->key, "oom")) {
        snprintf(diagnostic->message, sizeof(diagnostic->message),
                 "simulated validator allocation failure");
        return TTF_ENOMEM;
    }
    if (strcmp(record->key, "mode") ||
        (strcmp(record->value, "kana") && strcmp(record->value, "plain"))) {
        snprintf(diagnostic->message, sizeof(diagnostic->message), "mode must be kana or plain");
        return TTF_ERESOURCE;
    }
    return TTF_OK;
}
static ttf_status_t custom_final(const ttf_engine_t *engine, ttf_resource_diagnostic_t *diagnostic,
                                 void *userdata) {
    custom_state_t *state = (custom_state_t *)userdata;
    ++state->final_calls;
    EXPECT(engine == state->engine);
    if (state->test_reentrancy)
        check_read_only(state);
    if (state->reject_final) {
        snprintf(diagnostic->kind, sizeof(diagnostic->kind), "test.ja.policy");
        snprintf(diagnostic->language, sizeof(diagnostic->language), "ja-Test");
        snprintf(diagnostic->key, sizeof(diagnostic->key), "mode");
        snprintf(diagnostic->message, sizeof(diagnostic->message),
                 "custom final constraint rejected");
        return TTF_ERESOURCE;
    }
    return TTF_OK;
}
static ttf_resource_validator_t custom_validator(custom_state_t *state, const char *name) {
    ttf_resource_validator_t validator = {sizeof(ttf_resource_validator_t),
                                          TTF_RESOURCE_VALIDATOR_ABI_VERSION,
                                          name,
                                          state,
                                          custom_record,
                                          custom_final};
    return validator;
}

static ttf_status_t additional_record(const ttf_resource_record_t *record, int *handled,
                                      ttf_resource_diagnostic_t *diagnostic, void *userdata) {
    int *reject = (int *)userdata;
    *handled = !strcmp(record->kind, "test.ja.policy");
    if (*handled && *reject) {
        snprintf(diagnostic->message, sizeof(diagnostic->message),
                 "second applicable validator rejected");
        return TTF_ERESOURCE;
    }
    return TTF_OK;
}

static void test_multiple_validators(void) {
    ttf_engine_t *engine = create_engine("ko-KR");
    custom_state_t state = {0};
    int reject = 0;
    ttf_resource_validator_t first = custom_validator(&state, "test.first-applicable");
    ttf_resource_validator_t second = {sizeof(ttf_resource_validator_t),
                                       TTF_RESOURCE_VALIDATOR_ABI_VERSION,
                                       "test.second-applicable",
                                       &reject,
                                       additional_record,
                                       NULL};
    ttf_resource_validation_report_t before, after;
    ttf_resource_diagnostic_t diagnostic;
    if (!engine)
        return;
    state.engine = engine;
    STATUS(ttf_engine_register_resource_validator(engine, &first), TTF_OK);
    STATUS(ttf_engine_register_resource_validator(engine, &second), TTF_OK);
    before = report_for(engine);
    STATUS(load_text(engine, "test.ja.policy\tja-Test\tmode\tkana\n", TTF_LAYER_DOMAIN, 0, NULL),
           TTF_OK);
    after = report_for(engine);
    EXPECT(after.records == before.records + 1);
    EXPECT(after.validated_records == before.validated_records + 1); /* Rows, not validator hits. */
    reject = 1;
    ttf_resource_diagnostic_init(&diagnostic);
    STATUS(load_text(engine, "test.ja.policy\tja-Test\tmode\tplain\n", TTF_LAYER_CUSTOMER, 0,
                     &diagnostic),
           TTF_ERESOURCE);
    EXPECT(!strcmp(diagnostic.message, "second applicable validator rejected"));
    expect_value(engine, "test.ja.policy", "ja-Test", "mode", "kana", TTF_LAYER_DOMAIN);
    STATUS(ttf_engine_validate_resources(engine, NULL, &diagnostic), TTF_ERESOURCE);
    EXPECT(!strcmp(diagnostic.message, "second applicable validator rejected"));
    STATUS(ttf_engine_freeze(engine), TTF_ERESOURCE);
    reject = 0;
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    ttf_engine_destroy(engine);
}

static void test_custom_and_legacy_pack(void) {
    ttf_engine_t *engine = create_engine("ja-Test");
    custom_state_t state = {0};
    ttf_resource_validator_t validator = custom_validator(&state, "test.ja.validator");
    ttf_resource_validation_report_t before, after;
    ttf_resource_diagnostic_t diagnostic;
    ttf_result_t result = {0};
    if (!engine)
        return;
    state.engine = engine;
    state.validator = &validator;
    STATUS(ttf_engine_register_pack(engine, &third_pack), TTF_OK);
    before = report_for(engine);
    STATUS(load_text(engine, "test.ja.policy\tja-Test\tmode\tkana\n", TTF_LAYER_DOMAIN, 0, NULL),
           TTF_OK);
    after = report_for(engine);
    EXPECT(after.records == before.records + 1);
    EXPECT(after.unvalidated_records == before.unvalidated_records + 1);
    STATUS(ttf_engine_register_resource_validator(engine, &validator), TTF_OK);
    after = report_for(engine);
    EXPECT(after.records == before.records + 1);
    EXPECT(after.validated_records == before.validated_records + 1);
    EXPECT(after.unvalidated_records == before.unvalidated_records);
    EXPECT(state.final_calls > 0 && state.record_calls > 0);
    ttf_resource_diagnostic_init(&diagnostic);
    STATUS(load_text(engine, "test.ja.policy\tja-Test\tmode\tinvalid\n", TTF_LAYER_USER, 0,
                     &diagnostic),
           TTF_ERESOURCE);
    EXPECT(!strcmp(diagnostic.kind, "test.ja.policy"));
    EXPECT(!strcmp(diagnostic.language, "ja-Test"));
    EXPECT(!strcmp(diagnostic.key, "mode"));
    EXPECT(!strcmp(diagnostic.message, "mode must be kana or plain"));
    EXPECT(diagnostic.layer == TTF_LAYER_USER && diagnostic.line == 1);
    expect_value(engine, "test.ja.policy", "ja-Test", "mode", "kana", TTF_LAYER_DOMAIN);
    /* Recognized built-in namespace in another language is not owned by EN. */
    STATUS(load_text(engine, "date_policy\tja-Test\tyear_style\tcustom-calendar\n",
                     TTF_LAYER_SYSTEM, 0, NULL),
           TTF_OK);
    after = report_for(engine);
    EXPECT(after.unvalidated_records == before.unvalidated_records + 1);
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    STATUS(ttf_normalize(engine, "3", 1, &result), TTF_OK);
    EXPECT(result.text != NULL);
    if (result.text)
        EXPECT(!strcmp(result.text, "third-pack-number"));
    ttf_result_free(&result);
    after = report_for(engine); /* Read-only inspection also works after freeze. */
    EXPECT(after.records == before.records + 2);
    STATUS(ttf_engine_register_resource_validator(engine, &validator), TTF_ESTATE);
    STATUS(
        load_text(engine, "test.ja.policy\tja-Test\tmode\tplain\n", TTF_LAYER_USER, 0, &diagnostic),
        TTF_ESTATE);
    ttf_engine_destroy(engine);
}

static void test_unknown_layering_and_atomicity(void) {
    ttf_engine_t *engine = create_engine("ko-KR");
    ttf_resource_validation_report_t before, after;
    ttf_resource_diagnostic_t diagnostic;
    int compiled;
    if (!engine)
        return;
    before = report_for(engine);
    STATUS(load_text(engine, "future.kind\t*\tkey\tcommon\n", TTF_LAYER_COMMON, 0, NULL), TTF_OK);
    STATUS(load_text(engine,
                     "future.kind\tko-KR\tkey\tfirst\n"
                     "future.kind\tko-KR\tkey\tsecond\n",
                     TTF_LAYER_DOMAIN, 0, NULL),
           TTF_OK);
    expect_value(engine, "future.kind", "ko-KR", "key", "first", TTF_LAYER_DOMAIN);
    STATUS(
        load_text(engine, "future.kind\t*\tkey\tcustomer-wildcard\n", TTF_LAYER_CUSTOMER, 0, NULL),
        TTF_OK);
    expect_value(engine, "future.kind", "ko-KR", "key", "customer-wildcard", TTF_LAYER_CUSTOMER);
    STATUS(load_text(engine, "future.kind\tko-KR\tkey\tcustomer-specific\n", TTF_LAYER_CUSTOMER, 0,
                     NULL),
           TTF_OK);
    expect_value(engine, "future.kind", "ko-KR", "key", "customer-specific", TTF_LAYER_CUSTOMER);
    after = report_for(engine);
    EXPECT(after.records == before.records + 5); /* Includes shadowed declarations. */
    EXPECT(after.unvalidated_records == before.unvalidated_records + 5);
    EXPECT(after.validated_records == before.validated_records);
    before = after;
    for (compiled = 0; compiled <= 1; ++compiled) {
        char path[1024];
        ttf_resource_diagnostic_init(&diagnostic);
        STATUS(load_text(engine,
                         "# Physical row numbering includes comments and empty rows.\n"
                         "future.kind\tko-KR\trolled-back\tpresent-only-in-rejected-file\n"
                         "future.kind\tko-KR\tkey\tinvalid-user-override\n"
                         "\ninitialism_policy\ten-US\tdotted\tunknown-enum\n",
                         TTF_LAYER_USER, compiled, &diagnostic),
               TTF_ERESOURCE);
        path_for(path, compiled ? "validation-resource.bin" : "validation-resource.tsv");
        EXPECT(!strcmp(diagnostic.path, path));
        EXPECT(diagnostic.line == 5);
        EXPECT(diagnostic.layer == TTF_LAYER_USER);
        EXPECT(!strcmp(diagnostic.kind, "initialism_policy"));
        EXPECT(!strcmp(diagnostic.language, "en-US"));
        EXPECT(!strcmp(diagnostic.key, "dotted"));
        EXPECT(diagnostic.message[0] != '\0');
        EXPECT(ttf_resource_get(engine, "future.kind", "ko-KR", "rolled-back", NULL) == NULL);
        expect_value(engine, "future.kind", "ko-KR", "key", "customer-specific",
                     TTF_LAYER_CUSTOMER);
        after = report_for(engine);
        EXPECT(after.records == before.records);
        EXPECT(after.validated_records == before.validated_records);
        EXPECT(after.unvalidated_records == before.unvalidated_records);
    }
    STATUS(load_text(engine,
                     "future.kind\tko-KR\tkey\tuser-first\n"
                     "future.kind\tko-KR\tkey\tuser-second\n",
                     TTF_LAYER_USER, 0, NULL),
           TTF_OK);
    expect_value(engine, "future.kind", "ko-KR", "key", "user-first", TTF_LAYER_USER);
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    expect_value(engine, "future.kind", "ko-KR", "key", "user-first", TTF_LAYER_USER);
    ttf_engine_destroy(engine);
}

static void test_known_invalid_rows(void) {
    static const char *invalid[] = {
        "weekday_label\ten-US\tFri\t5\n",
        "weekday_label\ten-US\tfri\t8\n",
        "weekday_label\ten-US\tfri\t05\n",
        "weekday_label\ten-US\tfri.\t5\n",
        "weekday_label\tko-KR\t금\t5\n",
        "weekday\ten-US\t0\tFriday\n",
        "weekday\tko-KR\t8\t금요일\n",
        "weekday\ten-US\t5\t   \n",
        "initialism_policy\ten-US\tdotted\tletter\n",
        "initialism_policy\t*\tdotted\tletter\n",
        "initialism_policy\ten-US\tunknown-key\tletters\n",
        "date_policy\ten-US\tyear_style\tCalendar\n",
        "date_policy\ten-US\tmin_year\t999999999999999999999999999999\n",
        "date_policy\ten-US\tmin_year\t1000junk\n",
        "date_policy\ten-US\tmin_year\t 1000\n",
        "date_policy\ten-US\tmin_year\t0\n",
        "date_policy\ten-US\tmax_year\t10000\n",
        "date_policy\t*\tmax_year\tNaN\n",
        "currency_minor_digits\ten-US\tUSD\t3\n",
        "ordinal_policy\tko-KR\tlarge\tnative\n",
        "counter\tko-KR\t개\tNatural\n",
        "counter_role_before\tko-KR\t총\tfreqency\n",
        "ordinal_prefix_unit\tko-KR\t회\t2\n",
        "phone_onset\tko-KR\tK\t19\n",
        "phone_vowel\tko-KR\tAA\t21\n",
        "phone_coda\tko-KR\tK\t28\n",
        "classifier_priority\tko-KR\tko:suffix-ordinal\t1001\n",
        "classifier_priority\t*\tcustom:rule\t2suffix\n",
        "numeric_group_separator\t*\t,\t2\n",
        "numeric_group_separator\t*\t,\t3.0\n"};
    ttf_engine_t *engine = create_engine("ko-KR");
    ttf_resource_validation_report_t before, after;
    ttf_resource_diagnostic_t diagnostic;
    size_t i;
    int compiled;
    if (!engine)
        return;
    before = report_for(engine);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        for (compiled = 0; compiled <= 1; ++compiled) {
            ttf_resource_diagnostic_init(&diagnostic);
            {
                ttf_status_t observed =
                    load_text(engine, invalid[i], TTF_LAYER_CUSTOMER, compiled, &diagnostic);
                EXPECT(observed == TTF_ERESOURCE);
                if (observed != TTF_ERESOURCE)
                    fprintf(stderr, "accepted invalid fixture: %s", invalid[i]);
            }
            EXPECT(diagnostic.line == 1 && diagnostic.layer == TTF_LAYER_CUSTOMER);
            EXPECT(diagnostic.kind[0] && diagnostic.language[0] && diagnostic.key[0] &&
                   diagnostic.message[0]);
        }
    after = report_for(engine);
    EXPECT(after.records == before.records);
    EXPECT(after.validated_records == before.validated_records);
    EXPECT(after.unvalidated_records == before.unvalidated_records);
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    ttf_engine_destroy(engine);
}

static void test_effective_final_constraints(void) {
    ttf_engine_t *engine = create_engine("en-US");
    ttf_resource_diagnostic_t diagnostic;
    ttf_resource_validation_report_t report;
    ttf_result_t result = {0};
    if (!engine)
        return;
    ttf_resource_diagnostic_init(&diagnostic);
    STATUS(load_text(engine, "date_policy\ten-US\tmin_year\t2200\n", TTF_LAYER_CUSTOMER, 0,
                     &diagnostic),
           TTF_OK); /* Valid scalar, invalid effective relation. */
    ttf_resource_validation_report_init(&report);
    STATUS(ttf_engine_validate_resources(engine, &report, &diagnostic), TTF_ERESOURCE);
    EXPECT(!strcmp(diagnostic.kind, "date_policy"));
    EXPECT(!strcmp(diagnostic.language, "en-US"));
    EXPECT(diagnostic.message[0] && diagnostic.line == 0);
    STATUS(ttf_engine_freeze(engine), TTF_ERESOURCE);
    STATUS(ttf_normalize(engine, "12", 2, &result), TTF_ESTATE);
    ttf_result_free(&result);
    STATUS(load_text(engine, "date_policy\t*\tmax_year\t2300\n", TTF_LAYER_USER, 1, &diagnostic),
           TTF_OK);
    expect_value(engine, "date_policy", "en-US", "max_year", "2300", TTF_LAYER_USER);
    STATUS(ttf_engine_validate_resources(engine, &report, &diagnostic), TTF_OK);
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    STATUS(ttf_normalize(engine, "12", 2, &result), TTF_OK);
    EXPECT(result.text != NULL);
    if (result.text)
        EXPECT(!strcmp(result.text, "twelve"));
    ttf_result_free(&result);
    STATUS(ttf_engine_validate_resources(engine, &report, &diagnostic), TTF_OK);
    ttf_engine_destroy(engine);
}

static void test_required_boundary_companions(void) {
    static const struct {
        const char *policy;
        const char *companion;
    } cases[] = {{"month_alias\ten-US\tsmarch\t3\n", "boundary_word\t*\tsmarch\t1\n"},
                 {"date_year_cue\ten-US\treview cycle\t1\n",
                  "boundary_word\t*\treview\t1\nboundary_word\t*\tcycle\t1\n"},
                 {"counter_role_before\tko-KR\t총합\tfrequency\n", "boundary_word\t*\t총합\t1\n"},
                 {"ordinal_marker\tko-KR\tprefix\t검증\n", "boundary_word\t*\t검증\t1\n"}};
    size_t i;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        ttf_engine_t *engine = create_engine("ko-KR");
        ttf_resource_diagnostic_t diagnostic;
        if (!engine)
            continue;
        ttf_resource_diagnostic_init(&diagnostic);
        STATUS(load_text(engine, cases[i].policy, TTF_LAYER_CUSTOMER, 0, &diagnostic), TTF_OK);
        STATUS(ttf_engine_validate_resources(engine, NULL, &diagnostic), TTF_ERESOURCE);
        EXPECT(diagnostic.line == 0 && diagnostic.message[0]);
        STATUS(ttf_engine_freeze(engine), TTF_ERESOURCE);
        STATUS(load_text(engine, cases[i].companion, TTF_LAYER_USER, 1, &diagnostic), TTF_OK);
        STATUS(ttf_engine_validate_resources(engine, NULL, &diagnostic), TTF_OK);
        STATUS(ttf_engine_freeze(engine), TTF_OK);
        ttf_engine_destroy(engine);
    }
    {
        ttf_engine_t *engine = create_engine("ko-KR");
        if (engine) {
            /* Disabled cues do not need an active lookahead companion. */
            STATUS(load_text(engine,
                             "date_year_cue\ten-US\tdisabled cue\t0\n"
                             "counter_role_before\tko-KR\t비활성\t0\n",
                             TTF_LAYER_CUSTOMER, 0, NULL),
                   TTF_OK);
            STATUS(ttf_engine_freeze(engine), TTF_OK);
            ttf_engine_destroy(engine);
        }
    }
}

static void test_late_registration_and_callbacks(void) {
    ttf_engine_t *engine = create_engine("ko-KR");
    custom_state_t state = {0};
    ttf_resource_validator_t validator = custom_validator(&state, "test.callback-validator");
    ttf_resource_diagnostic_t diagnostic;
    ttf_resource_validation_report_t before, after;
    char path[1024];
    if (!engine)
        return;
    state.engine = engine;
    state.validator = &validator;
    path_for(path, "validation-reentrant.tsv");
    if (!write_file(path, "future.kind\t*\tcallback-mutation\tforbidden\n", 0)) {
        ttf_engine_destroy(engine);
        return;
    }
    state.load_path = path;
    STATUS(load_text(engine, "test.ja.policy\tja-Test\tmode\tinvalid\n", TTF_LAYER_DOMAIN, 0, NULL),
           TTF_OK); /* Not yet owned by any validator. */
    STATUS(ttf_engine_register_resource_validator(engine, &validator), TTF_OK);
    ttf_resource_diagnostic_init(&diagnostic);
    STATUS(ttf_engine_validate_resources(engine, NULL, &diagnostic), TTF_ERESOURCE);
    EXPECT(!strcmp(diagnostic.kind, "test.ja.policy"));
    STATUS(ttf_engine_freeze(engine), TTF_ERESOURCE);
    ttf_engine_destroy(engine);

    engine = create_engine("ko-KR");
    if (!engine)
        return;
    state.engine = engine;
    state.test_reentrancy = 1;
    STATUS(ttf_engine_register_resource_validator(engine, &validator), TTF_OK);
    before = report_for(engine);
    STATUS(load_text(engine, "test.ja.policy\tja-Test\tmode\tkana\n", TTF_LAYER_CUSTOMER, 0, NULL),
           TTF_OK);
    EXPECT(ttf_resource_get(engine, "future.kind", "ko-KR", "callback-mutation", NULL) == NULL);
    STATUS(ttf_engine_validate_resources(engine, NULL, &diagnostic), TTF_OK);
    after = report_for(engine);
    EXPECT(after.records == before.records + 1);
    before = after;
    STATUS(load_text(engine,
                     "future.kind\t*\toom-rollback\tshould-disappear\n"
                     "test.ja.policy\tja-Test\toom\tplain\n",
                     TTF_LAYER_USER, 0, &diagnostic),
           TTF_ENOMEM);
    EXPECT(ttf_resource_get(engine, "future.kind", "ko-KR", "oom-rollback", NULL) == NULL);
    EXPECT(diagnostic.line == 2);
    after = report_for(engine);
    EXPECT(after.records == before.records);
    state.reject_final = 1;
    STATUS(ttf_engine_validate_resources(engine, NULL, &diagnostic), TTF_ERESOURCE);
    EXPECT(!strcmp(diagnostic.message, "custom final constraint rejected"));
    EXPECT(!strcmp(diagnostic.kind, "test.ja.policy"));
    EXPECT(!strcmp(diagnostic.key, "mode") && diagnostic.line == 0);
    STATUS(ttf_engine_freeze(engine), TTF_ERESOURCE);
    state.reject_final = 0;
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    /* Frozen validation is concurrent/read-only; recursive callbacks violate its
       purity contract rather than acquiring a mutable per-engine guard. */
    state.test_reentrancy = 0;
    STATUS(ttf_engine_validate_resources(engine, NULL, &diagnostic), TTF_OK);
    ttf_engine_destroy(engine);
}

static void test_arguments_and_descriptors(void) {
    ttf_engine_t *engine = create_engine("ko-KR"), *created = NULL;
    ttf_resource_diagnostic_t diagnostic;
    ttf_resource_validation_report_t report;
    ttf_config_t config;
    custom_state_t state = {0};
    ttf_resource_validator_t validator = custom_validator(&state, "test.arguments"), bad;
    char path[1024];
    if (!engine)
        return;
    state.engine = engine;
    ttf_resource_diagnostic_init(NULL);
    ttf_resource_validation_report_init(NULL);
    memset(&diagnostic, 0xff, sizeof(diagnostic));
    memset(&report, 0xff, sizeof(report));
    ttf_resource_diagnostic_init(&diagnostic);
    ttf_resource_validation_report_init(&report);
    EXPECT(diagnostic.struct_size == sizeof(diagnostic));
    EXPECT(!diagnostic.line && !diagnostic.path[0] && !diagnostic.message[0]);
    EXPECT(report.struct_size == sizeof(report) && !report.records && !report.validated_records &&
           !report.unvalidated_records);
    STATUS(ttf_engine_validate_resources(NULL, &report, &diagnostic), TTF_EINVAL);
    STATUS(ttf_engine_register_resource_validator(NULL, &validator), TTF_EINVAL);
    STATUS(ttf_engine_register_resource_validator(engine, NULL), TTF_EINVAL);
    bad = validator;
    bad.struct_size--;
    STATUS(ttf_engine_register_resource_validator(engine, &bad), TTF_EINVAL);
    bad = validator;
    bad.abi_version++;
    STATUS(ttf_engine_register_resource_validator(engine, &bad), TTF_EINVAL);
    bad = validator;
    bad.name = NULL;
    STATUS(ttf_engine_register_resource_validator(engine, &bad), TTF_EINVAL);
    bad = validator;
    bad.name = "";
    STATUS(ttf_engine_register_resource_validator(engine, &bad), TTF_EINVAL);
    bad = validator;
    bad.validate_record = NULL;
    bad.validate_engine = NULL;
    STATUS(ttf_engine_register_resource_validator(engine, &bad), TTF_EINVAL);
    STATUS(ttf_engine_register_resource_validator(engine, &validator), TTF_OK);
    STATUS(ttf_engine_register_resource_validator(engine, &validator), TTF_EINVAL);
    bad = validator;
    bad.userdata = NULL;
    STATUS(ttf_engine_register_resource_validator(engine, &bad), TTF_EINVAL); /* Duplicate ID. */
    report.struct_size--;
    STATUS(ttf_engine_validate_resources(engine, &report, &diagnostic), TTF_EINVAL);
    ttf_resource_validation_report_init(&report);
    diagnostic.struct_size--;
    STATUS(ttf_engine_validate_resources(engine, &report, &diagnostic), TTF_EINVAL);
    ttf_resource_diagnostic_init(&diagnostic);
    path_for(path, "validation-arguments.tsv");
    if (write_file(path, "future.kind\t*\tkey\tvalue\n", 0)) {
        STATUS(ttf_resource_load_ex(NULL, path, TTF_LAYER_DOMAIN, &diagnostic), TTF_EINVAL);
        STATUS(ttf_resource_load_ex(engine, NULL, TTF_LAYER_DOMAIN, &diagnostic), TTF_EINVAL);
        STATUS(ttf_resource_load_ex(engine, path, (ttf_resource_layer_t)-1, &diagnostic),
               TTF_EINVAL);
        STATUS(ttf_resource_load_ex(engine, path, (ttf_resource_layer_t)5, &diagnostic),
               TTF_EINVAL);
        diagnostic.struct_size--;
        STATUS(ttf_resource_load_ex(engine, path, TTF_LAYER_USER, &diagnostic), TTF_EINVAL);
        ttf_resource_diagnostic_init(&diagnostic);
        EXPECT(ttf_resource_get(engine, "future.kind", "ko-KR", "key", NULL) == NULL);
    }
    ttf_config_init(&config);
    config.resource_root = resource_root;
    STATUS(ttf_engine_create_ex(&config, NULL, &diagnostic), TTF_EINVAL);
    diagnostic.struct_size--;
    STATUS(ttf_engine_create_ex(&config, &created, &diagnostic), TTF_EINVAL);
    EXPECT(created == NULL);
    ttf_resource_diagnostic_init(&diagnostic);
    config.struct_size--;
    STATUS(ttf_engine_create_ex(&config, &created, &diagnostic), TTF_EINVAL);
    EXPECT(created == NULL);
    STATUS(ttf_engine_validate_resources(engine, NULL, NULL), TTF_OK);
    ttf_engine_destroy(engine);
}

static void test_manifest_diagnostics(void) {
    char directory[1024], manifest[1100], resource[1100];
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_resource_diagnostic_t diagnostic;
    int compiled;
    path_for(directory, "validation-manifest");
    (void)MAKE_DIRECTORY(directory);
    snprintf(manifest, sizeof(manifest), "%s/manifest.ttfm", directory);
    ttf_config_init(&config);
    config.resource_root = directory;
    for (compiled = 0; compiled <= 1; ++compiled) {
        const char *filename = compiled ? "policy.bin" : "policy.tsv";
        char manifest_text[100];
        snprintf(resource, sizeof(resource), "%s/%s", directory, filename);
        snprintf(manifest_text, sizeof(manifest_text), "# first manifest comment\n%s\n", filename);
        if (!write_file(manifest, manifest_text, 0) ||
            !write_file(resource,
                        "# first resource comment\n\n"
                        "initialism_policy\ten-US\tdotted\tinvalid\n",
                        compiled))
            continue;
        ttf_resource_diagnostic_init(&diagnostic);
        STATUS(ttf_engine_create_ex(&config, &engine, &diagnostic), TTF_ERESOURCE);
        EXPECT(engine == NULL);
        EXPECT(!strcmp(diagnostic.path, resource));
        EXPECT(diagnostic.line == 3);
        EXPECT(!strcmp(diagnostic.kind, "initialism_policy"));
        EXPECT(!strcmp(diagnostic.language, "en-US"));
        EXPECT(!strcmp(diagnostic.key, "dotted") && diagnostic.message[0]);
        EXPECT(diagnostic.layer == TTF_LAYER_SYSTEM);
        remove(resource);
    }
    if (write_file(manifest, "# missing file\nabsent-policy.tsv\n", 0)) {
        ttf_resource_diagnostic_init(&diagnostic);
        STATUS(ttf_engine_create_ex(&config, &engine, &diagnostic), TTF_EIO);
        EXPECT(engine == NULL);
        snprintf(resource, sizeof(resource), "%s/absent-policy.tsv", directory);
        EXPECT(!strcmp(diagnostic.path, resource));
        EXPECT(diagnostic.message[0]);
    }
    if (write_file(manifest, "# unsafe entry\n../escape.tsv\n", 0)) {
        ttf_resource_diagnostic_init(&diagnostic);
        STATUS(ttf_engine_create_ex(&config, &engine, &diagnostic), TTF_ERESOURCE);
        EXPECT(engine == NULL);
        EXPECT(!strcmp(diagnostic.path, manifest));
        EXPECT(diagnostic.line == 2 && diagnostic.message[0]);
    }
    {
        static const char nul[] = "# invalid physical line\npolicy.tsv\0hidden.tsv\n";
        static const char invalid_utf8[] = "# invalid physical line\npolicy\xff.tsv\n";
        static const char interior_cr[] = "# invalid physical line\npolicy.tsv\rhidden.tsv\n";
        static const struct {
            const char *data;
            size_t length;
        } malformed[] = {{nul, sizeof(nul) - 1},
                         {invalid_utf8, sizeof(invalid_utf8) - 1},
                         {interior_cr, sizeof(interior_cr) - 1}};
        size_t i;
        for (i = 0; i < sizeof(malformed) / sizeof(malformed[0]); ++i) {
            FILE *file = fopen(manifest, "wb");
            EXPECT(file != NULL);
            if (!file)
                continue;
            EXPECT(fwrite(malformed[i].data, 1, malformed[i].length, file) == malformed[i].length);
            EXPECT(fclose(file) == 0);
            ttf_resource_diagnostic_init(&diagnostic);
            STATUS(ttf_engine_create_ex(&config, &engine, &diagnostic), TTF_ERESOURCE);
            EXPECT(engine == NULL);
            EXPECT(!strcmp(diagnostic.path, manifest));
            EXPECT(diagnostic.line == 2 && diagnostic.message[0]);
        }
    }
    if (write_file(manifest, "# syntactically valid, missing required verbalizer labels\n", 0)) {
        ttf_resource_diagnostic_init(&diagnostic);
        STATUS(ttf_engine_create_ex(&config, &engine, &diagnostic), TTF_OK);
        if (engine) {
            STATUS(ttf_engine_validate_resources(engine, NULL, &diagnostic), TTF_ERESOURCE);
            EXPECT(diagnostic.line == 0 && diagnostic.kind[0] && diagnostic.key[0] &&
                   diagnostic.message[0]);
            STATUS(ttf_engine_freeze(engine), TTF_ERESOURCE);
            ttf_engine_destroy(engine);
            engine = NULL;
        }
    }
    remove(manifest);
}

int main(int argc, char **argv) {
    static const char *files[] = {"validation-resource.tsv", "validation-resource.bin",
                                  "validation-arguments.tsv", "validation-reentrant.tsv"};
    size_t i;
    if (argc != 3) {
        fprintf(stderr, "usage: %s RESOURCE_ROOT SCRATCH_DIRECTORY\n", argv[0]);
        return 2;
    }
    resource_root = argv[1];
    scratch_root = argv[2];
    test_arguments_and_descriptors();
    test_unknown_layering_and_atomicity();
    test_known_invalid_rows();
    test_effective_final_constraints();
    test_required_boundary_companions();
    test_custom_and_legacy_pack();
    test_multiple_validators();
    test_late_registration_and_callbacks();
    test_manifest_diagnostics();
    for (i = 0; i < sizeof(files) / sizeof(files[0]); ++i) {
        char path[1024];
        path_for(path, files[i]);
        remove(path);
    }
    printf("resource validation: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
