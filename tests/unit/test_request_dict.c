/* Request dictionaries (2026-10-26): exact and lexicon rows of one request above every engine
   layer, the frozen engine unchanged, streams, errors, and concurrent requests each with
   their own dictionary. */
#include "tts_text_frontend/ttf.h"
#include "tts_text_frontend/ttf_frontend_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TTF_TEST_HAVE_THREADS
#include <pthread.h>
#endif

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)
static char out[4096];
static const char *run(const ttf_engine_t *e, const ttf_dict_t *d, const char *text) {
    ttf_result_t r;
    out[0] = 0;
    if (ttf_normalize_with(e, d, text, strlen(text), &r) != TTF_OK)
        return "(error)";
    snprintf(out, sizeof(out), "%s", r.text);
    ttf_result_free(&r);
    return out;
}
static int same(const ttf_engine_t *e, const ttf_dict_t *d, const char *text, const char *want) {
    const char *got = run(e, d, text);
    if (strcmp(got, want)) {
        fprintf(stderr, "%s\n  got:  %s\n  want: %s\n", text, got, want);
        return 0;
    }
    return 1;
}
static int collect(const ttf_segment_t *segment, void *user) {
    strncat((char *)user, segment->text, segment->length);
    return 0;
}
/* The output of a stream fed one byte at a time. */
static const char *bytewise(const ttf_engine_t *e, const ttf_dict_t *d, const char *text) {
    ttf_stream_t *s = NULL;
    size_t i;
    out[0] = 0;
    if (ttf_stream_create(e, collect, out, &s) != TTF_OK || ttf_stream_set_dict(s, d) != TTF_OK)
        return "(error)";
    for (i = 0; text[i]; ++i)
        if (ttf_stream_write(s, text + i, 1) != TTF_OK)
            return "(error)";
    if (ttf_stream_flush(s) != TTF_OK)
        return "(error)";
    ttf_stream_destroy(s);
    return out;
}
static ttf_engine_t *engine(const char *resources, const char *customer) {
    ttf_config_t config;
    ttf_frontend_options_t options;
    ttf_engine_t *e = NULL;
    ttf_config_init(&config);
    config.resource_root = resources;
    config.default_language = "ko-KR";
    ttf_frontend_options_init(&options);
    options.output_mode = TTF_OUTPUT_SPEECH;
    if (ttf_engine_create(&config, &e) != TTF_OK || ttf_engine_set_frontend_options(e, &options) != TTF_OK ||
        (customer && ttf_resource_load(e, customer, TTF_LAYER_USER) != TTF_OK) || ttf_engine_freeze(e) != TTF_OK) {
        ttf_engine_destroy(e);
        return NULL;
    }
    return e;
}
static int write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    fputs(text, f);
    return fclose(f) == 0;
}
#ifdef TTF_TEST_HAVE_THREADS
typedef struct {
    const ttf_engine_t *engine;
    ttf_dict_t *dict;
    char want[64];
    int failed;
} worker_t;
static void *work(void *arg) {
    worker_t *w = (worker_t *)arg;
    int i;
    for (i = 0; i < 300; ++i) {
        ttf_result_t r;
        const char *text = i % 2 ? "2NE1의 신곡" : "Apple은 사과입니다.";
        if (ttf_normalize_with(w->engine, i % 3 ? w->dict : NULL, text, strlen(text), &r) != TTF_OK) {
            w->failed = 1;
            continue;
        }
        if (strcmp(r.text, i % 2 ? (i % 3 ? w->want : "투애니원의 신곡") : "애플은 사과입니다."))
            w->failed = 1;
        ttf_result_free(&r);
    }
    return NULL;
}
#endif

