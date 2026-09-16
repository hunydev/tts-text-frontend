#include "tts_text_frontend/ttf.h"
#include <stdio.h>
#define RECORD(type, name) printf("%s %zu\n", name, sizeof(type))
#define FIELD(type, name, field) printf("%s.%s %zu\n", name, #field, offsetof(type, field))
int main(void) {
    RECORD(ttf_config_t, "Config"); RECORD(ttf_semantic_t, "Semantic");
    RECORD(ttf_span_t, "Span"); RECORD(ttf_result_t, "Result");
    RECORD(ttf_segment_t, "Segment"); RECORD(ttf_token_t, "Token");
    FIELD(ttf_config_t, "Config", resource_root); FIELD(ttf_config_t, "Config", render_policy);
    FIELD(ttf_config_t, "Config", first_segment_target); FIELD(ttf_config_t, "Config", sentence_split);
    FIELD(ttf_semantic_t, "Semantic", input_begin); FIELD(ttf_semantic_t, "Semantic", value);
    FIELD(ttf_semantic_t, "Semantic", layer); FIELD(ttf_span_t, "Span", semantic);
    FIELD(ttf_span_t, "Span", context_trace); FIELD(ttf_span_t, "Span", pronunciation_layer);
    FIELD(ttf_span_t, "Span", verbalizer); FIELD(ttf_result_t, "Result", spans);
    FIELD(ttf_segment_t, "Segment", input_begin); FIELD(ttf_token_t, "Token", script);
    return 0;
}
