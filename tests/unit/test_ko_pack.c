#include "tts_text_frontend/ttf_language_pack.h"
#include <stdio.h>
#include <string.h>

extern const ttf_language_pack_t ttf_pack_ko_KR;
static int failures;

static void expect_semantic(ttf_engine_t *engine, ttf_semantic_type_t type, const char *value,
                            const char *secondary, const char *unit, const char *expected) {
    ttf_semantic_t ir;
    ttf_buffer_t output = {0};
    ttf_status_t status;
    memset(&ir, 0, sizeof(ir));
    ir.struct_size = sizeof(ir);
    ir.type = type;
    snprintf(ir.value, sizeof(ir.value), "%s", value);
    snprintf(ir.secondary, sizeof(ir.secondary), "%s", secondary);
    snprintf(ir.unit, sizeof(ir.unit), "%s", unit);
    status = ttf_pack_ko_KR.verbalize(engine, &ir, value, strlen(value), &output);
    if (status != TTF_OK || !output.data || strcmp(output.data, expected)) {
        fprintf(stderr, "KO %s (%s): expected [%s], got [%s], status %s\n", value, unit, expected,
                output.data ? output.data : "", ttf_status_string(status));
        ++failures;
    }
    ttf_buffer_free(&output);
}

static void expect_phones(ttf_engine_t *engine, const char *phones, const char *expected) {
    ttf_pronunciation_t pronunciation;
    ttf_buffer_t output = {0};
    ttf_status_t status;
    memset(&pronunciation, 0, sizeof(pronunciation));
    snprintf(pronunciation.phones, sizeof(pronunciation.phones), "%s", phones);
    snprintf(pronunciation.alphabet, sizeof(pronunciation.alphabet), "ttf-phones-v1");
    status = ttf_pack_ko_KR.render_phones(engine, &pronunciation, &output);
    if (status != TTF_OK || !output.data || strcmp(output.data, expected)) {
        fprintf(stderr, "KO phones %s: expected [%s], got [%s]\n", phones, expected,
                output.data ? output.data : "");
        ++failures;
    }
    ttf_buffer_free(&output);
}

int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_buffer_t fallback = {0};
    ttf_status_t status;
    size_t i;
    if (argc != 2) {
        fprintf(stderr, "usage: test_ko_pack RESOURCE_ROOT\n");
        return 2;
    }
    ttf_config_init(&config);
    config.resource_root = argv[1];
    status = ttf_engine_create(&config, &engine);
    if (status != TTF_OK) {
        fprintf(stderr, "engine: %s\n", ttf_status_string(status));
        return 2;
    }
    expect_semantic(engine, TTF_SEM_CARDINAL, "0", "", "", "영");
    expect_semantic(engine, TTF_SEM_CARDINAL, "10001", "", "", "만일");
    expect_semantic(engine, TTF_SEM_CARDINAL, "100000000", "", "", "일억");
    expect_semantic(engine, TTF_SEM_CARDINAL, "100000001", "", "", "일억일");
    expect_semantic(engine, TTF_SEM_CARDINAL, "1000000000000", "", "", "일조");
    expect_semantic(engine, TTF_SEM_CARDINAL, "1001001001001", "", "", "일조십억백만천일");
    expect_semantic(engine, TTF_SEM_CARDINAL, "000100", "", "", "백");
    expect_semantic(engine, TTF_SEM_CARDINAL, "-12.50", "", "", "마이너스 십이 점 오영");
    expect_semantic(engine, TTF_SEM_COUNTER, "20", "", "개", "스무 개");
    expect_semantic(engine, TTF_SEM_COUNTER, "21", "", "명", "스물한 명");
    expect_semantic(engine, TTF_SEM_COUNTER, "99", "", "살", "아흔아홉 살");
    expect_semantic(engine, TTF_SEM_COUNTER, "100", "", "개", "백 개");
    expect_semantic(engine, TTF_SEM_TIME, "09:05", "", "", "아홉 시 오 분");
    expect_semantic(engine, TTF_SEM_TIME, "14:30", "", "", "열네 시 삼십 분");
    expect_semantic(engine, TTF_SEM_DATE, "2026-06-10", "", "", "이천이십육 년 유월 십 일");
    expect_semantic(engine, TTF_SEM_DATE, "2026-10-16", "", "", "이천이십육 년 시월 십육 일");
    expect_semantic(engine, TTF_SEM_PHONE, "010-1234-5678", "", "", "공일공 일이삼사 오육칠팔");
    expect_semantic(engine, TTF_SEM_RANGE, "123", "456", "", "백이십삼에서 사백오십육");
    expect_phones(engine, "F UW B AA R", "푸바");
    expect_phones(engine, "W AY F AY", "와이파이");
    expect_phones(engine, "letter.C letter.P letter.U", "씨피유");
    expect_phones(engine, "F UW1 B AA0 R", "푸바");
    status = ttf_pack_ko_KR.render_graphemes(engine, "wifi", 4, &fallback);
    if (status != TTF_OK || !fallback.data || strcmp(fallback.data, "위피"))
        ++failures;
    ttf_buffer_free(&fallback);
    status = ttf_pack_ko_KR.render_graphemes(engine, "ABCDEFGHIJKLMNOPQRSTUVWXYZ", 26, &fallback);
    if (status != TTF_OK || !fallback.length)
        ++failures;
    for (i = 0; i < fallback.length; ++i) {
        unsigned char c = (unsigned char)fallback.data[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
            ++failures;
    }
    ttf_buffer_free(&fallback);
    ttf_engine_destroy(engine);
    return failures ? 1 : 0;
}
