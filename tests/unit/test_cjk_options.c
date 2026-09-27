/* Stage 1: ABI size contracts and metadata-only script refinement. */
#define TTF_FRONTEND_LEGACY_INIT
#include "../../src/core/internal.h"
#undef NDEBUG
#include <assert.h>

typedef struct {
    uint32_t struct_size, abi_version;
    ttf_input_format_t input_format;
    ttf_output_mode_t output_mode;
    ttf_unreadable_policy_t unreadable_policy;
    size_t max_control_bytes;
} legacy_options_t;

int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_frontend_options_t current;
    struct { legacy_options_t options; unsigned char canary[16]; } old;
    ttf_token_t *tokens = NULL;
    size_t count, i;
    const char *mixed = "山カナЖ𠮷";
    const uint32_t han[] = {0x3400, 0x4dbf, 0x4e00, 0x9fff, 0xf900, 0xfaff,
        0x2f800, 0x2fa1f, 0x2e80, 0x2eff, 0x2f00, 0x2fdf, 0x20000, 0x2a6df,
        0x2a700, 0x2b73f, 0x2b740, 0x2b81f, 0x2b820, 0x2ceaf, 0x2ceb0, 0x2ebef,
        0x30000, 0x3134f, 0x31350, 0x323af};
    const uint32_t kana[] = {0x3040, 0x309f, 0x30a0, 0x30ff, 0x31f0, 0x31ff,
        0xff66, 0xff9d, 0x1b000, 0x1b16f};
    assert(argc == 2);
    assert(sizeof(legacy_options_t) == TTF_FRONTEND_OPTIONS_V020_SIZE);
    assert(TTF_SCRIPT_OTHER == 6 && TTF_SCRIPT_HAN == 7 && TTF_SCRIPT_KANA == 8);
    for (i = 0; i < sizeof(han) / sizeof(*han); ++i)
        assert(ttf_script_of(han[i]) == TTF_SCRIPT_HAN);
    for (i = 0; i < sizeof(kana) / sizeof(*kana); ++i)
        assert(ttf_script_of(kana[i]) == TTF_SCRIPT_KANA);
    assert(ttf_script_of(0x416) == TTF_SCRIPT_OTHER);
    assert(ttf_tokenize(mixed, strlen(mixed), &tokens, &count) == TTF_OK);
    assert(count == 1 && tokens[0].begin == 0 && tokens[0].end == strlen(mixed));
    assert(tokens[0].script == TTF_SCRIPT_HAN);
    ttf_tokens_free(tokens);
    ttf_config_init(&config);
    config.resource_root = argv[1];
    assert(ttf_engine_create(&config, &engine) == TTF_OK);
    memset(&old, 0xa5, sizeof(old));
    /* A binary compiled with the old header calls this historical symbol. */
    ttf_frontend_options_init((ttf_frontend_options_t *)&old.options);
    for (i = 0; i < sizeof(old.canary); ++i)
        assert(old.canary[i] == 0xa5);
    assert(old.options.struct_size == sizeof(old.options));
    assert(ttf_engine_set_frontend_options(engine, (ttf_frontend_options_t *)&old.options) == TTF_OK);
    assert(engine->frontend_options.hanja == TTF_HANJA_PASSTHROUGH);
    assert(engine->frontend_options.kana == TTF_KANA_PASSTHROUGH);
    ttf_frontend_options_init_sized(&current, sizeof(current));
    assert(current.hanja == TTF_HANJA_PASSTHROUGH && current.kana == TTF_KANA_PASSTHROUGH);
    current.hanja = TTF_HANJA_KOREAN;
    current.kana = TTF_KANA_KOREAN;
    assert(ttf_engine_set_frontend_options(engine, &current) == TTF_OK);
    assert(engine->frontend_options.hanja == TTF_HANJA_KOREAN);
    assert(engine->frontend_options.kana == TTF_KANA_KOREAN);
    current.struct_size = sizeof(current) + 1;
    assert(ttf_engine_set_frontend_options(engine, &current) == TTF_EINVAL);
    current.struct_size = TTF_FRONTEND_OPTIONS_V020_SIZE + 1;
    assert(ttf_engine_set_frontend_options(engine, &current) == TTF_OK);
    assert(engine->frontend_options.hanja == TTF_HANJA_PASSTHROUGH);
    assert(engine->frontend_options.kana == TTF_KANA_PASSTHROUGH);
    current.struct_size = sizeof(current);
    current.hanja = (ttf_hanja_mode_t)2;
    assert(ttf_engine_set_frontend_options(engine, &current) == TTF_EINVAL);
    ttf_engine_destroy(engine);
    puts("CJK stage 1: legacy/current/oversize ABI, defaults, bounds and token boundaries passed");
    return 0;
}
