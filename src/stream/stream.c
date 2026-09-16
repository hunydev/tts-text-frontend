#include "../core/internal.h"
struct ttf_stream {
    const ttf_engine_t *engine;
    ttf_callback_fn callback;
    void *userdata;
    ttf_buffer_t pending;
    size_t input_offset, segments;
    int finished, busy;
    ttf_status_t failed;
};
static ttf_status_t drain(ttf_stream_t *s, int final) {
    while (s->pending.length) {
        size_t valid = 0, w, boundary;
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
        if (final && valid < s->pending.length)
            return TTF_EUTF8;
        boundary = ttf_next_boundary(s->engine, s->pending.data, valid, final, s->segments == 0);
        if (!boundary)
            return TTF_OK;
        st = ttf_process_unit(s->engine, s->pending.data, boundary, s->input_offset, &result);
        if (st != TTF_OK)
            return st;
        seg.text = result.text;
        seg.length = result.length;
        seg.spans = result.spans;
        seg.span_count = result.span_count;
        seg.input_begin = s->input_offset;
        seg.input_end = s->input_offset + boundary;
        s->busy = 1;
        st = s->callback(&seg, s->userdata) ? TTF_ECALLBACK : TTF_OK;
        s->busy = 0;
        ttf_result_free(&result);
        if (st != TTF_OK)
            return st;
        s->segments++;
        s->input_offset += boundary;
        s->pending.length -= boundary;
        memmove(s->pending.data, s->pending.data + boundary, s->pending.length);
        s->pending.data[s->pending.length] = 0;
    }
    return TTF_OK;
}
ttf_status_t ttf_stream_create(const ttf_engine_t *e, ttf_callback_fn cb, void *user,
                               ttf_stream_t **out) {
    ttf_stream_t *s;
    if (!e || !cb || !out)
        return TTF_EINVAL;
    *out = NULL;
    if (!e->frozen)
        return TTF_ESTATE;
    s = (ttf_stream_t *)calloc(1, sizeof(*s));
    if (!s)
        return TTF_ENOMEM;
    s->engine = e;
    s->callback = cb;
    s->userdata = user;
    *out = s;
    return TTF_OK;
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
        free(s);
    }
}
