#include "../../src/core/internal.h"

static int failures;

static void add(ttf_engine_t *engine, const char *kind, const char *language,
                const char *key, const char *value, ttf_resource_layer_t layer) {
    if (ttf_resource_set(engine, kind, language, key, value, layer) != TTF_OK) {
        fprintf(stderr, "cannot add resource %s\n", key);
        ++failures;
    }
}

static void expect(const ttf_engine_t *engine, const char *language, const char *text,
                   const char *value, size_t length) {
    size_t matched = 999;
    const ttf_entry_t *entry = ttf_rule_match(engine, language, text, strlen(text), &matched);
    if ((!value && entry) || (value && (!entry || strcmp(entry->value, value))) ||
        matched != length) {
        fprintf(stderr, "rule match [%s]: expected %s/%zu, got %s/%zu\n", text,
                value ? value : "no match", length, entry ? entry->value : "no match", matched);
        ++failures;
    }
}

int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    size_t i;
    char key[32] = "en-US:scaleaaaaa";
    ttf_result_t result = {0};
    const char *input = "scaleaaaaa의 안내입니다.";
    const char *expected = "사전의 안내입니다.";
    if (argc != 2)
        return 2;
    ttf_config_init(&config);
    config.resource_root = argv[1];
    if (ttf_engine_create(&config, &engine) != TTF_OK)
        return 2;

    /* Pronunciation size must not change TN winner selection. Synthetic
       spellings and pronunciations are identifiers, not linguistic references. */
    for (i = 0; i < 10000; ++i) {
        size_t value = i, digit;
        for (digit = 0; digit < 5; ++digit) {
            key[15 - digit] = (char)('a' + value % 26);
            value /= 26;
        }
        add(engine, "lexicon", "ko-KR", key, "사전", TTF_LAYER_CUSTOMER);
    }
    add(engine, "exact", "*", "omega", "common", TTF_LAYER_COMMON);
    add(engine, "exact", "ko-KR", "omega", "domain", TTF_LAYER_DOMAIN);
    add(engine, "exact", "ko-KR", "number 42", "domain exact", TTF_LAYER_DOMAIN);
    add(engine, "pattern", "ko-KR", "number {digits}", "customer pattern", TTF_LAYER_CUSTOMER);
    add(engine, "pattern", "ko-KR", "scope {digits}", "long pattern", TTF_LAYER_CUSTOMER);
    add(engine, "exact", "ko-KR", "scope", "short exact", TTF_LAYER_CUSTOMER);
    add(engine, "pattern", "ko-KR", "delta {digits}", "shorter", TTF_LAYER_CUSTOMER);
    add(engine, "pattern", "ko-KR", "delta {digits} end", "longer", TTF_LAYER_CUSTOMER);
    add(engine, "pattern", "ko-KR", "rank {digits}", "less specific", TTF_LAYER_CUSTOMER);
    add(engine, "pattern", "ko-KR", "rank 1{digits}", "more specific", TTF_LAYER_CUSTOMER);
    add(engine, "pattern", "ko-KR", "first {digits}", "earlier", TTF_LAYER_CUSTOMER);
    add(engine, "pattern", "ko-KR", "first {digits}", "later", TTF_LAYER_CUSTOMER);
    add(engine, "exact", "ko-KR", "alpha", "whole word", TTF_LAYER_CUSTOMER);
    add(engine, "exact", "en-US", "language", "english", TTF_LAYER_CUSTOMER);
    if (ttf_engine_freeze(engine) != TTF_OK) {
        ttf_engine_destroy(engine);
        return 2;
    }
    expect(engine, "ko-KR", "omega", "domain", 5);
    expect(engine, "ko-KR", "number 42", "customer pattern", 9);
    expect(engine, "ko-KR", "scope 42", "short exact", 5);
    expect(engine, "ko-KR", "delta 42 end", "longer", 12);
    expect(engine, "ko-KR", "rank 123", "more specific", 8);
    expect(engine, "ko-KR", "first 42", "earlier", 8);
    expect(engine, "ko-KR", "alpha", "whole word", 5);
    expect(engine, "ko-KR", "alphabet", NULL, 0);
    expect(engine, "ko-KR", "language", NULL, 0);
    expect(engine, "en-US", "language", "english", 8);
    if (ttf_engine_freeze(engine) != TTF_OK)
        ++failures;
    expect(engine, "ko-KR", "first 42", "earlier", 8);
    if (ttf_normalize(engine, input, strlen(input), &result) != TTF_OK ||
        !result.text || strcmp(result.text, expected)) {
        fprintf(stderr, "large lexicon normalization failed\n");
        ++failures;
    }
    ttf_result_free(&result);
    ttf_engine_destroy(engine);
    return failures ? 1 : 0;
}
