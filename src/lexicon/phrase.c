#include "../core/internal.h"
#include "../cjk/cjk_reading.h"

static int horizontal(char c) {
    return c == ' ' || c == '\t';
}
static char folded(char c) {
    return c >= 'A' && c <= 'Z' ? (char)(c + 'a' - 'A') : c;
}
static int lexical(ttf_script_t script) {
    return script == TTF_SCRIPT_LATIN || script == TTF_SCRIPT_HANGUL ||
           ttf_script_legacy(script) == TTF_SCRIPT_OTHER;
}
/* Hyphen, apostrophe and ampersand join letters inside a phrase word
   (a-free cafe, mom's pizza, pull&bear); a lone & is a word of its own
   (procter & gamble). Keys with & or an apostrophe are phrases even without a
   space: those words can stay several tokens (Pull&Bear, NILLI'S). Phrase
   buckets hash the first letter run. */
static int joiner(char c) {
    return c == '-' || c == '\'' || c == '&';
}
static size_t first_run(const char *word) {
    size_t n = 0;
    while (word[n] && word[n] != ' ' && !joiner(word[n]))
        ++n;
    return n;
}
static size_t first_bucket(const char *word, size_t length) {
    uint32_t hash = 2166136261u;
    size_t i;
    for (i = 0; i < length; ++i) {
        hash ^= (unsigned char)folded(word[i]);
        hash *= 16777619u;
    }
    return hash % TTF_HASH_BUCKETS;
}

/* Single-token lexicons retain their existing punctuation/number inventory.
   Multiword entries have an explicit bounded grammar shared by all languages.
   Canonicalization belongs at resource/API ingestion, never in output text. */
ttf_status_t ttf_lexicon_canonical_key(const char *key, char **out) {
    const char *colon = strchr(key, ':');
    size_t prefix, i, used, length = strlen(key), words = 1;
    int multi = 0;
    char *copy;
    ttf_script_t script = TTF_SCRIPT_SYMBOL;
    *out = NULL;
    if (!colon || colon == key || !colon[1] || (size_t)(colon - key) >= TTF_LANGUAGE_TAG_SIZE)
        return TTF_EINVAL;
    prefix = (size_t)(colon - key) + 1;
    for (i = prefix; i < length;) {
        uint32_t cp;
        size_t width;
        if (ttf_utf8_decode(key + i, length - i, &cp, &width) != 1)
            return TTF_EINVAL;
        if (ttf_script_of(cp) == TTF_SCRIPT_WHITESPACE)
            multi = 1;
        i += width;
    }
    copy = (char *)malloc(length + 1);
    if (!copy)
        return TTF_ENOMEM;
    memcpy(copy, key, prefix);
    used = prefix;
    for (i = prefix; key[i];) {
        if (multi && horizontal(key[i])) {
            if (used == prefix || copy[used - 1] == ' ')
                goto invalid;
            while (horizontal(key[i]))
                ++i;
            if (!key[i] || ++words > TTF_PHRASE_WORDS)
                goto invalid;
            copy[used++] = ' ';
        } else if (multi && joiner(key[i])) {
            int inside = used > prefix && copy[used - 1] != ' ' && !joiner(copy[used - 1]) &&
                         key[i + 1] && !horizontal(key[i + 1]) && !joiner(key[i + 1]);
            int alone = key[i] == '&' && used > prefix && copy[used - 1] == ' ' &&
                        horizontal(key[i + 1]);
            if (!inside && !alone)
                goto invalid;
            copy[used++] = key[i++];
        } else if (multi) {
            uint32_t cp;
            size_t width, j;
            ttf_script_t current;
            if (ttf_utf8_decode(key + i, length - i, &cp, &width) != 1)
                goto invalid;
            current = ttf_script_legacy(ttf_script_of(cp));
            if (!lexical(current) || (script != TTF_SCRIPT_SYMBOL && current != script))
                goto invalid;
            script = current;
            for (j = 0; j < width; ++j)
                copy[used++] = folded(key[i++]);
        } else
            copy[used++] = folded(key[i++]);
        if (multi && used - prefix >= TTF_IR_VALUE_SIZE)
            goto invalid;
    }
    copy[used] = '\0';
    *out = copy;
    return TTF_OK;
invalid:
    free(copy);
    return TTF_EINVAL;
}

