#include "../core/internal.h"
/* Small typed pattern DSL: literal spans and {digits}. Generic recognizers
   implement syntax, external tables select customer/domain readings. */
static size_t pattern_match(const char *pattern, const char *text, size_t n, size_t *specificity) {
    size_t i = 0, p = 0, s = 0;
    while (pattern[p]) {
        if (!strncmp(pattern + p, "{digits}", 8)) {
            size_t begin = i;
            while (i < n && text[i] >= '0' && text[i] <= '9')
                i++;
            if (i == begin)
                return 0;
            p += 8;
        } else {
            if (i == n || pattern[p] != text[i])
                return 0;
            i++;
            p++;
            s++;
        }
    }
    *specificity = s;
    return i;
}
const ttf_entry_t *ttf_rule_match(const ttf_engine_t *e, const char *lang, const char *text,
                                  size_t n, size_t *length) {
    const ttf_entry_t *best = NULL, *p;
    size_t bucket_index, best_len = 0, best_spec = 0;
    int best_exact = 0;
    for (bucket_index = 0; bucket_index < TTF_HASH_BUCKETS; bucket_index++)
        for (p = e->resources[bucket_index]; p; p = p->next) {
            size_t len, spec;
            int exact = !strcmp(p->kind, "exact");
            if (!exact && strcmp(p->kind, "pattern"))
                continue;
            if (strcmp(p->lang, "*") && strcmp(p->lang, lang))
                continue;
            if (exact) {
                len = strlen(p->key);
                spec = len;
                if (len > n || memcmp(p->key, text, len))
                    continue;
            } else {
                len = pattern_match(p->key, text, n, &spec);
                if (!len)
                    continue;
            }
            /* Match complete lexical units, while allowing script transitions. */
            if (len < n) {
                uint32_t left, right;
                size_t w, start = len - 1;
                while (start && ((unsigned char)text[start] & 0xc0) == 0x80)
                    start--;
                if (ttf_utf8_decode(text + start, len - start, &left, &w) != 1 ||
                    ttf_utf8_decode(text + len, n - len, &right, &w) != 1)
                    continue;
                if (ttf_script_of(left) == ttf_script_of(right) &&
                    (ttf_script_of(right) == TTF_SCRIPT_LATIN ||
                     ttf_script_of(right) == TTF_SCRIPT_DIGIT ||
                     ttf_script_of(right) == TTF_SCRIPT_HANGUL))
                    continue;
            }
            if (!best || p->layer > best->layer ||
                (p->layer == best->layer &&
                 (exact > best_exact ||
                  (exact == best_exact &&
                   (len > best_len ||
                    (len == best_len &&
                     (spec > best_spec || (spec == best_spec && p->order < best->order)))))))) {
                best = p;
                best_len = len;
                best_spec = spec;
                best_exact = exact;
            }
        }
    *length = best_len;
    return best;
}
