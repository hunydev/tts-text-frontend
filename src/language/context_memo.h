#ifndef TTF_CONTEXT_MEMO_H
#define TTF_CONTEXT_MEMO_H
#include "../core/internal.h"

typedef ttf_status_t (*ttf_context_build_fn)(const ttf_pack_input_t *, void **);
typedef void (*ttf_context_dispose_fn)(void *);
struct ttf_context_memo_entry;
typedef struct ttf_context_memo {
    struct ttf_context_memo_entry **slots, *entries;
    size_t capacity, count;
    ttf_status_t status;
} ttf_context_memo_t;

/* Internal request-local facility, not part of the public pack ABI. A factory
   prepares a complete scope, independent of input.index. Identity is the address
   of a distinct static object. Factories may request other identities recursively.
   A failed factory may return partial data: dispose is called exactly once. */
int ttf_context_memo_enabled(const ttf_pack_input_t *input);
void *ttf_context_memo_get(const ttf_pack_input_t *input, const void *identity,
                          ttf_context_build_fn build, ttf_context_dispose_fn dispose);
/* Zero-initialize before first use; clear after token mutation and at unit exit. */
void ttf_context_memo_clear(ttf_context_memo_t *memo);
/* Returns 0 without a memo; otherwise supplies cached bounds or records an error. */
int ttf_context_run_cached(const ttf_pack_input_t *input, size_t *begin, size_t *end);
#endif
