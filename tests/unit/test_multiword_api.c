#include "tts_text_frontend/ttf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(test)                                                                              \
    do {                                                                                           \
        if (!(test)) {                                                                             \
            fprintf(stderr, "multiword API failure at line %d: %s\n", __LINE__, #test);            \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)

typedef struct {
    char text[8192];
    ttf_span_t spans[128];
    size_t length, span_count, input_end, calls;
    int invalid;
} copy_t;

/* All bytes and span structs are copied during callback lifetime. The tests
   deliberately examine the copies after stream destruction. */
static int copy_callback(const ttf_segment_t *segment, void *userdata) {
    copy_t *copy = (copy_t *)userdata;
    size_t i;
    if (segment->input_begin != copy->input_end ||
        segment->length >= sizeof(copy->text) - copy->length ||
        segment->span_count > sizeof(copy->spans) / sizeof(copy->spans[0]) - copy->span_count)
        return copy->invalid = 1;
    memcpy(copy->text + copy->length, segment->text, segment->length);
    for (i = 0; i < segment->span_count; ++i) {
        ttf_span_t *span = &copy->spans[copy->span_count++];
        *span = segment->spans[i];
        span->begin += copy->length;
        span->end += copy->length;
    }
    copy->length += segment->length;
    copy->text[copy->length] = '\0';
    copy->input_end = segment->input_end;
    ++copy->calls;
    return 0;
}

static int equal_copy(const copy_t *copy, const ttf_result_t *batch, size_t input_length) {
    size_t i;
    if (copy->invalid || copy->length != batch->length || copy->input_end != input_length ||
        copy->span_count != batch->span_count || strcmp(copy->text, batch->text))
        return 0;
    for (i = 0; i < copy->span_count; ++i) {
        const ttf_span_t *a = &copy->spans[i], *b = &batch->spans[i];
        if (a->begin != b->begin || a->end != b->end || a->script != b->script ||
            a->input_begin != b->input_begin || a->input_end != b->input_end ||
            memcmp(&a->semantic, &b->semantic, sizeof(a->semantic)) ||
            strcmp(a->lexicon, b->lexicon) || strcmp(a->context_trace, b->context_trace) ||
            strcmp(a->verbalizer, b->verbalizer) ||
            a->pronunciation_layer != b->pronunciation_layer)
            return 0;
    }
    return 1;
}

static int exhaustive(ttf_engine_t *engine, const char *input, const char *expected,
                      size_t *checks) {
    ttf_result_t batch = {0};
    size_t length = strlen(input), split;
    REQUIRE(ttf_normalize(engine, input, length, &batch) == TTF_OK);
    if (strcmp(batch.text, expected)) {
        fprintf(stderr, "input=%s\nexpected=%s\nactual=%s\n", input, expected, batch.text);
        ttf_result_free(&batch);
        return 1;
    }
    for (split = 0; split <= length; ++split) {
        ttf_stream_t *stream = NULL;
        copy_t copy = {0};
        REQUIRE(ttf_stream_create(engine, copy_callback, &copy, &stream) == TTF_OK);
        REQUIRE(ttf_stream_write(stream, input, split) == TTF_OK);
        REQUIRE(ttf_stream_write(stream, input + split, length - split) == TTF_OK);
        REQUIRE(ttf_stream_flush(stream) == TTF_OK);
        REQUIRE(ttf_stream_flush(stream) == TTF_OK);
        ttf_stream_destroy(stream);
        REQUIRE(equal_copy(&copy, &batch, length));
        ++*checks;
    }
    ttf_result_free(&batch);
    return 0;
}

static int create_engine(const char *resources, unsigned profile, ttf_engine_t **engine) {
    ttf_config_t config;
    ttf_config_init(&config);
    config.resource_root = resources;
    if (profile) {
        config.first_segment_target = 1;
        config.normal_segment_target = 3;
        config.max_segment_length = 12;
        config.sentence_split = profile == 1;
    }
    return ttf_engine_create(&config, engine) != TTF_OK;
}

static int check_first(ttf_engine_t *engine, const char *input, const char *expected,
                       const char *surface, ttf_resource_layer_t layer) {
    ttf_result_t result = {0};
    REQUIRE(ttf_normalize(engine, input, strlen(input), &result) == TTF_OK);
    REQUIRE(!strcmp(result.text, expected));
    REQUIRE(result.span_count && result.spans[0].input_begin == 0 &&
            result.spans[0].input_end == strlen(surface));
    REQUIRE(result.spans[0].pronunciation_layer == layer);
    ttf_result_free(&result);
    return 0;
}

int main(int argc, char **argv) {
    static const char *cases[][2] = {
        {"After Effects를 사용합니다.", "애프터 이펙츠를 사용합니다."},
        {"Bank of America는 은행입니다.", "뱅크 오브 아메리카는 은행입니다."},
        {"bAnK  of\tAmerica가 발표했습니다.", "뱅크 오브 아메리카가 발표했습니다."},
        {"Adobe Premiere Pro를 사용합니다.", "어도비 프리미어 프로를 사용합니다."},
        {"New York Times를 읽습니다.", "뉴욕 타임스를 읽습니다."},
        {"제목은 Adobe Premiere Pro입니다.", "제목은 Adobe Premiere Pro입니다."},
        {"Please check Bank of America.", "Please check Bank of America."},
        {"<code>Bank of America</code>를 확인합니다.",
         "<code>Bank of America</code>를 확인합니다."}};
    static const char *invalid[] = {"after\neffects",    "after-effects pro", "gpt4 pro",
                                    "a b c d e f g h i", " after effects",    "after effects ",
                                    "apple 애플"};
    size_t i, checks = 0;
    unsigned profile;
    REQUIRE(argc == 2);
    for (profile = 0; profile < 3; ++profile) {
        ttf_engine_t *engine = NULL;
        REQUIRE(!create_engine(argv[1], profile, &engine));
        REQUIRE(ttf_engine_freeze(engine) == TTF_OK);
        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
            REQUIRE(!exhaustive(engine, cases[i][0], cases[i][1], &checks));
        ttf_engine_destroy(engine);
    }
    /* Removing a normalized key exposes the next layer; higher-layer shorter
       entries and single words outrank longer lower-layer entries. */
    for (profile = 0; profile < 4; ++profile) {
        static const char *expected[] = {"사용자이펙트", "고객이펙트", "도메인이펙트",
                                         "애프터 이펙츠"};
        static const ttf_resource_layer_t layer[] = {TTF_LAYER_USER, TTF_LAYER_CUSTOMER,
                                                     TTF_LAYER_DOMAIN, TTF_LAYER_SYSTEM};
        ttf_engine_t *engine = NULL;
        char text[128];
        REQUIRE(!create_engine(argv[1], 0, &engine));
        REQUIRE(ttf_lexicon_add(engine, TTF_LAYER_DOMAIN, "en-US", "ko-KR", "after effects",
                                "도메인이펙트") == TTF_OK);
        REQUIRE(ttf_lexicon_add(engine, TTF_LAYER_CUSTOMER, "en-US", "ko-KR", "after effects",
                                "고객이펙트") == TTF_OK);
        REQUIRE(ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "AFTER  EFFECTS",
                                "사용자이펙트") == TTF_OK);
        if (profile >= 1)
            REQUIRE(ttf_lexicon_remove(engine, TTF_LAYER_USER, "en-US", "ko-KR",
                                       "After\tEffects") == TTF_OK);
        if (profile >= 2)
            REQUIRE(ttf_lexicon_remove(engine, TTF_LAYER_CUSTOMER, "en-US", "ko-KR",
                                       "after effects") == TTF_OK);
        if (profile >= 3)
            REQUIRE(ttf_lexicon_remove(engine, TTF_LAYER_DOMAIN, "en-US", "ko-KR",
                                       "after effects") == TTF_OK);
        REQUIRE(ttf_engine_freeze(engine) == TTF_OK);
        snprintf(text, sizeof(text), "%s를 사용합니다.", expected[profile]);
        REQUIRE(!check_first(engine, "After Effects를 사용합니다.", text, "After Effects",
                             layer[profile]));
        REQUIRE(ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "after effects",
                                "변경") == TTF_ESTATE);
        REQUIRE(ttf_lexicon_remove(engine, TTF_LAYER_SYSTEM, "en-US", "ko-KR", "after effects") ==
                TTF_ESTATE);
        ttf_engine_destroy(engine);
        ++checks;
    }
    {
        ttf_engine_t *engine = NULL;
        REQUIRE(!create_engine(argv[1], 1, &engine));
        REQUIRE(ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "apple", "사용자애플") ==
                TTF_OK);
        REQUIRE(ttf_lexicon_add(engine, TTF_LAYER_CUSTOMER, "en-US", "ko-KR", "adobe premiere",
                                "고객편집기") == TTF_OK);
        REQUIRE(ttf_lexicon_add(engine, TTF_LAYER_DOMAIN, "en-US", "ko-KR", "pro", "프로") ==
                TTF_OK);
        REQUIRE(ttf_engine_freeze(engine) == TTF_OK);
        REQUIRE(!check_first(engine, "Apple Watch를 소개합니다.", "사용자애플 워치를 소개합니다.",
                             "Apple", TTF_LAYER_USER));
        REQUIRE(!check_first(engine, "Adobe Premiere Pro를 사용합니다.",
                             "고객편집기 프로를 사용합니다.", "Adobe Premiere",
                             TTF_LAYER_CUSTOMER));
        REQUIRE(!exhaustive(engine, "Apple Watch를 소개합니다.", "사용자애플 워치를 소개합니다.",
                            &checks));
        ttf_engine_destroy(engine);
    }
    {
        ttf_engine_t *engine = NULL;
        char oversized[99];
        REQUIRE(!create_engine(argv[1], 0, &engine));
        memset(oversized, 'a', 94);
        memcpy(oversized + 94, " bb", 4);
        for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i) {
            REQUIRE(ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", invalid[i],
                                    "잘못된읽기") == TTF_EINVAL);
            ++checks;
        }
        REQUIRE(ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", oversized,
                                "너무긴읽기") == TTF_EINVAL);
        REQUIRE(ttf_engine_freeze(engine) == TTF_OK);
        REQUIRE(!check_first(engine, "After Effects를 사용합니다.", "애프터 이펙츠를 사용합니다.",
                             "After Effects", TTF_LAYER_SYSTEM));
        ttf_engine_destroy(engine);
        ++checks;
    }
    {
        ttf_config_t config;
        ttf_engine_t *engine = NULL;
        ttf_result_t result = {0};
        const char *input = "After Effects를 사용합니다.";
        ttf_config_init(&config);
        config.resource_root = argv[1];
        config.render_policy = TTF_PRESERVE_LANGUAGE;
        REQUIRE(ttf_engine_create(&config, &engine) == TTF_OK);
        REQUIRE(ttf_engine_freeze(engine) == TTF_OK);
        REQUIRE(ttf_normalize(engine, input, strlen(input), &result) == TTF_OK);
        REQUIRE(!strcmp(result.text, input));
        for (i = 0; i < result.span_count; ++i)
            REQUIRE(strcmp(result.spans[i].semantic.rule, "lexicon.phrase"));
        ttf_result_free(&result);
        ttf_engine_destroy(engine);
        ++checks;
    }
    {
        ttf_engine_t *engine = NULL;
        ttf_stream_t *stream = NULL;
        ttf_result_t batch = {0};
        copy_t copy = {0};
        const char *input = "After Effects를 사용합니다. ";
        REQUIRE(!create_engine(argv[1], 1, &engine));
        REQUIRE(ttf_engine_freeze(engine) == TTF_OK);
        REQUIRE(ttf_normalize(engine, input, strlen(input), &batch) == TTF_OK);
        REQUIRE(ttf_stream_create(engine, copy_callback, &copy, &stream) == TTF_OK);
        REQUIRE(ttf_stream_write(stream, "After ", 6) == TTF_OK);
        REQUIRE(copy.calls == 0);
        REQUIRE(ttf_stream_write(stream, input + 6, strlen(input) - 6) == TTF_OK);
        REQUIRE(copy.calls > 0); /* real output callback before EOF/flush */
        REQUIRE(ttf_stream_flush(stream) == TTF_OK);
        ttf_stream_destroy(stream);
        ttf_engine_destroy(engine);
        REQUIRE(equal_copy(&copy, &batch, strlen(input)));
        ttf_result_free(&batch);
        ++checks;
    }
    printf("%zu multiword API/layer/lifetime/exhaustive split checks passed\n", checks);
    return 0;
}
