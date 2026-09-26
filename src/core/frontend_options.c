#include "internal.h"

void ttf_frontend_options_init(ttf_frontend_options_t *options) {
    if (!options)
        return;
    memset(options, 0, sizeof(*options));
    options->struct_size = sizeof(*options);
    options->abi_version = TTF_ABI_VERSION;
    options->output_mode = TTF_OUTPUT_SPEECH;
    options->unreadable_policy = TTF_UNREADABLE_PRESERVE;
    options->max_control_bytes = 4096;
}

ttf_status_t ttf_engine_set_frontend_options(ttf_engine_t *engine,
                                             const ttf_frontend_options_t *options) {
    if (!engine || !options || options->struct_size != sizeof(*options) ||
        options->abi_version != TTF_ABI_VERSION ||
        (options->input_format != TTF_INPUT_PLAIN &&
         options->input_format != TTF_INPUT_ANNOTATED) ||
        (options->output_mode != TTF_OUTPUT_TEXT && options->output_mode != TTF_OUTPUT_SPEECH) ||
        (options->unreadable_policy != TTF_UNREADABLE_PRESERVE &&
         options->unreadable_policy != TTF_UNREADABLE_DROP &&
         options->unreadable_policy != TTF_UNREADABLE_READ) ||
        !options->max_control_bytes)
        return TTF_EINVAL;
    if (engine->frozen || engine->validating)
        return TTF_ESTATE;
    engine->frontend_options = *options;
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
    if (!strcmp(name, "decorative")) {
        if (!strcmp(value, "follow"))
            engine->decorative_policy = TTF_DECORATIVE_FOLLOW;
        else if (!strcmp(value, "read"))
            engine->decorative_policy = TTF_DECORATIVE_READ;
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
