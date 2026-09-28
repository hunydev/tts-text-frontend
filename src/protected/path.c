#include "protected.h"

/* This is a lexical guard, not a filesystem parser. Requiring an explicit path
   prefix keeps ordinary words, fractions and URLs in their existing pipelines.
   ASCII single/double quotes may enclose a path containing spaces; the quotes
   are part of the protected source span. */
static int ascii_alpha(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int path_left_boundary(const char *s, size_t begin) {
    unsigned char previous;
    size_t word_begin, i;
    if (!begin)
        return 1;
    previous = (unsigned char)s[begin - 1];
    if (previous == ' ' || previous == '\t' || previous == '\n' || previous == '\r')
        return 1;
    if (previous != '(' && previous != '[' && previous != '{' && previous != '=' &&
        previous != '"' && previous != '\'' && previous != '`')
        return 0;
    /* An equals sign or opening delimiter inside a URL query is not a path
       boundary. Leave the complete URL to the universal URL recognizer. */
    word_begin = begin;
    while (word_begin && (unsigned char)s[word_begin - 1] > ' ')
        --word_begin;
    for (i = word_begin; i + 2 < begin; ++i) {
        if (s[i] == ':' && s[i + 1] == '/' && s[i + 2] == '/')
            return 0;
        if (i + 3 < begin && !memcmp(s + i, "www.", 4))
            return 0;
    }
    return 1;
}

static int path_character(unsigned char c) {
    return ascii_alpha(c) || (c >= '0' && c <= '9') || c == '/' || c == '\\' || c == '.' ||
           c == '_' || c == '-' || c == '~' || c == '+' || c == '@' || c == '$';
}

static int set_match(ttf_protected_match_t *match, size_t end, int complete,
                      ttf_resource_layer_t layer) {
    match->end = end;
    match->kind = "path";
    match->rule = "protected.path";
    match->layer = layer;
    match->complete = complete;
    return 1;
}

int ttf_protect_path(const ttf_engine_t *engine, const char *s, size_t n,
                     size_t begin, int final, ttf_protected_match_t *match) {
    const char *policy;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON;
    size_t p, prefix_end, end, separators = 0, component_length = 0;
    int named = 0, drive = 0, unc = 0;
    unsigned char quote = 0;
    if (!engine || !s || !match || begin >= n)
        return 0;
    policy = ttf_resource_get(engine, "protection", "*", "path", &layer);
    if (!policy || strcmp(policy, "1"))
        return 0;
    p = begin;
    if (s[p] == '"' || s[p] == '\'') {
        quote = (unsigned char)s[p++];
        /* An isolated opening quote is not enough to identify a path. */
        if (p == n)
            return 0;
    }
    /* Reject non-prefixes before scanning left for URL query context. This is
       constant work for ordinary words and long punctuation runs. */
    if (s[p] != '/' && s[p] != '.' && s[p] != '~' && s[p] != '\\' &&
        !(ascii_alpha((unsigned char)s[p]) && (p + 1 == n || s[p + 1] == ':')))
        return 0;
    if (!path_left_boundary(s, begin))
        return 0;
    if (s[p] == '/') {
        ++p;
        /* Protocol-relative URLs use //, not the explicit Windows UNC form. */
        if (p < n && s[p] == '/')
            return 0;
    } else if (s[p] == '.' || s[p] == '~') {
        unsigned char first = (unsigned char)s[p++];
        if (first == '.' && p < n && s[p] == '.')
            ++p;
        if (p == n)
            return !final ? set_match(match, n, 0, layer) : 0;
        if (s[p] != '/')
            return 0;
        ++p;
    } else if (s[p] == '\\') {
        ++p;
        if (p == n)
            return !final ? set_match(match, n, 0, layer) : 0;
        if (s[p++] != '\\')
            return 0;
        unc = 1;
    } else if (ascii_alpha((unsigned char)s[p])) {
        ++p;
        if (p == n)
            return !final ? set_match(match, n, 0, layer) : 0;
        if (s[p++] != ':')
            return 0;
        if (p == n)
            return !final ? set_match(match, n, 0, layer) : 0;
        if (s[p] != '\\' && s[p] != '/')
            return 0;
        ++p;
        drive = 1;
    } else
        return 0;
    prefix_end = p;
    if (quote) {
        /* After an explicit prefix, quoted source is opaque, including spaces
           and UTF-8 filenames. An unclosed quote is retained until flush; its
           final raw tail remains literal rather than localizing path fragments.
           Quotes are delimiters, with no shell escape or expansion semantics. */
        while (p < n && (unsigned char)s[p] != quote)
            ++p;
        return set_match(match, p < n ? p + 1 : n, p < n || final, layer);
    }
    while (p < n) {
        unsigned char c = (unsigned char)s[p];
        if (c >= 128) {
            uint32_t cp;
            size_t width;
            if (ttf_utf8_decode(s + p, n - p, &cp, &width) != 1 || cp < 0xff10 || cp > 0xff19)
                break;
            ++component_length;
            p += width;
            continue;
        }
        if (!path_character(c))
            break;
        if (ascii_alpha(c) || c == '_')
            named = 1;
        if (c == '/' || c == '\\') {
            /* A UNC path needs both a nonempty host and a nonempty share. */
            if (component_length)
                ++separators;
            component_length = 0;
        } else
            ++component_length;
        ++p;
    }
    if (p == n && !final)
        return set_match(match, n, 0, layer);
    end = p;
    /* A prose full stop belongs to the surrounding sentence. A quoted path is
       unambiguous and therefore retains any trailing dots verbatim. */
    while (end > prefix_end && s[end - 1] == '.')
        --end;
    if (!named && !drive)
        return 0;
    if (unc && separators + (component_length != 0) < 2)
        return 0;
    if (end == prefix_end && !drive)
        return 0;
    return set_match(match, end, 1, layer);
}
