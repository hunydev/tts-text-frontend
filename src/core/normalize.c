#include "../pronunciation/korean_phoneme.h"
#include "internal.h"
#include "speech_output.h"
#include "../cjk/cjk_reading.h"
#include "../language/context_memo.h"
#include "../pronunciation/trace.h"
ttf_status_t ttf_trace_foreign_lexicon(const ttf_engine_t *, const char *, const char *,
                                       const char *, size_t, const char *);
#include "../protected/protected.h"
#include "../semantic/numeric_width.h"
#include "../unicode_fallback/fallback.h"
const ttf_entry_t *ttf_rule_match(const ttf_engine_t *, const char *, const char *, size_t,
                                  size_t *);
ttf_status_t ttf_foreign_render(const ttf_engine_t *, const ttf_language_pack_t *,
                                const ttf_language_pack_t *, const char *, size_t,
                                ttf_lexical_class_t, ttf_buffer_t *, char[64],
                                ttf_resource_layer_t *);
static void tag(char dest[TTF_LANGUAGE_TAG_SIZE], const char *source) {
    (void)snprintf(dest, TTF_LANGUAGE_TAG_SIZE, "%s", source);
}
/* Aliases share the normal layer order; an exact spelling wins ties, but a
   system exact spelling must not shadow a user's canonical pronunciation. */
static ttf_status_t pronunciation_candidate(const ttf_engine_t *engine, const char *source,
                                            const char *target, const char *text, size_t length,
                                            const char **best, ttf_resource_layer_t *layer,
                                            const char **selected_text, size_t *selected_length) {
    const char *candidate = NULL;
    ttf_resource_layer_t candidate_layer = TTF_LAYER_COMMON;
    ttf_status_t status =
        ttf_foreign_lookup(engine, source, target, text, length, &candidate, &candidate_layer);
    if (status == TTF_OK && candidate && (!*best || candidate_layer > *layer)) {
        *best = candidate;
        *layer = candidate_layer;
        *selected_text = text;
        *selected_length = length;
    }
    return status;
}
typedef struct {
    size_t begin, end;
} context_scope_t;

/* A complete quotation is an independent context window. Numeric/lexical
   classification still sees the original token stream and byte offsets. */
static context_scope_t *quote_scopes(const char *text, size_t length, const ttf_token_t *tokens,
                                     size_t count) {
    context_scope_t *scopes;
    size_t starts[32], depth = 0, i;
    uint32_t closers[32];
    if (!count)
        return NULL;
    if (count > SIZE_MAX / sizeof(*scopes))
        return NULL;
    scopes = (context_scope_t *)calloc(count, sizeof(*scopes));
    if (!scopes)
        return NULL;
    for (i = 0; i < count; ++i) {
        uint32_t cp, closer;
        size_t width, j;
        if (tokens[i].script != TTF_SCRIPT_PUNCTUATION)
            continue;
        if (ttf_utf8_decode(text + tokens[i].begin, length - tokens[i].begin, &cp, &width) != 1)
            continue;
        if (depth && cp == closers[depth - 1]) {
            size_t begin = starts[--depth] + 1;
            for (j = begin; j < i; ++j)
                if (!scopes[j].end) {
                    scopes[j].begin = begin;
                    scopes[j].end = i;
                }
        } else if ((closer = ttf_quote_closer(cp)) && depth < 32) {
            starts[depth] = i;
            closers[depth++] = closer;
        }
    }
    return scopes;
}
/* A pack read only when declared (context declared_only 1, ja-JP) owns no
   script in another request: kana in a Korean sentence stays Korean. */
static int undeclared(const ttf_engine_t *e, size_t j) {
    const char *flag;
    if (e->frozen)
        return e->undeclared[j];
    flag = ttf_resource_get(e, "context", e->packs[j]->language_tag, "declared_only", NULL);
    return flag && strcmp(flag, "0") &&
           (!e->default_language || strcmp(e->default_language, e->packs[j]->language_tag));
}
static const ttf_language_pack_t *script_pack(const ttf_engine_t *e, ttf_script_t script) {
    const ttf_language_pack_t *best = NULL;
    int score = 0;
    size_t j;
    for (j = 0; j < e->pack_count; j++) {
        int n = e->packs[j]->script_score && !undeclared(e, j)
                    ? e->packs[j]->script_score(script)
                    : 0;
        if (n > score) {
            score = n;
            best = e->packs[j];
        }
    }
    return best;
}
/* Sentence language (docs/matrix-language.ko.md). Each sentence of the context
   view gets one matrix pack. A pack that absorbs foreign material (resource
   `context <tag> absorb_foreign 1`, the default) keeps per-token scoring; for a
   pack that does not, every token of the sentence is routed to it except words
   of a script another pack owns, which keep their own reading. */
typedef struct {
    const ttf_language_pack_t *pack; /* NULL: per-token scoring decides */
    /* The sentence's decided language (kana or hanzi sentence, or a matrix chosen
       by words or grammar); NULL when the sentence fell back to the default. */
    const ttf_language_pack_t *sentence;
    char why[40];
} matrix_route_t;
typedef struct {
    size_t count;
    matrix_route_t routes[];
} matrix_plan_t;
static const char matrix_memo_identity = 0;

static int pack_flag(const ttf_engine_t *e, const ttf_language_pack_t *pack, const char *key,
                     int fallback) {
    const char *value = ttf_resource_get(e, "context", pack->language_tag, key, NULL);
    return value ? strcmp(value, "0") != 0 : fallback;
}
/* An eojeol carries its particles and endings, so a pack may weigh its words. */
static unsigned word_weight(const ttf_engine_t *e, const ttf_language_pack_t *pack) {
    const char *value = ttf_resource_get(e, "context", pack->language_tag, "sentence_word_weight", NULL);
    return value && *value >= '1' && *value <= '9' && !value[1] ? (unsigned)(*value - '0') : 1u;
}
static uint32_t token_cp(const ttf_pack_input_t *in, size_t k) {
    uint32_t cp = 0;
    size_t width;
    if (in->tokens[k].end > in->tokens[k].begin &&
        ttf_utf8_decode(in->text + in->tokens[k].begin, in->tokens[k].end - in->tokens[k].begin,
                        &cp, &width) == 1 &&
        width == in->tokens[k].end - in->tokens[k].begin)
        return cp;
    return 0;
}
/* A sentence ends after . ! ? (ASCII or ideographic/full-width) followed by
   whitespace or the end of the view, and at a newline. */
static int sentence_end(const ttf_pack_input_t *in, size_t k) {
    const ttf_token_t *t = &in->tokens[k];
    if (t->script == TTF_SCRIPT_WHITESPACE)
        return memchr(in->text + t->begin, '\n', t->end - t->begin) != NULL;
    if (t->script == TTF_SCRIPT_PUNCTUATION) {
        uint32_t cp = token_cp(in, k);
        if (cp == '.' || cp == '!' || cp == '?' || cp == 0x3002 || cp == 0xff01 || cp == 0xff1f)
            return k + 1 == in->token_count || in->tokens[k + 1].script == TTF_SCRIPT_WHITESPACE;
    }
    return 0;
}
static uint32_t matrix_closer(uint32_t cp) {
    switch (cp) {
    case '(':
        return ')';
    case '[':
        return ']';
    case '{':
        return '}';
    case 0xff08:
        return 0xff09;
    default:
        return ttf_quote_closer(cp);
    }
}
/* A letter run joined to an earlier letter or digit without whitespace
   (report.txt, R&D, x-y, 3km) continues that word; it is not another vote. */
static int continues_word(const ttf_pack_input_t *in, size_t begin, size_t k) {
    while (k > begin && in->tokens[k - 1].end == in->tokens[k].begin) {
        ttf_script_t script = in->tokens[--k].script;
        if (script == TTF_SCRIPT_WHITESPACE)
            return 0;
        if (script != TTF_SCRIPT_PUNCTUATION && script != TTF_SCRIPT_SYMBOL)
            return 1;
    }
    return 0;
}
/* A word written only in capitals (RAM, SSD, Q) is a code or an initialism;
   such words appear in every language and are not sentence evidence. */
static int capital_code(const char *s, size_t n) {
    size_t i;
    int upper = 0;
    for (i = 0; i < n; ++i) {
        if (s[i] >= 'a' && s[i] <= 'z')
            return 0;
        upper |= s[i] >= 'A' && s[i] <= 'Z';
    }
    return upper;
}
static size_t pack_index(const ttf_engine_t *e, const ttf_language_pack_t *pack) {
    size_t j;
    for (j = 0; j < e->pack_count; ++j)
        if (e->packs[j] == pack)
            return j;
    return SIZE_MAX;
}
/* Decide the matrix of tokens [begin, end). Contents of complete brackets and
   quotations do not vote. A word of a grammar-attaching pack written against a
   preceding token of another script (meeting은, 3시에, "..."라고) is grammar
   evidence; otherwise words vote for the pack owning their script. Single
   letters, all-capital codes and runs continuing a word (3km, report.txt) do
   not vote, and one word or a tie is no sentence language. */
static const ttf_language_pack_t *sentence_matrix(const ttf_engine_t *e,
                                                  const ttf_pack_input_t *in, size_t begin,
                                                  size_t end, char why[40]) {
    const ttf_language_pack_t *fallback = ttf_find_pack(e, e->default_language), *best = NULL;
    unsigned votes[TTF_MAX_PACKS] = {0}, grammar[TTF_MAX_PACKS] = {0};
    uint32_t closers[32];
    size_t openers[32], depth = 0, k, j, top = SIZE_MAX, second = 0;
    int *inner = (int *)calloc(end - begin + 1, sizeof(*inner)), nested = 0;
    const char *basis = "vote";
    /* One pass pairs brackets and quotations; unmatched openers mark nothing. */
    for (k = begin; inner && k < end; ++k) {
        uint32_t cp = in->tokens[k].script == TTF_SCRIPT_PUNCTUATION ? token_cp(in, k) : 0, closer;
        if (!cp)
            continue;
        if (depth && cp == closers[depth - 1]) {
            --depth;
            ++inner[openers[depth] + 1 - begin];
            --inner[k - begin];
        } else if ((closer = matrix_closer(cp)) && depth < 32) {
            openers[depth] = k;
            closers[depth++] = closer;
        }
    }
    for (k = begin; k < end; ++k) {
        const ttf_token_t *t = &in->tokens[k];
        const ttf_language_pack_t *owner;
        if (inner)
            nested += inner[k - begin];
        if (nested > 0 || !(owner = script_pack(e, t->script)) ||
            (j = pack_index(e, owner)) == SIZE_MAX)
            continue;
        if (k > begin && in->tokens[k - 1].end == t->begin &&
            in->tokens[k - 1].script != t->script &&
            in->tokens[k - 1].script != TTF_SCRIPT_WHITESPACE &&
            pack_flag(e, owner, "attached_grammar", 0)) {
            ++grammar[j];
            continue;
        }
        if (continues_word(in, begin, k) || t->end - t->begin == 1 ||
            capital_code(in->text + t->begin, t->end - t->begin))
            continue;
        votes[j] += word_weight(e, owner);
    }
    free(inner);
    for (j = 0; j < e->pack_count; ++j)
        if (grammar[j] && (top == SIZE_MAX || grammar[j] > grammar[top]))
            top = j;
    if (top != SIZE_MAX) {
        basis = "grammar";
        best = e->packs[top];
    } else {
        for (j = 0; j < e->pack_count; ++j)
            if (top == SIZE_MAX || votes[j] > votes[top])
                top = j;
        for (j = 0; j < e->pack_count; ++j)
            if (j != top && votes[j] > second)
                second = votes[j];
        /* One word, or a tie, is not a sentence language. */
        if (top != SIZE_MAX && votes[top] >= 2 && votes[top] > second)
            best = e->packs[top];
    }
    if (!best) {
        basis = "default";
        best = fallback;
    }
    if (why) {
        unsigned total = 0;
        for (j = 0; j < e->pack_count; ++j)
            total += votes[j];
        snprintf(why, 40, "matrix=%s(%s %u/%u)", best ? best->language_tag : "und", basis,
                 top != SIZE_MAX ? votes[top] : 0u, total);
    }
    return best;
}
static void sentence_bounds(const ttf_pack_input_t *in, size_t index, size_t *begin, size_t *end) {
    size_t b = index, n = in->token_count;
    while (b > 0 && !sentence_end(in, b - 1))
        --b;
    *begin = b;
    while (index < n && !sentence_end(in, index))
        ++index;
    *end = index < n ? index + 1 : n;
}
/* Hiragana in a token that also holds kanji or katakana (明日は), or attached to a
   neighbouring word or number (3時に, iPhoneを), is Japanese grammar. */
static int hiragana_token(const ttf_pack_input_t *in, size_t k, size_t begin, size_t end,
                          int *grammar) {
    const ttf_token_t *t = &in->tokens[k];
    size_t at = t->begin;
    int hiragana = 0, other = 0;
    while (at < t->end) {
        uint32_t cp;
        size_t w;
        if (ttf_utf8_decode(in->text + at, t->end - at, &cp, &w) != 1)
            break;
        if (cp >= 0x3041 && cp <= 0x309f)
            hiragana = 1;
        else if ((cp >= 0x30a1 && cp <= 0x30fa) || (cp >= 0x3400 && cp <= 0x9fff))
            other = 1;
        at += w;
    }
    if (!hiragana)
        return 0;
    *grammar |= other ||
                (k > begin && in->tokens[k - 1].end == t->begin &&
                 in->tokens[k - 1].script != TTF_SCRIPT_WHITESPACE &&
                 in->tokens[k - 1].script != TTF_SCRIPT_PUNCTUATION) ||
                (k + 1 < end && in->tokens[k + 1].begin == t->end &&
                 in->tokens[k + 1].script != TTF_SCRIPT_WHITESPACE &&
                 in->tokens[k + 1].script != TTF_SCRIPT_PUNCTUATION);
    return 1;
}
/* With the Korean hanja/kana reading options, a sentence without Hangul is still
   Chinese or Japanese when it is a whole sentence: it ends in a full stop (a
   closing quote or bracket may follow), or it has ten or more CJK characters. A
   bare word (列車, とうきょう) stays with the request and is read (2026-10-12,
   project owner: the options are for Chinese and Japanese inside Korean sentences;
   before, they kept every sentence Korean and read whole Chinese sentences
   character by character). */
