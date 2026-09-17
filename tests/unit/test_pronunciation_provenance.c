/* ABI v1 provenance protocol: custom language callbacks, no language-pair table. */
#include "tts_text_frontend/ttf_language_pack.h"
/* Internal pipeline entry, linked statically for custom callback contract tests. */
ttf_status_t ttf_foreign_render(const ttf_engine_t *, const ttf_language_pack_t *,
                                const ttf_language_pack_t *, const char *, size_t,
                                ttf_lexical_class_t, ttf_buffer_t *, char[64],
                                ttf_resource_layer_t *);
#include <stdio.h>
#include <string.h>

static int failures, checks;
#define EXPECT(condition) do { ++checks; if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; } } while (0)

static ttf_status_t source_phones(const ttf_engine_t *engine, const char *word, size_t length,
                                  ttf_lexical_class_t lexical, ttf_pronunciation_t *out) {
    (void)engine;
    (void)lexical;
    memset(out, 0, sizeof(*out));
    snprintf(out->alphabet, sizeof(out->alphabet), "custom-phones");
    snprintf(out->phones, sizeof(out->phones), "phone");
    if (length >= sizeof(out->provenance))
        return TTF_ELIMIT;
    memcpy(out->provenance, word, length);
    if (!strcmp(out->provenance, "unterminated-provenance"))
        memset(out->provenance, 'x', sizeof(out->provenance));
    else if (!strcmp(out->provenance, "unterminated-phones"))
        memset(out->phones, 'x', sizeof(out->phones));
    else if (!strcmp(out->provenance, "unterminated-alphabet"))
        memset(out->alphabet, 'x', sizeof(out->alphabet));
    else if (!strcmp(out->provenance, "rejected-source")) {
        snprintf(out->provenance, sizeof(out->provenance), "ttf-pron-v1:customer:dictionary");
        snprintf(out->phones, sizeof(out->phones), "REJECT");
    } else if (!strcmp(out->provenance, "empty-render")) {
        snprintf(out->provenance, sizeof(out->provenance), "ttf-pron-v1:user:dictionary");
        out->phones[0] = '\0';
    }
    return TTF_OK;
}
static ttf_status_t target_phones(const ttf_engine_t *engine, const ttf_pronunciation_t *pron,
                                   ttf_buffer_t *out) {
    (void)engine;
    if (!strcmp(pron->phones, "REJECT")) {
        ttf_status_t st = ttf_buffer_puts(out, "discarded");
        return st == TTF_OK ? TTF_EINVAL : st;
    }
    return *pron->phones ? ttf_buffer_puts(out, "mapped") : TTF_OK;
}
static ttf_status_t target_graphemes(const ttf_engine_t *engine, const char *word, size_t length,
                                      ttf_buffer_t *out) {
    (void)engine;
    (void)word;
    (void)length;
    return ttf_buffer_puts(out, "emergency");
}
static const ttf_language_pack_t source = {
    sizeof(ttf_language_pack_t), TTF_ABI_VERSION, "xx-Source", NULL,
    NULL, NULL, NULL, NULL, source_phones, NULL, NULL
};
static const ttf_language_pack_t target = {
    sizeof(ttf_language_pack_t), TTF_ABI_VERSION, "xx-Target", NULL,
    NULL, NULL, NULL, NULL, NULL, target_phones, target_graphemes
};

