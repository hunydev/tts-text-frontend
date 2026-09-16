#include "protected.h"
#include "../core/internal.h"

/* This is a literal-span scanner, not an HTML or Markdown interpreter.  The
 * finite grammar intentionally never removes markup or decodes its content. */
#define TTF_TAG_NAME_LIMIT 63u
#define TTF_CODE_NESTING_LIMIT 32u
#define TTF_BACKTICK_LIMIT 16u

typedef struct {
    size_t end;
    char name[TTF_TAG_NAME_LIMIT + 1];
    int closing;
    int self_closing;
} markup_tag_t;

static int ascii_letter(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

static int ascii_digit(unsigned char c) {
    return c >= '0' && c <= '9';
}

static int tag_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f';
}

static int name_char(unsigned char c) {
    return ascii_letter(c) || ascii_digit(c) || c == ':' || c == '_' || c == '-';
}

static unsigned char ascii_lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}

static int enabled(const ttf_engine_t *e, const char *key, ttf_resource_layer_t *layer) {
    const char *value = ttf_resource_get(e, "protection", "*", key, layer);
    return value && !strcmp(value, "1");
}

static int result(ttf_protected_match_t *out, size_t end, const char *kind,
                  const char *rule, ttf_resource_layer_t layer, int complete) {
    out->end = end;
    out->kind = kind;
    out->rule = rule;
    out->layer = layer;
    out->complete = complete;
    return 1;
}

/* 1: syntactically complete tag; -1: valid prefix requiring more bytes;
 * 0: not a tag.  All reads are bounded by length, including attribute values. */
static int parse_tag(const char *s, size_t length, size_t begin, markup_tag_t *tag) {
    size_t p = begin + 1, name_start, name_length;
    memset(tag, 0, sizeof(*tag));
    if (p == length)
        return -1;
    if (s[p] == '/') {
        tag->closing = 1;
        if (++p == length)
            return -1;
    }
    if (!ascii_letter((unsigned char)s[p]))
        return 0;
    name_start = p;
    while (p < length && name_char((unsigned char)s[p]))
        ++p;
    name_length = p - name_start;
    if (name_length > TTF_TAG_NAME_LIMIT)
        return 0;
    {
        size_t i;
        for (i = 0; i < name_length; ++i)
            tag->name[i] = (char)ascii_lower((unsigned char)s[name_start + i]);
    }
    for (;;) {
        int whitespace = 0;
        while (p < length && tag_space((unsigned char)s[p])) {
            ++p;
            whitespace = 1;
        }
        if (p == length)
            return -1;
        if (s[p] == '>') {
            tag->end = p + 1;
            return 1;
        }
        if (s[p] == '/' && !tag->closing) {
            if (++p == length)
                return -1;
            if (s[p] != '>')
                return 0;
            tag->self_closing = 1;
            tag->end = p + 1;
            return 1;
        }
        if (tag->closing || !whitespace ||
            !(ascii_letter((unsigned char)s[p]) || s[p] == '_' || s[p] == ':'))
            return 0;
        while (p < length && name_char((unsigned char)s[p]))
            ++p;
        if (p == length)
            return -1;
        /* A boolean attribute may be followed by another attribute.  Keep
         * that whitespace available for the next iteration. */
        {
            size_t after_name = p;
            while (p < length && tag_space((unsigned char)s[p]))
                ++p;
            if (p == length)
                return -1;
            if (s[p] != '=') {
                p = after_name;
                continue;
            }
        }
        ++p;
        while (p < length && tag_space((unsigned char)s[p]))
            ++p;
        if (p == length)
            return -1;
        if (s[p] == '\'' || s[p] == '"') {
            char quote = s[p++];
            while (p < length && s[p] != quote)
                ++p;
            if (p == length)
                return -1;
            ++p;
        } else {
            size_t value_start = p;
            while (p < length && !tag_space((unsigned char)s[p]) && s[p] != '>') {
                unsigned char c = (unsigned char)s[p];
                if (c < 0x20 || c == '"' || c == '\'' || c == '<' || c == '=' || c == '`')
                    return 0;
                ++p;
            }
            if (p == value_start)
                return 0;
            if (p == length)
                return -1;
        }
    }
}

static int prefix_of(const char *s, size_t length, size_t begin, const char *prefix) {
    size_t n = strlen(prefix), available = length - begin;
    if (available > n)
        available = n;
    return !memcmp(s + begin, prefix, available);
}

/* Returns the end of a full comment, or zero for an incomplete comment. */
static size_t comment_end(const char *s, size_t length, size_t begin) {
    size_t p;
    if (length - begin < 4)
        return 0;
    for (p = begin + 4; length - p >= 3; ++p)
        if (s[p] == '-' && s[p + 1] == '-' && s[p + 2] == '>')
            return p + 3;
    return 0;
}

