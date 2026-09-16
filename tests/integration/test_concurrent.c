#include "tts_text_frontend/ttf.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const ttf_engine_t *engine;
    int failed;
} worker_t;
static void *run_worker(void *arg) {
    worker_t *worker = (worker_t *)arg;
    const char *input = "Apple은 사과입니다. Please go to the 1st floor.";
    const char *expected = "애플은 사과입니다. Please go to the first floor.";
    unsigned i;
    for (i = 0; i < 100; ++i) {
        ttf_result_t result = {0};
        if (ttf_normalize(worker->engine, input, strlen(input), &result) != TTF_OK ||
            !result.text || strcmp(result.text, expected))
            worker->failed = 1;
        ttf_result_free(&result);
    }
    return NULL;
}
int main(int argc, char **argv) {
    enum { COUNT = 8 };
    pthread_t threads[COUNT];
    worker_t workers[COUNT];
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    unsigned i, started = 0;
    int failed = 0;
    if (argc != 2)
        return 2;
    ttf_config_init(&config);
    config.resource_root = argv[1];
    if (ttf_engine_create(&config, &engine) != TTF_OK || ttf_engine_freeze(engine) != TTF_OK) {
        ttf_engine_destroy(engine);
        return 1;
    }
    for (i = 0; i < COUNT; ++i) {
        workers[i].engine = engine;
        workers[i].failed = 0;
        if (pthread_create(&threads[i], NULL, run_worker, &workers[i])) {
            failed = 1;
            break;
        }
        ++started;
    }
    for (i = 0; i < started; ++i) {
        if (pthread_join(threads[i], NULL))
            failed = 1;
        if (workers[i].failed)
            failed = 1;
    }
    ttf_engine_destroy(engine);
    if (!failed)
        puts("8 concurrent workers completed 800 shared-engine requests");
    return failed;
}
