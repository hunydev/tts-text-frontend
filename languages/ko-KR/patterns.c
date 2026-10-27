/* Building floor shorthand needs an explicit local building cue.
 * Temperature marks and isolated identifiers keep their own interpretation. */
#include "patterns.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cue(const ttf_pack_input_t *in, const char *kind, size_t index,
               ttf_resource_layer_t *layer) {
    const ttf_token_t *token = &in->tokens[index];
    char key[96];
    const char *value;
    size_t width = token->end - token->begin;
    if (!width || width >= sizeof(key))
        return 0;
    memcpy(key, in->text + token->begin, width);
    key[width] = 0;
    value = ttf_resource_get(in->engine, kind, "ko-KR", key, layer);
    if (value)
        return !strcmp(value, "1");
    if (!strcmp(kind, "floor_cue_before")) {
        /* A named building can retain its productive noun ending: the longest
           declared suffix wins, including an explicit disabled suffix. */
        for (size_t offset = 0; offset < width; ++offset) {
            if (((unsigned char)key[offset] & 0xc0u) == 0x80u)
                continue;
            value = ttf_resource_get(in->engine, "floor_cue_before_suffix", "ko-KR", key + offset,
                                     layer);
            if (value)
                return !strcmp(value, "1");
        }
    }
    return 0;
}
static int horizontal(const ttf_pack_input_t *in, size_t index) {
    const ttf_token_t *token = &in->tokens[index];
    size_t p;
    if (token->script != TTF_SCRIPT_WHITESPACE)
        return 0;
    for (p = token->begin; p < token->end; ++p)
        if (in->text[p] != ' ' && in->text[p] != '\t')
            return 0;
    return 1;
}
size_t ttf_ko_classify_floor(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    size_t start = in->tokens[in->index].begin, end = start, previous = in->index;
    size_t next, consumed;
    char alias[2];
    const char *unit;
    ttf_resource_layer_t layer = TTF_LAYER_COMMON, cue_layer = TTF_LAYER_COMMON;
    int matched = 0;
    while (end < in->length && in->text[end] >= '0' && in->text[end] <= '9')
        ++end;
    if (end == start || end - start >= sizeof(out->value) || end == in->length)
        return 0;
    alias[0] = in->text[end];
    alias[1] = 0;
    unit = ttf_resource_get(in->engine, "floor_alias", "ko-KR", alias, &layer);
    if (!unit)
        return 0;
    ++end;
    /* The alias must finish a Latin token, not prefix F2/Foo. */
    for (next = in->index; next < in->token_count && in->tokens[next].begin < end; ++next)
        if (in->tokens[next].end > end)
            return 0;
    if (next < in->token_count && in->tokens[next].begin == end &&
        (in->tokens[next].script == TTF_SCRIPT_LATIN ||
         in->tokens[next].script == TTF_SCRIPT_DIGIT))
        return 0;
    consumed = next - in->index;
    while (previous && horizontal(in, previous - 1))
        --previous;
    if (previous)
        matched = cue(in, "floor_cue_before", previous - 1, &cue_layer);
    while (next < in->token_count && horizontal(in, next))
        ++next;
    if (!matched && next < in->token_count)
        matched = cue(in, "floor_cue_after", next, &cue_layer);
    if (!matched)
        return 0;
    memcpy(out->value, in->text + start, end - start - 1);
    out->value[end - start - 1] = 0;
    snprintf(out->unit, sizeof(out->unit), "%s", unit);
    snprintf(out->rule, sizeof(out->rule), "ko:building-floor");
    out->type = TTF_SEM_MEASURE;
    out->input_end = end;
    out->layer = layer > cue_layer ? layer : cue_layer;
    return consumed;
}

/* B1층, B1F and B1~B3층: a declared basement prefix, digits and an explicit
   floor unit (a floor alias or the unit word). Bare B1 stays an identifier. */
