#include "../../src/core/internal.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)
static int put(const char *root, const char *name, const char *data) {
    char path[4096];
    FILE *f;
    if (snprintf(path, sizeof(path), "%s/%s", root, name) >= (int)sizeof(path)) return 0;
    f = fopen(path, "wb");
    if (!f) return 0;
    if (fwrite(data, 1, strlen(data), f) != strlen(data)) { fclose(f); return 0; }
    return fclose(f) == 0;
}
static size_t records(ttf_engine_t *e) {
    ttf_resource_validation_report_t r;
    ttf_resource_validation_report_init(&r);
    if (ttf_engine_validate_resources(e, &r, NULL) != TTF_OK) return SIZE_MAX;
    return r.records;
}
static ttf_status_t reject_late(const ttf_engine_t *e, ttf_resource_diagnostic_t *d, void *data) {
    (void)d;
    return *(int *)data && ttf_resource_get(e, "kana_reading", "ko-KR", "か", NULL)
        ? TTF_ERESOURCE : TTF_OK;
}
static int create(const char *root, int mode, ttf_engine_t **e) {
    ttf_config_t c;
    ttf_frontend_options_t o;
    ttf_config_init(&c); c.resource_root = root;
    CHECK(ttf_engine_create(&c, e) == TTF_OK);
    /* mode -1 models a legacy caller that never sets frontend options. */
    if (mode >= 0) {
        ttf_frontend_options_init(&o);
        o.hanja = (mode & 1) ? TTF_HANJA_KOREAN : TTF_HANJA_PASSTHROUGH;
        o.kana = (mode & 2) ? TTF_KANA_KOREAN : TTF_KANA_PASSTHROUGH;
        CHECK(ttf_engine_set_frontend_options(*e, &o) == TTF_OK);
    }
    return 0;
}
int main(int argc, char **argv) {
    const char *han = "languages/ko-KR/cjk/hanja_readings.tsv";
    const char *kana = "languages/ko-KR/cjk/kana_readings.tsv";
    const char *word = "languages/ko-KR/cjk/cjk_words.tsv";
    const char *rules = "languages/ko-KR/cjk/hanja_rules.tsv";
    const char *valid_rules = "hanja_soft_after\tko-KR\t률\t율\n";
    const char *stdict = "languages/ko-KR/cjk/hanja_readings_stdict.tsv";
    const char *valid_stdict = "hanja_reading\tko-KR\t\xe5\xbc\xb7\t강\n"; /* U+5F37 */
    const char *valid_han = "hanja_reading\tko-KR\t山\t산\n";
    const char *valid_kana = "kana_reading\tko-KR\tか\t카\n";
    const char *valid_word = "cjk_word\tko-KR\t音樂\t음악\n";
    ttf_engine_t *e = NULL;
    int mode, reject = 1;
    ttf_resource_validator_t validator = {sizeof(validator), TTF_RESOURCE_VALIDATOR_ABI_VERSION,
        "reject-late", &reject, NULL, reject_late};
    size_t checkpoint, base_records;
    const ttf_entry_t *original;
    CHECK(argc == 3);
    CHECK(create(argv[1], -1, &e) == 0);
    CHECK(!ttf_resource_get(e, "hanja_reading", "ko-KR", "山", NULL));
    CHECK(!ttf_resource_get(e, "kana_reading", "ko-KR", "か", NULL));
    base_records = records(e);
    CHECK(base_records != SIZE_MAX && base_records > 0);
    ttf_engine_destroy(e);
    /* Missing files, then invalid files: disabled paths must not be opened. */
    for (mode = -1; mode <= 0; ++mode) {
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", argv[2], han); remove(path);
        snprintf(path, sizeof(path), "%s/%s", argv[2], kana); remove(path);
        snprintf(path, sizeof(path), "%s/%s", argv[2], word); remove(path);
        snprintf(path, sizeof(path), "%s/%s", argv[2], rules); remove(path);
        snprintf(path, sizeof(path), "%s/%s", argv[2], stdict); remove(path);
        CHECK(create(argv[2], mode, &e) == 0);
        CHECK(ttf_engine_freeze(e) == TTF_OK && records(e) == base_records);
        ttf_engine_destroy(e);
    }
    for (mode = 0; mode < 4; ++mode) {
        CHECK(put(argv[2], han, mode & 1 ? valid_han : "invalid\n"));
        CHECK(put(argv[2], kana, mode & 2 ? valid_kana : "invalid\n"));
        CHECK(put(argv[2], word, mode ? valid_word : "invalid\n"));
        CHECK(put(argv[2], rules, mode & 1 ? valid_rules : "invalid\n"));
        CHECK(put(argv[2], stdict, mode & 1 ? valid_stdict : "invalid\n"));
        CHECK(create(argv[2], mode, &e) == 0);
        CHECK(records(e) == base_records);
        CHECK(ttf_engine_freeze(e) == TTF_OK);
        CHECK(records(e) == base_records + (size_t)(3 * (!!(mode & 1)) + (!!(mode & 2)) + !!mode));
        CHECK(!!ttf_resource_get(e, "hanja_reading", "ko-KR", "\xe5\xbc\xb7", NULL) == !!(mode & 1));
        CHECK(!!ttf_resource_get(e, "hanja_reading", "ko-KR", "山", NULL) == !!(mode & 1));
        CHECK(!!ttf_resource_get(e, "hanja_soft_after", "ko-KR", "률", NULL) == !!(mode & 1));
        CHECK(!!ttf_resource_get(e, "kana_reading", "ko-KR", "か", NULL) == !!(mode & 2));
        CHECK(!!ttf_resource_get(e, "cjk_word", "ko-KR", "音樂", NULL) == !!mode);
        CHECK(ttf_resource_load(e, "missing", TTF_LAYER_SYSTEM) == TTF_ESTATE);
        checkpoint = e->resource_order;
        CHECK(ttf_engine_freeze(e) == TTF_OK && checkpoint == e->resource_order);
        ttf_engine_destroy(e);
    }
    /* The word table fails after two successful loads (the rule file loads
       after it). Retain explicit rows. */
    CHECK(put(argv[2], rules, valid_rules) && put(argv[2], stdict, valid_stdict));
    CHECK(put(argv[2], han, valid_han) && put(argv[2], kana, valid_kana));
    CHECK(put(argv[2], word, "cjk_word\tko-KR\t音樂\tUNK\n"));
    CHECK(create(argv[2], 3, &e) == 0);
    CHECK(ttf_resource_set(e, "hanja_reading", "ko-KR", "山", "뫼", TTF_LAYER_SYSTEM) == TTF_OK);
    CHECK(ttf_resource_set(e, "cjk_word", "ko-KR", "音樂", "고객", TTF_LAYER_CUSTOMER) == TTF_OK);
    original = ttf_resource_entry(e, "hanja_reading", "ko-KR", "山");
    checkpoint = e->resource_order;
    CHECK(ttf_engine_freeze(e) == TTF_ERESOURCE);
    CHECK(!e->frozen && !e->validating && !e->cjk && !e->rules && !e->boundary_cues);
    CHECK(records(e) == base_records + 2 && checkpoint == e->resource_order);
    CHECK(ttf_resource_entry(e, "hanja_reading", "ko-KR", "山") == original);
    CHECK(!ttf_resource_get(e, "kana_reading", "ko-KR", "か", NULL));
    CHECK(put(argv[2], word, valid_word));
    CHECK(ttf_engine_register_resource_validator(e, &validator) == TTF_OK);
    CHECK(records(e) == base_records + 2); /* final engine validator sees no deferred rows yet */
    CHECK(ttf_engine_freeze(e) == TTF_ERESOURCE);
    CHECK(records(e) == base_records + 2 && checkpoint == e->resource_order && !e->cjk && !e->frozen);
    reject = 0;
    CHECK(ttf_engine_freeze(e) == TTF_OK && records(e) == base_records + 7);
    CHECK(e->resource_order == checkpoint + 5);
    CHECK(ttf_resource_entry(e, "hanja_reading", "ko-KR", "山") == original);
    CHECK(!strcmp(ttf_resource_get(e, "cjk_word", "ko-KR", "音樂", NULL), "고객"));
    ttf_engine_destroy(e);
    /* Production manifest counts, both explicit and compiled-in NULL roots. */
    for (mode = -1; mode < 4; ++mode) {
        size_t added = mode <= 0 ? 0 : (mode & 1 ? 9024 : 0) + (mode & 2 ? 301 : 0) + 5;
        CHECK(create(mode == 3 ? NULL : argv[1], mode, &e) == 0);
        CHECK(records(e) == base_records);
        CHECK(ttf_engine_freeze(e) == TTF_OK && records(e) == base_records + added);
        printf("mode %d: %zu -> %zu records\n", mode, base_records, records(e));
        ttf_engine_destroy(e);
    }
    puts("lazy CJK: disabled paths, four modes, NULL root, rollback/retry, validators, priority passed");
    return 0;
}
