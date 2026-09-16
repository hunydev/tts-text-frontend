#include "tts_text_frontend/ttf.h"
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
          "Options: --resources DIR --language TAG --domain FILE --customer FILE\n"
          "         --first-target N --normal-target N --max-length N\n"
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
static int emit_json(const ttf_segment_t *segment, void *user_data) {
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
    fputs("]}\n", stdout);
    if (ferror(stdout) || fflush(stdout) != 0)
        return 1;
    output->segment_count++;
    return 0;
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
    ttf_engine_t *engine = NULL;
    ttf_status_t status;
    const char *command, *input = NULL;
    const char **overrides;
    ttf_resource_layer_t *layers;
    size_t count = 0, j, chunk_bytes = 4096;
    int i, is_stream, is_json, positional_only = 0, exit_code = EXIT_SUCCESS;
    if (argc < 2) {
        usage(stderr);
        return EXIT_FAILURE;
    }
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        usage(stdout);
        return EXIT_SUCCESS;
    }
    command = argv[1];
    is_json = !strcmp(command, "stream-json");
    is_stream = is_json || !strcmp(command, "stream");
    if (strcmp(command, "normalize") && strcmp(command, "inspect") && !is_stream) {
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
    for (i = 2; i < argc; ++i) {
        const char *arg = argv[i];
        if (!positional_only && !strcmp(arg, "--")) {
            positional_only = 1;
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
    if (!input) {
        usage(stderr);
        exit_code = EXIT_FAILURE;
        goto done;
    }
    status = ttf_engine_create(&config, &engine);
    if (status != TTF_OK) {
        exit_code = report("create engine", status);
        goto done;
    }
    for (j = 0; j < count; ++j) {
        status = ttf_resource_load(engine, overrides[j], layers[j]);
        if (status != TTF_OK) {
            exit_code = report(overrides[j], status);
            goto done;
        }
    }
    status = ttf_engine_freeze(engine);
    if (status != TTF_OK) {
        exit_code = report("freeze engine", status);
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
        ttf_result_free(&result);
    }
done:
    ttf_engine_destroy(engine);
    free(overrides);
    free(layers);
    return exit_code;
}
