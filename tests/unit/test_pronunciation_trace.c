/* Request-local, additive pronunciation trace contract. Synthetic resources
   isolate provenance and policy selection from linguistic correctness. */
#include "tts_text_frontend/ttf_trace.h"
#include "tts_text_frontend/ttf_language_pack.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TTF_TEST_HAVE_THREADS
#include <pthread.h>
#endif

static size_t checks, failures;
static const char *resources, *scratch;
#define EXPECT(condition) do { ++checks; if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; } } while (0)
#define STATUS(expression, expected) do { ttf_status_t status_ = (expression); ++checks; \
    if (status_ != (expected)) { fprintf(stderr, "%s:%d: %s: expected %s, got %s\n", \
        __FILE__, __LINE__, #expression, ttf_status_string(expected), ttf_status_string(status_)); \
        ++failures; } } while (0)

static ttf_engine_t *engine_new(int short_segments) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_config_init(&config);
    config.resource_root = resources;
    if (short_segments) {
        config.first_segment_target = 1;
        config.normal_segment_target = 3;
        config.max_segment_length = 12;
    }
    STATUS(ttf_engine_create(&config, &engine), TTF_OK);
    return engine;
}
static void load(ttf_engine_t *engine, ttf_resource_layer_t layer, const char *text) {
    char path[1024];
    FILE *file;
    int size = snprintf(path, sizeof(path), "%s/detailed-trace-resource.tsv", scratch);
    EXPECT(size > 0 && (size_t)size < sizeof(path));
    file = fopen(path, "wb");
    EXPECT(file != NULL);
    if (!file)
        return;
    EXPECT(fwrite(text, 1, strlen(text), file) == strlen(text));
    EXPECT(fclose(file) == 0);
    STATUS(ttf_resource_load(engine, path, layer), TTF_OK);
    remove(path);
}
static int same_result(const ttf_result_t *a, const ttf_result_t *b) {
    return a->length == b->length && a->span_count == b->span_count &&
           (!a->length || !memcmp(a->text, b->text, a->length)) &&
           (!a->span_count || !memcmp(a->spans, b->spans, a->span_count * sizeof(*a->spans)));
}
static int same_event(const ttf_trace_event_t *a, const ttf_trace_event_t *b) {
#define NUMBER(field) if (a->field != b->field) return 0
#define STRING(field) if (!a->field || !b->field || strcmp(a->field, b->field)) return 0
    NUMBER(struct_size); NUMBER(stage); NUMBER(disposition);
    NUMBER(input_begin); NUMBER(input_end); NUMBER(output_begin); NUMBER(output_end);
    NUMBER(part_begin); NUMBER(part_end); NUMBER(phone_index); NUMBER(layer);
    NUMBER(layer_known); NUMBER(declaration_order);
    STRING(language); STRING(resource_language); STRING(kind); STRING(key);
    STRING(value); STRING(text); STRING(detail);
#undef NUMBER
#undef STRING
    return 1;
}
static int same_events(const ttf_trace_event_t *a, size_t an,
                       const ttf_trace_event_t *b, size_t bn) {
    size_t i;
    if (an != bn)
        return 0;
    for (i = 0; i < an; ++i)
        if (!same_event(a + i, b + i)) {
            fprintf(stderr, "event %zu differs: %s/%s versus %s/%s\n", i,
                    a[i].kind, a[i].key, b[i].kind, b[i].key);
            return 0;
        }
    return 1;
}
static ttf_traced_result_t normalized(const ttf_engine_t *engine, const char *input,
                                      const char *expected) {
    ttf_traced_result_t traced = {0};
    ttf_result_t legacy = {0};
    size_t i;
    STATUS(ttf_normalize(engine, input, strlen(input), &legacy), TTF_OK);
    STATUS(ttf_normalize_traced(engine, input, strlen(input), NULL, &traced), TTF_OK);
    EXPECT(same_result(&legacy, &traced.result));
    if (expected)
        EXPECT(traced.result.text && !strcmp(traced.result.text, expected));
    for (i = 0; i < traced.event_count; ++i) {
        const ttf_trace_event_t *event = traced.events + i;
        EXPECT(event->struct_size == sizeof(*event));
        EXPECT(event->input_begin <= event->input_end && event->input_end <= strlen(input));
        EXPECT(event->output_begin <= event->output_end && event->output_end <= traced.result.length);
        EXPECT(event->language && event->resource_language && event->kind && event->key &&
               event->value && event->text && event->detail);
        EXPECT(event->part_begin == SIZE_MAX || event->part_begin <= event->part_end);
    }
    ttf_result_free(&legacy);
    return traced;
}
static const ttf_trace_event_t *find_event(const ttf_traced_result_t *result,
                                          ttf_trace_stage_t stage, const char *kind,
                                          const char *key, size_t *count) {
    size_t i;
    const ttf_trace_event_t *first = NULL;
    *count = 0;
    for (i = 0; i < result->event_count; ++i) {
        const ttf_trace_event_t *event = result->events + i;
        if (event->stage == stage && !strcmp(event->kind, kind) && !strcmp(event->key, key)) {
            if (!first)
                first = event;
            ++*count;
        }
    }
    return first;
}

