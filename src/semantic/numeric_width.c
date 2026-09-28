#include "numeric_width.h"

static int wide(const char *s, size_t n, size_t at) {
    return n - at >= 3 && (unsigned char)s[at] == 0xef &&
           (unsigned char)s[at + 1] == 0xbc &&
           (unsigned char)s[at + 2] >= 0x90 && (unsigned char)s[at + 2] <= 0x99;
}
void ttf_numeric_width_free(ttf_numeric_width_t *v) {
    free(v->text);
    free(v->raw);
    free(v->scan);
    free(v->tokens);
    free(v->regions);
    memset(v, 0, sizeof(*v));
}
ttf_status_t ttf_numeric_width_create(const char *s, size_t n, const ttf_token_t *tokens,
                                       size_t count, const ttf_protected_region_t *regions,
                                       size_t region_count, ttf_numeric_width_t *v) {
    size_t at = 0, k = 0, region = 0;
    memset(v, 0, sizeof(*v));
    /* ASCII and all other Unicode incur no allocation. Search the rare UTF-8
       lead byte once per unit, never once per classifier/token. */
    while (at < n) {
        const char *hit = memchr(s + at, 0xef, n - at);
        if (!hit)
            return TTF_OK;
        at = (size_t)(hit - s);
        while (region < region_count && regions[region].match.end <= at)
            ++region;
        if (region < region_count && regions[region].begin <= at) {
            at = regions[region].match.end;
            continue;
        }
        if (wide(s, n, at))
            break;
        ++at;
    }
    if (at == n)
        return TTF_OK;
    if (n >= SIZE_MAX / sizeof(size_t) || count > SIZE_MAX / sizeof(*v->tokens) ||
        region_count > SIZE_MAX / sizeof(*v->regions))
        return TTF_ELIMIT;
    v->text = malloc(n + 1);
    v->raw = malloc((n + 1) * sizeof(*v->raw));
    v->scan = malloc((n + 1) * sizeof(*v->scan));
    v->tokens = count ? malloc(count * sizeof(*v->tokens)) : NULL;
    v->regions = region_count ? malloc(region_count * sizeof(*v->regions)) : NULL;
    if (!v->text || !v->raw || !v->scan || (count && !v->tokens) ||
        (region_count && !v->regions)) {
        ttf_numeric_width_free(v);
        return TTF_ENOMEM;
    }
    at = region = 0;
    while (at < n) {
        int protected;
        while (region < region_count && regions[region].match.end <= at)
            ++region;
        protected = region < region_count && regions[region].begin <= at;
        v->raw[k] = at;
        v->scan[at] = k;
        if (!protected && wide(s, n, at)) {
            v->scan[at + 1] = v->scan[at + 2] = k;
            v->text[k++] = (char)('0' + (unsigned char)s[at + 2] - 0x90);
            at += 3;
        } else
            v->text[k++] = s[at++];
    }
    v->raw[k] = n;
    v->scan[n] = k;
    v->text[k] = '\0';
    v->length = k;
    for (size_t i = 0; i < count; ++i) {
        v->tokens[i] = tokens[i];
        v->tokens[i].begin = v->scan[tokens[i].begin];
        v->tokens[i].end = v->scan[tokens[i].end];
    }
    for (size_t i = 0; i < region_count; ++i) {
        ttf_control_match_t *c;
        v->regions[i] = regions[i];
        v->regions[i].begin = v->scan[regions[i].begin];
        v->regions[i].match.end = v->scan[regions[i].match.end];
        c = &v->regions[i].match.control;
        if (c->kind != TTF_CONTROL_NONE) {
            c->begin = v->scan[c->begin];
            c->end = v->scan[c->end];
            c->base_begin = v->scan[c->base_begin];
            c->base_end = v->scan[c->base_end];
            c->payload_begin = v->scan[c->payload_begin];
            c->payload_end = v->scan[c->payload_end];
        }
    }
    return TTF_OK;
}
int ttf_numeric_width_surface(ttf_semantic_type_t type) {
    switch (type) {
    case TTF_SEM_NUMBER: case TTF_SEM_CARDINAL: case TTF_SEM_ORDINAL:
    case TTF_SEM_DIGIT_SEQUENCE: case TTF_SEM_COUNTER: case TTF_SEM_DATE:
    case TTF_SEM_TIME: case TTF_SEM_PHONE: case TTF_SEM_CURRENCY:
    case TTF_SEM_PERCENT: case TTF_SEM_MEASURE: case TTF_SEM_FRACTION:
    case TTF_SEM_RANGE: case TTF_SEM_VERSION: case TTF_SEM_IP_ADDRESS:
    case TTF_SEM_ALNUM_TERM:
        return 1;
    default:
        return 0;
    }
}

/* Unsupported fullwidth punctuation is not silently dropped or interpreted as
   a field boundary (12，500 is one unknown amount, never 12 then 500). Only the
   digit width is supported here; do not expand this into general NFKC. */
static size_t punctuation(const char *s, size_t n, size_t at) {
    uint32_t cp;
    size_t width;
    if (at >= n || (unsigned char)s[at] != 0xef ||
        ttf_utf8_decode(s + at, n - at, &cp, &width) != 1)
        return 0;
    switch (cp) {
    case 0xff0b: case 0xff0d: case 0xff0c: case 0xff0e:
    case 0xff0f: case 0xff1a: case 0xff05: case 0xff5e:
        return width;
    default: return 0;
    }
}
size_t ttf_numeric_width_guard(const ttf_pack_input_t *in, ttf_semantic_t *sem,
                               size_t consumed) {
    size_t start = in->tokens[in->index].begin, end = consumed ? sem->input_end : start;
    size_t width = punctuation(in->text, in->length, end), at, tokens;
    if (!width || (consumed && !ttf_numeric_width_surface(sem->type) && sem->type != TTF_SEM_TEXT))
        return consumed;
    at = end + width;
    /* A standalone fullwidth mark is outside this numeric guard. A trailing
       percent sign is still structure; the other trailing marks need digits. */
    if (!(consumed && (unsigned char)in->text[end + 2] == 0x85) &&
        !(at < in->length && in->text[at] >= '0' && in->text[at] <= '9'))
        return consumed;
    while (at < in->length) {
        char c = in->text[at];
        width = punctuation(in->text, in->length, at);
        if (width) at += width;
        else if ((c >= '0' && c <= '9') || c == '.' || c == ',' || c == ':' ||
                 c == '-' || c == '+' || c == '/' || c == '%' || c == '~') ++at;
        else break;
    }
    memset(sem, 0, sizeof(*sem));
    sem->struct_size = sizeof(*sem);
    sem->type = TTF_SEM_TEXT;
    sem->input_end = at;
    strcpy(sem->rule, "syntax.width-punctuation");
    tokens = in->index;
    while (tokens < in->token_count && in->tokens[tokens].begin < at) ++tokens;
    return tokens - in->index;
}
