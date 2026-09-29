#ifndef TTF_CONTROL_H
#define TTF_CONTROL_H
#include "../core/internal.h"

typedef enum {
    TTF_CONTROL_NONE = 0,
    TTF_CONTROL_LITERAL,
    TTF_CONTROL_PATTERN,
    TTF_CONTROL_ESCAPE
} ttf_control_kind_t;

/* All ranges are byte offsets in the original input. A grouped base excludes
   its [[ ]] delimiters. Payload ranges exclude braces and retain escapes until
   decoded. No pointer or allocation is owned by this structure. */
typedef struct {
    size_t begin, end;
    size_t base_begin, base_end;
    size_t payload_begin, payload_end;
    ttf_control_kind_t kind;
    int complete, grouped;
    char wait_quote;
} ttf_control_match_t;

/* Called only in annotated mode, after protected code/markup/path matching.
   NONE is an ordinary-text miss. Incomplete syntax is retained when !final;
   malformed/final-incomplete syntax returns EINVAL, byte limits return ELIMIT. */
ttf_status_t ttf_control_parse(const ttf_engine_t *engine, const char *text, size_t length,
                               size_t begin, int final, ttf_control_match_t *match);
/* Decode the five reserved escapes: \\, \{, \}, \[, \]. Appends to caller-owned
   output. Decoded aliases are output text and must never be parsed recursively. */
ttf_status_t ttf_control_decode(const char *text, size_t begin, size_t end, ttf_buffer_t *output);
#endif
