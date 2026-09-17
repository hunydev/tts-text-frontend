#include "context_memo.h"

typedef struct ttf_context_memo_entry {
    const void *identity;
    const char *text;
    const ttf_token_t *tokens;
    size_t length, token_count;
    void *data;
    ttf_context_build_fn build;
    ttf_context_dispose_fn dispose;
    int building;
    struct ttf_context_memo_entry *next;
} entry_t;

static size_t mix(size_t h, size_t value) {
    return h ^ (value + (h << 6) + (h >> 2));
}
static size_t hash(const ttf_pack_input_t *in, const void *identity) {
    size_t h = (size_t)(uintptr_t)in->tokens;
    h = mix(h, (size_t)(uintptr_t)in->text);
    h = mix(h, (size_t)(uintptr_t)identity);
    h = mix(h, in->length);
    return mix(h, in->token_count);
}
static size_t entry_hash(const entry_t *entry) {
    ttf_pack_input_t in;
    memset(&in, 0, sizeof(in));
    in.text = entry->text;
    in.length = entry->length;
    in.tokens = entry->tokens;
    in.token_count = entry->token_count;
    return hash(&in, entry->identity);
}
static ttf_status_t grow(ttf_context_memo_t *memo) {
    size_t capacity = memo->capacity ? memo->capacity * 2 : 16;
    entry_t **slots, *entry;
    if (capacity < memo->capacity || capacity > SIZE_MAX / sizeof(*slots))
        return TTF_ELIMIT;
    slots = (entry_t **)calloc(capacity, sizeof(*slots));
    if (!slots)
        return TTF_ENOMEM;
    for (entry = memo->entries; entry; entry = entry->next) {
        size_t at = entry_hash(entry) & (capacity - 1);
        while (slots[at])
            at = (at + 1) & (capacity - 1);
        slots[at] = entry;
    }
    free(memo->slots);
    memo->slots = slots;
    memo->capacity = capacity;
    return TTF_OK;
}
int ttf_context_memo_enabled(const ttf_pack_input_t *in) {
    return in && in->engine && in->engine->context_memo;
}
void *ttf_context_memo_get(const ttf_pack_input_t *in, const void *identity,
                          ttf_context_build_fn build, ttf_context_dispose_fn dispose) {
    ttf_context_memo_t *memo;
    entry_t *entry;
    size_t at;
    ttf_status_t status;
    void *data = NULL;
    if (!ttf_context_memo_enabled(in))
        return NULL;
    memo = in->engine->context_memo;
    if (memo->status != TTF_OK)
        return NULL;
    if (!identity || !build || !dispose || (!in->tokens && in->token_count)) {
        memo->status = TTF_EINVAL;
        return NULL;
    }
    at = memo->capacity ? hash(in, identity) & (memo->capacity - 1) : 0;
    while (memo->capacity && (entry = memo->slots[at])) {
        if (entry->identity == identity && entry->text == in->text &&
            entry->tokens == in->tokens && entry->length == in->length &&
            entry->token_count == in->token_count) {
            if (entry->building || entry->build != build || entry->dispose != dispose) {
                memo->status = TTF_ERESOURCE;
                return NULL;
            }
            return entry->data;
        }
        at = (at + 1) & (memo->capacity - 1);
    }
    if (!memo->capacity || memo->count >= memo->capacity - memo->capacity / 4) {
        memo->status = grow(memo);
        if (memo->status != TTF_OK)
            return NULL;
        at = hash(in, identity) & (memo->capacity - 1);
        while (memo->slots[at])
            at = (at + 1) & (memo->capacity - 1);
    }
    entry = (entry_t *)calloc(1, sizeof(*entry));
    if (!entry) {
        memo->status = TTF_ENOMEM;
        return NULL;
    }
    entry->identity = identity;
    entry->text = in->text;
    entry->length = in->length;
    entry->tokens = in->tokens;
    entry->token_count = in->token_count;
    entry->dispose = dispose;
    entry->build = build;
    entry->building = 1;
    entry->next = memo->entries;
    memo->entries = entry;
    memo->slots[at] = entry;
    ++memo->count;
    /* Entries remain at stable addresses if a nested factory grows the table. */
    status = build(in, &data);
    entry->building = 0;
    if (memo->status == TTF_OK && status != TTF_OK)
        memo->status = status;
    if (memo->status == TTF_OK && !data)
        memo->status = TTF_ERESOURCE;
    if (memo->status != TTF_OK) {
        if (data)
            dispose(data);
        return NULL;
    }
    entry->data = data;
    return data;
}
void ttf_context_memo_clear(ttf_context_memo_t *memo) {
    entry_t *entry = memo->entries;
    while (entry) {
        entry_t *next = entry->next;
        if (entry->data)
            entry->dispose(entry->data);
        free(entry);
        entry = next;
    }
    free(memo->slots);
    memset(memo, 0, sizeof(*memo));
}