static size_t basement_endpoint(const ttf_pack_input_t *in, size_t p, char *value, size_t capacity,
                                int *unit) {
    char prefix[2] = {0, 0}, alias[2] = {0, 0};
    const char *word = ttf_resource_get(in->engine, "unit", "ko-KR", "floor", NULL);
    size_t start;
    *unit = 0;
    if (p >= in->length)
        return 0;
    prefix[0] = in->text[p];
    if (!ttf_resource_get(in->engine, "floor_basement_prefix", "ko-KR", prefix, NULL))
        return 0;
    start = ++p;
    while (p < in->length && in->text[p] >= '0' && in->text[p] <= '9')
        ++p;
    if (p == start || p - start >= capacity || in->text[start] == '0')
        return 0;
    memcpy(value, in->text + start, p - start);
    value[p - start] = 0;
    if (p < in->length) {
        alias[0] = in->text[p];
        if (ttf_resource_get(in->engine, "floor_alias", "ko-KR", alias, NULL) &&
            !(p + 1 < in->length && ((in->text[p + 1] >= 'A' && in->text[p + 1] <= 'Z') ||
                                     (in->text[p + 1] >= 'a' && in->text[p + 1] <= 'z') ||
                                     (in->text[p + 1] >= '0' && in->text[p + 1] <= '9')))) {
            *unit = 1;
            return p + 1;
        }
        if (word && !strncmp(in->text + p, word, strlen(word))) {
            *unit = 1;
            return p + strlen(word);
        }
    }
    if (p < in->length &&
        ((in->text[p] >= 'A' && in->text[p] <= 'Z') || (in->text[p] >= 'a' && in->text[p] <= 'z')))
        return 0;
    return p;
}

static int hangul_syllable_at(const ttf_pack_input_t *in, size_t p) {
    const unsigned char *u = (const unsigned char *)in->text + p;
    unsigned cp;
    if (p + 3 > in->length || u[0] < 0xea || u[0] > 0xed)
        return 0;
    cp = ((unsigned)(u[0] & 0x0f) << 12) | ((unsigned)(u[1] & 0x3f) << 6) | (u[2] & 0x3f);
    return cp >= 0xac00 && cp <= 0xd7a3;
}

/* B3F로 지하 삼 층으로: the writer chose the particle for 에프, and the reading ends
   in 층, so 로 를 가 는 와 take their consonant forms (2026-10-27; was 층로). Only a
   particle that ends the word (로 may go on as 로부터 로서 로는 로도 로만 로의), so
   B1F로비 is not touched. Returns the end of the particle, or 0. */
static size_t floor_particle(const ttf_pack_input_t *in, size_t p, ttf_semantic_t *out) {
    static const char *const pairs[][2] = {{"로", "으로"}, {"를", "을"}, {"가", "이"},
                                           {"는", "은"}, {"와", "과"}};
    static const char *const after_ro[] = {"부터", "서", "는", "도", "만", "의"};
    size_t k, j, q;
    for (k = 0; k < sizeof(pairs) / sizeof(pairs[0]); ++k) {
        if (p + 3 > in->length || memcmp(in->text + p, pairs[k][0], 3))
            continue;
        q = p + 3;
        for (j = 0; k == 0 && j < sizeof(after_ro) / sizeof(after_ro[0]); ++j)
            if (q + strlen(after_ro[j]) <= in->length &&
                !memcmp(in->text + q, after_ro[j], strlen(after_ro[j])) &&
                !hangul_syllable_at(in, q + strlen(after_ro[j])))
                q += strlen(after_ro[j]);
        if (hangul_syllable_at(in, q))
            return 0;
        (void)snprintf(out->secondary, sizeof(out->secondary), "%s", pairs[k][1]);
        return p + 3;
    }
    return 0;
}

