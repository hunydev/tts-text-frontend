#ifndef TTS_TEXT_FRONTEND_OPTIONS_H
#define TTS_TEXT_FRONTEND_OPTIONS_H
#include "ttf.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { TTF_INPUT_PLAIN = 0, TTF_INPUT_ANNOTATED = 1 } ttf_input_format_t;
typedef enum { TTF_OUTPUT_TEXT = 0, TTF_OUTPUT_SPEECH = 1 } ttf_output_mode_t;
/* READ keeps the semantic refusal but lets the final speech gate say the text,
   character by character, so every segment is sayable. Text mode is unaffected. */
typedef enum {
    TTF_UNREADABLE_PRESERVE = 0,
    TTF_UNREADABLE_DROP = 1,
    TTF_UNREADABLE_READ = 2
} ttf_unreadable_policy_t;
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

/* Opt-in Korean orthographic readings. Independent defaults preserve input. */
typedef enum { TTF_HANJA_PASSTHROUGH = 0, TTF_HANJA_KOREAN = 1 } ttf_hanja_mode_t;
typedef enum { TTF_KANA_PASSTHROUGH = 0, TTF_KANA_KOREAN = 1 } ttf_kana_mode_t;
/* Sentence language (v0.24): decided per sentence, fixed to the configured
   default language, or the pre-v0.24 per-token scoring only. */
typedef enum { TTF_MATRIX_AUTO = 0, TTF_MATRIX_LEGACY = 1, TTF_MATRIX_DECLARED = 2 } ttf_matrix_policy_t;
/* Unicode spoken fallback (v0.24): name a symbol or emoji no rule reads, from
   the language's files, then (on) from the English files; or never (off). */
typedef enum {
    TTF_SYMBOL_FALLBACK_ON = 0,
    TTF_SYMBOL_FALLBACK_NATIVE = 1,
    TTF_SYMBOL_FALLBACK_OFF = 2
} ttf_symbol_fallback_policy_t;

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
    ttf_hanja_mode_t hanja;
    ttf_kana_mode_t kana;
} ttf_frontend_options_t;

/* Additional named policies preserve older callers and their defaults. Apply
   before engine freeze. Unknown names and values return TTF_EINVAL.
   Currently: "brackets" = read|skip|preserve,
   "unknown_foreign" = guess|spell|preserve, "decorative" = follow|read,
   "initialism" = spell|word, "spacing" = orthographic|reading-unit (Korean
   number spacing: resource default, NIKL article 44; or a space after every
   Sino unit and one word per digit in digit readings), "matrix" =
   auto|legacy|declared (sentence language: auto decides each sentence from its
   words and attached grammar; declared uses the default language for every
   sentence; legacy is the pre-v0.24 per-token routing). An English sentence
   leaves words of another script as written. "symbol_fallback" = on|native|off
   (speech: a symbol or emoji sequence no rule reads is named from
   <resources>/unicode-fallback/<language>.idx/.dat/.seq; on also uses the
   English files; off keeps the unreadable policy), "particle_agreement" =
   off|on (Korean particle after a reading: keep it as written, or agree it with
   the reading), "numeric_fallback" = read|preserve (a number the grammar
   rejects is read as spoken, or kept), "invalid_control" = ignore|error (an
   unknown control is dropped, or the call fails), "pronunciation" =
   orthographic|hangul (hangul writes Korean as it is pronounced, colloquial and
   without vowel length: 국물이 궁무리; see docs/korean-phoneme.ko.md),
   "foreign_sentence" = keep|localize (speech: keep an English sentence's
   reading in English, or write that reading in the default language's
   letters through its loanword readings: I have 3 apples 아이 해브 쓰리 애플스;
   see docs/foreign-sentence.ko.md). */
TTF_API ttf_status_t ttf_engine_set_option(ttf_engine_t *engine, const char *name,
                                           const char *value);

/* The historical symbol initializes ONLY the v0.20 prefix, including its size.
   This is essential for already-compiled callers with a smaller allocation.
   New headers select the sized entry point, also available to FFI callers. */
#define TTF_FRONTEND_OPTIONS_V020_SIZE offsetof(ttf_frontend_options_t, hanja)
TTF_API void ttf_frontend_options_init(ttf_frontend_options_t *options);
TTF_API void ttf_frontend_options_init_sized(ttf_frontend_options_t *options, size_t size);
#ifndef TTF_FRONTEND_LEGACY_INIT
#define ttf_frontend_options_init(options) \
    ttf_frontend_options_init_sized((options), sizeof(ttf_frontend_options_t))
#endif
/* Accept v0.20..current sizes, default incomplete/missing trailing fields,
   reject larger or incompatible versions. All calls remain ABI version 1. */
TTF_API ttf_status_t ttf_engine_set_frontend_options(ttf_engine_t *engine,
                                                     const ttf_frontend_options_t *options);

#ifdef __cplusplus
}
#endif
#endif
