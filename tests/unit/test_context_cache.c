#include "../../src/language/context_memo.h"
#include "../../languages/context_run.h"
#ifdef TTF_TEST_HAVE_THREADS
#include <pthread.h>
#endif

extern const ttf_language_pack_t ttf_pack_ko_KR;
extern const ttf_language_pack_t ttf_pack_en_US;
static size_t checks, failures;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); ++failures; \
} } while (0)

static const char *texts[] = {
    "", " \t  ", "go home please", "home go please", "home please go", "we go home", "home go we",
    "  go home please  ", "Apple is red; we go home!", "go\r\nplease home\nwe go",
    "1st 2nd 3kg 12% 1e-3 U.S. $12.50 2GHz", "C++ C# don't re-use foo/bar 1,000.5",
    "가 나 다 라 Apple", "Apple은 사과입니다. GPU를 사용합니다.", "Apple 개입니다.",
    "3개입니다. 3시입니다. 3만원입니다.", "Phone:+12125550100 Account:0012",
    "문의는 Phone:+1 212 555 0100입니다.", "제목은: First Light", "제목은\nFirst Light",
    "그는 \"Please go to the 1st floor.\"라고 말했습니다.", "home “please «go home» we” go",
    "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu",
    "7 7 7 7 7 7", "3 % Apple은 사과입니다.", "A + + # 3 +4 -5 .6"
};

static void scores_in_scope(ttf_engine_t *owner, ttf_engine_t *view,
                            const char *text, size_t length, ttf_token_t *tokens,
                            size_t begin, size_t end) {
    const ttf_language_pack_t *packs[] = {&ttf_pack_ko_KR, &ttf_pack_en_US};
    ttf_pack_input_t plain, cached;
    size_t i, p;
    memset(&plain, 0, sizeof(plain));
    plain.text = text;
    plain.length = length;
    plain.tokens = tokens ? tokens + begin : NULL;
    plain.token_count = end - begin;
    plain.engine = owner;
    cached = plain;
    cached.engine = view;
    /* Include both in-range tokens and the documented out-of-range callback
       path. Shifted scopes retain absolute byte offsets into the full text. */
    for (i = 0; i <= plain.token_count; ++i) {
        size_t expected_begin, expected_end, actual_begin, actual_end;
        plain.index = cached.index = i;
        context_run_bounds(&plain, &expected_begin, &expected_end);
        context_run_bounds(&cached, &actual_begin, &actual_end);
        CHECK(expected_begin == actual_begin && expected_end == actual_end);
        for (p = 0; p < 2; ++p) {
            int expected = packs[p]->context_score(&plain);
            int actual = packs[p]->context_score(&cached);
            ++checks;
            if (expected != actual) {
                fprintf(stderr, "context mismatch %s [%zu,%zu) index %zu: %d != %d; text=%s\n",
                        packs[p]->language_tag, begin, end, i, expected, actual, text);
                ++failures;
            }
            CHECK(view->context_memo->status == TTF_OK);
        }
    }
}

static void score_oracle(ttf_engine_t *owner) {
    size_t item;
    for (item = 0; item < sizeof(texts) / sizeof(texts[0]); ++item) {
        ttf_context_memo_t memo = {0};
        ttf_engine_t view = *owner;
        ttf_token_t *tokens = NULL;
        size_t count = 0, begin, end;
        view.context_memo = &memo;
        CHECK(ttf_tokenize(texts[item], strlen(texts[item]), &tokens, &count) == TTF_OK);
        /* Every contiguous view includes complete, partial and quote-like
           scopes. Revisit the full view after many entries/rehashes. */
        for (begin = 0; begin <= count; ++begin)
            for (end = begin; end <= count; ++end)
                scores_in_scope(owner, &view, texts[item], strlen(texts[item]), tokens, begin, end);
        scores_in_scope(owner, &view, texts[item], strlen(texts[item]), tokens, 0, count);
        CHECK(owner->context_memo == NULL);
        ttf_context_memo_clear(&memo);
        CHECK(!memo.count && !memo.capacity && !memo.slots && !memo.entries && memo.status == TTF_OK);
        ttf_tokens_free(tokens);
    }
}