size_t ttf_ko_classify_basement(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    size_t start = in->tokens[in->index].begin, end, second_end = 0, next;
    char first[32], second[32];
    int first_unit, second_unit = 0;
    if (in->tokens[in->index].end != start + 1 ||
        (start && ((in->text[start - 1] >= 'A' && in->text[start - 1] <= 'Z') ||
                   (in->text[start - 1] >= 'a' && in->text[start - 1] <= 'z') ||
                   (in->text[start - 1] >= '0' && in->text[start - 1] <= '9'))))
        return 0;
    end = basement_endpoint(in, start, first, sizeof(first), &first_unit);
    if (!end)
        return 0;
    if (end < in->length) {
        size_t width = 0;
        if (in->text[end] == '~' || in->text[end] == '-')
            width = 1;
        else if (in->length - end >= 3 && !memcmp(in->text + end, "\xef\xbd\x9e", 3))
            width = 3;
        if (width)
            second_end = basement_endpoint(in, end + width, second, sizeof(second), &second_unit);
    }
    if (second_end && second_unit) {
        snprintf(out->value, sizeof(out->value), "%s", first);
        snprintf(out->secondary, sizeof(out->secondary), "%s", second);
        snprintf(out->unit, sizeof(out->unit), "basement_floor");
        snprintf(out->rule, sizeof(out->rule), "ko:basement-floor-range");
        out->type = TTF_SEM_RANGE;
        end = second_end;
    } else if (first_unit) {
        snprintf(out->value, sizeof(out->value), "%s", first);
        snprintf(out->unit, sizeof(out->unit), "basement_floor");
        snprintf(out->rule, sizeof(out->rule), "ko:basement-floor");
        out->type = TTF_SEM_MEASURE;
    } else
        return 0;
    {
        /* 지하 B2층 지하 이 층: 지하 already written, B2층 reads as a floor (2026-10-27) */
        size_t b = start;
        while (b && (in->text[b - 1] == ' ' || in->text[b - 1] == '\t'))
            --b;
        if (b >= 6 && !memcmp(in->text + b - 6, "지하", 6) && !(second_end && second_unit))
            snprintf(out->unit, sizeof(out->unit), "floor");
    }
    out->input_end = end;
    if (!(second_end && second_unit) && ((in->text[end - 1] | 0x20) >= 'a' && (in->text[end - 1] | 0x20) <= 'z')) {
        size_t particle = floor_particle(in, end, out);
        if (particle)
            out->input_end = end = particle;
    }
    /* B1층에 지하 일 층에: the floor word may carry its particle (2026-10-27; was 비원층에) */
    for (next = in->index; next < in->token_count && in->tokens[next].begin < end; ++next)
        if (in->tokens[next].end > end && in->tokens[next].script != TTF_SCRIPT_HANGUL)
            return 0;
    return next - in->index;
}

/* 6·25 전쟁, 5.18민주화운동, 3·1절: an event named by its month and day. A
   declared event date (event_date) with a middle dot reads so anywhere; a
   period or an undeclared date needs an event word right after it
   (event_date_word 1, attached when it must touch the date, known when it only
   confirms a declared date). Otherwise 5.18 stays a decimal and 1·2학년 a list. */
static size_t event_separator(const char *t, size_t n, size_t p) {
    static const char *const dots[] = {"\xc2\xb7", "\xe3\x86\x8d", "\xe2\x80\xa7", "\xe3\x83\xbb"};
    size_t i;
    if (p < n && t[p] == '.')
        return 1;
    for (i = 0; i < sizeof(dots) / sizeof(dots[0]); ++i)
        if (n - p >= strlen(dots[i]) && !memcmp(t + p, dots[i], strlen(dots[i])))
            return strlen(dots[i]);
    return 0;
}

static const char *event_word(const ttf_pack_input_t *in, size_t p, ttf_resource_layer_t *layer) {
    char key[64];
    size_t end = p, width;
    /* The Hangul run after the date (U+AC00..U+D7A3 lead with 0xEA..0xED). */
    while (end + 3 <= in->length && (unsigned char)in->text[end] >= 0xea &&
           (unsigned char)in->text[end] <= 0xed && end - p + 3 < sizeof(key))
        end += 3;
    for (width = end - p; width >= 3; width -= 3) {
        const char *value;
        memcpy(key, in->text + p, width);
        key[width] = 0;
        value = ttf_resource_get(in->engine, "event_date_word", "ko-KR", key, layer);
        if (value)
            return value;
    }
    return NULL;
}

/* 5.18을 맞아 광주를, 4.19는 ... 부정선거: a declared date written with a period
   and only a particle after it is the event when the sentence names one
   (event_date_cue); 평균자책점 4.19를 stays a decimal. */
static int sentence_cue(const ttf_pack_input_t *in) {
    char key[24];
    size_t p, width;
    for (p = 0; p + 6 <= in->length; ++p) {
        if ((unsigned char)in->text[p] < 0xea || (unsigned char)in->text[p] > 0xed)
            continue;
        for (width = 6; width <= 15 && p + width <= in->length; width += 3) {
            const char *value;
            if ((unsigned char)in->text[p + width - 3] < 0xea ||
                (unsigned char)in->text[p + width - 3] > 0xed)
                break;
            memcpy(key, in->text + p, width);
            key[width] = 0;
            value = ttf_resource_get(in->engine, "event_date_cue", "ko-KR", key, NULL);
            if (value && !strcmp(value, "1"))
                return 1;
        }
    }
    return 0;
}

