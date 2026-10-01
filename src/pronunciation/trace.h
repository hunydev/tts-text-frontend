#ifndef TTF_PRONUNCIATION_TRACE_INTERNAL_H
#define TTF_PRONUNCIATION_TRACE_INTERNAL_H
#include "../core/internal.h"
#include "tts_text_frontend/ttf_trace.h"

typedef struct ttf_trace_state ttf_trace_state_t;
ttf_status_t ttf_trace_state_create(const ttf_trace_options_t *options, ttf_trace_state_t **out);
void ttf_trace_state_destroy(ttf_trace_state_t *state);
void ttf_trace_segment_clear(ttf_trace_state_t *state);
const ttf_trace_event_t *ttf_trace_events(const ttf_trace_state_t *state, size_t *count);
void ttf_trace_span_begin(const ttf_engine_t *engine, size_t input_begin, size_t input_end);
void ttf_trace_span_finish(const ttf_engine_t *engine, size_t output_begin, size_t output_end);
void ttf_trace_stage(const ttf_engine_t *engine, ttf_trace_stage_t stage, size_t part_begin,
                     size_t part_end);
size_t ttf_trace_mark(const ttf_engine_t *engine);
/* Events since mark move from a stripped copy back to input offsets (map[k] is
   the input offset of copy byte k; offset is added). */
void ttf_trace_remap_input(const ttf_engine_t *engine, size_t mark, const size_t *map,
                           size_t used, size_t length, size_t offset);
void ttf_trace_reject_since(const ttf_engine_t *engine, size_t mark);
ttf_status_t ttf_trace_status(const ttf_engine_t *engine);
ttf_status_t ttf_trace_emit(const ttf_engine_t *engine, ttf_trace_disposition_t disposition,
                            ttf_resource_layer_t layer, int layer_known, const char *language,
                            const char *kind, const char *key, const char *value, const char *text,
                            const char *detail, size_t phone_index);
/* Lookup the actual effective row; consumes no event on a miss. */
ttf_status_t ttf_trace_resource(const ttf_engine_t *engine, const char *kind, const char *language,
                                const char *key, const char *text, const char *detail,
                                size_t phone_index);
#endif
