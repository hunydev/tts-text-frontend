#ifndef TTF_INTERNAL_H
#define TTF_INTERNAL_H
#include "tts_text_frontend/ttf_language_pack.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#define TTF_MAX_PACKS 32
#define TTF_HASH_BUCKETS 1021

typedef struct ttf_entry {
    char *kind, *lang, *key, *value;
    ttf_resource_layer_t layer;
    size_t order;
    struct ttf_entry *next;
} ttf_entry_t;
struct ttf_engine {
    ttf_config_t config;
    char *resource_root, *default_language;
    int frozen;
    const ttf_language_pack_t *packs[TTF_MAX_PACKS];
    size_t pack_count, resource_order;
    ttf_entry_t *resources[TTF_HASH_BUCKETS];
};
char *ttf_strdup(const char *s);
char *ttf_strndup(const char *s, size_t n);
int ttf_ascii_equal(const char *a, const char *b);
int ttf_utf8_decode(const char *s, size_t n, uint32_t *cp, size_t *width);
ttf_script_t ttf_script_of(uint32_t cp);
ttf_status_t ttf_resource_set(ttf_engine_t *e, const char *kind, const char *lang, const char *key,
                              const char *value, ttf_resource_layer_t layer);
void ttf_resource_destroy(ttf_engine_t *e);
const ttf_language_pack_t *ttf_find_pack(const ttf_engine_t *e, const char *tag);
ttf_status_t ttf_register_builtins(ttf_engine_t *e);
/* Generic semantic recognition supplied by semantic/classifier.c. */
size_t ttf_classify_universal(const ttf_pack_input_t *input, ttf_semantic_t *out);
/* Normalize one stable context unit; shared by batch and streaming. */
ttf_status_t ttf_process_unit(const ttf_engine_t *e, const char *text, size_t length,
                              size_t input_offset, ttf_result_t *out);
/* Shared lexical boundary planner. 0 means more lookahead is required. */
size_t ttf_next_boundary(const ttf_engine_t *e, const char *text, size_t length, int final,
                         int first);
#endif
