#include "../core/internal.h"
#include "../pronunciation/korean_phoneme.h"
#include "../protected/protected.h"
#include "../semantic/numeric_grouping.h"
#include "../semantic/phone.h"
#include "../semantic/numeric_width.h"
static char ascii_fold(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
}
/* 」』）】〕〉》 close a Japanese quotation or bracket. */
static int cjk_closer(uint32_t cp) {
    return cp == 0x300d || cp == 0x300f || cp == 0xff09 || cp == 0x3011 || cp == 0x3015 ||
           cp == 0x3009 || cp == 0x300b || cp == 0x3017 || cp == 0x3019 || cp == ')' ||
           cp == ']' || cp == '"' || cp == '\'' || cp == 0x201d || cp == 0x2019;
}
static int cjk_letter(uint32_t cp) {
    ttf_script_t script = ttf_script_of(cp);
    return script == TTF_SCRIPT_HAN || script == TTF_SCRIPT_KANA;
}
static int cjk_letter_at(const char *s, size_t n) {
    uint32_t cp;
    size_t w;
    return ttf_utf8_decode(s, n, &cp, &w) == 1 && cjk_letter(cp);
}
static int horizontal_space(char c) {
    return c == ' ' || c == '\t';
}
/* Packs declare lexical prefixes which require a following field. Decisions
   use only the available prefix, so an arriving chunk cannot undo a cut. */
static int boundary_resource(const ttf_engine_t *e, const char *kind, const char *key) {
    size_t pack;
    const char *value = ttf_resource_get(e, kind, "*", key, NULL);
    if (value && !strcmp(value, "1"))
        return 1;
    for (pack = 0; pack < e->pack_count; ++pack) {
        value = ttf_resource_get(e, kind, e->packs[pack]->language_tag, key, NULL);
        if (value && !strcmp(value, "1"))
            return 1;
    }
    return 0;
}
static int pending_word(const ttf_engine_t *e, const char *s, const ttf_token_t *tokens,
                        size_t index, size_t end) {
    char key[96];
    size_t begin, j;
    if (end && (s[end - 1] == ',' || s[end - 1] == '.'))
        --end;
    while (index && tokens[index - 1].begin >= end)
        --index;
    if (!index)
        return 0;
    begin = tokens[index - 1].begin;
    if (end <= begin || end - begin >= sizeof(key))
        return 0;
    for (j = 0; j < end - begin; ++j) {
        char c = s[begin + j];
        key[j] = c >= 'A' && c <= 'Z' ? (char)(c + ('a' - 'A')) : c;
    }
    key[j] = 0;
    if (boundary_resource(e, "boundary_word", key))
        return 1;
    /* Product/building names may carry a declared cue as their suffix. Keep
       the whole preceding token until its numeric field can arrive. Exact
       floor policy, including an explicit zero, takes precedence per pack. */
    for (size_t pack = 0; pack < e->pack_count; ++pack) {
        const char *language = e->packs[pack]->language_tag;
        /* A declared year cue keeps its number in the same segment without
           making the word a general boundary for ordinary phrases. */
        const char *value = ttf_resource_get(e, "date_year_cue", language, key, NULL);
        if (value && !strcmp(value, "1"))
            return 1;
        /* An enclosure mark introduces a year the same way, so the planner must
           keep the number with it or batch and stream would disagree. */
        if (ttf_resource_get(e, "date_year_enclosure", language, key, NULL))
            return 1;
        value = ttf_resource_get(e, "floor_cue_before", language, key, NULL);
        if (value) {
            if (!strcmp(value, "1"))
                return 1;
            continue;
        }
        for (size_t suffix = 0; suffix < j; ++suffix) {
            if (((unsigned char)key[suffix] & 0xc0u) == 0x80u)
                continue;
            value = ttf_resource_get(e, "floor_cue_before_suffix", language, key + suffix, NULL);
            if (value) {
                if (!strcmp(value, "1"))
                    return 1;
                break;
            }
        }
    }
    return 0;
}
/* Whitespace token prefixes after digits must remain pending even if several
   grouping separators arrive in separate chunks. Retain the whole whitespace
   token; never decide using a not-yet-arrived following digit. */
