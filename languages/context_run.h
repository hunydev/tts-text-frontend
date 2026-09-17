#ifndef TTF_LANGUAGE_CONTEXT_RUN_H
#define TTF_LANGUAGE_CONTEXT_RUN_H
#include "tts_text_frontend/ttf_language_pack.h"
#include "../src/language/context_memo.h"
#include <string.h>

/* A lexical run is local to the already committed context window. It crosses
   spaces inside a foreign phrase, but not a script switch or clause delimiter.
   This helper carries no language names, vocabulary or reading policy. */
static int context_run_ascii(const ttf_token_t *token) {
    return token->script == TTF_SCRIPT_LATIN || token->script == TTF_SCRIPT_DIGIT;
}
static int context_run_member(const ttf_pack_input_t *in, size_t index) {
    const ttf_token_t *token = &in->tokens[index];
    size_t j;
    if (context_run_ascii(token))
        return 1;
    if (token->script == TTF_SCRIPT_WHITESPACE) {
        for (j = token->begin; j < token->end; ++j)
            if (in->text[j] == '\r' || in->text[j] == '\n')
                return 0;
        return 1;
    }
    /* A universal currency prefix is part of the quantity and inherits the
       same lexical context as the following number. Prefixes remain data. */
    if (token->script == TTF_SCRIPT_SYMBOL || token->script == TTF_SCRIPT_PUNCTUATION) {
        char key[32];
        size_t width = token->end - token->begin;
        if (width < sizeof(key)) {
            memcpy(key, in->text + token->begin, width);
            key[width] = '\0';
            if (ttf_resource_get(in->engine, "currency_prefix", "*", key, NULL))
                return 1;
            if (ttf_resource_get(in->engine, "context_connector", "*", key, NULL)) {
                j = index;
                while (j && in->tokens[j - 1].script == TTF_SCRIPT_WHITESPACE)
                    --j;
                if (j && in->tokens[j - 1].script == TTF_SCRIPT_DIGIT)
                    return 1;
            }
        }
    }
    if (token->end == token->begin + 1) {
        char c = in->text[token->begin];
        /* Numeric sign / leading decimal point, including at the start of a
           run. The semantic classifier still decides whether syntax is valid. */
        if ((c == '+' || c == '-' || c == '.') && index + 1 < in->token_count &&
            in->tokens[index + 1].script == TTF_SCRIPT_DIGIT &&
            token->end == in->tokens[index + 1].begin)
            return 1;
        /* A digit grouping comma does not end the quantity's context run. */
        if (c == ',' && index && index + 1 < in->token_count &&
            in->tokens[index - 1].script == TTF_SCRIPT_DIGIT &&
            in->tokens[index + 1].script == TTF_SCRIPT_DIGIT &&
            in->tokens[index - 1].end == token->begin && token->end == in->tokens[index + 1].begin)
            return 1;
        /* Decimal/version components, contractions and hyphenated terms. */
        if ((c == '.' || c == '-' || c == '\'' || c == ':' || c == '/') && index &&
            index + 1 < in->token_count && context_run_ascii(&in->tokens[index - 1]) &&
            context_run_ascii(&in->tokens[index + 1]) &&
            in->tokens[index - 1].end == token->begin && token->end == in->tokens[index + 1].begin)
            return 1;
        /* Keep spellings such as C++ and C# in the same script run. */
        if (c == '+' || c == '#') {
            j = index;
            while (j && in->tokens[j - 1].end == in->tokens[j].begin) {
                const ttf_token_t *previous = &in->tokens[--j];
                if (context_run_ascii(previous))
                    return 1;
                if (previous->end != previous->begin + 1 ||
                    (in->text[previous->begin] != '+' && in->text[previous->begin] != '#'))
                    break;
            }
        }
    }
    return 0;
}
static void context_run_bounds(const ttf_pack_input_t *in, size_t *begin, size_t *end) {
    if (ttf_context_run_cached(in, begin, end))
        return;
    *begin = *end = in->index;
    if (in->index >= in->token_count || !context_run_member(in, in->index))
        return;
    *end = in->index + 1;
    while (*begin && context_run_member(in, *begin - 1))
        --*begin;
    while (*end < in->token_count && context_run_member(in, *end))
        ++*end;
    while (*begin < *end && in->tokens[*begin].script == TTF_SCRIPT_WHITESPACE)
        ++*begin;
    while (*end > *begin && in->tokens[*end - 1].script == TTF_SCRIPT_WHITESPACE)
        --*end;
}
#endif
