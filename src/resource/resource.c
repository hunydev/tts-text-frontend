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
uint32_t ttf_entry_hash(const char *kind, const char *lang, const char *key);
#define entry_hash ttf_entry_hash
uint32_t ttf_entry_hash(const char *kind, const char *lang, const char *key) {
    uint32_t h = hash_bytes(2166136261u, kind);
    h = hash_bytes(h, "\t");
    h = hash_bytes(h, lang);
    h = hash_bytes(h, "\t");
    return hash_bytes(h, key);
}
/* A row and its four strings are one allocation (2026-10-25: a million rows took 286 MB as
   five allocations each, and freeze walked them scattered). */
static void entry_free(ttf_entry_t *p) {
    free(p);
}
static ttf_status_t resource_insert(ttf_engine_t *e, const char *kind, const char *lang, const char *key,
                                   const char *value, ttf_resource_layer_t layer, ttf_entry_t **inserted) {
    size_t h;
    ttf_entry_t *p;
    if (e->resource_order == SIZE_MAX)
        return TTF_ELIMIT;
    if (!e->resources &&
        !(e->resources = (ttf_entry_t **)calloc(ttf_buckets(e), sizeof(*e->resources))))
        return TTF_ENOMEM;
    if (ttf_rule_resource_kind(kind) && !e->rule_table) {
        if (!(e->rule_table = (ttf_entry_t **)calloc(TTF_RULE_BUCKETS, sizeof(*e->rule_table))))
            return TTF_ENOMEM;
        e->rule_buckets = TTF_RULE_BUCKETS;
    }
    {
        char *canonical = NULL, *at;
        size_t kl = strlen(kind) + 1, ll = strlen(lang) + 1, keyl, vl = strlen(value) + 1;
        if (!strcmp(kind, "lexicon")) {
            ttf_status_t key_status = ttf_lexicon_canonical_key(key, &canonical);
            if (key_status != TTF_OK) {
                free(canonical);
                return key_status == TTF_EINVAL ? TTF_ERESOURCE : key_status;
            }
            key = canonical;
        }
        keyl = strlen(key) + 1;
        p = (ttf_entry_t *)calloc(1, sizeof(*p) + kl + ll + keyl + vl);
        if (!p) {
            free(canonical);
            return TTF_ENOMEM;
        }
        at = (char *)(p + 1);
        p->kind = memcpy(at, kind, kl);
        p->lang = memcpy(at += kl, lang, ll);
        p->key = memcpy(at += ll, key, keyl);
        p->value = memcpy(at + keyl, value, vl);
        free(canonical);
    }
    p->layer = layer;
    p->order = e->resource_order++;
    p->hash = entry_hash(kind, lang, p->key);
    if (ttf_rule_resource_kind(kind)) {
        h = p->hash % e->rule_buckets;
        p->next = e->rule_table[h];
        e->rule_table[h] = p;
        ++e->rule_rows;
    } else {
        h = p->hash % ttf_buckets(e);
        p->next = e->resources[h];
        e->resources[h] = p;
    }
    if (e->staging) {
        p->load_next = e->staged;
        e->staged = p;
    }
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
    uint32_t full = entry_hash(kind, lang, key);
    size_t h = full % ttf_buckets(e);
    p = e->rule_table && ttf_rule_resource_kind(kind) ? e->rule_table[full % e->rule_buckets]
        : e->cjk && ttf_cjk_resource_kind(kind) ? e->cjk->resources[h]
        : e->resources                         ? e->resources[h]
                                               : NULL;
    for (; p; p = p->next) {
        if (p->hash != full || strcmp(kind, p->kind) || strcmp(lang, p->lang) || strcmp(key, p->key))
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
const ttf_entry_t *ttf_resource_chain(const ttf_engine_t *e, uint32_t hash) {
    if (e->rule_table)
        return e->rule_table[hash % e->rule_buckets];
    return e->resources ? e->resources[hash % ttf_buckets(e)] : NULL;
}
static size_t table_size(size_t rows) {
    static const size_t primes[] = {1021, 8191, 65521, 131071, 262139, 524287, 1048573, 2097143, 4194301,
                                    8388593, 16777213, 33554393};
    size_t i;
    for (i = 0; i + 1 < sizeof(primes) / sizeof(*primes) && primes[i] < rows; ++i)
        ;
    return primes[i];
}
ttf_status_t ttf_resource_resize_rules(ttf_engine_t *e) {
    size_t i, size;
    ttf_entry_t **table, *p, *next;
    if (!e->rule_table || e->rule_rows <= 2 * e->rule_buckets)
        return TTF_OK;
    size = table_size(e->rule_rows);
    table = (ttf_entry_t **)calloc(size, sizeof(*table));
    if (!table)
        return TTF_OK; /* longer chains, same rows */
    for (i = 0; i < e->rule_buckets; ++i)
        for (p = e->rule_table[i]; p; p = next) {
            next = p->next;
            p->next = table[p->hash % size];
            table[p->hash % size] = p;
        }
    free(e->rule_table);
    e->rule_table = table;
    e->rule_buckets = size;
    return TTF_OK;
}
ttf_status_t ttf_resource_resize(ttf_engine_t *e) {
    static const size_t primes[] = {65521, 131071, 262139, 524287, 1048573, 2097143, 4194301, 8388593,
                                    16777213, 33554393};
    size_t rows = 0, i, size = ttf_buckets(e);
    ttf_entry_t **table, *p, *next;
    if (!e->resources)
        return TTF_OK;
    for (i = 0; i < ttf_buckets(e); ++i)
        for (p = e->resources[i]; p; p = p->next)
            ++rows;
    for (i = 0; i < sizeof(primes) / sizeof(*primes) && primes[i] < rows; ++i)
        ;
    if (i < sizeof(primes) / sizeof(*primes))
        size = primes[i] > size ? primes[i] : size;
    else
        size = primes[i - 1];
    if (size < ttf_buckets(e))
        size = ttf_buckets(e);
    if (size == ttf_buckets(e))
        return TTF_OK;
    table = (ttf_entry_t **)calloc(size, sizeof(*table));
    if (!table)
        return TTF_ENOMEM;
    for (i = 0; i < ttf_buckets(e); ++i)
        for (p = e->resources[i]; p; p = next) {
            next = p->next;
            p->next = table[p->hash % size];
            table[p->hash % size] = p;
        }
    free(e->resources);
    e->resources = table;
    e->resource_buckets = size;
    return TTF_OK;
}
uint32_t ttf_kind_slot(const char *kind, const char *lang) {
    uint32_t h = hash_bytes(2166136261u, kind);
    h = hash_bytes(h, "\t");
    return hash_bytes(h, lang) % TTF_KIND_SLOTS;
}
static uint64_t kind_lengths(const ttf_engine_t *e, const char *kind, const char *lang) {
    uint32_t i = ttf_kind_slot(kind, lang), probes;
    for (probes = 0; probes < TTF_KIND_SLOTS; ++probes, i = (i + 1) % TTF_KIND_SLOTS) {
        const struct ttf_kind_lengths *slot = &e->kind_lengths[i];
        if (!slot->kind)
            return 0;
        if (!strcmp(slot->kind, kind) && !strcmp(slot->lang, lang))
            return slot->lengths;
    }
    return 0;
}
uint64_t ttf_resource_key_lengths(const ttf_engine_t *e, const char *kind, const char *lang) {
    uint64_t lengths;
    if (!e->frozen || !e->kind_lengths)
        return ~(uint64_t)0;
    lengths = kind_lengths(e, kind, lang);
    if (strcmp(lang, "*"))
        lengths |= kind_lengths(e, kind, "*");
    return lengths;
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
    for (table = 0; table < (e->cjk ? 2u : 1u); ++table) for (i = 0; i < ttf_buckets(e); i++) {
        ttf_entry_t **head;
        if (!table && !e->resources)
            break;
        head = table ? &e->cjk->resources[i] : &e->resources[i];
        ttf_entry_t *p = *head;
        while (p) {
            ttf_entry_t *n = p->next;
            entry_free(p);
            p = n;
        }
        *head = NULL;
    }
    for (i = 0; e->rule_table && i < e->rule_buckets; i++) {
        ttf_entry_t *p = e->rule_table[i], *n;
        for (; p; p = n) {
            n = p->next;
            entry_free(p);
        }
    }
    free(e->rule_table);
    e->rule_table = NULL;
    free(e->resources);
    e->resources = NULL;
    if (e->cjk)
        free(e->cjk->resources);
    free(e->cjk); /* Word buckets borrow rows; never free their entries again. */
    e->cjk = NULL;
}
void ttf_resource_rollback(ttf_engine_t *e, size_t checkpoint) {
    size_t i;
    for (i = 0; e->rule_table && i < e->rule_buckets; ++i) {
        ttf_entry_t **at = &e->rule_table[i];
        while (*at) {
            ttf_entry_t *entry = *at;
            if (entry->order < checkpoint) {
                at = &entry->next;
            } else {
                *at = entry->next;
                entry_free(entry);
                --e->rule_rows;
            }
        }
    }
    for (i = 0; e->resources && i < ttf_buckets(e); ++i) {
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
    if (tmp)
        tmp->staging = 1;
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
    if (st == TTF_OK && !e->resources &&
        !(e->resources = (ttf_entry_t **)calloc(ttf_buckets(e), sizeof(*e->resources))))
        st = TTF_ENOMEM;
    if (st == TTF_OK) {
        ttf_entry_t *p = tmp->staged, *next;
        if (tmp->rule_table && !e->rule_table) {
            if (!(e->rule_table = (ttf_entry_t **)calloc(TTF_RULE_BUCKETS, sizeof(*e->rule_table))))
                st = TTF_ENOMEM;
            else
                e->rule_buckets = TTF_RULE_BUCKETS;
        }
        for (; st == TTF_OK && p; p = next) {
            next = p->load_next;
            p->load_next = NULL;
            p->order += e->resource_order;
            if (ttf_rule_resource_kind(p->kind)) {
                i = p->hash % e->rule_buckets;
                p->next = e->rule_table[i];
                e->rule_table[i] = p;
                ++e->rule_rows;
            } else {
                i = p->hash % ttf_buckets(e);
                p->next = e->resources[i];
                e->resources[i] = p;
            }
        }
    }
    if (st == TTF_OK) {
        tmp->staged = NULL;
        free(tmp->resources); /* every row moved: nothing left to walk */
        tmp->resources = NULL;
        free(tmp->rule_table);
        tmp->rule_table = NULL;
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
    size_t h = entry_hash("lexicon", render, key) % ttf_buckets(e);
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
        p = tmp.resources[entry_hash("lexicon", render, key) % ttf_buckets(&tmp)];
        free(tmp.resources);
        h = p->hash % ttf_buckets(e);
        p->order = e->resource_order++;
        p->next = e->resources[h];
        e->resources[h] = p;
    }
    free(key);
    return st;
}
