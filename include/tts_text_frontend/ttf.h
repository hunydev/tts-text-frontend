#ifndef TTS_TEXT_FRONTEND_TTF_H
#define TTS_TEXT_FRONTEND_TTF_H
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32) && defined(TTF_SHARED)
#if defined(TTF_BUILDING)
#define TTF_API __declspec(dllexport)
#else
#define TTF_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define TTF_API __attribute__((visibility("default")))
#else
#define TTF_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define TTF_ABI_VERSION 1u
#define TTF_LANGUAGE_TAG_SIZE 24
#define TTF_IR_VALUE_SIZE 96

typedef struct ttf_engine ttf_engine_t;
typedef struct ttf_stream ttf_stream_t;
typedef enum {
    TTF_OK = 0,
    TTF_EINVAL,
    TTF_ENOMEM,
    TTF_EUTF8,
    TTF_EIO,
    TTF_ERESOURCE,
    TTF_ESTATE,
    TTF_ELIMIT,
    TTF_ECALLBACK
} ttf_status_t;
typedef enum {
    TTF_SCRIPT_HANGUL,
    TTF_SCRIPT_LATIN,
    TTF_SCRIPT_DIGIT,
    TTF_SCRIPT_WHITESPACE,
    TTF_SCRIPT_PUNCTUATION,
    TTF_SCRIPT_SYMBOL,
    TTF_SCRIPT_OTHER
} ttf_script_t;
typedef enum {
    TTF_SEM_TEXT,
    TTF_SEM_WORD,
    TTF_SEM_NUMBER,
    TTF_SEM_CARDINAL,
    TTF_SEM_ORDINAL,
    TTF_SEM_DIGIT_SEQUENCE,
    TTF_SEM_COUNTER,
    TTF_SEM_DATE,
    TTF_SEM_TIME,
    TTF_SEM_DATETIME,
    TTF_SEM_PHONE,
    TTF_SEM_CURRENCY,
    TTF_SEM_PERCENT,
    TTF_SEM_MEASURE,
    TTF_SEM_FRACTION,
    TTF_SEM_RANGE,
    TTF_SEM_VERSION,
    TTF_SEM_IP_ADDRESS,
    TTF_SEM_URL,
    TTF_SEM_EMAIL,
    TTF_SEM_ABBREVIATION,
    TTF_SEM_INITIALISM,
    TTF_SEM_ACRONYM,
    TTF_SEM_ALNUM_TERM,
    TTF_SEM_SYMBOL,
    TTF_SEM_CUSTOM
} ttf_semantic_type_t;
typedef enum {
    TTF_LEX_NONE,
    TTF_LEX_WORD,
    TTF_LEX_PROPER_NOUN,
    TTF_LEX_INITIALISM,
    TTF_LEX_ACRONYM,
    TTF_LEX_ALNUM_TERM,
    TTF_LEX_UNKNOWN_LATIN
} ttf_lexical_class_t;
typedef enum {
    TTF_LAYER_COMMON = 0,
    TTF_LAYER_SYSTEM = 1,
    TTF_LAYER_DOMAIN = 2,
    TTF_LAYER_CUSTOMER = 3,
    TTF_LAYER_USER = 4
} ttf_resource_layer_t;
typedef enum {
    TTF_PRESERVE_LANGUAGE,
    TTF_PHONETIC_LOCALIZE,
    TTF_SEMANTIC_LOCALIZE
} ttf_render_policy_t;
typedef struct {
    size_t begin, end;
    ttf_script_t script;
} ttf_token_t;
typedef struct {
    uint32_t struct_size;
    ttf_semantic_type_t type;
    ttf_lexical_class_t lexical_class;
    size_t input_begin, input_end;
    char value[TTF_IR_VALUE_SIZE];
    char secondary[TTF_IR_VALUE_SIZE];
    char unit[32];
    char source_lang[TTF_LANGUAGE_TAG_SIZE];
    char context_lang[TTF_LANGUAGE_TAG_SIZE];
    char render_lang[TTF_LANGUAGE_TAG_SIZE];
    char rule[64];
    ttf_resource_layer_t layer;
} ttf_semantic_t;
typedef struct {
    size_t begin, end;             /* UTF-8 byte offsets in normalized text */
    size_t input_begin, input_end; /* UTF-8 byte offsets in original input */
    ttf_script_t script;
    ttf_semantic_t semantic;
    char context_trace[192]; /* deterministic per-pack context scores */
    char lexicon[64];
    ttf_resource_layer_t pronunciation_layer; /* independent of semantic rule layer */
    char verbalizer[TTF_LANGUAGE_TAG_SIZE];
} ttf_span_t;
typedef struct {
    char *text;
    size_t length;
    ttf_span_t *spans;
    size_t span_count;
} ttf_result_t;
typedef struct {
    uint32_t struct_size, abi_version;
    const char
        *resource_root; /* NULL uses compiled install default; CLI also supports --resources */
    const char *default_language;
    ttf_render_policy_t render_policy;
    size_t first_segment_target, normal_segment_target, max_segment_length;
    size_t max_pending_bytes; /* hard limit for an unbreakable token */
    int sentence_split, phrase_split;
} ttf_config_t;
typedef struct {
    const char *text;
    size_t length;
    const ttf_span_t *spans;
    size_t span_count;
    size_t input_begin, input_end;
} ttf_segment_t;
/* Callback data are valid only during the call. Nonzero return cancels the stream. */
typedef int (*ttf_callback_fn)(const ttf_segment_t *segment, void *user_data);
TTF_API void ttf_config_init(ttf_config_t *config);
TTF_API ttf_status_t ttf_engine_create(const ttf_config_t *config, ttf_engine_t **out);
TTF_API ttf_status_t ttf_engine_freeze(ttf_engine_t *engine);
TTF_API void ttf_engine_destroy(ttf_engine_t *engine);
TTF_API ttf_status_t ttf_normalize(const ttf_engine_t *engine, const char *text, size_t length,
                                   ttf_result_t *out);
TTF_API void ttf_result_free(ttf_result_t *result);
TTF_API ttf_status_t ttf_resource_load(ttf_engine_t *engine, const char *path,
                                       ttf_resource_layer_t layer);
TTF_API ttf_status_t ttf_lexicon_add(ttf_engine_t *engine, ttf_resource_layer_t layer,
                                     const char *source_lang, const char *render_lang,
                                     const char *word, const char *pronunciation);
TTF_API ttf_status_t ttf_lexicon_remove(ttf_engine_t *engine, ttf_resource_layer_t layer,
                                        const char *source_lang, const char *render_lang,
                                        const char *word);
TTF_API ttf_status_t ttf_stream_create(const ttf_engine_t *engine, ttf_callback_fn callback,
                                       void *user_data, ttf_stream_t **out);
TTF_API ttf_status_t ttf_stream_write(ttf_stream_t *stream, const char *data, size_t length);
TTF_API ttf_status_t ttf_stream_flush(ttf_stream_t *stream);
TTF_API void ttf_stream_destroy(ttf_stream_t *stream);
TTF_API ttf_status_t ttf_tokenize(const char *text, size_t length, ttf_token_t **tokens,
                                  size_t *count);
TTF_API void ttf_tokens_free(ttf_token_t *tokens);
TTF_API const char *ttf_status_string(ttf_status_t status);
TTF_API const char *ttf_semantic_type_name(ttf_semantic_type_t type);
TTF_API const char *ttf_script_name(ttf_script_t script);
TTF_API const char *ttf_layer_name(ttf_resource_layer_t layer);
#ifdef __cplusplus
}
#endif
#endif
