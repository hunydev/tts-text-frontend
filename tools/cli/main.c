#include "tts_text_frontend/ttf.h"
#include "tts_text_frontend/ttf_frontend_options.h"
#include "tts_text_frontend/ttf_trace.h"
#include "tts_text_frontend/ttf_resource_validation.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#else
#include <unistd.h>
#endif

typedef struct {
    size_t segment_count;
} json_output_t;

static void usage(FILE *out) {
    fputs("Usage: ttf normalize|inspect [options] TEXT\n"
          "       ttf stream|stream-json [options] FILE|-\n"
          "       ttf validate-resource [options] FILE\n"
          "Validation: --resource-layer common|system|domain|customer|user\n"
          "            --require-complete-validation (reject unknown namespaces)\n"
          "Options: --resources DIR --language TAG --domain FILE --customer FILE\n"
          "         --first-target N --normal-target N --max-length N\n"
          "Trace: --trace (inspect or stream-json), --trace-max-events N --trace-max-bytes N\n"
          "Frontend: --input-format plain|annotated --output-mode text|speech\n"
          "          --unreadable preserve|drop --brackets read|skip|preserve\n"
          "          --unknown-foreign guess|spell|preserve --decorative follow|read\n"
          "          --max-control-bytes N\n"
          "Stream options: --chunk-bytes N (default 4096)\n",
          out);
}
static int report(const char *operation, ttf_status_t status) {
    fprintf(stderr, "ttf: %s: %s\n", operation, ttf_status_string(status));
    return EXIT_FAILURE;
}
static int emit(const ttf_segment_t *segment, void *user_data) {
    FILE *out = (FILE *)user_data;
    return fwrite(segment->text, 1, segment->length, out) != segment->length || fflush(out) != 0;
}
static void escaped(const char *s, size_t n) {
    size_t i;
    putchar('"');
    for (i = 0; i < n; ++i) {
        switch (s[i]) {
        case '\n':
            fputs("\\n", stdout);
            break;
        case '\r':
            fputs("\\r", stdout);
            break;
        case '\t':
            fputs("\\t", stdout);
            break;
        case '\b':
            fputs("\\b", stdout);
            break;
        case '\f':
            fputs("\\f", stdout);
            break;
        case '"':
            fputs("\\\"", stdout);
            break;
        case '\\':
            fputs("\\\\", stdout);
            break;
        default:
            if ((unsigned char)s[i] < 0x20)
                printf("\\u%04x", (unsigned int)(unsigned char)s[i]);
            else
                putchar((unsigned char)s[i]);
            break;
        }
    }
    putchar('"');
}
static void json_string_field(const char *name, const char *value) {
    printf(",\"%s\":", name);
    escaped(value, strlen(value));
}
static void validation_json(ttf_status_t status, const ttf_resource_validation_report_t *result,
                            const ttf_resource_diagnostic_t *diagnostic) {
    fputs("{\"version\":1,\"event\":\"resource-validation\"", stdout);
    json_string_field("status", ttf_status_string(status));
    printf(",\"records\":%zu,\"validated_records\":%zu,\"unvalidated_records\":%zu,\"diagnostic\":{"
           "\"line\":%zu",
           result->records, result->validated_records, result->unvalidated_records,
           diagnostic->line);
    json_string_field("path", diagnostic->path);
    json_string_field("kind", diagnostic->kind);
    json_string_field("language", diagnostic->language);
    json_string_field("key", diagnostic->key);
    json_string_field("layer", ttf_layer_name(diagnostic->layer));
    json_string_field("message", diagnostic->message);
    fputs("}}\n", stdout);
}
static int resource_report(const char *operation, ttf_status_t status,
                           const ttf_resource_diagnostic_t *diagnostic) {
    int result = report(operation, status);
    if (diagnostic->message[0])
        fprintf(stderr, "ttf: %s:%zu [%s %s %s; %s] %s\n", diagnostic->path, diagnostic->line,
                diagnostic->kind, diagnostic->language, diagnostic->key,
                ttf_layer_name(diagnostic->layer), diagnostic->message);
    return result;
}
static void optional_size(const char *name, size_t value) {
    printf(",\"%s\":", name);
    if (value == SIZE_MAX)
        fputs("null", stdout);
    else
        printf("%zu", value);
}
static void trace_events_json(const ttf_trace_event_t *events, size_t count) {
    size_t i;
    putchar('[');
    for (i = 0; i < count; ++i) {
        const ttf_trace_event_t *event = events + i;
        if (i)
            putchar(',');
        printf("{\"input_begin\":%zu,\"input_end\":%zu,\"output_begin\":%zu,\"output_end\":%zu",
               event->input_begin, event->input_end, event->output_begin, event->output_end);
        optional_size("part_begin", event->part_begin);
        optional_size("part_end", event->part_end);
        optional_size("phone_index", event->phone_index);
        optional_size("declaration_order", event->declaration_order);
        json_string_field("stage", ttf_trace_stage_name(event->stage));
        json_string_field("disposition", ttf_trace_disposition_name(event->disposition));
        printf(",\"layer_known\":%s", event->layer_known ? "true" : "false");
        json_string_field("layer", event->layer_known ? ttf_layer_name(event->layer) : "unknown");
        json_string_field("language", event->language);
        json_string_field("resource_language", event->resource_language);
        json_string_field("kind", event->kind);
        json_string_field("key", event->key);
        json_string_field("value", event->value);
        json_string_field("text", event->text);
        json_string_field("detail", event->detail);
        putchar('}');
    }
    putchar(']');
}
static int emit_json_detail(const ttf_segment_t *segment, void *user_data,
                            const ttf_trace_event_t *events, size_t count, int traced) {
    json_output_t *output = (json_output_t *)user_data;
    size_t i;
    printf("{\"version\":1,\"event\":\"segment\",\"sequence\":%zu,"
           "\"input_begin\":%zu,\"input_end\":%zu,\"text\":",
           output->segment_count, segment->input_begin, segment->input_end);
    escaped(segment->text, segment->length);
    fputs(",\"spans\":[", stdout);
    for (i = 0; i < segment->span_count; ++i) {
        const ttf_span_t *span = &segment->spans[i];
        const ttf_semantic_t *sem = &span->semantic;
        if (i)
            putchar(',');
        printf("{\"begin\":%zu,\"end\":%zu,\"input_begin\":%zu,\"input_end\":%zu", span->begin,
               span->end, span->input_begin, span->input_end);
        json_string_field("script", ttf_script_name(span->script));
        json_string_field("source_lang", sem->source_lang);
        json_string_field("context_lang", sem->context_lang);
        json_string_field("render_lang", sem->render_lang);
        json_string_field("semantic_type", ttf_semantic_type_name(sem->type));
        json_string_field("value", sem->value);
        json_string_field("secondary", sem->secondary);
        json_string_field("unit", sem->unit);
        json_string_field("rule", sem->rule);
        json_string_field("resource_layer", ttf_layer_name(sem->layer));
        json_string_field("pronunciation_layer", ttf_layer_name(span->pronunciation_layer));
        json_string_field("lexicon", span->lexicon);
        json_string_field("verbalizer", span->verbalizer);
        json_string_field("context_trace", span->context_trace);
        putchar('}');
    }
    putchar(']');
    if (traced) {
        fputs(",\"trace_version\":1,\"trace\":", stdout);
        trace_events_json(events, count);
    }
    fputs("}\n", stdout);
    if (ferror(stdout) || fflush(stdout) != 0)
        return 1;
    output->segment_count++;
    return 0;
}
static int emit_json(const ttf_segment_t *segment, void *userdata) {
    return emit_json_detail(segment, userdata, NULL, 0, 0);
}
static int emit_json_traced(const ttf_traced_segment_t *segment, void *userdata) {
    return emit_json_detail(&segment->segment, userdata, segment->events, segment->event_count, 1);
}
static int positive_size(const char *text, size_t *out) {
    size_t value = 0;
    const unsigned char *p = (const unsigned char *)text;
    if (!*p)
        return 0;
    for (; *p; ++p) {
        unsigned int digit;
        if (*p < '0' || *p > '9')
            return 0;
        digit = (unsigned int)(*p - '0');
        if (value > (SIZE_MAX - digit) / 10)
            return 0;
        value = value * 10 + digit;
    }
    if (!value)
        return 0;
    *out = value;
    return 1;
}
/* A pipe read returns available bytes; fread could wait for the whole request.
 * Regular files retain portable stdio. Only this CLI transport needs OS I/O. */