static void test_consumed_resources(void) {
    ttf_engine_t *engine = engine_new(0);
    ttf_traced_result_t result;
    const ttf_trace_event_t *event;
    size_t count;
    if (!engine)
        return;
    load(engine, TTF_LAYER_DOMAIN, "g2p\ten-US\tba\tB AA\n");
    load(engine, TTF_LAYER_CUSTOMER, "g2p\ten-US\tb\tB IY\n"
                                     "phone_vowel\tko-KR\tAA\t4\n");
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    result = normalized(engine, "baba는 표기입니다.", "버버는 표기입니다.");
    event = find_event(&result, TTF_TRACE_SOURCE_PRONUNCIATION, "g2p", "ba", &count);
    EXPECT(event && count == 2);
    if (event) {
        EXPECT(event->layer_known && event->layer == TTF_LAYER_DOMAIN);
        EXPECT(!strcmp(event->language, "en-US") && !strcmp(event->resource_language, "en-US"));
        EXPECT(!strcmp(event->value, "B AA") && event->declaration_order != SIZE_MAX);
        EXPECT(event->disposition == TTF_TRACE_SELECTED);
    }
    find_event(&result, TTF_TRACE_SOURCE_PRONUNCIATION, "g2p", "b", &count);
    EXPECT(count == 0); /* A losing higher-layer probe must not appear as used. */
    event = find_event(&result, TTF_TRACE_TARGET_PHONETIC, "phone_vowel", "AA", &count);
    EXPECT(event && count == 2);
    if (event) {
        EXPECT(event->layer_known && event->layer == TTF_LAYER_CUSTOMER);
        EXPECT(!strcmp(event->resource_language, "ko-KR"));
        EXPECT(event->phone_index != SIZE_MAX);
    }
    ttf_traced_result_free(&result);
    ttf_engine_destroy(engine);

    /* Actual wildcard provenance and stable first declaration at one layer. */
    engine = engine_new(0);
    if (!engine)
        return;
    load(engine, TTF_LAYER_CUSTOMER, "g2p\t*\tba\tB AA\n"
                                     "g2p\t*\tba\tB IY\n");
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    result = normalized(engine, "baba는 표기입니다.", "바바는 표기입니다.");
    event = find_event(&result, TTF_TRACE_SOURCE_PRONUNCIATION, "g2p", "ba", &count);
    EXPECT(event && count == 2);
    if (event) {
        size_t i, order = event->declaration_order;
        EXPECT(!strcmp(event->resource_language, "*") && !strcmp(event->value, "B AA"));
        for (i = 0; i < result.event_count; ++i)
            if (result.events[i].stage == TTF_TRACE_SOURCE_PRONUNCIATION &&
                !strcmp(result.events[i].kind, "g2p") && !strcmp(result.events[i].key, "ba"))
                EXPECT(result.events[i].declaration_order == order);
    }
    ttf_traced_result_free(&result);
    ttf_engine_destroy(engine);
}

