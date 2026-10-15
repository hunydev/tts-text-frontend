#include "../core/internal.h"
/* Returns 1 complete, 0 incomplete, -1 invalid. Reject overlong encodings,
   surrogates, NUL and code points outside Unicode. Locale independent. */
int ttf_utf8_decode(const char *s, size_t n, uint32_t *cp, size_t *w) {
    uint32_t c, min;
    size_t k, i;
    if (!n)
        return 0;
    c = (unsigned char)s[0];
    if (c < 0x80) {
        if (!c)
            return -1;
        *cp = c;
        *w = 1;
        return 1;
    }
    if (c >= 0xc2 && c <= 0xdf) {
        k = 2;
        c &= 31;
        min = 0x80;
    } else if (c >= 0xe0 && c <= 0xef) {
        k = 3;
        c &= 15;
        min = 0x800;
    } else if (c >= 0xf0 && c <= 0xf4) {
        k = 4;
        c &= 7;
        min = 0x10000;
    } else
        return -1;
    for (i = 1; i < k && i < n; i++) {
        if (((unsigned char)s[i] & 0xc0) != 0x80)
            return -1;
    }
    if (n < k)
        return 0;
    for (i = 1; i < k; i++)
        c = (c << 6) | ((unsigned char)s[i] & 63);
    if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
        return -1;
    *cp = c;
    *w = k;
    return 1;
}
ttf_script_t ttf_script_of(uint32_t c) {
    if ((c >= 0xac00 && c <= 0xd7a3) || (c >= 0x1100 && c <= 0x11ff) ||
        (c >= 0x3130 && c <= 0x318f) || (c >= 0xa960 && c <= 0xa97f) ||
        (c >= 0xd7b0 && c <= 0xd7ff))
        return TTF_SCRIPT_HANGUL;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= 0xc0 && c <= 0x2af && c != 0xd7 && c != 0xf7) ||
        /* Vietnamese ệ ở ữ (Latin Extended Additional) and decomposed tone marks */
        (c >= 0x1e00 && c <= 0x1eff) || (c >= 0x300 && c <= 0x36f))
        return TTF_SCRIPT_LATIN;
    /* Thai digits ๐-๙ are digits too (2026-10-14): ๒๕๖๙ is a number, not a Thai word */
    if ((c >= '0' && c <= '9') || (c >= 0xff10 && c <= 0xff19) || (c >= 0xe50 && c <= 0xe59))
        return TTF_SCRIPT_DIGIT;
    if (c == ' ' || (c >= 9 && c <= 13) || c == 0xa0 || c == 0x202f || c == 0x3000 ||
        (c >= 0x2000 && c <= 0x200a))
        return TTF_SCRIPT_WHITESPACE;
    if (c == '.' || c == ',' || c == '!' || c == '?' || c == ':' || c == ';' || c == '-' ||
        c == '\'' || c == '"' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' ||
        c == '}' || c == '/' || c == '\\' || c == '_' || (c >= 0x2010 && c <= 0x2027) ||
        c == 0x3001 || c == 0x3002 || (c >= 0x300c && c <= 0x300f) || c == 0xab || c == 0xbb ||
        c == 0xff01 || c == 0xff1f)
        return TTF_SCRIPT_PUNCTUATION;
    if (c == '$' || c == '%' || c == '#' || c == '@' || c == '&' || c == '+' || c == '=' ||
        c == '*' || c == '<' || c == '>' || c == '~' || c == '^' || c == '|' || c == '`' ||
        c == 0x301c || c == 0xff5e ||
        (c >= 0x20a0 && c <= 0x20cf) || c == 0xa3 || c == 0xa5 || c == 0xb0 ||
        /* ￥980, ￡, ￦: the fullwidth signs are symbols, not part of the kana before them
           (owner v2, 2026-10-15) */
        (c >= 0xffe0 && c <= 0xffe6) ||
        (c >= 0x2190 && c <= 0x2bff) || (c >= 0x1f000 && c <= 0x1faff))
        return TTF_SCRIPT_SYMBOL;
    if ((c >= 0x3400 && c <= 0x4dbf) || (c >= 0x4e00 && c <= 0x9fff) ||
        (c >= 0xf900 && c <= 0xfaff) || (c >= 0x2f800 && c <= 0x2fa1f) ||
        (c >= 0x2e80 && c <= 0x2eff) || (c >= 0x2f00 && c <= 0x2fdf) ||
        (c >= 0x20000 && c <= 0x2a6df) || (c >= 0x2a700 && c <= 0x2b73f) ||
        (c >= 0x2b740 && c <= 0x2b81f) || (c >= 0x2b820 && c <= 0x2ceaf) ||
        (c >= 0x2ceb0 && c <= 0x2ebef) || (c >= 0x30000 && c <= 0x3134f) ||
        (c >= 0x31350 && c <= 0x323af))
        return TTF_SCRIPT_HAN;
    if ((c >= 0x3040 && c <= 0x309f) || (c >= 0x30a0 && c <= 0x30ff) ||
        (c >= 0x31f0 && c <= 0x31ff) || (c >= 0xff66 && c <= 0xff9d) ||
        (c >= 0x1b000 && c <= 0x1b16f))
        return TTF_SCRIPT_KANA;
    return TTF_SCRIPT_OTHER;
}
ttf_status_t ttf_tokenize(const char *s, size_t n, ttf_token_t **out, size_t *count) {
    ttf_token_t *a = NULL;
    size_t len = 0, cap = 0, i = 0, w;
    uint32_t cp;
    if (!out || !count || (!s && n))
        return TTF_EINVAL;
    *out = NULL;
    *count = 0;
    while (i < n) {
        ttf_script_t sc;
        ttf_token_t *p;
        if (ttf_utf8_decode(s + i, n - i, &cp, &w) != 1) {
            free(a);
            return TTF_EUTF8;
        }
        sc = ttf_script_of(cp);
        if (len && ttf_script_legacy(a[len - 1].script) == ttf_script_legacy(sc) &&
            sc != TTF_SCRIPT_PUNCTUATION &&
            sc != TTF_SCRIPT_SYMBOL)
            a[len - 1].end = i + w;
        else {
            if (len == cap) {
                size_t next = cap ? cap * 2 : 32;
                if (next < cap || next > SIZE_MAX / sizeof(*p)) {
                    free(a);
                    return TTF_ELIMIT;
                }
                p = (ttf_token_t *)realloc(a, next * sizeof(*p));
                if (!p) {
                    free(a);
                    return TTF_ENOMEM;
                }
                a = p;
                cap = next;
            }
            a[len].begin = i;
            a[len].end = i + w;
            a[len].script = sc;
            len++;
        }
        i += w;
    }
    *out = a;
    *count = len;
    return TTF_OK;
}
void ttf_tokens_free(ttf_token_t *t) {
    free(t);
}
