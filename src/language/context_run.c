#include "context_memo.h"

typedef struct { size_t begin, end; } bounds_t;
static const char run_identity = 0;

static int ascii_token(const ttf_token_t *token) {
    return token->script == TTF_SCRIPT_LATIN || token->script == TTF_SCRIPT_DIGIT;
}
/* Exactly the legacy membership predicate, using forward evidence instead of
   rescanning whitespace and arbitrarily long adjacent C++/# chains. */
static int member(const ttf_pack_input_t *in, size_t i, size_t previous_word, int ascii_chain) {
    const ttf_token_t *t = &in->tokens[i];
    size_t j, width = t->end - t->begin;
    if (ascii_token(t))
        return 1;
    if (t->script == TTF_SCRIPT_WHITESPACE) {
        for (j = t->begin; j < t->end; ++j)
            if (in->text[j] == '\r' || in->text[j] == '\n')
                return 0;
        return 1;
    }
    if (t->script == TTF_SCRIPT_SYMBOL || t->script == TTF_SCRIPT_PUNCTUATION) {
        char key[32];
        if (width < sizeof(key)) {
            memcpy(key, in->text + t->begin, width);
            key[width] = 0;
            if (ttf_resource_get(in->engine, "currency_prefix", "*", key, NULL))
                return 1;
            if (ttf_resource_get(in->engine, "context_connector", "*", key, NULL) &&
                previous_word != SIZE_MAX && in->tokens[previous_word].script == TTF_SCRIPT_DIGIT)
                return 1;
        }
    }
    if (width == 1) {
        char c = in->text[t->begin];
        if ((c == '+' || c == '-' || c == '.') && i + 1 < in->token_count &&
            in->tokens[i + 1].script == TTF_SCRIPT_DIGIT && t->end == in->tokens[i + 1].begin)
            return 1;
        if (c == ',' && i && i + 1 < in->token_count &&
            in->tokens[i - 1].script == TTF_SCRIPT_DIGIT &&
            in->tokens[i + 1].script == TTF_SCRIPT_DIGIT && in->tokens[i - 1].end == t->begin &&
            t->end == in->tokens[i + 1].begin)
            return 1;
        if ((c == '.' || c == '-' || c == '\'' || c == ':' || c == '/') && i &&
            i + 1 < in->token_count && ascii_token(&in->tokens[i - 1]) &&
            ascii_token(&in->tokens[i + 1]) && in->tokens[i - 1].end == t->begin &&
            t->end == in->tokens[i + 1].begin)
            return 1;
        if ((c == '+' || c == '#') && i && in->tokens[i - 1].end == t->begin && ascii_chain)
            return 1;
    }
    return 0;
}
static ttf_status_t build_runs(const ttf_pack_input_t *in, void **out) {
    bounds_t *bounds;
    size_t i, previous_word = SIZE_MAX, n = in->token_count;
    int ascii_chain = 0;
    *out = NULL;
    if (n > SIZE_MAX / sizeof(*bounds))
        return TTF_ELIMIT;
    bounds = (bounds_t *)malloc((n ? n : 1) * sizeof(*bounds));
    if (!bounds)
        return TTF_ENOMEM;
    for (i = 0; i < n; ++i) {
        const ttf_token_t *t = &in->tokens[i];
        int chained = i && in->tokens[i - 1].end == t->begin && ascii_chain;
        bounds[i].begin = i;
        bounds[i].end = member(in, i, previous_word, ascii_chain) ? SIZE_MAX : i;
        ascii_chain = ascii_token(t) ||
                      (chained && t->end == t->begin + 1 &&
                       (in->text[t->begin] == '+' || in->text[t->begin] == '#'));
        if (t->script != TTF_SCRIPT_WHITESPACE)
            previous_word = i;
    }
    for (i = 0; i < n;) {
        size_t first, after, begin, end, j;
        if (bounds[i].end != SIZE_MAX) {
            ++i;
            continue;
        }
        first = i;
        while (i < n && bounds[i].end == SIZE_MAX)
            ++i;
        after = i;
        begin = first;
        end = after;
        while (begin < end && in->tokens[begin].script == TTF_SCRIPT_WHITESPACE)
            ++begin;
        while (end > begin && in->tokens[end - 1].script == TTF_SCRIPT_WHITESPACE)
            --end;
        for (j = first; j < after; ++j) {
            bounds[j].begin = begin;
            bounds[j].end = end;
        }
    }
    *out = bounds;
    return TTF_OK;
}
int ttf_context_run_cached(const ttf_pack_input_t *in, size_t *begin, size_t *end) {
    const bounds_t *bounds;
    if (!ttf_context_memo_enabled(in))
        return 0;
    *begin = *end = in->index;
    if (in->index >= in->token_count)
        return 1;
    bounds = (const bounds_t *)ttf_context_memo_get(in, &run_identity, build_runs, free);
    if (bounds) {
        *begin = bounds[in->index].begin;
        *end = bounds[in->index].end;
    }
    return 1;
}
