#include "../core/internal.h"
/* A request dictionary (2026-10-26): exact rules and lexicon rows owned by the caller and
   attached to one request or stream, above every engine layer (TTF_LAYER_REQUEST). The
   frozen engine stays shared and unchanged; lookups ask the dictionary first. */
#define DICT_BUCKETS 257
static void entry_free(ttf_entry_t *p) {
    free(p);
}
static int dict_kind(const char *kind) {
    return (kind[0] == 'e' && !strcmp(kind, "exact")) || (kind[0] == 'l' && !strcmp(kind, "lexicon"));
}
/* A language tag of a pack or `*` (exact rules only): letters, digits and '-'. */
static int tag_ok(const char *tag, int star) {
    size_t i, n;
    if (!tag || !*tag)
        return 0;
    if (star && !strcmp(tag, "*"))
        return 1;
    n = strlen(tag);
    if (n >= TTF_LANGUAGE_TAG_SIZE)
        return 0;
    for (i = 0; i < n; ++i)
        if (!((tag[i] >= 'a' && tag[i] <= 'z') || (tag[i] >= 'A' && tag[i] <= 'Z') ||
              (tag[i] >= '0' && tag[i] <= '9') || (i && tag[i] == '-')))
            return 0;
    return 1;
}
/* Valid UTF-8 without tab, line break or NUL: one field of a resource row. */
static int text_ok(const char *s) {
    size_t n, i = 0;
    if (!s || !*s)
        return 0;
    n = strlen(s);
    while (i < n) {
        uint32_t cp;
        size_t width;
        if (ttf_utf8_decode(s + i, n - i, &cp, &width) != 1 || cp == '\t' || cp == '\n' || cp == '\r')
            return 0;
        i += width;
    }
    return 1;
}
static void lengths_add(ttf_dict_t *d, const ttf_entry_t *p) {
    size_t i, n = strlen(p->key);
    uint64_t bit = (uint64_t)1 << (n < 63 ? n : 63);
    for (i = 0; i < d->length_count; ++i)
        if (!strcmp(d->lengths[i].kind, p->kind) && !strcmp(d->lengths[i].lang, p->lang)) {
            d->lengths[i].lengths |= bit;
            return;
        }
    /* the slots borrow the row's strings; a removal rebuilds them (reindex) */
    if (i < TTF_DICT_LENGTH_SLOTS) {
        d->lengths[i].kind = p->kind;
        d->lengths[i].lang = p->lang;
        d->lengths[i].lengths = bit;
        d->length_count = i + 1;
    } else
        d->lengths_full = 1;
}
/* The derived lists of one row: key lengths, long exact keys, multiword lexicon keys. */
static void index_row(ttf_dict_t *d, ttf_entry_t *p) {
    const char *word;
    lengths_add(d, p);
    p->rule_next = NULL;
    p->phrase_next = NULL;
    if (p->kind[0] == 'e' && strlen(p->key) >= 63) {
        p->rule_next = d->long_rules;
        d->long_rules = p;
    }
    if (p->kind[0] == 'l' && (word = strchr(p->key, ':')) != NULL &&
        (strchr(word + 1, ' ') || strchr(word + 1, '&') || strchr(word + 1, '\'')) &&
        strncmp(p->key, p->lang, (size_t)(word - p->key))) {
        size_t at = ttf_phrase_bucket(word + 1);
        p->phrase_next = d->phrases[at];
        d->phrases[at] = p;
    }
}
static void reindex(ttf_dict_t *d) {
    size_t b;
    d->length_count = 0;
    d->lengths_full = 0;
    d->long_rules = NULL;
    memset(d->phrases, 0, sizeof(d->phrases));
    for (b = 0; b < d->bucket_count; ++b) {
        ttf_entry_t *p;
        for (p = d->buckets[b]; p; p = p->next)
            index_row(d, p);
    }
}
static ttf_status_t grow(ttf_dict_t *d) {
    size_t count = d->bucket_count * 4 + 1, b;
    ttf_entry_t **buckets = (ttf_entry_t **)calloc(count, sizeof(*buckets));
    if (!buckets)
        return TTF_ENOMEM;
    for (b = 0; b < d->bucket_count; ++b) {
        ttf_entry_t *p = d->buckets[b], *next;
        for (; p; p = next) {
            next = p->next;
            p->next = buckets[p->hash % count];
            buckets[p->hash % count] = p;
        }
    }
    free(d->buckets);
    d->buckets = buckets;
    d->bucket_count = count;
    return TTF_OK;
}
static ttf_entry_t **slot(const ttf_dict_t *d, const char *kind, const char *lang, const char *key,
                          uint32_t hash) {
    ttf_entry_t **at = &d->buckets[hash % d->bucket_count];
    for (; *at; at = &(*at)->next)
        if ((*at)->hash == hash && !strcmp((*at)->kind, kind) && !strcmp((*at)->lang, lang) &&
            !strcmp((*at)->key, key))
            break;
    return at;
}
static ttf_status_t put(ttf_dict_t *d, const char *kind, const char *lang, const char *key,
                        const char *value) {
    size_t a = strlen(kind) + 1, b = strlen(lang) + 1, c = strlen(key) + 1, v = strlen(value) + 1;
    uint32_t hash = ttf_entry_hash(kind, lang, key);
    ttf_entry_t **at, *p;
    if (d->rows >= 2 * d->bucket_count && grow(d) != TTF_OK)
        return TTF_ENOMEM;
    p = (ttf_entry_t *)calloc(1, sizeof(*p) + a + b + c + v);
    if (!p)
        return TTF_ENOMEM;
    p->kind = (char *)(p + 1);
    p->lang = p->kind + a;
    p->key = p->lang + b;
    p->value = p->key + c;
    memcpy(p->kind, kind, a);
    memcpy(p->lang, lang, b);
    memcpy(p->key, key, c);
    memcpy(p->value, value, v);
    p->layer = TTF_LAYER_REQUEST;
    p->order = ++d->order;
    p->hash = hash;
    at = slot(d, kind, lang, key, hash);
    if (*at) {
        /* a replaced row may sit in the derived lists */
        p->next = (*at)->next;
        entry_free(*at);
        *at = p;
        reindex(d);
    } else {
        p->next = d->buckets[hash % d->bucket_count];
        d->buckets[hash % d->bucket_count] = p;
        ++d->rows;
        index_row(d, p);
    }
    return TTF_OK;
}
static ttf_status_t drop(ttf_dict_t *d, const char *kind, const char *lang, const char *key) {
    ttf_entry_t **at, *p;
    at = slot(d, kind, lang, key, ttf_entry_hash(kind, lang, key));
    if (!*at)
        return TTF_OK;
    p = *at;
    *at = p->next;
    entry_free(p);
    --d->rows;
    reindex(d);
    return TTF_OK;
}

