#ifndef TTF_KO_IDENTIFIERS_H
#define TTF_KO_IDENTIFIERS_H
#include "tts_text_frontend/ttf_language_pack.h"

/* Explicit resource labels identify a digit string; this does not validate an
 * account number, institution, checksum or any real-world identifier. */
size_t ttf_ko_classify_identifier(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
size_t ttf_ko_classify_digit_identifier(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
/* A letter+digit code next to Korean text (K37PA1798, M4137번, C5135-12).
 * Speech output only; the caller checks the output mode. */
size_t ttf_ko_classify_number_letters(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
size_t ttf_ko_classify_clothing_size(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
size_t ttf_ko_classify_apparel_size(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
size_t ttf_ko_classify_plate(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
size_t ttf_ko_classify_card(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
size_t ttf_ko_classify_alnum_identifier(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
ttf_status_t ttf_ko_render_identifier(const ttf_engine_t *engine, const char *value,
                                      ttf_buffer_t *output);
#endif
