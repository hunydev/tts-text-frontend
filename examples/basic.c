#include "tts_text_frontend/ttf.h"
#include <stdio.h>
#include <string.h>

static int on_segment(const ttf_segment_t *segment, void *user_data) {
    FILE *out = (FILE *)user_data;
    /* Pass segment->text and span context language tags to your TTS engine.
       Copy them here if synthesis will outlive this callback. */
    return fwrite(segment->text, 1, segment->length, out) != segment->length;
}
int main(void) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_stream_t *stream = NULL;
    ttf_result_t result = {0};
    ttf_status_t status;
    const char *text = "Apple은 사과입니다. Please go to the 1st floor.";
    ttf_config_init(&config);
    status = ttf_engine_create(&config, &engine);
    if (status == TTF_OK)
        status = ttf_engine_freeze(engine);
    if (status == TTF_OK)
        status = ttf_normalize(engine, text, strlen(text), &result);
    if (status == TTF_OK)
        puts(result.text);
    ttf_result_free(&result);
    if (status == TTF_OK)
        status = ttf_stream_create(engine, on_segment, stdout, &stream);
    if (status == TTF_OK)
        status = ttf_stream_write(stream, "가격은 12", strlen("가격은 12"));
    if (status == TTF_OK)
        status = ttf_stream_write(stream, ",500원입니다.", strlen(",500원입니다."));
    if (status == TTF_OK)
        status = ttf_stream_flush(stream);
    ttf_stream_destroy(stream);
    ttf_engine_destroy(engine);
    if (status != TTF_OK)
        fprintf(stderr, "%s\n", ttf_status_string(status));
    return status == TTF_OK ? 0 : 1;
}
