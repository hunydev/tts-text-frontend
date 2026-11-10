#ifndef TTF_SPEECH_OUTPUT_H
#define TTF_SPEECH_OUTPUT_H
#include "internal.h"
ttf_status_t ttf_speech_output(const ttf_engine_t *engine, const ttf_language_pack_t *renderer,
                               ttf_buffer_t *text, const char *left, size_t left_length,
                               const char *right, size_t right_length);
int ttf_speech_spoken_before(const ttf_engine_t *engine, const char *language, const char *text,
                             size_t length);
int ttf_speech_lengthening_span(const ttf_engine_t *engine, const char *language, const char *left,
                                size_t left_length, const char *span, size_t span_length,
                                const char *right, size_t right_length);
int ttf_speech_spoken_after(const ttf_engine_t *engine, const char *language, const char *text,
                            size_t length);
const char *ttf_speech_kana_lengthening(const ttf_engine_t *engine, const char *language,
                                        const char *source_left, size_t source_left_length,
                                        const char *span, size_t span_length, const char *right,
                                        size_t right_length);
const char *ttf_speech_range_open(const ttf_engine_t *engine, const char *language,
                                  const char *source_left, size_t source_left_length,
                                  const char *span, size_t span_length, const char *right,
                                  size_t right_length);
int ttf_speech_spaced_silent(const ttf_engine_t *engine, const char *language, const char *left,
                             size_t left_length, const char *span, size_t span_length,
                             const char *right, size_t right_length);
const char *ttf_speech_range_mark(const ttf_engine_t *engine, const char *language,
                                  const char *left, size_t left_length, const char *span,
                                  size_t span_length, const char *right, size_t right_length);
#endif