ttf_status_t ttf_dict_create(ttf_dict_t **out) {
    ttf_dict_t *d;
    if (!out)
        return TTF_EINVAL;
    *out = NULL;
    d = (ttf_dict_t *)calloc(1, sizeof(*d));
    if (!d)
        return TTF_ENOMEM;
    d->buckets = (ttf_entry_t **)calloc(DICT_BUCKETS, sizeof(*d->buckets));
    if (!d->buckets) {
        free(d);
        return TTF_ENOMEM;
    }
    d->bucket_count = DICT_BUCKETS;
    *out = d;
    return TTF_OK;
}
void ttf_dict_clear(ttf_dict_t *d) {
    size_t b;
    if (!d)
        return;
    for (b = 0; b < d->bucket_count; ++b) {
        ttf_entry_t *p = d->buckets[b], *next;
        for (; p; p = next) {
            next = p->next;
            entry_free(p);
        }
        d->buckets[b] = NULL;
    }
    d->rows = 0;
    reindex(d);
}
void ttf_dict_destroy(ttf_dict_t *d) {
    if (!d)
        return;
    ttf_dict_clear(d);
    free(d->buckets);
    free(d);
}
size_t ttf_dict_size(const ttf_dict_t *d) {
    return d ? d->rows : 0;
}
ttf_status_t ttf_dict_add_exact(ttf_dict_t *d, const char *language, const char *input,
                                const char *output) {
    if (!d || !tag_ok(language, 1) || !text_ok(input) || !text_ok(output))
        return TTF_EINVAL;
    return put(d, "exact", language, input, output);
}
ttf_status_t ttf_dict_remove_exact(ttf_dict_t *d, const char *language, const char *input) {
    if (!d || !tag_ok(language, 1) || !text_ok(input))
        return TTF_EINVAL;
    return drop(d, "exact", language, input);
}
/* The engine's lexicon key: source tag, ':' and the canonical word (lexicon.md). */
static ttf_status_t lexicon_key(const char *source, const char *word, char **out) {
    size_t a, b;
    char *raw;
    ttf_status_t status;
    *out = NULL;
    if (!tag_ok(source, 0) || !text_ok(word))
        return TTF_EINVAL;
    a = strlen(source);
    b = strlen(word);
    raw = (char *)malloc(a + b + 2);
    if (!raw)
        return TTF_ENOMEM;
    memcpy(raw, source, a);
    raw[a] = ':';
    memcpy(raw + a + 1, word, b + 1);
    status = ttf_lexicon_canonical_key(raw, out);
    free(raw);
    return status;
}
ttf_status_t ttf_dict_add_lexicon(ttf_dict_t *d, const char *source_lang, const char *render_lang,
                                  const char *word, const char *pronunciation) {
    char *key;
    ttf_status_t status;
    if (!d || !tag_ok(render_lang, 0) || !text_ok(pronunciation))
        return TTF_EINVAL;
    status = lexicon_key(source_lang, word, &key);
    if (status != TTF_OK)
        return status;
    status = put(d, "lexicon", render_lang, key, pronunciation);
    free(key);
    return status;
}
ttf_status_t ttf_dict_remove_lexicon(ttf_dict_t *d, const char *source_lang,
                                     const char *render_lang, const char *word) {
    char *key;
    ttf_status_t status;
    if (!d || !tag_ok(render_lang, 0))
        return TTF_EINVAL;
    status = lexicon_key(source_lang, word, &key);
    if (status != TTF_OK)
        return status;
    status = drop(d, "lexicon", render_lang, key);
    free(key);
    return status;
}
/* Resource TSV rows of kind exact (lang input output) or lexicon (render source:word reading);
   blank lines and # comments are skipped. Any other kind fails the whole file. */
