#include "../../src/core/internal.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)
static size_t rules(const ttf_engine_t *e) {
    size_t n = 0;
    const ttf_entry_t *p;
    for (p = e->rules; p; p = p->rule_next) ++n;
    return n;
}
/* exact and pattern rows live in their own table (2026-10-25) */
static size_t rule_rows(const ttf_engine_t *e) {
    size_t i, n = 0;
    for (i = 0; e->rule_table && i < e->rule_buckets; ++i) {
        const ttf_entry_t *p;
        for (p = e->rule_table[i]; p; p = p->next) ++n;
    }
    return n;
}
static size_t common_rows(const ttf_engine_t *e) {
    size_t i, n = 0;
    for (i = 0; i < ttf_buckets(e); ++i) {
        const ttf_entry_t *p;
        for (p = e->resources[i]; p; p = p->next) ++n;
    }
    return n;
}
static int checked_freeze(ttf_engine_t *e, size_t added) {
    typedef struct {
        const ttf_entry_t *entry, *selected;
        size_t order;
        ttf_resource_layer_t layer;
    } saved_t;
    size_t i, n = common_rows(e), at = 0, ordinary = 0;
    saved_t *saved = calloc(n, sizeof(*saved));
    ttf_resource_validation_report_t before, after;
    ttf_cjk_index_t *index;
    CHECK(saved);
    ttf_resource_validation_report_init(&before);
    ttf_resource_validation_report_init(&after);
    CHECK(ttf_engine_validate_resources(e, &before, NULL) == TTF_OK);
    for (i = 0; i < ttf_buckets(e); ++i) {
        const ttf_entry_t *p;
        for (p = e->resources[i]; p; p = p->next) {
            saved[at].entry = p;
            saved[at].selected = ttf_resource_entry(e, p->kind, p->lang, p->key);
            saved[at].order = p->order; saved[at].layer = p->layer;
            ++at;
            if (!ttf_cjk_resource_kind(p->kind)) ++ordinary;
        }
    }
    CHECK(ttf_engine_freeze(e) == TTF_OK);
    index = e->cjk;
    CHECK(index && common_rows(e) == ordinary);
    CHECK(ttf_engine_freeze(e) == TTF_OK && e->cjk == index);
    CHECK(ttf_engine_validate_resources(e, &after, NULL) == TTF_OK);
    CHECK(before.records == n + rule_rows(e) && after.records == n + rule_rows(e) + added);
    CHECK(before.validated_records + added == after.validated_records);
    CHECK(before.unvalidated_records == after.unvalidated_records);
    for (i = 0; i < n; ++i) {
        const ttf_entry_t *p = saved[i].entry, *hit = saved[i].selected;
        ttf_resource_layer_t layer;
        CHECK(p->order == saved[i].order && p->layer == saved[i].layer);
        CHECK(ttf_resource_entry(e, p->kind, p->lang, p->key) == hit);
        CHECK(ttf_resource_get(e, p->kind, p->lang, p->key, &layer) == hit->value);
        CHECK(layer == hit->layer);
    }
    printf("freeze: before=%zu, after=%zu, common=%zu, detached=%zu, engine_bytes=%zu; explicit rows unchanged\n",
           n, n + added, ordinary, n + added - ordinary, sizeof(*e));
    free(saved);
    return 0;
}
static int normalized(ttf_engine_t *e, const char *text, const char *expected) {
    ttf_result_t result = {0};
    int equal;
    if (ttf_normalize(e, text, strlen(text), &result) != TTF_OK) return 0;
    equal = !strcmp(result.text, expected);
    ttf_result_free(&result);
    return equal;
}
int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_frontend_options_t options;
    ttf_engine_t *base = NULL, *e = NULL;
    size_t i, indexed = 0;
    CHECK(argc == 2);
    ttf_config_init(&config); config.resource_root = argv[1];
    ttf_frontend_options_init(&options);
    options.hanja = TTF_HANJA_KOREAN;
    options.kana = TTF_KANA_KOREAN;
    CHECK(ttf_engine_create(&config, &base) == TTF_OK);
    CHECK(checked_freeze(base, 0) == 0);
    CHECK(ttf_engine_create(&config, &e) == TTF_OK);
    CHECK(ttf_engine_set_frontend_options(e, &options) == TTF_OK);
    CHECK(ttf_resource_set(e, "cjk_word", "ko-KR", "音樂", "도메인", TTF_LAYER_DOMAIN) == TTF_OK);
    CHECK(ttf_resource_set(e, "cjk_word", "ko-KR", "音樂", "고객", TTF_LAYER_CUSTOMER) == TTF_OK);
    CHECK(ttf_lexicon_add(e, TTF_LAYER_USER, "und", "ko-KR", "音樂", "사용자") == TTF_OK);
    /* Wildcard, exact-language, same-layer declaration tie, plus another lang. */
    CHECK(ttf_resource_set(e, "hanja_reading", "*", "山", "뫼", TTF_LAYER_CUSTOMER) == TTF_OK);
    CHECK(ttf_resource_set(e, "hanja_reading", "ko-KR", "山", "산", TTF_LAYER_CUSTOMER) == TTF_OK);
    CHECK(ttf_resource_set(e, "hanja_reading", "ko-KR", "山", "강", TTF_LAYER_CUSTOMER) == TTF_OK);
    CHECK(ttf_resource_set(e, "hanja_reading", "xx-Test", "山", "mountain", TTF_LAYER_SYSTEM) == TTF_OK);
    for (i = 0; i < 18000; ++i) {
        uint32_t cp = 0x4e00 + (uint32_t)i;
        char key[4] = {(char)(0xe0 | (cp >> 12)), (char)(0x80 | ((cp >> 6) & 63)),
                       (char)(0x80 | (cp & 63)), 0};
        CHECK(ttf_resource_set(e, "cjk_word", "ko-KR", key, "가", TTF_LAYER_SYSTEM) == TTF_OK);
    }
    /* the deferred CJK tables, hanja_readings_variants.tsv (627 rows) included */
    CHECK(checked_freeze(e, 14889) == 0);
    CHECK(rules(e) == rules(base));
    for (i = 0; i < TTF_HASH_BUCKETS; ++i) {
        const ttf_entry_t *p;
        size_t previous = SIZE_MAX;
        for (p = e->cjk->words[i]; p; p = p->cjk_next) {
            CHECK(p->cjk_length <= previous);
            CHECK(!strcmp(p->kind, "cjk_word") || !strcmp(p->kind, "lexicon"));
            previous = p->cjk_length; ++indexed;
        }
    }
    /* five effective word rows + user lexicon + stress, and the 4932 dictionary
       words of cjk_words_stdict.tsv (two to six characters) since 2026-10-05 */
    CHECK(indexed == 18006 + 4932);
    CHECK(e->cjk->max_length == 6);
    CHECK(!strcmp(ttf_resource_get(e, "hanja_reading", "ko-KR", "山", NULL), "산"));
    CHECK(!strcmp(ttf_resource_get(e, "hanja_reading", "xx-Test", "山", NULL), "뫼"));
    CHECK(normalized(e, "音樂", "사용자"));
    CHECK(normalized(e, "오늘 시장이 열렸습니다.", "오늘 시장이 열렸습니다."));
    CHECK(normalized(e, "ソウル", "서울"));
    ttf_engine_destroy(e); ttf_engine_destroy(base);
    puts("18000 CJK rows: dedicated sorted index, unchanged rules, user > customer > domain > system");
    return 0;
}
