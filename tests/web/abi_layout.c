#include "tts_text_frontend/ttf.h"
#include "tts_text_frontend/ttf_trace.h"
#include "tts_text_frontend/ttf_resource_validation.h"
#include "tts_text_frontend/ttf_frontend_options.h"
#include <stdio.h>
#define RECORD(type, name) printf("%s %zu\n", name, sizeof(type))
#define FIELD(type, name, field) printf("%s.%s %zu\n", name, #field, offsetof(type, field))
int main(void) {
    RECORD(ttf_config_t, "Config");
    RECORD(ttf_frontend_options_t, "FrontendOptions");
    FIELD(ttf_frontend_options_t, "FrontendOptions", input_format);
    FIELD(ttf_frontend_options_t, "FrontendOptions", output_mode);
    FIELD(ttf_frontend_options_t, "FrontendOptions", unreadable_policy);
    FIELD(ttf_frontend_options_t, "FrontendOptions", max_control_bytes);
    RECORD(ttf_semantic_t, "Semantic");
    RECORD(ttf_span_t, "Span");
    RECORD(ttf_result_t, "Result");
    RECORD(ttf_segment_t, "Segment");
    RECORD(ttf_token_t, "Token");
    FIELD(ttf_config_t, "Config", resource_root);
    FIELD(ttf_config_t, "Config", render_policy);
    FIELD(ttf_config_t, "Config", first_segment_target);
    FIELD(ttf_config_t, "Config", sentence_split);
    FIELD(ttf_semantic_t, "Semantic", input_begin);
    FIELD(ttf_semantic_t, "Semantic", value);
    FIELD(ttf_semantic_t, "Semantic", layer);
    FIELD(ttf_span_t, "Span", semantic);
    FIELD(ttf_span_t, "Span", context_trace);
    FIELD(ttf_span_t, "Span", pronunciation_layer);
    FIELD(ttf_span_t, "Span", verbalizer);
    FIELD(ttf_result_t, "Result", spans);
    FIELD(ttf_segment_t, "Segment", input_begin);
    FIELD(ttf_token_t, "Token", script);
    RECORD(ttf_resource_diagnostic_t, "ResourceDiagnostic");
    FIELD(ttf_resource_diagnostic_t, "ResourceDiagnostic", line);
    FIELD(ttf_resource_diagnostic_t, "ResourceDiagnostic", key);
    FIELD(ttf_resource_diagnostic_t, "ResourceDiagnostic", message);
    RECORD(ttf_resource_validation_report_t, "ResourceValidationReport");
    FIELD(ttf_resource_validation_report_t, "ResourceValidationReport", records);
    FIELD(ttf_resource_validation_report_t, "ResourceValidationReport", unvalidated_records);
    RECORD(ttf_trace_options_t, "TraceOptions");
    RECORD(ttf_trace_event_t, "TraceEvent");
    RECORD(ttf_traced_result_t, "TracedResult");
    RECORD(ttf_traced_segment_t, "TracedSegment");
    FIELD(ttf_trace_event_t, "TraceEvent", input_begin);
    FIELD(ttf_trace_event_t, "TraceEvent", phone_index);
    FIELD(ttf_trace_event_t, "TraceEvent", layer);
    FIELD(ttf_trace_event_t, "TraceEvent", declaration_order);
    FIELD(ttf_trace_event_t, "TraceEvent", language);
    FIELD(ttf_trace_event_t, "TraceEvent", detail);
    FIELD(ttf_traced_segment_t, "TracedSegment", events);
    return 0;
}