ttf_status_t ttf_dict_load(ttf_dict_t *d, const char *path) {
    FILE *f;
    char *line = NULL;
    size_t capacity = 0, length = 0, rows = 0;
    ttf_status_t status = TTF_OK;
    int c;
    if (!d || !path)
        return TTF_EINVAL;
    f = fopen(path, "rb");
    if (!f)
        return TTF_EIO;
    for (;;) {
        c = fgetc(f);
        if (c != EOF && c != '\n') {
            if (length + 2 > capacity) {
                size_t grown = capacity ? capacity * 2 : 256;
                char *next = (char *)realloc(line, grown);
                if (!next) {
                    status = TTF_ENOMEM;
                    break;
                }
                line = next;
                capacity = grown;
            }
            line[length++] = (char)c;
            continue;
        }
        if (length && line[length - 1] == '\r')
            --length;
        if (length && line[0] != '#') {
            char *field[4];
            size_t n = 0;
            char *p = line;
            line[length] = '\0';
            field[n++] = p;
            while (n < 4 && (p = strchr(p, '\t')) != NULL) {
                *p++ = '\0';
                field[n++] = p;
            }
            if (n != 4 || strchr(field[3], '\t') || !dict_kind(field[0])) {
                status = TTF_ERESOURCE;
                break;
            }
            if (!strcmp(field[0], "exact"))
                status = ttf_dict_add_exact(d, field[1], field[2], field[3]);
            else {
                char *colon = strchr(field[2], ':');
                if (!colon) {
                    status = TTF_ERESOURCE;
                    break;
                }
                *colon = '\0';
                status = ttf_dict_add_lexicon(d, field[2], field[1], colon + 1, field[3]);
            }
            if (status != TTF_OK) {
                if (status == TTF_EINVAL)
                    status = TTF_ERESOURCE;
                break;
            }
            ++rows;
        }
        length = 0;
        if (c == EOF)
            break;
    }
    if (status == TTF_OK && ferror(f))
        status = TTF_EIO;
    free(line);
    fclose(f);
    (void)rows;
    return status;
}

/* Lookups (the request view's dict; resource.c, rule.c, phrase.c). */
const ttf_entry_t *ttf_dict_find(const ttf_dict_t *d, const char *kind, const char *lang,
                                 const char *key) {
    if (!d->rows || !dict_kind(kind))
        return NULL;
    return *slot(d, kind, lang, key, ttf_entry_hash(kind, lang, key));
}
uint64_t ttf_dict_lengths(const ttf_dict_t *d, const char *kind, const char *lang) {
    size_t i;
    if (!d->rows || !dict_kind(kind))
        return 0;
    if (d->lengths_full)
        return ~(uint64_t)0;
    for (i = 0; i < d->length_count; ++i)
        if (!strcmp(d->lengths[i].kind, kind) && !strcmp(d->lengths[i].lang, lang))
            return d->lengths[i].lengths;
    return 0;
}