static void check(ttf_engine_t *engine, const char *metadata, ttf_status_t expected_status,
                  const char *expected_trace, ttf_resource_layer_t expected_layer,
                  const char *expected_output) {
    ttf_buffer_t output = {0};
    char trace[64] = {0};
    ttf_resource_layer_t layer = TTF_LAYER_USER; /* Detect failure to clear a previous stage. */
    ttf_status_t status;
    EXPECT(ttf_buffer_puts(&output, "prefix:") == TTF_OK);
    status = ttf_foreign_render(engine, &source, &target, metadata, strlen(metadata),
                               TTF_LEX_WORD, &output, trace, &layer);
    EXPECT(status == expected_status);
    if (expected_status == TTF_OK) {
        EXPECT(!strcmp(trace, expected_trace));
        EXPECT(layer == expected_layer);
        EXPECT(!strcmp(output.data + strlen("prefix:"), expected_output));
    } else {
        EXPECT(!strcmp(output.data, "prefix:"));
        EXPECT(!*trace);
    }
    ttf_buffer_free(&output);
}
int main(int argc, char **argv) {
    static const char *invalid[] = {
        "ttf-pron-v2:customer:dictionary", "ttf-pron-v1", "ttf-pron-",
        "ttf-pron-v1:customer", "ttf-pron-v1::dictionary",
        "ttf-pron-v1:unknown:dictionary", "ttf-pron-v1:Customer:dictionary",
        "ttf-pron-v1:3:dictionary", "ttf-pron-v1:customer:",
        "ttf-pron-v1:customer:path:extra", "ttf-pron-v1:customer:UPPER",
        "ttf-pron-v1:customer:path/extra", "ttf-pron-v1:customer:.path",
        "ttf-pron-v1:customer:한글", "unterminated-provenance",
        "unterminated-phones", "unterminated-alphabet"
    };
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    size_t i;
    if (argc != 2)
        return 2;
    ttf_config_init(&config);
    config.resource_root = argv[1];
    if (ttf_engine_create(&config, &engine) != TTF_OK)
        return 2;
    EXPECT(ttf_engine_freeze(engine) == TTF_OK);
    for (i = TTF_LAYER_COMMON; i <= TTF_LAYER_USER; ++i) {
        char metadata[64];
        snprintf(metadata, sizeof(metadata), "ttf-pron-v1:%s:dictionary.v1-2_x", ttf_layer_name((ttf_resource_layer_t)i));
        check(engine, metadata, TTF_OK, "dictionary.v1-2_x", (ttf_resource_layer_t)i, "mapped");
    }
    check(engine, "legacy-dictionary", TTF_OK, "legacy-dictionary", TTF_LAYER_COMMON, "mapped");
    check(engine, "legacy:custom", TTF_OK, "legacy:custom", TTF_LAYER_COMMON, "mapped");
    check(engine, "", TTF_OK, "", TTF_LAYER_COMMON, "mapped");
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        check(engine, invalid[i], TTF_ERESOURCE, NULL, TTF_LAYER_COMMON, NULL);
    check(engine, "rejected-source", TTF_OK, "grapheme-fallback", TTF_LAYER_COMMON, "emergency");
    check(engine, "empty-render", TTF_OK, "grapheme-fallback", TTF_LAYER_COMMON, "emergency");
    check(engine, "grapheme-g2p-prototype legacy", TTF_OK, "phrase:g2p ",
          TTF_LAYER_COMMON, "mapped mapped");
    check(engine, "ttf-pron-v1:customer:dictionary ttf-pron-v1:domain:grapheme-g2p-prototype",
          TTF_OK, "phrase:dictionary g2p ", TTF_LAYER_CUSTOMER, "mapped mapped");
    /* A target lexicon hit always wins before any source metadata is inspected. */
    ttf_engine_destroy(engine);
    engine = NULL;
    EXPECT(ttf_engine_create(&config, &engine) == TTF_OK);
    if (!engine)
        return 2;
    EXPECT(ttf_lexicon_add(engine, TTF_LAYER_USER, "xx-Source", "xx-Target",
                           invalid[0], "preferred") == TTF_OK);
    EXPECT(ttf_engine_freeze(engine) == TTF_OK);
    check(engine, invalid[0], TTF_OK, "lexicon:user", TTF_LAYER_USER, "preferred");
    {
        char metadata[64], expected[64];
        memset(metadata, 'a', sizeof(metadata) - 1);
        metadata[sizeof(metadata) - 1] = '\0';
        check(engine, metadata, TTF_OK, metadata, TTF_LAYER_COMMON, "mapped");
        memcpy(metadata, "ttf-pron-v1:customer:", strlen("ttf-pron-v1:customer:"));
        snprintf(expected, sizeof(expected), "%s", metadata + strlen("ttf-pron-v1:customer:"));
        check(engine, metadata, TTF_OK, expected, TTF_LAYER_CUSTOMER, "mapped");
    }
    ttf_engine_destroy(engine);
    printf("pronunciation provenance: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
