#ifndef TTS_TEXT_FRONTEND_OPTIONS_H
#define TTS_TEXT_FRONTEND_OPTIONS_H
#include "ttf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TTF_INPUT_PLAIN = 0, TTF_INPUT_ANNOTATED = 1 } ttf_input_format_t;
typedef enum { TTF_OUTPUT_TEXT = 0, TTF_OUTPUT_SPEECH = 1 } ttf_output_mode_t;
typedef enum { TTF_UNREADABLE_PRESERVE = 0, TTF_UNREADABLE_DROP = 1 } ttf_unreadable_policy_t;
/* Bracketed asides: read them, leave them out of the reading, or keep their
   source text unnormalized. Quotation marks are a separate scope. */
typedef enum {
    TTF_BRACKET_READ = 0,
    TTF_BRACKET_SKIP = 1,
    TTF_BRACKET_PRESERVE = 2
} ttf_bracket_policy_t;
/* A Latin word with no lexicon or dictionary entry: guess its target-language
   pronunciation, spell its letters, or keep the source text. */
typedef enum {
    TTF_UNKNOWN_FOREIGN_GUESS = 0,
    TTF_UNKNOWN_FOREIGN_SPELL = 1,
    TTF_UNKNOWN_FOREIGN_PRESERVE = 2
} ttf_unknown_foreign_policy_t;
/* Decorative marks (stars, hearts, arrows, emoji) have no reading by default.
   `read` speaks the declared name of such a character and leaves the rest to the
   unreadable policy. */
typedef enum { TTF_DECORATIVE_FOLLOW = 0, TTF_DECORATIVE_READ = 1 } ttf_decorative_policy_t;
/* An all-uppercase run of two or more letters: spell its letters even when the
   same spelling is an ordinary lowercase word, or let the word reading win.
   A pronunciation lexicon entry (NASA) outranks both. */
typedef enum { TTF_INITIALISM_SPELL = 0, TTF_INITIALISM_WORD = 1 } ttf_initialism_policy_t;

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

/* Named policies are added as functions, not as new fields, so a consumer built
   against an older header keeps its structure layout and its defaults. Apply
   before engine freeze. Unknown names and values return TTF_EINVAL.
   Currently: "brackets" = read|skip|preserve,
   "unknown_foreign" = guess|spell|preserve, "decorative" = follow|read,
   "initialism" = spell|word. */
TTF_API ttf_status_t ttf_engine_set_option(ttf_engine_t *engine, const char *name,
                                           const char *value);

TTF_API void ttf_frontend_options_init(ttf_frontend_options_t *options);
TTF_API ttf_status_t ttf_engine_set_frontend_options(ttf_engine_t *engine,
                                                     const ttf_frontend_options_t *options);

#ifdef __cplusplus
}
#endif
#endif
