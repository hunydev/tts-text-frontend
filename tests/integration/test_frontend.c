#include "tts_text_frontend/ttf.h"
#include "tts_text_frontend/ttf_language_pack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(expr)                                                                                \
    do {                                                                                           \
        if (!(expr)) {                                                                             \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr);                             \
            ++failures;                                                                            \
        }                                                                                          \
    } while (0)
#define OK(expr) CHECK((expr) == TTF_OK)
static ttf_engine_t *make_engine(const char *language, int freeze) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_config_init(&config);
    config.resource_root = TTF_RESOURCE_ROOT;
    config.default_language = language;
    OK(ttf_engine_create(&config, &engine));
    if (engine && freeze)
        OK(ttf_engine_freeze(engine));
    return engine;
}
static void expect(ttf_engine_t *engine, const char *input, const char *expected) {
    ttf_result_t result = {0};
    ttf_status_t status = ttf_normalize(engine, input, strlen(input), &result);
    OK(status);
    if (status == TTF_OK && strcmp(result.text, expected)) {
        fprintf(stderr, "input:    %s\nexpected: %s\nactual:   %s\n", input, expected, result.text);
        ++failures;
    }
    ttf_result_free(&result);
}
static void unescape(char *s) {
    char *read = s, *write = s;
    while (*read) {
        if (*read == '\\' && read[1]) {
            ++read;
            if (*read == 'n')
                *write++ = '\n';
            else if (*read == 't')
                *write++ = '\t';
            else if (*read == 'r')
                *write++ = '\r';
            else
                *write++ = *read;
            ++read;
        } else
            *write++ = *read++;
    }
    *write = 0;
}
static void load_fixture(ttf_engine_t *engine, const char *name, ttf_resource_layer_t layer) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/fixtures/%s", TTF_TEST_ROOT, name);
    OK(ttf_resource_load(engine, path, layer));
}
static void golden(const char *category) {
    char path[1024], line[16384];
    FILE *file;
    size_t cases = 0;
    ttf_engine_t *engine = make_engine(!strcmp(category, "en") ? "en-US" : "ko-KR", 0);
    if (!engine)
        return;
    if (!strcmp(category, "domain") || !strcmp(category, "customer"))
        load_fixture(engine, "domain.tsv", TTF_LAYER_DOMAIN);
    if (!strcmp(category, "customer"))
        load_fixture(engine, "customer.tsv", TTF_LAYER_CUSTOMER);
    OK(ttf_engine_freeze(engine));
    snprintf(path, sizeof(path), "%s/golden/%s/cases.tsv", TTF_TEST_ROOT, category);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    if (!file) {
        ttf_engine_destroy(engine);
        return;
    }
    while (fgets(line, sizeof(line), file)) {
        char *tab;
        size_t n = strlen(line);
        if (line[0] == '#' || line[0] == '\n')
            continue;
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = 0;
        tab = strchr(line, '\t');
        CHECK(tab != NULL);
        if (!tab)
            continue;
        *tab++ = 0;
        unescape(line);
        unescape(tab);
        expect(engine, line, tab);
        ++cases;
    }
    fclose(file);
    CHECK(cases > 0);
    printf("%s: %zu golden cases\n", category, cases);
    ttf_engine_destroy(engine);
}
static void api(void) {
    ttf_token_t *tokens = NULL;
    size_t count = 0, i;
    ttf_engine_t *engine = make_engine("ko-KR", 0);
    ttf_result_t result = {0};
    const char *input = "OpenAI는 좋다";
    int found = 0;
    OK(ttf_tokenize(input, strlen(input), &tokens, &count));
    CHECK(count == 4);
    if (count >= 4) {
        CHECK(tokens[0].script == TTF_SCRIPT_LATIN && tokens[0].begin == 0 && tokens[0].end == 6);
        CHECK(tokens[1].script == TTF_SCRIPT_HANGUL && tokens[1].begin == 6 && tokens[1].end == 9);
        CHECK(tokens[2].script == TTF_SCRIPT_WHITESPACE);
        CHECK(tokens[3].script == TTF_SCRIPT_HANGUL);
    }
    ttf_tokens_free(tokens);
    CHECK(ttf_tokenize("\xff", 1, &tokens, &count) == TTF_EUTF8);
    CHECK(ttf_tokenize("\xc0\xaf", 2, &tokens, &count) == TTF_EUTF8);
    CHECK(ttf_tokenize("\xed\xa0\x80", 3, &tokens, &count) == TTF_EUTF8);
    if (!engine)
        return;
    CHECK(ttf_normalize(engine, "1", 1, &result) == TTF_ESTATE);
    CHECK(ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "\xff", "단어") == TTF_EUTF8);
    OK(ttf_engine_freeze(engine));
    CHECK(ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "Apple", "수정") == TTF_ESTATE);
    CHECK(ttf_lexicon_remove(engine, TTF_LAYER_USER, "en-US", "ko-KR", "Apple") == TTF_ESTATE);
    CHECK(ttf_normalize(engine, "\xf0\x9f", 2, &result) == TTF_EUTF8);
    expect(engine, "", "");
    input = "1st는 첫 번째입니다. Please go to the 1st floor.";
    OK(ttf_normalize(engine, input, strlen(input), &result));
    for (i = 0; i < result.span_count; ++i) {
        const ttf_span_t *span = &result.spans[i];
        CHECK(span->begin <= span->end && span->end <= result.length);
        CHECK(span->input_begin <= span->input_end && span->input_end <= strlen(input));
        if (span->semantic.type == TTF_SEM_ORDINAL) {
            CHECK(!strcmp(span->semantic.source_lang, "en-US"));
            CHECK(!strcmp(span->semantic.value, "1"));
            if (!strcmp(span->semantic.context_lang, "ko-KR"))
                found |= 1;
            if (!strcmp(span->semantic.context_lang, "en-US"))
                found |= 2;
        }
    }
    CHECK(found == 3);
    ttf_result_free(&result);
    ttf_engine_destroy(engine);
}
static int custom_script(ttf_script_t script) {
    return script == TTF_SCRIPT_OTHER ? 100 : 0;
}
static int custom_context(const ttf_pack_input_t *input) {
    size_t i;
    for (i = 0; i < input->token_count; ++i)
        if (input->tokens[i].script == TTF_SCRIPT_OTHER)
            return 100;
    return 0;
}
static ttf_status_t custom_verbalize(const ttf_engine_t *engine, const ttf_semantic_t *semantic,
                                     const char *surface, size_t length, ttf_buffer_t *output) {
    (void)engine;
    (void)surface;
    (void)length;
    if (semantic->type == TTF_SEM_CARDINAL || semantic->type == TTF_SEM_NUMBER)
        return ttf_buffer_puts(output, "synthetic-number");
    return ttf_buffer_append(output, surface, length);
}
static void extension(void) {
    static const ttf_language_pack_t pack = {sizeof(ttf_language_pack_t),
                                             TTF_ABI_VERSION,
                                             "xx-Test",
                                             NULL,
                                             custom_script,
                                             custom_context,
                                             NULL,
                                             custom_verbalize,
                                             NULL,
                                             NULL,
                                             NULL};
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_result_t result = {0};
    ttf_config_init(&config);
    config.resource_root = TTF_RESOURCE_ROOT;
    config.default_language = "ko-KR";
    OK(ttf_engine_create(&config, &engine));
    if (!engine)
        return;
    OK(ttf_engine_register_pack(engine, &pack));
    OK(ttf_engine_freeze(engine));
    OK(ttf_normalize(engine, "語 7", strlen("語 7"), &result));
    if (result.text)
        CHECK(!strcmp(result.text, "語 synthetic-number"));
    ttf_result_free(&result);
    CHECK(ttf_engine_register_pack(engine, &pack) == TTF_ESTATE);
    ttf_engine_destroy(engine);
}
static int contains_ascii_letters(const char *s) {
    for (; *s; ++s)
        if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z'))
            return 1;
    return 0;
}
static void resources(void) {
    ttf_engine_t *engine = make_engine("ko-KR", 0);
    ttf_result_t result = {0};
    char path[1024];
    if (!engine)
        return;
    load_fixture(engine, "priority_domain.tsv", TTF_LAYER_DOMAIN);
    load_fixture(engine, "priority_customer.tsv", TTF_LAYER_CUSTOMER);
    OK(ttf_lexicon_add(engine, TTF_LAYER_DOMAIN, "en-US", "ko-KR", "Apple", "도메인애플"));
    OK(ttf_lexicon_add(engine, TTF_LAYER_CUSTOMER, "en-US", "ko-KR", "Apple", "고객애플"));
    OK(ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "Apple", "사용자애플"));
    snprintf(path, sizeof(path), "%s/fixtures/invalid.tsv", TTF_TEST_ROOT);
    CHECK(ttf_resource_load(engine, path, TTF_LAYER_CUSTOMER) == TTF_ERESOURCE);
    CHECK(ttf_resource_get(engine, "exact", "ko-KR", "transaction", NULL) == NULL);
    OK(ttf_engine_freeze(engine));
    expect(engine, "Apple은 사과입니다.", "사용자애플은 사과입니다.");
    OK(ttf_normalize(engine, "Apple은", strlen("Apple은"), &result));
    if (result.span_count) {
        CHECK(result.spans[0].semantic.layer == TTF_LAYER_SYSTEM);
        CHECK(result.spans[0].pronunciation_layer == TTF_LAYER_USER);
        CHECK(!strcmp(result.spans[0].verbalizer, "en-US"));
    }
    ttf_result_free(&result);
        /* Particle agreement (2026-09-22): a particle written against a reading
       takes the form that reading requires. */
    expect(engine, "alpha는 단어입니다.", "고객은 단어입니다.");
    expect(engine, "beta gamma는 단어입니다.", "긴일치는 단어입니다.");
    expect(engine, "X12는 값입니다.", "정확은 값입니다.");
    expect(engine, "X99는 값입니다.", "패턴은 값입니다.");
    ttf_engine_destroy(engine);
    engine = make_engine("ko-KR", 0);
    if (!engine)
        return;
    OK(ttf_lexicon_add(engine, TTF_LAYER_CUSTOMER, "en-US", "ko-KR", "Apple", "고객애플"));
    OK(ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "Apple", "사용자애플"));
    OK(ttf_lexicon_remove(engine, TTF_LAYER_USER, "en-US", "ko-KR", "Apple"));
    OK(ttf_lexicon_remove(engine, TTF_LAYER_SYSTEM, "en-US", "ko-KR", "WiFi"));
    OK(ttf_engine_freeze(engine));
    expect(engine, "Apple은 사과입니다.", "고객애플은 사과입니다.");
    OK(ttf_normalize(engine, "WiFi를 사용합니다.", strlen("WiFi를 사용합니다."), &result));
    if (result.text) {
        CHECK(result.length > strlen("를 사용합니다."));
        CHECK(!contains_ascii_letters(result.text));
    }
    ttf_result_free(&result);
    OK(ttf_normalize(engine, "foobarquux는 단어입니다.", strlen("foobarquux는 단어입니다."),
                     &result));
    if (result.text) {
        CHECK(result.length > strlen("는 단어입니다."));
        CHECK(!contains_ascii_letters(result.text));
    }
    ttf_result_free(&result);
    ttf_engine_destroy(engine);
}
typedef struct {
    ttf_buffer_t text;
    size_t calls, source_end;
} collector_t;
static int collect(const ttf_segment_t *segment, void *user_data) {
    collector_t *collector = (collector_t *)user_data;
    size_t i;
    CHECK(segment->input_begin == collector->source_end);
    CHECK(segment->input_end >= segment->input_begin);
    collector->source_end = segment->input_end;
    for (i = 0; i < segment->span_count; ++i) {
        CHECK(segment->spans[i].begin <= segment->spans[i].end);
        CHECK(segment->spans[i].end <= segment->length);
    }
    ++collector->calls;
    return ttf_buffer_append(&collector->text, segment->text, segment->length) != TTF_OK;
}
static void run_chunks(ttf_engine_t *engine, const char *input, const char *expected, size_t split,
                       unsigned random_seed) {
    ttf_stream_t *stream = NULL;
    collector_t collector = {{0}, 0, 0};
    size_t len = strlen(input), offset = 0;
    ttf_status_t status = ttf_stream_create(engine, collect, &collector, &stream);
    OK(status);
    if (status != TTF_OK)
        return;
    if (split <= len) {
        OK(ttf_stream_write(stream, input, split));
        OK(ttf_stream_write(stream, input + split, len - split));
    } else {
        while (offset < len) {
            size_t n;
            if (random_seed) {
                random_seed = random_seed * 1664525u + 1013904223u;
                n = 1 + random_seed % 19u;
            } else
                n = 1;
            if (n > len - offset)
                n = len - offset;
            OK(ttf_stream_write(stream, input + offset, n));
            offset += n;
        }
    }
    OK(ttf_stream_flush(stream));
    CHECK(collector.text.data != NULL);
    if (collector.text.data && strcmp(collector.text.data, expected)) {
        fprintf(stderr, "stream split %zu mismatch\ninput: %s\nexpected: %s\nactual: %s\n", split,
                input, expected, collector.text.data);
        ++failures;
    }
    CHECK(collector.source_end == len);
    ttf_stream_destroy(stream);
    ttf_buffer_free(&collector.text);
}
static int cancel(const ttf_segment_t *segment, void *user_data) {
    (void)segment;
    (void)user_data;
    return 1;
}
static void streaming(void) {
    static const char *cases[] = {
        "1st는 첫 번째라는 뜻입니다.\n영어로는 다음과 같은 예문이 있습니다.\nPlease go to the 1st "
        "floor",
        "가격은 12,500원입니다. Apple은 사과입니다. GPU를 사용합니다. WiFi를 사용합니다.",
        "3.5만원입니다. ver. 2.1.3을 사용합니다. www.example.com입니다. 2026. 9. 16. 기준입니다.",
        "GPT-4를 사용합니다. Apple released the iPhone. Please go to the 1st floor.",
        "길고 긴 문장에서 첫 번째 문구를 빠르게 확정하고 다음 문구도 순서대로 전달합니다, 숫자 "
        "12,500원을 중간에서 끊으면 안 됩니다. 마지막 문장입니다!"};
    ttf_engine_t *engine = make_engine("ko-KR", 1);
    size_t i, split, runs = 0;
    ttf_stream_t *stream = NULL;
    collector_t collector = {{0}, 0, 0};
    if (!engine)
        return;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        ttf_result_t result = {0};
        OK(ttf_normalize(engine, cases[i], strlen(cases[i]), &result));
        if (!result.text)
            continue;
        for (split = 0; split <= strlen(cases[i]); ++split) {
            run_chunks(engine, cases[i], result.text, split, 0);
            ++runs;
        }
        run_chunks(engine, cases[i], result.text, strlen(cases[i]) + 1, 0);
        ++runs;
        for (split = 1; split <= 20; ++split) {
            run_chunks(engine, cases[i], result.text, strlen(cases[i]) + 1, (unsigned)split);
            ++runs;
        }
        ttf_result_free(&result);
    }
    OK(ttf_stream_create(engine, collect, &collector, &stream));
    OK(ttf_stream_write(stream, "가격은 12", strlen("가격은 12")));
    CHECK(collector.calls == 0);
    OK(ttf_stream_write(stream, ",500원입니다. 다음", strlen(",500원입니다. 다음")));
    CHECK(collector.calls > 0);
    OK(ttf_stream_flush(stream));
    ttf_stream_destroy(stream);
    ttf_buffer_free(&collector.text);
    stream = NULL;
    OK(ttf_stream_create(engine, cancel, NULL, &stream));
    CHECK(ttf_stream_write(stream, "문장입니다. 다음", strlen("문장입니다. 다음")) ==
          TTF_ECALLBACK);
    ttf_stream_destroy(stream);
    stream = NULL;
    OK(ttf_stream_create(engine, collect, &collector, &stream));
    OK(ttf_stream_write(stream, "\xed", 1));
    CHECK(ttf_stream_flush(stream) == TTF_EUTF8);
    ttf_stream_destroy(stream);
    printf("%zu arbitrary chunk streaming comparisons\n", runs);
    ttf_engine_destroy(engine);
    {
        ttf_config_t config;
        const char *dated = "날짜는 2026. 9. 16. 기준입니다.";
        const char *expected = "날짜는 이천이십육 년 구 월 십육 일 기준입니다.";
        ttf_config_init(&config);
        config.resource_root = TTF_RESOURCE_ROOT;
        config.first_segment_target = 9;
        config.normal_segment_target = 9;
        config.max_segment_length = 9;
        engine = NULL;
        OK(ttf_engine_create(&config, &engine));
        if (engine) {
            OK(ttf_engine_freeze(engine));
            expect(engine, dated, expected);
            for (split = 0; split <= strlen(dated); ++split)
                run_chunks(engine, dated, expected, split, 0);
            ttf_engine_destroy(engine);
        }
    }
}
int main(int argc, char **argv) {
    if (argc < 2)
        return 2;
    if (!strcmp(argv[1], "golden") && argc == 3)
        golden(argv[2]);
    else if (!strcmp(argv[1], "api"))
        api();
    else if (!strcmp(argv[1], "extension"))
        extension();
    else if (!strcmp(argv[1], "resources"))
        resources();
    else if (!strcmp(argv[1], "streaming"))
        streaming();
    else
        return 2;
    if (failures)
        fprintf(stderr, "%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