static void test_failed_attempt_and_aliases(void) {
    ttf_engine_t *engine = engine_new(0);
    ttf_traced_result_t result;
    const ttf_trace_event_t *event;
    size_t count, i, rejected = 0, selected = 0;
    if (!engine)
        return;
    load(engine, TTF_LAYER_CUSTOMER, "pronunciation\ten-US\tbaba\tAH UNMAPPED\n");
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    result = normalized(engine, "baba는 표기입니다.", "브아브아는 표기입니다.");
    event = find_event(&result, TTF_TRACE_SOURCE_PRONUNCIATION, "pronunciation", "baba", &count);
    EXPECT(event && count == 1);
    if (event)
        EXPECT(event->disposition == TTF_TRACE_REJECTED && event->layer == TTF_LAYER_CUSTOMER);
    for (i = 0; i < result.event_count; ++i) {
        event = result.events + i;
        if (event->stage == TTF_TRACE_TARGET_PHONETIC) {
            EXPECT(event->disposition == TTF_TRACE_REJECTED);
            ++rejected;
        }
        if (event->stage == TTF_TRACE_TARGET_GRAPHEME) {
            EXPECT(event->disposition == TTF_TRACE_SELECTED);
            ++selected;
        }
    }
    EXPECT(rejected && selected);
    ttf_traced_result_free(&result);
    ttf_engine_destroy(engine);

    engine = engine_new(0);
    if (!engine)
        return;
    load(engine, TTF_LAYER_DOMAIN, "lexicon\tko-KR\ten-US:u.s.\t도메인\n");
    load(engine, TTF_LAYER_CUSTOMER, "lexicon\tko-KR\ten-US:us\t고객\n");
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    result = normalized(engine, "U.S.는 표기입니다.", "고객은 표기입니다.");
    event = find_event(&result, TTF_TRACE_LEXICON, "lexicon", "en-US:us", &count);
    EXPECT(event && count == 1);
    if (event)
        EXPECT(event->layer == TTF_LAYER_CUSTOMER && !strcmp(event->value, "고객"));
    find_event(&result, TTF_TRACE_LEXICON, "lexicon", "en-US:u.s.", &count);
    EXPECT(!count);
    ttf_traced_result_free(&result);
    ttf_engine_destroy(engine);
}

static void test_phrase_and_ordinal_offsets(void) {
    static const char *keys[] = {"en-US:one", "en-US:hundred", "en-US:first"};
    static const size_t begin[] = {0, 4, 12}, end[] = {3, 11, 17};
    const char *input = "101st는 표기입니다. After Effects를 씁니다.";
    ttf_engine_t *engine = engine_new(0);
    ttf_traced_result_t result;
    size_t i, count;
    if (!engine)
        return;
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    result = normalized(engine, input, "원 헌드레드 퍼스트는 표기입니다. 애프터 이펙트를 씁니다.");
    for (i = 0; i < 3; ++i) {
        const ttf_trace_event_t *event = find_event(&result, TTF_TRACE_LEXICON, "lexicon", keys[i], &count);
        EXPECT(event && count == 1);
        if (event) {
            EXPECT(event->input_begin == 0 && event->input_end == 5);
            EXPECT(event->part_begin == begin[i] && event->part_end == end[i]);
            EXPECT(event->output_begin == 0 && event->output_end == strlen("원 헌드레드 퍼스트"));
        }
    }
    {
        const ttf_trace_event_t *event = find_event(&result, TTF_TRACE_LEXICON, "lexicon",
                                                    "en-US:after effects", &count);
        EXPECT(event && count == 1);
        if (event) {
            size_t at = (size_t)(strstr(input, "After Effects") - input);
            EXPECT(event->input_begin == at && event->input_end == at + strlen("After Effects"));
            EXPECT(event->disposition == TTF_TRACE_SELECTED);
        }
    }
    ttf_traced_result_free(&result);
    ttf_engine_destroy(engine);
}

