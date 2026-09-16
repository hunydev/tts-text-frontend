#include "tts_text_frontend/ttf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)

typedef struct {
    const char *expected;
    size_t length, used, calls, literals;
} observed_t;

static int collect(const ttf_segment_t *segment, void *userdata) {
    observed_t *observed = (observed_t *)userdata;
    size_t i;
    if (segment->length > observed->length - observed->used ||
        memcmp(segment->text, observed->expected + observed->used, segment->length))
        return 1;
    observed->used += segment->length;
    ++observed->calls;
    for (i = 0; i < segment->span_count; ++i) {
        const ttf_span_t *span = &segment->spans[i];
        if (strncmp(span->semantic.rule, "protected.", 10))
            continue;
        if (span->semantic.type != TTF_SEM_TEXT || strcmp(span->semantic.source_lang, "und") ||
            strcmp(span->semantic.context_lang, "und") || strcmp(span->semantic.render_lang, "und"))
            return 1;
        ++observed->literals;
    }
    return 0;
}

/* No elapsed-time assertion: the test checks observable behavior at each write.
   Long ordinary body bytes may wait, but a completed literal followed by a safe
   boundary must produce its callback before flush. */
static int long_literal(const ttf_engine_t *engine, const char *opening, const char *closing) {
    const size_t body = 10000;
    size_t head = strlen(opening), tail = strlen(closing), n = head + body + tail, i;
    char *text = (char *)malloc(n + 2);
    observed_t observed;
    ttf_stream_t *stream = NULL;
    CHECK(text != NULL);
    memcpy(text, opening, head);
    memset(text + head, 'a', body);
    memcpy(text + head + body, closing, tail);
    text[n] = ' ';
    text[n + 1] = '\0';
    memset(&observed, 0, sizeof(observed));
    observed.expected = text;
    observed.length = n + 1;
    CHECK(ttf_stream_create(engine, collect, &observed, &stream) == TTF_OK);
    for (i = 0; i < n; ++i) {
        CHECK(ttf_stream_write(stream, text + i, 1) == TTF_OK);
        CHECK(observed.calls == 0);
    }
    CHECK(ttf_stream_write(stream, text + n, 1) == TTF_OK);
    CHECK(observed.calls == 1 && observed.literals == 1 && observed.used == n + 1);
    CHECK(ttf_stream_flush(stream) == TTF_OK);
    CHECK(ttf_stream_flush(stream) == TTF_OK && observed.calls == 1);
    ttf_stream_destroy(stream);
    free(text);
    return 0;
}

static int utf8_waiting(const ttf_engine_t *engine) {
    const char *valid = "<code>한글</code> ";
    const unsigned char incomplete[] = {0xf0};
    const unsigned char invalid[] = {0xff};
    const char nul = '\0';
    observed_t observed = {valid, 0, 0, 0, 0};
    ttf_stream_t *stream = NULL;
    size_t i;
    observed.length = strlen(valid);
    CHECK(ttf_stream_create(engine, collect, &observed, &stream) == TTF_OK);
    for (i = 0; i < observed.length; ++i)
        CHECK(ttf_stream_write(stream, valid + i, 1) == TTF_OK);
    CHECK(observed.calls == 1 && observed.literals == 1 && observed.used == observed.length);
    CHECK(ttf_stream_flush(stream) == TTF_OK);
    ttf_stream_destroy(stream);

    observed.calls = observed.used = observed.literals = 0;
    CHECK(ttf_stream_create(engine, collect, &observed, &stream) == TTF_OK);
    CHECK(ttf_stream_write(stream, "<code>abc", 9) == TTF_OK);
    CHECK(ttf_stream_write(stream, (const char *)incomplete, sizeof(incomplete)) == TTF_OK);
    CHECK(ttf_stream_write(stream, "X", 1) == TTF_EUTF8);
    CHECK(observed.calls == 0 && ttf_stream_flush(stream) == TTF_EUTF8);
    ttf_stream_destroy(stream);

    CHECK(ttf_stream_create(engine, collect, &observed, &stream) == TTF_OK);
    CHECK(ttf_stream_write(stream, "<!--abc", 7) == TTF_OK);
    CHECK(ttf_stream_write(stream, (const char *)invalid, sizeof(invalid)) == TTF_EUTF8);
    CHECK(observed.calls == 0);
    ttf_stream_destroy(stream);

    CHECK(ttf_stream_create(engine, collect, &observed, &stream) == TTF_OK);
    CHECK(ttf_stream_write(stream, "`abc", 4) == TTF_OK);
    CHECK(ttf_stream_write(stream, &nul, 1) == TTF_EUTF8 && observed.calls == 0);
    ttf_stream_destroy(stream);

    CHECK(ttf_stream_create(engine, collect, &observed, &stream) == TTF_OK);
    CHECK(ttf_stream_write(stream, "<code>abc", 9) == TTF_OK);
    CHECK(ttf_stream_write(stream, (const char *)incomplete, sizeof(incomplete)) == TTF_OK);
    CHECK(ttf_stream_flush(stream) == TTF_EUTF8 && observed.calls == 0);
    ttf_stream_destroy(stream);
    return 0;
}

