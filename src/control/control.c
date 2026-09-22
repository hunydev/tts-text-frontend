#include "control.h"

static int digit(char c) {
    return c >= '0' && c <= '9';
}
static int escaped(char c) {
    return c == '\\' || c == '{' || c == '}' || c == '[' || c == ']';
}
static int pattern_character(char c, int first) {
    return (c >= 'a' && c <= 'z') || (!first && (digit(c) || c == '_' || c == '-'));
}
static ttf_script_t script_at(const char *text, size_t length, size_t at, size_t *width) {
    uint32_t cp;
    *width = 0;
    if (at >= length || ttf_utf8_decode(text + at, length - at, &cp, width) != 1)
        return TTF_SCRIPT_SYMBOL;
    return ttf_script_of(cp);
}
static int alnum_script(ttf_script_t script) {
    return script == TTF_SCRIPT_LATIN || script == TTF_SCRIPT_DIGIT;
}
/* Atom boundaries use scanner categories. Latin and digit runs may combine;
   decimal/grouping punctuation stays inside a numeric atom for pack validation. */
static size_t atom_end(const char *text, size_t length, size_t begin) {
    size_t p = begin, width;
    ttf_script_t first;
    int numeric = digit(text[p]);
    if ((text[p] == '+' || text[p] == '-') && p + 1 < length && digit(text[p + 1])) {
        numeric = 1;
        ++p;
    }
    first = script_at(text, length, p, &width);
    if (!width || !(alnum_script(first) || first == TTF_SCRIPT_HANGUL || first == TTF_SCRIPT_OTHER))
        return begin;
    while (p < length) {
        ttf_script_t script = script_at(text, length, p, &width);
        if (!width)
            break;
        if ((alnum_script(first) && alnum_script(script)) ||
            (!alnum_script(first) && first == script)) {
            p += width;
            continue;
        }
        if (numeric && (text[p] == '.' || text[p] == ',') && p > begin && digit(text[p - 1]) &&
            p + 1 < length && digit(text[p + 1])) {
            ++p;
            continue;
        }
        break;
    }
    return p;
}
/* A script-boundary subtoken inside one Latin/digit atom cannot acquire its own
   override. This also avoids repeatedly rescanning a long alternating atom. */
