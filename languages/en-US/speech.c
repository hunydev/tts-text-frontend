/* Explicit address/alphabet spelling never enters a pronunciation lexicon or G2P. */
#include "speech.h"
#include "../../src/pronunciation/trace.h"
#include <stdio.h>
#include <string.h>

ttf_status_t ttf_en_speech_spelling(const ttf_engine_t *e, const char *text, size_t length,
                                    int address, ttf_buffer_t *out) {
    const char *join = ttf_resource_get(e, "speech_join", "en-US", "alphabet", NULL);
    size_t i;
    int previous_symbol = 0, join_recorded = 0;
    if (!text || !length)
        return TTF_EINVAL;
    if (!join || (strcmp(join, "none") && strcmp(join, "space")))
        return TTF_ERESOURCE;
    /* Validate the full input before appending; no prefix succeeds on invalid controls. */
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        int alnum = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!alnum && (!address || c <= 32 || c >= 127))
            return TTF_EINVAL;
    }
    for (i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)text[i];
        char key[16];
        const char *kind, *label;
        int symbol = 0;
        ttf_status_t status;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            key[0] = c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : (char)c;
            key[1] = 0;
            kind = "letter_name";
        } else {
            key[0] = (char)c;
            key[1] = 0;
            if (c >= '0' && c <= '9')
                kind = "number";
            else {
                kind = "speech_symbol";
                symbol = 1;
            }
        }
        label = ttf_resource_get(e, kind, "en-US", key, NULL);
        if (!label)
            return TTF_ERESOURCE;
        if (i && !symbol && !previous_symbol && !join_recorded) {
            ttf_trace_stage(e, TTF_TRACE_POLICY, 0, length);
            status = ttf_trace_resource(e, "speech_join", "en-US", "alphabet", join,
                                        "spelling-join-policy", SIZE_MAX);
            if (status != TTF_OK)
                return status;
            join_recorded = 1;
        }
        if (i && (symbol || previous_symbol || !strcmp(join, "space"))) {
            status = ttf_buffer_puts(out, " ");
            if (status != TTF_OK)
                return status;
        }
        ttf_trace_stage(e, TTF_TRACE_POLICY, i, i + 1);
        {
            char written[2] = {(char)c, 0};
            status = ttf_trace_resource(e, kind, "en-US", key, written,
                                        address ? "address-spelling" : "explicit-alphabet-spelling",
                                        SIZE_MAX);
        }
        if (status == TTF_OK)
            status = ttf_buffer_puts(out, label);
        if (status != TTF_OK)
            return status;
        previous_symbol = symbol;
    }
    return TTF_OK;
}
