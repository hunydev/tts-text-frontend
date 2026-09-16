#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "tts_text_frontend/ttf.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

/* Timing only concerns frontend commitment. There is no audio synthesis here. */
typedef struct {
#if defined(_WIN32)
    LARGE_INTEGER frequency;
#else
    int unused;
#endif
} bench_clock_t;

typedef struct {
    bench_clock_t *clock;
    const char *expected;
    size_t expected_length, output_bytes, callbacks;
    size_t submitted_bytes, first_submitted_bytes, first_committed_bytes;
    uint64_t hash;
    double start_ms, first_ms;
    int timed, flushing, first_before_flush, clock_failed;
} callback_state_t;

static const uint64_t fnv_offset = UINT64_C(14695981039346656037);

static const char *clock_name(void) {
#if defined(_WIN32)
    return "QueryPerformanceCounter";
#elif defined(CLOCK_MONOTONIC)
    return "CLOCK_MONOTONIC";
#else
    return "timespec_get(TIME_UTC)";
#endif
}

static int clock_init(bench_clock_t *clock) {
#if defined(_WIN32)
    return QueryPerformanceFrequency(&clock->frequency) && clock->frequency.QuadPart > 0;
#else
    clock->unused = 0;
    return 1;
#endif
}

static int clock_ms(const bench_clock_t *clock, double *ms) {
#if defined(_WIN32)
    LARGE_INTEGER now;
    if (!QueryPerformanceCounter(&now))
        return 0;
    *ms = (double)now.QuadPart * 1000.0 / (double)clock->frequency.QuadPart;
#else
    struct timespec now;
    (void)clock;
#if defined(CLOCK_MONOTONIC)
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
#else
    if (timespec_get(&now, TIME_UTC) != TIME_UTC)
        return 0;
#endif
    *ms = (double)now.tv_sec * 1000.0 + (double)now.tv_nsec / 1000000.0;
#endif
    return 1;
}

