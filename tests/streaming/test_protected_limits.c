#include "tts_text_frontend/ttf_language_pack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
typedef struct {
    char text[512];
    size_t used, callbacks, literals;
} output_t;
static int collect(const ttf_segment_t *segment, void *userdata) {
    output_t *out = (output_t *)userdata;
    size_t i;
    if (segment->length >= sizeof(out->text) - out->used)
        return 1;
    memcpy(out->text + out->used, segment->text, segment->length);
    out->used += segment->length;
    out->text[out->used] = '\0';
    ++out->callbacks;
    for (i = 0; i < segment->span_count; ++i)
        if (!strcmp(segment->spans[i].semantic.unit, "code"))
            ++out->literals;
    return 0;
}
static size_t overrun_tokens(const ttf_pack_input_t *input, ttf_semantic_t *out) {
    out->type = TTF_SEM_TEXT;
    out->input_end = input->length;
    return input->token_count - input->index + 1;
}
static size_t overrun_bytes(const ttf_pack_input_t *input, ttf_semantic_t *out) {
    out->type = TTF_SEM_TEXT;
    out->input_end = input->length + 1;
    return 1;
}
static ttf_status_t identity(const ttf_engine_t *engine, const ttf_semantic_t *semantic,
                              const char *surface, size_t length, ttf_buffer_t *output) {
    (void)engine;
    (void)semantic;
    return ttf_buffer_append(output, surface, length);
}
int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_stream_t *stream = NULL;
    output_t out = {{0}, 0, 0, 0};
    const char *first = "<code>GPU.\n", *second = "3개</code> ", *last = "3원입니다.";
    char oversized[81];
    unsigned mode;
    ttf_config_init(&config);
    REQUIRE(argc == 2);
    config.resource_root = argv[1];
    config.first_segment_target = config.normal_segment_target = 1;
    config.max_segment_length = 8;
    config.max_pending_bytes = 64;
    REQUIRE(ttf_engine_create(&config, &engine) == TTF_OK);
    REQUIRE(ttf_engine_freeze(engine) == TTF_OK);
    REQUIRE(ttf_stream_create(engine, collect, &out, &stream) == TTF_OK);
    REQUIRE(ttf_stream_write(stream, first, strlen(first)) == TTF_OK);
    REQUIRE(out.callbacks == 0);
    REQUIRE(ttf_stream_write(stream, second, strlen(second)) == TTF_OK);
    REQUIRE(out.callbacks == 1 && out.literals == 1);
    REQUIRE(!strcmp(out.text, "<code>GPU.\n3개</code> "));
    REQUIRE(ttf_stream_write(stream, last, strlen(last)) == TTF_OK);
    REQUIRE(ttf_stream_flush(stream) == TTF_OK);
    REQUIRE(!strcmp(out.text, "<code>GPU.\n3개</code> 삼 원입니다."));
    ttf_stream_destroy(stream);

    memset(&out, 0, sizeof(out));
    REQUIRE(ttf_stream_create(engine, collect, &out, &stream) == TTF_OK);
    REQUIRE(ttf_stream_write(stream, "`GPU.", 5) == TTF_OK);
    REQUIRE(out.callbacks == 0);
    REQUIRE(ttf_stream_flush(stream) == TTF_OK);
    REQUIRE(ttf_stream_flush(stream) == TTF_OK);
    REQUIRE(out.callbacks == 1 && out.literals == 1 && !strcmp(out.text, "`GPU."));
    ttf_stream_destroy(stream);

    memset(&out, 0, sizeof(out));
    memset(oversized, 'x', sizeof(oversized));
    memcpy(oversized, "<code>", 6);
    REQUIRE(ttf_stream_create(engine, collect, &out, &stream) == TTF_OK);
    REQUIRE(ttf_stream_write(stream, oversized, sizeof(oversized)) == TTF_ELIMIT);
    REQUIRE(ttf_stream_flush(stream) == TTF_ELIMIT && out.callbacks == 0);
    ttf_stream_destroy(stream);
    ttf_engine_destroy(engine);
    /* A third-party pack receives the prose window before a literal. Invalid
       callbacks must not consume protected bytes or leave a stale region index. */
    for (mode = 0; mode < 2; ++mode) {
        const char *input = "x<code>GPU</code>";
        ttf_result_t result = {0};
        ttf_language_pack_t pack = {0};
        pack.struct_size = sizeof(pack);
        pack.abi_version = TTF_ABI_VERSION;
        pack.language_tag = "xx-Limit";
        pack.classify = mode ? overrun_bytes : overrun_tokens;
        pack.verbalize = identity;
        REQUIRE(ttf_engine_create(&config, &engine) == TTF_OK);
        REQUIRE(ttf_engine_register_pack(engine, &pack) == TTF_OK);
        REQUIRE(ttf_engine_freeze(engine) == TTF_OK);
        REQUIRE(ttf_normalize(engine, input, strlen(input), &result) == TTF_ERESOURCE);
        ttf_result_free(&result);
        ttf_engine_destroy(engine);
    }
    puts("Protected pending, early callback, final flush and byte cap checks passed");
    return 0;
}