static void test_variants(void) {
    static const char *choices[] = {NULL, "alt", "base"};
    size_t which;
    for (which = 0; which < 3; ++which) {
        ttf_engine_t *engine = engine_new(0);
        ttf_traced_result_t result;
        const ttf_trace_event_t *event;
        size_t count;
        if (!engine)
            continue;
        load(engine, TTF_LAYER_DOMAIN, "pronunciation\ten-US\tabacus\tAH S\n"
                                      "pronunciation_variant\ten-US\tabacus#alt\tB IY\n");
        if (choices[which]) {
            char row[200];
            snprintf(row, sizeof(row), "pronunciation_choice\ten-US\tabacus\t%s\n", choices[which]);
            load(engine, TTF_LAYER_CUSTOMER, row);
        }
        STATUS(ttf_engine_freeze(engine), TTF_OK);
        result = normalized(engine, "abacus는 표기입니다.",
                            which == 1 ? "비는 표기입니다." : "어스는 표기입니다.");
        event = find_event(&result, TTF_TRACE_SOURCE_PRONUNCIATION,
                            which == 1 ? "pronunciation_variant" : "pronunciation",
                            which == 1 ? "abacus#alt" : "abacus", &count);
        EXPECT(event && count == 1);
        if (event)
            EXPECT(event->layer == TTF_LAYER_DOMAIN && event->disposition == TTF_TRACE_SELECTED);
        ttf_traced_result_free(&result);
        ttf_engine_destroy(engine);
    }
    {
        ttf_engine_t *engine = engine_new(0);
        ttf_traced_result_t result;
        size_t count;
        if (!engine)
            return;
        load(engine, TTF_LAYER_DOMAIN, "pronunciation_choice\ten-US\tabacus\tmissing-variant\n");
        load(engine, TTF_LAYER_USER, "pronunciation\ten-US\tabacus\tAH S\n");
        STATUS(ttf_engine_freeze(engine), TTF_OK);
        result = normalized(engine, "abacus는 표기입니다.", "어스는 표기입니다.");
        find_event(&result, TTF_TRACE_SOURCE_PRONUNCIATION, "pronunciation_variant", "abacus#missing-variant", &count);
        EXPECT(count == 0);
        ttf_traced_result_free(&result);
        ttf_engine_destroy(engine);
    }
}