static int atom_continuation(const char *text, size_t length, size_t begin) {
    size_t previous, width, ignored;
    ttf_script_t left, right;
    if (!begin)
        return 0;
    previous = begin - 1;
    while (previous && ((unsigned char)text[previous] & 0xc0u) == 0x80u)
        --previous;
    left = script_at(text, length, previous, &width);
    right = script_at(text, length, begin, &ignored);
    if (previous + width == begin && alnum_script(left) && alnum_script(right))
        return 1;
    if (digit(text[begin]) && begin >= 2 && (text[begin - 1] == '.' || text[begin - 1] == ',') &&
        digit(text[begin - 2]))
        return 1;
    return 0;
}
static ttf_status_t incomplete(const ttf_engine_t *engine, size_t length, int final, char quote,
                               ttf_control_match_t *match) {
    match->end = length;
    match->wait_quote = quote;
    if (length - match->begin > engine->frontend_options.max_control_bytes)
        return TTF_ELIMIT;
    return final ? TTF_EINVAL : TTF_OK;
}
static ttf_status_t payload(const ttf_engine_t *engine, const char *text, size_t length,
                            size_t opening, int final, ttf_control_match_t *match) {
    size_t p = opening + 1;
    match->payload_begin = p;
    match->kind = p < length && text[p] == '@' ? TTF_CONTROL_PATTERN : TTF_CONTROL_LITERAL;
    while (p < length) {
        if (p - match->begin >= engine->frontend_options.max_control_bytes)
            return TTF_ELIMIT;
        if (text[p] == '}') {
            size_t i, n = p - match->payload_begin;
            if (!n)
                return TTF_EINVAL;
            if (match->kind == TTF_CONTROL_PATTERN) {
                /* The public CUSTOM.unit field includes '@' and its terminator. */
                if (match->grouped || n < 2 || n > 31)
                    return TTF_EINVAL;
                for (i = match->payload_begin + 1; i < p; ++i)
                    if (!pattern_character(text[i], i == match->payload_begin + 1))
                        return TTF_EINVAL;
            }
            match->payload_end = p;
            match->end = p + 1;
            match->complete = 1;
            return TTF_OK;
        }
        if (text[p] == '{')
            return TTF_EINVAL;
        if (text[p] == '\\') {
            if (p + 1 == length)
                return incomplete(engine, length, final, '}', match);
            if (!escaped(text[p + 1]))
                return TTF_EINVAL;
            p += 2;
        } else
            ++p;
    }
    return incomplete(engine, length, final, '}', match);
}
ttf_status_t ttf_control_parse(const ttf_engine_t *engine, const char *text, size_t length,
                               size_t begin, int final, ttf_control_match_t *match) {
    size_t p, base_end;
    if (!engine || !text || !match || begin >= length)
        return TTF_EINVAL;
    memset(match, 0, sizeof(*match));
    match->begin = begin;
    if (text[begin] == '\\') {
        match->kind = TTF_CONTROL_ESCAPE;
        match->base_begin = match->payload_begin = begin;
        if (begin + 1 == length)
            return incomplete(engine, length, final, '\\', match);
        if (!escaped(text[begin + 1]))
            return TTF_EINVAL;
        if (engine->frontend_options.max_control_bytes < 2)
            return TTF_ELIMIT;
        match->end = match->base_end = match->payload_end = begin + 2;
        match->complete = 1;
        return TTF_OK;
    }
    if (text[begin] == '{' || text[begin] == '}')
        return TTF_EINVAL;
    if (text[begin] == '[' && begin + 1 < length && text[begin + 1] == '[') {
        int nonspace = 0;
        match->kind = TTF_CONTROL_LITERAL;
        match->grouped = 1;
        match->base_begin = p = begin + 2;
        while (p < length) {
            if (p - begin >= engine->frontend_options.max_control_bytes)
                return TTF_ELIMIT;
            if (text[p] == ']' && p + 1 < length && text[p + 1] == ']')
                break;
            if (text[p] == '\\') {
                if (p + 1 == length)
                    return incomplete(engine, length, final, ']', match);
                if (!escaped(text[p + 1]))
                    return TTF_EINVAL;
                nonspace = 1;
                p += 2;
                continue;
            }
            if (text[p] == '[' || text[p] == '{' || text[p] == '}')
                return TTF_EINVAL;
            if (text[p] == ']') {
                if (p + 1 == length)
                    return incomplete(engine, length, final, ']', match);
                return TTF_EINVAL;
            }
            if (text[p] != ' ' && text[p] != '\t' && text[p] != '\r' && text[p] != '\n')
                nonspace = 1;
            ++p;
        }
        if (p == length)
            return incomplete(engine, length, final, ']', match);
        if (!nonspace)
            return TTF_EINVAL;
        match->base_end = p;
        p += 2;
        if (p == length)
            return incomplete(engine, length, final, '{', match);
        if (text[p] != '{')
            return TTF_EINVAL;
        return payload(engine, text, length, p, final, match);
    }
    if (atom_continuation(text, length, begin))
        return TTF_OK;
    base_end = atom_end(text, length, begin);
    if (base_end == begin || base_end == length || text[base_end] != '{')
        return TTF_OK;
    match->base_begin = begin;
    match->base_end = base_end;
    if (base_end - begin >= engine->frontend_options.max_control_bytes)
        return TTF_ELIMIT;
    return payload(engine, text, length, base_end, final, match);
}
ttf_status_t ttf_control_decode(const char *text, size_t begin, size_t end, ttf_buffer_t *output) {
    size_t p = begin, start = begin;
    ttf_status_t status;
    if (!text || !output || begin > end)
        return TTF_EINVAL;
    while (p < end) {
        if (text[p] != '\\') {
            ++p;
            continue;
        }
        status = ttf_buffer_append(output, text + start, p - start);
        if (status != TTF_OK)
            return status;
        if (p + 1 == end || !escaped(text[p + 1]))
            return TTF_EINVAL;
        status = ttf_buffer_append(output, text + p + 1, 1);
        if (status != TTF_OK)
            return status;
        start = p += 2;
    }
    return ttf_buffer_append(output, text + start, end - start);
}