static int undecided_path_prefixes(const ttf_engine_t *engine) {
    const char *prefixes[] = {"'.", "'/", "'C", "'C:", "'..", "'~"};
    const char *continuations[] = {"x ", "/server ", "at ", "oops ", "word ", "abc "};
    size_t c;
    for (c = 0; c < sizeof(prefixes) / sizeof(prefixes[0]); ++c) {
        char input[1024];
        size_t i, prefix = strlen(prefixes[c]);
        ttf_result_t batch;
        ttf_stream_t *stream = NULL;
        observed_t observed;
        strcpy(input, prefixes[c]);
        strcat(input, continuations[c]);
        for (i = 0; i < 24; ++i)
            strcat(input, "3개입니다. ");
        CHECK(ttf_normalize(engine, input, strlen(input), &batch) == TTF_OK);
        memset(&observed, 0, sizeof(observed));
        observed.expected = batch.text;
        observed.length = batch.length;
        CHECK(ttf_stream_create(engine, collect, &observed, &stream) == TTF_OK);
        CHECK(ttf_stream_write(stream, input, prefix) == TTF_OK);
        for (i = prefix; i < strlen(input); ++i)
            CHECK(ttf_stream_write(stream, input + i, 1) == TTF_OK);
        CHECK(ttf_stream_flush(stream) == TTF_OK);
        CHECK(observed.used == batch.length && observed.literals == 0 && observed.calls > 1);
        ttf_stream_destroy(stream);
        ttf_result_free(&batch);
    }
    return 0;
}

int main(int argc, char **argv) {
    ttf_engine_t *engine = NULL;
    ttf_stream_t *stream = NULL;
    ttf_config_t config;
    observed_t observed = {"", 0, 0, 0, 0};
    size_t i;
    CHECK(argc == 2);
    ttf_config_init(&config);
    config.resource_root = argv[1];
    config.first_segment_target = config.normal_segment_target = 1;
    config.max_segment_length = 8;
    CHECK(ttf_engine_create(&config, &engine) == TTF_OK);
    CHECK(ttf_engine_freeze(engine) == TTF_OK);
    CHECK(long_literal(engine, "<code>", "</code>") == 0);
    CHECK(long_literal(engine, "`", "`") == 0);
    CHECK(long_literal(engine, "```", "```") == 0);
    CHECK(long_literal(engine, "<!--", "-->") == 0);
    CHECK(long_literal(engine, "<span data-note=\"", "\">") == 0);
    CHECK(long_literal(engine, "\"/tmp/", "\"") == 0);
    CHECK(long_literal(engine, "/tmp/", ".txt") == 0);
    CHECK(utf8_waiting(engine) == 0);
    ttf_engine_destroy(engine);

    config.max_pending_bytes = 64;
    CHECK(ttf_engine_create(&config, &engine) == TTF_OK);
    CHECK(ttf_engine_freeze(engine) == TTF_OK);
    CHECK(undecided_path_prefixes(engine) == 0);
    CHECK(ttf_stream_create(engine, collect, &observed, &stream) == TTF_OK);
    CHECK(ttf_stream_write(stream, "<code>", 6) == TTF_OK);
    for (i = 6; i < 64; ++i)
        CHECK(ttf_stream_write(stream, "a", 1) == TTF_OK);
    CHECK(ttf_stream_write(stream, "a", 1) == TTF_ELIMIT);
    CHECK(ttf_stream_flush(stream) == TTF_ELIMIT && observed.calls == 0);
    ttf_stream_destroy(stream);
    ttf_engine_destroy(engine);
    puts("7 long literals, 6 undecided prefixes, early callbacks, UTF-8 errors and cap passed");
    return 0;
}