typedef struct {
    ttf_result_t result;
    ttf_trace_event_t *events;
    size_t event_count, callbacks;
    ttf_stream_t *stream;
    int reenter, cancel, failed;
} capture_t;
static char *copy_string(const char *value) {
    size_t length = strlen(value);
    char *copy = (char *)malloc(length + 1);
    if (copy)
        memcpy(copy, value, length + 1);
    return copy;
}
static int capture(const ttf_traced_segment_t *traced, void *userdata) {
    capture_t *out = (capture_t *)userdata;
    const ttf_segment_t *segment = &traced->segment;
    size_t offset = out->result.length, i;
    void *memory;
    ++out->callbacks;
    if (out->reenter) {
        if (ttf_stream_write(out->stream, "x", 1) != TTF_ESTATE ||
            ttf_stream_flush(out->stream) != TTF_ESTATE)
            out->failed = 1;
    }
    if (out->cancel)
        return 1;
    memory = realloc(out->result.text, out->result.length + segment->length + 1);
    if (!memory)
        return out->failed = 1;
    out->result.text = (char *)memory;
    memcpy(out->result.text + offset, segment->text, segment->length);
    out->result.length += segment->length;
    out->result.text[out->result.length] = 0;
    if (segment->span_count) {
        memory = realloc(out->result.spans, (out->result.span_count + segment->span_count) * sizeof(ttf_span_t));
        if (!memory)
            return out->failed = 1;
        out->result.spans = (ttf_span_t *)memory;
        for (i = 0; i < segment->span_count; ++i) {
            ttf_span_t *span = out->result.spans + out->result.span_count++;
            *span = segment->spans[i];
            span->begin += offset;
            span->end += offset;
        }
    }
    if (traced->event_count) {
        memory = realloc(out->events, (out->event_count + traced->event_count) * sizeof(ttf_trace_event_t));
        if (!memory)
            return out->failed = 1;
        out->events = (ttf_trace_event_t *)memory;
        for (i = 0; i < traced->event_count; ++i) {
            ttf_trace_event_t *event = out->events + out->event_count++;
            *event = traced->events[i];
            event->output_begin += offset;
            event->output_end += offset;
#define COPY(field) event->field = copy_string(traced->events[i].field); if (!event->field) out->failed = 1
            COPY(language); COPY(resource_language); COPY(kind); COPY(key);
            COPY(value); COPY(text); COPY(detail);
#undef COPY
        }
    }
    return out->failed;
}
static void capture_free(capture_t *captured) {
    size_t i;
    ttf_result_free(&captured->result);
    for (i = 0; i < captured->event_count; ++i) {
        ttf_trace_event_t *event = captured->events + i;
#define RELEASE(field) free((void *)event->field)
        RELEASE(language); RELEASE(resource_language); RELEASE(kind); RELEASE(key);
        RELEASE(value); RELEASE(text); RELEASE(detail);
#undef RELEASE
    }
    free(captured->events);
    memset(captured, 0, sizeof(*captured));
}
static void stream_compare(const ttf_engine_t *engine, const char *input,
                            const ttf_traced_result_t *batch, size_t first, unsigned seed) {
    capture_t output = {0};
    size_t length = strlen(input), at = 0;
    output.reenter = 1;
    STATUS(ttf_stream_create_traced(engine, NULL, capture, &output, &output.stream), TTF_OK);
    if (!output.stream)
        return;
    while (at < length) {
        size_t chunk;
        if (!seed)
            chunk = at ? length - at : first;
        else {
            seed = seed * 1664525u + 1013904223u;
            chunk = 1 + seed % 13;
        }
        if (chunk > length - at)
            chunk = length - at;
        STATUS(ttf_stream_write(output.stream, input + at, chunk), TTF_OK);
        at += chunk;
    }
    STATUS(ttf_stream_flush(output.stream), TTF_OK);
    STATUS(ttf_stream_flush(output.stream), TTF_OK);
    ttf_stream_destroy(output.stream);
    output.stream = NULL;
    /* Access copies after stream destruction: no callback-owned pointer remains. */
    EXPECT(!output.failed && output.callbacks > 0);
    EXPECT(same_result(&batch->result, &output.result));
    EXPECT(same_events(batch->events, batch->event_count, output.events, output.event_count));
    capture_free(&output);
}
static void test_streaming(void) {
    const char *input = "baba는 표기입니다. 101st는 순서입니다. After Effects를 씁니다. U.S.는 표기입니다.";
    int profile;
    for (profile = 0; profile < 2; ++profile) {
        ttf_engine_t *engine = engine_new(profile);
        ttf_traced_result_t batch;
        size_t split;
        unsigned random;
        if (!engine)
            continue;
        STATUS(ttf_engine_freeze(engine), TTF_OK);
        batch = normalized(engine, input, NULL);
        EXPECT(batch.event_count > 0);
        for (split = 1; split < strlen(input); ++split)
            stream_compare(engine, input, &batch, split, 0);
        for (random = 1; random <= 20; ++random)
            stream_compare(engine, input, &batch, 0, random);
        ttf_traced_result_free(&batch);
        ttf_engine_destroy(engine);
    }
}

