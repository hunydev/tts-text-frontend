#ifndef TTF_SPEECH_OUTPUT_H
#define TTF_SPEECH_OUTPUT_H
#include "internal.h"
ttf_status_t ttf_speech_output(const ttf_engine_t *engine, const ttf_language_pack_t *renderer,
                               ttf_buffer_t *text);
#endif
