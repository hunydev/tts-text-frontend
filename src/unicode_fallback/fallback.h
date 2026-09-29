#ifndef TTF_UNICODE_FALLBACK_H
#define TTF_UNICODE_FALLBACK_H
#include "../core/internal.h"

/* Unicode spoken fallback (docs/unicode-fallback.ko.md). One read-only file
   triple per language under <resource root>/unicode-fallback/: <tag>.idx maps
   a code point straight to an offset in <tag>.dat, a pool of NUL-terminated
   names; <tag>.seq holds sorted multi-code-point sequences. The files are
   mapped, never loaded, and are immutable while the engine lives. */
typedef struct ttf_symbol_fallback ttf_symbol_fallback_t;

/* *out is NULL, with TTF_OK, when the language has no files. A present but
   malformed or mismatched triple is TTF_ERESOURCE. */
ttf_status_t ttf_symbol_fallback_open(const char *root, const char *tag,
                                      ttf_symbol_fallback_t **out);
void ttf_symbol_fallback_close(ttf_symbol_fallback_t *fallback);
const char *ttf_symbol_fallback_single(const ttf_symbol_fallback_t *fallback, uint32_t cp);
const char *ttf_symbol_fallback_sequence(const ttf_symbol_fallback_t *fallback, const char *key,
                                         size_t length);

/* Open the triple of every pack, or none with symbol_fallback=off. */
ttf_status_t ttf_symbol_fallback_attach(ttf_engine_t *engine);
void ttf_symbol_fallback_detach(ttf_engine_t *engine);

/* End of the emoji sequence at text[begin] (flag pair, or a pictograph with
   presentation selectors, skin tones, keycap, tags or ZWJ-joined pictographs),
   or 0 when the text there is not a sequence of two or more code points. */
size_t ttf_emoji_cluster_end(const char *text, size_t length, size_t begin);

/* Speak one fallback object (a code point or a complete sequence) in the
   renderer's language. *found is 0 when no allowed name exists; the caller
   then applies its unreadable policy. A name from the English file in another
   language is rendered through the foreign-word renderer; a name containing
   digits is normalized once more with the fallback disabled. */
ttf_status_t ttf_symbol_fallback_speak(const ttf_engine_t *engine,
                                       const ttf_language_pack_t *renderer, const char *text,
                                       size_t length, ttf_buffer_t *out, int *found);
#endif