static void test_limits_and_arguments(void) {
    const char *input = "baba는 표기입니다. 다음입니다.";
    ttf_engine_t *engine = engine_new(0);
    ttf_trace_options_t options;
    ttf_traced_result_t result = {0}, ordinary;
    capture_t output = {0};
    size_t i;
    if (!engine)
        return;
    ttf_trace_options_init(NULL);
    ttf_trace_options_init(&options);
    EXPECT(options.struct_size == sizeof(options) && options.max_events && options.max_bytes);
    STATUS(ttf_normalize_traced(engine, input, strlen(input), NULL, &result), TTF_ESTATE);
    STATUS(ttf_stream_create_traced(engine, NULL, capture, &output, &output.stream), TTF_ESTATE);
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    STATUS(ttf_normalize_traced(NULL, input, strlen(input), NULL, &result), TTF_EINVAL);
    STATUS(ttf_normalize_traced(engine, NULL, 1, NULL, &result), TTF_EINVAL);
    STATUS(ttf_normalize_traced(engine, input, strlen(input), NULL, NULL), TTF_EINVAL);
    STATUS(ttf_stream_create_traced(engine, NULL, NULL, &output, &output.stream), TTF_EINVAL);
    options.struct_size--;
    STATUS(ttf_normalize_traced(engine, input, strlen(input), &options, &result), TTF_EINVAL);
    STATUS(ttf_stream_create_traced(engine, &options, capture, &output, &output.stream), TTF_EINVAL);
    ttf_trace_options_init(&options);
    options.max_events = 0;
    STATUS(ttf_normalize_traced(engine, input, strlen(input), &options, &result), TTF_EINVAL);
    options.max_events = 100;
    options.max_bytes = 0;
    STATUS(ttf_normalize_traced(engine, input, strlen(input), &options, &result), TTF_EINVAL);
    ordinary = normalized(engine, input, NULL);
    EXPECT(ordinary.event_count > 1);
    for (i = 0; i < 2; ++i) {
        ttf_status_t status;
        ttf_trace_options_init(&options);
        if (i)
            options.max_bytes = 1;
        else
            options.max_events = 1;
        STATUS(ttf_normalize_traced(engine, input, strlen(input), &options, &result), TTF_ELIMIT);
        EXPECT(!result.events && !result.event_count && !result.result.text && !result.result.spans);
        ttf_traced_result_free(&result);
        STATUS(ttf_stream_create_traced(engine, &options, capture, &output, &output.stream), TTF_OK);
        status = ttf_stream_write(output.stream, input, strlen(input));
        if (status == TTF_OK)
            status = ttf_stream_flush(output.stream);
        EXPECT(status == TTF_ELIMIT && output.callbacks == 0);
        STATUS(ttf_stream_write(output.stream, "x", 1), TTF_ELIMIT);
        STATUS(ttf_stream_flush(output.stream), TTF_ELIMIT);
        ttf_stream_destroy(output.stream);
        capture_free(&output);
    }
    output.cancel = 1;
    output.reenter = 1;
    STATUS(ttf_stream_create_traced(engine, NULL, capture, &output, &output.stream), TTF_OK);
    {
        ttf_status_t status = ttf_stream_write(output.stream, input, strlen(input));
        if (status == TTF_OK)
            status = ttf_stream_flush(output.stream);
        EXPECT(status == TTF_ECALLBACK && output.callbacks == 1 && !output.failed);
    }
    STATUS(ttf_stream_write(output.stream, "x", 1), TTF_ECALLBACK);
    STATUS(ttf_stream_flush(output.stream), TTF_ECALLBACK);
    ttf_stream_destroy(output.stream);
    capture_free(&output);
    ttf_traced_result_free(&ordinary);
    ttf_traced_result_free(NULL);
    ttf_engine_destroy(engine);
}

static size_t event_payload(const ttf_trace_event_t *event) {
    return sizeof(*event) + strlen(event->language) + strlen(event->resource_language) +
           strlen(event->kind) + strlen(event->key) + strlen(event->value) +
           strlen(event->text) + strlen(event->detail) + 7;
}
static void test_cumulative_limits(void) {
    const char *sentence = "Apple은 사과입니다.";
    const char *prefix = "Apple은 사과입니다. ";
    ttf_engine_t *engine = engine_new(0);
    ttf_traced_result_t batch, exact = {0};
    ttf_trace_options_t options;
    size_t mode, i, bytes = 0;
    if (!engine)
        return;
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    batch = normalized(engine, sentence, "애플은 사과입니다.");
    EXPECT(batch.event_count > 0);
    for (i = 0; i < batch.event_count; ++i)
        bytes += event_payload(batch.events + i);
    ttf_trace_options_init(&options);
    options.max_events = batch.event_count;
    options.max_bytes = bytes;
    STATUS(ttf_normalize_traced(engine, sentence, strlen(sentence), &options, &exact), TTF_OK);
    EXPECT(same_result(&batch.result, &exact.result));
    EXPECT(same_events(batch.events, batch.event_count, exact.events, exact.event_count));
    ttf_traced_result_free(&exact);
    for (mode = 0; mode < 2; ++mode) {
        capture_t output = {0};
        ttf_status_t status;
        ttf_trace_options_init(&options);
        if (mode)
            options.max_bytes = bytes;
        else
            options.max_events = batch.event_count;
        STATUS(ttf_stream_create_traced(engine, &options, capture, &output, &output.stream), TTF_OK);
        STATUS(ttf_stream_write(output.stream, prefix, strlen(prefix)), TTF_OK);
        EXPECT(output.callbacks == 1 && output.event_count == batch.event_count);
        status = ttf_stream_write(output.stream, prefix, strlen(prefix));
        if (status == TTF_OK)
            status = ttf_stream_flush(output.stream);
        EXPECT(status == TTF_ELIMIT);
        EXPECT(output.callbacks == 1 && output.event_count == batch.event_count);
        EXPECT(output.result.text && !strncmp(output.result.text, batch.result.text, batch.result.length));
        STATUS(ttf_stream_write(output.stream, "x", 1), TTF_ELIMIT);
        STATUS(ttf_stream_flush(output.stream), TTF_ELIMIT);
        ttf_stream_destroy(output.stream);
        capture_free(&output);
    }
    ttf_traced_result_free(&batch);
    ttf_engine_destroy(engine);
}

