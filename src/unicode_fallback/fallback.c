#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "fallback.h"
#include <errno.h>
#include "../semantic/emoji_ranges.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

ttf_status_t ttf_foreign_render(const ttf_engine_t *, const ttf_language_pack_t *,
                                const ttf_language_pack_t *, const char *, size_t,
                                ttf_lexical_class_t, ttf_buffer_t *, char[64],
                                ttf_resource_layer_t *);

#define HEADER 16u
#define KEY_BYTES 60u
#define RECORD (KEY_BYTES + 4u)

typedef struct {
    const unsigned char *data;
    size_t size;
} mapping_t;
struct ttf_symbol_fallback {
    mapping_t idx, dat, seq;
    uint32_t limit, count;
};

static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

/* 1 mapped, 0 absent, -1 present but unusable. */
static int map_file(const char *path, mapping_t *m) {
#ifdef _WIN32
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, NULL), mapping;
    LARGE_INTEGER size;
    void *view;
    if (file == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND
                   ? 0 : -1;
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 ||
        (unsigned long long)size.QuadPart > (size_t)-1) {
        CloseHandle(file);
        return -1;
    }
    mapping = CreateFileMappingA(file, NULL, PAGE_READONLY, 0, 0, NULL);
    CloseHandle(file);
    if (!mapping)
        return -1;
    view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(mapping);
    if (!view)
        return -1;
    m->data = (const unsigned char *)view;
    m->size = (size_t)size.QuadPart;
    return 1;
#else
    struct stat st;
    void *view;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return errno == ENOENT ? 0 : -1;
    if (fstat(fd, &st) != 0 || st.st_size <= 0 || (unsigned long long)st.st_size > (size_t)-1) {
        close(fd);
        return -1;
    }
    view = mmap(NULL, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (view == MAP_FAILED)
        return -1;
    m->data = (const unsigned char *)view;
    m->size = (size_t)st.st_size;
    return 1;
#endif
}
static void unmap_file(mapping_t *m) {
    if (!m->data)
        return;
#ifdef _WIN32
    UnmapViewOfFile((void *)m->data);
#else
    munmap((void *)m->data, m->size);
#endif
    m->data = NULL;
}

void ttf_symbol_fallback_close(ttf_symbol_fallback_t *f) {
    if (f) {
        unmap_file(&f->idx);
        unmap_file(&f->dat);
        unmap_file(&f->seq);
        free(f);
    }
}

static int open_triple(const char *root, const char *tag, ttf_symbol_fallback_t *f) {
    static const char *const names[3] = {"idx", "dat", "seq"};
    mapping_t *maps[3];
    char path[4096];
    int k, present;
    maps[0] = &f->idx;
    maps[1] = &f->dat;
    maps[2] = &f->seq;
    for (k = 0; k < 3; ++k) {
        if (snprintf(path, sizeof(path), "%s/unicode-fallback/%s.%s", root, tag, names[k]) >=
            (int)sizeof(path))
            return -1;
        present = map_file(path, maps[k]);
        if (present <= 0)
            return k == 0 && present == 0 ? 0 : -1; /* a missing .idx: no files */
    }
    return 1;
}

/* Headers, sizes and the pool terminator are checked once here, so a lookup
   only bounds its offset: every in-range offset ends at a NUL in the pool. */
static int valid(const ttf_symbol_fallback_t *f) {
    uint32_t id;
    if (f->idx.size < HEADER || f->dat.size <= HEADER || f->seq.size < HEADER ||
        memcmp(f->idx.data, "TTFUFIX1", 8) || memcmp(f->dat.data, "TTFUFDT1", 8) ||
        memcmp(f->seq.data, "TTFUFSQ1", 8))
        return 0;
    id = le32(f->idx.data + 8);
    if (le32(f->dat.data + 8) != id || le32(f->seq.data + 8) != id)
        return 0;
    if (f->limit > 0x110000u || f->idx.size != HEADER + (size_t)f->limit * 4u ||
        f->dat.size != HEADER + (size_t)le32(f->dat.data + 12) ||
        f->seq.size != HEADER + (size_t)f->count * RECORD || f->dat.data[f->dat.size - 1] != 0)
        return 0;
    return 1;
}

ttf_status_t ttf_symbol_fallback_open(const char *root, const char *tag,
                                      ttf_symbol_fallback_t **out) {
    char language[TTF_LANGUAGE_TAG_SIZE];
    ttf_symbol_fallback_t *f;
    int found;
    size_t cut;
    *out = NULL;
    if (!root || !tag)
        return TTF_OK;
    f = (ttf_symbol_fallback_t *)calloc(1, sizeof(*f));
    if (!f)
        return TTF_ENOMEM;
    /* ko-KR uses ko.* when there is no region-specific triple. */
    found = open_triple(root, tag, f);
    cut = strcspn(tag, "-_");
    if (!found && tag[cut] && cut < sizeof(language)) {
        memcpy(language, tag, cut);
        language[cut] = 0;
        found = open_triple(root, language, f);
    }
    if (found > 0) {
        f->limit = le32(f->idx.data + 12);
        f->count = le32(f->seq.data + 12);
    }
    if (found <= 0 || !valid(f)) {
        ttf_symbol_fallback_close(f);
        return found == 0 ? TTF_OK : TTF_ERESOURCE;
    }
    *out = f;
    return TTF_OK;
}

