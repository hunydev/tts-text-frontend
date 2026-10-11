#include "internal.h"
#include "../unicode_fallback/fallback.h"
#include "../resource/validation.h"
#include "../cjk/cjk_reading.h"
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
    if (e->frozen || e->validating)
        return TTF_ESTATE;
    if (ttf_find_pack(e, p->language_tag))
        return TTF_EINVAL;
    if (e->pack_count == TTF_MAX_PACKS)
        return TTF_ELIMIT;
    e->packs[e->pack_count++] = p;
    return TTF_OK;
}
static ttf_status_t load_manifest(ttf_engine_t *e, ttf_resource_diagnostic_t *d, int deferred) {
    char path[4096], manifest[4096], line[2048];
    size_t line_number = 0;
    FILE *f;
    ttf_status_t st = TTF_OK;
    if (snprintf(manifest, sizeof(manifest), "%s/manifest.ttfm", e->resource_root) >=
        (int)sizeof(manifest)) {
        (void)ttf_validation_error(d, "Manifest path exceeds the path limit");
        return TTF_ELIMIT;
    }
    ttf_validation_copy(d->path, sizeof(d->path), manifest);
    f = fopen(manifest, "rb");
    if (!f) {
        (void)ttf_validation_error(d, "Cannot open resource manifest");
        return TTF_EIO;
    }
    for (;;) {
        size_t n = 0, at;
        int byte;
        ttf_resource_diagnostic_init(d);
        ttf_validation_copy(d->path, sizeof(d->path), manifest);
        d->line = line_number + 1;
        /* Read bounded bytes explicitly: fgets plus strlen would silently hide
           an embedded NUL and could load a different path than the file shows. */
        while ((byte = fgetc(f)) != EOF && byte != '\n') {
            if (!byte) {
                st = ttf_validation_error(d, "Manifest must not contain embedded NUL");
                break;
            }
            if (n + 1 >= sizeof(line)) {
                st = ttf_validation_error(d, "Manifest line exceeds the line limit");
                break;
            }
            line[n++] = (char)byte;
        }
        if (st != TTF_OK || (byte == EOF && !n))
            break;
        ++line_number;
        if (n && line[n - 1] == '\r')
            --n;
        line[n] = 0;
        for (at = 0; at < n;) {
            uint32_t cp;
            size_t width;
            if (ttf_utf8_decode(line + at, n - at, &cp, &width) != 1 || cp < 32 || cp == 127) {
                st = ttf_validation_error(d,
                                          "Manifest line must be UTF-8 without control characters");
                break;
            }
            at += width;
        }
        if (st != TTF_OK)
            break;
        if (!n || line[0] == '#')
            continue;
        if (line[0] == '/' || strstr(line, "..") || strchr(line, '\\') || strchr(line, ':')) {
            st = ttf_validation_error(d, "Manifest requires relative paths without traversal");
            break;
        }
        /* Keep every path in the manifest for offline tools. Disabled CJK
           tables are never opened, including by legacy engines. */
        if (!ttf_cjk_manifest_selected(e, line, deferred))
            continue;
        if (snprintf(path, sizeof(path), "%s/%s", e->resource_root, line) >= (int)sizeof(path)) {
            (void)ttf_validation_error(d, "Manifest resource path exceeds the path limit");
            st = TTF_ELIMIT;
            break;
        }
        st = ttf_resource_load_ex(
            e, path, !strncmp(line, "common/", 7) ? TTF_LAYER_COMMON : TTF_LAYER_SYSTEM, d);
        if (st != TTF_OK)
            break;
    }
    if (ferror(f)) {
        ttf_resource_diagnostic_init(d);
        ttf_validation_copy(d->path, sizeof(d->path), manifest);
        d->line = line_number;
        (void)ttf_validation_error(d, "Cannot read resource manifest");
        st = TTF_EIO;
    }
    fclose(f);
    if (st == TTF_OK)
        ttf_resource_diagnostic_init(d);
    return st;
}
ttf_status_t ttf_engine_create(const ttf_config_t *cfg, ttf_engine_t **out) {
    return ttf_engine_create_ex(cfg, out, NULL);
}
ttf_status_t ttf_engine_create_ex(const ttf_config_t *cfg, ttf_engine_t **out,
                                  ttf_resource_diagnostic_t *diagnostic) {
    ttf_config_t defaults;
    ttf_resource_diagnostic_t local;
    ttf_engine_t *e;
    ttf_status_t st;
    if (!out)
        return TTF_EINVAL;
    *out = NULL;
    if (diagnostic && diagnostic->struct_size != sizeof(*diagnostic))
        return TTF_EINVAL;
    if (!diagnostic)
        diagnostic = &local;
    ttf_resource_diagnostic_init(diagnostic);
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
    /* An engine always owns its resource table, even with no rows. */
    e->resources = (ttf_entry_t **)calloc(TTF_RESOURCE_BUCKETS, sizeof(*e->resources));
    if (!e->resources) {
        free(e);
        return TTF_ENOMEM;
    }
    e->config = *cfg;
    ttf_frontend_options_init(&e->frontend_options);
    e->frontend_options.output_mode = TTF_OUTPUT_TEXT;
    e->frontend_options.unreadable_policy = TTF_UNREADABLE_PRESERVE;
    e->resource_root =
        ttf_strdup(cfg->resource_root ? cfg->resource_root : TTF_DEFAULT_RESOURCE_ROOT);
    e->default_language = ttf_strdup(cfg->default_language);
    if (!e->resource_root || !e->default_language) {
        ttf_engine_destroy(e);
        return TTF_ENOMEM;
    }
    e->config.resource_root = e->resource_root;
    e->config.default_language = e->default_language;
    st = ttf_engine_register_resource_validator(e, &ttf_resource_validator_core);
    if (st == TTF_OK)
        st = ttf_register_builtins(e);
    e->builtin_validators = e->validator_count;
    if (st == TTF_OK)
        st = load_manifest(e, diagnostic, 0);
    if (st != TTF_OK) {
        ttf_engine_destroy(e);
        return st;
    }
    *out = e;
    return TTF_OK;
}
ttf_status_t ttf_engine_freeze(ttf_engine_t *e) {
    size_t bucket, checkpoint;
    ttf_status_t status;
    ttf_resource_diagnostic_t diagnostic;
    if (!e)
        return TTF_EINVAL;
    if (e->validating)
        return TTF_ESTATE;
    if (e->frozen)
        return TTF_OK;
    if (!ttf_find_pack(e, e->default_language))
        return TTF_EINVAL;
    checkpoint = e->resource_order;
    status = TTF_OK;
    if (e->frontend_options.hanja == TTF_HANJA_KOREAN ||
        e->frontend_options.kana == TTF_KANA_KOREAN || e->phoneme_output ||
        e->frontend_options.input_format == TTF_INPUT_ANNOTATED) {
        ttf_resource_diagnostic_init(&diagnostic);
        status = load_manifest(e, &diagnostic, 1);
    }
    if (status == TTF_OK)
        status = ttf_engine_validate_resources(e, NULL, NULL);
    /* Map the Unicode fallback files before the last fallible allocation. */
    if (status == TTF_OK)
        status = ttf_symbol_fallback_attach(e);
    /* Allocate/detach only after all files and engine validators succeed.
       Index allocation is the last fallible operation; on failure no rows
       have moved and the complete deferred load can be rolled back. */
    if (status == TTF_OK)
        status = ttf_cjk_index(e);
    if (status != TTF_OK) {
        ttf_symbol_fallback_detach(e);
        ttf_resource_rollback(e, checkpoint);
        return status;
    }
    /* Build immutable indexes before sharing the engine. Rules and boundary
       cues must not scan pronunciation dictionaries during normalization. */
    e->rules = NULL;
    e->boundary_cues = NULL;
    for (bucket = 0; bucket < TTF_RESOURCE_BUCKETS; ++bucket) {
        ttf_entry_t *entry;
        for (entry = e->resources[bucket]; entry; entry = entry->next) {
            int active = 0;
            size_t pack;
            entry->rule_next = NULL;
            entry->boundary_next = NULL;
            if (!strcmp(entry->kind, "exact") || !strcmp(entry->kind, "pattern")) {
                entry->rule_next = e->rules;
                e->rules = entry;
            }
            /* Most resources, especially large lexicons, cannot be boundary
               cues. Avoid effective-value lookups for those entries. */
            if (strcmp(entry->kind, "identifier_label") && strcmp(entry->kind, "context_cue") &&
                strcmp(entry->kind, "phone_label"))
                continue;
            if (strcmp(entry->lang, "*") && !ttf_find_pack(e, entry->lang))
                continue;
            if (ttf_resource_get(e, entry->kind, entry->lang, entry->key, NULL) != entry->value)
                continue;
            if (!strcmp(entry->lang, "*")) {
                for (pack = 0; pack < e->pack_count; ++pack)
                    if (ttf_resource_get(e, entry->kind, e->packs[pack]->language_tag, entry->key,
                                         NULL) == entry->value)
                        break;
                if (pack == e->pack_count)
                    continue;
            }
            if (!strcmp(entry->kind, "identifier_label"))
                /* A disabled longer cue still blocks an enabled suffix. Keep
                   its scope intact until classification resolves precedence. */
                active = !strcmp(entry->value, "digits") || !strcmp(entry->value, "disabled");
            else if (!strcmp(entry->kind, "phone_label"))
                active = !strcmp(entry->value, "1") || !strcmp(entry->value, "0");
            else if (!strcmp(entry->kind, "context_cue")) {
                char *end;
                long score = strtol(entry->value, &end, 10);
                active = *end == '\0' && score > 0 && score <= 1000;
            }
            if (active) {
                entry->boundary_next = e->boundary_cues;
                e->boundary_cues = entry;
            }
        }
    }
    ttf_phrase_index(e);
    for (bucket = 0; bucket < e->pack_count; ++bucket) {
        const char *tag = e->packs[bucket]->language_tag,
                   *flag = ttf_resource_get(e, "context", tag, "declared_only", NULL);
        e->undeclared[bucket] = flag && strcmp(flag, "0") && strcmp(e->default_language, tag);
        flag = ttf_resource_get(e, "context", tag, "han_sentence", NULL);
        e->han_only[bucket] = flag && strcmp(flag, "0");
    }
    e->frozen = 1;
    return TTF_OK;
}
void ttf_engine_destroy(ttf_engine_t *e) {
    if (e) {
        ttf_resource_destroy(e);
        ttf_symbol_fallback_detach(e);
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