static int backticks(const char *s, size_t length, size_t begin, int final,
                     ttf_resource_layer_t layer, ttf_protected_match_t *out) {
    size_t count = 0, p = begin;
    /* Never reinterpret a suffix of an unsupported long delimiter run. */
    if (begin && s[begin - 1] == '`')
        return 0;
    while (p < length && s[p] == '`') {
        ++p;
        if (++count > TTF_BACKTICK_LIMIT)
            return 0;
    }
    if (p == length)
        return result(out, length, "code", final ? "protected.unclosed-code" : "protected.code",
                      layer, final);
    while (p < length) {
        size_t close_begin;
        if (s[p] != '`') {
            ++p;
            continue;
        }
        close_begin = p;
        while (p < length && s[p] == '`')
            ++p;
        /* At a write boundary the exact closing run is not yet known. */
        if (p == length && !final)
            return result(out, length, "code", "protected.code", layer, 0);
        if (p - close_begin == count)
            return result(out, p, "code", "protected.code", layer, 1);
    }
    return result(out, length, "code", final ? "protected.unclosed-code" : "protected.code",
                  layer, final);
}

static int opaque_element(const char *s, size_t length, const markup_tag_t *opening,
                          int final, ttf_resource_layer_t layer, ttf_protected_match_t *out) {
    size_t p = opening->end;
    unsigned depth = 1;
    while (p < length) {
        markup_tag_t tag;
        int status;
        if (s[p] != '<') {
            ++p;
            continue;
        }
        if (prefix_of(s, length, p, "<!--")) {
            size_t end = comment_end(s, length, p);
            if (!end)
                break;
            p = end;
            continue;
        }
        status = parse_tag(s, length, p, &tag);
        if (status < 0)
            break;
        if (!status) {
            ++p;
            continue;
        }
        if (!strcmp(tag.name, opening->name)) {
            if (tag.closing) {
                if (!--depth)
                    return result(out, tag.end, "code", "protected.code", layer, 1);
            } else if (!tag.self_closing) {
                /* Fail closed by preserving the entire tail.  This avoids
                 * interpreting deeply nested code as ordinary spoken text. */
                if (depth == TTF_CODE_NESTING_LIMIT)
                    return result(out, length, "code", "protected.unclosed-code", layer, final);
                ++depth;
            }
        }
        p = tag.end;
    }
    return result(out, length, "code", final ? "protected.unclosed-code" : "protected.code",
                  layer, final);
}

int ttf_protect_markup(const ttf_engine_t *e, const char *s, size_t length, size_t begin,
                       int final, ttf_protected_match_t *out) {
    ttf_resource_layer_t markup_layer = TTF_LAYER_COMMON, code_layer = TTF_LAYER_COMMON;
    int markup_on, code_on, status;
    markup_tag_t tag;
    const char *opaque;
    if (!e || !s || !out || begin >= length)
        return 0;
    markup_on = enabled(e, "markup", &markup_layer);
    code_on = enabled(e, "code", &code_layer);
    if (s[begin] == '`')
        return code_on ? backticks(s, length, begin, final, code_layer, out) : 0;
    if (s[begin] != '<' || (!markup_on && !code_on))
        return 0;
    if (length - begin >= 2 && s[begin + 1] == '!' && prefix_of(s, length, begin, "<!--")) {
        size_t end;
        if (!markup_on)
            return 0;
        end = comment_end(s, length, begin);
        return result(out, end ? end : length, "markup",
                      end || !final ? "protected.comment" : "protected.unclosed-comment",
                      markup_layer, end ? 1 : final);
    }
    status = parse_tag(s, length, begin, &tag);
    if (!status)
        return 0;
    if (status < 0) {
        if (!markup_on) {
            /* Code-only mode may retain an ambiguous opening-name prefix only
             * while more input can extend it. A delimiter or final flush
             * establishes the name, which must select an opaque resource. */
            if (tag.closing || (!tag.name[0] && final))
                return 0;
            if (tag.name[0]) {
                size_t p = begin + 1 + strlen(tag.name);
                if (p < length || final) {
                    ttf_resource_layer_t tag_layer = TTF_LAYER_COMMON;
                    opaque = ttf_resource_get(e, "protected_tag", "*", tag.name, &tag_layer);
                    if (!opaque || strcmp(opaque, "opaque"))
                        return 0;
                    if (tag_layer > code_layer)
                        code_layer = tag_layer;
                }
            }
        }
        return result(out, length, markup_on ? "markup" : "code",
                      final ? (markup_on ? "protected.unclosed-markup" : "protected.unclosed-code")
                            : (markup_on ? "protected.markup" : "protected.code"),
                      markup_on ? markup_layer : code_layer, final);
    }
    {
        ttf_resource_layer_t tag_layer = TTF_LAYER_COMMON;
        opaque = ttf_resource_get(e, "protected_tag", "*", tag.name, &tag_layer);
        if (code_on && opaque && !strcmp(opaque, "opaque") && !tag.closing && !tag.self_closing) {
            if (tag_layer > code_layer)
                code_layer = tag_layer;
            return opaque_element(s, length, &tag, final, code_layer, out);
        }
    }
    if (!markup_on)
        return 0;
    return result(out, tag.end, "markup", "protected.markup", markup_layer, 1);
}