static int whole_sentence(const ttf_pack_input_t *in, size_t begin, size_t end) {
    size_t k = end, cjk = 0;
    while (k > begin) {
        const ttf_token_t *t = &in->tokens[k - 1];
        uint32_t cp;
        size_t w;
        if (t->script == TTF_SCRIPT_WHITESPACE) {
            --k;
            continue;
        }
        if (t->script != TTF_SCRIPT_PUNCTUATION ||
            ttf_utf8_decode(in->text + t->begin, t->end - t->begin, &cp, &w) != 1)
            break;
        if (cp == 0x3002 || cp == 0xff01 || cp == 0xff1f || cp == 0xff0e || cp == '.' || cp == '!' ||
            cp == '?')
            return 1;
        if (!(cp == '"' || cp == '\'' || cp == 0x201d || cp == 0x2019 || cp == 0x300d ||
              cp == 0x300f || cp == 0xff09 || cp == ')' || cp == 0x3011))
            break;
        --k;
    }
    for (k = begin; k < end; ++k) {
        size_t at = in->tokens[k].begin;
        while (at < in->tokens[k].end) {
            uint32_t cp;
            size_t w;
            if (ttf_utf8_decode(in->text + at, in->tokens[k].end - at, &cp, &w) != 1)
                break;
            cjk += (cp >= 0x3041 && cp <= 0x30ff) || (cp >= 0x3400 && cp <= 0x9fff);
            at += w;
        }
    }
    return cjk >= 10;
}
static void sentence_bounds(const ttf_pack_input_t *in, size_t index, size_t *begin, size_t *end);
/* The sentence of token `index` has a Hangul word (a Korean sentence). */
static int hangul_sentence(const ttf_pack_input_t *in, size_t index) {
    size_t begin, end, k;
    sentence_bounds(in, index, &begin, &end);
    for (k = begin; k < end; ++k)
        if (in->tokens[k].script == TTF_SCRIPT_HANGUL)
            return 1;
    return 0;
}
static int reading_options(const ttf_engine_t *e) {
    return e->frontend_options.kana == TTF_KANA_KOREAN || e->frontend_options.hanja == TTF_HANJA_KOREAN;
}
/* A kana sentence (2026-10-08, project owner): with hiragana grammar, or only
   kana and no Latin word, and no Hangul at all, the sentence belongs to the pack
   declaring `context <tag> kana_sentence 1` (ja-JP), every token included. A
   Hangul word keeps the sentence out (「ありがとう」라고 stays Korean); with the
   Korean kana/hanja reading options only a whole sentence counts; a standalone kana
   word in an English sentence is no grammar. */
static const ttf_language_pack_t *kana_sentence(const ttf_engine_t *e, const ttf_pack_input_t *in,
                                                size_t begin, size_t end) {
    const ttf_language_pack_t *pack = NULL;
    size_t j, k;
    int grammar = 0, hiragana = 0, latin = 0;
    if (reading_options(e) && !whole_sentence(in, begin, end))
        return NULL;
    for (j = 0; j < e->pack_count && !pack; ++j)
        if (pack_flag(e, e->packs[j], "kana_sentence", 0))
            pack = e->packs[j];
    if (!pack)
        return NULL;
    for (k = begin; k < end; ++k) {
        if (in->tokens[k].script == TTF_SCRIPT_HANGUL)
            return NULL;
        if (in->tokens[k].script == TTF_SCRIPT_LATIN)
            latin = 1;
        else
            hiragana |= hiragana_token(in, k, begin, end, &grammar);
    }
    return grammar || (hiragana && !latin) ? pack : NULL;
}
/* A hanzi sentence (2026-10-11, project owner: a kanji-only sentence prefers
   Chinese): no Hangul, no kana, at least two hanzi, at least twice as many hanzi
   as Latin words, and no character the pack lists as han_foreign (Japanese-only
   or traditional forms: 駅 円 國 時) belongs to the pack declaring `context <tag>
   han_sentence 1` (zh-CN), every token included. A request whose own pack takes
   kana sentences (ja-JP) keeps its kanji sentences; with the Korean kana/hanja
   reading options only a whole sentence counts. In a zh-CN request one hanzi is
   enough (Galaxy S24 Ultra售价8999元). */
static const ttf_language_pack_t *han_sentence(const ttf_engine_t *e, const ttf_pack_input_t *in,
                                               size_t begin, size_t end) {
    const ttf_language_pack_t *pack = NULL,
                              *declared = e->default_language ? ttf_find_pack(e, e->default_language)
                                                              : NULL;
    size_t j, k, han = 0, latin = 0;
    if ((reading_options(e) && !whole_sentence(in, begin, end)) ||
        (declared && pack_flag(e, declared, "kana_sentence", 0)))
        return NULL;
    for (j = 0; j < e->pack_count && !pack; ++j)
        if (pack_flag(e, e->packs[j], "han_sentence", 0))
            pack = e->packs[j];
    if (!pack)
        return NULL;
    for (k = begin; k < end; ++k) {
        const ttf_token_t *t = &in->tokens[k];
        size_t at = t->begin;
        if (t->script == TTF_SCRIPT_HANGUL || t->script == TTF_SCRIPT_KANA)
            return NULL;
        if (t->script == TTF_SCRIPT_LATIN) {
            /* 60km/h: letters attached to a number are a unit, not a word. */
            const ttf_token_t *prev = k > begin ? &in->tokens[k - 1] : NULL;
            latin += !(prev && prev->end == t->begin &&
                       (prev->script == TTF_SCRIPT_DIGIT ||
                        (prev->end - prev->begin == 1 && strchr("/-.", in->text[prev->begin]))));
            continue;
        }
        if (t->script == TTF_SCRIPT_DIGIT || t->script == TTF_SCRIPT_WHITESPACE)
            continue;
        while (at < t->end) {
            uint32_t cp;
            size_t w;
            char key[5];
            if (ttf_utf8_decode(in->text + at, t->end - at, &cp, &w) != 1 || w > 4)
                break;
            memcpy(key, in->text + at, w);
            key[w] = '\0';
            if (cp >= 0x3000 &&
                ttf_resource_get(e, "han_foreign", pack->language_tag, key, NULL))
                return NULL;
            /* a hanzi may share a token with a symbol (10²是一百) */
            han += (cp >= 0x3400 && cp <= 0x4dbf) || (cp >= 0x4e00 && cp <= 0x9fff) ||
                   (cp >= 0xf900 && cp <= 0xfaff) || (cp >= 0x20000 && cp <= 0x3134f);
            at += w;
        }
    }
    if (declared == pack)
        return han >= 1 ? pack : NULL;
    return han >= 2 && han >= 2 * latin ? pack : NULL;
}
static void matrix_route(const ttf_engine_t *e, const ttf_pack_input_t *in, size_t begin,
                         size_t end, matrix_route_t *routes) {
    char why[40];
    const ttf_language_pack_t *kana =
        e->matrix_policy == TTF_MATRIX_DECLARED ? NULL : kana_sentence(e, in, begin, end);
    const ttf_language_pack_t *matrix = e->matrix_policy == TTF_MATRIX_DECLARED
                                            ? ttf_find_pack(e, e->default_language)
                                            : sentence_matrix(e, in, begin, end, why);
    int forced = matrix && !pack_flag(e, matrix, "absorb_foreign", 1);
    size_t k;
    if (e->matrix_policy == TTF_MATRIX_DECLARED)
        snprintf(why, sizeof(why), "matrix=%s(declared)", matrix ? matrix->language_tag : "und");
    if (!kana && e->matrix_policy != TTF_MATRIX_DECLARED &&
        (kana = han_sentence(e, in, begin, end)) != NULL)
        snprintf(why, sizeof(why), "matrix=%s(han)", kana->language_tag);
    else if (kana)
        snprintf(why, sizeof(why), "matrix=%s(kana)", kana->language_tag);
    if (kana) {
        for (k = begin; k < end; ++k) {
            routes[k - begin].pack = kana;
            routes[k - begin].sentence = kana;
            memcpy(routes[k - begin].why, why, sizeof(why));
        }
        return;
    }
    for (k = begin; k < end; ++k) {
        const ttf_language_pack_t *owner = script_pack(e, in->tokens[k].script);
        routes[k - begin].pack = forced && (!owner || owner == matrix) ? matrix : NULL;
        routes[k - begin].sentence =
            e->matrix_policy == TTF_MATRIX_DECLARED || strstr(why, "(default") ? NULL : matrix;
        memcpy(routes[k - begin].why, why, sizeof(why));
    }
}
static ttf_status_t build_matrix_plan(const ttf_pack_input_t *in, void **out) {
    matrix_plan_t *plan;
    size_t begin = 0, end;
    *out = NULL;
    if (in->token_count > (SIZE_MAX - sizeof(*plan)) / sizeof(plan->routes[0]))
        return TTF_ELIMIT;
    plan = (matrix_plan_t *)calloc(1, sizeof(*plan) + in->token_count * sizeof(plan->routes[0]));
    if (!plan)
        return TTF_ENOMEM;
    plan->count = in->token_count;
    while (begin < in->token_count) {
        sentence_bounds(in, begin, &begin, &end);
        matrix_route(in->engine, in, begin, end, plan->routes + begin);
        begin = end;
    }
    *out = plan;
    return TTF_OK;
}
static void dispose_matrix_plan(void *plan) {
    free(plan);
}
/* NULL keeps per-token scoring; otherwise the sentence's non-absorbing matrix. */
static const ttf_language_pack_t *matrix_pack(const ttf_engine_t *e, const ttf_pack_input_t *in,
                                              char why[40]) {
    matrix_plan_t *plan;
    matrix_route_t route;
    size_t begin, end;
    if (e->matrix_policy == TTF_MATRIX_LEGACY || in->index >= in->token_count)
        return NULL;
    if (ttf_context_memo_enabled(in) &&
        (plan = (matrix_plan_t *)ttf_context_memo_get(in, &matrix_memo_identity,
                                                      build_matrix_plan, dispose_matrix_plan))) {
        memcpy(why, plan->routes[in->index].why, 40);
        return plan->routes[in->index].pack;
    }
    /* Direct callers without a request memo decide the one sentence. */
    sentence_bounds(in, in->index, &begin, &end);
    {
        matrix_route_t *routes = (matrix_route_t *)malloc((end - begin) * sizeof(*routes));
        if (!routes)
            return NULL;
        matrix_route(e, in, begin, end, routes);
        route = routes[in->index - begin];
        free(routes);
    }
    memcpy(why, route.why, 40);
    return route.pack;
}
/* The decided language of the current token's sentence (see matrix_route_t),
   NULL when none is decided or the matrix policy is not auto. A pack that claims
   every token of its own request (zh-CN, ja-JP) leaves a Korean or English
   sentence to its language with it (2026-10-12, project owner: 오전 9시 in a zh-CN
   request read 九시). */
const ttf_language_pack_t *ttf_sentence_language(const ttf_pack_input_t *in) {
    const ttf_engine_t *e = in->engine;
    matrix_plan_t *plan;
    matrix_route_t *routes;
    const ttf_language_pack_t *found;
    size_t begin, end;
    if (!e || e->matrix_policy != TTF_MATRIX_AUTO || in->index >= in->token_count)
        return NULL;
    if (ttf_context_memo_enabled(in) &&
        (plan = (matrix_plan_t *)ttf_context_memo_get(in, &matrix_memo_identity, build_matrix_plan,
                                                      dispose_matrix_plan)))
        return plan->routes[in->index].sentence;
    sentence_bounds(in, in->index, &begin, &end);
    routes = (matrix_route_t *)calloc(end - begin, sizeof(*routes));
    if (!routes)
        return NULL;
    matrix_route(e, in, begin, end, routes);
    found = routes[in->index - begin].sentence;
    free(routes);
    return found;
}
static int declared_only(const ttf_engine_t *e, size_t j) {
    const char *flag = ttf_resource_get(e, "context", e->packs[j]->language_tag, "declared_only", NULL);
    return flag && strcmp(flag, "0");
}
static const ttf_language_pack_t *context_pack(const ttf_engine_t *e, const ttf_pack_input_t *in,
                                               char trace[192]) {
    const ttf_language_pack_t *best = ttf_find_pack(e, e->default_language);
    int score = 0;
    size_t j;
    for (j = 0; j < e->pack_count; j++) {
        int n = e->packs[j]->context_score ? e->packs[j]->context_score(in) : 0;
        size_t used = strlen(trace);
        /* A pack read only when declared (ja-JP) is silent elsewhere. */
        if (!n && undeclared(e, j))
            continue;
        if (used < 191)
            snprintf(trace + used, 192 - used, "%s%s=%d", used ? " " : "",
                     e->packs[j]->language_tag, n);
        if (n > score) {
            score = n;
            best = e->packs[j];
        }
    }
    {
        /* The score trace stays; a matrix that changes the route is appended. */
        char why[40];
        const ttf_language_pack_t *matrix = matrix_pack(e, in, why);
        size_t used = strlen(trace);
        if (matrix && matrix != best) {
            if (used < 191)
                snprintf(trace + used, 192 - used, "%s%s", used ? " " : "", why);
            best = matrix;
        }
    }
    return best;
}
/* Every recognizer rejected this pure digit/hyphen object. Text output keeps it
   literally (docs/api.md), but speech output would hand raw digits to TTS, so it
   is read with the renderer's own identifier grammar. The IR keeps the
   recognizer's verdict. Only the two numeric rejections qualify; alphanumeric
   codes are a separate rule family. A phone-shaped run (first group 0X or 0XX)
   stays preserved: a malformed phone number keeps its existing visible-error
   contract (phone-cue-v21, phone_number_tail overlays). */
