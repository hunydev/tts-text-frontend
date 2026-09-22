#ifndef TTF_PROTECTED_H
#define TTF_PROTECTED_H
#include "../core/internal.h"
#include "../control/control.h"

typedef struct {
    size_t end;
    const char *kind;
    const char *rule;
    ttf_resource_layer_t layer;
    int complete;
    ttf_control_match_t control; /* NONE for ordinary literal preservation. */
} ttf_protected_match_t;

int ttf_protect_markup(const ttf_engine_t *engine, const char *text, size_t length, size_t begin,
                       int final, ttf_protected_match_t *match);
int ttf_protect_path(const ttf_engine_t *engine, const char *text, size_t length, size_t begin,
                     int final, ttf_protected_match_t *match);

typedef struct {
    size_t begin;
    ttf_protected_match_t match;
} ttf_protected_region_t;
/* Internal token view: literal regions become one neutral token. Public scanner
   categories/ABI remain unchanged. Both streaming and normalization use it. */
ttf_status_t ttf_tokenize_protected(const ttf_engine_t *engine, const char *text, size_t length,
                                    int final, ttf_token_t **tokens, size_t *count,
                                    ttf_protected_region_t **regions, size_t *region_count);
#endif