void ttf_phrase_index(ttf_engine_t *engine) {
    size_t bucket;
    memset(engine->phrases, 0, sizeof(engine->phrases));
    for (bucket = 0; bucket < TTF_RESOURCE_BUCKETS; ++bucket) {
        ttf_entry_t *entry;
        for (entry = engine->resources[bucket]; entry; entry = entry->next) {
            const char *word;
            size_t run;
            char source[TTF_LANGUAGE_TAG_SIZE];
            const ttf_language_pack_t *source_pack, *script_pack = NULL;
            uint32_t cp;
            size_t at, source_length, width, pack;
            int script_score = 0;
            entry->phrase_next = NULL;
            if (strcmp(entry->kind, "lexicon") || !(word = strchr(entry->key, ':')) ||
                (!strchr(++word, ' ') && !strchr(word, '&') && !strchr(word, '\'')))
                continue;
            run = first_run(word);
            if (!run)
                continue;
            source_length = (size_t)(word - entry->key) - 1;
            if (source_length >= sizeof(source))
                continue;
            memcpy(source, entry->key, source_length);
            source[source_length] = '\0';
            source_pack = ttf_find_pack(engine, source);
            if (!source_pack || !strcmp(source, entry->lang) ||
                (strcmp(entry->lang, "*") && !ttf_find_pack(engine, entry->lang)))
                continue;
            if (ttf_utf8_decode(word, run, &cp, &width) != 1)
                continue;
            /* Match the core's source-script router. Entries for a registered
               pack that does not own this script cannot render these words and
               must not change commitment/context merely by being present. */
            for (pack = 0; pack < engine->pack_count; ++pack) {
                const ttf_language_pack_t *candidate = engine->packs[pack];
                int score = candidate->script_score ? candidate->script_score(ttf_script_of(cp)) : 0;
                if (score > script_score) {
                    script_score = score;
                    script_pack = candidate;
                }
            }
            if (script_pack != source_pack)
                continue;
            if (ttf_resource_get(engine, "lexicon", entry->lang, entry->key, NULL) != entry->value)
                continue;
            at = first_bucket(word, run);
            entry->phrase_next = engine->phrases[at];
            engine->phrases[at] = entry;
        }
    }
}

static const ttf_entry_t *first_entries(const ttf_engine_t *engine, const char *text,
                                       const ttf_token_t *token) {
    if (!lexical(token->script) || token->end - token->begin >= TTF_IR_VALUE_SIZE)
        return NULL;
    return engine->phrases[first_bucket(text + token->begin, token->end - token->begin)];
}

/* Return a matching prefix extent even after a later mismatch. Earlier cuts
   must stay protected when that mismatch was unavailable in an earlier write. */
static size_t prefix_end(const char *text, size_t length, size_t begin, const char *key) {
    size_t at = begin, k = 0;
    size_t limit = length - begin > TTF_PHRASE_INPUT ? begin + TTF_PHRASE_INPUT : length;
    while (at < limit && key[k]) {
        if (key[k] == ' ' && horizontal(text[at])) {
            while (at < limit && horizontal(text[at]))
                ++at;
            ++k;
        } else if (folded(text[at]) == key[k]) {
            ++at;
            ++k;
        } else
            break;
    }
    return at;
}

/* Script tokens are sub-tokens of identifiers, URLs and hyphenated words.
   Only a continuation in the same source script (or a digit) binds: a closing
   apostrophe followed by a different-script particle remains a delimiter. */
static int connector(const char *text, const ttf_token_t *token) {
    uint32_t cp;
    size_t width;
    if (ttf_utf8_decode(text + token->begin, token->end - token->begin, &cp, &width) != 1 ||
        width != token->end - token->begin)
        return 0;
    return (cp < 128 && strchr("-'.@_/\\+", (int)cp) != NULL) ||
           cp == 0x2019 || cp == 0x2010 || cp == 0x2011;
}
static int same_word_script(ttf_script_t script, ttf_script_t neighbor) {
    return ttf_script_legacy(neighbor) == ttf_script_legacy(script) || neighbor == TTF_SCRIPT_DIGIT;
}
static int word_continues(const char *text, const ttf_token_t *tokens, size_t count, size_t at) {
    if (at + 1 >= count || tokens[at].end != tokens[at + 1].begin)
        return 0;
    if (tokens[at + 1].script == TTF_SCRIPT_DIGIT)
        return 1;
    return at + 2 < count && tokens[at + 1].end == tokens[at + 2].begin &&
           same_word_script(tokens[at].script, tokens[at + 2].script) &&
           connector(text, &tokens[at + 1]);
}
static int word_started_before(const char *text, const ttf_token_t *tokens, size_t at) {
    if (!at || tokens[at - 1].end != tokens[at].begin)
        return 0;
    if (tokens[at - 1].script == TTF_SCRIPT_DIGIT)
        return 1;
    return at >= 2 && tokens[at - 2].end == tokens[at - 1].begin &&
           same_word_script(tokens[at].script, tokens[at - 2].script) &&
           connector(text, &tokens[at - 1]);
}

