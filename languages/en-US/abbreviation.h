#ifndef TTF_EN_ABBREVIATION_H
#define TTF_EN_ABBREVIATION_H
#include "tts_text_frontend/ttf_language_pack.h"
size_t ttf_en_classify_abbreviation(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
void ttf_en_initialism_policy(const ttf_engine_t *engine, ttf_semantic_t *semantic,
                              const char *surface, size_t length, const char *kind);
ttf_status_t ttf_en_verbalize_abbreviation(const ttf_engine_t *engine,
                                           const ttf_semantic_t *semantic,
                                           const char *surface, size_t length,
                                           ttf_buffer_t *output);
#endif
