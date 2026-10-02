#include "tts_text_frontend/ttf.h"
#include "tts_text_frontend/ttf_frontend_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)
typedef struct { const char *expected; size_t used, committed, calls; } capture_t;
static int collect(const ttf_segment_t *s, void *user) {
    capture_t *c = user;
    size_t n = strlen(c->expected);
    if (c->used > n || s->length > n - c->used ||
        memcmp(c->expected + c->used, s->text, s->length) ||
        s->input_begin != c->committed || s->input_end < s->input_begin)
        return 1;
    c->used += s->length;
    c->committed = s->input_end;
    ++c->calls;
    return 0;
}
static int check(const ttf_engine_t *e, const char *source, const char *expected) {
    size_t n = strlen(source), cut, pos;
    ttf_result_t r;
    CHECK(ttf_normalize(e, source, n, &r) == TTF_OK);
    CHECK(!strcmp(r.text, expected));
    for (pos = 0; pos < r.span_count; ++pos) {
        const ttf_span_t *s = &r.spans[pos];
        CHECK(s->input_begin <= s->input_end && s->input_end <= n);
        CHECK(s->input_begin == n || ((unsigned char)source[s->input_begin] & 0xc0) != 0x80);
        CHECK(s->input_end == n || ((unsigned char)source[s->input_end] & 0xc0) != 0x80);
    }
    ttf_result_free(&r);
    for (cut = 1; cut < n; ++cut) {
        ttf_stream_t *stream;
        capture_t c = {expected, 0, 0, 0};
        CHECK(ttf_stream_create(e, collect, &c, &stream) == TTF_OK);
        CHECK(ttf_stream_write(stream, source, cut) == TTF_OK);
        CHECK(ttf_stream_write(stream, source + cut, n - cut) == TTF_OK);
        CHECK(ttf_stream_flush(stream) == TTF_OK);
        CHECK(c.used == strlen(expected) && c.committed == n && c.calls);
        ttf_stream_destroy(stream);
    }
    return 0;
}
int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_frontend_options_t options;
    ttf_engine_t *e;
    ttf_stream_t *stream;
    ttf_token_t *tokens;
    size_t count, i;
    char *path;
    capture_t c;
    const char *cases[][2] = {
        {"１２３원", "백이십삼 원"}, {"３개 /tmp/１２３.txt", "세 개 /tmp/１２３.txt"},
        {"１２３원 [[１２３]]{@preserve}", "백이십삼 원 １２３"},
        {"３개 `１２３원`", "세 개 `１２３원`"},
        {"０１０-１２３４-５６７８", "공일공 일이삼사 오육칠팔"},
        {"１２，５００원", "１２，５００원"}, {"₩１２，５００", "₩１２，５００"},
        {"$12，500", "$12，500"}, {"－１２３", "－１２３"},
        {"１２．５０%", "１２．５０%"}, {"０９：０５", "０９：０５"},
        {"September 17, ２０２６．５", "September 17, ２０２６．５"},
        /* 2026-10-02: a trailing fullwidth percent sign reads like % (was kept). */
        {"１２３％", "백이십삼 퍼센트"}
    };
    CHECK(argc == 2);
    CHECK(ttf_tokenize("1２3가", strlen("1２3가"), &tokens, &count) == TTF_OK);
    CHECK(count == 2 && tokens[0].script == TTF_SCRIPT_DIGIT && tokens[0].end == 5);
    ttf_tokens_free(tokens);
    ttf_config_init(&config);
    config.resource_root = argv[1];
    config.first_segment_target = 1; config.normal_segment_target = 3;
    config.max_segment_length = 12;
    CHECK(ttf_engine_create(&config, &e) == TTF_OK);
    ttf_frontend_options_init(&options);
    options.output_mode = TTF_OUTPUT_SPEECH; options.input_format = TTF_INPUT_ANNOTATED;
    CHECK(ttf_engine_set_frontend_options(e, &options) == TTF_OK);
    CHECK(ttf_engine_freeze(e) == TTF_OK);
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
        CHECK(check(e, cases[i][0], cases[i][1]) == 0);
    /* A long protected component must retain every original byte while input
       UTF-8 characters are split. Pending protection stays incremental. */
    path = malloc(9010); CHECK(path);
    memcpy(path, "/tmp/", 5);
    for (i = 0; i < 3000; ++i) memcpy(path + 5 + i * 3, "１", 3);
    memcpy(path + 9005, ".txt", 5);
    c.expected = path; c.used = c.committed = c.calls = 0;
    CHECK(ttf_stream_create(e, collect, &c, &stream) == TTF_OK);
    for (i = 0; i < 9009; ++i) CHECK(ttf_stream_write(stream, path + i, 1) == TTF_OK);
    CHECK(ttf_stream_flush(stream) == TTF_OK && c.used == 9009 && c.committed == 9009);
    ttf_stream_destroy(stream); free(path);
    ttf_engine_destroy(e);
    puts("numeric width: original UTF-8 bounds, typed/code/path protection, punctuation guards and incremental path passed");
    return 0;
}