size_t ttf_phrase_protect_until(const ttf_engine_t *engine, const char *text, size_t length,
                                const ttf_token_t *tokens, size_t index) {
    const ttf_token_t *token = &tokens[index];
    const ttf_entry_t *entry;
    size_t until = token->begin;
    if (engine->frontend_options.hanja != TTF_HANJA_PASSTHROUGH ||
        engine->frontend_options.kana != TTF_KANA_PASSTHROUGH)
        until = ttf_cjk_protect_until(engine, text, length, until);
    if (engine->config.render_policy != TTF_PHONETIC_LOCALIZE ||
        word_started_before(text, tokens, index))
        return until;
    for (entry = first_entries(engine, text, token); entry; entry = entry->phrase_next) {
        const char *key = strchr(entry->key, ':') + 1;
        size_t end = prefix_end(text, length, token->begin, key);
        if (end > until)
            until = end;
    }
    return until;
}

/* One lookup is made at each complete lexical word. Including the first word
   enforces layer precedence before longest match, even for a user single-word
   override competing with a longer system phrase. */
const char *ttf_phrase_match(const ttf_engine_t *engine, const char *source, const char *target,
                             const char *text, const ttf_token_t *tokens, size_t count,
                             size_t index, size_t *consumed, char value[TTF_IR_VALUE_SIZE],
                             ttf_resource_layer_t *layer) {
    char key[TTF_LANGUAGE_TAG_SIZE + TTF_IR_VALUE_SIZE];
    size_t prefix = strlen(source) + 1, used = 0, at = index, words = 0, best_tokens = 0;
    const char *best = NULL;
    ttf_script_t script = tokens[index].script;
    if (word_started_before(text, tokens, index) ||
        !first_entries(engine, text, &tokens[index]))
        return NULL;
    memcpy(key, source, prefix - 1);
    key[prefix - 1] = ':';
    while (at < count && words < TTF_PHRASE_WORDS) {
        const ttf_token_t *token = &tokens[at];
        size_t n = token->end - token->begin, j;
        const char *hit;
        ttf_resource_layer_t candidate_layer;
        if (ttf_script_legacy(token->script) != ttf_script_legacy(script) || !lexical(script) ||
            token->end - tokens[index].begin > TTF_PHRASE_INPUT ||
            n >= TTF_IR_VALUE_SIZE - used)
            break;
        for (j = 0; j < n; ++j)
            key[prefix + used++] = folded(text[token->begin + j]);
        key[prefix + used] = '\0';
        ++words;
        /* A-FREE, MOM'S, Pull&Bear: an adjacent joiner and letter run continue
           the same phrase word. */
        if (at + 2 < count && tokens[at + 1].begin == token->end &&
            tokens[at + 1].end == tokens[at + 1].begin + 1 && joiner(text[tokens[at + 1].begin]) &&
            tokens[at + 2].begin == tokens[at + 1].end &&
            ttf_script_legacy(tokens[at + 2].script) == ttf_script_legacy(script) &&
            used + 1 < TTF_IR_VALUE_SIZE) {
            key[prefix + used++] = text[tokens[at + 1].begin];
            --words;
            at += 2;
            continue;
        }
        hit = word_continues(text, tokens, count, at) ? NULL :
            ttf_resource_get(engine, "lexicon", target, key, &candidate_layer);
        if (hit && (!best || candidate_layer > *layer ||
                    (candidate_layer == *layer && at - index + 1 > best_tokens))) {
            best = hit;
            *layer = candidate_layer;
            best_tokens = at - index + 1;
            *consumed = at - index + 1;
            memcpy(value, key + prefix, used + 1);
        }
        if (at + 2 >= count || tokens[at + 1].script != TTF_SCRIPT_WHITESPACE)
            break;
        for (j = tokens[at + 1].begin; j < tokens[at + 1].end; ++j)
            if (!horizontal(text[j]))
                break;
        if (j != tokens[at + 1].end || used + 1 >= TTF_IR_VALUE_SIZE)
            break;
        key[prefix + used++] = ' ';
        at += 2;
        /* Procter & Gamble: a lone ampersand between spaces is a phrase word. */
        if (at + 2 < count && tokens[at].end == tokens[at].begin + 1 && text[tokens[at].begin] == '&' &&
            tokens[at + 1].script == TTF_SCRIPT_WHITESPACE && used + 2 < TTF_IR_VALUE_SIZE &&
            words < TTF_PHRASE_WORDS) {
            for (j = tokens[at + 1].begin; j < tokens[at + 1].end; ++j)
                if (!horizontal(text[j]))
                    break;
            if (j != tokens[at + 1].end)
                break;
            key[prefix + used++] = '&';
            key[prefix + used++] = ' ';
            ++words;
            at += 2;
        }
    }
    return best_tokens > 1 ? best : NULL;
}
