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
/* The exact rules whose key is text[0, len) for lang or `*`, then the rule list (patterns
   and long exact keys): the matching rules in turn, through *cursor (2026-10-25: a million
   exact rules scanned in a list took seconds a sentence). */
typedef struct {
    const ttf_entry_t *chain;
    uint64_t lengths;
    size_t len, pass;
    const char *lang;
    char key[64];
    int listed;
} rule_cursor_t;
static const ttf_entry_t *next_rule(const ttf_engine_t *e, const char *text, size_t n, rule_cursor_t *c) {
    for (;;) {
        if (c->listed)
            return c->chain ? (c->chain = c->chain->rule_next, c->chain) : NULL;
        while (c->chain) {
            const ttf_entry_t *p = c->chain;
            c->chain = p->next;
            if (!strcmp(p->kind, "exact") && !strcmp(p->key, c->key) &&
                !strcmp(p->lang, c->pass ? "*" : c->lang))
                return p;
        }
        if (!c->pass && strcmp(c->lang, "*")) {
            c->pass = 1;
            c->chain = ttf_resource_chain(e, ttf_entry_hash("exact", "*", c->key));
            continue;
        }
        do
            ++c->len;
        while (c->len < 63 && c->len <= n && !((c->lengths >> c->len) & 1));
        if (c->len >= 63 || c->len > n) {
            c->listed = 1;
            c->chain = e->rules;
            return c->chain;
        }
        memcpy(c->key, text, c->len);
        c->key[c->len] = 0;
        c->pass = 0;
        c->chain = ttf_resource_chain(e, ttf_entry_hash("exact", c->lang, c->key));
    }
}
const ttf_entry_t *ttf_rule_match(const ttf_engine_t *e, const char *lang, const char *text,
                                  size_t n, size_t *length) {
    const ttf_entry_t *best = NULL, *p;
    size_t best_len = 0, best_spec = 0;
    int best_exact = 0;
    rule_cursor_t cursor;
    memset(&cursor, 0, sizeof(cursor));
    cursor.lang = lang;
    cursor.lengths = e->rules_indexed ? ttf_resource_key_lengths(e, "exact", lang) : 0;
    cursor.pass = 1; /* the first call moves to the first key length */
    if (!e->rules_indexed) {
        cursor.listed = 1;
        cursor.chain = e->rules;
    }
    for (p = cursor.listed ? e->rules : next_rule(e, text, n, &cursor); p; p = next_rule(e, text, n, &cursor)) {
        size_t len, spec;
        int exact = !strcmp(p->kind, "exact");
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