static const char *pooled(const ttf_symbol_fallback_t *f, uint32_t offset) {
    return offset >= HEADER && offset < f->dat.size ? (const char *)f->dat.data + offset : NULL;
}
const char *ttf_symbol_fallback_single(const ttf_symbol_fallback_t *f, uint32_t cp) {
    if (!f || cp >= f->limit)
        return NULL;
    return pooled(f, le32(f->idx.data + HEADER + (size_t)cp * 4u));
}
const char *ttf_symbol_fallback_sequence(const ttf_symbol_fallback_t *f, const char *key,
                                         size_t length) {
    unsigned char padded[KEY_BYTES];
    size_t low = 0, high;
    if (!f || !length || length > KEY_BYTES)
        return NULL;
    memset(padded, 0, sizeof(padded));
    memcpy(padded, key, length);
    high = f->count;
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        const unsigned char *record = f->seq.data + HEADER + middle * RECORD;
        int order = memcmp(record, padded, KEY_BYTES);
        if (!order)
            return pooled(f, le32(record + KEY_BYTES));
        if (order < 0)
            low = middle + 1;
        else
            high = middle;
    }
    return NULL;
}

ttf_status_t ttf_symbol_fallback_attach(ttf_engine_t *e) {
    size_t j;
    if (e->symbol_fallback_policy == TTF_SYMBOL_FALLBACK_OFF)
        return TTF_OK;
    for (j = 0; j < e->pack_count; ++j) {
        ttf_status_t status = ttf_symbol_fallback_open(e->resource_root,
                                                       e->packs[j]->language_tag,
                                                       &e->symbol_fallback[j]);
        if (status != TTF_OK) {
            ttf_symbol_fallback_detach(e);
            return status;
        }
    }
    return TTF_OK;
}
void ttf_symbol_fallback_detach(ttf_engine_t *e) {
    size_t j;
    for (j = 0; j < TTF_MAX_PACKS; ++j) {
        ttf_symbol_fallback_close(e->symbol_fallback[j]);
        e->symbol_fallback[j] = NULL;
    }
}

static int pictographic(uint32_t cp) {
    size_t low = 0, high = sizeof(ttf_extended_pictographic) / sizeof(ttf_extended_pictographic[0]);
    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (cp < ttf_extended_pictographic[middle][0])
            high = middle;
        else if (cp > ttf_extended_pictographic[middle][1])
            low = middle + 1;
        else
            return 1;
    }
    return 0;
}
static int regional(uint32_t cp) {
    return cp >= 0x1F1E6 && cp <= 0x1F1FF;
}
static int component(uint32_t cp) {
    return cp == 0xFE0E || cp == 0xFE0F || (cp >= 0x1F3FB && cp <= 0x1F3FF) || cp == 0x20E3 ||
           (cp >= 0xE0020 && cp <= 0xE007F);
}
size_t ttf_emoji_cluster_end(const char *s, size_t n, size_t b) {
    uint32_t cp, next;
    size_t width, step, p, scalars = 1;
    if (b >= n || ttf_utf8_decode(s + b, n - b, &cp, &width) != 1)
        return 0;
    p = b + width;
    if (regional(cp))
        return p < n && ttf_utf8_decode(s + p, n - p, &next, &step) == 1 && regional(next)
                   ? p + step
                   : 0;
    if (!pictographic(cp))
        return 0;
    while (p < n && ttf_utf8_decode(s + p, n - p, &next, &step) == 1) {
        size_t after;
        uint32_t joined;
        if (component(next)) {
            p += step;
            ++scalars;
            continue;
        }
        if (next == 0x200D && p + step < n &&
            ttf_utf8_decode(s + p + step, n - p - step, &joined, &after) == 1 &&
            pictographic(joined)) {
            p += step + after;
            scalars += 2;
            continue;
        }
        break;
    }
    return scalars > 1 ? p : 0;
}

static int has_digit(const char *s) {
    for (; *s; ++s)
        if (*s >= '0' && *s <= '9')
            return 1;
    return 0;
}
/* A name is plain text in its own language; digits in it (1st place medal,
   키 캡, 10) are read by one more normalization with the fallback disabled. */
