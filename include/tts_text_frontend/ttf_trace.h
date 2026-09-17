#ifndef TTS_TEXT_FRONTEND_TTF_TRACE_H
#define TTS_TEXT_FRONTEND_TTF_TRACE_H
#include "ttf.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Additive ABI. Existing result, segment, configuration and pack layouts are unchanged. */
typedef enum {
    TTF_TRACE_LEXICON,
    TTF_TRACE_SOURCE_PRONUNCIATION,
    TTF_TRACE_TARGET_PHONETIC,
    TTF_TRACE_TARGET_GRAPHEME,
    TTF_TRACE_POLICY
} ttf_trace_stage_t;
typedef enum { TTF_TRACE_SELECTED, TTF_TRACE_REJECTED } ttf_trace_disposition_t;
typedef struct {
    uint32_t struct_size;
    size_t max_events, max_bytes;
} ttf_trace_options_t;
typedef struct {
    uint32_t struct_size;
    ttf_trace_stage_t stage;
    ttf_trace_disposition_t disposition;
    size_t input_begin, input_end;   /* Absolute original UTF-8 byte offsets. */
    size_t output_begin, output_end; /* Whole span; batch or segment relative. */
    size_t part_begin, part_end;     /* Chosen pronunciation input part (surface/canonical/verbalized), or SIZE_MAX. */
    size_t phone_index;              /* Phone ordinal, or SIZE_MAX when inapplicable. */
    ttf_resource_layer_t layer;
    int layer_known;
    size_t declaration_order; /* Engine resource declaration, or SIZE_MAX. */
    const char *language, *resource_language;
    const char *kind, *key, *value, *text, *detail;
} ttf_trace_event_t;
typedef struct {
    ttf_result_t result;
    ttf_trace_event_t *events;
    size_t event_count;
} ttf_traced_result_t;
typedef struct {
    ttf_segment_t segment;
    const ttf_trace_event_t *events;
    size_t event_count;
} ttf_traced_segment_t;
typedef int (*ttf_traced_callback_fn)(const ttf_traced_segment_t *, void *);

/* All non-NULL strings must be NUL-terminated UTF-8 and are copied.
   NULL strings become empty strings. Invalid UTF-8 fails with TTF_EUTF8. Resource provenance
   must use record_resource; generated notes have no resource declaration. */
typedef struct {
    uint32_t struct_size;
    ttf_trace_disposition_t disposition;
    ttf_resource_layer_t layer;
    int layer_known;
    size_t phone_index;
    const char *language, *kind, *key, *value, *text, *detail;
} ttf_trace_note_t;

/* Defaults: 65,536 events and 16 MiB logical event/string payload per request.
   Limits include rejected attempts and remain cumulative across stream segments.
   Exceeding either limit fails explicitly with TTF_ELIMIT, without truncation.
   Internal allocation growth can temporarily use more than the payload budget. */
TTF_API void ttf_trace_options_init(ttf_trace_options_t *options);
TTF_API ttf_status_t ttf_normalize_traced(const ttf_engine_t *engine, const char *text,
                                          size_t length, const ttf_trace_options_t *options,
                                          ttf_traced_result_t *out);
TTF_API void ttf_traced_result_free(ttf_traced_result_t *result);
/* Engine must remain frozen and alive until stream destruction. Events and all
   callback pointers expire on callback return. Copy them to retain evidence.
   Previously delivered segments remain committed if a later segment fails. */
TTF_API ttf_status_t ttf_stream_create_traced(const ttf_engine_t *engine,
                                              const ttf_trace_options_t *options,
                                              ttf_traced_callback_fn callback, void *userdata,
                                              ttf_stream_t **out);
TTF_API const char *ttf_trace_stage_name(ttf_trace_stage_t stage);
TTF_API const char *ttf_trace_disposition_name(ttf_trace_disposition_t disposition);
/* Optional instrumentation for language packs, within the current core-selected
   span/stage/part. A pack receives a borrowed request engine: never retain or
   destroy it, or use its pointer as a persistent engine identity. Resource
   strings remain owned by the underlying frozen engine. Ordinary context_score
   callbacks may also receive borrowed views. These calls are no-ops during
   ordinary, untraced normalization.
   Failure is sticky and prevents delivery of an incomplete traced segment. */
TTF_API ttf_status_t ttf_trace_record(const ttf_engine_t *engine, const ttf_trace_note_t *note);
TTF_API ttf_status_t ttf_trace_record_resource(const ttf_engine_t *engine, const char *kind,
                                               const char *language, const char *key,
                                               const char *text, const char *detail,
                                               size_t phone_index);
#ifdef __cplusplus
}
#endif
#endif