static int numeric_space_prefix(const ttf_engine_t *e, const char *s, size_t length,
                                const ttf_token_t *tokens, size_t index) {
    size_t at = tokens[index].begin;
    while (at) {
        size_t previous = at - 1;
        while (previous && ((unsigned char)s[previous] & 0xc0u) == 0x80u)
            --previous;
        if (at == previous + 1 && s[previous] >= '0' && s[previous] <= '9')
            return 1;
        if (ttf_numeric_group_separator(e, s, length, previous, NULL, NULL) != at - previous)
            return 0;
        at = previous;
    }
    return 0;
}
/* A floor alias may obtain its building cue from the following lexical token.
   Retain its whitespace before that token exists; consulting future cue bytes
   here would make a one-byte stream choose a different earlier boundary. */
static int floor_before_space(const ttf_engine_t *e, const char *s, const ttf_token_t *tokens,
                              size_t index) {
    const ttf_token_t *alias, *number;
    char key[2];
    size_t pack;
    if (index < 2)
        return 0;
    alias = &tokens[index - 1];
    number = &tokens[index - 2];
    if (alias->end - alias->begin != 1 || number->script != TTF_SCRIPT_DIGIT ||
        number->end != alias->begin || number->end - number->begin >= TTF_IR_VALUE_SIZE)
        return 0;
    key[0] = s[alias->begin];
    key[1] = 0;
    for (pack = 0; pack < e->pack_count; ++pack)
        if (ttf_resource_get(e, "floor_alias", e->packs[pack]->language_tag, key, NULL))
            return 1;
    return 0;
}
/* Cue decisions depend only on the prefix before a candidate cut. A complete
   input lets us compute protection ranges in advance, but a key mismatch must
   still protect earlier matching prefixes: the same cut must be chosen when
   the mismatch arrives in a later write. */
static int cue_match_prefix(const char *s, size_t begin, size_t end, const char *key, int fold,
                            size_t *after) {
    size_t p = begin, k = 0;
    while (p < end && key[k]) {
        if (horizontal_space(s[p]) && key[k] == ' ') {
            while (p < end && horizontal_space(s[p]))
                ++p;
            while (key[k] == ' ')
                ++k;
        } else if (s[p] == key[k] || (fold && ascii_fold(s[p]) == ascii_fold(key[k]))) {
            ++p;
            ++k;
        } else
            break;
    }
    *after = p;
    return key[k] == '\0';
}
typedef struct {
    size_t begin, end;
    int valid;
} cue_scope_cache_t;
static size_t cue_scope_end(const char *s, size_t begin, size_t end, int kind,
                            cue_scope_cache_t *cache) {
    size_t p = begin;
    /* A dot is only a continuation after a digit inside the same scope. */
    if (begin < end && s[begin] == '.')
        return begin;
    if (cache->valid && begin >= cache->begin && begin <= cache->end)
        return cache->end < end ? cache->end : end;
    while (p < end) {
        unsigned char c = (unsigned char)s[p];
        if (cache->valid && p == cache->begin) {
            p = cache->end < end ? cache->end : end;
            break;
        }
        if (horizontal_space((char)c) || c == ':' || c == '-' || (c >= '0' && c <= '9'))
            ++p;
        else if (p + 3 <= end && !memcmp(s + p, "\xef\xbc\x9a", 3))
            p += 3;
        else if (kind == 2 && (c == '+' || c == '(' || c == ')'))
            ++p;
        else if (kind != 1 && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '\'' ||
                               (c == '.' && p > begin && s[p - 1] >= '0' && s[p - 1] <= '9')))
            ++p;
        else
            break;
    }
    cache->begin = begin;
    cache->end = p;
    cache->valid = 1;
    return p;
}
static size_t title_scope_limit(const char *s, size_t n, size_t maximum) {
    size_t p = 0, count = 0;
    size_t limit = maximum > 256 ? maximum : 256;
    while (p < n && count < limit) {
        uint32_t cp;
        size_t width;
        if (ttf_utf8_decode(s + p, n - p, &cp, &width) != 1)
            break;
        p += width;
        ++count;
    }
    return p;
}
/* Match from each lexical start once. Scope caches reuse the valid suffix when
   labels overlap, instead of scanning back over the entire title at every
   whitespace. This planner state is local to a single scan. */