/* Descriptor layout is the original pack ABI; tracing is optional instrumentation. */
static int third_script(ttf_script_t script) { return script == TTF_SCRIPT_LATIN ? 100 : 0; }
static size_t third_classify(const ttf_pack_input_t *input, ttf_semantic_t *semantic) {
    const ttf_token_t *token = input->tokens + input->index;
    if (token->script != TTF_SCRIPT_LATIN)
        return 0;
    semantic->type = TTF_SEM_WORD;
    semantic->lexical_class = TTF_LEX_WORD;
    semantic->input_end = token->end;
    snprintf(semantic->source_lang, sizeof(semantic->source_lang), "xx-Trace");
    snprintf(semantic->rule, sizeof(semantic->rule), "test.third-language");
    return 1;
}
static ttf_status_t third_verbalize(const ttf_engine_t *engine, const ttf_semantic_t *semantic,
                                    const char *surface, size_t length, ttf_buffer_t *out) {
    (void)engine; (void)semantic;
    return ttf_buffer_append(out, surface, length);
}
static ttf_status_t third_pronounce(const ttf_engine_t *engine, const char *word, size_t length,
                                    ttf_lexical_class_t lexical, ttf_pronunciation_t *out) {
    ttf_trace_note_t note;
    char local_value[] = "temporary-note";
    ttf_status_t status;
    (void)lexical;
    memset(out, 0, sizeof(*out));
    snprintf(out->alphabet, sizeof(out->alphabet), "ttf-phones-v1");
    snprintf(out->phones, sizeof(out->phones), "B IY");
    snprintf(out->provenance, sizeof(out->provenance), "old-pack-generated");
    memset(&note, 0, sizeof(note));
    note.struct_size = sizeof(note);
    note.disposition = TTF_TRACE_SELECTED;
    note.phone_index = SIZE_MAX;
    note.language = "xx-Trace";
    note.kind = "custom-generated";
    note.key = "test";
    note.value = local_value;
    note.detail = "third-pack-emitter";
    if (length == 6 && !memcmp(word, "badutf", 6)) {
        note.value = "invalid\xff";
        (void)ttf_trace_record(engine, &note); /* Ignored callback error must remain sticky. */
        return TTF_OK;
    }
    if (length == 9 && !memcmp(word, "badlimits", 9)) {
        (void)ttf_trace_record(engine, &note);
        (void)ttf_trace_record(engine, &note);
        note.struct_size = 0;
        (void)ttf_trace_record(engine, &note); /* First ELIMIT wins over later EINVAL. */
        return TTF_OK;
    }
    status = ttf_trace_record(engine, &note);
    memset(local_value, 'x', sizeof(local_value) - 1); /* The emitter must copy strings. */
    return status;
}
static const ttf_language_pack_t third_pack = {
    sizeof(ttf_language_pack_t), TTF_ABI_VERSION, "xx-Trace", NULL,
    third_script, NULL, third_classify, third_verbalize, third_pronounce, NULL, NULL
};
static void test_third_language(void) {
    ttf_engine_t *engine = engine_new(0);
    ttf_traced_result_t result;
    const ttf_trace_event_t *event;
    size_t count;
    if (!engine)
        return;
    STATUS(ttf_engine_register_pack(engine, &third_pack), TTF_OK);
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    result = normalized(engine, "customword는 표기입니다.", "비는 표기입니다.");
    event = find_event(&result, TTF_TRACE_SOURCE_PRONUNCIATION, "custom-generated", "test", &count);
    EXPECT(event && count == 1);
    if (event) {
        EXPECT(!strcmp(event->value, "temporary-note"));
        EXPECT(!event->layer_known && event->declaration_order == SIZE_MAX);
        EXPECT(!strcmp(event->language, "xx-Trace"));
    }
    ttf_traced_result_free(&result);
    {
        ttf_trace_options_t options;
        memset(&result, 0, sizeof(result));
        STATUS(ttf_normalize_traced(engine, "badutf는 표기입니다.", strlen("badutf는 표기입니다."),
                                     NULL, &result), TTF_EUTF8);
        EXPECT(!result.events && !result.result.text);
        ttf_traced_result_free(&result);
        ttf_trace_options_init(&options);
        options.max_events = 1;
        STATUS(ttf_normalize_traced(engine, "badlimits는 표기입니다.", strlen("badlimits는 표기입니다."),
                                     &options, &result), TTF_ELIMIT);
        EXPECT(!result.events && !result.result.text);
        ttf_traced_result_free(&result);
    }
    ttf_engine_destroy(engine);
}

