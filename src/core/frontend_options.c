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
         options->unreadable_policy != TTF_UNREADABLE_DROP) ||
        !options->max_control_bytes)
        return TTF_EINVAL;
    if (engine->frozen || engine->validating)
        return TTF_ESTATE;
    engine->frontend_options = *options;
    return TTF_OK;
}
