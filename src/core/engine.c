#include "internal.h"
#ifndef TTF_DEFAULT_RESOURCE_ROOT
#define TTF_DEFAULT_RESOURCE_ROOT "resources"
#endif
void ttf_config_init(ttf_config_t *c) {
    if (!c)
        return;
    memset(c, 0, sizeof(*c));
    c->struct_size = sizeof(*c);
    c->abi_version = TTF_ABI_VERSION;
    c->default_language = "ko-KR";
    c->render_policy = TTF_PHONETIC_LOCALIZE;
    c->first_segment_target = 48;
    c->normal_segment_target = 112;
    c->max_segment_length = 200;
    c->max_pending_bytes = 1024 * 1024;
    c->sentence_split = 1;
    c->phrase_split = 1;
}
const ttf_language_pack_t *ttf_find_pack(const ttf_engine_t *e, const char *tag) {
    size_t i;
    for (i = 0; i < e->pack_count; i++)
        if (!strcmp(e->packs[i]->language_tag, tag))
            return e->packs[i];
    return NULL;
}
ttf_status_t ttf_engine_register_pack(ttf_engine_t *e, const ttf_language_pack_t *p) {
    if (!e || !p || p->struct_size != sizeof(*p) || p->abi_version != TTF_ABI_VERSION ||
        !p->language_tag || strlen(p->language_tag) >= TTF_LANGUAGE_TAG_SIZE || !p->verbalize)
        return TTF_EINVAL;
    if (e->frozen)
        return TTF_ESTATE;
    if (ttf_find_pack(e, p->language_tag))
        return TTF_EINVAL;
    if (e->pack_count == TTF_MAX_PACKS)
        return TTF_ELIMIT;
    e->packs[e->pack_count++] = p;
    return TTF_OK;
}
static ttf_status_t load_manifest(ttf_engine_t *e) {
    char path[4096], line[2048];
    FILE *f;
    ttf_status_t st = TTF_OK;
    if (snprintf(path, sizeof(path), "%s/manifest.ttfm", e->resource_root) >= (int)sizeof(path))
        return TTF_ELIMIT;
    f = fopen(path, "rb");
    if (!f)
        return TTF_EIO;
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        if (n == sizeof(line) - 1 && line[n - 1] != '\n') {
            st = TTF_ERESOURCE;
            break;
        }
        while (n && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = 0;
        if (!n || line[0] == '#')
            continue;
        if (line[0] == '/' || strstr(line, "..") || strchr(line, '\\') || strchr(line, ':')) {
            st = TTF_ERESOURCE;
            break;
        }
        if (snprintf(path, sizeof(path), "%s/%s", e->resource_root, line) >= (int)sizeof(path)) {
            st = TTF_ELIMIT;
            break;
        }
        st = ttf_resource_load(e, path,
                               !strncmp(line, "common/", 7) ? TTF_LAYER_COMMON : TTF_LAYER_SYSTEM);
        if (st != TTF_OK)
            break;
    }
    if (ferror(f))
        st = TTF_EIO;
    fclose(f);
    return st;
}
ttf_status_t ttf_engine_create(const ttf_config_t *cfg, ttf_engine_t **out) {
    ttf_config_t defaults;
    ttf_engine_t *e;
    ttf_status_t st;
    if (!out)
        return TTF_EINVAL;
    *out = NULL;
    if (!cfg) {
        ttf_config_init(&defaults);
        cfg = &defaults;
    }
    if (cfg->struct_size != sizeof(*cfg) || cfg->abi_version != TTF_ABI_VERSION ||
        !cfg->default_language || !cfg->first_segment_target || !cfg->normal_segment_target ||
        !cfg->max_segment_length || !cfg->max_pending_bytes ||
        (unsigned)cfg->render_policy > TTF_SEMANTIC_LOCALIZE)
        return TTF_EINVAL;
    e = (ttf_engine_t *)calloc(1, sizeof(*e));
    if (!e)
        return TTF_ENOMEM;
    e->config = *cfg;
    e->resource_root =
        ttf_strdup(cfg->resource_root ? cfg->resource_root : TTF_DEFAULT_RESOURCE_ROOT);
    e->default_language = ttf_strdup(cfg->default_language);
    if (!e->resource_root || !e->default_language) {
        ttf_engine_destroy(e);
        return TTF_ENOMEM;
    }
    e->config.resource_root = e->resource_root;
    e->config.default_language = e->default_language;
    st = ttf_register_builtins(e);
    if (st == TTF_OK)
        st = load_manifest(e);
    if (st != TTF_OK) {
        ttf_engine_destroy(e);
        return st;
    }
    *out = e;
    return TTF_OK;
}
ttf_status_t ttf_engine_freeze(ttf_engine_t *e) {
    if (!e)
        return TTF_EINVAL;
    if (!ttf_find_pack(e, e->default_language))
        return TTF_EINVAL;
    e->frozen = 1;
    return TTF_OK;
}
void ttf_engine_destroy(ttf_engine_t *e) {
    if (e) {
        ttf_resource_destroy(e);
        free(e->resource_root);
        free(e->default_language);
        free(e);
    }
}
void ttf_result_free(ttf_result_t *r) {
    if (r) {
        free(r->text);
        free(r->spans);
        memset(r, 0, sizeof(*r));
    }
}
