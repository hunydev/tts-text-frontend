#include "../../src/semantic/phone.h"
#include <time.h>

#ifndef TTF_PHONE_SCAN_LARGE
#define TTF_PHONE_SCAN_LARGE 100000
#endif

/* A generous CTest timeout is the only timing gate. The complete normalization
   pipeline has other context-scoring costs on enormous clauses; this test
   isolates the phone classifier's work on an ordinary list of numbers. */
static size_t checks;

static int control(ttf_engine_t *engine, const char *text, ttf_semantic_type_t type,
                   const char *value) {
    ttf_token_t *tokens = NULL;
    size_t count = 0, consumed;
    ttf_pack_input_t input;
    ttf_semantic_t semantic;
    if (ttf_tokenize(text, strlen(text), &tokens, &count) != TTF_OK)
        return 0;
    memset(&input, 0, sizeof(input));
    input.text = text;
    input.length = strlen(text);
    input.tokens = tokens;
    input.token_count = count;
    input.engine = engine;
    memset(&semantic, 0, sizeof(semantic));
    semantic.struct_size = sizeof(semantic);
    consumed = ttf_classify_phone(&input, &semantic);
    ++checks;
    if (consumed != count || semantic.type != type || strcmp(semantic.value, value)) {
        fprintf(stderr, "phone control failed: %s (%zu/%zu, type %d, value %s)\n",
                text, consumed, count, (int)semantic.type, semantic.value);
        ttf_tokens_free(tokens);
        return 0;
    }
    ttf_tokens_free(tokens);
    return 1;
}

static int ordinary_list(ttf_engine_t *engine, size_t numbers) {
    const size_t length = numbers * 2 - 1;
    char *text = (char *)malloc(length + 1);
    ttf_token_t *tokens = NULL;
    ttf_pack_input_t input;
    ttf_semantic_t untouched, semantic;
    size_t count = 0, i, seen = 0;
    clock_t start;
    if (!text)
        return 0;
    for (i = 0; i < length; ++i)
        text[i] = i % 2 ? ' ' : '7';
    text[length] = '\0';
    if (ttf_tokenize(text, length, &tokens, &count) != TTF_OK) {
        free(text);
        return 0;
    }
    memset(&input, 0, sizeof(input));
    input.text = text;
    input.length = length;
    input.tokens = tokens;
    input.token_count = count;
    input.engine = engine;
    memset(&untouched, 0, sizeof(untouched));
    untouched.struct_size = sizeof(untouched);
    untouched.type = TTF_SEM_CUSTOM;
    strcpy(untouched.value, "caller-owned-sentinel");
    start = clock();
    for (i = 0; i < count; ++i) {
        if (tokens[i].script != TTF_SCRIPT_DIGIT)
            continue;
        input.index = i;
        semantic = untouched;
        /* No phone cue exists anywhere in the numeric list. The classifier
           must decline every suffix without changing the caller's IR. */
        if (ttf_classify_phone(&input, &semantic) != 0 ||
            semantic.type != TTF_SEM_CUSTOM || strcmp(semantic.value, untouched.value)) {
            fprintf(stderr, "ordinary list falsely claimed at token %zu/%zu\n", i, count);
            ttf_tokens_free(tokens);
            free(text);
            return 0;
        }
        ++checks;
        ++seen;
    }
    printf("phone scan: %zu numbers, %.3f CPU seconds\n", numbers,
           (double)(clock() - start) / (double)CLOCKS_PER_SEC);
    ttf_tokens_free(tokens);
    free(text);
    return seen == numbers;
}

int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    int passed;
    if (argc != 2)
        return 2;
    ttf_config_init(&config);
    config.resource_root = argv[1];
    config.default_language = "en-US";
    if (ttf_engine_create(&config, &engine) != TTF_OK || ttf_engine_freeze(engine) != TTF_OK) {
        ttf_engine_destroy(engine);
        return 2;
    }
    passed = control(engine, "+1 212 555 0100", TTF_SEM_PHONE, "+1-212-555-0100") &&
             control(engine, "212-555-0100", TTF_SEM_PHONE, "212-555-0100") &&
             control(engine, "+1 ((212)) 555-0100", TTF_SEM_TEXT, "") &&
             ordinary_list(engine, 10000) && ordinary_list(engine, TTF_PHONE_SCAN_LARGE);
    ttf_engine_destroy(engine);
    printf("phone scan: %zu independent classification checks\n", checks);
    return passed ? 0 : 1;
}