/* 417-00-32154번: an attached Hangul word after the last digit group is kept
   as written and is not part of the identifier. */
static size_t identifier_tail(const char *surface, size_t length) {
    size_t end = length;
    while (end && (unsigned char)surface[end - 1] >= 0x80)
        --end;
    return end && surface[end - 1] >= '0' && surface[end - 1] <= '9' ? end : length;
}
static int identifier_fallback(const ttf_semantic_t *sem, const char *surface, size_t length) {
    size_t i, groups = 1, first = 0, widths[3] = {0, 0, 0};
    int digit = 0, date = !strcmp(sem->rule, "syntax.invalid-date");
    if (sem->type != TTF_SEM_TEXT ||
        (strcmp(sem->rule, "syntax.invalid-phone") && strcmp(sem->rule, "syntax.invalid-range") &&
         !date) ||
        !length || length >= TTF_IR_VALUE_SIZE)
        return 0;
    length = identifier_tail(surface, length);
    for (i = 0; i < length; ++i) {
        if (surface[i] >= '0' && surface[i] <= '9') {
            digit = 1;
            if (groups == 1)
                ++first;
            if (groups <= 3)
                ++widths[groups - 1];
        } else if (surface[i] == '-' && digit) {
            digit = 0;
            ++groups;
        } else if (surface[i] == ' ' && i + 1 < length &&
                   (surface[i + 1] == '-' || (i && surface[i - 1] == '-'))) {
            continue; /* 201218 -12531: a space next to the hyphen */
        } else
            return 0;
    }
    /* A malformed phone number (0X/0XX, a 3-4 digit middle, a 3-5 digit last
       group: 010-1234-56789) keeps its visible error; 077-19-254985 or
       080-434-146780 is not phone-shaped and is read as an identifier. */
    /* 031-120, 01-126834560: two groups are not a malformed phone (2026-10-02). */
    if (surface[0] == '0' && (first == 2 || first == 3) && groups == 3 &&
        widths[1] >= 3 && widths[1] <= 4 && widths[2] >= 3 && widths[2] <= 5)
        return 0;
    /* A YYYY-MM-DD shape that is not a date (2026-13-45) keeps its visible
       error, like a malformed phone number; 3883-0661-9521 is an identifier. */
    if (date && groups == 3 && widths[0] == 4 && widths[1] <= 2 && widths[2] <= 2)
        return 0;
    return digit && groups >= 2;
}
static ttf_status_t read_identifier(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                                    const ttf_semantic_t *sem, const char *surface, size_t length,
                                    ttf_buffer_t *normalized, int *done) {
    ttf_semantic_t identifier = *sem;
    ttf_buffer_t spoken = {0};
    ttf_status_t st;
    *done = 0;
    size_t i, used = 0, tail = identifier_tail(surface, length);
    identifier.type = TTF_SEM_DIGIT_SEQUENCE;
    snprintf(identifier.unit, sizeof(identifier.unit), "financial_identifier");
    for (i = 0; i < tail; ++i)
        if (surface[i] != ' ')
            identifier.value[used++] = surface[i];
    identifier.value[used] = '\0';
    st = renderer->verbalize(e, &identifier, surface, tail, &spoken);
    if (st == TTF_OK && tail < length)
        st = ttf_buffer_append(&spoken, surface + tail, length - tail);
    if (st != TTF_OK) {
        ttf_buffer_free(&spoken);
        /* A pack without identifier grammar keeps the literal under the guard. */
        return st == TTF_EINVAL ? TTF_OK : st;
    }
    ttf_buffer_free(normalized);
    *normalized = spoken;
    *done = 1;
    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, normalized->length);
    return ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, renderer->language_tag,
                          "decision", "identifier-fallback", "", identifier.value,
                          "rejected-numeric-read-as-identifier", SIZE_MAX);
}
/* Korean speech reads a rejected numeric span as written through the ko-KR
   written fallback instead of keeping it (27만 천 원, 1970년 2월 29일, 9034MDSAP;
   project-owner decision 2026-10-02). numeric_fallback=preserve keeps the old
   behaviour; a colon that is not a time (24:00) stays preserved as decided. */
static int written_fallback(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                            const ttf_semantic_t *sem, const char *surface, size_t length) {
    static const char *const rules[] = {"syntax.invalid-", "syntax.unsupported-", "syntax.width-",
                                        "ko:invalid-",     "ko:unsupported-",     "ko:ambiguous-",
                                        "en.date.invalid"};
    size_t i;
    int digit = 0;
    /* English speech reads them as written too (75F seventy five F, 787s seven
       eighty sevens), as wetext reads every leftover digit; text output keeps
       the written form (project-owner decision 2026-10-07). */
    if (e->numeric_fallback_off || sem->type != TTF_SEM_TEXT || !renderer ||
        !strcmp(sem->rule, "syntax.invalid-time") ||
        (strcmp(renderer->language_tag, "ko-KR") &&
         (strcmp(renderer->language_tag, "en-US") ||
          e->frontend_options.output_mode != TTF_OUTPUT_SPEECH)))
        return 0;
    for (i = 0; i < length; ++i)
        digit |= (surface[i] >= '0' && surface[i] <= '9') ||
                 (i + 2 < length && (unsigned char)surface[i] == 0xEF && /* fullwidth digits */
                  (unsigned char)surface[i + 1] == 0xBC && (unsigned char)surface[i + 2] >= 0x90 &&
                  (unsigned char)surface[i + 2] <= 0x99);
    /* The English reading is ASCII: a fullwidth span keeps its width guard. */
    for (i = 0; digit && !strcmp(renderer->language_tag, "en-US") && i < length; ++i)
        if ((unsigned char)surface[i] >= 0x80)
            return 0;
    for (i = 0; digit && i < sizeof(rules) / sizeof(rules[0]); ++i)
        if (!strncmp(sem->rule, rules[i], strlen(rules[i])))
            return 1;
    return 0;
}
static ttf_status_t read_written(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                                 const ttf_semantic_t *sem, const char *surface, size_t length,
                                 ttf_buffer_t *normalized, int *done) {
    ttf_semantic_t written = *sem;
    ttf_buffer_t spoken = {0};
    ttf_status_t st;
    written.type = TTF_SEM_DIGIT_SEQUENCE;
    snprintf(written.unit, sizeof(written.unit), "written_fallback");
    st = renderer->verbalize(e, &written, surface, length, &spoken);
    if (st != TTF_OK) {
        ttf_buffer_free(&spoken);
        return st == TTF_EINVAL || st == TTF_ERESOURCE ? TTF_OK : st;
    }
    ttf_buffer_free(normalized);
    *normalized = spoken;
    *done = 1;
    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, normalized->length);
    return ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, renderer->language_tag,
                          "decision", "written-fallback", "", sem->rule,
                          "rejected-numeric-read-as-written", SIZE_MAX);
}
static int foreign_type(ttf_semantic_type_t t) {
    return t == TTF_SEM_WORD || t == TTF_SEM_INITIALISM || t == TTF_SEM_ACRONYM ||
           t == TTF_SEM_ALNUM_TERM || t == TTF_SEM_ORDINAL || t == TTF_SEM_ABBREVIATION;
}
static ttf_status_t add_span(ttf_result_t *r, const ttf_span_t *span, size_t *capacity) {
    if (r->span_count == *capacity) {
        size_t cap = *capacity ? *capacity * 2 : 32;
        ttf_span_t *p;
        if (cap < *capacity || cap > SIZE_MAX / sizeof(*p))
            return TTF_ELIMIT;
        p = (ttf_span_t *)realloc(r->spans, cap * sizeof(*p));
        if (!p)
            return TTF_ENOMEM;
        r->spans = p;
        *capacity = cap;
    }
    r->spans[r->span_count++] = *span;
    return TTF_OK;
}
/* An electronic address is neutral context evidence. Hide its internal Latin
   tokens for this one routing decision; do not mutate the shared token cache. */
static const ttf_language_pack_t *electronic_context(const ttf_engine_t *e,
                                                     const ttf_pack_input_t *in, size_t consumed,
                                                     char trace[192]) {
    ttf_pack_input_t neutral = *in;
    ttf_engine_t view = *e;
    ttf_token_t *tokens;
    const ttf_language_pack_t *pack;
    size_t after = in->index + consumed;
    if (in->token_count > SIZE_MAX / sizeof(*tokens))
        return NULL;
    tokens = malloc(in->token_count * sizeof(*tokens));
    if (!tokens)
        return NULL;
    memcpy(tokens, in->tokens, in->index * sizeof(*tokens));
    tokens[in->index] = in->tokens[in->index];
    tokens[in->index].end = in->tokens[after - 1].end;
    tokens[in->index].script = TTF_SCRIPT_OTHER;
    memcpy(tokens + in->index + 1, in->tokens + after, (in->token_count - after) * sizeof(*tokens));
    neutral.tokens = tokens;
    neutral.token_count -= consumed - 1;
    view.context_memo = NULL;
    neutral.engine = &view;
    snprintf(trace, 192, "electronic-neutral ");
    pack = context_pack(e, &neutral, trace);
    free(tokens);
    return pack;
}
/* A precomposed Hangul syllable (3 UTF-8 bytes) at p. */
static int hangul_syllable(const char *p) {
    const unsigned char *u = (const unsigned char *)p;
    uint32_t cp;
    if (u[0] < 0xea || u[0] > 0xed || (u[1] & 0xc0) != 0x80 || (u[2] & 0xc0) != 0x80)
        return 0;
    cp = ((uint32_t)(u[0] & 0x0f) << 12) | ((uint32_t)(u[1] & 0x3f) << 6) | (uint32_t)(u[2] & 0x3f);
    return cp >= 0xac00 && cp <= 0xd7a3;
}

/* Output ranges read digit by digit, for the pronunciation output: a phone number
   or a digit sequence (112 일일이), but not a date read that way (8·15 팔일오, which
   links as usual) or a reading with a place value (백십이). */
static ttf_status_t digit_regions(const ttf_result_t *out, const char *text, size_t **digits,
                                  size_t *count) {
    size_t k, cap = 0;
    static const char *const places[] = {"십", "백", "천", "만", "억", "조"};
    for (k = 0; k < out->span_count; ++k) {
        const ttf_span_t *span = &out->spans[k];
        size_t q, p;
        int place = 0;
        if (!(span->semantic.type == TTF_SEM_PHONE ||
              (span->semantic.type == TTF_SEM_DIGIT_SEQUENCE && strcmp(span->semantic.unit, "event_date"))) ||
            span->end <= span->begin)
            continue;
        for (q = span->begin; q + 3 <= span->end && !place; ++q)
            for (p = 0; p < sizeof(places) / sizeof(*places); ++p)
                place |= !memcmp(text + q, places[p], 3);
        if (place)
            continue;
        if (*count * 2 + 2 > cap) {
            size_t next = cap ? cap * 2 : 8;
            size_t *grown = realloc(*digits, next * sizeof(**digits));
            if (!grown)
                return TTF_ENOMEM;
            *digits = grown;
            cap = next;
        }
        (*digits)[*count * 2] = span->begin;
        (*digits)[*count * 2 + 1] = span->end;
        ++*count;
    }
    return TTF_OK;
}

/* X{@pronounce} on one atom: the atom's reading depends on its sentence (112에 신고
   reads 112 digit by digit), so it is read with up to two words on each side and only
   its own output is kept. Returns 0 when the atom's spans cannot be separated. */
static size_t context_edge(const char *t, size_t at, size_t limit, int forward) {
    size_t p = at, spaces = 0;
    while (forward ? p < limit && p - at < 80 : p > limit && at - p < 80) {
        char c = forward ? t[p] : t[p - 1];
        if (c == '\n' || c == '{' || c == '}' || c == '[' || c == ']' || (c == ' ' && ++spaces > 2))
            break;
        p += forward ? 1 : (size_t)-1;
    }
    while (p > limit && p < at && ((unsigned char)t[p] & 0xC0) == 0x80)
        ++p;
    while (forward && p > at && p < limit && ((unsigned char)t[p] & 0xC0) == 0x80)
        --p;
    return p;
}
static int atom_in_context(const ttf_engine_t *e, const char *text, size_t length,
                           const ttf_control_match_t *control, const ttf_buffer_t *base, size_t offset,
                           ttf_buffer_t *output, ttf_result_t *out, size_t *capacity, ttf_status_t *status) {
    size_t ls = context_edge(text, control->begin, 0, 0), re = context_edge(text, control->end, length, 1);
    size_t left = control->begin - ls, k, obeg = SIZE_MAX, oend = 0, begin = output->length;
    ttf_buffer_t ctx = {0};
    ttf_result_t inner = {0};
    ttf_engine_t view = *e;
    int ok = 1;
    view.phoneme_output = 0;
    view.trace_state = NULL;
    view.context_memo = NULL;
    view.frontend_options.input_format = TTF_INPUT_PLAIN;
    *status = ttf_buffer_append(&ctx, text + ls, left);
    if (*status == TTF_OK)
        *status = ttf_buffer_append(&ctx, base->data ? base->data : "", base->length);
    if (*status == TTF_OK)
        *status = ttf_buffer_append(&ctx, text + control->end, re - control->end);
    if (*status == TTF_OK)
        *status = ttf_process_unit(&view, ctx.data ? ctx.data : "", ctx.length,
                                   offset + control->base_begin - left, &inner);
    ttf_buffer_free(&ctx);
    if (*status != TTF_OK)
        return 1;
    for (k = 0; k < inner.span_count; ++k) {
        size_t ib = inner.spans[k].input_begin - (offset + control->base_begin - left),
               ie = inner.spans[k].input_end - (offset + control->base_begin - left);
        int inside = ib >= left && ie <= left + base->length, outside = ie <= left || ib >= left + base->length;
        if (!inside && !outside)
            ok = 0;
        if (inside) {
            obeg = inner.spans[k].begin < obeg ? inner.spans[k].begin : obeg;
            oend = inner.spans[k].end > oend ? inner.spans[k].end : oend;
        }
    }
    if (!ok || obeg == SIZE_MAX) {
        ttf_result_free(&inner);
        return 0;
    }
    *status = ttf_buffer_append(output, inner.text + obeg, oend - obeg);
    for (k = 0; *status == TTF_OK && k < inner.span_count; ++k) {
        ttf_span_t span = inner.spans[k];
        if (span.begin < obeg || span.end > oend)
            continue;
        span.begin = span.begin - obeg + begin;
        span.end = span.end - obeg + begin;
        if (span.begin == begin)
            span.input_begin = span.semantic.input_begin = offset + control->begin;
        if (span.end == output->length)
            span.input_end = span.semantic.input_end = offset + control->end;
        *status = add_span(out, &span, capacity);
    }
    ttf_result_free(&inner);
    return 1;
}

