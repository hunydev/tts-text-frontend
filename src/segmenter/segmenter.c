#include "../core/internal.h"
static int protected_period(const ttf_engine_t *e, const char *s, size_t at) {
    size_t begin = at, n;
    char key[96];
    if (at && s[at - 1] >= '0' && s[at - 1] <= '9')
        return 1;
    while (begin && ((s[begin - 1] >= 'a' && s[begin - 1] <= 'z') ||
                     (s[begin - 1] >= 'A' && s[begin - 1] <= 'Z') || s[begin - 1] == '.'))
        begin--;
    n = at - begin;
    if (!n || n >= sizeof(key))
        return 0;
    memcpy(key, s + begin, n);
    key[n] = 0;
    for (begin = 0; begin < n; begin++)
        if (key[begin] >= 'A' && key[begin] <= 'Z')
            key[begin] += 'a' - 'A';
    if (ttf_resource_get(e, "abbreviation", "*", key, NULL))
        return 1;
    for (begin = 0; begin < e->pack_count; begin++)
        if (ttf_resource_get(e, "abbreviation", e->packs[begin]->language_tag, key, NULL))
            return 1;
    return 0;
}
size_t ttf_next_boundary(const ttf_engine_t *e, const char *s, size_t n, int final, int first) {
    ttf_token_t *tokens = NULL;
    size_t count = 0, i, at, chars = 0,
           target = first ? e->config.first_segment_target : e->config.normal_segment_target;
    size_t selected = 0;
    uint32_t quote_stack[32];
    size_t quote_depth = 0;
    if (ttf_tokenize(s, n, &tokens, &count) != TTF_OK)
        return 0;
    for (i = 0; i < count; i++) {
        const ttf_token_t *t = &tokens[i];
        /* The same lexical categories drive routing, semantic recognition and
           safe cuts. Only whitespace cuts can be used for phrase segmentation. */
        for (at = t->begin; at < t->end;) {
            uint32_t cp, next = 0;
            size_t w, nw = 0;
            int sentence = 0;
            if (ttf_utf8_decode(s + at, n - at, &cp, &w) != 1)
                goto done;
            chars++;
            if (quote_depth && cp == quote_stack[quote_depth - 1])
                quote_depth--;
            else if (ttf_quote_closer(cp) && quote_depth < 32)
                quote_stack[quote_depth++] = ttf_quote_closer(cp);
            if (e->config.sentence_split && !quote_depth) {
                if (cp == '\n')
                    sentence = 1;
                else if (cp == '.' || cp == '!' || cp == '?' || cp == 0x3002 || cp == 0xff01 ||
                         cp == 0xff1f) {
                    int has_next =
                        at + w < n && ttf_utf8_decode(s + at + w, n - at - w, &next, &nw) == 1;
                    if ((has_next && ttf_script_of(next) == TTF_SCRIPT_WHITESPACE) ||
                        (!has_next && final))
                        sentence = 1;
                    if (cp == '.' && protected_period(e, s, at))
                        sentence = 0;
                }
            }
            if (sentence) {
                selected = at + w;
                goto done;
            }
            if (!quote_depth && t->script == TTF_SCRIPT_WHITESPACE &&
                ((e->config.phrase_split && chars >= target) ||
                 chars >= e->config.max_segment_length)) {
                size_t previous = at;
                while (previous && (s[previous - 1] == ' ' || s[previous - 1] == '\t' ||
                                    s[previous - 1] == '\r'))
                    previous--;
                /* A number, sign, currency prefix or fraction separator may
                   bind to the following token even across spaces. Retain it
                   without inspecting unavailable future input. */
                if (previous &&
                    ((s[previous - 1] >= '0' && s[previous - 1] <= '9') || s[previous - 1] == '/' ||
                     s[previous - 1] == ':' || s[previous - 1] == '+' || s[previous - 1] == '-')) {
                    at += w;
                    continue;
                }
                if (i && tokens[i - 1].end - tokens[i - 1].begin < 64) {
                    char key[64];
                    size_t key_length = tokens[i - 1].end - tokens[i - 1].begin;
                    memcpy(key, s + tokens[i - 1].begin, key_length);
                    key[key_length] = 0;
                    if (ttf_resource_get(e, "currency_prefix", "*", key, NULL)) {
                        at += w;
                        continue;
                    }
                    if (i >= 2) {
                        size_t previous_token = i - 1, pack;
                        int continuation = 0;
                        while (previous_token &&
                               tokens[previous_token - 1].script == TTF_SCRIPT_WHITESPACE)
                            previous_token--;
                        if (previous_token &&
                            tokens[previous_token - 1].script == TTF_SCRIPT_DIGIT) {
                            continuation =
                                ttf_resource_get(e, "numeric_continuation", "*", key, NULL) != NULL;
                            for (pack = 0; !continuation && pack < e->pack_count; ++pack)
                                continuation = ttf_resource_get(e, "numeric_continuation",
                                                                e->packs[pack]->language_tag, key,
                                                                NULL) != NULL;
                        }
                        if (continuation) {
                            at += w;
                            continue;
                        }
                    }
                }
                /* A spaced date or abbreviation may continue after this whitespace.
                   Never freeze its leading number merely to hit a phrase target. */
                if (!(previous && s[previous - 1] == '.' && protected_period(e, s, previous - 1))) {
                    selected = at + w;
                    goto done;
                }
            }
            at += w;
        }
    }
    if (final)
        selected = n;
done:
    free(tokens);
    return selected;
}