static uint64_t hash_bytes(uint64_t hash, const char *data, size_t length) {
    size_t i;
    for (i = 0; i < length; ++i) {
        hash ^= (unsigned char)data[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int receive(const ttf_segment_t *segment, void *user_data) {
    callback_state_t *state = (callback_state_t *)user_data;
    if (!state->callbacks) {
        if (state->timed) {
            double now;
            if (!clock_ms(state->clock, &now) || now < state->start_ms) {
                state->clock_failed = 1;
                return 1;
            }
            state->first_ms = now - state->start_ms;
        }
        state->first_submitted_bytes = state->submitted_bytes;
        state->first_committed_bytes = segment->input_end;
        state->first_before_flush = !state->flushing;
    }
    if (segment->length > state->expected_length - state->output_bytes)
        return 1;
    if (state->expected &&
        memcmp(segment->text, state->expected + state->output_bytes, segment->length))
        return 1;
    state->hash = hash_bytes(state->hash, segment->text, segment->length);
    state->output_bytes += segment->length;
    ++state->callbacks;
    return 0;
}

static ttf_status_t run_stream(const ttf_engine_t *engine, const char *input, size_t length,
                               size_t chunk_bytes, callback_state_t *state, double *total_ms) {
    ttf_stream_t *stream = NULL;
    ttf_status_t status = ttf_stream_create(engine, receive, state, &stream);
    size_t position = 0;
    state->hash = fnv_offset;
    if (status != TTF_OK)
        return status;
    if (state->timed && !clock_ms(state->clock, &state->start_ms)) {
        state->clock_failed = 1;
        status = TTF_EIO;
    }
    while (status == TTF_OK && position < length) {
        size_t take = length - position;
        if (take > chunk_bytes)
            take = chunk_bytes;
        state->submitted_bytes = position + take;
        status = ttf_stream_write(stream, input + position, take);
        position += take;
    }
    if (status == TTF_OK) {
        state->flushing = 1;
        status = ttf_stream_flush(stream);
    }
    if (state->timed && status == TTF_OK) {
        double end_ms;
        if (!clock_ms(state->clock, &end_ms) || end_ms < state->start_ms ||
            end_ms - state->start_ms < state->first_ms) {
            state->clock_failed = 1;
            status = TTF_EIO;
        } else {
            *total_ms = end_ms - state->start_ms;
        }
    }
    ttf_stream_destroy(stream);
    return status;
}

static void usage(FILE *out) {
    fputs("Usage: ttf_bench [options] FILE|-\n"
          "       ttf_bench [options] --text TEXT\n"
          "Options: --iterations N (20) --chunk-bytes N (64) --resources DIR\n"
          "         --first-target N --normal-target N --max-length N\n"
          "Measures frontend commitment latency, not audio TTFB. Emits JSON.\n",
          out);
}

static int parse_size(const char *text, size_t *value) {
    uintmax_t parsed;
    char *end;
    const char *at;
    if (!*text)
        return 0;
    for (at = text; *at; ++at)
        if (*at < '0' || *at > '9')
            return 0;
    errno = 0;
    parsed = strtoumax(text, &end, 10);
    if (errno || *end || !parsed || parsed > SIZE_MAX)
        return 0;
    *value = (size_t)parsed;
    return 1;
}

static char *read_file(const char *path, size_t *length) {
    FILE *file = !strcmp(path, "-") ? stdin : fopen(path, "rb");
    char *data = NULL;
    size_t capacity = 0;
    *length = 0;
    if (!file) {
        perror(path);
        return NULL;
    }
#if defined(_WIN32)
    if (file == stdin && _setmode(_fileno(stdin), _O_BINARY) == -1) {
        fputs("ttf_bench: cannot set standard input to binary mode\n", stderr);
        return NULL;
    }
#endif
    for (;;) {
        size_t taken;
        if (capacity - *length < 65536) {
            char *next;
            size_t grown = capacity ? capacity * 2 : 65536;
            if (grown <= capacity) {
                fputs("ttf_bench: input exceeds addressable memory\n", stderr);
                free(data);
                data = NULL;
                break;
            }
            next = (char *)realloc(data, grown);
            if (!next) {
                fputs("ttf_bench: input allocation failed\n", stderr);
                free(data);
                data = NULL;
                break;
            }
            data = next;
            capacity = grown;
        }
        taken = fread(data + *length, 1, capacity - *length, file);
        *length += taken;
        if (ferror(file)) {
            fprintf(stderr, "ttf_bench: cannot read %s\n", path);
            free(data);
            data = NULL;
            break;
        }
        if (feof(file))
            break;
    }
    if (file != stdin && fclose(file) != 0) {
        fprintf(stderr, "ttf_bench: cannot close %s\n", path);
        free(data);
        data = NULL;
    }
    return data;
}

static int compare_double(const void *a, const void *b) {
    double left = *(const double *)a, right = *(const double *)b;
    return (left > right) - (left < right);
}

static size_t percentile_index(size_t count, size_t percentile) {
    return (count / 100) * percentile + ((count % 100) * percentile + 99) / 100 - 1;
}

static void print_stats(const char *name, double *values, size_t count) {
    qsort(values, count, sizeof(*values), compare_double);
    printf("  \"%s\": {\"min\": %.9f, \"p50\": %.9f, \"p95\": %.9f, \"max\": %.9f}", name,
           values[0], values[percentile_index(count, 50)], values[percentile_index(count, 95)],
           values[count - 1]);
}

static int report(const char *operation, ttf_status_t status) {
    fprintf(stderr, "ttf_bench: %s: %s\n", operation, ttf_status_string(status));
    return EXIT_FAILURE;
}

int main(int argc, char **argv) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_result_t batch = {0};
    ttf_status_t status;
    bench_clock_t clock;
    callback_state_t baseline = {0};
    const char *input = NULL, *path = NULL;
    char *owned_input = NULL;
    size_t input_bytes = 0, iterations = 20, chunk_bytes = 64, run;
    size_t incremental_runs = 0, before_flush_runs = 0;
    double setup_start, setup_end, setup_ms, total_sum = 0.0;
    double *first_values = NULL, *total_values = NULL;
    uint64_t expected_hash = 0;
    int i, options = 1, exit_code = EXIT_FAILURE;

    ttf_config_init(&config);
    for (i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        size_t *number = NULL;
        if (options && !strcmp(arg, "--")) {
            options = 0;
            continue;
        }
        if (options && (!strcmp(arg, "--help") || !strcmp(arg, "-h"))) {
            usage(stdout);
            return EXIT_SUCCESS;
        }
        if (options && !strcmp(arg, "--iterations"))
            number = &iterations;
        else if (options && !strcmp(arg, "--chunk-bytes"))
            number = &chunk_bytes;
        else if (options && !strcmp(arg, "--first-target"))
            number = &config.first_segment_target;
        else if (options && !strcmp(arg, "--normal-target"))
            number = &config.normal_segment_target;
        else if (options && !strcmp(arg, "--max-length"))
            number = &config.max_segment_length;
        if (number) {
            if (++i == argc || !parse_size(argv[i], number)) {
                fprintf(stderr, "ttf_bench: %s requires a positive integer\n", arg);
                goto done;
            }
        } else if (options && !strcmp(arg, "--resources")) {
            if (++i == argc || !*argv[i]) {
                fputs("ttf_bench: --resources requires a directory\n", stderr);
                goto done;
            }
            config.resource_root = argv[i];
        } else if (options && !strcmp(arg, "--text")) {
            if (++i == argc || input || path) {
                fputs("ttf_bench: provide exactly one --text value or input file\n", stderr);
                goto done;
            }
            input = argv[i];
        } else if (options && arg[0] == '-' && strcmp(arg, "-")) {
            fprintf(stderr, "ttf_bench: unknown option %s\n", arg);
            goto done;
        } else if (input || path) {
            fputs("ttf_bench: provide exactly one --text value or input file\n", stderr);
            goto done;
        } else {
            path = arg;
        }
    }
    if (!input && !path) {
        usage(stderr);
        goto done;
    }
    if (path) {
        owned_input = read_file(path, &input_bytes);
        if (!owned_input)
            goto done;
        input = owned_input;
    } else {
        input_bytes = strlen(input);
    }
    if (!input_bytes) {
        fputs("ttf_bench: input must not be empty\n", stderr);
        goto done;
    }
    if (iterations > SIZE_MAX / sizeof(double)) {
        fputs("ttf_bench: iteration count exceeds addressable memory\n", stderr);
        goto done;
    }
    first_values = (double *)malloc(iterations * sizeof(*first_values));
    total_values = (double *)malloc(iterations * sizeof(*total_values));
    if (!first_values || !total_values) {
        report("allocate measurements", TTF_ENOMEM);
        goto done;
    }
    if (!clock_init(&clock) || !clock_ms(&clock, &setup_start)) {
        fputs("ttf_bench: clock initialization failed\n", stderr);
        goto done;
    }
    status = ttf_engine_create(&config, &engine);
    if (status == TTF_OK)
        status = ttf_engine_freeze(engine);
    if (!clock_ms(&clock, &setup_end) || setup_end < setup_start) {
        fputs("ttf_bench: setup clock failed or moved backwards\n", stderr);
        goto done;
    }
    setup_ms = setup_end - setup_start;
    if (status != TTF_OK) {
        report("create/freeze engine", status);
        goto done;
    }
    status = ttf_normalize(engine, input, input_bytes, &batch);
    if (status != TTF_OK) {
        report("batch validation", status);
        goto done;
    }
    expected_hash = hash_bytes(fnv_offset, batch.text, batch.length);
    baseline.expected = batch.text;
    baseline.expected_length = batch.length;
    status = run_stream(engine, input, input_bytes, chunk_bytes, &baseline, NULL);
    if (status != TTF_OK || baseline.output_bytes != batch.length ||
        baseline.hash != expected_hash || !baseline.callbacks) {
        report("exact batch/stream validation", status == TTF_OK ? TTF_ESTATE : status);
        goto done;
    }
    baseline.expected = NULL;
    ttf_result_free(&batch);
    if (baseline.callbacks > SIZE_MAX / iterations) {
        fputs("ttf_bench: total callback count exceeds addressable range\n", stderr);
        goto done;
    }
    for (run = 0; run < iterations; ++run) {
        callback_state_t state = {0};
        state.clock = &clock;
        state.expected_length = baseline.output_bytes;
        state.timed = 1;
        status = run_stream(engine, input, input_bytes, chunk_bytes, &state, &total_values[run]);
        if (state.clock_failed) {
            fputs("ttf_bench: run clock failed or moved backwards\n", stderr);
            goto done;
        }
        if (status != TTF_OK) {
            report("timed stream", status);
            goto done;
        }
        if (state.output_bytes != baseline.output_bytes || state.hash != expected_hash ||
            state.callbacks != baseline.callbacks ||
            state.first_submitted_bytes != baseline.first_submitted_bytes ||
            state.first_committed_bytes != baseline.first_committed_bytes ||
            state.first_before_flush != baseline.first_before_flush) {
            fputs("ttf_bench: timed stream differs from validated batch/stream output\n", stderr);
            goto done;
        }
        first_values[run] = state.first_ms;
        total_sum += total_values[run];
        if (state.first_submitted_bytes < input_bytes)
            ++incremental_runs;
        if (state.first_before_flush)
            ++before_flush_runs;
    }
    printf("{\n  \"schema_version\": 1,\n"
           "  \"metric\": \"frontend_commit_latency_not_audio_ttfb\",\n"
           "  \"clock\": \"%s\",\n"
           "  \"input_bytes\": %zu,\n  \"output_bytes\": %zu,\n"
           "  \"iterations\": %zu,\n  \"chunk_bytes\": %zu,\n"
           "  \"first_segment_target\": %zu,\n  \"normal_segment_target\": %zu,\n"
           "  \"max_segment_length\": %zu,\n"
           "  \"engine_setup_ms\": %.9f,\n"
           "  \"callbacks\": %zu,\n  \"total_callbacks\": %zu,\n"
           "  \"first_callback_input_bytes\": %zu,\n"
           "  \"first_callback_committed_input_bytes\": %zu,\n"
           "  \"first_callback_before_final_input_runs\": %zu,\n"
           "  \"first_callback_before_flush_runs\": %zu,\n"
           "  \"validation\": \"exact_preflight_and_fnv1a64_each_iteration\",\n"
           "  \"output_fnv1a64\": \"%016" PRIx64 "\",\n",
           clock_name(), input_bytes, baseline.output_bytes, iterations, chunk_bytes,
           config.first_segment_target, config.normal_segment_target, config.max_segment_length,
           setup_ms, baseline.callbacks, baseline.callbacks * iterations,
           baseline.first_submitted_bytes, baseline.first_committed_bytes, incremental_runs,
           before_flush_runs, expected_hash);
    print_stats("first_callback_ms", first_values, iterations);
    fputs(",\n", stdout);
    print_stats("total_ms", total_values, iterations);
    if (total_sum > 0.0)
        printf(",\n  \"throughput_mib_s\": %.9f\n}\n",
               ((double)input_bytes * (double)iterations / 1048576.0) / (total_sum / 1000.0));
    else
        fputs(",\n  \"throughput_mib_s\": null\n}\n", stdout);
    if (ferror(stdout) || fflush(stdout) != 0) {
        report("write JSON", TTF_EIO);
        goto done;
    }
    exit_code = EXIT_SUCCESS;
done:
    ttf_result_free(&batch);
    ttf_engine_destroy(engine);
    free(owned_input);
    free(first_values);
    free(total_values);
    return exit_code;
}
