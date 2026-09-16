#include "internal.h"

char *ttf_strndup(const char *s, size_t n) {
    char *p;
    if (n == SIZE_MAX)
        return NULL;
    p = (char *)malloc(n + 1);
    if (p) {
        memcpy(p, s, n);
        p[n] = '\0';
    }
    return p;
}
char *ttf_strdup(const char *s) {
    return s ? ttf_strndup(s, strlen(s)) : NULL;
}
int ttf_ascii_equal(const char *a, const char *b) {
    while (*a && *b) {
        unsigned char x = (unsigned char)*a++, y = (unsigned char)*b++;
        if (x >= 'A' && x <= 'Z')
            x += 'a' - 'A';
        if (y >= 'A' && y <= 'Z')
            y += 'a' - 'A';
        if (x != y)
            return 0;
    }
    return *a == *b;
}
ttf_status_t ttf_buffer_append(ttf_buffer_t *b, const char *s, size_t n) {
    size_t cap;
    char *p;
    if (!b || (!s && n))
        return TTF_EINVAL;
    if (n > SIZE_MAX - b->length - 1)
        return TTF_ELIMIT;
    if (b->length + n + 1 > b->capacity) {
        cap = b->capacity ? b->capacity : 128;
        while (cap < b->length + n + 1) {
            if (cap > SIZE_MAX / 2) {
                cap = b->length + n + 1;
                break;
            }
            cap *= 2;
        }
        p = (char *)realloc(b->data, cap);
        if (!p)
            return TTF_ENOMEM;
        b->data = p;
        b->capacity = cap;
    }
    if (n)
        memcpy(b->data + b->length, s, n);
    b->length += n;
    b->data[b->length] = '\0';
    return TTF_OK;
}
ttf_status_t ttf_buffer_puts(ttf_buffer_t *b, const char *s) {
    return s ? ttf_buffer_append(b, s, strlen(s)) : TTF_EINVAL;
}
void ttf_buffer_free(ttf_buffer_t *b) {
    if (b) {
        free(b->data);
        memset(b, 0, sizeof(*b));
    }
}
const char *ttf_status_string(ttf_status_t s) {
    static const char *const names[] = {"success",
                                        "invalid argument",
                                        "out of memory",
                                        "invalid UTF-8",
                                        "I/O error",
                                        "invalid resource",
                                        "invalid engine/stream state",
                                        "resource limit exceeded",
                                        "callback cancelled"};
    return (unsigned)s < sizeof(names) / sizeof(names[0]) ? names[s] : "unknown error";
}
const char *ttf_script_name(ttf_script_t s) {
    static const char *const names[] = {"HANGUL",      "LATIN",  "DIGIT", "WHITESPACE",
                                        "PUNCTUATION", "SYMBOL", "OTHER"};
    return (unsigned)s < sizeof(names) / sizeof(names[0]) ? names[s] : "UNKNOWN";
}
const char *ttf_layer_name(ttf_resource_layer_t s) {
    static const char *const names[] = {"common", "system", "domain", "customer", "user"};
    return (unsigned)s < sizeof(names) / sizeof(names[0]) ? names[s] : "unknown";
}
const char *ttf_semantic_type_name(ttf_semantic_type_t s) {
    static const char *const names[] = {
        "TEXT",    "WORD",    "NUMBER",       "CARDINAL",   "ORDINAL", "DIGIT_SEQUENCE",
        "COUNTER", "DATE",    "TIME",         "DATETIME",   "PHONE",   "CURRENCY",
        "PERCENT", "MEASURE", "FRACTION",     "RANGE",      "VERSION", "IP_ADDRESS",
        "URL",     "EMAIL",   "ABBREVIATION", "INITIALISM", "ACRONYM", "ALNUM_TERM",
        "SYMBOL",  "CUSTOM"};
    return (unsigned)s < sizeof(names) / sizeof(names[0]) ? names[s] : "UNKNOWN";
}