static void extend_cue_range(const ttf_engine_t *e, const char *s, size_t n, size_t begin,
                             size_t title_limit, size_t *protected_until,
                             cue_scope_cache_t caches[3]) {
    const ttf_entry_t *cue;
    for (cue = e->boundary_cues; cue; cue = cue->boundary_next) {
        size_t after, end;
        int complete, kind = !strcmp(cue->kind, "identifier_label") ? 1
                             : !strcmp(cue->kind, "phone_label")    ? 2
                                                                    : 0;
        size_t limit = kind == 1 ? n : title_limit;
        if (begin >= limit ||
            (s[begin] != cue->key[0] && (!kind || ascii_fold(s[begin]) != ascii_fold(cue->key[0]))))
            continue;
        complete = cue_match_prefix(s, begin, limit, cue->key, kind != 0, &after);
        end = complete ? cue_scope_end(s, after, limit, kind, &caches[kind]) : after;
        if (end > *protected_until)
            *protected_until = end;
    }
}
/* A written currency/rate/measure can be the first endpoint of a spaced
   interval. Keep its following whitespace pending without consulting future
   bytes, so tiny phrase targets cannot make chunk boundaries change the IR.
   Reuse pack classification over a bounded prefix; the core knows no unit or
   language spelling. Counter and clock endpoints need the same lookahead. */
