/* A token whose reading a pack cannot produce must not fail the whole sentence:
   the written token is kept and every other token is still normalized. */
#include "tts_text_frontend/ttf_language_pack.h"
#include <stdio.h>
#include <string.h>

#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static ttf_status_t failure = TTF_EINVAL;

static int script_score(ttf_script_t script) {
    (void)script;
    return 100;
}
static int context_score(const ttf_pack_input_t *in) {
    (void)in;
    return 1000; /* outrank the built-in packs for this Latin fixture */
}
/* Every token is already TEXT from the universal classifier, so the fixture
   claims the token and its following space (two tokens) to win. */
static size_t classify(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const ttf_token_t *t = &in->tokens[in->index];
    size_t length = t->end - t->begin;
    const char *value = NULL;
    if (length == 3 && !memcmp(in->text + t->begin, "BAD", 3))
        value = "1";
    else if (length == 2 && !memcmp(in->text + t->begin, "OK", 2))
        value = "2";
    if (!value || in->index + 1 >= in->token_count)
        return 0;
    out->type = TTF_SEM_CARDINAL;
    (void)snprintf(out->value, sizeof(out->value), "%s", value);
    (void)snprintf(out->rule, sizeof(out->rule), "fixture.cardinal");
    out->input_end = in->tokens[in->index + 1].end;
    return 2;
}
static ttf_status_t verbalize(const ttf_engine_t *engine, const ttf_semantic_t *semantic,
                              const char *surface, size_t length, ttf_buffer_t *output) {
    (void)engine;
    if (semantic->type == TTF_SEM_CARDINAL && !strcmp(semantic->value, "1"))
        return failure;
    if (semantic->type == TTF_SEM_CARDINAL)
        return ttf_buffer_puts(output, "two ");
    return ttf_buffer_append(output, surface, length);
}

int main(int argc, char **argv) {
    static const ttf_status_t failures[] = {TTF_EINVAL, TTF_ERESOURCE};
    size_t i;
    REQUIRE(argc == 2);
    for (i = 0; i < sizeof(failures) / sizeof(failures[0]); ++i) {
        ttf_config_t config;
        ttf_engine_t *engine = NULL;
        ttf_result_t result = {0};
        ttf_language_pack_t pack = {0};
        const char *input = "x OK BAD OK y";
        failure = failures[i];
        ttf_config_init(&config);
        config.resource_root = argv[1];
        config.default_language = "xx-Fallback";
        pack.struct_size = sizeof(pack);
        pack.abi_version = TTF_ABI_VERSION;
        pack.language_tag = "xx-Fallback";
        pack.script_score = script_score;
        pack.context_score = context_score;
        pack.classify = classify;
        pack.verbalize = verbalize;
        REQUIRE(ttf_engine_create(&config, &engine) == TTF_OK);
        REQUIRE(ttf_engine_register_pack(engine, &pack) == TTF_OK);
        REQUIRE(ttf_engine_freeze(engine) == TTF_OK);
        REQUIRE(ttf_normalize(engine, input, strlen(input), &result) == TTF_OK);
        if (!result.text || strcmp(result.text, "x two BAD two y"))
            fprintf(stderr, "status %d: got [%s]\n", (int)failure, result.text ? result.text : "");
        REQUIRE(result.text && !strcmp(result.text, "x two BAD two y"));
        ttf_result_free(&result);
        ttf_engine_destroy(engine);
    }
    puts("Unrenderable token kept literally; sentence normalized");
    return 0;
}