/* [[민주주의의 의의]]{@pronounce}: the span is normalized like plain text, then its
   Hangul is read as spoken (ttf_korean_phonemize) after the whole unit is done,
   so the words around it still decide liaison. */
static ttf_status_t phoneme_control(const ttf_engine_t *e, const char *text, size_t length,
                                    const ttf_control_match_t *control, size_t offset,
                                    ttf_buffer_t *output, ttf_result_t *out, size_t *capacity,
                                    size_t **regions, size_t *region_count, size_t *region_cap) {
    ttf_buffer_t base = {0};
    ttf_result_t inner;
    ttf_engine_t view = *e;
    size_t begin = output->length, k;
    ttf_status_t st = ttf_control_decode(text, control->base_begin, control->base_end, &base);
    if (st != TTF_OK)
        return st;
    if (!control->grouped && atom_in_context(e, text, length, control, &base, offset, output, out, capacity, &st)) {
        ttf_buffer_free(&base);
        if (st != TTF_OK)
            return st;
        goto region;
    }
    view.phoneme_output = 0;
    st = ttf_process_unit(&view, base.data ? base.data : "", base.length, offset + control->base_begin,
                          &inner);
    ttf_buffer_free(&base);
    if (st != TTF_OK)
        return st;
    st = ttf_buffer_append(output, inner.text, inner.length);
    for (k = 0; st == TTF_OK && k < inner.span_count; ++k) {
        ttf_span_t span = inner.spans[k];
        span.begin += begin;
        span.end += begin;
        if (!k)
            span.input_begin = span.semantic.input_begin = offset + control->begin;
        if (k + 1 == inner.span_count)
            span.input_end = span.semantic.input_end = offset + control->end;
        st = add_span(out, &span, capacity);
    }
    ttf_result_free(&inner);
    if (st != TTF_OK)
        return st;
region:
    if (*region_count + 2 > *region_cap) {
        size_t next = *region_cap ? *region_cap * 2 : 8;
        size_t *grown = realloc(*regions, next * sizeof(**regions));
        if (!grown)
            return TTF_ENOMEM;
        *regions = grown;
        *region_cap = next;
    }
    (*regions)[(*region_count)++] = begin;
    (*regions)[(*region_count)++] = output->length;
    return TTF_OK;
}
static ttf_status_t render_control(const ttf_engine_t *e, const ttf_pack_input_t *in,
                                   const ttf_control_match_t *control, size_t offset,
                                   ttf_span_t *span, ttf_buffer_t *output) {
    const ttf_language_pack_t *context, *source = NULL;
    ttf_semantic_t *sem = &span->semantic;
    ttf_token_t *base_tokens = NULL;
    size_t base_count = 0, base_length = control->base_end - control->base_begin;
    ttf_buffer_t decoded = {0}, base = {0};
    const char *source_text = in->text + control->base_begin;
    ttf_status_t status;
    context = context_pack(e, in, span->context_trace);
    if (!context)
        return TTF_EINVAL;
    if (control->kind == TTF_CONTROL_PATTERN && control->grouped) {
        status = ttf_control_decode(in->text, control->base_begin, control->base_end, &base);
        if (status != TTF_OK)
            goto done;
        source_text = base.data;
        base_length = base.length;
        /* Borrowed complete source is carried by the existing pack callback.
           Never truncate a long group into the fixed ABI v1 value field. */
        snprintf(sem->secondary, sizeof(sem->secondary), "control-surface");
    }
    status = ttf_tokenize(source_text, base_length, &base_tokens, &base_count);
    if (status != TTF_OK)
        goto done;
    for (size_t k = 0; k < base_count && !source; ++k)
        source = script_pack(e, base_tokens[k].script);
    span->script = base_count ? base_tokens[0].script : TTF_SCRIPT_SYMBOL;
    free(base_tokens);
    sem->type = TTF_SEM_CUSTOM;
    sem->input_begin = span->input_begin = offset + control->begin;
    sem->input_end = span->input_end = offset + control->end;
    tag(sem->source_lang, source ? source->language_tag : "und");
    tag(sem->context_lang, context->language_tag);
    tag(sem->render_lang, context->language_tag);
    tag(span->verbalizer, context->language_tag);
    snprintf(sem->rule, sizeof(sem->rule), "control.%s",
             control->kind == TTF_CONTROL_PATTERN ? "pattern" : "literal");
    if (base_length < sizeof(sem->value))
        memcpy(sem->value, source_text, base_length);
    else if (control->kind == TTF_CONTROL_PATTERN && !control->grouped) {
        status = TTF_ELIMIT;
        goto done;
    }
    status = ttf_control_decode(in->text, control->payload_begin, control->payload_end, &decoded);
    if (status != TTF_OK)
        goto done;
    ttf_trace_span_begin(e, span->input_begin, span->input_end);
    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, control->end - control->begin);
    if (control->kind == TTF_CONTROL_PATTERN) {
        if (decoded.length >= sizeof(sem->unit)) {
            status = TTF_ELIMIT;
            goto done;
        }
        memcpy(sem->unit, decoded.data, decoded.length);
        status = context->verbalize(e, sem, source_text, base_length, output);
    } else {
        snprintf(sem->unit, sizeof(sem->unit), "@literal");
        status = ttf_buffer_append(output, decoded.data, decoded.length);
    }
    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, control->end - control->begin);
    if (status == TTF_OK)
        status = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_USER, 0, context->language_tag,
                                "input-control", sem->unit, decoded.data ? decoded.data : "",
                                control->kind == TTF_CONTROL_PATTERN && control->grouped
                                    ? base.data : sem->value,
                                "explicit-input-reading", SIZE_MAX);
done:
    ttf_buffer_free(&base);
    ttf_buffer_free(&decoded);
    return status;
}
/* A declared particle written against the reading stays attached to it. */
static int following_particle(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                              const char *text, size_t n) {
    char key[32];
    size_t width = 0;
    while (width < n && width + 1 < sizeof(key)) {
        uint32_t cp;
        size_t step;
        if (ttf_utf8_decode(text + width, n - width, &cp, &step) != 1 ||
            ttf_script_of(cp) != TTF_SCRIPT_HANGUL || width + step + 1 >= sizeof(key))
            break;
        width += step;
    }
    if (!width)
        return 0;
    memcpy(key, text, width);
    key[width] = 0;
    return ttf_resource_get(e, "particle", renderer->language_tag, key, NULL) != NULL;
}

/* Korean-style particle agreement: a written particle that stands alone after a
   normalized reading takes the form the spoken text requires. The alternation
   table and forms are resources, so the mechanism stays language independent. */
static ttf_status_t particle_agreement(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                                       const char *source, size_t source_length,
                                       const ttf_buffer_t *output, ttf_buffer_t *normalized) {
    char key[32], forms[96], *vowel, *rieul;
    const char *value;
    uint32_t cp;
    size_t width, start = output->length;
    int final;
    /* Cheap shape tests first: a particle is a short non-ASCII span left as
       written, and it can only agree after a syllable of the same script. */
    if (!source_length || source_length >= sizeof(key) || !output->length ||
        !((unsigned char)source[0] & 0x80))
        return TTF_OK;
    /* Off unless the language policy turns it on (2026-10-02, project-owner
       decision): the written text is explicit, and a following word is not
       always a particle (성수동1가 keeps 가, not 이). */
    if (e->particle_option == 1)
        return TTF_OK;
    if (e->particle_option != 2) {
        const char *policy = ttf_resource_get(e, "policy", renderer->language_tag, "particle_agreement", NULL);
        if (!policy || strcmp(policy, "on"))
            return TTF_OK;
    }
    if (normalized->length != source_length || memcmp(normalized->data, source, source_length))
        return TTF_OK;
    while (start && ((unsigned char)output->data[start - 1] & 0xc0) == 0x80)
        --start;
    if (!start ||
        ttf_utf8_decode(output->data + start - 1, output->length - start + 1, &cp, &width) != 1 ||
        cp < 0xac00 || cp > 0xd7a3)
        return TTF_OK;
    final = (int)((cp - 0xac00) % 28);
    memcpy(key, source, source_length);
    key[source_length] = 0;
    value = ttf_resource_get(e, "particle_form", renderer->language_tag, key, NULL);

    if (!value || strlen(value) >= sizeof(forms))
        return TTF_OK;
    memcpy(forms, value, strlen(value) + 1);
    vowel = strchr(forms, '|');
    if (!vowel)
        return TTF_OK;
    *vowel++ = 0;
    rieul = strchr(vowel, '|');
    if (rieul)
        *rieul++ = 0;
    value = !final ? vowel : final == 8 && rieul ? rieul : forms;
    if (!strcmp(value, key))
        return TTF_OK;
    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, source_length);
    {
        ttf_status_t status = ttf_trace_resource(e, "particle_form", renderer->language_tag, key,
                                                 value, "particle-agreement", SIZE_MAX);
        if (status != TTF_OK)
            return status;
    }
    normalized->length = 0;
    if (normalized->data)
        normalized->data[0] = 0;
    return ttf_buffer_puts(normalized, value);
}

/* The final consonant (jongseong index, 0 none, 8 ㄹ) of the last syllable of
   b, or -1 when it does not end in a Hangul syllable. */
static int last_final(const char *data, size_t length) {
    size_t start = length;
    uint32_t cp;
    size_t width;
    while (start && ((unsigned char)data[start - 1] & 0xc0) == 0x80)
        --start;
    if (!start || ttf_utf8_decode(data + start - 1, length - start + 1, &cp, &width) != 1 ||
        cp < 0xac00 || cp > 0xd7a3)
        return -1;
    return (int)((cp - 0xac00) % 28);
}
/* Whether a reading ending in final fits the written particle key, from its
   particle_form "consonant|vowel[|rieul]" entry; -1 when key is no particle. */
static int particle_fits(const ttf_engine_t *e, const char *lang, const char *key, int final) {
    char forms[96], *vowel, *rieul;
    const char *value = ttf_resource_get(e, "particle_form", lang, key, NULL);
    if (!value || strlen(value) >= sizeof(forms))
        return -1;
    memcpy(forms, value, strlen(value) + 1);
    vowel = strchr(forms, '|');
    if (!vowel)
        return -1;
    *vowel++ = 0;
    rieul = strchr(vowel, '|');
    if (rieul)
        *rieul++ = 0;
    value = !final ? vowel : final == 8 && rieul ? rieul : forms;
    return !strcmp(value, key);
}
/* The writer's particle tells how they read the word before it: L와 is 라지와
   (엘 would take 과), 5281를 ends in 하나 and 6422을 in 둘 (the Korean references
   read codes so), TIME이 is 타임이. When the reading before a directly attached
   particle disagrees with it, an alternative that agrees replaces the end of
   that reading: particle_reading "alternative|scope" for the last word (scope
   digits: a digit sequence or phone number; or the exact written letter), or the
   lexicon word for a spelled all-letter span. The particle stays as written
   (project-owner idea, 2026-10-08). */