#ifdef TTF_TEST_HAVE_THREADS
typedef struct {
    const ttf_engine_t *engine;
    const char *input;
    const ttf_traced_result_t *expected;
    int failed;
} worker_t;
static void *worker(void *userdata) {
    worker_t *work = (worker_t *)userdata;
    unsigned i;
    for (i = 0; i < 25; ++i) {
        ttf_traced_result_t result = {0};
        if (ttf_normalize_traced(work->engine, work->input, strlen(work->input), NULL, &result) != TTF_OK ||
            !same_result(&work->expected->result, &result.result) ||
            !same_events(work->expected->events, work->expected->event_count, result.events, result.event_count))
            work->failed = 1;
        ttf_traced_result_free(&result);
    }
    return NULL;
}
static void test_concurrent(void) {
    static const char *inputs[] = {"baba는 표기입니다.", "101st는 순서입니다.",
                                  "After Effects를 씁니다.", "Apple은 사과입니다."};
    enum { COUNT = 4 };
    pthread_t threads[COUNT];
    worker_t workers[COUNT];
    ttf_traced_result_t expected[COUNT];
    ttf_engine_t *engine = engine_new(0);
    size_t i, started = 0;
    if (!engine)
        return;
    STATUS(ttf_engine_freeze(engine), TTF_OK);
    for (i = 0; i < COUNT; ++i) {
        expected[i] = normalized(engine, inputs[i], NULL);
        workers[i].engine = engine;
        workers[i].input = inputs[i];
        workers[i].expected = expected + i;
        workers[i].failed = 0;
    }
    for (i = 0; i < COUNT; ++i) {
        int status = pthread_create(threads + i, NULL, worker, workers + i);
        EXPECT(status == 0);
        if (status)
            break;
        ++started;
    }
    for (i = 0; i < started; ++i) {
        EXPECT(pthread_join(threads[i], NULL) == 0);
        EXPECT(!workers[i].failed);
    }
    for (i = 0; i < COUNT; ++i)
        ttf_traced_result_free(expected + i);
    ttf_engine_destroy(engine);
}
#endif

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s RESOURCE_ROOT SCRATCH_DIRECTORY\n", argv[0]);
        return 2;
    }
    resources = argv[1];
    scratch = argv[2];
    test_consumed_resources();
    test_failed_attempt_and_aliases();
    test_phrase_and_ordinal_offsets();
    test_variants();
    test_streaming();
    test_limits_and_arguments();
    test_cumulative_limits();
    test_third_language();
#ifdef TTF_TEST_HAVE_THREADS
    test_concurrent();
#endif
    printf("detailed pronunciation trace: %zu checks, %zu failures\n", checks, failures);
    return failures ? 1 : 0;
}