static ttf_status_t read_digits(const ttf_engine_t *e, const ttf_language_pack_t *pack,
                                const char *name, ttf_buffer_t *out) {
    ttf_engine_t nested = *e;
    ttf_result_t result;
    ttf_status_t status;
    nested.trace_state = NULL;
    nested.context_memo = NULL;
    nested.symbol_fallback_policy = TTF_SYMBOL_FALLBACK_OFF;
    nested.default_language = (char *)pack->language_tag;
    nested.frontend_options.output_mode = TTF_OUTPUT_SPEECH;
    status = ttf_normalize(&nested, name, strlen(name), &result);
    if (status == TTF_OK)
        status = ttf_buffer_append(out, result.text, result.length);
    ttf_result_free(&result);
    return status;
}
/* An English name in another language is read word by word as a foreign word. */
static ttf_status_t read_foreign(const ttf_engine_t *e, const ttf_language_pack_t *source,
                                 const ttf_language_pack_t *target, const char *name,
                                 size_t length, ttf_buffer_t *out) {
    size_t i = 0;
    while (i < length) {
        size_t j = i;
        while (j < length && ((name[j] >= 'a' && name[j] <= 'z') ||
                              (name[j] >= 'A' && name[j] <= 'Z') || name[j] == '\''))
            ++j;
        if (j > i) {
            char trace[64] = "";
            ttf_resource_layer_t layer = TTF_LAYER_COMMON;
            ttf_status_t status = ttf_foreign_render(e, source, target, name + i, j - i,
                                                     TTF_LEX_UNKNOWN_LATIN, out, trace, &layer);
            if (status != TTF_OK)
                return status;
            i = j;
        } else {
            ttf_status_t status = ttf_buffer_append(out, name + i, 1);
            if (status != TTF_OK)
                return status;
            ++i;
        }
    }
    return TTF_OK;
}

static size_t pack_slot(const ttf_engine_t *e, const ttf_language_pack_t *pack) {
    size_t j;
    for (j = 0; j < e->pack_count; ++j)
        if (e->packs[j] == pack)
            return j;
    return SIZE_MAX;
}
static const char *lookup(const ttf_symbol_fallback_t *f, const char *key, size_t length,
                          int single, uint32_t cp) {
    return single ? ttf_symbol_fallback_single(f, cp) : ttf_symbol_fallback_sequence(f, key, length);
}
ttf_status_t ttf_symbol_fallback_speak(const ttf_engine_t *e, const ttf_language_pack_t *renderer,
                                       const char *text, size_t length, ttf_buffer_t *out,
                                       int *found) {
    char key[KEY_BYTES + 1];
    size_t at = 0, key_length = 0, scalars = 0, slot, j;
    uint32_t cp, first = 0;
    const char *name = NULL;
    const ttf_language_pack_t *owner = renderer;
    ttf_buffer_t digits = {0};
    ttf_status_t status = TTF_OK;
    *found = 0;
    if (e->symbol_fallback_policy == TTF_SYMBOL_FALLBACK_OFF ||
        (slot = pack_slot(e, renderer)) == SIZE_MAX)
        return TTF_OK;
    /* Keys drop presentation selectors, as the builder and CLDR keys do. */
    while (at < length) {
        size_t width;
        if (ttf_utf8_decode(text + at, length - at, &cp, &width) != 1)
            return TTF_OK;
        if (cp != 0xFE0E && cp != 0xFE0F) {
            if (key_length + width > KEY_BYTES)
                return TTF_OK;
            memcpy(key + key_length, text + at, width);
            key_length += width;
            if (!scalars++)
                first = cp;
        }
        at += width;
    }
    if (!scalars)
        return TTF_OK;
    key[key_length] = 0;
    /* A decorative character follows its own named policy (decorative=follow). */
    if (e->decorative_policy == TTF_DECORATIVE_FOLLOW &&
        ttf_resource_get(e, "speech_decorative", renderer->language_tag, key, NULL))
        return TTF_OK;
    name = lookup(e->symbol_fallback[slot], key, key_length, scalars == 1, first);
    /* symbol_fallback=on: a name the language lacks comes from English. */
    for (j = 0; !name && e->symbol_fallback_policy == TTF_SYMBOL_FALLBACK_ON && j < e->pack_count;
         ++j)
        if (j != slot && !strncmp(e->packs[j]->language_tag, "en", 2) &&
            (e->packs[j]->language_tag[2] == 0 || e->packs[j]->language_tag[2] == '-') &&
            (name = lookup(e->symbol_fallback[j], key, key_length, scalars == 1, first)))
            owner = e->packs[j];
    if (!name || !*name)
        return TTF_OK;
    if (has_digit(name)) {
        status = read_digits(e, owner, name, &digits);
        if (status != TTF_OK) {
            ttf_buffer_free(&digits);
            return status;
        }
        name = digits.data ? digits.data : "";
    }
    status = owner == renderer ? ttf_buffer_puts(out, name)
                               : read_foreign(e, owner, renderer, name, strlen(name), out);
    ttf_buffer_free(&digits);
    if (status == TTF_OK)
        *found = 1;
    return status;
}