static ttf_status_t particle_reading(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                                     const char *text, size_t offset, const char *source,
                                     size_t source_length, ttf_buffer_t *output, ttf_result_t *out) {
    char key[32], word[64], alt[96];
    const char *lang = renderer->language_tag, *value, *surface;
    ttf_span_t *previous;
    size_t width, surface_length, i;
    int final;
    if (e->frontend_options.output_mode != TTF_OUTPUT_SPEECH || strcmp(lang, "ko-KR") ||
        !source_length || source_length >= sizeof(key) || !out->span_count || !output->length)
        return TTF_OK;
    memcpy(key, source, source_length);
    key[source_length] = 0;
    final = last_final(output->data, output->length);
    if (final < 0 || particle_fits(e, lang, key, final) != 0)
        return TTF_OK;
    previous = &out->spans[out->span_count - 1];
    if (previous->end != output->length || previous->input_begin < offset)
        return TTF_OK;
    surface = text + (previous->input_begin - offset);
    surface_length = previous->input_end - previous->input_begin;
    /* A spelled all-letter span with a lexicon word that fits (TIME이 타임이). */
    if (surface_length >= 2 && surface_length + 7 < sizeof(word)) {
        /* Only an all-capital run the engine itself chose to spell (an inferred
           initialism); an explicit policy (Apple spelled on request) stays. */
        int letters = previous->semantic.type == TTF_SEM_INITIALISM;
        for (i = 0; i < surface_length; ++i)
            letters &= surface[i] >= 'A' && surface[i] <= 'Z';
        if (letters) {
            (void)snprintf(word, sizeof(word), "en-US:");
            for (i = 0; i < surface_length; ++i)
                word[6 + i] = (char)(surface[i] >= 'A' && surface[i] <= 'Z' ? surface[i] + 32 : surface[i]);
            word[6 + surface_length] = 0;
            value = ttf_resource_get(e, "lexicon", lang, word, NULL);
            if (value && strcmp(value, output->data + previous->begin) &&
                particle_fits(e, lang, key, last_final(value, strlen(value))) == 1) {
                output->length = previous->begin;
                output->data[output->length] = 0;
                ttf_status_t st = ttf_buffer_puts(output, value);
                previous->end = output->length;
                if (st == TTF_OK)
                    st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, lang, "decision",
                                        "particle-reading", word, value, "particle-chooses-word", SIZE_MAX);
                return st;
            }
        }
    }
    /* 디스트릭트9을 디스트릭트나인을, 아이언맨3를 아이언맨쓰리를: a number up to ten
       written right after a word (a title) is read with the English numeral, as after a
       listed product name (아이폰7 아이폰세븐), when only that reading fits the particle
       (2026-10-10). A number after a space (숫자 9을) may be a plain number and stays;
       가 and 로 may be the address units 街 and 路 (종로3가, 무명3로) and choose nothing. */
    if (previous->semantic.type == TTF_SEM_CARDINAL && surface_length && surface_length <= 2 &&
        (surface_length == 1 || surface[0] != '0') && previous->input_begin - offset >= 1 &&
        strcmp(key, "가") && strcmp(key, "로")) {
        static const char *const names[] = {"zero", "one", "two", "three", "four", "five",
                                            "six",  "seven", "eight", "nine", "ten"};
        size_t at = previous->input_begin - offset;
        unsigned number = 0;
        int digits = 1, attached;
        uint32_t cp = 0;
        size_t w;
        for (i = 0; i < surface_length; ++i) {
            digits &= surface[i] >= '0' && surface[i] <= '9';
            number = number * 10 + (unsigned)(surface[i] - '0');
        }
        attached = (text[at - 1] >= 'A' && text[at - 1] <= 'Z') || (text[at - 1] >= 'a' && text[at - 1] <= 'z') ||
                   (at >= 3 && ttf_utf8_decode(text + at - 3, 3, &cp, &w) == 1 && cp >= 0xac00 && cp <= 0xd7a3);
        if (digits && attached && number <= 10) {
            (void)snprintf(word, sizeof(word), "en-US:%s", names[number]);
            value = ttf_resource_get(e, "lexicon", lang, word, NULL);
            if (value && strcmp(value, output->data + previous->begin) &&
                particle_fits(e, lang, key, last_final(value, strlen(value))) == 1) {
                ttf_status_t st;
                output->length = previous->begin;
                output->data[output->length] = 0;
                st = ttf_buffer_puts(output, value);
                previous->end = output->length;
                if (st == TTF_OK)
                    st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, lang, "decision",
                                        "particle-reading", word, value, "particle-chooses-numeral", SIZE_MAX);
                return st;
            }
        }
    }
    /* The last word of the reading: one to three syllables. */
    for (width = 3; width <= 9 && width <= output->length - previous->begin; width += 3) {
        char *bar;
        memcpy(word, output->data + output->length - width, width);
        word[width] = 0;
        value = ttf_resource_get(e, "particle_reading", lang, word, NULL);
        if (!value || strlen(value) >= sizeof(alt))
            continue;
        memcpy(alt, value, strlen(value) + 1);
        bar = strchr(alt, '|');
        if (!bar)
            continue;
        *bar++ = 0;
        if (!strcmp(bar, "digits")
                ? !((previous->semantic.type == TTF_SEM_DIGIT_SEQUENCE &&
                     strcmp(previous->semantic.unit, "clothing_size")) ||
                    previous->semantic.type == TTF_SEM_PHONE)
                : !(strlen(bar) == surface_length && !memcmp(bar, surface, surface_length)))
            continue;
        if (particle_fits(e, lang, key, last_final(alt, strlen(alt))) != 1)
            continue;
        output->length -= width;
        output->data[output->length] = 0;
        {
            ttf_status_t st = ttf_buffer_puts(output, alt);
            previous->end = output->length;
            if (st == TTF_OK)
                st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, lang, "decision",
                                    "particle-reading", word, alt, "particle-chooses-reading", SIZE_MAX);
            return st;
        }
    }
    return TTF_OK;
}

/* Declared bracket pairs mark an aside. The policy decides whether its content
   is read, left out of the reading, or kept exactly as written. */
static uint32_t bracket_closer(const ttf_engine_t *e, const char *text, size_t length) {
    char key[8];
    uint32_t cp, closer;
    size_t width;
    const char *value;
    if (ttf_utf8_decode(text, length, &cp, &width) != 1 || width >= sizeof(key))
        return 0;
    memcpy(key, text, width);
    key[width] = 0;
    value = ttf_resource_get(e, "bracket_pair", "*", key, NULL);
    if (!value || ttf_utf8_decode(value, strlen(value), &closer, &width) != 1)
        return 0;
    return closer;
}

static size_t bracket_region(const ttf_engine_t *e, const char *text, size_t length,
                             const ttf_token_t *tokens, size_t count, size_t index) {
    uint32_t closer = bracket_closer(e, text + tokens[index].begin, length - tokens[index].begin);
    size_t i, depth = 1;
    if (!closer || tokens[index].end != tokens[index].begin + 1)
        return 0;
    for (i = index + 1; i < count; ++i) {
        uint32_t cp;
        size_t width;
        if (ttf_utf8_decode(text + tokens[i].begin, length - tokens[i].begin, &cp, &width) != 1)
            continue;
        if (cp == closer && !--depth)
            return i;
        if (bracket_closer(e, text + tokens[i].begin, length - tokens[i].begin) == closer)
            ++depth;
    }
    return 0;
}

/* An invalid control (45821{@digit}: unknown name, bad value) is ignored: the
   unit is read again with only that control's markup removed, as if it were
   never written, and spans and trace map back to the input. Dropped markup
   joins the span before it (the next one at the start of the unit). */
static ttf_status_t ignore_control(const ttf_engine_t *e, const char *text, size_t length,
                                   size_t offset, const ttf_control_match_t *control,
                                   size_t mark, ttf_result_t *out) {
    size_t *map = (size_t *)malloc((length + 1) * sizeof(*map)), used = 0, i, retry;
    char *copy = (char *)malloc(length + 1), name[TTF_IR_VALUE_SIZE];
    ttf_status_t st = TTF_ENOMEM;
    if (!map || !copy)
        goto done;
    ttf_trace_reject_since(e, mark);
    for (i = 0; i < length; ++i)
        if (!(i >= control->begin && i < control->base_begin) &&
            !(i >= control->base_end && i < control->end)) {
            copy[used] = text[i];
            map[used++] = i;
        }
    map[used] = length;
    retry = ttf_trace_mark(e);
    st = ttf_process_unit(e, copy, used, 0, out);
    if (st != TTF_OK)
        goto done;
    ttf_trace_remap_input(e, retry, map, used, length, offset);
    for (i = 0; i < out->span_count; ++i) {
        ttf_span_t *span = &out->spans[i];
        size_t b = span->input_begin, end = span->input_end;
        span->input_begin = offset + (!b ? 0 : b < used ? map[b] : length);
        span->input_end = offset + (end < used ? map[end] : length);
        span->semantic.input_begin = span->input_begin;
        span->semantic.input_end = span->input_end;
    }
    i = control->payload_end - control->payload_begin;
    if (i >= sizeof(name))
        i = sizeof(name) - 1;
    memcpy(name, text + control->payload_begin, i);
    name[i] = 0;
    ttf_trace_span_begin(e, offset + control->begin, offset + control->end);
    st = ttf_trace_emit(e, TTF_TRACE_REJECTED, TTF_LAYER_USER, 0, "und", "input-control", name, "",
                        "", "ignored-invalid-control", SIZE_MAX);
    for (i = 0; i < out->span_count; ++i)
        if (out->spans[i].input_end > offset + control->begin || i + 1 == out->span_count)
            break;
    ttf_trace_span_finish(e, i < out->span_count ? out->spans[i].begin : 0,
                          i < out->span_count ? out->spans[i].end : 0);
    if (st != TTF_OK)
        ttf_result_free(out);
done:
    free(map);
    free(copy);
    return st;
}
ttf_status_t ttf_process_unit(const ttf_engine_t *e, const char *text, size_t length, size_t offset,
                              ttf_result_t *out) {
    return ttf_process_unit_context(e, NULL, 0, text, length, offset, out);
}
/* A segment cut leaves the words before a number in the previous unit (born.
   1880., ( b . 1952 )). The classifiers already read a bounded window before a
   token (64 bytes), so they receive the previous segment's tail in front of the
   unit, with token offsets moved past it; what they return is moved back. Only
   classification sees it: routing, rendering and spans are unchanged, and a
   stream decides like a batch because both cut the same units (2026-10-07). */
