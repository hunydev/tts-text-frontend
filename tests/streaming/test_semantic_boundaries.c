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
        {"2026년 6월 1일", "이천이십육 년 유월 일 일"},
        {"3.5만   원", "삼만오천 원"},
        {"1억2천3백만원", "일억이천삼백만 원"},
        {"1 억 2 천 3 백 만 원", "일억이천삼백만 원"},
        {"1억 2,500만원", "일억이천오백만 원"},
        {"1억2.5천만원", "일억이천오백만 원"},
        {"2조3억4만원", "이조삼억사만 원"},
        {"-1억2천만원", "마이너스 일억이천만 원"},
        {"1억2억원", "1억2억원"},
        {"2만 3억원", "2만 3억원"},
        {"1억 2,50만원", "1억 2,50만원"},
        {"계좌는 110-123-456789입니다.", "계좌는 일일공 일이삼 사오육칠팔구입니다."},
        {"거래 참조 번호는 001204입니다.", "거래 참조 번호는 공공일이공사입니다."},
        {"참조  \t번호: 000123", "참조  \t번호: 공공공일이삼"},
        {"계좌는 3개입니다.", "계좌는 세 개입니다."},
        {"금리는 1.5% ~ 2.5%입니다.", "금리는 일 점 오 퍼센트에서 이 점 오 퍼센트입니다."},
        {"금액은 1만원~2만원입니다.", "금액은 만 원에서 이만 원입니다."},
        {"수량은 3개 ~ 5개입니다.", "수량은 세 개에서 다섯 개입니다."},
        {"금액은 1~2만 원입니다.", "금액은 만 원에서 이만 원입니다."},
        {"시간은 1시~2시에 가능합니다.", "시간은 한 시에서 두 시에 가능합니다."},
        {"09:00 ~ 10:30", "아홉 시 영 분에서 열 시 삼십 분"},
        {"금액은 1억 원 - 2억 원입니다.", "금액은 일억 원에서 이억 원입니다."},
        {"\"The price is $10-$20.\"", "\"The price is ten to twenty dollars.\""},
        {"단위는 1kg~2km입니다.", "단위는 1kg~2km입니다."},
        {"25bp", "이십오 베이시스포인트"},
        {"0.25%p", "영 점 이오 퍼센트포인트"},
        {"1.5-2.5%", "일 점 오 퍼센트에서 이 점 오 퍼센트"},
        {"1.5 - 2.5 %", "일 점 오 퍼센트에서 이 점 오 퍼센트"},
        {"1.2e6원", "백이십만 원"},
        {"1.25e-2%", "영 점 영일이오 퍼센트"},
        {"제목은 The 1st day입니다.", "제목은 The first day입니다."},
        {"제목은: The 1st day입니다.", "제목은: The first day입니다."},
        {"<code>GPU.\n3개 1st</code> 예제는 3개입니다.",
         "<code>GPU.\n3개 1st</code> 예제는 세 개입니다."},
        {"<code title=\"3 > 2\">GPU</code>은 예제입니다.",
         "<code title=\"3 > 2\">GPU</code>은 예제입니다."},
        {"<code>GPU <code>3개</code> CPU</code>",
         "<code>GPU <code>3개</code> CPU</code>"},
        {"<em>Apple은 사과입니다.</em>", "<em>애플은 사과입니다.</em>"},
        {"<!-- GPU 3개 --> 수량은 3개입니다.", "<!-- GPU 3개 --> 수량은 세 개입니다."},
        {"``GPU `3개` 1st`` 예제는 3개입니다.", "``GPU `3개` 1st`` 예제는 세 개입니다."},
        {"```c\nint GPU = 3;\n```\n파일은 3개입니다.",
         "```c\nint GPU = 3;\n```\n파일은 세 개입니다."},
        {"예제는 3개와 `GPU 1st", "예제는 세 개와 `GPU 1st"},
        {"예제는 3개와 <code>GPU 1st", "예제는 세 개와 <code>GPU 1st"},
        {"파일 /etc/app/config-v2.json을 확인합니다.",
         "파일 /etc/app/config-v2.json을 확인합니다."},
        {"파일 ./a-1.txt, ../b-2.txt는 2개입니다.", "파일 ./a-1.txt, ../b-2.txt는 두 개입니다."},
        {"파일 C:\\reports\\Q1.csv를 3개 준비합니다.", "파일 C:\\reports\\Q1.csv를 세 개 준비합니다."},
        {"파일 \\\\server\\share\\a-1.csv를 3개 준비합니다.",
         "파일 \\\\server\\share\\a-1.csv를 세 개 준비합니다."},
        {"파일 \"/home/user/한글 원고.txt\"를 3개 준비합니다.",
         "파일 \"/home/user/한글 원고.txt\"를 세 개 준비합니다."},
        {"`GPU`<code>3개</code> 예제는 3개입니다.", "`GPU`<code>3개</code> 예제는 세 개입니다."},
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
