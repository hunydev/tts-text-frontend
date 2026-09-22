#ifndef TTS_TEXT_FRONTEND_OPTIONS_H
#define TTS_TEXT_FRONTEND_OPTIONS_H
#include "ttf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TTF_INPUT_PLAIN = 0, TTF_INPUT_ANNOTATED = 1 } ttf_input_format_t;
typedef enum { TTF_OUTPUT_TEXT = 0, TTF_OUTPUT_SPEECH = 1 } ttf_output_mode_t;
typedef enum { TTF_UNREADABLE_PRESERVE = 0, TTF_UNREADABLE_DROP = 1 } ttf_unreadable_policy_t;

/* Additive options: the ABI v1 config, result and language-pack layouts stay
   unchanged. Initialize before editing fields. Apply before engine freeze.
   Legacy engine_create uses plain/text; this initializer selects plain/speech
   with unrecognized characters preserved. Protected literal regions remain
   explicit preservation contracts in both modes. */
typedef struct {
    uint32_t struct_size, abi_version;
    ttf_input_format_t input_format;
    ttf_output_mode_t output_mode;
    ttf_unreadable_policy_t unreadable_policy;
    size_t max_control_bytes;
} ttf_frontend_options_t;

TTF_API void ttf_frontend_options_init(ttf_frontend_options_t *options);
TTF_API ttf_status_t ttf_engine_set_frontend_options(ttf_engine_t *engine,
                                                     const ttf_frontend_options_t *options);

#ifdef __cplusplus
}
#endif
#endif
