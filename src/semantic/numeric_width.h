#ifndef TTF_NUMERIC_WIDTH_H
#define TTF_NUMERIC_WIDTH_H
#include "../protected/protected.h"
/* Request-local scanner input; only U+FF10..U+FF19 outside protected regions
   fold. Byte maps retain original source ownership/offsets. No NFKC or policy. */
typedef struct {
    char *text;
    size_t length;
    size_t *raw, *scan;
    ttf_token_t *tokens;
    ttf_protected_region_t *regions;
} ttf_numeric_width_t;
ttf_status_t ttf_numeric_width_create(const char *text, size_t length,
                                       const ttf_token_t *tokens, size_t count,
                                       const ttf_protected_region_t *regions, size_t region_count,
                                       ttf_numeric_width_t *view);
void ttf_numeric_width_free(ttf_numeric_width_t *view);
/* Verbalizers which consume surface digits rather than only canonical IR. */
size_t ttf_numeric_width_guard(const ttf_pack_input_t *input, ttf_semantic_t *semantic,
                               size_t consumed);
int ttf_numeric_width_surface(ttf_semantic_type_t type);
#endif