static void mutate_token(ttf_engine_t *owner) {
    const char *text = "3kgis red";
    ttf_context_memo_t memo = {0};
    ttf_engine_t view = *owner;
    ttf_token_t *tokens = NULL;
    ttf_pack_input_t plain, cached;
    size_t count = 0, i;
    int before, after;
    view.context_memo = &memo;
    CHECK(ttf_tokenize(text, strlen(text), &tokens, &count) == TTF_OK);
    memset(&plain, 0, sizeof(plain));
    plain.text = text; plain.length = strlen(text); plain.tokens = tokens;
    plain.token_count = count; plain.engine = owner;
    cached = plain; cached.engine = &view;
    before = ttf_pack_en_US.context_score(&plain);
    CHECK(ttf_pack_en_US.context_score(&cached) == before);
    for (i = 0; i < count; ++i)
        if (tokens[i].script == TTF_SCRIPT_LATIN) {
            tokens[i].begin += strlen("kg");
            break;
        }
    CHECK(i < count);
    /* Simulate a consumed prefix exposing the finite verb "is". normalize.c
       invalidates after partial token consumption; pointer/count identity alone
       is insufficient. This is a token-view contract, not a linguistic gold. */
    ttf_context_memo_clear(&memo);
    after = ttf_pack_en_US.context_score(&plain);
    CHECK(before != after);
    CHECK(ttf_pack_en_US.context_score(&cached) == after);
    scores_in_scope(owner, &view, text, strlen(text), tokens, 0, count);
    ttf_context_memo_clear(&memo);
    ttf_tokens_free(tokens);
}

static size_t builds, disposals;
static const char key_a, key_b, key_fail, key_reentrant, key_nested;
static void dispose_data(void *data) { ++disposals; free(data); }
static ttf_status_t build_value(const ttf_pack_input_t *in, void **out) {
    size_t *value = (size_t *)malloc(sizeof(*value));
    ++builds;
    if (!value) return TTF_ENOMEM;
    *value = in->token_count;
    *out = value;
    return TTF_OK;
}
static ttf_status_t build_failure(const ttf_pack_input_t *in, void **out) {
    ttf_status_t status = build_value(in, out);
    return status == TTF_OK ? TTF_ELIMIT : status;
}
static ttf_status_t build_reentrant(const ttf_pack_input_t *in, void **out) {
    (void)ttf_context_memo_get(in, &key_reentrant, build_reentrant, dispose_data);
    return build_value(in, out); /* Cannot suppress the nested sticky error. */
}
static ttf_status_t build_nested(const ttf_pack_input_t *in, void **out) {
    ttf_result_t result = {0};
    const char *text = "Apple은 사과입니다.";
    ttf_status_t status = ttf_normalize(in->engine, text, strlen(text), &result);
    if (status != TTF_OK || !result.text || strcmp(result.text, "애플은 사과입니다.")) {
        ttf_result_free(&result);
        return TTF_ERESOURCE;
    }
    ttf_result_free(&result);
    return build_value(in, out);
}

