#ifndef TTF_KOREAN_PHONEME_H
#define TTF_KOREAN_PHONEME_H
#include "../core/internal.h"

/* Rewrite the Hangul syllables of text[begin, end) in place with their spoken
   form (colloquial, no length). Words are read whole, so a syllable next to the
   range still decides liaison inside it; the byte length never changes. */
ttf_status_t ttf_korean_phonemize(const ttf_engine_t *engine, char *text, size_t length, size_t begin,
                                  size_t end);

/* 1 when the Hangul words s[prev_begin, prev_end) and s[next_begin, next_end), one
   space apart, read together (남지 않았다, 할 수, 할 일이, 구두를 신고, 운동화만 신고,
   신고 다녔다). */
int ttf_korean_phoneme_joined(const ttf_engine_t *engine, const char *s, size_t prev_begin,
                              size_t prev_end, size_t next_begin, size_t next_end);
#endif
