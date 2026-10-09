#ifndef TTF_KOREAN_PHONEME_H
#define TTF_KOREAN_PHONEME_H
#include "../core/internal.h"

/* Rewrite the Hangul syllables of text[begin, end) in place with their spoken
   form (colloquial, no length). Words are read whole, so a syllable next to the
   range still decides liaison inside it; the byte length never changes. */
ttf_status_t ttf_korean_phonemize(const ttf_engine_t *engine, char *text, size_t length, size_t begin,
                                  size_t end);
#endif