static void memo_contract(ttf_engine_t *owner) {
    ttf_context_memo_t memo = {0};
    ttf_engine_t view = *owner;
    ttf_pack_input_t in;
    ttf_token_t *tokens = NULL;
    const char *text = "we go home";
    size_t count = 0, old_builds, old_disposals, *first, *second, i;
    static const unsigned char many_keys[80] = {0};
    CHECK(ttf_tokenize(text, strlen(text), &tokens, &count) == TTF_OK);
    memset(&in, 0, sizeof(in));
    in.text = text; in.length = strlen(text); in.tokens = tokens; in.token_count = count;
    in.engine = owner;
    CHECK(!ttf_context_memo_enabled(&in));
    CHECK(ttf_context_memo_get(&in, &key_a, build_value, dispose_data) == NULL);
    view.context_memo = &memo; in.engine = &view;
    CHECK(ttf_context_memo_enabled(&in));
    first = ttf_context_memo_get(&in, &key_a, build_value, dispose_data);
    CHECK(first && *first == count);
    old_builds = builds;
    in.index = count;
    CHECK(ttf_context_memo_get(&in, &key_a, build_value, dispose_data) == first && builds == old_builds);
    second = ttf_context_memo_get(&in, &key_b, build_value, dispose_data);
    CHECK(second && second != first);
    --in.token_count;
    CHECK(ttf_context_memo_get(&in, &key_a, build_value, dispose_data) != first);
    ++in.token_count;
    for (i = 0; i < sizeof(many_keys); ++i)
        CHECK(ttf_context_memo_get(&in, &many_keys[i], build_value, dispose_data) != NULL);
    CHECK(ttf_context_memo_get(&in, &key_a, build_value, dispose_data) == first && *first == count);
    CHECK(ttf_context_memo_get(&in, &key_nested, build_nested, dispose_data) != NULL);
    CHECK(memo.status == TTF_OK && owner->context_memo == NULL);
    old_builds = builds;
    ttf_context_memo_clear(&memo);
    CHECK(disposals == old_builds);
    old_disposals = disposals;
    CHECK(ttf_context_memo_get(&in, &key_fail, build_failure, dispose_data) == NULL);
    CHECK(memo.status == TTF_ELIMIT && disposals == old_disposals + 1);
    old_builds = builds;
    CHECK(ttf_context_memo_get(&in, NULL, NULL, NULL) == NULL);
    CHECK(memo.status == TTF_ELIMIT && builds == old_builds);
    ttf_context_memo_clear(&memo);
    CHECK(disposals == old_disposals + 1);
    old_disposals = disposals;
    CHECK(ttf_context_memo_get(&in, &key_reentrant, build_reentrant, dispose_data) == NULL);
    CHECK(memo.status == TTF_ERESOURCE && disposals == old_disposals + 1);
    ttf_context_memo_clear(&memo);
    CHECK(disposals == old_disposals + 1);
    CHECK(ttf_context_memo_get(&in, NULL, build_value, dispose_data) == NULL);
    CHECK(memo.status == TTF_EINVAL);
    ttf_context_memo_clear(&memo);
    ttf_tokens_free(tokens);
}

