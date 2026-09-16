#include "tts_text_frontend/ttf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out) {
    fputs("Usage: ttf normalize|inspect [options] TEXT\n"
          "       ttf stream [options] FILE|-\n"
          "Options: --resources DIR --language TAG --domain FILE --customer FILE\n",
          out);
}
static int report(const char *operation, ttf_status_t status) {
    fprintf(stderr, "ttf: %s: %s\n", operation, ttf_status_string(status));
    return EXIT_FAILURE;
}
static int emit(const ttf_segment_t *segment, void *user_data) {
    FILE *out = (FILE *)user_data;
    return fwrite(segment->text, 1, segment->length, out) != segment->length;
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
        case '"':
            fputs("\\\"", stdout);
            break;
        case '\\':
            fputs("\\\\", stdout);
            break;
        default:
            putchar((unsigned char)s[i]);
            break;
        }
    }
    putchar('"');
}
int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_status_t status;
    const char *command, *input = NULL;
    const char **overrides;
    ttf_resource_layer_t *layers;
    size_t count = 0, j;
    int i, exit_code = EXIT_SUCCESS;
    if (argc < 2) {
        usage(stderr);
        return EXIT_FAILURE;
    }
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) {
        usage(stdout);
        return EXIT_SUCCESS;
    }
    command = argv[1];
    if (strcmp(command, "normalize") && strcmp(command, "inspect") && strcmp(command, "stream")) {
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
        if (!strcmp(arg, "--resources") || !strcmp(arg, "--language") || !strcmp(arg, "--domain") ||
            !strcmp(arg, "--customer")) {
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
    if (!strcmp(command, "stream")) {
        FILE *file = !strcmp(input, "-") ? stdin : fopen(input, "rb");
        ttf_stream_t *stream = NULL;
        char buffer[4096];
        size_t n;
        if (!file) {
            perror(input);
            exit_code = EXIT_FAILURE;
            goto done;
        }
        status = ttf_stream_create(engine, emit, stdout, &stream);
        while (status == TTF_OK && (n = fread(buffer, 1, sizeof(buffer), file)) != 0)
            status = ttf_stream_write(stream, buffer, n);
        if (ferror(file) && status == TTF_OK)
            status = TTF_EIO;
        if (status == TTF_OK)
            status = ttf_stream_flush(stream);
        ttf_stream_destroy(stream);
        if (file != stdin)
            fclose(file);
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
