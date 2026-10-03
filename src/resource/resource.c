#include "../core/internal.h"
#include "validation.h"
#define RESOURCE_LIMIT (16u * 1024u * 1024u)
static uint32_t hash_bytes(uint32_t h, const char *s) {
    while (*s) {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    return h;
}
static size_t bucket(const char *kind, const char *lang, const char *key) {
    uint32_t h = hash_bytes(2166136261u, kind);
    h = hash_bytes(h, "\t");
    h = hash_bytes(h, lang);
    h = hash_bytes(h, "\t");
    return hash_bytes(h, key) % TTF_HASH_BUCKETS;
}
static void entry_free(ttf_entry_t *p) {
    free(p->kind);
    free(p->lang);
    free(p->key);
    free(p->value);
    free(p);
}
static ttf_status_t resource_insert(ttf_engine_t *e, const char *kind, const char *lang, const char *key,
                                   const char *value, ttf_resource_layer_t layer, ttf_entry_t **inserted) {
    size_t h;
    ttf_entry_t *p;
    if (e->resource_order == SIZE_MAX)
        return TTF_ELIMIT;
    p = (ttf_entry_t *)calloc(1, sizeof(*p));
    if (!p)
        return TTF_ENOMEM;
    p->kind = ttf_strdup(kind);
    p->lang = ttf_strdup(lang);
    if (!strcmp(kind, "lexicon")) {
        ttf_status_t key_status = ttf_lexicon_canonical_key(key, &p->key);
        if (key_status != TTF_OK) {
            entry_free(p);
            return key_status == TTF_EINVAL ? TTF_ERESOURCE : key_status;
        }
    } else
        p->key = ttf_strdup(key);
    p->value = ttf_strdup(value);
    p->layer = layer;
    p->order = e->resource_order++;
    if (!p->kind || !p->lang || !p->key || !p->value) {
        entry_free(p);
        return TTF_ENOMEM;
    }
    h = bucket(kind, lang, p->key);
    p->next = e->resources[h];
    e->resources[h] = p;
    if (inserted)
        *inserted = p;
    return TTF_OK;
}
ttf_status_t ttf_resource_set(ttf_engine_t *e, const char *kind, const char *lang, const char *key,
                              const char *value, ttf_resource_layer_t layer) {
    return resource_insert(e, kind, lang, key, value, layer, NULL);
}
static const ttf_entry_t *find_entry(const ttf_engine_t *e, const char *kind, const char *lang,
                                     const char *key) {
    const ttf_entry_t *p, *best = NULL;
    size_t h = bucket(kind, lang, key);
    p = e->cjk && ttf_cjk_resource_kind(kind) ? e->cjk->resources[h] : e->resources[h];
    for (; p; p = p->next) {
        if (strcmp(kind, p->kind) || strcmp(lang, p->lang) || strcmp(key, p->key))
            continue;
        if (!best || p->layer > best->layer || (p->layer == best->layer && p->order < best->order))
            best = p;
    }
    return best;
}
const ttf_entry_t *ttf_resource_entry(const ttf_engine_t *e, const char *kind, const char *lang,
                                      const char *key) {
    const ttf_entry_t *a, *b;
    if (!e || !kind || !lang || !key)
        return NULL;
    a = find_entry(e, kind, lang, key);
    b = strcmp(lang, "*") ? find_entry(e, kind, "*", key) : NULL;
    if (b && (!a || b->layer > a->layer))
        a = b;
    return a;
}
const char *ttf_resource_get(const ttf_engine_t *e, const char *kind, const char *lang,
                             const char *key, ttf_resource_layer_t *layer) {
    const ttf_entry_t *a = ttf_resource_entry(e, kind, lang, key);
    if (!a)
        return NULL;
    if (layer)
        *layer = a->layer;
    return a->value;
}
void ttf_resource_destroy(ttf_engine_t *e) {
    size_t i, table;
    for (table = 0; table < (e->cjk ? 2u : 1u); ++table) for (i = 0; i < TTF_HASH_BUCKETS; i++) {
        ttf_entry_t **head = table ? &e->cjk->resources[i] : &e->resources[i];
        ttf_entry_t *p = *head;
        while (p) {
            ttf_entry_t *n = p->next;
            entry_free(p);
            p = n;
        }
        *head = NULL;
    }
    free(e->cjk); /* Word buckets borrow rows; never free their entries again. */
    e->cjk = NULL;
}
void ttf_resource_rollback(ttf_engine_t *e, size_t checkpoint) {
    size_t i;
    for (i = 0; i < TTF_HASH_BUCKETS; ++i) {
        ttf_entry_t **at = &e->resources[i];
        while (*at) {
            ttf_entry_t *entry = *at;
            if (entry->order < checkpoint) {
                at = &entry->next;
            } else {
                *at = entry->next;
                entry_free(entry);
            }
        }
    }
    e->resource_order = checkpoint;
}
static uint32_t read_u32(const unsigned char *s) {
    return (uint32_t)s[0] | ((uint32_t)s[1] << 8) | ((uint32_t)s[2] << 16) | ((uint32_t)s[3] << 24);
}
static ttf_status_t parse_tsv(ttf_engine_t *e, const ttf_engine_t *owner, char *data, size_t n,
                              ttf_resource_layer_t layer, ttf_resource_diagnostic_t *d) {
    size_t pos = 0, i, w, line = 1;
    uint32_t cp;
    for (i = 0; i < n; i += w) {
        if (!data[i] || ttf_utf8_decode(data + i, n - i, &cp, &w) != 1) {
            d->line = line;
            return ttf_validation_error(d, "Resource must be valid UTF-8 without embedded NUL");
        }
        if (cp == '\n')
            ++line;
    }
    line = 0;
    while (pos < n) {
        char *f[4] = {NULL, NULL, NULL, NULL};
        size_t start = pos, end, k = 0;
        int extra_field = 0;
        d->kind[0] = d->language[0] = d->key[0] = 0;
        d->line = ++line;
        while (pos < n && data[pos] != '\n')
            pos++;
        end = pos;
        if (pos < n)
            pos++;
        if (end > start && data[end - 1] == '\r')
            end--;
        data[end] = '\0';
        if (end == start || data[start] == '#')
            continue;
        f[0] = data + start;
        for (i = start; i < end; i++)
            if (data[i] == '\t') {
                if (k == 3) {
                    data[i] = 0;
                    extra_field = 1;
                    break;
                }
                data[i] = '\0';
                f[++k] = data + i + 1;
            }
        if (f[0])
            ttf_validation_copy(d->kind, sizeof(d->kind), f[0]);
        if (f[1])
            ttf_validation_copy(d->language, sizeof(d->language), f[1]);
        if (f[2])
            ttf_validation_copy(d->key, sizeof(d->key), f[2]);
        if (extra_field || k != 3 || !f[0][0] || !f[1][0] || !f[2][0] || !f[3][0] ||
            strlen(f[1]) >= TTF_LANGUAGE_TAG_SIZE)
            return ttf_validation_error(d,
                                        "Expected four nonempty fields and a bounded language tag");
        {
            ttf_resource_record_t row = {sizeof(row), f[0], f[1], f[2], f[3], layer};
            int validated;
            ttf_entry_t *entry = NULL;
            ttf_status_t st = ttf_validate_resource_record(owner, &row, &validated, d);
            if (st == TTF_OK)
                st = resource_insert(e, f[0], f[1], f[2], f[3], layer, &entry);
            if (st != TTF_OK)
                return st;
            /* Engine-wide validation reuses this result instead of validating the
               row again (2026-10-03: start-up checked every row three times). */
            if (owner->validator_count == owner->builtin_validators) { /* deterministic only */
                entry->checked = validated ? 1 : 2;
                entry->checked_validators = owner->validator_count;
            }
        }
    }
    return TTF_OK;
}
ttf_status_t ttf_resource_load(ttf_engine_t *e, const char *path, ttf_resource_layer_t layer) {
    return ttf_resource_load_ex(e, path, layer, NULL);
}
ttf_status_t ttf_resource_load_ex(ttf_engine_t *e, const char *path, ttf_resource_layer_t layer,
                                  ttf_resource_diagnostic_t *diagnostic) {
    FILE *f;
    long size;
    char *data;
    size_t n, offset = 0, i;
    ttf_status_t st;
    ttf_engine_t *tmp;
    ttf_resource_diagnostic_t local;
    if (!e || !path || (unsigned)layer > TTF_LAYER_USER ||
        (diagnostic && diagnostic->struct_size != sizeof(*diagnostic)))
        return TTF_EINVAL;
    if (e->frozen || e->validating)
        return TTF_ESTATE;
    if (!diagnostic)
        diagnostic = &local;
    ttf_resource_diagnostic_init(diagnostic);
    ttf_validation_copy(diagnostic->path, sizeof(diagnostic->path), path);
    diagnostic->layer = layer;
    f = fopen(path, "rb");
    if (!f) {
        (void)ttf_validation_error(diagnostic, "Cannot open resource file");
        return TTF_EIO;
    }
    if (fseek(f, 0, SEEK_END) || ((size = ftell(f)) < 0) || size > (long)RESOURCE_LIMIT ||
        fseek(f, 0, SEEK_SET)) {
        fclose(f);
        return ttf_validation_error(diagnostic, "Resource size is invalid or exceeds 16 MiB");
    }
    n = (size_t)size;
    data = (char *)malloc(n + 1);
    if (!data) {
        fclose(f);
        return TTF_ENOMEM;
    }
    if (fread(data, 1, n, f) != n) {
        fclose(f);
        free(data);
        (void)ttf_validation_error(diagnostic, "Cannot read resource file");
        return TTF_EIO;
    }
    fclose(f);
    data[n] = 0;
    if (n >= 4 && !memcmp(data, "TTFB", 4)) {
        uint32_t h = 2166136261u;
        if (n < 16 || memcmp(data, "TTFB\1\0\0\0", 8) ||
            read_u32((unsigned char *)data + 8) != n - 16) {
            free(data);
            return ttf_validation_error(diagnostic, "Invalid TTFB version or payload size");
        }
        for (i = 16; i < n; i++) {
            h ^= (unsigned char)data[i];
            h *= 16777619u;
        }
        if (h != read_u32((unsigned char *)data + 12)) {
            free(data);
            return ttf_validation_error(diagnostic, "TTFB checksum mismatch");
        }
        offset = 16;
    }
    tmp = (ttf_engine_t *)calloc(1, sizeof(*tmp));
    if (!tmp) {
        free(data);
        return TTF_ENOMEM;
    }
    e->validating = 1;
    st = parse_tsv(tmp, e, data + offset, n - offset, layer, diagnostic);
    e->validating = 0;
    free(data);
    if (st == TTF_OK && tmp->resource_order > SIZE_MAX - e->resource_order)
        st = TTF_ELIMIT;
    if (st == TTF_OK) {
        for (i = 0; i < TTF_HASH_BUCKETS; i++) {
            ttf_entry_t *p = tmp->resources[i];
            while (p) {
                ttf_entry_t *next = p->next;
                p->order += e->resource_order;
                p->next = e->resources[i];
                e->resources[i] = p;
                p = next;
            }
            tmp->resources[i] = NULL;
        }
        e->resource_order += tmp->resource_order;
        ttf_resource_diagnostic_init(diagnostic);
    }
    ttf_resource_destroy(tmp);
    free(tmp);
    return st;
}
static ttf_status_t lexkey(const char *source, const char *word, char **out) {
    size_t a = strlen(source), b = strlen(word), i;
    char *p;
    *out = NULL;
    if (a > SIZE_MAX - b - 2)
        return TTF_ELIMIT;
    p = (char *)malloc(a + b + 2);
    if (!p)
        return TTF_ENOMEM;
    memcpy(p, source, a);
    p[a] = ':';
    memcpy(p + a + 1, word, b + 1);
    for (i = a + 1; i < a + b + 1; i++)
        if (p[i] >= 'A' && p[i] <= 'Z')
            p[i] += 'a' - 'A';
    {
        char *canonical = NULL;
        ttf_status_t status = ttf_lexicon_canonical_key(p, &canonical);
        free(p);
        *out = canonical;
        return status;
    }
}
static void remove_key(ttf_engine_t *e, ttf_resource_layer_t layer, const char *render,
                       const char *key) {
    size_t h = bucket("lexicon", render, key);
    ttf_entry_t **at = &e->resources[h];
    while (*at) {
        ttf_entry_t *p = *at;
        if (p->layer == layer && !strcmp(p->kind, "lexicon") && !strcmp(p->lang, render) &&
            !strcmp(p->key, key)) {
            *at = p->next;
            entry_free(p);
        } else
            at = &p->next;
    }
}
ttf_status_t ttf_lexicon_remove(ttf_engine_t *e, ttf_resource_layer_t layer, const char *source,
                                const char *render, const char *word) {
    char *key;
    ttf_status_t status;
    if (!e || !source || !*source || !render || !*render || !word || !*word ||
        (unsigned)layer > TTF_LAYER_USER)
        return TTF_EINVAL;
    if (e->frozen || e->validating)
        return TTF_ESTATE;
    status = lexkey(source, word, &key);
    if (status != TTF_OK)
        return status;
    remove_key(e, layer, render, key);
    free(key);
    return TTF_OK;
}
ttf_status_t ttf_lexicon_add(ttf_engine_t *e, ttf_resource_layer_t layer, const char *source,
                             const char *render, const char *word, const char *pron) {
    char *key;
    ttf_status_t st;
    ttf_token_t *tokens = NULL;
    size_t count = 0;
    if (!e || !source || !*source || !render || !*render || !word || !pron || !*word || !*pron ||
        strlen(source) >= TTF_LANGUAGE_TAG_SIZE || strlen(render) >= TTF_LANGUAGE_TAG_SIZE ||
        (unsigned)layer > TTF_LAYER_USER)
        return TTF_EINVAL;
    if (e->frozen || e->validating)
        return TTF_ESTATE;
    if (e->resource_order == SIZE_MAX)
        return TTF_ELIMIT;
    st = ttf_tokenize(pron, strlen(pron), &tokens, &count);
    free(tokens);
    if (st != TTF_OK)
        return st;
    st = ttf_tokenize(word, strlen(word), &tokens, &count);
    free(tokens);
    if (st != TTF_OK)
        return st;
    st = lexkey(source, word, &key);
    if (st != TTF_OK)
        return st;
    /* Allocate the replacement before removing the previous value. */
    {
        ttf_engine_t tmp;
        ttf_entry_t *p;
        size_t h;
        memset(&tmp, 0, sizeof(tmp));
        st = ttf_resource_set(&tmp, "lexicon", render, key, pron, layer);
        if (st != TTF_OK) {
            free(key);
            return st;
        }
        remove_key(e, layer, render, key);
        h = bucket("lexicon", render, key);
        p = tmp.resources[h];
        p->order = e->resource_order++;
        p->next = e->resources[h];
        e->resources[h] = p;
    }
    free(key);
    return st;
}