static void layered_resources(ttf_engine_t *engine) {
    static const char *rows[][4] = {
        {"context", "en-US", "phrase_ceiling", "12"},
        {"context", "en-US", "latin_weight", "0"},
        {"context", "en-US", "clause_weight", "19"},
        {"context_word", "en-US", "go", "0"},
        {"context_role", "en-US", "home", "subject"},
        {"context_cue", "en-US", "제목은", "37"},
        {"context", "ko-KR", "hangul_weight", "3"},
        {"context", "ko-KR", "clause_floor", "23"},
        {"context", "ko-KR", "particle_weight", "29"},
        {"context", "ko-KR", "script_weight", "11"},
        {"context_connector", "*", "#", "number"},
    };
    size_t i;
    CHECK(ttf_resource_set(engine, "context", "en-US", "clause_weight", "77", TTF_LAYER_DOMAIN) == TTF_OK);
    CHECK(ttf_resource_set(engine, "context", "ko-KR", "hangul_weight", "12", TTF_LAYER_DOMAIN) == TTF_OK);
    for (i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        CHECK(ttf_resource_set(engine, rows[i][0], rows[i][1], rows[i][2], rows[i][3], TTF_LAYER_CUSTOMER) == TTF_OK);
}

static int same_result(const ttf_result_t *a, const ttf_result_t *b) {
    size_t i;
    if (a->length != b->length || a->span_count != b->span_count ||
        memcmp(a->text, b->text, a->length)) return 0;
    for (i = 0; i < a->span_count; ++i) {
        const ttf_span_t *x = &a->spans[i], *y = &b->spans[i];
        const ttf_semantic_t *xs = &x->semantic, *ys = &y->semantic;
        if (x->begin != y->begin || x->end != y->end || x->input_begin != y->input_begin ||
            x->input_end != y->input_end || x->script != y->script ||
            x->pronunciation_layer != y->pronunciation_layer ||
            xs->type != ys->type || xs->lexical_class != ys->lexical_class ||
            xs->layer != ys->layer || xs->input_begin != ys->input_begin || xs->input_end != ys->input_end ||
            strcmp(xs->value, ys->value) || strcmp(xs->secondary, ys->secondary) ||
            strcmp(xs->unit, ys->unit) || strcmp(xs->source_lang, ys->source_lang) ||
            strcmp(xs->context_lang, ys->context_lang) || strcmp(xs->render_lang, ys->render_lang) ||
            strcmp(xs->rule, ys->rule) || strcmp(x->context_trace, y->context_trace) ||
            strcmp(x->lexicon, y->lexicon) || strcmp(x->verbalizer, y->verbalizer)) return 0;
    }
    return 1;
}
typedef struct {
    ttf_engine_t *engine;
    const char *text;
    ttf_result_t expected;
    int failed;
} worker_t;
static void *normalize_worker(void *userdata) {
    worker_t *worker = (worker_t *)userdata;
    size_t i;
    for (i = 0; i < 20; ++i) {
        ttf_result_t result = {0};
        if (ttf_normalize(worker->engine, worker->text, strlen(worker->text), &result) != TTF_OK ||
            !same_result(&worker->expected, &result)) worker->failed = 1;
        ttf_result_free(&result);
    }
    return NULL;
}
static void concurrent_requests(ttf_engine_t *owner) {
    static const char *inputs[] = {
        "3개입니다. Apple은 사과입니다.", "Please go to the 1st floor.",
        "그는 \"Apple released the iPhone.\"라고 말했습니다.", "Phone:+12125550100"
    };
    worker_t workers[4];
    size_t i;
#ifdef TTF_TEST_HAVE_THREADS
    pthread_t threads[4];
    int started[4] = {0};
#endif
    memset(workers, 0, sizeof(workers));
    for (i = 0; i < 4; ++i) {
        workers[i].engine = owner;
        workers[i].text = inputs[i];
        CHECK(ttf_normalize(owner, inputs[i], strlen(inputs[i]), &workers[i].expected) == TTF_OK);
#ifdef TTF_TEST_HAVE_THREADS
        started[i] = pthread_create(&threads[i], NULL, normalize_worker, &workers[i]) == 0;
        CHECK(started[i]);
#else
        (void)normalize_worker(&workers[i]);
#endif
    }
    for (i = 0; i < 4; ++i) {
#ifdef TTF_TEST_HAVE_THREADS
        if (started[i]) CHECK(pthread_join(threads[i], NULL) == 0);
#endif
        CHECK(!workers[i].failed);
        ttf_result_free(&workers[i].expected);
    }
    CHECK(owner->context_memo == NULL);
}
typedef struct {
    ttf_engine_t *engine;
    ttf_buffer_t copied;
    size_t callbacks;
    int failed, cancel;
} callback_state_t;
static int nested_callback(const ttf_segment_t *segment, void *userdata) {
    callback_state_t *state = (callback_state_t *)userdata;
    ttf_result_t nested = {0};
    const char *source = "Apple released the iPhone.";
    ++state->callbacks;
    if (ttf_normalize(state->engine, source, strlen(source), &nested) != TTF_OK ||
        !nested.text || strcmp(nested.text, source)) state->failed = 1;
    ttf_result_free(&nested);
    if (ttf_buffer_append(&state->copied, segment->text, segment->length) != TTF_OK)
        state->failed = 1;
    return state->cancel || state->failed;
}
static void stream_reentrancy(ttf_engine_t *owner) {
    const char *source = "3개입니다.\nApple은 사과입니다.\nPlease go to the 1st floor.\n";
    ttf_result_t batch = {0};
    ttf_stream_t *stream = NULL;
    callback_state_t state;
    size_t i;
    memset(&state, 0, sizeof(state));
    state.engine = owner;
    CHECK(ttf_normalize(owner, source, strlen(source), &batch) == TTF_OK);
    CHECK(ttf_stream_create(owner, nested_callback, &state, &stream) == TTF_OK);
    for (i = 0; i < strlen(source); ++i)
        CHECK(ttf_stream_write(stream, source + i, 1) == TTF_OK);
    CHECK(state.callbacks > 0); /* Safe sentences reached the callback before flush. */
    CHECK(ttf_stream_flush(stream) == TTF_OK && !state.failed);
    CHECK(state.copied.length == batch.length && !memcmp(state.copied.data, batch.text, batch.length));
    ttf_stream_destroy(stream);
    ttf_buffer_free(&state.copied);
    ttf_result_free(&batch);
    memset(&state, 0, sizeof(state));
    state.engine = owner; state.cancel = 1; stream = NULL;
    CHECK(ttf_stream_create(owner, nested_callback, &state, &stream) == TTF_OK);
    CHECK(ttf_stream_write(stream, source, strlen(source)) == TTF_ECALLBACK);
    CHECK(ttf_stream_write(stream, "x", 1) == TTF_ECALLBACK);
    CHECK(ttf_stream_flush(stream) == TTF_ECALLBACK);
    CHECK(state.callbacks == 1 && !state.failed && owner->context_memo == NULL);
    ttf_stream_destroy(stream);
    ttf_buffer_free(&state.copied);
}

static const char key_public_failure;
static int failure_context(const ttf_pack_input_t *input) {
    if (input->length >= 8 && !memcmp(input->text, "FAILMEMO", 8)) {
        (void)ttf_context_memo_get(input, &key_public_failure, build_failure, dispose_data);
        return 777; /* Core must propagate the memo error despite this score. */
    }
    return 0;
}
static ttf_status_t identity_verbalizer(const ttf_engine_t *engine, const ttf_semantic_t *semantic,
                                       const char *text, size_t length, ttf_buffer_t *output) {
    (void)engine; (void)semantic;
    return ttf_buffer_append(output, text, length);
}
static const ttf_language_pack_t error_pack = {
    .struct_size = sizeof(ttf_language_pack_t), .abi_version = TTF_ABI_VERSION,
    .language_tag = "zz-MEMO", .context_score = failure_context,
    .verbalize = identity_verbalizer
};
static int count_callback(const ttf_segment_t *segment, void *userdata) {
    size_t *count = (size_t *)userdata;
    (void)segment;
    ++*count;
    return 0;
}
static void request_error_contract(const char *resources) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_stream_t *stream = NULL;
    ttf_result_t result = {0};
    const char *failed = "FAILMEMO.\n", *valid = "Apple은 사과입니다.";
    size_t callbacks = 0, old_disposals = disposals;
    ttf_config_init(&config);
    config.resource_root = resources;
    CHECK(ttf_engine_create(&config, &engine) == TTF_OK);
    if (!engine) return;
    CHECK(ttf_engine_register_pack(engine, &error_pack) == TTF_OK);
    CHECK(ttf_engine_freeze(engine) == TTF_OK);
    CHECK(ttf_normalize(engine, failed, strlen(failed), &result) == TTF_ELIMIT);
    CHECK(!result.text && !result.spans && !result.length && !result.span_count);
    CHECK(disposals == old_disposals + 1 && engine->context_memo == NULL);
    CHECK(ttf_stream_create(engine, count_callback, &callbacks, &stream) == TTF_OK);
    CHECK(ttf_stream_write(stream, failed, strlen(failed)) == TTF_ELIMIT);
    CHECK(ttf_stream_write(stream, valid, strlen(valid)) == TTF_ELIMIT);
    CHECK(ttf_stream_flush(stream) == TTF_ELIMIT);
    CHECK(!callbacks && disposals == old_disposals + 2 && engine->context_memo == NULL);
    ttf_stream_destroy(stream);
    CHECK(ttf_normalize(engine, valid, strlen(valid), &result) == TTF_OK);
    CHECK(result.text && !strcmp(result.text, "애플은 사과입니다."));
    ttf_result_free(&result);
    ttf_engine_destroy(engine);
}

int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_engine_t *owner = NULL;
    size_t round;
    if (argc != 2) return 2;
    for (round = 0; round < 2; ++round) {
        ttf_config_init(&config);
        config.resource_root = argv[1];
        CHECK(ttf_engine_create(&config, &owner) == TTF_OK);
        if (!owner) return 2;
        if (round) layered_resources(owner);
        CHECK(ttf_engine_freeze(owner) == TTF_OK);
        score_oracle(owner);
        mutate_token(owner);
        if (!round) {
            memo_contract(owner);
            concurrent_requests(owner);
            stream_reentrancy(owner);
        }
        CHECK(owner->context_memo == NULL);
        ttf_engine_destroy(owner);
        owner = NULL;
    }
    request_error_contract(argv[1]);
    printf("context cache: %zu checks, %zu failures\n", checks, failures);
    return failures ? 1 : 0;
}
