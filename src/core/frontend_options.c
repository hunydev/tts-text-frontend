#define TTF_FRONTEND_LEGACY_INIT
#include "internal.h"

void ttf_frontend_options_init_sized(ttf_frontend_options_t *options, size_t size) {
    ttf_frontend_options_t defaults = {0};
    if (!options || size < TTF_FRONTEND_OPTIONS_V020_SIZE || size > sizeof(defaults))
        return;
    defaults.struct_size = (uint32_t)size;
    defaults.abi_version = TTF_ABI_VERSION;
    defaults.output_mode = TTF_OUTPUT_SPEECH;
    defaults.unreadable_policy = TTF_UNREADABLE_PRESERVE;
    defaults.max_control_bytes = 4096;
    memcpy(options, &defaults, size);
}

void ttf_frontend_options_init(ttf_frontend_options_t *options) {
    ttf_frontend_options_init_sized(options, TTF_FRONTEND_OPTIONS_V020_SIZE);
}

ttf_status_t ttf_engine_set_frontend_options(ttf_engine_t *engine,
                                             const ttf_frontend_options_t *options) {
    ttf_frontend_options_t value;
    if (!engine || !options || options->struct_size < TTF_FRONTEND_OPTIONS_V020_SIZE ||
        options->struct_size > sizeof(*options) || options->abi_version != TTF_ABI_VERSION)
        return TTF_EINVAL;
    ttf_frontend_options_init_sized(&value, sizeof(value));
    memcpy(&value, options, TTF_FRONTEND_OPTIONS_V020_SIZE);
    value.struct_size = sizeof(value);
    if (options->struct_size >= offsetof(ttf_frontend_options_t, hanja) + sizeof(value.hanja))
        value.hanja = options->hanja;
    if (options->struct_size >= offsetof(ttf_frontend_options_t, kana) + sizeof(value.kana))
        value.kana = options->kana;
    if ((value.input_format != TTF_INPUT_PLAIN && value.input_format != TTF_INPUT_ANNOTATED) ||
        (value.output_mode != TTF_OUTPUT_TEXT && value.output_mode != TTF_OUTPUT_SPEECH) ||
        (value.unreadable_policy != TTF_UNREADABLE_PRESERVE &&
         value.unreadable_policy != TTF_UNREADABLE_DROP &&
         value.unreadable_policy != TTF_UNREADABLE_READ) || !value.max_control_bytes ||
        (value.hanja != TTF_HANJA_PASSTHROUGH && value.hanja != TTF_HANJA_KOREAN) ||
        (value.kana != TTF_KANA_PASSTHROUGH && value.kana != TTF_KANA_KOREAN))
        return TTF_EINVAL;
    if (engine->frozen || engine->validating)
        return TTF_ESTATE;
    engine->frontend_options = value;
    return TTF_OK;
}

ttf_status_t ttf_engine_set_option(ttf_engine_t *engine, const char *name, const char *value) {
    if (!engine || !name || !value)
        return TTF_EINVAL;
    if (engine->frozen || engine->validating)
        return TTF_ESTATE;
    if (!strcmp(name, "brackets")) {
        if (!strcmp(value, "read"))
            engine->bracket_policy = TTF_BRACKET_READ;
        else if (!strcmp(value, "skip"))
            engine->bracket_policy = TTF_BRACKET_SKIP;
        else if (!strcmp(value, "preserve"))
            engine->bracket_policy = TTF_BRACKET_PRESERVE;
        else
            return TTF_EINVAL;
        return TTF_OK;
    }
    if (!strcmp(name, "initialism")) {
        if (!strcmp(value, "spell"))
            engine->initialism_policy = TTF_INITIALISM_SPELL;
        else if (!strcmp(value, "word"))
            engine->initialism_policy = TTF_INITIALISM_WORD;
        else
            return TTF_EINVAL;
        return TTF_OK;
    }
    if (!strcmp(name, "spacing")) {
        if (!strcmp(value, "orthographic"))
            engine->reading_unit_spacing = 0;
        else if (!strcmp(value, "reading-unit"))
            engine->reading_unit_spacing = 1;
        else
            return TTF_EINVAL;
        return TTF_OK;
    }
    if (!strcmp(name, "pronunciation")) {
        if (!strcmp(value, "orthographic"))
            engine->phoneme_output = 0;
        else if (!strcmp(value, "hangul"))
            engine->phoneme_output = 1;
        else
            return TTF_EINVAL;
        return TTF_OK;
    }
    if (!strcmp(name, "symbol_fallback")) {
        if (!strcmp(value, "on"))
            engine->symbol_fallback_policy = TTF_SYMBOL_FALLBACK_ON;
        else if (!strcmp(value, "native"))
            engine->symbol_fallback_policy = TTF_SYMBOL_FALLBACK_NATIVE;
        else if (!strcmp(value, "off"))
            engine->symbol_fallback_policy = TTF_SYMBOL_FALLBACK_OFF;
        else
            return TTF_EINVAL;
        return TTF_OK;
    }
    if (!strcmp(name, "matrix")) {
        if (!strcmp(value, "auto"))
            engine->matrix_policy = TTF_MATRIX_AUTO;
        else if (!strcmp(value, "legacy"))
            engine->matrix_policy = TTF_MATRIX_LEGACY;
        else if (!strcmp(value, "declared"))
            engine->matrix_policy = TTF_MATRIX_DECLARED;
        else
            return TTF_EINVAL;
        return TTF_OK;
    }
    if (!strcmp(name, "decorative")) {
        if (!strcmp(value, "follow"))
            engine->decorative_policy = TTF_DECORATIVE_FOLLOW;
        else if (!strcmp(value, "read"))
            engine->decorative_policy = TTF_DECORATIVE_READ;
        else
            return TTF_EINVAL;
        return TTF_OK;
    }
    if (!strcmp(name, "particle_agreement")) {
        if (!strcmp(value, "off"))
            engine->particle_option = 1;
        else if (!strcmp(value, "on"))
            engine->particle_option = 2;
        else
            return TTF_EINVAL;
        return TTF_OK;
    }
    if (!strcmp(name, "numeric_fallback")) {
        if (!strcmp(value, "read"))
            engine->numeric_fallback_off = 0;
        else if (!strcmp(value, "preserve"))
            engine->numeric_fallback_off = 1;
        else
            return TTF_EINVAL;
        return TTF_OK;
    }
    if (!strcmp(name, "invalid_control")) {
        if (!strcmp(value, "ignore"))
            engine->strict_controls = 0;
        else if (!strcmp(value, "error"))
            engine->strict_controls = 1;
        else
            return TTF_EINVAL;
        return TTF_OK;
    }
    if (!strcmp(name, "unknown_foreign")) {
        if (!strcmp(value, "guess"))
            engine->unknown_foreign_policy = TTF_UNKNOWN_FOREIGN_GUESS;
        else if (!strcmp(value, "spell"))
            engine->unknown_foreign_policy = TTF_UNKNOWN_FOREIGN_SPELL;
        else if (!strcmp(value, "preserve"))
            engine->unknown_foreign_policy = TTF_UNKNOWN_FOREIGN_PRESERVE;
        else
            return TTF_EINVAL;
        return TTF_OK;
    }
    return TTF_EINVAL;
}
