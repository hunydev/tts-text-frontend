#ifndef TTS_TEXT_FRONTEND_LANGUAGE_PACK_H
#define TTS_TEXT_FRONTEND_LANGUAGE_PACK_H
#include "ttf.h"
#ifdef __cplusplus
extern "C" {
#endif
/* A pack's descriptor, callbacks and userdata must outlive its engine. */
typedef struct {
    char *data;
    size_t length, capacity;
} ttf_buffer_t;
TTF_API ttf_status_t ttf_buffer_append(ttf_buffer_t *b, const char *data, size_t length);
TTF_API ttf_status_t ttf_buffer_puts(ttf_buffer_t *b, const char *text);
TTF_API void ttf_buffer_free(ttf_buffer_t *b);
/* Language-neutral pronunciation representation. Initial version uses a documented
   shared ASCII phone inventory, space-separated. No pair-specific core dispatch. */
typedef struct {
    char phones[1024];
    char alphabet[24];
    char provenance[64];
} ttf_pronunciation_t;
typedef struct {
    const char *text;
    size_t length;
    const ttf_token_t *tokens;
    size_t token_count, index;
    const ttf_engine_t *engine;
} ttf_pack_input_t;
typedef struct ttf_language_pack {
    uint32_t struct_size, abi_version;
    const char *language_tag;
    void *userdata;
    int (*script_score)(ttf_script_t script);
    /* Score the stable local clause. Attached particles may override inherited context. */
    int (*context_score)(const ttf_pack_input_t *input);
    /* Return token count consumed, zero for no match. Populate IR, never output text. */
    size_t (*classify)(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
    ttf_status_t (*verbalize)(const ttf_engine_t *engine, const ttf_semantic_t *semantic,
                              const char *surface, size_t length, ttf_buffer_t *output);
    ttf_status_t (*pronounce)(const ttf_engine_t *engine, const char *word, size_t length,
                              ttf_lexical_class_t lexical_class, ttf_pronunciation_t *out);
    ttf_status_t (*render_phones)(const ttf_engine_t *engine,
                                  const ttf_pronunciation_t *pronunciation, ttf_buffer_t *out);
    ttf_status_t (*render_graphemes)(const ttf_engine_t *engine, const char *word, size_t length,
                                     ttf_buffer_t *out);
} ttf_language_pack_t;
TTF_API ttf_status_t ttf_engine_register_pack(ttf_engine_t *engine,
                                              const ttf_language_pack_t *pack);
/* Resource kind/lang/key lookup. Resources are immutable after freeze. Returned
   strings are engine-owned; setup mutations may invalidate them. After freeze,
   they remain valid until engine destruction. NULL indicates miss. '*' language is fallback. */
TTF_API const char *ttf_resource_get(const ttf_engine_t *engine, const char *kind,
                                     const char *language, const char *key,
                                     ttf_resource_layer_t *layer);
#ifdef __cplusplus
}
#endif
#endif