ttf_status_t ttf_process_unit_context(const ttf_engine_t *e, const char *prefix,
                                      size_t context_length, const char *text, size_t length,
                                      size_t offset, ttf_result_t *out) {
    const ttf_language_pack_t *sentence_lang = NULL;
    char *context_text = NULL;
    const char *context_base = NULL;
    ttf_token_t *context_tokens = NULL;
    ttf_context_memo_t memo = {0};
    ttf_numeric_width_t width_view = {0};
    ttf_engine_t context_view = *e;
    unsigned context_invalidations = 0;
    ttf_token_t *tokens = NULL;
    context_scope_t *scopes = NULL;
    ttf_protected_region_t *regions = NULL;
    size_t region_count = 0, region_index = 0;
    size_t count = 0, i = 0, capacity = 0;
    int removed_before = 0;
    ttf_buffer_t output = {0};
    ttf_status_t st;
    ttf_control_match_t rejected = {0};
    size_t trace_mark = ttf_trace_mark(e);
    size_t *phoneme_regions = NULL, phoneme_count = 0, phoneme_cap = 0;
    int ignore = 0;
    context_view.context_memo = &memo;
    memset(out, 0, sizeof(*out));
    st = ttf_tokenize_protected(e, text, length, 1, &tokens, &count, &regions, &region_count);
    if (st != TTF_OK)
        return st;
    scopes = quote_scopes(text, length, tokens, count);
    if (count && !scopes) {
        free(tokens);
        free(regions);
        return TTF_ENOMEM;
    }
    st = ttf_numeric_width_create(text, length, tokens, count, regions, region_count, &width_view);
    if (st != TTF_OK)
        goto fail;
    while (i < count) {
        ttf_pack_input_t in;
        ttf_span_t span;
        ttf_semantic_t *sem = &span.semantic;
        size_t consumed = 0, j, end, matched = 0, semantic_limit = length, phrase_count = count;
        long classifier_priority = 0;
        const ttf_language_pack_t *source, *context, *renderer;
        const char *reading = NULL, *render_text;
        size_t render_length;
        const ttf_entry_t *rule = NULL;
        const char *phrase_hit = NULL;
        ttf_buffer_t normalized = {0};
        int identifier_read = 0;
        int han_context = 0;
        memset(&span, 0, sizeof(span));
        sem->struct_size = sizeof(*sem);
        sem->type = TTF_SEM_TEXT;
        sem->layer = TTF_LAYER_COMMON;
        /* - 사과, • 항목: a list marker (list_marker) at the start of a line followed
           by a space is not read in speech (it was 하이픈 or 마이너스). -3도 with
           no space stays a negative number. */
        if (e->frontend_options.output_mode == TTF_OUTPUT_SPEECH && i + 2 < count &&
            tokens[i].end - tokens[i].begin < 8 &&
            tokens[i + 1].script == TTF_SCRIPT_WHITESPACE) {
            size_t back = tokens[i].begin, w = tokens[i].end - tokens[i].begin;
            char marker[8];
            int at_line_start;
            while (back && (text[back - 1] == ' ' || text[back - 1] == '\t'))
                --back;
            at_line_start = !back || text[back - 1] == '\n' || text[back - 1] == '\r';
            memcpy(marker, text + tokens[i].begin, w);
            marker[w] = '\0';
            if (at_line_start && text[tokens[i + 1].begin] != '\n' &&
                text[tokens[i + 1].begin] != '\r' &&
                ttf_resource_get(e, "list_marker", "*", marker, NULL)) {
                size_t skip_end = tokens[i + 1].end;
                for (size_t k = tokens[i + 1].begin; k < tokens[i + 1].end; ++k)
                    if (text[k] == '\n' || text[k] == '\r') {
                        skip_end = k;
                        break;
                    }
                span.input_begin = sem->input_begin = tokens[i].begin + offset;
                span.input_end = sem->input_end = skip_end + offset;
                snprintf(sem->rule, sizeof(sem->rule), "syntax.list-marker");
                snprintf(sem->unit, sizeof(sem->unit), "@list-marker");
                tag(sem->source_lang, "und");
                tag(sem->context_lang, "und");
                tag(sem->render_lang, "und");
                tag(span.verbalizer, "identity");
                span.script = TTF_SCRIPT_PUNCTUATION;
                span.begin = span.end = output.length;
                ttf_trace_span_begin(e, span.input_begin, span.input_end);
                ttf_trace_stage(e, TTF_TRACE_POLICY, 0, skip_end - tokens[i].begin);
                st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, "und", "decision",
                                    "list-marker", marker, "", "list-marker-skipped", SIZE_MAX);
                ttf_trace_span_finish(e, span.begin, span.end);
                if (st == TTF_OK)
                    st = add_span(out, &span, &capacity);
                if (st != TTF_OK)
                    goto fail;
                removed_before = 0;
                if (skip_end == tokens[i + 1].end)
                    i += 2;
                else {
                    tokens[i + 1].begin = skip_end;
                    i += 1;
                }
                continue;
            }
        }
        if (e->bracket_policy != TTF_BRACKET_READ) {
            size_t close = bracket_region(e, text, length, tokens, count, i);
            if (close) {
                size_t end_offset = tokens[close].end;
                span.input_begin = tokens[i].begin + offset;
                span.input_end = end_offset + offset;
                sem->input_begin = span.input_begin;
                sem->input_end = span.input_end;
                snprintf(sem->rule, sizeof(sem->rule), "%s",
                         e->bracket_policy == TTF_BRACKET_SKIP ? "policy.bracket-skipped"
                                                               : "policy.bracket-preserved");
                snprintf(sem->unit, sizeof(sem->unit), "@bracket");
                tag(sem->source_lang, "und");
                tag(sem->context_lang, "und");
                tag(sem->render_lang, "und");
                tag(span.verbalizer, "identity");
                span.script = TTF_SCRIPT_PUNCTUATION;
                span.begin = output.length;
                ttf_trace_span_begin(e, span.input_begin, span.input_end);
                ttf_trace_stage(e, TTF_TRACE_POLICY, 0, end_offset - tokens[i].begin);
                st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, "und", "decision",
                                    "bracket", "", sem->rule, sem->rule, SIZE_MAX);
                if (st == TTF_OK && e->bracket_policy == TTF_BRACKET_PRESERVE)
                    st = ttf_buffer_append(&output, text + tokens[i].begin,
                                           end_offset - tokens[i].begin);
                span.end = output.length;
                ttf_trace_span_finish(e, span.begin, span.end);
                if (st == TTF_OK)
                    st = add_span(out, &span, &capacity);
                if (st != TTF_OK)
                    goto fail;
                removed_before = e->bracket_policy == TTF_BRACKET_SKIP;
                while (region_index < region_count && regions[region_index].begin < end_offset)
                    ++region_index;
                i = close + 1;
                continue;
            }
        }
        if (region_index < region_count && regions[region_index].begin == tokens[i].begin) {
            const ttf_protected_match_t *literal = &regions[region_index++].match;
            /* [[group]]{@pronounce} and, since 2026-10-10, X{@pronounce} on one atom. */
            if (literal->control.kind == TTF_CONTROL_PATTERN &&
                literal->control.payload_end - literal->control.payload_begin == 10 &&
                !memcmp(text + literal->control.payload_begin, "@pronounce", 10)) {
                st = phoneme_control(e, text, length, &literal->control, offset, &output, out, &capacity,
                                     &phoneme_regions, &phoneme_count, &phoneme_cap);
                if (st != TTF_OK)
                    goto fail;
                removed_before = 0;
                ++i;
                continue;
            }
            if (literal->control.kind != TTF_CONTROL_NONE) {
                ttf_pack_input_t control_input = {text, length, tokens, count, i, e};
                if (scopes[i].end) {
                    control_input.tokens = tokens + scopes[i].begin;
                    control_input.token_count = scopes[i].end - scopes[i].begin;
                    control_input.index -= scopes[i].begin;
                    snprintf(span.context_trace, sizeof(span.context_trace), "quotation");
                }
                span.begin = output.length;
                st = render_control(e, &control_input, &literal->control, offset, &span, &output);
                span.end = output.length;
                ttf_trace_span_finish(e, span.begin, span.end);
                if ((st == TTF_EINVAL || st == TTF_ELIMIT) && !e->strict_controls) {
                    rejected = literal->control;
                    ignore = 1;
                }
                if (st == TTF_OK)
                    st = add_span(out, &span, &capacity);
                if (st != TTF_OK)
                    goto fail;
                removed_before = 0;
                ++i;
                continue;
            }
            sem->input_begin = span.input_begin = tokens[i].begin + offset;
            sem->input_end = span.input_end = literal->end + offset;
            sem->layer = literal->layer;
            snprintf(sem->unit, sizeof(sem->unit), "%s", literal->kind);
            snprintf(sem->rule, sizeof(sem->rule), "%s", literal->rule);
            tag(sem->source_lang, "und");
            tag(sem->context_lang, "und");
            tag(sem->render_lang, "und");
            tag(span.verbalizer, "identity");
            snprintf(span.context_trace, sizeof(span.context_trace), "protected:excluded");
            span.script = TTF_SCRIPT_SYMBOL;
            span.begin = output.length;
            st = ttf_buffer_append(&output, text + tokens[i].begin, literal->end - tokens[i].begin);
            span.end = output.length;
            if (st == TTF_OK)
                st = add_span(out, &span, &capacity);
            if (st != TTF_OK)
                goto fail;
            removed_before = 0;
            ++i;
            continue;
        }
        in.text = text;
        in.length = length;
        in.tokens = tokens;
        in.token_count = count;
        in.index = i;
        in.engine = e;
        /* Only context callbacks receive this borrowed request view. Other pack
           callbacks retain their original engine and the public ABI is unchanged. */
        if (scopes[i].end) {
            ttf_pack_input_t scoped = in;
            scoped.engine = &context_view;
            scoped.tokens = tokens + scopes[i].begin;
            scoped.token_count = scopes[i].end - scopes[i].begin;
            scoped.index -= scopes[i].begin;
            snprintf(span.context_trace, sizeof(span.context_trace), "quotation");
            context = context_pack(e, &scoped, span.context_trace);
            sentence_lang = ttf_sentence_language(&scoped);
        } else {
            ttf_pack_input_t scoped = in;
            scoped.engine = &context_view;
            context = context_pack(e, &scoped, span.context_trace);
            sentence_lang = ttf_sentence_language(&scoped);
        }
        if (memo.status != TTF_OK) {
            st = memo.status;
            goto fail;
        }
        source = script_pack(e, tokens[i].script);
        if (!context) {
            st = TTF_EINVAL;
            goto fail;
        }
        if (tokens[i].script != TTF_SCRIPT_WHITESPACE) {
            ttf_pack_input_t classification = in;
            if (region_index < region_count) {
                classification.length = regions[region_index].begin;
                classification.token_count = i;
                while (classification.token_count < count &&
                       tokens[classification.token_count].begin < classification.length)
                    ++classification.token_count;
            }
            semantic_limit = classification.length;
            phrase_count = classification.token_count;
            if (width_view.text) {
                width_view.tokens[i].begin = width_view.scan[tokens[i].begin];
                classification.text = width_view.text;
                classification.length = width_view.scan[classification.length];
                classification.tokens = width_view.tokens;
            }
            if (context_length) {
                const char *base = classification.text;
                size_t base_length = width_view.text ? width_view.length : length, k;
                if (context_base != base) {
                    char *grown = (char *)realloc(context_text, context_length + base_length + 1);
                    if (!grown) {
                        st = TTF_ENOMEM;
                        goto fail;
                    }
                    context_text = grown;
                    memcpy(context_text, prefix, context_length);
                    memcpy(context_text + context_length, base, base_length);
                    context_text[context_length + base_length] = '\0';
                    context_base = base;
                }
                if (!context_tokens &&
                    !(context_tokens = (ttf_token_t *)malloc(count * sizeof(*context_tokens)))) {
                    st = TTF_ENOMEM;
                    goto fail;
                }
                for (k = 0; k < count; ++k) {
                    context_tokens[k] = classification.tokens[k];
                    context_tokens[k].begin += context_length;
                    context_tokens[k].end += context_length;
                }
                classification.text = context_text;
                classification.length += context_length;
                classification.tokens = context_tokens;
            }
            consumed = ttf_classify_universal(&classification, sem);
            /* Looked up once per token, not once per pack. */
            han_context = context && context->classify &&
                          pack_flag(e, context, "han_sentence", 0);
            for (j = 0; j < e->pack_count; j++)
                /* A declared-only pack classifies only where it is the context
                   (its declared request or a kana sentence). A hanzi-sentence
                   pack (zh-CN) is the only pack classifying where it is the
                   context, maths included: 1+1=2 in a Korean request is not
                   일 더하기 일. */
                if (e->packs[j]->classify &&
                    (strcmp(sem->unit, "@math") || (han_context && context == e->packs[j])) &&
                    (!undeclared(e, j) || context == e->packs[j]) &&
                    !(han_context && context != e->packs[j]) &&
                    /* ja-JP in its own request leaves an English or Korean
                       sentence alone (AI in an English sentence is not エーアイ). */
                    !(context != e->packs[j] && sentence_lang && sentence_lang != e->packs[j] &&
                      declared_only(e, j)) &&
                    !(context != e->packs[j] &&
                      (e->frozen ? e->han_only[j] : pack_flag(e, e->packs[j], "han_sentence", 0)))) {
                    ttf_semantic_t candidate;
                    size_t n;
                    long candidate_priority = 0;
                    memset(&candidate, 0, sizeof(candidate));
                    candidate.struct_size = sizeof(candidate);
                    candidate.layer = TTF_LAYER_SYSTEM;
                    n = e->packs[j]->classify(&classification, &candidate);
                    if (n > classification.token_count - i ||
                        (n && candidate.input_end > classification.length)) {
                        st = TTF_ERESOURCE;
                        goto fail;
                    }
                    if (n) {
                        const char *priority =
                            ttf_resource_get(e, "classifier_priority", e->packs[j]->language_tag,
                                             candidate.rule, NULL);
                        if (priority) {
                            char *end_priority;
                            candidate_priority = strtol(priority, &end_priority, 10);
                            if (!*priority || *end_priority || candidate_priority < -1000 ||
                                candidate_priority > 1000) {
                                st = TTF_ERESOURCE;
                                goto fail;
                            }
                        }
                    }
                    /* An explicit pack identifier cue is more specific than
                       generic numeric syntax over the identical byte extent. */
                    int labeled_identifier =
                        n && n == consumed && candidate.input_end == sem->input_end &&
                        candidate.type == TTF_SEM_DIGIT_SEQUENCE &&
                        (sem->type == TTF_SEM_CARDINAL || sem->type == TTF_SEM_NUMBER ||
                         sem->type == TTF_SEM_PHONE || sem->type == TTF_SEM_RANGE ||
                         sem->type == TTF_SEM_DATE || sem->type == TTF_SEM_TEXT ||
                         /* 항공 번호 63107508번: a labelled code with its counter. */
                         sem->type == TTF_SEM_COUNTER ||
                         /* A leading zero already reads as digits; a declared label
                            still decides how, so 001204 after 참조 번호 keeps 공. */
                         sem->type == TTF_SEM_DIGIT_SEQUENCE);
                    /* A pack may establish a calendar meaning using explicit
                       context. A bare generic cardinal has no such evidence. */
                    int contextual_date =
                        n && n == consumed && candidate.input_end == sem->input_end &&
                        candidate.type == TTF_SEM_DATE &&
                        (sem->type == TTF_SEM_CARDINAL || sem->type == TTF_SEM_NUMBER);
                    /* A pack rule declared with priority 100 or more replaces the
                       generic numeric grammar even where that grammar also took a
                       following counter or unit (27022-8080 통장, 2020-1234호): the
                       pack's shorter object is kept and the word after it is
                       classified on its own. */
                    /* ...but a recognized phone number, or an identifier the shared
                       grammar already rejected as a range or phone, stays as it was. */
                    /* A hanzi-sentence pack (zh-CN) owns its sentence: the shared
                       guards do not hold its spans (120-140bpm, 11-08). */
                    int keeps_shared =
                        candidate_priority >= 100 && !(e->frozen && e->han_only[j]) &&
                        (sem->type == TTF_SEM_PHONE ||
                         (sem->type == TTF_SEM_TEXT && (!strcmp(sem->rule, "syntax.invalid-range") ||
                                                        !strcmp(sem->rule, "syntax.invalid-phone"))));
                    int replacing_numeric =
                        !keeps_shared && n && n < consumed && candidate_priority >= 100 &&
                        candidate.input_end < sem->input_end &&
                        (sem->type == TTF_SEM_RANGE || sem->type == TTF_SEM_CARDINAL ||
                         sem->type == TTF_SEM_NUMBER || sem->type == TTF_SEM_COUNTER ||
                         sem->type == TTF_SEM_MEASURE || sem->type == TTF_SEM_CURRENCY ||
                         sem->type == TTF_SEM_TEXT);
                    if (keeps_shared && n <= consumed)
                        continue;
                    if (n > consumed ||
                        (n && n == consumed &&
                         (candidate.input_end > sem->input_end ||
                          (candidate.input_end == sem->input_end &&
                           candidate_priority > classifier_priority))) ||
                        (n && n == consumed && sem->type == TTF_SEM_WORD) || labeled_identifier ||
                        contextual_date || replacing_numeric) {
                        *sem = candidate;
                        consumed = n;
                        classifier_priority = candidate_priority;
                    }
                }
            /* In a hanzi sentence ， is a clause comma, never part of a number
               (验证码是846201，5分钟): the full-width guard does not join across it. */
            {
                size_t comma = consumed ? sem->input_end : classification.tokens[i].begin;
                if (!(han_context && comma + 3 <= classification.length &&
                      !memcmp(classification.text + comma, "，", 3)))
                    consumed = ttf_numeric_width_guard(&classification, sem, consumed);
            }
            if (context_length) {
                if (consumed)
                    sem->input_end -= context_length;
                classification.length -= context_length;
            }
            if (width_view.text && consumed) {
                if (sem->input_end > classification.length) {
                    st = TTF_ERESOURCE;
                    goto fail;
                }
                sem->input_end = width_view.raw[sem->input_end];
            }
            rule = ttf_rule_match(e, context->language_tag, text + tokens[i].begin,
                                  semantic_limit - tokens[i].begin, &matched);
        }
        if (rule) {
            sem->type = TTF_SEM_CUSTOM;
            sem->layer = rule->layer;
            snprintf(sem->rule, sizeof(sem->rule), "%s:%.48s", rule->kind, rule->key);
            sem->input_end = tokens[i].begin + matched;
            consumed = 0;
            while (i + consumed < count && tokens[i + consumed].begin < sem->input_end)
                consumed++;
        }
        if (!consumed) {
            consumed = 1;
            sem->type =
                (tokens[i].script == TTF_SCRIPT_LATIN || tokens[i].script == TTF_SCRIPT_HANGUL ||
                 ttf_script_legacy(tokens[i].script) == TTF_SCRIPT_OTHER)
                    ? TTF_SEM_WORD
                    : TTF_SEM_TEXT;
            if (tokens[i].script == TTF_SCRIPT_SYMBOL)
                sem->type = TTF_SEM_SYMBOL;
            sem->lexical_class =
                tokens[i].script == TTF_SCRIPT_LATIN ? TTF_LEX_UNKNOWN_LATIN : TTF_LEX_NONE;
            snprintf(sem->rule, sizeof(sem->rule), "identity");
        }
        if (!rule && (consumed == 1 || !strcmp(sem->rule, "pack.hyphenated-word")) && source &&
            source != context &&
            e->config.render_policy == TTF_PHONETIC_LOCALIZE &&
            (sem->type == TTF_SEM_WORD || sem->type == TTF_SEM_INITIALISM ||
             sem->type == TTF_SEM_ACRONYM)) {
            size_t phrase_consumed = 0;
            char phrase_value[TTF_IR_VALUE_SIZE];
            ttf_resource_layer_t phrase_layer = TTF_LAYER_COMMON;
            phrase_hit =
                ttf_phrase_match(e, source->language_tag, context->language_tag, text, tokens,
                                 phrase_count, i, &phrase_consumed, phrase_value, &phrase_layer);
            if (phrase_hit && phrase_consumed < consumed)
                phrase_hit = NULL;
            if (phrase_hit) {
                sem->type = TTF_SEM_WORD;
                sem->lexical_class = TTF_LEX_WORD;
                sem->layer = phrase_layer;
                sem->input_end = tokens[i + phrase_consumed - 1].end;
                memcpy(sem->value, phrase_value, strlen(phrase_value) + 1);
                snprintf(sem->rule, sizeof(sem->rule), "lexicon.phrase");
                consumed = phrase_consumed;
                snprintf(span.lexicon, sizeof(span.lexicon), "lexicon:%s",
                         ttf_layer_name(phrase_layer));
                span.pronunciation_layer = phrase_layer;
            }
        }
        if (consumed > count - i) {
            st = TTF_ERESOURCE;
            goto fail;
        }
        end = sem->input_end ? sem->input_end : tokens[i + consumed - 1].end;
        if (end <= tokens[i].begin || end > tokens[i + consumed - 1].end || end > semantic_limit) {
            st = TTF_ERESOURCE;
            goto fail;
        }
        if (!source)
            for (j = i; j < i + consumed; j++) {
                source = script_pack(e, tokens[j].script);
                if (source)
                    break;
            }
        /* An address or an expression's own letters are not language evidence. */
        if (!rule && ((e->frontend_options.output_mode == TTF_OUTPUT_SPEECH &&
                       (sem->type == TTF_SEM_EMAIL || sem->type == TTF_SEM_URL ||
                        !strcmp(sem->unit, "@handle"))) ||
                      (sem->type == TTF_SEM_SYMBOL && !strcmp(sem->unit, "@expression")))) {
            ttf_pack_input_t electronic_input = in;
            if (scopes[i].end && consumed <= scopes[i].end - i) {
                electronic_input.tokens = tokens + scopes[i].begin;
                electronic_input.token_count = scopes[i].end - scopes[i].begin;
                electronic_input.index -= scopes[i].begin;
            }
            context = electronic_context(e, &electronic_input, consumed, span.context_trace);
            if (!context) {
                st = TTF_ENOMEM;
                goto fail;
            }
        }
        /* Numeric units convey semantics; an attached unit does not change the
           source language of a neutral number. Ordinal suffixes do carry source. */
        if (sem->type != TTF_SEM_ORDINAL && !foreign_type(sem->type) && !rule)
            source = script_pack(e, tokens[i].script);
        if (sem->source_lang[0])
            source = ttf_find_pack(e, sem->source_lang);
        tag(sem->source_lang, source ? source->language_tag : "und");
        tag(sem->context_lang, context->language_tag);
        renderer = context;
        if (e->config.render_policy == TTF_PRESERVE_LANGUAGE && source)
            renderer = source;
        tag(sem->render_lang, renderer->language_tag);
        sem->input_begin = tokens[i].begin + offset;
        sem->input_end = end + offset;
        span.input_begin = sem->input_begin;
        span.input_end = sem->input_end;
        span.script = tokens[i].script;
        tag(span.verbalizer, renderer->language_tag);
        ttf_trace_span_begin(e, span.input_begin, span.input_end);
        render_text = text + tokens[i].begin;
        render_length = end - tokens[i].begin;
        if (width_view.text && !rule &&
            width_view.scan[end] - width_view.scan[tokens[i].begin] != render_length) {
            if (e->trace_state) {
                char *original = ttf_strndup(render_text, render_length);
                if (!original) { st = TTF_ENOMEM; goto fail; }
                ttf_trace_stage(e, TTF_TRACE_POLICY, 0, render_length);
                st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0,
                                    renderer->language_tag, "decision", "numeric-width",
                                    "U+FF10..FF19 -> ASCII", original,
                                    "scanner-view; subsequent part offsets use canonical input", SIZE_MAX);
                free(original);
                if (st != TTF_OK)
                    goto fail;
            }
            if (ttf_numeric_width_surface(sem->type) &&
                !(sem->type == TTF_SEM_ALNUM_TERM && source == context &&
                  e->frontend_options.output_mode == TTF_OUTPUT_TEXT)) {
                render_text = width_view.text + width_view.scan[tokens[i].begin];
                render_length = width_view.scan[end] - width_view.scan[tokens[i].begin];
            }
        }
        if (rule)
            st = ttf_buffer_puts(&normalized, rule->value);
        else if (phrase_hit) {
            tag(span.verbalizer, source->language_tag);
            ttf_trace_stage(e, TTF_TRACE_LEXICON, 0, strlen(sem->value));
            st = ttf_trace_foreign_lexicon(e, source->language_tag, context->language_tag,
                                           sem->value, strlen(sem->value), "multiword-lexicon");
            if (st == TTF_OK)
                st = ttf_buffer_puts(&normalized, phrase_hit);
        } else if (source && source != context && foreign_type(sem->type) &&
                   e->config.render_policy == TTF_PHONETIC_LOCALIZE) {
            ttf_buffer_t spoken = {0};
            const char *original_hit = NULL, *selected_text = NULL;
            size_t selected_length = 0;
            tag(span.verbalizer, source->language_tag);
            /* An abbreviation may share its final period with sentence
               punctuation. Keep punctuation as its own output span while
               still honoring the user's complete dotted spelling. */
            st = TTF_OK;
            if ((sem->type == TTF_SEM_INITIALISM || sem->type == TTF_SEM_ABBREVIATION) &&
                end < length && text[end] == '.')
                st = pronunciation_candidate(e, source->language_tag, context->language_tag,
                                             text + tokens[i].begin, end + 1 - tokens[i].begin,
                                             &original_hit, &span.pronunciation_layer,
                                             &selected_text, &selected_length);
            if (st == TTF_OK)
                st = pronunciation_candidate(e, source->language_tag, context->language_tag,
                                             text + tokens[i].begin, end - tokens[i].begin,
                                             &original_hit, &span.pronunciation_layer,
                                             &selected_text, &selected_length);
            if (st == TTF_OK && (!original_hit || span.pronunciation_layer < TTF_LAYER_USER)) {
                ttf_status_t verbalized = source->verbalize(e, sem, render_text, render_length, &spoken);
                if (verbalized == TTF_OK)
                    st = pronunciation_candidate(e, source->language_tag, context->language_tag,
                                                 spoken.data ? spoken.data : "", spoken.length,
                                                 &original_hit, &span.pronunciation_layer,
                                                 &selected_text, &selected_length);
                else if (!original_hit || verbalized == TTF_ENOMEM || verbalized == TTF_ELIMIT)
                    st = verbalized;
            }
            if (st == TTF_OK && original_hit) {
                snprintf(span.lexicon, sizeof(span.lexicon), "lexicon:%s",
                         ttf_layer_name(span.pronunciation_layer));
                ttf_trace_stage(e, TTF_TRACE_LEXICON, 0, selected_length);
                st = ttf_trace_foreign_lexicon(
                    e, source->language_tag, context->language_tag, selected_text, selected_length,
                    selected_text == spoken.data ? "verbalized-alias" : "surface-alias");
                if (st == TTF_OK)
                    st = ttf_buffer_puts(&normalized, original_hit);
            } else if (st == TTF_OK) {
                st = ttf_foreign_render(e, source, context, spoken.data ? spoken.data : "",
                                        spoken.length, sem->lexical_class, &normalized,
                                        span.lexicon, &span.pronunciation_layer);
            }
            ttf_buffer_free(&spoken);
            if (st == TTF_OK && !strcmp(span.lexicon, "preserved:no-renderer"))
                tag(sem->render_lang, source->language_tag);
        } else if ((sem->type == TTF_SEM_EMAIL || sem->type == TTF_SEM_URL ||
                    !strcmp(sem->unit, "@handle")) &&
                   e->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
            /* 03wlswk@test.com: an address that starts with digits takes the
               source language of its Latin letters, so test still reads 테스트. */
            const ttf_language_pack_t *address_source = source;
            if (!address_source)
                address_source = script_pack(e, TTF_SCRIPT_LATIN);
            st = ttf_electronic_render(e, address_source, renderer, text + tokens[i].begin,
                                       end - tokens[i].begin, &normalized);
        }
        else if ((sem->type == TTF_SEM_EMAIL || sem->type == TTF_SEM_URL ||
                  sem->type == TTF_SEM_VERSION || sem->type == TTF_SEM_IP_ADDRESS ||
                  !strcmp(sem->unit, "@number-sign") || !strcmp(sem->unit, "@handle")) &&
                 e->frontend_options.output_mode == TTF_OUTPUT_TEXT)
            st = ttf_buffer_append(&normalized, text + tokens[i].begin, end - tokens[i].begin);
        else if (sem->type == TTF_SEM_SYMBOL && !sem->unit[0] &&
                 (reading = ttf_speech_kana_lengthening(
                      e, renderer->language_tag, text, tokens[i].begin, text + tokens[i].begin,
                      end - tokens[i].begin, text + end, length - end))) {
            /* Text keeps the written mark; speech writes the long vowel. */
            char mark[8];
            size_t mark_length = end - tokens[i].begin;
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, mark_length);
            if (mark_length >= sizeof(mark))
                mark_length = sizeof(mark) - 1;
            memcpy(mark, text + tokens[i].begin, mark_length);
            mark[mark_length] = 0;
            st = ttf_trace_resource(e, "speech_kana_lengthening", renderer->language_tag, mark,
                                    reading, "kana-lengthening", SIZE_MAX);
            if (st == TTF_OK)
                st = e->frontend_options.output_mode == TTF_OUTPUT_TEXT
                         ? ttf_buffer_append(&normalized, text + tokens[i].begin,
                                             end - tokens[i].begin)
                         : ttf_buffer_puts(&normalized, reading);
        } else if ((sem->type == TTF_SEM_SYMBOL || sem->type == TTF_SEM_TEXT ||
                    sem->type == TTF_SEM_WORD) &&
                   !sem->unit[0] &&
                   (reading = ttf_speech_range_open(
                        e, renderer->language_tag, text, tokens[i].begin, text + tokens[i].begin,
                        end - tokens[i].begin, text + end, length - end))) {
            char mark[8];
            size_t mark_length = end - tokens[i].begin;
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, mark_length);
            if (mark_length >= sizeof(mark))
                mark_length = sizeof(mark) - 1;
            memcpy(mark, text + tokens[i].begin, mark_length);
            mark[mark_length] = 0;
            st = ttf_trace_resource(e, "speech_range_open", renderer->language_tag, mark, reading,
                                    "open-range-reading", SIZE_MAX);
            if (st == TTF_OK)
                st = e->frontend_options.output_mode == TTF_OUTPUT_TEXT
                         ? ttf_buffer_append(&normalized, text + tokens[i].begin,
                                             end - tokens[i].begin)
                         : ttf_buffer_puts(&normalized, reading);
        } else if ((sem->type == TTF_SEM_SYMBOL || sem->type == TTF_SEM_TEXT ||
                  sem->type == TTF_SEM_WORD) &&
                 !sem->unit[0] &&
                 (reading = ttf_speech_range_mark(
                      e, renderer->language_tag, output.data, output.length, text + tokens[i].begin,
                      end - tokens[i].begin, text + end, length - end))) {
            char mark[8];
            size_t mark_length = end - tokens[i].begin;
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, mark_length);
            if (mark_length >= sizeof(mark))
                mark_length = sizeof(mark) - 1;
            memcpy(mark, text + tokens[i].begin, mark_length);
            mark[mark_length] = 0;
            st = ttf_trace_resource(e, "speech_range_mark", renderer->language_tag, mark, reading,
                                    "range-mark-reading", SIZE_MAX);
            if (st == TTF_OK)
                st = ttf_buffer_puts(&normalized, reading);
        } else if (sem->type == TTF_SEM_SYMBOL && !sem->unit[0] &&
                   ttf_speech_lengthening_span(e, renderer->language_tag, output.data,
                                               output.length, text + tokens[i].begin,
                                               end - tokens[i].begin, text + end, length - end)) {
            /* Text keeps the written mark; speech drops it as prosody. */
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, end - tokens[i].begin);
            st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0, renderer->language_tag,
                                "decision", "lengthening-mark", "", "", "word-final-lengthening",
                                SIZE_MAX);
            if (st == TTF_OK && e->frontend_options.output_mode == TTF_OUTPUT_TEXT)
                st = ttf_buffer_append(&normalized, text + tokens[i].begin, end - tokens[i].begin);
        } else if ((e->frontend_options.hanja == TTF_HANJA_KOREAN ||
                    e->frontend_options.kana == TTF_KANA_KOREAN) &&
                   !strcmp(sem->rule, "identity") && ttf_cjk_applies(renderer->language_tag, span.script))
            st = ttf_cjk_render(e, renderer->language_tag, text + tokens[i].begin,
                                end - tokens[i].begin, sem, &normalized,
                                span.lexicon, &span.pronunciation_layer, hangul_sentence(&in, i));
        else {
            st = renderer->verbalize(e, sem, render_text, render_length, &normalized);
            /* A reading the pack cannot produce (missing resource, value outside
               its grammar) must not fail the whole sentence: keep the written
               token, record why, and normalize everything else. */
            if ((st == TTF_EINVAL || st == TTF_ERESOURCE) && sem->type != TTF_SEM_TEXT) {
                ttf_buffer_free(&normalized);
                st = ttf_buffer_append(&normalized, text + tokens[i].begin, end - tokens[i].begin);
                if (st == TTF_OK) {
                    ttf_trace_stage(e, TTF_TRACE_POLICY, 0, normalized.length);
                    st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0,
                                        renderer->language_tag, "decision", "verbalizer-fallback",
                                        "", sem->rule, "unrenderable-token-kept-literally", SIZE_MAX);
                }
            }
        }
        if (st == TTF_OK && (!strcmp(sem->unit, "@math") || !strcmp(sem->unit, "@number-sign"))) {
            ttf_buffer_t spaced = {0};
            size_t begin = tokens[i].begin;
            if (begin && text[begin - 1] != ' ' && text[begin - 1] != '\t' &&
                text[begin - 1] != '\n')
                st = ttf_buffer_puts(&spaced, " ");
            if (st == TTF_OK)
                st = ttf_buffer_append(&spaced, normalized.data, normalized.length);
            /* Punctuation after the reading needs no separating space. */
            if (st == TTF_OK && end < length &&
                ttf_speech_spoken_after(e, renderer->language_tag, text + end, length - end) &&
                !following_particle(e, renderer, text + end, length - end))
                st = ttf_buffer_puts(&spaced, " ");
            ttf_buffer_free(&normalized);
            normalized = spaced;
        }
        if (st == TTF_OK && e->frontend_options.output_mode == TTF_OUTPUT_SPEECH && !rule &&
            identifier_fallback(sem, text + tokens[i].begin, end - tokens[i].begin))
            st = read_identifier(e, renderer, sem, text + tokens[i].begin, end - tokens[i].begin,
                                 &normalized, &identifier_read);
        if (st == TTF_OK && e->frontend_options.output_mode == TTF_OUTPUT_SPEECH && !rule &&
            !identifier_read && written_fallback(e, renderer, sem, text + tokens[i].begin, end - tokens[i].begin))
            st = read_written(e, renderer, sem, text + tokens[i].begin, end - tokens[i].begin,
                              &normalized, &identifier_read);
        if (st == TTF_OK && e->frontend_options.output_mode == TTF_OUTPUT_SPEECH && !rule &&
            (strncmp(span.lexicon, "lexicon:", 8) || !strncmp(sem->rule, "cjk.", 4)) &&
            strcmp(span.lexicon, "unknown-foreign-preserved")) {
            if (!identifier_read && sem->type == TTF_SEM_TEXT && strcmp(sem->rule, "identity") &&
                e->frontend_options.unreadable_policy != TTF_UNREADABLE_READ) {
                ttf_trace_stage(e, TTF_TRACE_POLICY, 0, normalized.length);
                st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0,
                                    renderer->language_tag, "decision", "semantic-guard", "",
                                    sem->value, "ambiguous-semantic-preserved", SIZE_MAX);
            } else if (!strcmp(sem->unit, "@unreadable-cluster")) {
                ttf_trace_stage(e, TTF_TRACE_POLICY, 0, normalized.length);
                const char *cluster = normalized.data ? ttf_resource_get(e, "speech_character",
                                                                         renderer->language_tag,
                                                                         normalized.data, NULL)
                                                      : NULL;
                if (!cluster && e->decorative_policy == TTF_DECORATIVE_READ && normalized.data)
                    cluster = ttf_resource_get(e, "speech_decorative", renderer->language_tag,
                                               normalized.data, NULL);
                if (!cluster && normalized.data) {
                    ttf_buffer_t name = {0};
                    int found = 0;
                    st = ttf_symbol_fallback_speak(e, renderer, normalized.data, normalized.length,
                                                   &name, &found);
                    if (st == TTF_OK && found) {
                        /* A spoken name is a word: keep it apart from its neighbours. */
                        int before = ttf_speech_spoken_before(e, renderer->language_tag,
                                                              output.data, output.length);
                        int after = ttf_speech_spoken_after(e, renderer->language_tag,
                                                            text + end, length - end);
                        st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0,
                                            renderer->language_tag, "decision", "unicode-fallback",
                                            "", normalized.data, "final-unicode-fallback",
                                            SIZE_MAX);
                        normalized.length = 0;
                        if (normalized.data)
                            normalized.data[0] = 0;
                        if (st == TTF_OK && before)
                            st = ttf_buffer_puts(&normalized, " ");
                        if (st == TTF_OK)
                            st = ttf_buffer_append(&normalized, name.data ? name.data : "",
                                                   name.length);
                        if (st == TTF_OK && after)
                            st = ttf_buffer_puts(&normalized, " ");
                        ttf_buffer_free(&name);
                        goto cluster_done;
                    }
                    ttf_buffer_free(&name);
                    if (st != TTF_OK)
                        goto fail;
                }
                if (cluster) {
                    st = ttf_trace_resource(e, "speech_character", renderer->language_tag,
                                            normalized.data, normalized.data,
                                            "final-cluster-mapped", SIZE_MAX);
                    ttf_buffer_free(&normalized);
                    if (st == TTF_OK)
                        st = ttf_buffer_puts(&normalized, cluster);
                } else {
                    st = ttf_trace_emit(e, TTF_TRACE_SELECTED, TTF_LAYER_COMMON, 0,
                                        renderer->language_tag, "decision", "unreadable-cluster",
                                        "", normalized.data,
                                        e->frontend_options.unreadable_policy == TTF_UNREADABLE_DROP
                                            ? "final-unreadable-dropped"
                                            : "final-unreadable-preserved",
                                        SIZE_MAX);
                    if (e->frontend_options.unreadable_policy == TTF_UNREADABLE_DROP) {
                        /* Removing a cluster between two words must not join them. */
                        int gap = ttf_speech_spoken_before(e, renderer->language_tag, output.data,
                                                           output.length) &&
                                  ttf_speech_spoken_after(e, renderer->language_tag, text + end,
                                                          length - end);
                        normalized.length = 0;
                        if (normalized.data)
                            normalized.data[0] = 0;
                        if (gap)
                            st = ttf_buffer_puts(&normalized, " ");
                    }
                }
            cluster_done:;
            } else
                st = ttf_speech_output(e, renderer, &normalized, output.data, output.length,
                                       text + end, length - end);
        }
        /* Only a particle written directly against the previous reading, with no
           space, is part of that phrase; a separate word keeps its own text.
           A final-pass name for an unclassified OTHER character or an unreadable
           cluster does not establish a noun boundary. Keep that substitution
           local to its span, without changing the next word. */
        if (st == TTF_OK && output.length && tokens[i].begin &&
            !(text[tokens[i].begin - 1] == ' ' || text[tokens[i].begin - 1] == '\t' ||
              text[tokens[i].begin - 1] == '\n') &&
            !(out->span_count &&
              ((!strcmp(out->spans[out->span_count - 1].semantic.unit, "@unreadable-cluster")) ||
               (ttf_script_legacy(out->spans[out->span_count - 1].script) == TTF_SCRIPT_OTHER &&
                !strcmp(out->spans[out->span_count - 1].semantic.rule, "identity")))))
        {
            if (normalized.length == end - tokens[i].begin &&
                !memcmp(normalized.data, text + tokens[i].begin, normalized.length))
                st = particle_reading(e, renderer, text, offset, text + tokens[i].begin,
                                      end - tokens[i].begin, &output, out);
            if (st == TTF_OK)
                st = particle_agreement(e, renderer, text + tokens[i].begin, end - tokens[i].begin,
                                        &output, &normalized);
        }
        if (st == TTF_OK && e->frontend_options.output_mode == TTF_OUTPUT_SPEECH) {
            /* A removed span leaves its neighboring spaces behind. Keep one. */
            size_t b = 0;
            while (b < normalized.length &&
                   (normalized.data[b] == ' ' || normalized.data[b] == '\t'))
                ++b;
            if (!normalized.length)
                removed_before = end > tokens[i].begin;
            else if (b == normalized.length && removed_before &&
                     (!output.length || output.data[output.length - 1] == ' ' ||
                      output.data[output.length - 1] == '\t')) {
                normalized.length = 0;
                normalized.data[0] = 0;
            } else
                removed_before = 0;
        }
        if (st == TTF_OK)
            st = ttf_trace_status(e);
        if (st != TTF_OK) {
            ttf_buffer_free(&normalized);
            goto fail;
        }
        span.begin = output.length;
        st = ttf_buffer_append(&output, normalized.data, normalized.length);
        span.end = output.length;
        ttf_trace_span_finish(e, span.begin, span.end);
        ttf_buffer_free(&normalized);
        if (st != TTF_OK)
            goto fail;
        st = add_span(out, &span, &capacity);
        if (st != TTF_OK)
            goto fail;
        j = i + consumed - 1;
        if (end < tokens[j].end) {
            tokens[j].begin = end;
            /* A partial semantic match changes the evidence seen by later
               context callbacks, even though the token array address is stable. */
            if (context_view.context_memo) {
                ttf_context_memo_clear(&memo);
                /* Repeated partial matches would rebuild every scope for each
                   suffix. Bound this extra work to two memo generations, then
                   use the same uncached pack policy for the rest of this unit. */
                if (++context_invalidations >= 2)
                    context_view.context_memo = NULL;
            }
            i = j;
        } else
            i = j + 1;
    }
    st = ttf_buffer_append(&output, "", 0);
    if (st != TTF_OK)
        goto fail;
    /* pronunciation=hangul reads the whole unit; {@pronounce} reads its spans. */
    if (e->phoneme_output || phoneme_count) {
        size_t *digits = NULL, digit_count = 0;
        st = digit_regions(out, output.data, &digits, &digit_count);
        if (st == TTF_OK && e->phoneme_output)
            st = ttf_korean_phonemize(e, output.data, output.length, 0, output.length, digits, digit_count);
        for (size_t k = 0; st == TTF_OK && !e->phoneme_output && k + 1 < phoneme_count; k += 2) {
            /* apple은{@pronounce} 애프른, [[국물]]{@pronounce}이 궁무리: a word the control
               touches is rewritten whole, or liaison across its edge would double a
               consonant (애플른); a word after a space stays as written (2026-10-10). */
            size_t b = phoneme_regions[k], z = phoneme_regions[k + 1];
            while (b >= 3 && hangul_syllable(output.data + b - 3))
                b -= 3;
            while (z + 3 <= output.length && hangul_syllable(output.data + z))
                z += 3;
            st = ttf_korean_phonemize(e, output.data, output.length, b, z, digits, digit_count);
        }
        free(digits);
    }
    free(phoneme_regions);
    phoneme_regions = NULL;
    if (st != TTF_OK)
        goto fail;
    out->text = output.data;
    out->length = output.length;
    ttf_context_memo_clear(&memo);
    ttf_numeric_width_free(&width_view);
    free(tokens);
    free(scopes);
    free(regions);
    free(context_text);
    free(context_tokens);
    return TTF_OK;