static int quantity_before_space(const ttf_engine_t *e, const char *s, const ttf_token_t *tokens,
                                 size_t index, size_t end) {
    size_t j = index, pack;
    while (j) {
        ttf_pack_input_t in;
        ttf_semantic_t best = {0};
        --j;
        if (end - tokens[j].begin > 512)
            break;
        if (tokens[j].script == TTF_SCRIPT_WHITESPACE &&
            memchr(s + tokens[j].begin, '\n', tokens[j].end - tokens[j].begin))
            break;
        if (tokens[j].script != TTF_SCRIPT_DIGIT)
            continue;
        /* 85,5 triệu, 1.500 USD: classify from the start of the number, not
           from its last digit group (2026-10-13). */
        while (j >= 2 && tokens[j - 1].end == tokens[j].begin &&
               tokens[j - 1].end == tokens[j - 1].begin + 1 &&
               (s[tokens[j - 1].begin] == ',' || s[tokens[j - 1].begin] == '.') &&
               tokens[j - 2].script == TTF_SCRIPT_DIGIT && tokens[j - 2].end == tokens[j - 1].begin)
            j -= 2;
        memset(&in, 0, sizeof(in));
        in.text = s;
        in.length = end;
        in.tokens = tokens;
        in.token_count = index;
        in.index = j;
        in.engine = e;
        best.struct_size = sizeof(best);
        (void)ttf_classify_universal(&in, &best);
        for (pack = 0; pack < e->pack_count; ++pack) {
            ttf_semantic_t candidate = {0};
            candidate.struct_size = sizeof(candidate);
            if (e->packs[pack]->classify && e->packs[pack]->classify(&in, &candidate) &&
                candidate.input_end > best.input_end && candidate.input_end <= end)
                best = candidate;
        }
        if (best.input_end < end && end - best.input_end < 96) {
            char tail[96];
            memcpy(tail, s + best.input_end, end - best.input_end);
            tail[end - best.input_end] = 0;
            if (boundary_resource(e, "quantity_lookahead_tail", tail))
                best.input_end = end;
        }
        return best.input_end == end &&
               (best.type == TTF_SEM_CURRENCY || best.type == TTF_SEM_PERCENT ||
                best.type == TTF_SEM_COUNTER || best.type == TTF_SEM_ORDINAL ||
                best.type == TTF_SEM_TIME || best.type == TTF_SEM_MEASURE ||
                (best.type == TTF_SEM_RANGE && best.unit[0]));
    }
    return 0;
}
static int protected_period(const ttf_engine_t *e, const char *s, size_t at) {
    size_t begin = at, n;
    char key[96];
    if (at && s[at - 1] >= '0' && s[at - 1] <= '9')
        return 1;
    /* ม.ค. น. บ.: Thai writes no sentence period, so a period after a Thai
       letter (U+0E01-U+0E4E) closes an abbreviation (2026-10-14). */
    if (at >= 3 && (unsigned char)s[at - 3] == 0xe0 &&
        (((unsigned char)s[at - 2] == 0xb8 && (unsigned char)s[at - 1] >= 0x81) ||
         ((unsigned char)s[at - 2] == 0xb9 && (unsigned char)s[at - 1] <= 0x8e)))
        return 1;
    while (begin && ((s[begin - 1] >= 'a' && s[begin - 1] <= 'z') ||
                     (s[begin - 1] >= 'A' && s[begin - 1] <= 'Z') || s[begin - 1] == '.'))
        begin--;
    n = at - begin;
    if (!n || n >= sizeof(key))
        return 0;
    /* A single capital followed by a period can still be the first part of
       initials. Retain this prefix even when the next chunk is not present.
       Packs opt into the syntax; the core owns no language abbreviation list. */
    if ((n & 1u) && boundary_resource(e, "boundary_syntax", "dotted_initials")) {
        size_t j;
        for (j = 0; j < n; ++j)
            if ((j & 1u) ? s[begin + j] != '.' : (s[begin + j] < 'A' || s[begin + j] > 'Z'))
                break;
        if (j == n)
            return 1;
    }
    memcpy(key, s + begin, n);
    key[n] = 0;
    for (begin = 0; begin < n; begin++)
        if (key[begin] >= 'A' && key[begin] <= 'Z')
            key[begin] += 'a' - 'A';
    if (boundary_resource(e, "boundary_period", key))
        return 1;
    if (ttf_resource_get(e, "abbreviation", "*", key, NULL))
        return 1;
    for (begin = 0; begin < e->pack_count; begin++)
        if (ttf_resource_get(e, "abbreviation", e->packs[begin]->language_tag, key, NULL))
            return 1;
    return 0;
}
static int quoted_path_prefix(const char *s, size_t n, size_t begin) {
    size_t p = begin + 1;
    unsigned char c;
    if (p == n)
        return 0;
    c = (unsigned char)s[p++];
    /* An absolute '/' still needs a byte proving it is not protocol-relative
       '//'. Other prefixes are decisive once their final slash is present. */
    if (c == '/')
        return p < n && s[p] != '/';
    if (c == '.') {
        if (p < n && s[p] == '.')
            ++p;
        return p < n && s[p] == '/';
    }
    if (c == '~')
        return p < n && s[p] == '/';
    if (c == '\\')
        return p < n && s[p] == '\\';
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        return n - p >= 2 && s[p] == ':' && (s[p + 1] == '/' || s[p + 1] == '\\');
    return 0;
}
static void incomplete_literal_hint(const char *s, size_t n, const ttf_protected_region_t *region,
                                    ttf_boundary_wait_t *wait) {
    size_t p = region->begin;
    char quote = 0;
    wait->begin = p;
    if (!strcmp(region->match.kind, "path")) {
        if (s[p] == '\'' || s[p] == '"') {
            if (quoted_path_prefix(s, n, p)) {
                wait->kind = TTF_WAIT_QUOTED;
                wait->quote = s[p];
            }
        } else
            wait->kind = TTF_WAIT_PATH;
    } else if (s[p] == '`')
        wait->kind = TTF_WAIT_BACKTICKS;
    else if (n - p >= 4 && !memcmp(s + p, "<!--", 4))
        wait->kind = TTF_WAIT_COMMENT;
    else if (s[p] == '<') {
        /* The recognizer has already checked tag syntax. Determine only if its
           first opening tag ended, or a quoted attribute is still open. Never
           cache an undecided tag name or an unquoted attribute value. */
        for (++p; p < n; ++p) {
            if (quote) {
                if (s[p] == quote)
                    quote = 0;
            } else if (s[p] == '\'' || s[p] == '"')
                quote = s[p];
            else if (s[p] == '>') {
                if (!strcmp(region->match.kind, "code"))
                    wait->kind = TTF_WAIT_ELEMENT;
                return;
            }
        }
        if (quote) {
            wait->kind = TTF_WAIT_QUOTED;
            wait->quote = quote;
        }
    }
}
ttf_status_t ttf_next_boundary(const ttf_engine_t *e, const char *s, size_t n, int final, int first,
                               size_t *boundary, ttf_boundary_wait_t *wait) {
    ttf_token_t *tokens = NULL;
    ttf_numeric_width_t width_view = {0};
    ttf_protected_region_t *regions = NULL;
    size_t region_count = 0, region_index = 0;
    size_t count = 0, i, at, chars = 0,
           target = first ? e->config.first_segment_target : e->config.normal_segment_target;
    size_t selected = 0, protected_until = 0, title_limit;
    cue_scope_cache_t cue_caches[3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    uint32_t quote_stack[32];
    size_t quote_depth = 0;
    int tex_open = 0;
    ttf_status_t status =
        ttf_tokenize_protected(e, s, n, final, &tokens, &count, &regions, &region_count);
    *boundary = 0;
    memset(wait, 0, sizeof(*wait));
    if (status != TTF_OK)
        return status;
    status = ttf_numeric_width_create(s, n, tokens, count, regions, region_count,
                                      ttf_numeric_width_mode(e->default_language), &width_view);
    if (status != TTF_OK) {
        free(tokens);
        free(regions);
        return status;
    }
    if (width_view.text) {
        s = width_view.text;
        n = width_view.length;
        free(tokens);
        free(regions);
        tokens = width_view.tokens;
        regions = width_view.regions;
        width_view.tokens = NULL;
        width_view.regions = NULL;
    }
    /* A long title eventually yields a phrase. The cap is measured from the
       current unit start, so input chunk boundaries cannot change it. */
    title_limit = title_scope_limit(s, n, e->config.max_segment_length);
    for (i = 0; i < count; i++) {
        const ttf_token_t *t = &tokens[i];
        if (region_index < region_count && regions[region_index].begin == t->begin) {
            if (!regions[region_index].match.complete) {
                if (regions[region_index].match.control.kind != TTF_CONTROL_NONE) {
                    wait->kind = TTF_WAIT_QUOTED;
                    wait->begin = t->begin;
                    wait->control_limit = e->frontend_options.max_control_bytes;
                    wait->quote = regions[region_index].match.control.wait_quote;
                    /* Recheck escapes on each byte. */
                    if (regions[region_index].match.control.kind == TTF_CONTROL_ESCAPE)
                        wait->kind = TTF_WAIT_NONE;
                } else
                    incomplete_literal_hint(s, n, &regions[region_index], wait);
                goto done;
            }
            ++region_index;
            /* Internal punctuation, whitespace and quotes belong to the literal.
               Count codepoints for the next safe cut, without interpreting them. */
            for (at = t->begin; at < t->end;) {
                uint32_t cp;
                size_t width;
                if (ttf_utf8_decode(s + at, t->end - at, &cp, &width) != 1)
                    goto done;
                ++chars;
                at += width;
            }
            continue;
        }
        extend_cue_range(e, s, n, t->begin, title_limit, &protected_until, cue_caches);
        if (t->script == TTF_SCRIPT_WHITESPACE && numeric_space_prefix(e, s, n, tokens, i) &&
            t->end > protected_until)
            protected_until = t->end;
        {
            size_t phrase_end = ttf_phrase_protect_until(e, s, n, tokens, i);
            if (phrase_end > protected_until)
                protected_until = phrase_end;
        }
        /* The same lexical categories drive routing, semantic recognition and
           safe cuts. Only whitespace cuts can be used for phrase segmentation. */
        for (at = t->begin; at < t->end;) {
            uint32_t cp, next = 0;
            size_t w, nw = 0;
            int sentence = 0;
            if (ttf_utf8_decode(s + at, n - at, &cp, &w) != 1)
                goto done;
            chars++;
            /* A TeX quote (`` ... `` or `` ... '') is a quotation too: a cut
               inside it leaves its closing marks to open code in the next
               segment (2026-10-07). */
            if (cp == '`' && at + 1 < n && s[at + 1] == '`' && (!at || s[at - 1] != '`'))
                tex_open = !tex_open;
            else if (cp == '\'' && tex_open && at + 1 < n && s[at + 1] == '\'' &&
                     (!at || s[at - 1] != '\''))
                tex_open = 0;
            if (quote_depth && cp == quote_stack[quote_depth - 1])
                quote_depth--;
            else if (ttf_quote_closer(cp) && quote_depth < 32)
                quote_stack[quote_depth++] = ttf_quote_closer(cp);
            if (e->config.sentence_split && !quote_depth && !tex_open) {
                if (cp == '\n')
                    sentence = 1;
                else if (cp == '.' || cp == '!' || cp == '?' || cp == 0x3002 || cp == 0xff01 ||
                         cp == 0xff1f) {
                    int has_next =
                        at + w < n && ttf_utf8_decode(s + at + w, n - at - w, &next, &nw) == 1;
                    if ((has_next && ttf_script_of(next) == TTF_SCRIPT_WHITESPACE) ||
                        (!has_next && final))
                        sentence = 1;
                    /* 晴れです。明日は: Japanese writes no space after its full stop;
                       a closing mark or another stop after it continues the sentence
                       (2026-10-08). */
                    if (has_next && cp >= 0x3000 && !cjk_closer(next) &&
                        next != 0x3002 && next != 0xff01 && next != 0xff1f && next != '!' &&
                        next != '?' && ttf_script_of(next) != TTF_SCRIPT_WHITESPACE)
                        sentence = 1;
                    if (cp == '.' && protected_period(e, s, at))
                        sentence = 0;
                    /* 5! = 120, 3! + 1: a factorial before an operator (2026-10-14) */
                    if (cp == '!' && sentence && at && s[at - 1] >= '0' && s[at - 1] <= '9') {
                        size_t z = at + w;
                        while (z < n && s[z] == ' ')
                            ++z;
                        /* streaming: wait for the next mark before cutting after 5! */
                        if (z < n ? strchr("=+-*/<>", s[z]) != NULL : !final)
                            sentence = 0;
                    }
                    if (cp == '.' && sentence &&
                        ttf_phone_boundary_pending(e, s, at + w, tokens, i + 1))
                        sentence = 0;
                }
            }
            if (sentence) {
                selected = at + w;
                goto done;
            }
            /* 、 is where Japanese text without spaces takes a phrase cut; at the
               length limit a cut falls between two kana or kanji (2026-10-08). */
            if (!tex_open && at + w > protected_until &&
                ((!quote_depth && (cp == 0x3001 || cp == 0xff0c) &&
                  /* １２，５００: a comma between digits groups a number; after a
                     digit an unseen next character waits for more input. */
                  !(at && s[at - 1] >= '0' && s[at - 1] <= '9' &&
                    (at + w >= n ? !final : s[at + w] >= '0' && s[at + w] <= '9')) &&
                  ((e->config.phrase_split && chars >= target) ||
                   chars >= e->config.max_segment_length)) ||
                 (chars >= e->config.max_segment_length && cjk_letter(cp) && at + w < n &&
                  cjk_letter_at(s + at + w, n - at - w)))) {
                selected = at + w;
                goto done;
            }
            if (!quote_depth && !tex_open && t->script == TTF_SCRIPT_WHITESPACE &&
                ((e->config.phrase_split && chars >= target) ||
                 chars >= e->config.max_segment_length)) {
                size_t previous = at;
                if (at < protected_until) {
                    at += w;
                    continue;
                }
                if (ttf_phone_boundary_pending(e, s, at + w, tokens, i + 1)) {
                    at += w;
                    continue;
                }
                /* pronunciation=hangul: 남지 | 않았다 reads across the space, so the
                   cut moves on; an unfinished next word waits for more input. */
                if (e->phoneme_output && i && i + 1 < count && t->end - t->begin == 1 && s[t->begin] == ' ' &&
                    tokens[i - 1].script == TTF_SCRIPT_HANGUL && tokens[i - 1].end == t->begin &&
                    tokens[i + 1].script == TTF_SCRIPT_HANGUL &&
                    ((tokens[i + 1].end >= n && !final) ||
                     ttf_korean_phoneme_joined(e, s, tokens[i - 1].begin, tokens[i - 1].end,
                                               tokens[i + 1].begin, tokens[i + 1].end))) {
                    at += w;
                    continue;
                }
                while (previous && (s[previous - 1] == ' ' || s[previous - 1] == '\t' ||
                                    s[previous - 1] == '\r'))
                    previous--;
                if (pending_word(e, s, tokens, i, previous) ||
                    floor_before_space(e, s, tokens, i) ||
                    (previous >= 2 && s[previous - 1] == ',' && s[previous - 2] >= '0' &&
                     s[previous - 2] <= '9')) {
                    at += w;
                    continue;
                }
                /* A number keeps the word before it in its segment: its reading
                   depends on it (from 1918 to 1923, the # 7 seed), and the next
                   segment cannot see it (2026-10-07). After a word an unseen next
                   token waits for more input, so a stream cuts where a batch does;
                   after a literal, punctuation or a line break the cut stays
                   immediate. */
                if (!i || memchr(s + t->begin, '\n', t->end - t->begin) ||
                    memchr(s + t->begin, '\r', t->end - t->begin) ||
                    (tokens[i - 1].script != TTF_SCRIPT_LATIN &&
                           tokens[i - 1].script != TTF_SCRIPT_HANGUL &&
                           tokens[i - 1].script != TTF_SCRIPT_HAN &&
                           tokens[i - 1].script != TTF_SCRIPT_KANA)) {
                    /* no word before: nothing to keep */
                } else if (i + 1 < count) {
                    size_t next = tokens[i + 1].begin;
                    if (next < n && s[next] == '#' && next + 1 < n && s[next + 1] == ' ')
                        next += 2;
                    else if (next < n && s[next] == '#')
                        ++next;
                    if (next >= n ? !final : (s[next] >= '0' && s[next] <= '9')) {
                        at += w;
                        continue;
                    }
                } else if (!final) {
                    at += w;
                    continue;
                }
                /* A number, sign, currency prefix or fraction separator may
                   bind to the following token even across spaces. Retain it
                   without inspecting unavailable future input. */
                if (previous &&
                    ((s[previous - 1] >= '0' && s[previous - 1] <= '9') || s[previous - 1] == '/' ||
                     s[previous - 1] == ':' || s[previous - 1] == '+' || s[previous - 1] == '-' ||
                     s[previous - 1] == '#')) {
                    at += w;
                    continue;
                }
                /* 112에 신고, 2-1로 앞서: a number with an attached particle keeps
                   the word after it in its segment too (2026-10-07). */
                {
                    size_t k = previous, steps = 0;
                    int numeric = 0;
                    while (k && steps < 32 && s[k - 1] != ' ' && s[k - 1] != '\t' &&
                           s[k - 1] != '\n' && s[k - 1] != '\r') {
                        numeric |= s[k - 1] >= '0' && s[k - 1] <= '9';
                        --k;
                        ++steps;
                    }
                    /* A protected literal (code) is not a number. */
                    if (numeric && i && tokens[i - 1].script != TTF_SCRIPT_SYMBOL) {
                        at += w;
                        continue;
                    }
                }
                if (quantity_before_space(e, s, tokens, i, previous)) {
                    at += w;
                    continue;
                }
                if (i && tokens[i - 1].end - tokens[i - 1].begin < 64) {
                    char key[64];
                    size_t key_length = tokens[i - 1].end - tokens[i - 1].begin;
                    memcpy(key, s + tokens[i - 1].begin, key_length);
                    key[key_length] = 0;
                    const char *range_flag =
                        ttf_resource_get(e, "quantity_range_separator", "*", key, NULL);
                    if (ttf_resource_get(e, "currency_prefix", "*", key, NULL) ||
                        (range_flag && !strcmp(range_flag, "1"))) {
                        at += w;
                        continue;
                    }
                    if (i >= 2) {
                        size_t previous_token = i - 1;
                        int continuation = 0;
                        while (previous_token &&
                               tokens[previous_token - 1].script == TTF_SCRIPT_WHITESPACE)
                            previous_token--;
                        if (previous_token &&
                            tokens[previous_token - 1].script == TTF_SCRIPT_DIGIT) {
                            continuation =
                                boundary_resource(e, "numeric_continuation", key) ||
                                ttf_resource_get(e, "currency_scale", "*", key, NULL) != NULL;
                        }
                        if (continuation) {
                            at += w;
                            continue;
                        }
                    }
                }
                /* A spaced date or abbreviation may continue after this whitespace.
                   Never freeze its leading number merely to hit a phrase target. */
                if (!(previous && s[previous - 1] == '.' && protected_period(e, s, previous - 1))) {
                    selected = at + w;
                    goto done;
                }
            }
            at += w;
        }
    }
    if (final)
        selected = n;
done:
    free(tokens);
    free(regions);
    *boundary = width_view.text ? width_view.raw[selected] : selected;
    if (width_view.text)
        wait->begin = width_view.raw[wait->begin];
    ttf_numeric_width_free(&width_view);
    return TTF_OK;
}
