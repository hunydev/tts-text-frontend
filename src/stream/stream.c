#include "../core/internal.h"
#include "../pronunciation/trace.h"
struct ttf_stream {
    const ttf_engine_t *engine;
    ttf_callback_fn callback;
    ttf_traced_callback_fn traced_callback;
    ttf_engine_t *engine_view;
    ttf_trace_state_t *trace;
    void *userdata;
    ttf_buffer_t pending;
    size_t input_offset, segments;
    size_t validated, wait_scanned;
    ttf_boundary_wait_t wait;
    int finished, busy;
    ttf_status_t failed;
};
static int path_continuation(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' ||
           c == '\\' || c == '.' || c == '_' || c == '-' || c == '~' || c == '+' || c == '@' ||
           c == '$';
}
/* UTF-8 is validated before this check. Only literal body bytes can be skipped:
   possible closers and syntax changes immediately restore the full planner.
   This does not batch or delay sentence callbacks for ordinary text. */
static int waiting_for_literal(ttf_stream_t *s, size_t valid) {
    size_t p = s->wait_scanned;
    if (s->wait.kind == TTF_WAIT_NONE)
        return 0;
    if (s->wait.kind == TTF_WAIT_BACKTICKS && p && p < valid && s->pending.data[p - 1] == '`')
        return 0; /* A closing delimiter run needs one byte of lookahead. */
    for (; p < valid; ++p) {
        unsigned char c = (unsigned char)s->pending.data[p];
        if ((s->wait.kind == TTF_WAIT_BACKTICKS && c == '`') ||
            (s->wait.kind == TTF_WAIT_ELEMENT && (c == '<' || c == '>')) ||
            (s->wait.kind == TTF_WAIT_COMMENT && c == '>') ||
            (s->wait.kind == TTF_WAIT_QUOTED && c == (unsigned char)s->wait.quote) ||
            (s->wait.kind == TTF_WAIT_PATH && !path_continuation(c)))
            return 0;
    }
    s->wait_scanned = valid;
    return 1;
}
static int control_limit_exceeded(const ttf_stream_t *s) {
    /* Count raw input bytes, including an incomplete UTF-8 suffix. Subtraction
       avoids overflow for caller-selected SIZE_MAX limits. Only a known-open
       control owns this bound; closed controls may precede arbitrary text. */
    return s->wait.control_limit && s->wait.begin <= s->pending.length &&
           s->pending.length - s->wait.begin > s->wait.control_limit;
}
static ttf_status_t drain(ttf_stream_t *s, int final) {
    while (s->pending.length) {
        size_t valid = s->validated, w, boundary;
        uint32_t cp;
        ttf_result_t result;
        ttf_segment_t seg;
        ttf_status_t st;
        while (valid < s->pending.length) {
            int decoded =
                ttf_utf8_decode(s->pending.data + valid, s->pending.length - valid, &cp, &w);
            if (decoded < 0)
                return TTF_EUTF8;
            if (!decoded)
                break;
            valid += w;
        }
        s->validated = valid;
        if (final && valid < s->pending.length)
            return TTF_EUTF8;
        if (!final && waiting_for_literal(s, valid))
            return control_limit_exceeded(s) ? TTF_ELIMIT : TTF_OK;
        st = ttf_next_boundary(s->engine, s->pending.data, valid, final, s->segments == 0,
                               &boundary, &s->wait);
        s->wait_scanned = valid;
        if (st != TTF_OK)
            return st;
        if (!boundary)
            return control_limit_exceeded(s) ? TTF_ELIMIT : TTF_OK;
        st = ttf_process_unit(s->engine, s->pending.data, boundary, s->input_offset, &result);
        if (st == TTF_OK)
            st = ttf_trace_status(s->engine);
        if (st != TTF_OK) {
            ttf_result_free(&result);
            return st;
        }
        seg.text = result.text;
        seg.length = result.length;
        seg.spans = result.spans;
        seg.span_count = result.span_count;
        seg.input_begin = s->input_offset;
        seg.input_end = s->input_offset + boundary;
        s->busy = 1;
        if (s->traced_callback) {
            ttf_traced_segment_t traced;
            traced.segment = seg;
            traced.events = ttf_trace_events(s->trace, &traced.event_count);
            st = s->traced_callback(&traced, s->userdata) ? TTF_ECALLBACK : TTF_OK;
        } else
            st = s->callback(&seg, s->userdata) ? TTF_ECALLBACK : TTF_OK;
        s->busy = 0;
        ttf_result_free(&result);
        ttf_trace_segment_clear(s->trace);
        if (st != TTF_OK)
            return st;
        s->segments++;
        s->input_offset += boundary;
        s->pending.length -= boundary;
        s->validated -= boundary;
        memset(&s->wait, 0, sizeof(s->wait));
        s->wait_scanned = 0;
        memmove(s->pending.data, s->pending.data + boundary, s->pending.length);
        s->pending.data[s->pending.length] = 0;
    }
    return TTF_OK;
}
static ttf_status_t stream_create(const ttf_engine_t *e, ttf_callback_fn cb,
                                  ttf_traced_callback_fn traced_cb,
                                  const ttf_trace_options_t *options, void *user,
                                  ttf_stream_t **out) {
    ttf_stream_t *s;
    ttf_status_t status;
    if (!out)
        return TTF_EINVAL;
    *out = NULL;
    if (!e || (!cb && !traced_cb))
        return TTF_EINVAL;
    if (!e->frozen)
        return TTF_ESTATE;
    s = (ttf_stream_t *)calloc(1, sizeof(*s));
    if (!s)
        return TTF_ENOMEM;
    s->engine = e;
    s->callback = cb;
    s->traced_callback = traced_cb;
    s->userdata = user;
    /* Immutable resources are borrowed. A nested ordinary normalization from a
       pack receives an independent, untraced view instead of changing its parent
       request's evidence. Never call engine_destroy on this shallow view. */
    if (traced_cb || e->trace_state || e->context_memo) {
        s->engine_view = (ttf_engine_t *)malloc(sizeof(*s->engine_view));
        if (!s->engine_view) {
            free(s);
            return TTF_ENOMEM;
        }
        *s->engine_view = *e;
        s->engine_view->trace_state = NULL;
        s->engine_view->context_memo = NULL;
        s->engine = s->engine_view;
    }
    if (traced_cb) {
        status = ttf_trace_state_create(options, &s->trace);
        if (status != TTF_OK) {
            free(s->engine_view);
            free(s);
            return status;
        }
        s->engine_view->trace_state = s->trace;
    }
    *out = s;
    return TTF_OK;
}
ttf_status_t ttf_stream_create(const ttf_engine_t *e, ttf_callback_fn cb, void *user,
                               ttf_stream_t **out) {
    return stream_create(e, cb, NULL, NULL, user, out);
}
ttf_status_t ttf_stream_create_traced(const ttf_engine_t *e, const ttf_trace_options_t *options,
                                      ttf_traced_callback_fn cb, void *user, ttf_stream_t **out) {
    return stream_create(e, NULL, cb, options, user, out);
}
ttf_status_t ttf_stream_write(ttf_stream_t *s, const char *data, size_t n) {
    size_t at = 0;
    ttf_status_t st;
    if (!s || (!data && n))
        return TTF_EINVAL;
    if (s->busy || s->finished)
        return TTF_ESTATE;
    if (s->failed)
        return s->failed;
    while (at < n) {
        size_t room = s->engine->config.max_pending_bytes - s->pending.length, take = n - at;
        if (!room) {
            s->failed = TTF_ELIMIT;
            return s->failed;
        }
        if (take > room)
            take = room;
        if (take > 4096)
            take = 4096;
        st = ttf_buffer_append(&s->pending, data + at, take);
        if (st == TTF_OK)
            st = drain(s, 0);
        if (st != TTF_OK) {
            s->failed = st;
            return st;
        }
        at += take;
    }
    return TTF_OK;
}
ttf_status_t ttf_stream_flush(ttf_stream_t *s) {
    ttf_status_t st;
    if (!s)
        return TTF_EINVAL;
    if (s->busy)
        return TTF_ESTATE;
    if (s->failed)
        return s->failed;
    if (s->finished)
        return TTF_OK;
    st = drain(s, 1);
    if (st != TTF_OK)
        s->failed = st;
    else
        s->finished = 1;
    return st;
}
void ttf_stream_destroy(ttf_stream_t *s) {
    if (s && !s->busy) {
        ttf_buffer_free(&s->pending);
        ttf_trace_state_destroy(s->trace);
        free(s->engine_view);
        free(s);
    }
}
