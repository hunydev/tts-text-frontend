#include "tts_text_frontend/ttf.h"
#include "tts_text_frontend/ttf_language_pack.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Reproducible generative smoke test: it mixes complete semantic atoms with
   troublesome punctuation and UTF-8, then cuts bytes without Unicode alignment. */
static uint32_t state = UINT32_C(0x187ff423);
static uint32_t next_random(void) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}
typedef struct {
    ttf_buffer_t text;
    size_t input_end, span_count;
    int invalid;
} collector_t;
static int collect(const ttf_segment_t *segment, void *userdata) {
    collector_t *c = (collector_t *)userdata;
    size_t i;
    if (segment->input_begin != c->input_end || segment->input_end < segment->input_begin)
        c->invalid = 1;
    for (i = 0; i < segment->span_count; ++i) {
        const ttf_span_t *s = &segment->spans[i];
        if (s->begin > s->end || s->end > segment->length || s->input_begin > s->input_end ||
            s->input_end > segment->input_end || !s->semantic.source_lang[0] ||
            !s->semantic.context_lang[0] || !s->semantic.render_lang[0])
            c->invalid = 1;
    }
    c->span_count += segment->span_count;
    c->input_end = segment->input_end;
    return ttf_buffer_append(&c->text, segment->text, segment->length) != TTF_OK;
}
static int compare(ttf_engine_t *engine, const char *text, size_t n, unsigned case_id) {
    ttf_result_t batch = {0};
    unsigned pass;
    ttf_status_t st;
    st = ttf_normalize(engine, text, n, &batch);
    if (st != TTF_OK) {
        fprintf(stderr, "generated case %u batch failed: %s\n%s\n", case_id, ttf_status_string(st),
                text);
        return 1;
    }
    for (pass = 0; pass < 5; ++pass) {
        ttf_stream_t *stream = NULL;
        collector_t c = {{0}, 0, 0, 0};
        size_t pos = 0;
        st = ttf_stream_create(engine, collect, &c, &stream);
        while (st == TTF_OK && pos < n) {
            size_t take = pass ? 1 + next_random() % 23 : 1;
            if (take > n - pos)
                take = n - pos;
            st = ttf_stream_write(stream, text + pos, take);
            pos += take;
        }
        if (st == TTF_OK)
            st = ttf_stream_flush(stream);
        if (st != TTF_OK || c.invalid || c.input_end != n || c.span_count != batch.span_count ||
            c.text.length != batch.length || !c.text.data ||
            memcmp(c.text.data, batch.text, batch.length)) {
            fprintf(stderr,
                    "generated case %u pass %u mismatch (%s)\ninput: %s\nbatch: %s\nstream: %s\n",
                    case_id, pass, ttf_status_string(st), text, batch.text,
                    c.text.data ? c.text.data : "<empty>");
            ttf_stream_destroy(stream);
            ttf_buffer_free(&c.text);
            ttf_result_free(&batch);
            return 1;
        }
        ttf_stream_destroy(stream);
        ttf_buffer_free(&c.text);
    }
    ttf_result_free(&batch);
    return 0;
}
int main(void) {
    static const char *atoms[] = {"3 kg",
                                  "3 개",
                                  "₩ 12,500",
                                  "1 / 2",
                                  "14:30:05",
                                  "12,34",
                                  "2026-02-30",
                                  "C++를",
                                  "USB-C를",
                                  "3D는",
                                  "그는 \"Please go to the 1st floor\"라고 말했다",
                                  "Apple은",
                                  "사과입니다",
                                  "1st는",
                                  "Please go to the 1st floor",
                                  "12,500원입니다",
                                  "2026. 9. 16.",
                                  "2026-09-16",
                                  "14:30",
                                  "09:05",
                                  "3.5만원입니다",
                                  "32GB",
                                  "2GHz",
                                  "3개",
                                  "12.5%",
                                  "$12.50",
                                  "₩12,500",
                                  "010-1234-5678",
                                  "02-123-4567",
                                  "123-456",
                                  "ver. 2.1.3",
                                  "127.0.0.1",
                                  "www.example.com입니다",
                                  "https://example.com/a?x=3.5",
                                  "a@example.com",
                                  "GPU를",
                                  "WiFi를",
                                  "foobar는",
                                  "OpenAI가",
                                  "GPT-4를",
                                  "USB와",
                                  "한글",
                                  "語",
                                  "😀",
                                  "café는",
                                  "0",
                                  "100000000",
                                  "18446744073709551615"};
    static const char *separators[] = {" ", " ", " ", ", ", ". ", "! ", "? ", "\n", "; ", "\t"};
    unsigned profile, id;
    for (profile = 0; profile < 4; ++profile) {
        ttf_config_t cfg;
        ttf_engine_t *engine = NULL;
        ttf_config_init(&cfg);
        cfg.resource_root = TEST_RESOURCE_ROOT;
        cfg.first_segment_target = profile ? 7 : 48;
        cfg.normal_segment_target = profile ? 13 : 112;
        cfg.max_segment_length = profile ? 25 : 200;
        cfg.sentence_split = profile != 2;
        cfg.phrase_split = profile != 3;
        if (ttf_engine_create(&cfg, &engine) != TTF_OK || ttf_engine_freeze(engine) != TTF_OK)
            return 1;
        for (id = 0; id < 50; ++id) {
            ttf_buffer_t input = {0};
            unsigned i, count = 4 + next_random() % 20;
            for (i = 0; i < count; ++i) {
                if (ttf_buffer_puts(&input,
                                    atoms[next_random() % (sizeof(atoms) / sizeof(atoms[0]))]) !=
                    TTF_OK)
                    return 1;
                if (ttf_buffer_puts(
                        &input,
                        separators[next_random() % (sizeof(separators) / sizeof(separators[0]))]) !=
                    TTF_OK)
                    return 1;
            }
            if (compare(engine, input.data, input.length, profile * 50 + id)) {
                ttf_buffer_free(&input);
                ttf_engine_destroy(engine);
                return 1;
            }
            ttf_buffer_free(&input);
        }
        ttf_engine_destroy(engine);
    }
    /* Random byte scanner probes exercise malformed, truncated and overlong
       encodings independently of normalization and resource policy. */
    for (id = 0; id < 5000; ++id) {
        char bytes[32];
        size_t n = 1 + next_random() % sizeof(bytes), i, count = 0, end = 0;
        ttf_token_t *tokens = NULL;
        ttf_status_t st;
        for (i = 0; i < n; ++i)
            bytes[i] = (char)(next_random() & 255);
        st = ttf_tokenize(bytes, n, &tokens, &count);
        if (st == TTF_OK) {
            for (i = 0; i < count; ++i) {
                if (tokens[i].begin != end || tokens[i].end <= end || tokens[i].end > n)
                    return 1;
                end = tokens[i].end;
            }
            if (end != n)
                return 1;
        } else if (st != TTF_EUTF8)
            return 1;
        ttf_tokens_free(tokens);
    }
    puts("200 generated inputs, 1000 byte-chunk comparisons, 5000 byte-scanner probes passed");
    return 0;
}
