#include "../../src/cjk/cjk_reading.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)
typedef struct { char text[8192]; size_t used, calls; } capture_t;
static int collect(const ttf_segment_t *seg, void *user) {
    capture_t *c = user;
    if (seg->length >= sizeof(c->text) - c->used) return 1;
    memcpy(c->text + c->used, seg->text, seg->length);
    c->used += seg->length; c->text[c->used] = 0; ++c->calls;
    return 0;
}
static size_t protected(const ttf_engine_t *e, const char *text, size_t length) {
    ttf_token_t *tokens = NULL;
    size_t count = 0, end = 0;
    if (ttf_tokenize(text, length, &tokens, &count) == TTF_OK && count)
        end = ttf_phrase_protect_until(e, text, length, tokens, 0);
    ttf_tokens_free(tokens);
    return end;
}
int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_frontend_options_t options;
    ttf_engine_t *e = NULL;
    ttf_stream_t *stream = NULL;
    capture_t capture = {{0}, 0, 0};
    size_t mode, i, split;
    const char *text = "男女老少音樂";
    const char *cases[][2] = {
        {"張勳", "장훈"}, {"男女老少", "남녀노소"}, {"ソウル", "서울"},
        {"うどん", "우동"}, {"東京とうきょう", "동경도쿄"}, {"ボウル", "보우루"},
        {"ﾄｳｷｮｳ", "도우쿄우"}, {"女\xEF\xB8\x8F子", "여자"},
        {"가𠮷나", "가𠮷나"}, {"っん", "っん"}
    };
    CHECK(argc == 2);
    ttf_config_init(&config); config.resource_root = argv[1];
    config.first_segment_target = 1; config.normal_segment_target = 3;
    config.max_segment_length = 12;
    for (mode = 0; mode < 4; ++mode) {
        ttf_frontend_options_init(&options);
        options.unreadable_policy = TTF_UNREADABLE_PRESERVE; /* 가𠮷나 and っん stay as written */
        options.hanja = (mode & 1) ? TTF_HANJA_KOREAN : TTF_HANJA_PASSTHROUGH;
        options.kana = (mode & 2) ? TTF_KANA_KOREAN : TTF_KANA_PASSTHROUGH;
        CHECK(ttf_engine_create(&config, &e) == TTF_OK);
        CHECK(ttf_engine_set_frontend_options(e, &options) == TTF_OK);
        CHECK(ttf_resource_set(e, "cjk_word", "ko-KR", text, "모두음악", TTF_LAYER_CUSTOMER) == TTF_OK);
        CHECK(ttf_engine_freeze(e) == TTF_OK);
        CHECK(e->cjk->max_length == 6);
        CHECK(protected(e, "男女老少音樂女", 21) == ((mode & 1) ? 18u : 0u));
        CHECK(protected(e, "男女 老少", 13) == ((mode & 1) ? 6u : 0u));
        CHECK(protected(e, "音樂うどん", 15) == ((mode & 1) ? ((mode & 2) ? 15u : 6u) : 0u));
        CHECK(protected(e, "女\xEF\xB8\x8F子", 9) == ((mode & 1) ? 9u : 0u));
        if (mode == 3) {
            for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
                size_t n = strlen(cases[i][0]);
                for (split = 1; split < n; ++split) {
                    memset(&capture, 0, sizeof(capture));
                    CHECK(ttf_stream_create(e, collect, &capture, &stream) == TTF_OK);
                    CHECK(ttf_stream_write(stream, cases[i][0], split) == TTF_OK);
                    CHECK(capture.calls == 0); /* incomplete run, including UTF-8 bytes */
                    CHECK(ttf_stream_write(stream, cases[i][0] + split, n - split) == TTF_OK);
                    CHECK(capture.calls == 0); /* no safe boundary yet */
                    CHECK(ttf_stream_flush(stream) == TTF_OK);
                    CHECK(!strcmp(capture.text, cases[i][1]));
                    ttf_stream_destroy(stream);
                }
            }
            memset(&capture, 0, sizeof(capture));
            CHECK(ttf_stream_create(e, collect, &capture, &stream) == TTF_OK);
            CHECK(ttf_stream_write(stream, text, 9) == TTF_OK);
            CHECK(capture.calls == 0);
            CHECK(ttf_stream_write(stream, text + 9, 9) == TTF_OK);
            CHECK(capture.calls == 0);
            CHECK(ttf_stream_write(stream, " 다음", strlen(" 다음")) == TTF_OK);
            CHECK(capture.calls > 0 && !strcmp(capture.text, "모두음악 "));
            CHECK(ttf_stream_flush(stream) == TTF_OK);
            CHECK(!strcmp(capture.text, "모두음악 다음"));
            ttf_stream_destroy(stream);
        }
        ttf_engine_destroy(e);
    }
    puts("CJK bounded phrase protection, 4 option combinations, byte splits and pre-flush callback passed");
    return 0;
}