/* The date ends with a particle: one or two syllables starting with 은는이가을를의에로와과도만. */
static int particle_after(const ttf_pack_input_t *in, size_t p) {
    static const char *const first[] = {"은", "는", "이", "가", "을", "를", "의", "에", "로", "와", "과", "도", "만"};
    size_t run = p, i;
    while (run + 3 <= in->length && (unsigned char)in->text[run] >= 0xea &&
           (unsigned char)in->text[run] <= 0xed)
        run += 3;
    if (run == p || run - p > 6)
        return 0;
    for (i = 0; i < sizeof(first) / sizeof(first[0]); ++i)
        if (!memcmp(in->text + p, first[i], 3))
            return 1;
    return 0;
}

size_t ttf_ko_classify_event_date(const ttf_pack_input_t *in, ttf_semantic_t *out) {
    const char *t = in->text, *known, *word;
    size_t n = in->length, b = in->tokens[in->index].begin, m_end, sep, d, d_end, next, consumed;
    unsigned month, day;
    char key[16];
    ttf_resource_layer_t layer = TTF_LAYER_COMMON, word_layer = TTF_LAYER_COMMON;
    int period, spaced;
    if (b >= n || t[b] < '1' || t[b] > '9')
        return 0;
    if (b && ((t[b - 1] >= '0' && t[b - 1] <= '9') || t[b - 1] == '.' || t[b - 1] == ',' ||
              t[b - 1] == '-' || (t[b - 1] >= 'A' && t[b - 1] <= 'Z') ||
              (t[b - 1] >= 'a' && t[b - 1] <= 'z')))
        return 0;
    for (m_end = b; m_end < n && t[m_end] >= '0' && t[m_end] <= '9' && m_end - b < 3; ++m_end) {
    }
    sep = event_separator(t, n, m_end);
    if (m_end - b > 2 || !sep)
        return 0;
    period = t[m_end] == '.';
    d = m_end + sep;
    if (d >= n || t[d] < '1' || t[d] > '9')
        return 0;
    for (d_end = d; d_end < n && t[d_end] >= '0' && t[d_end] <= '9' && d_end - d < 3; ++d_end) {
    }
    if (d_end - d > 2 || (d_end < n && (t[d_end] == '.' || t[d_end] == ',' || t[d_end] == ':' ||
                                        (t[d_end] >= 'A' && t[d_end] <= 'Z') ||
                                        (t[d_end] >= 'a' && t[d_end] <= 'z'))))
        return 0;
    month = (unsigned)atoi(t + b);
    day = (unsigned)atoi(t + d);
    if (month < 1 || month > 12 || day < 1 || day > 31)
        return 0;
    (void)snprintf(key, sizeof(key), "%u\xc2\xb7%u", month, day);
    known = ttf_resource_get(in->engine, "event_date", "ko-KR", key, &layer);
    if (known && !strcmp(known, "0"))
        known = NULL;
    next = d_end;
    spaced = next < n && t[next] == ' ';
    if (spaced)
        ++next;
    word = event_word(in, next, &word_layer);
    if (word && (!strcmp(word, "0") || (!strcmp(word, "attached") && spaced) ||
                 (!strcmp(word, "known") && !known)))
        word = NULL;
    if (!word && period && known && !spaced && particle_after(in, d_end) && sentence_cue(in))
        word = "cue";
    if (!word && (period || !known))
        return 0;
    (void)snprintf(out->value, sizeof(out->value), "%s", key);
    (void)snprintf(out->unit, sizeof(out->unit), "event_date");
    (void)snprintf(out->rule, sizeof(out->rule), "ko:event-date");
    out->secondary[0] = 0;
    out->type = TTF_SEM_DIGIT_SEQUENCE;
    out->input_end = d_end;
    out->layer = word && word_layer > layer ? word_layer : layer;
    for (consumed = in->index; consumed < in->token_count && in->tokens[consumed].begin < d_end; ++consumed)
        if (in->tokens[consumed].end > d_end)
            return 0;
    return consumed - in->index;
}
