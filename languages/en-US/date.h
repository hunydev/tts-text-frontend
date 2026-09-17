#ifndef TTF_EN_DATE_H
#define TTF_EN_DATE_H

#include "tts_text_frontend/ttf_language_pack.h"

/* Internal pack grammar; public ABI remains the language-pack callback. */
size_t ttf_en_classify_date(const ttf_pack_input_t *input, ttf_semantic_t *semantic);

#endif
