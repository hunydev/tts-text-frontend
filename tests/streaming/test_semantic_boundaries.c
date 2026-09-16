#include "tts_text_frontend/ttf_language_pack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const ttf_result_t *batch;
    size_t output_at, input_at, spans, callbacks;
    int invalid;
} collector_t;

static int collect(const ttf_segment_t *segment, void *userdata) {
    collector_t *c = (collector_t *)userdata;
    size_t i;
    c->callbacks++;
    if (segment->input_begin != c->input_at || segment->length > c->batch->length - c->output_at ||
        memcmp(segment->text, c->batch->text + c->output_at, segment->length))
        return c->invalid = 1;
    for (i = 0; i < segment->span_count; ++i) {
        const ttf_span_t *actual = &segment->spans[i], *expected;
        if (c->spans >= c->batch->span_count)
            return c->invalid = 1;
        expected = &c->batch->spans[c->spans++];
        if (actual->begin + c->output_at != expected->begin ||
            actual->end + c->output_at != expected->end ||
            actual->input_begin != expected->input_begin ||
            actual->input_end != expected->input_end ||
            memcmp(&actual->semantic, &expected->semantic, sizeof(actual->semantic)) ||
            strcmp(actual->context_trace, expected->context_trace) ||
            strcmp(actual->lexicon, expected->lexicon))
            return c->invalid = 1;
    }
    c->output_at += segment->length;
    c->input_at = segment->input_end;
    return 0;
}

static int check(ttf_engine_t *engine, const char *input, const char *expected, size_t *checks) {
    ttf_result_t batch = {0};
    size_t length = strlen(input), split;
    if (ttf_normalize(engine, input, length, &batch) != TTF_OK ||
        strcmp(batch.text ? batch.text : "", expected)) {
        fprintf(stderr, "input: %s\nexpected: %s\nactual: %s\n", input, expected,
                batch.text ? batch.text : "<error>");
        ttf_result_free(&batch);
        return 1;
    }
    for (split = 0; split <= length; ++split) {
        ttf_stream_t *stream = NULL;
        collector_t c = {&batch, 0, 0, 0, 0, 0};
        ttf_status_t st = ttf_stream_create(engine, collect, &c, &stream);
        if (st == TTF_OK)
            st = ttf_stream_write(stream, input, split);
        if (st == TTF_OK)
            st = ttf_stream_write(stream, input + split, length - split);
        if (st == TTF_OK)
            st = ttf_stream_flush(stream);
        ttf_stream_destroy(stream);
        if (st != TTF_OK || c.invalid || c.input_at != length || c.output_at != batch.length ||
            c.spans != batch.span_count) {
            fprintf(stderr, "chunk/metadata mismatch at %zu: %s\n", split, input);
            ttf_result_free(&batch);
            return 1;
        }
        (*checks)++;
    }
    ttf_result_free(&batch);
    return 0;
}

int main(int argc, char **argv) {
    static const char *cases[][2] = {
        {"3 kg", "삼 킬로그램"},
        {"3 개", "세 개"},
        {"12.5 %", "십이 점 오 퍼센트"},
        {"₩ 12,500", "만이천오백 원"},
        {"1 / 2", "이 분의 일"},
        {"14:30:05", "열네 시 삼십 분 오 초"},
        {"2026. 9. 16.", "이천이십육 년 구 월 십육 일"},
        {"12,34", "12,34"},
        {"2026-02-30", "2026-02-30"},
        {"2026년 2월 29일", "2026년 2월 29일"},
        {"2026년 6월 1일", "이천이십육 년 유 월 일 일"},
        {"3.5만   원", "삼만오천 원"},
        {"그는 \"Please go to the 1st floor\"라고 말했다.",
         "그는 \"Please go to the first floor\"라고 말했다."},
        {"그는 “Please go to the 1st floor”라고 말했다.",
         "그는 “Please go to the first floor”라고 말했다."},
        {"\"Apple은 사과입니다.\"라고 말했다.", "\"애플은 사과입니다.\"라고 말했다."},
        {"\"Please go to the 1st floor. Apple released the iPhone.\"",
         "\"Please go to the first floor. Apple released the iPhone.\""}};
    size_t i, checks = 0;
    unsigned profile;
    if (argc != 2)
        return 2;
    for (profile = 0; profile < 3; ++profile) {
        ttf_engine_t *engine = NULL;
        ttf_config_t config;
        ttf_config_init(&config);
        config.resource_root = argv[1];
        if (profile) {
            config.first_segment_target = 1;
            config.normal_segment_target = 3;
            config.max_segment_length = 12;
            config.sentence_split = profile == 1;
        }
        if (ttf_engine_create(&config, &engine) != TTF_OK || ttf_engine_freeze(engine) != TTF_OK)
            return 1;
        for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
            if (check(engine, cases[i][0], cases[i][1], &checks)) {
                ttf_engine_destroy(engine);
                return 1;
            }
        ttf_engine_destroy(engine);
    }
    /* Phrase-wide user entries outrank per-word lookups; a miss must still
       consult user entries for each word emitted by the source verbalizer. */
    for (profile = 0; profile < 2; ++profile) {
        ttf_engine_t *engine = NULL;
        ttf_config_t config;
        ttf_result_t result = {0};
        const char *input = "21st는 순서입니다.";
        ttf_config_init(&config);
        config.resource_root = argv[1];
        if (ttf_engine_create(&config, &engine) != TTF_OK ||
            ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "twenty", "사용자트웬티") !=
                TTF_OK ||
            (profile && ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "twenty first",
                                        "사용자서수") != TTF_OK) ||
            ttf_engine_freeze(engine) != TTF_OK)
            return 1;
        if (check(engine, input,
                  profile ? "사용자서수는 순서입니다." : "사용자트웬티 퍼스트는 순서입니다.",
                  &checks) ||
            ttf_normalize(engine, input, strlen(input), &result) != TTF_OK || !result.span_count ||
            result.spans[0].pronunciation_layer != TTF_LAYER_USER)
            return 1;
        ttf_result_free(&result);
        ttf_engine_destroy(engine);
    }
    printf("%zu exhaustive semantic/quotation byte-split comparisons passed\n", checks);
    return 0;
}