fail:
    free(phoneme_regions);
    free(context_text);
    free(context_tokens);
    ttf_context_memo_clear(&memo);
    ttf_numeric_width_free(&width_view);
    free(tokens);
    free(scopes);
    free(regions);
    ttf_buffer_free(&output);
    ttf_result_free(out);
    return ignore ? ignore_control(e, text, length, offset, &rejected, trace_mark, out) : st;
}

typedef struct {
    ttf_buffer_t text;
    ttf_result_t *out;
    size_t cap;
    ttf_status_t status;
} collector_t;
static int collect(const ttf_segment_t *seg, void *userdata) {
    collector_t *c = (collector_t *)userdata;
    size_t i, base = c->text.length;
    c->status = ttf_buffer_append(&c->text, seg->text, seg->length);
    if (c->status != TTF_OK)
        return 1;
    for (i = 0; i < seg->span_count; i++) {
        ttf_span_t span = seg->spans[i];
        span.begin += base;
        span.end += base;
        c->status = add_span(c->out, &span, &c->cap);
        if (c->status != TTF_OK)
            return 1;
    }
    return 0;
}
ttf_status_t ttf_normalize(const ttf_engine_t *e, const char *text, size_t length,
                           ttf_result_t *out) {
    ttf_stream_t *s = NULL;
    collector_t c;
    ttf_status_t st;
    if (!e || !out || (!text && length))
        return TTF_EINVAL;
    memset(out, 0, sizeof(*out));
    memset(&c, 0, sizeof(c));
    c.out = out;
    st = ttf_stream_create(e, collect, &c, &s);
    if (st != TTF_OK)
        return st;
    st = ttf_stream_write(s, text, length);
    if (st == TTF_OK)
        st = ttf_stream_flush(s);
    ttf_stream_destroy(s);
    if (c.status != TTF_OK)
        st = c.status;
    if (st == TTF_OK)
        st = ttf_buffer_append(&c.text, "", 0);
    if (st != TTF_OK) {
        ttf_buffer_free(&c.text);
        ttf_result_free(out);
        return st;
    }
    out->text = c.text.data;
    out->length = c.text.length;
    return TTF_OK;
}
