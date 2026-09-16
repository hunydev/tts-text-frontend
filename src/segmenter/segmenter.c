#include "../core/internal.h"
#include "../protected/protected.h"
static int horizontal_space(char c) {
    return c == ' ' || c == '\t';
}
/* Cue decisions depend only on the prefix before a candidate cut. A complete
   input lets us compute protection ranges in advance, but a key mismatch must
   still protect earlier matching prefixes: the same cut must be chosen when
   the mismatch arrives in a later write. */
static int cue_match_prefix(const char *s, size_t begin, size_t end, const char *key,
                            size_t *after) {
    size_t p = begin, k = 0;
    while (p < end && key[k]) {
        if (horizontal_space(s[p]) && key[k] == ' ') {
            while (p < end && horizontal_space(s[p]))
                ++p;
            while (key[k] == ' ')
                ++k;
        } else if (s[p] == key[k]) {
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
static size_t cue_scope_end(const char *s, size_t begin, size_t end, int identifier,
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
        else if (!identifier && ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                 c == '\'' || (c == '.' && p > begin &&
                                 s[p - 1] >= '0' && s[p - 1] <= '9')))
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
                              cue_scope_cache_t caches[2]) {
    const ttf_entry_t *cue;
    for (cue = e->boundary_cues; cue; cue = cue->boundary_next) {
        size_t after, end;
        int complete, identifier = !strcmp(cue->kind, "identifier_label");
        size_t limit = identifier ? n : title_limit;
        if (begin >= limit || s[begin] != cue->key[0])
            continue;
        complete = cue_match_prefix(s, begin, limit, cue->key, &after);
        end = complete ? cue_scope_end(s, after, limit, identifier, &caches[identifier]) : after;
        if (end > *protected_until)
            *protected_until = end;
    }
}
/* A written currency/rate/measure can be the first endpoint of a spaced
   interval. Keep its following whitespace pending without consulting future
   bytes, so tiny phrase targets cannot make chunk boundaries change the IR.
   Reuse pack classification over a bounded prefix; the core knows no unit or
   language spelling. Counters retain their existing phrase behavior. */
static int quantity_before_space(const ttf_engine_t *e, const char *s,
                                 const ttf_token_t *tokens, size_t index, size_t end) {
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
        return best.input_end == end &&
               (best.type == TTF_SEM_CURRENCY || best.type == TTF_SEM_PERCENT ||
                best.type == TTF_SEM_MEASURE ||
                (best.type == TTF_SEM_RANGE && best.unit[0]));
    }
    return 0;
}
static int protected_period(const ttf_engine_t *e, const char *s, size_t at) {
    size_t begin = at, n;
    char key[96];
    if (at && s[at - 1] >= '0' && s[at - 1] <= '9')
        return 1;
    while (begin && ((s[begin - 1] >= 'a' && s[begin - 1] <= 'z') ||
                     (s[begin - 1] >= 'A' && s[begin - 1] <= 'Z') || s[begin - 1] == '.'))
        begin--;
    n = at - begin;
    if (!n || n >= sizeof(key))
        return 0;
    memcpy(key, s + begin, n);
    key[n] = 0;
    for (begin = 0; begin < n; begin++)
        if (key[begin] >= 'A' && key[begin] <= 'Z')
            key[begin] += 'a' - 'A';
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
static void incomplete_literal_hint(const char *s, size_t n,
                                     const ttf_protected_region_t *region,
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
    ttf_protected_region_t *regions = NULL;
    size_t region_count = 0, region_index = 0;
    size_t count = 0, i, at, chars = 0,
           target = first ? e->config.first_segment_target : e->config.normal_segment_target;
    size_t selected = 0, protected_until = 0, title_limit;
    cue_scope_cache_t cue_caches[2] = {{0, 0, 0}, {0, 0, 0}};
    uint32_t quote_stack[32];
    size_t quote_depth = 0;
    ttf_status_t status =
        ttf_tokenize_protected(e, s, n, final, &tokens, &count, &regions, &region_count);
    *boundary = 0;
    memset(wait, 0, sizeof(*wait));
    if (status != TTF_OK)
        return status;
    /* A long title eventually yields a phrase. The cap is measured from the
       current unit start, so input chunk boundaries cannot change it. */
    title_limit = title_scope_limit(s, n, e->config.max_segment_length);
    for (i = 0; i < count; i++) {
        const ttf_token_t *t = &tokens[i];
        if (region_index < region_count && regions[region_index].begin == t->begin) {
            if (!regions[region_index].match.complete) {
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
        /* The same lexical categories drive routing, semantic recognition and
           safe cuts. Only whitespace cuts can be used for phrase segmentation. */
        for (at = t->begin; at < t->end;) {
            uint32_t cp, next = 0;
            size_t w, nw = 0;
            int sentence = 0;
            if (ttf_utf8_decode(s + at, n - at, &cp, &w) != 1)
                goto done;
            chars++;
            if (quote_depth && cp == quote_stack[quote_depth - 1])
                quote_depth--;
            else if (ttf_quote_closer(cp) && quote_depth < 32)
                quote_stack[quote_depth++] = ttf_quote_closer(cp);
            if (e->config.sentence_split && !quote_depth) {
                if (cp == '\n')
                    sentence = 1;
                else if (cp == '.' || cp == '!' || cp == '?' || cp == 0x3002 || cp == 0xff01 ||
                         cp == 0xff1f) {
                    int has_next =
                        at + w < n && ttf_utf8_decode(s + at + w, n - at - w, &next, &nw) == 1;
                    if ((has_next && ttf_script_of(next) == TTF_SCRIPT_WHITESPACE) ||
                        (!has_next && final))
                        sentence = 1;
                    if (cp == '.' && protected_period(e, s, at))
                        sentence = 0;
                }
            }
            if (sentence) {
                selected = at + w;
                goto done;
            }
            if (!quote_depth && t->script == TTF_SCRIPT_WHITESPACE &&
                ((e->config.phrase_split && chars >= target) ||
                 chars >= e->config.max_segment_length)) {
                size_t previous = at;
                if (at < protected_until) {
                    at += w;
                    continue;
                }
                while (previous && (s[previous - 1] == ' ' || s[previous - 1] == '\t' ||
                                    s[previous - 1] == '\r'))
                    previous--;
                /* A number, sign, currency prefix or fraction separator may
                   bind to the following token even across spaces. Retain it
                   without inspecting unavailable future input. */
                if (previous &&
                    ((s[previous - 1] >= '0' && s[previous - 1] <= '9') || s[previous - 1] == '/' ||
                     s[previous - 1] == ':' || s[previous - 1] == '+' || s[previous - 1] == '-')) {
                    at += w;
                    continue;
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
                    if (ttf_resource_get(e, "currency_prefix", "*", key, NULL) ||
                        ttf_resource_get(e, "quantity_range_separator", "*", key, NULL)) {
                        at += w;
                        continue;
                    }
                    if (i >= 2) {
                        size_t previous_token = i - 1, pack;
                        int continuation = 0;
                        while (previous_token &&
                               tokens[previous_token - 1].script == TTF_SCRIPT_WHITESPACE)
                            previous_token--;
                        if (previous_token &&
                            tokens[previous_token - 1].script == TTF_SCRIPT_DIGIT) {
                            continuation =
                                ttf_resource_get(e, "numeric_continuation", "*", key, NULL) != NULL;
                            for (pack = 0; !continuation && pack < e->pack_count; ++pack)
                                continuation = ttf_resource_get(e, "numeric_continuation",
                                                                e->packs[pack]->language_tag, key,
                                                                NULL) != NULL;
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
    *boundary = selected;
    return TTF_OK;
}
