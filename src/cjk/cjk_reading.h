#ifndef TTF_CJK_READING_H
#define TTF_CJK_READING_H
#include "../core/internal.h"

/* Called only for an opt-in, unclassified lexical run. No language pack claim:
   source stays und; these resources render Korean orthography directly. */
int ttf_cjk_applies(const char *language, ttf_script_t script);
/* Reserve the three system table paths, including compiled .bin/.ttfb forms. */
int ttf_cjk_manifest_selected(const ttf_engine_t *engine, const char *path, int deferred);
uint32_t ttf_cjk_initial_variant(uint32_t syllable, ttf_script_t script);
/* Bounded lookahead for the existing phrase segmenter, in source bytes. */
size_t ttf_cjk_protect_until(const ttf_engine_t *engine, const char *text,
                              size_t length, size_t begin);
/* What the sentence around a CJK run allows (ttf_cjk_render `sentence`). */
#define TTF_CJK_VARIANTS 1 /* simplified and Japanese new forms read (hanja_variant_reading) */
#define TTF_CJK_CHINESE 2  /* no Hangul or kana: hanja_chinese_reading first (几 기) */
#define TTF_CJK_JAPANESE 4 /* kana, no Hangul: hanja_japanese_reading first (予 예) */
ttf_status_t ttf_cjk_render(const ttf_engine_t *engine, const char *language,
                            const char *text, size_t length, ttf_semantic_t *semantic,
                            ttf_buffer_t *output, char lexicon[64],
                            ttf_resource_layer_t *pronunciation_layer, int sentence);
/* Shared by the KO resource validator and the runtime's bounded lookups. */
ttf_status_t ttf_cjk_validate(const ttf_resource_record_t *record, int *handled,
                              ttf_resource_diagnostic_t *diagnostic);
#endif
