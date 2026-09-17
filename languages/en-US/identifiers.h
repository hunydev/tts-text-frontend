#ifndef TTF_EN_IDENTIFIERS_H
#define TTF_EN_IDENTIFIERS_H
#include "tts_text_frontend/ttf_language_pack.h"
/* Explicit labels identify a digit sequence, without asserting financial validity. */
size_t ttf_en_classify_identifier(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
ttf_status_t ttf_en_render_identifier(const ttf_engine_t *engine, const char *value,
                                      ttf_buffer_t *output);
#endif
