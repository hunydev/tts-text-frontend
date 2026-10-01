#include "trace.h"

struct ttf_trace_state {
    ttf_trace_options_t options;
    ttf_trace_event_t *events;
    size_t count, capacity, total_events, total_bytes;
    size_t span_start, input_begin, input_end, part_begin, part_end;
    ttf_trace_stage_t stage;
    ttf_status_t status;
    int active;
};

void ttf_trace_options_init(ttf_trace_options_t *options) {
    if (!options)
        return;
    memset(options, 0, sizeof(*options));
    options->struct_size = sizeof(*options);
    options->max_events = 65536;
    options->max_bytes = 16u * 1024u * 1024u;
}
const char *ttf_trace_stage_name(ttf_trace_stage_t stage) {
    switch (stage) {
    case TTF_TRACE_LEXICON:
        return "lexicon";
    case TTF_TRACE_SOURCE_PRONUNCIATION:
        return "source-pronunciation";
    case TTF_TRACE_TARGET_PHONETIC:
        return "target-phonetic";
    case TTF_TRACE_TARGET_GRAPHEME:
        return "target-grapheme";
    case TTF_TRACE_POLICY:
        return "policy";
    }
    return "unknown";
}
const char *ttf_trace_disposition_name(ttf_trace_disposition_t disposition) {
    switch (disposition) {
    case TTF_TRACE_SELECTED:
        return "selected";
    case TTF_TRACE_REJECTED:
        return "rejected";
    }
    return "unknown";
}
ttf_status_t ttf_trace_state_create(const ttf_trace_options_t *options, ttf_trace_state_t **out) {
    ttf_trace_options_t defaults;
    ttf_trace_state_t *state;
    if (!out)
        return TTF_EINVAL;
    *out = NULL;
    if (!options) {
        ttf_trace_options_init(&defaults);
        options = &defaults;
    }
    if (options->struct_size != sizeof(*options) || !options->max_events || !options->max_bytes)
        return TTF_EINVAL;
    state = (ttf_trace_state_t *)calloc(1, sizeof(*state));
    if (!state)
        return TTF_ENOMEM;
    state->options = *options;
    *out = state;
    return TTF_OK;
}
void ttf_trace_segment_clear(ttf_trace_state_t *state) {
    size_t i;
    if (!state)
        return;
    for (i = 0; i < state->count; ++i)
        free((void *)state->events[i].language);
    state->count = 0;
    state->active = 0;
}
void ttf_trace_state_destroy(ttf_trace_state_t *state) {
    if (!state)
        return;
    ttf_trace_segment_clear(state);
    free(state->events);
    free(state);
}
const ttf_trace_event_t *ttf_trace_events(const ttf_trace_state_t *state, size_t *count) {
    if (count)
        *count = state ? state->count : 0;
    return state ? state->events : NULL;
}
void ttf_trace_span_begin(const ttf_engine_t *engine, size_t begin, size_t end) {
    ttf_trace_state_t *state = engine ? engine->trace_state : NULL;
    if (!state)
        return;
    state->active = 1;
    state->span_start = state->count;
    state->input_begin = begin;
    state->input_end = end;
    state->stage = TTF_TRACE_POLICY;
    state->part_begin = state->part_end = SIZE_MAX;
}
void ttf_trace_span_finish(const ttf_engine_t *engine, size_t begin, size_t end) {
    ttf_trace_state_t *state = engine ? engine->trace_state : NULL;
    size_t i;
    if (!state)
        return;
    for (i = state->span_start; i < state->count; ++i) {
        state->events[i].output_begin = begin;
        state->events[i].output_end = end;
    }
    state->active = 0;
}
void ttf_trace_stage(const ttf_engine_t *engine, ttf_trace_stage_t stage, size_t begin,
                     size_t end) {
    ttf_trace_state_t *state = engine ? engine->trace_state : NULL;
    if (!state)
        return;
    state->stage = stage;
    state->part_begin = begin;
    state->part_end = end;
}
size_t ttf_trace_mark(const ttf_engine_t *engine) {
    return engine && engine->trace_state ? engine->trace_state->count : 0;
}
void ttf_trace_remap_input(const ttf_engine_t *engine, size_t mark, const size_t *map,
                           size_t used, size_t length, size_t offset) {
    ttf_trace_state_t *state = engine ? engine->trace_state : NULL;
    if (!state)
        return;
    for (; mark < state->count; ++mark) {
        ttf_trace_event_t *event = &state->events[mark];
        size_t b = event->input_begin, e = event->input_end;
        event->input_begin = offset + (!b ? 0 : b < used ? map[b] : length);
        event->input_end = offset + (e < used ? map[e] : length);
    }
}
void ttf_trace_reject_since(const ttf_engine_t *engine, size_t mark) {
    ttf_trace_state_t *state = engine ? engine->trace_state : NULL;
    if (!state)
        return;
    for (; mark < state->count; ++mark)
        state->events[mark].disposition = TTF_TRACE_REJECTED;
}
ttf_status_t ttf_trace_status(const ttf_engine_t *engine) {
    return engine && engine->trace_state ? engine->trace_state->status : TTF_OK;
}
static ttf_status_t string_bytes(const ttf_trace_event_t *event, size_t *bytes) {
    const char *strings[] = {event->language, event->resource_language,
                             event->kind,     event->key,
                             event->value,    event->text,
                             event->detail};
    size_t i, sum = 0;
    for (i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        size_t length = strings[i] ? strlen(strings[i]) : 0;
        if (length == SIZE_MAX || sum > SIZE_MAX - length - 1)
            return TTF_ELIMIT;
        if (strings[i]) {
            size_t at = 0, width;
            uint32_t cp;
            while (at < length) {
                if (ttf_utf8_decode(strings[i] + at, length - at, &cp, &width) != 1)
                    return TTF_EUTF8;
                at += width;
            }
        }
        sum += length + 1;
    }
    *bytes = sum;
    return TTF_OK;
}
/* The first string owns one allocation containing all seven strings. */
static ttf_status_t copy_event(ttf_trace_event_t *target, const ttf_trace_event_t *source,
                               size_t bytes) {
    const char *strings[] = {source->language, source->resource_language,
                             source->kind,     source->key,
                             source->value,    source->text,
                             source->detail};
    const char **fields[] = {&target->language, &target->resource_language,
                             &target->kind,     &target->key,
                             &target->value,    &target->text,
                             &target->detail};
    char *allocation = (char *)malloc(bytes), *at = allocation;
    size_t i;
    if (!allocation)
        return TTF_ENOMEM;
    *target = *source;
    for (i = 0; i < sizeof(strings) / sizeof(strings[0]); ++i) {
        const char *string = strings[i] ? strings[i] : "";
        size_t length = strlen(string) + 1;
        memcpy(at, string, length);
        *fields[i] = at;
        at += length;
    }
    return TTF_OK;
}
static ttf_status_t reserve_events(ttf_trace_event_t **events, size_t *capacity, size_t needed) {
    size_t next = *capacity ? *capacity : 16;
    ttf_trace_event_t *allocation;
    if (needed <= *capacity)
        return TTF_OK;
    while (next < needed) {
        if (next > SIZE_MAX / 2)
            return TTF_ELIMIT;
        next *= 2;
    }
    if (next > SIZE_MAX / sizeof(*allocation))
        return TTF_ELIMIT;
    allocation = (ttf_trace_event_t *)realloc(*events, next * sizeof(*allocation));
    if (!allocation)
        return TTF_ENOMEM;
    *events = allocation;
    *capacity = next;
    return TTF_OK;
}
static ttf_status_t record_event(const ttf_engine_t *engine, ttf_trace_event_t *event) {
    ttf_trace_state_t *state = engine ? engine->trace_state : NULL;
    size_t bytes, payload;
    ttf_status_t status;
    if (!state)
        return TTF_OK;
    if (state->status != TTF_OK)
        return state->status;
    if (!state->active)
        return TTF_OK;
    status = string_bytes(event, &bytes);
    if (status != TTF_OK)
        return state->status = status;
    if (bytes > SIZE_MAX - sizeof(*event))
        return state->status = TTF_ELIMIT;
    payload = bytes + sizeof(*event);
    if (state->total_events >= state->options.max_events ||
        payload > state->options.max_bytes - state->total_bytes)
        return state->status = TTF_ELIMIT;
    status = reserve_events(&state->events, &state->capacity, state->count + 1);
    if (status != TTF_OK)
        return state->status = status;
    event->struct_size = sizeof(*event);
    event->stage = state->stage;
    event->input_begin = state->input_begin;
    event->input_end = state->input_end;
    event->output_begin = event->output_end = SIZE_MAX;
    event->part_begin = state->part_begin;
    event->part_end = state->part_end;
    status = copy_event(&state->events[state->count], event, bytes);
    if (status != TTF_OK)
        return state->status = status;
    ++state->count;
    ++state->total_events;
    state->total_bytes += payload;
    return TTF_OK;
}
ttf_status_t ttf_trace_record(const ttf_engine_t *engine, const ttf_trace_note_t *note) {
    ttf_trace_event_t event;
    if (!engine)
        return TTF_EINVAL;
    if (engine->trace_state && engine->trace_state->status != TTF_OK)
        return engine->trace_state->status;
    if (!note || note->struct_size != sizeof(*note)) {
        if (engine->trace_state)
            engine->trace_state->status = TTF_EINVAL;
        return TTF_EINVAL;
    }
    if (!engine->trace_state)
        return TTF_OK;
    if ((note->disposition != TTF_TRACE_SELECTED && note->disposition != TTF_TRACE_REJECTED) ||
        (note->layer_known != 0 && note->layer_known != 1) ||
        (note->layer_known && (note->layer < TTF_LAYER_COMMON || note->layer > TTF_LAYER_USER)))
        return engine->trace_state->status = TTF_EINVAL;
    memset(&event, 0, sizeof(event));
    event.disposition = note->disposition;
    event.layer = note->layer_known ? note->layer : TTF_LAYER_COMMON;
    event.layer_known = note->layer_known;
    event.declaration_order = SIZE_MAX;
    event.phone_index = note->phone_index;
    event.language = note->language;
    event.kind = note->kind;
    event.key = note->key;
    event.value = note->value;
    event.text = note->text;
    event.detail = note->detail;
    return record_event(engine, &event);
}
ttf_status_t ttf_trace_record_resource(const ttf_engine_t *engine, const char *kind,
                                       const char *language, const char *key, const char *text,
                                       const char *detail, size_t phone_index) {
    const ttf_entry_t *entry;
    ttf_trace_event_t event;
    if (!engine)
        return TTF_EINVAL;
    if (engine->trace_state && engine->trace_state->status != TTF_OK)
        return engine->trace_state->status;
    if (!kind || !language || !key) {
        if (engine->trace_state)
            engine->trace_state->status = TTF_EINVAL;
        return TTF_EINVAL;
    }
    if (!engine->trace_state)
        return TTF_OK;
    entry = ttf_resource_entry(engine, kind, language, key);
    if (!entry)
        return TTF_OK;
    memset(&event, 0, sizeof(event));
    event.disposition = TTF_TRACE_SELECTED;
    event.layer = entry->layer;
    event.layer_known = 1;
    event.declaration_order = entry->order;
    event.phone_index = phone_index;
    event.language = language;
    event.resource_language = entry->lang;
    event.kind = entry->kind;
    event.key = entry->key;
    event.value = entry->value;
    event.text = text;
    event.detail = detail;
    return record_event(engine, &event);
}
ttf_status_t ttf_trace_emit(const ttf_engine_t *engine, ttf_trace_disposition_t disposition,
                            ttf_resource_layer_t layer, int known, const char *language,
                            const char *kind, const char *key, const char *value, const char *text,
                            const char *detail, size_t phone_index) {
    ttf_trace_note_t note;
    memset(&note, 0, sizeof(note));
    note.struct_size = sizeof(note);
    note.disposition = disposition;
    note.layer = layer;
    note.layer_known = known;
    note.phone_index = phone_index;
    note.language = language;
    note.kind = kind;
    note.key = key;
    note.value = value;
    note.text = text;
    note.detail = detail;
    return ttf_trace_record(engine, &note);
}
ttf_status_t ttf_trace_resource(const ttf_engine_t *engine, const char *kind, const char *language,
                                const char *key, const char *text, const char *detail,
                                size_t phone_index) {
    return ttf_trace_record_resource(engine, kind, language, key, text, detail, phone_index);
}
void ttf_traced_result_free(ttf_traced_result_t *result) {
    size_t i;
    if (!result)
        return;
    for (i = 0; i < result->event_count; ++i)
        free((void *)result->events[i].language);
    free(result->events);
    ttf_result_free(&result->result);
    memset(result, 0, sizeof(*result));
}
typedef struct {
    ttf_traced_result_t *out;
    ttf_buffer_t text;
    size_t event_capacity, span_capacity;
    ttf_status_t status;
} trace_collector_t;
static int collect_traced(const ttf_traced_segment_t *segment, void *userdata) {
    trace_collector_t *collector = (trace_collector_t *)userdata;
    ttf_traced_result_t *out = collector->out;
    size_t i, base = collector->text.length, count;
    ttf_status_t status;
    status = ttf_buffer_append(&collector->text, segment->segment.text, segment->segment.length);
    if (status != TTF_OK)
        goto fail;
    if (segment->segment.span_count > SIZE_MAX - out->result.span_count) {
        status = TTF_ELIMIT;
        goto fail;
    }
    count = out->result.span_count + segment->segment.span_count;
    if (count > collector->span_capacity) {
        ttf_span_t *spans;
        size_t capacity = collector->span_capacity ? collector->span_capacity : 32;
        while (capacity < count) {
            if (capacity > SIZE_MAX / 2) {
                status = TTF_ELIMIT;
                goto fail;
            }
            capacity *= 2;
        }
        if (capacity > SIZE_MAX / sizeof(*spans)) {
            status = TTF_ELIMIT;
            goto fail;
        }
        spans = (ttf_span_t *)realloc(out->result.spans, capacity * sizeof(*spans));
        if (!spans) {
            status = TTF_ENOMEM;
            goto fail;
        }
        out->result.spans = spans;
        collector->span_capacity = capacity;
    }
    for (i = 0; i < segment->segment.span_count; ++i) {
        ttf_span_t span = segment->segment.spans[i];
        span.begin += base;
        span.end += base;
        out->result.spans[out->result.span_count++] = span;
    }
    if (segment->event_count > SIZE_MAX - out->event_count) {
        status = TTF_ELIMIT;
        goto fail;
    }
    status = reserve_events(&out->events, &collector->event_capacity,
                            out->event_count + segment->event_count);
    if (status != TTF_OK)
        goto fail;
    for (i = 0; i < segment->event_count; ++i) {
        size_t bytes;
        ttf_trace_event_t *event = &out->events[out->event_count];
        status = string_bytes(&segment->events[i], &bytes);
        if (status == TTF_OK)
            status = copy_event(event, &segment->events[i], bytes);
        if (status != TTF_OK)
            goto fail;
        event->output_begin += base;
        event->output_end += base;
        ++out->event_count;
    }
    return 0;
fail:
    collector->status = status;
    return 1;
}
ttf_status_t ttf_normalize_traced(const ttf_engine_t *engine, const char *text, size_t length,
                                  const ttf_trace_options_t *options, ttf_traced_result_t *out) {
    ttf_stream_t *stream = NULL;
    trace_collector_t collector;
    ttf_status_t status;
    if (!out)
        return TTF_EINVAL;
    memset(out, 0, sizeof(*out));
    if (!engine || (!text && length))
        return TTF_EINVAL;
    memset(&collector, 0, sizeof(collector));
    collector.out = out;
    status = ttf_stream_create_traced(engine, options, collect_traced, &collector, &stream);
    if (status == TTF_OK)
        status = ttf_stream_write(stream, text, length);
    if (status == TTF_OK)
        status = ttf_stream_flush(stream);
    ttf_stream_destroy(stream);
    if (collector.status != TTF_OK)
        status = collector.status;
    if (status == TTF_OK)
        status = ttf_buffer_append(&collector.text, "", 0);
    if (status != TTF_OK) {
        ttf_buffer_free(&collector.text);
        ttf_traced_result_free(out);
        return status;
    }
    out->result.text = collector.text.data;
    out->result.length = collector.text.length;
    return TTF_OK;
}