static ttf_status_t read_input(FILE *file, char *buffer, size_t capacity, size_t *length) {
    if (file == stdin) {
#ifdef _WIN32
        int n;
#else
        ssize_t n;
#endif
        do {
#ifdef _WIN32
            n = _read(_fileno(stdin), buffer, (unsigned int)capacity);
#else
            n = read(STDIN_FILENO, buffer, capacity);
#endif
        } while (n < 0 && errno == EINTR);
        if (n < 0)
            return TTF_EIO;
        *length = (size_t)n;
    } else {
        *length = fread(buffer, 1, capacity, file);
        if (ferror(file))
            return TTF_EIO;
    }
    return TTF_OK;
}
int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_frontend_options_t frontend;
    const char *bracket_policy = NULL, *unknown_foreign = NULL, *decorative = NULL;
    ttf_trace_options_t trace_options;
    int is_trace = 0, trace_limit_set = 0;
    ttf_engine_t *engine = NULL;
    ttf_status_t status;
    ttf_resource_diagnostic_t diagnostic;
    ttf_resource_validation_report_t validation;
    ttf_resource_layer_t validation_layer = TTF_LAYER_CUSTOMER;
    const char *command, *input = NULL;
    const char **overrides;
    ttf_resource_layer_t *layers;
    size_t count = 0, j, chunk_bytes = 4096;
    int i, is_stream, is_json, is_validation, require_complete = 0;
    int positional_only = 0, exit_code = EXIT_SUCCESS;
    ttf_resource_diagnostic_init(&diagnostic);
    ttf_resource_validation_report_init(&validation);
    if (argc < 2) {
        usage(stderr);
        return EXIT_FAILURE;
    }
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        usage(stdout);
        return EXIT_SUCCESS;
    }
    command = argv[1];
    is_validation = !strcmp(command, "validate-resource");
    is_json = !strcmp(command, "stream-json");
    is_stream = is_json || !strcmp(command, "stream");
    if (strcmp(command, "normalize") && strcmp(command, "inspect") && !is_stream &&
        !is_validation) {
        usage(stderr);
        return EXIT_FAILURE;
    }
    overrides = (const char **)calloc((size_t)argc, sizeof(*overrides));
    layers = (ttf_resource_layer_t *)calloc((size_t)argc, sizeof(*layers));
    if (!overrides || !layers) {
        free(overrides);
        free(layers);
        return report("allocate", TTF_ENOMEM);
    }
    ttf_config_init(&config);
    ttf_frontend_options_init(&frontend);
    frontend.output_mode = TTF_OUTPUT_TEXT;
    ttf_trace_options_init(&trace_options);
    for (i = 2; i < argc; ++i) {
        const char *arg = argv[i];
        if (!positional_only &&
            (!strcmp(arg, "--input-format") || !strcmp(arg, "--output-mode") ||
             !strcmp(arg, "--unreadable") || !strcmp(arg, "--brackets") ||
             !strcmp(arg, "--unknown-foreign") || !strcmp(arg, "--decorative") ||
             !strcmp(arg, "--max-control-bytes"))) {
            int valid = 1;
            if (++i == argc) {
                exit_code = report("frontend option", TTF_EINVAL);
                goto done;
            }
            if (!strcmp(arg, "--input-format")) {
                valid = !strcmp(argv[i], "plain") || !strcmp(argv[i], "annotated");
                frontend.input_format =
                    !strcmp(argv[i], "annotated") ? TTF_INPUT_ANNOTATED : TTF_INPUT_PLAIN;
            } else if (!strcmp(arg, "--output-mode")) {
                valid = !strcmp(argv[i], "text") || !strcmp(argv[i], "speech");
                frontend.output_mode =
                    !strcmp(argv[i], "speech") ? TTF_OUTPUT_SPEECH : TTF_OUTPUT_TEXT;
            } else if (!strcmp(arg, "--unreadable")) {
                valid = !strcmp(argv[i], "preserve") || !strcmp(argv[i], "drop");
                frontend.unreadable_policy =
                    !strcmp(argv[i], "drop") ? TTF_UNREADABLE_DROP : TTF_UNREADABLE_PRESERVE;
            } else if (!strcmp(arg, "--decorative")) {
                valid = !strcmp(argv[i], "follow") || !strcmp(argv[i], "read");
                decorative = argv[i];
            } else if (!strcmp(arg, "--unknown-foreign")) {
                valid = !strcmp(argv[i], "guess") || !strcmp(argv[i], "spell") ||
                        !strcmp(argv[i], "preserve");
                unknown_foreign = argv[i];
            } else if (!strcmp(arg, "--brackets")) {
                valid = !strcmp(argv[i], "read") || !strcmp(argv[i], "skip") ||
                        !strcmp(argv[i], "preserve");
                bracket_policy = argv[i];
            } else
                valid = positive_size(argv[i], &frontend.max_control_bytes);
            if (!valid) {
                exit_code = report("frontend option", TTF_EINVAL);
                goto done;
            }
        } else if (!positional_only && !strcmp(arg, "--trace")) {
            is_trace = 1;
        } else if (!positional_only &&
                   (!strcmp(arg, "--trace-max-events") || !strcmp(arg, "--trace-max-bytes"))) {
            size_t value;
            if (++i == argc || !positive_size(argv[i], &value)) {
                exit_code = report("trace limit", TTF_EINVAL);
                goto done;
            }
            if (!strcmp(arg, "--trace-max-events"))
                trace_options.max_events = value;
            else
                trace_options.max_bytes = value;
            trace_limit_set = 1;
        } else if (!positional_only && !strcmp(arg, "--")) {
            positional_only = 1;
        } else if (!positional_only && !strcmp(arg, "--require-complete-validation") &&
                   is_validation) {
            require_complete = 1;
        } else if (!positional_only && !strcmp(arg, "--resource-layer") && is_validation) {
            int layer, found = 0;
            if (++i == argc) {
                usage(stderr);
                exit_code = EXIT_FAILURE;
                goto done;
            }
            for (layer = TTF_LAYER_COMMON; layer <= TTF_LAYER_USER; ++layer)
                if (!strcmp(argv[i], ttf_layer_name((ttf_resource_layer_t)layer))) {
                    validation_layer = (ttf_resource_layer_t)layer;
                    found = 1;
                }
            if (!found) {
                fprintf(stderr, "ttf: invalid resource layer: %s\n", argv[i]);
                exit_code = EXIT_FAILURE;
                goto done;
            }
        } else if (!positional_only &&
                   (!strcmp(arg, "--chunk-bytes") || !strcmp(arg, "--first-target") ||
                    !strcmp(arg, "--normal-target") || !strcmp(arg, "--max-length"))) {
            size_t value;
            if (++i == argc || !positive_size(argv[i], &value) ||
                (!strcmp(arg, "--chunk-bytes") && !is_stream)) {
                fprintf(stderr, "ttf: invalid %s: expected a positive size%s\n", arg,
                        !strcmp(arg, "--chunk-bytes") ? " in a stream command" : "");
                exit_code = EXIT_FAILURE;
                goto done;
            }
            if (!strcmp(arg, "--chunk-bytes"))
                chunk_bytes = value;
            else if (!strcmp(arg, "--first-target"))
                config.first_segment_target = value;
            else if (!strcmp(arg, "--normal-target"))
                config.normal_segment_target = value;
            else
                config.max_segment_length = value;
        } else if (!positional_only && (!strcmp(arg, "--resources") || !strcmp(arg, "--language") ||
                                        !strcmp(arg, "--domain") || !strcmp(arg, "--customer"))) {
            if (++i == argc) {
                usage(stderr);
                exit_code = EXIT_FAILURE;
                goto done;
            }
            if (!strcmp(arg, "--resources"))
                config.resource_root = argv[i];
            else if (!strcmp(arg, "--language"))
                config.default_language = argv[i];
            else {
                overrides[count] = argv[i];
                layers[count++] = !strcmp(arg, "--domain") ? TTF_LAYER_DOMAIN : TTF_LAYER_CUSTOMER;
            }
        } else if (!positional_only && !strncmp(arg, "--", 2)) {
            fprintf(stderr, "ttf: unknown option: %s\n", arg);
            exit_code = EXIT_FAILURE;
            goto done;
        } else if (!input)
            input = arg;
        else {
            usage(stderr);
            exit_code = EXIT_FAILURE;
            goto done;
        }
    }
    if (!input || (trace_limit_set && !is_trace) ||
        (is_trace && strcmp(command, "inspect") && !is_json)) {
        usage(stderr);
        exit_code = EXIT_FAILURE;
        goto done;
    }
    status = ttf_engine_create_ex(&config, &engine, &diagnostic);
    if (status != TTF_OK) {
        if (is_validation)
            validation_json(status, &validation, &diagnostic);
        exit_code = resource_report("create engine", status, &diagnostic);
        goto done;
    }
    status = ttf_engine_set_frontend_options(engine, &frontend);
    if (status == TTF_OK && bracket_policy)
        status = ttf_engine_set_option(engine, "brackets", bracket_policy);
    if (status == TTF_OK && unknown_foreign)
        status = ttf_engine_set_option(engine, "unknown_foreign", unknown_foreign);
    if (status == TTF_OK && decorative)
        status = ttf_engine_set_option(engine, "decorative", decorative);
    if (status != TTF_OK) {
        exit_code = report("frontend options", status);
        goto done;
    }
    for (j = 0; j < count; ++j) {
        status = ttf_resource_load_ex(engine, overrides[j], layers[j], &diagnostic);
        if (status != TTF_OK) {
            if (is_validation)
                validation_json(status, &validation, &diagnostic);
            exit_code = resource_report(overrides[j], status, &diagnostic);
            goto done;
        }
    }
    if (is_validation) {
        status = ttf_resource_load_ex(engine, input, validation_layer, &diagnostic);
        if (status == TTF_OK)
            status = ttf_engine_validate_resources(engine, &validation, &diagnostic);
        if (status == TTF_OK && require_complete && validation.unvalidated_records) {
            status = TTF_ERESOURCE;
            snprintf(diagnostic.message, sizeof(diagnostic.message),
                     "unknown resource records require a registered validator");
        }
        if (status == TTF_OK) {
            status = ttf_engine_freeze(engine);
            if (status != TTF_OK)
                snprintf(diagnostic.message, sizeof(diagnostic.message),
                         "Engine freeze failed after resource validation; check language and "
                         "engine configuration");
        }
        validation_json(status, &validation, &diagnostic);
        if (status != TTF_OK)
            exit_code = resource_report("validate resource", status, &diagnostic);
        else if (ferror(stdout) || fflush(stdout))
            exit_code = report("write validation report", TTF_EIO);
        goto done;
    }
    status = ttf_engine_validate_resources(engine, &validation, &diagnostic);
    if (status == TTF_OK)
        status = ttf_engine_freeze(engine);
    if (status != TTF_OK) {
        exit_code = resource_report("freeze engine", status, &diagnostic);
        goto done;
    }
    if (is_stream) {
        FILE *file = !strcmp(input, "-") ? stdin : fopen(input, "rb");
        ttf_stream_t *stream = NULL;
        json_output_t json_output = {0};
        char buffer[65536];
        size_t n = 0, input_bytes = 0;
        if (!file) {
            perror(input);
            exit_code = EXIT_FAILURE;
            goto done;
        }
        if (chunk_bytes > sizeof(buffer))
            chunk_bytes = sizeof(buffer);
#ifdef _WIN32
        if ((file == stdin && _setmode(_fileno(stdin), _O_BINARY) == -1) ||
            _setmode(_fileno(stdout), _O_BINARY) == -1) {
            if (file != stdin)
                fclose(file);
            exit_code = report("binary stream mode", TTF_EIO);
            goto done;
        }
#endif
        if (is_trace)
            status = ttf_stream_create_traced(engine, &trace_options, emit_json_traced,
                                              &json_output, &stream);
        else
            status = ttf_stream_create(engine, is_json ? emit_json : emit,
                                       is_json ? (void *)&json_output : (void *)stdout, &stream);
        while (status == TTF_OK) {
            status = read_input(file, buffer, chunk_bytes, &n);
            if (status != TTF_OK || !n)
                break;
            if (n > SIZE_MAX - input_bytes) {
                status = TTF_ELIMIT;
                break;
            }
            input_bytes += n;
            status = ttf_stream_write(stream, buffer, n);
        }
        if (status == TTF_OK)
            status = ttf_stream_flush(stream);
        ttf_stream_destroy(stream);
        if (file != stdin)
            fclose(file);
        if (status == TTF_OK && is_json) {
            printf("{\"version\":1,\"event\":\"end\",\"segment_count\":%zu,\"input_bytes\":%zu}\n",
                   json_output.segment_count, input_bytes);
            if (ferror(stdout) || fflush(stdout) != 0)
                status = TTF_EIO;
        }
        if (status != TTF_OK)
            exit_code = report("stream", status);
    } else {
        ttf_result_t result = {0};
        ttf_traced_result_t traced = {0};
        if (is_trace) {
            status = ttf_normalize_traced(engine, input, strlen(input), &trace_options, &traced);
            result = traced.result;
        } else
            status = ttf_normalize(engine, input, strlen(input), &result);
        if (status != TTF_OK) {
            exit_code = report("normalize", status);
            goto done;
        }
        if (!strcmp(command, "normalize")) {
            if (fwrite(result.text, 1, result.length, stdout) != result.length ||
                putchar('\n') == EOF)
                exit_code = report("write output", TTF_EIO);
        } else {
            for (j = 0; j < result.span_count; ++j) {
                const ttf_span_t *span = &result.spans[j];
                const ttf_semantic_t *sem = &span->semantic;
                fputs("surface=", stdout);
                escaped(input + span->input_begin, span->input_end - span->input_begin);
                printf(" script=%s source=%s context=%s render=%s semantic=%s value=",
                       ttf_script_name(span->script), sem->source_lang, sem->context_lang,
                       sem->render_lang, ttf_semantic_type_name(sem->type));
                escaped(sem->value, strlen(sem->value));
                printf(" secondary=");
                escaped(sem->secondary, strlen(sem->secondary));
                printf(" unit=");
                escaped(sem->unit, strlen(sem->unit));
                fputs(" scores=", stdout);
                escaped(span->context_trace, strlen(span->context_trace));
                printf(" rule=%s layer=%s lexicon=%s pronunciation_layer=%s verbalizer=%s "
                       "input=[%zu,%zu) "
                       "output=[%zu,%zu) final=",
                       sem->rule, ttf_layer_name(sem->layer), span->lexicon,
                       ttf_layer_name(span->pronunciation_layer), span->verbalizer,
                       span->input_begin, span->input_end, span->begin, span->end);
                escaped(result.text + span->begin, span->end - span->begin);
                putchar('\n');
            }
            fputs("normalized=", stdout);
            escaped(result.text, result.length);
            putchar('\n');
        }
        if (is_trace) {
            fputs("trace=", stdout);
            trace_events_json(traced.events, traced.event_count);
            putchar('\n');
            ttf_traced_result_free(&traced);
        } else
            ttf_result_free(&result);
        if (ferror(stdout) || fflush(stdout))
            exit_code = report("write output", TTF_EIO);
    }
done:
    ttf_engine_destroy(engine);
    free(overrides);
    free(layers);
    return exit_code;
}
