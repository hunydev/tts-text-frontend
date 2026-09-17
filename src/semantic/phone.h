#ifndef TTF_SEMANTIC_PHONE_H
#define TTF_SEMANTIC_PHONE_H
#include "../core/internal.h"
/* A recognized malformed phone is one TEXT span; zero means another grammar owns it. */
size_t ttf_classify_phone(const ttf_pack_input_t *input, ttf_semantic_t *semantic);
/* The caller supplies only bytes through the candidate cut. Token ends are clipped
   to prefix_length. No decision reads later bytes in the available input buffer. */
int ttf_phone_boundary_pending(const ttf_engine_t *engine, const char *text, size_t prefix_length,
                               const ttf_token_t *tokens, size_t token_count);
/* Labels are not part of the numeric span. NULL language searches registered packs
   plus '*'; a specific language uses that language and wildcard fallback only.
   A longer disabled cue overrides its enabled suffix. */
int ttf_phone_preceding_label(const ttf_pack_input_t *input, size_t begin, const char *kind,
                              const char *language, char matched[TTF_IR_VALUE_SIZE],
                              ttf_resource_layer_t *layer);
#endif
