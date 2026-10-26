/* Explicit Korean address reading, not geographic recognition or validation. */
#include "address.h"
#include "speech.h"
#include "../../src/core/internal.h"
#include "../../src/pronunciation/trace.h"

static int digit(char c) { return c >= '0' && c <= '9'; }
static int alpha(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static int space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

ttf_status_t ttf_ko_address(const ttf_engine_t *e, const char *s, size_t n, ttf_buffer_t *out) {
    const char *number = ttf_resource_get(e, "address_policy", "ko-KR", "number", NULL);
    const char *zero = ttf_resource_get(e, "address_policy", "ko-KR", "leading_zero", NULL);
    const char *hyphen = ttf_resource_get(e, "address_policy", "ko-KR", "hyphen", NULL);
    const ttf_language_pack_t *pack = ttf_find_pack(e, "ko-KR");
    size_t p = 0, width;
    int content = 0;
    ttf_status_t st = TTF_OK;
    if (!s || !n)
        return TTF_EINVAL;
    if (!pack || !number || !zero || !hyphen || !*hyphen ||
        (strcmp(number, "cardinal") && strcmp(number, "digits")) ||
        (strcmp(zero, "cardinal") && strcmp(zero, "digits")))
        return TTF_ERESOURCE;
    /* Validate every byte before rendering. Numbers and hyphens are structural;
       place names are carried through, without postal lookup or language guessing. */
    while (p < n) {
        uint32_t cp;
        if (ttf_utf8_decode(s + p, n - p, &cp, &width) != 1)
            return TTF_EUTF8;
        if (!(ttf_script_of(cp) == TTF_SCRIPT_HANGUL ||
              (cp > 0 && cp < 128 && (alpha((char)cp) || digit((char)cp) || space((char)cp) ||
                            strchr("-.,()", (int)cp)))))
            return TTF_EINVAL;
        if (cp == '-') {
            /* 12-3, and a unit letter or syllable before it: B-101호, 가-101호 (2026-10-27) */
            size_t left = p, right = p + 1;
            uint32_t before = 0;
            size_t w;
            while (left && space(s[left - 1])) --left;
            while (right < n && space(s[right])) ++right;
            if (left >= 3 && ttf_utf8_decode(s + left - 3, 3, &before, &w) != 1)
                before = 0;
            if (!left || right == n || !digit(s[right]) ||
                !(digit(s[left - 1]) || alpha(s[left - 1]) ||
                  (left >= 3 && before >= 0xac00 && before <= 0xd7a3)))
                return TTF_EINVAL;
        }
        if (ttf_script_of(cp) == TTF_SCRIPT_HANGUL || (cp < 128 && (alpha((char)cp) || digit((char)cp))))
            content = 1;
        p += width;
    }
    if (!content)
        return TTF_EINVAL;
    p = 0;
    while (p < n && st == TTF_OK) {
        size_t end = p;
        /* 20F 이십 층, B1F 지하 일 층: a floor written with F (2026-10-02); B1층 지하 일 층 too
           (2026-10-27; was 비일층). */
        {
            size_t q = p + ((s[p] == 'B' || s[p] == 'b') && p + 1 < n && digit(s[p + 1])), r = q, after;
            while (r < n && digit(s[r])) ++r;
            after = q > p && r + 3 <= n && !memcmp(s + r, "층", 3) ? r + 3 : r + 1;
            if (r > q && (q == p || s[p] == 'B' || s[p] == 'b') && r < n &&
                (after == r + 3 || ((s[r] == 'F' || s[r] == 'f') && (r + 1 == n || !alpha(s[r + 1])))) &&
                (!p || !alpha(s[p - 1]) || q > p) && r - q < 8) {
                ttf_semantic_t part = {0};
                const char *floor = ttf_resource_get(e, "unit", "ko-KR", "floor", NULL);
                const char *below = ttf_resource_get(e, "quantity_prefix", "ko-KR", "basement_floor", NULL);
                if (!floor)
                    return TTF_ERESOURCE;
                if (q > p)
                    st = ttf_buffer_puts(out, below ? below : "지하 ");
                part.struct_size = sizeof(part);
                part.type = TTF_SEM_CARDINAL;
                memcpy(part.value, s + q, r - q);
                ttf_trace_stage(e, TTF_TRACE_POLICY, p, after);
                if (st == TTF_OK)
                    st = ttf_trace_resource(e, "unit", "ko-KR", "floor", floor, "explicit-address-floor",
                                            SIZE_MAX);
                if (st == TTF_OK)
                    st = pack->verbalize(e, &part, s + q, r - q, out);
                if (st == TTF_OK)
                    st = ttf_buffer_puts(out, " ");
                if (st == TTF_OK)
                    st = ttf_buffer_puts(out, floor);
                p = after;
                continue;
            }
        }
        if (digit(s[p])) {
            ttf_semantic_t part = {0};
            const char *key, *policy;
            while (end < n && digit(s[end])) ++end;
            key = s[p] == '0' && end - p > 1 ? "leading_zero" : "number";
            policy = !strcmp(key, "leading_zero") ? zero : number;
            if (end - p >= sizeof(part.value) || (!strcmp(policy, "cardinal") && end - p > 44))
                return TTF_ELIMIT;
            part.struct_size = sizeof(part);
            part.type = !strcmp(policy, "digits") ? TTF_SEM_DIGIT_SEQUENCE : TTF_SEM_CARDINAL;
            memcpy(part.value, s + p, end - p);
            ttf_trace_stage(e, TTF_TRACE_POLICY, p, end);
            st = ttf_trace_resource(e, "address_policy", "ko-KR", key, part.value,
                                    "explicit-address-number", SIZE_MAX);
            if (st == TTF_OK)
                st = pack->verbalize(e, &part, s + p, end - p, out);
        } else if (alpha(s[p])) {
            while (end < n && alpha(s[end])) ++end;
            st = ttf_ko_speech_spelling(e, s + p, end - p, 0, out);
        } else if (s[p] == '-') {
            end = p + 1;
            ttf_trace_stage(e, TTF_TRACE_POLICY, p, end);
            st = ttf_trace_resource(e, "address_policy", "ko-KR", "hyphen", "-",
                                    "explicit-address-hyphen", SIZE_MAX);
            if (st == TTF_OK && out->length && !space(out->data[out->length - 1]))
                st = ttf_buffer_puts(out, " ");
            if (st == TTF_OK) st = ttf_buffer_puts(out, hyphen);
            if (st == TTF_OK && end < n && !space(s[end])) st = ttf_buffer_puts(out, " ");
        } else {
            uint32_t cp;
            if (ttf_utf8_decode(s + p, n - p, &cp, &width) != 1)
                return TTF_EUTF8;
            end = p + width;
            st = ttf_buffer_append(out, s + p, width);
        }
        p = end;
    }
    return st;
}