int main(int argc, char **argv) {
    const char *user = "/tmp/ttf_request_dict_user.tsv", *file = "/tmp/ttf_request_dict.tsv";
    char long_key[80], long_text[128];
    ttf_engine_t *e, *plain;
    ttf_dict_t *d = NULL;
    ttf_result_t r;
    if (argc != 2)
        return 2;
    /* the engine's user layer; `*` first, so file order would pick it (2026-10-26) */
    CHECK(write_file(user, "exact\t*\t알쓰\t엔진 별표\nexact\tko-KR\t알쓰\t엔진 사용자\n"
                           "exact\tko-KR\t1279455\t일이칠구사오오\n"));
    CHECK((e = engine(argv[1], user)) != NULL);
    CHECK((plain = engine(argv[1], NULL)) != NULL);
    CHECK(!strcmp(ttf_layer_name(TTF_LAYER_REQUEST), "request"));
    CHECK(ttf_resource_load(plain, user, TTF_LAYER_REQUEST) == TTF_EINVAL);

    /* a language's rule beats a `*` rule of the same layer */
    CHECK(same(e, NULL, "알쓰 모임", "엔진 사용자 모임"));
    /* an entry inside a grammar span is read as written (제 N호, 2026-10-26) */
    CHECK(same(e, NULL, "전자관보 제 1279455호에", "전자관보 제 일이칠구사오오호에"));
    CHECK(same(plain, NULL, "전자관보 제 1279455호에", "전자관보 제백이십칠만 구천사백오십오 호에"));
    CHECK(ttf_dict_create(&d) == TTF_OK && ttf_dict_size(d) == 0);
    CHECK(same(e, d, "알쓰 모임", "엔진 사용자 모임"));

    /* exact rows: above the engine's user layer; the engine itself is unchanged */
    CHECK(ttf_dict_add_exact(d, "ko-KR", "알쓰", "요청값") == TTF_OK);
    CHECK(ttf_dict_add_exact(d, "ko-KR", "2NE1", "투에니원") == TTF_OK);
    CHECK(ttf_dict_add_exact(d, "*", "2NE1", "two N E one") == TTF_OK);
    CHECK(ttf_dict_add_exact(d, "*", "🍔👌", "햄버거 좋아") == TTF_OK);
    CHECK(ttf_dict_add_exact(d, "ko-KR", "알콜 쓰레기", "알쓰") == TTF_OK);
    CHECK(ttf_dict_size(d) == 5);
    CHECK(same(e, d, "알쓰 모임", "요청값 모임"));
    CHECK(same(e, d, "2NE1의 신곡", "투에니원의 신곡"));
    CHECK(same(e, d, "2NE1 released a new song.", "two N E one released a new song."));
    CHECK(same(e, d, "점심은 🍔👌", "점심은 햄버거 좋아"));
    CHECK(same(e, d, "그 친구는 알콜 쓰레기 맞아.", "그 친구는 알쓰 맞아."));
    /* as written only: an attached particle or another spacing is not the key */
    CHECK(same(e, d, "알쓰는 빠질게", "알쓰는 빠질게"));
    CHECK(same(e, d, "알콜  쓰레기 맞아.", "알콜  쓰레기 맞아."));
    CHECK(same(e, NULL, "2NE1의 신곡", "투애니원의 신곡"));
    CHECK(same(plain, d, "2NE1의 신곡", "투에니원의 신곡"));
    /* replacing and removing a row; the engine's row shows again */
    CHECK(ttf_dict_add_exact(d, "ko-KR", "알쓰", "다른 값") == TTF_OK && ttf_dict_size(d) == 5);
    CHECK(same(e, d, "알쓰 모임", "다른 값 모임"));
    CHECK(ttf_dict_remove_exact(d, "ko-KR", "알쓰") == TTF_OK && ttf_dict_size(d) == 4);
    CHECK(same(e, d, "알쓰 모임", "엔진 사용자 모임"));
    CHECK(ttf_dict_remove_exact(d, "ko-KR", "없는 키") == TTF_OK && ttf_dict_size(d) == 4);
    CHECK(ttf_dict_add_exact(d, "ko-KR", "1231987", "일이삼일구팔칠") == TTF_OK);
    CHECK(same(plain, d, "법원 제 1231987호 법정", "법원 제 일이삼일구팔칠호 법정"));
    CHECK(same(plain, d, "제 50조와 제 1231987호", "제오십 조와 제 일이삼일구팔칠호"));
    CHECK(ttf_dict_remove_exact(d, "ko-KR", "1231987") == TTF_OK);
    /* a key of 63 bytes or more */
    memset(long_key, 'q', 70);
    long_key[70] = 0;
    snprintf(long_text, sizeof(long_text), "코드 %s 끝", long_key);
    CHECK(ttf_dict_add_exact(d, "ko-KR", long_key, "긴 키") == TTF_OK);
    CHECK(same(e, d, long_text, "코드 긴 키 끝"));

    /* lexicon rows: one word and a phrase, in a request and in a stream fed byte by byte */
    CHECK(ttf_dict_add_lexicon(d, "en-US", "ko-KR", "Qorvex", "코르벡스") == TTF_OK);
    CHECK(ttf_dict_add_lexicon(d, "en-US", "ko-KR", "Qor Vex Labs", "코르벡스 랩스") == TTF_OK);
    CHECK(same(e, d, "Qorvex 신제품과 Qor Vex Labs 발표", "코르벡스 신제품과 코르벡스 랩스 발표"));
    CHECK(!strcmp(bytewise(e, d, "Qorvex 신제품과 Qor Vex Labs 발표"), "코르벡스 신제품과 코르벡스 랩스 발표"));
    CHECK(ttf_dict_remove_lexicon(d, "en-US", "ko-KR", "QORVEX") == TTF_OK);
    CHECK(strcmp(run(e, d, "Qorvex 신제품"), "코르벡스 신제품"));
    /* the span reports the request layer */
    CHECK(ttf_normalize_with(e, d, "2NE1의 신곡", strlen("2NE1의 신곡"), &r) == TTF_OK);
    CHECK(r.span_count && r.spans[0].semantic.layer == TTF_LAYER_REQUEST);
    ttf_result_free(&r);

    /* a stream takes its dictionary before the first write only */
    {
        ttf_stream_t *s = NULL;
        CHECK(ttf_stream_create(e, collect, out, &s) == TTF_OK);
        out[0] = 0;
        CHECK(ttf_stream_write(s, "2NE1", 4) == TTF_OK);
        CHECK(ttf_stream_set_dict(s, d) == TTF_ESTATE);
        ttf_stream_destroy(s);
    }

    /* files and invalid rows */
    CHECK(write_file(file, "# request\nexact\tko-KR\t알쓰\t파일값\n\nlexicon\tko-KR\ten-US:Qorvex\t코르벡스\n"));
    ttf_dict_clear(d);
    CHECK(ttf_dict_size(d) == 0 && same(e, d, "알쓰 모임", "엔진 사용자 모임"));
    CHECK(ttf_dict_load(d, file) == TTF_OK && ttf_dict_size(d) == 2);
    CHECK(same(e, d, "알쓰 모임", "파일값 모임"));
    CHECK(same(e, d, "Qorvex 신제품", "코르벡스 신제품"));
    CHECK(write_file(file, "pattern\tko-KR\t{digits}호\t호\n"));
    CHECK(ttf_dict_load(d, file) == TTF_ERESOURCE);
    CHECK(write_file(file, "exact\tko-KR\t키만\n"));
    CHECK(ttf_dict_load(d, file) == TTF_ERESOURCE);
    CHECK(ttf_dict_load(d, "/tmp/ttf_request_dict_missing.tsv") == TTF_EIO);
    CHECK(ttf_dict_add_exact(d, "ko KR", "a", "b") == TTF_EINVAL);
    CHECK(ttf_dict_add_exact(d, "ko-KR", "", "b") == TTF_EINVAL);
    CHECK(ttf_dict_add_exact(d, "ko-KR", "a\tb", "c") == TTF_EINVAL);
    CHECK(ttf_dict_add_exact(d, "ko-KR", "a", "\xff") == TTF_EINVAL);
    CHECK(ttf_dict_add_lexicon(d, "*", "ko-KR", "word", "워드") == TTF_EINVAL);

#ifdef TTF_TEST_HAVE_THREADS
    {
        /* one engine, each thread its own dictionary, with and without it */
        enum { COUNT = 4 };
        pthread_t threads[COUNT];
        worker_t workers[COUNT];
        int i, failed = 0;
        for (i = 0; i < COUNT; ++i) {
            memset(&workers[i], 0, sizeof(workers[i]));
            workers[i].engine = plain;
            snprintf(workers[i].want, sizeof(workers[i].want), "값%d", i);
            CHECK(ttf_dict_create(&workers[i].dict) == TTF_OK);
            CHECK(ttf_dict_add_exact(workers[i].dict, "ko-KR", "2NE1", workers[i].want) == TTF_OK);
            strcat(workers[i].want, "의 신곡");
        }
        for (i = 0; i < COUNT; ++i)
            CHECK(pthread_create(&threads[i], NULL, work, &workers[i]) == 0);
        for (i = 0; i < COUNT; ++i) {
            pthread_join(threads[i], NULL);
            failed |= workers[i].failed;
            ttf_dict_destroy(workers[i].dict);
        }
        CHECK(!failed);
    }
#endif
    {
        /* [[...]]{@dictionary:NAME}, {@exact:NAME.tsv} and {@nodict} (2026-10-27): a named
           dictionary owned by the engine, added before the freeze */
        ttf_config_t config;
        ttf_frontend_options_t options;
        ttf_engine_t *n = NULL;
        ttf_dict_t *kpop = NULL, *other = NULL;
        ttf_config_init(&config);
        config.resource_root = argv[1];
        config.default_language = "ko-KR";
        ttf_frontend_options_init(&options);
        options.output_mode = TTF_OUTPUT_SPEECH;
        options.input_format = TTF_INPUT_ANNOTATED;
        CHECK(ttf_engine_create(&config, &n) == TTF_OK && ttf_engine_set_frontend_options(n, &options) == TTF_OK);
        CHECK(ttf_dict_create(&kpop) == TTF_OK && ttf_dict_create(&other) == TTF_OK);
        CHECK(ttf_dict_add_exact(kpop, "ko-KR", "알쓰", "명명값") == TTF_OK);
        CHECK(ttf_engine_add_dictionary(n, "bad name", kpop) == TTF_EINVAL);
        CHECK(ttf_engine_add_dictionary(n, "kpop.tsv", kpop) == TTF_EINVAL);
        CHECK(ttf_engine_add_dictionary(n, "kpop", kpop) == TTF_OK);
        CHECK(ttf_engine_add_dictionary(n, "kpop", other) == TTF_EINVAL);
        CHECK(ttf_engine_freeze(n) == TTF_OK);
        CHECK(ttf_engine_add_dictionary(n, "other", other) == TTF_ESTATE);
        ttf_dict_destroy(other);
        CHECK(same(n, NULL, "알쓰 [[알쓰]]{@dictionary:kpop} [[알쓰]]{@exact:kpop.tsv}", "알쓰 명명값 명명값"));
        /* inside the group the named dictionary takes the request dictionary's place */
        {
            ttf_dict_t *req = NULL;
            CHECK(ttf_dict_create(&req) == TTF_OK && ttf_dict_add_exact(req, "ko-KR", "알쓰", "요청값") == TTF_OK);
            CHECK(same(n, req, "알쓰 [[알쓰]]{@dictionary:kpop}", "요청값 명명값"));
            CHECK(same(n, req, "알쓰 [[알쓰]]{@nodict}", "요청값 알쓰"));
            CHECK(!strcmp(bytewise(n, req, "알쓰 [[알쓰]]{@dictionary:kpop} [[알쓰]]{@nodict}"), "요청값 명명값 알쓰"));
            /* an unknown name reads the group as usual (invalid_control=ignore) */
            CHECK(same(n, req, "[[알쓰]]{@dictionary:missing}", "요청값"));
            ttf_dict_destroy(req);
        }
        ttf_engine_destroy(n);
    }
    ttf_dict_destroy(d);
    ttf_dict_destroy(NULL);
    ttf_engine_destroy(e);
    ttf_engine_destroy(plain);
    remove(user);
    remove(file);
    puts("request dictionary: ok");
    return 0;
}
