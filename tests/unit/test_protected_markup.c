#include "../../src/protected/protected.h"
#include <stdio.h>
#include <string.h>

static int failures;

static void policy(ttf_engine_t *engine, const char *kind, const char *key, const char *value,
                   ttf_resource_layer_t layer) {
    if (ttf_resource_set(engine, kind, "*", key, value, layer) != TTF_OK) {
        fprintf(stderr, "resource allocation failed\n");
        ++failures;
    }
}

static void expect(ttf_engine_t *engine, const char *input, size_t begin, int final,
                   int found, size_t end, const char *kind, const char *rule, int complete,
                   ttf_resource_layer_t layer) {
    ttf_protected_match_t match = {0};
    int actual = ttf_protect_markup(engine, input, strlen(input), begin, final, &match);
    if (actual != found ||
        (found && (match.end != end || match.complete != complete || match.layer != layer ||
                   strcmp(match.kind, kind) || strcmp(match.rule, rule)))) {
        fprintf(stderr, "markup [%s] at %zu final=%d: got %d %zu %s %s complete=%d layer=%d\n",
                input, begin, final, actual, match.end, match.kind ? match.kind : "-",
                match.rule ? match.rule : "-", match.complete, (int)match.layer);
        ++failures;
    }
}

static void whole(ttf_engine_t *engine, const char *input, const char *kind, const char *rule) {
    expect(engine, input, 0, 1, 1, strlen(input), kind, rule, 1, TTF_LAYER_SYSTEM);
}

static void prefixes(ttf_engine_t *engine, const char *input, const char *kind,
                     const char *rule) {
    size_t i, length = strlen(input);
    for (i = 1; i < length; ++i) {
        ttf_protected_match_t match = {0};
        if (!ttf_protect_markup(engine, input, i, 0, 0, &match) || match.complete ||
            match.end != i) {
            fprintf(stderr, "lost incomplete protected prefix at %zu: [%s]\n", i, input);
            ++failures;
        }
    }
    expect(engine, input, 0, 1, 1, length, kind, rule, 1, TTF_LAYER_SYSTEM);
}

int main(void) {
    ttf_engine_t engine = {0};
    size_t i;
    char nested[512] = "";
    static const char *invalid[] = {"<3", "<>", "< a>", "<a =x>", "</a x>", "<a x=>",
                                    "<a x=\"q\"v=\"t\">", "<a x=<q>>"};
    policy(&engine, "protection", "markup", "1", TTF_LAYER_SYSTEM);
    policy(&engine, "protection", "code", "1", TTF_LAYER_SYSTEM);
    policy(&engine, "protected_tag", "code", "opaque", TTF_LAYER_SYSTEM);
    policy(&engine, "protected_tag", "pre", "opaque", TTF_LAYER_SYSTEM);
    whole(&engine, "<b>", "markup", "protected.markup");
    whole(&engine, "</b >", "markup", "protected.markup");
    whole(&engine, "<br/>", "markup", "protected.markup");
    whole(&engine, "<x:tag hidden data-x='a > b' data-y=ok>", "markup", "protected.markup");
    whole(&engine, "<b first second third=\"value\" >", "markup", "protected.markup");
    whole(&engine, "<code>GPU</code>", "code", "protected.code");
    whole(&engine, "<CoDe title=\"a > b\"><code>x</code> y</CODE>", "code", "protected.code");
    whole(&engine, "<pre><!-- </pre> -->text</pre>", "code", "protected.code");
    whole(&engine, "<!-- <code>GPU</code> -->", "markup", "protected.comment");
    whole(&engine, "`GPU 1st`", "code", "protected.code");
    whole(&engine, "``a ` b``", "code", "protected.code");
    whole(&engine, "```c\nGPU\n```", "code", "protected.code");
    whole(&engine, "`unclosed", "code", "protected.unclosed-code");
    whole(&engine, "<code>unclosed", "code", "protected.unclosed-code");
    whole(&engine, "<code>GPU</pre>", "code", "protected.unclosed-code");
    whole(&engine, "<a x=\"unclosed", "markup", "protected.unclosed-markup");
    whole(&engine, "<!-- unclosed", "markup", "protected.unclosed-comment");
    whole(&engine, "<!-", "markup", "protected.unclosed-comment");
    whole(&engine, "<", "markup", "protected.unclosed-markup");
    expect(&engine, "앞 <b>뒤", 4, 1, 1, 7, "markup", "protected.markup", 1, TTF_LAYER_SYSTEM);
    expect(&engine, "<code>x</code>tail", 0, 0, 1, 14, "code", "protected.code", 1,
           TTF_LAYER_SYSTEM);
    expect(&engine, "`x`", 0, 0, 1, 3, "code", "protected.code", 0, TTF_LAYER_SYSTEM);
    expect(&engine, "`x` ", 0, 0, 1, 3, "code", "protected.code", 1, TTF_LAYER_SYSTEM);
    expect(&engine, "`x``", 0, 1, 1, 4, "code", "protected.unclosed-code", 1, TTF_LAYER_SYSTEM);
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        expect(&engine, invalid[i], 0, 1, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "`````````````````x`````````````````", 0, 1, 0, 0, "", "", 0,
           TTF_LAYER_COMMON);
    expect(&engine, "`````````````````x", 1, 1, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    prefixes(&engine, "<code data-x=\"quoted > value\">GPU</code>", "code", "protected.code");
    prefixes(&engine, "<!-- <GPU> -->", "markup", "protected.comment");
    prefixes(&engine, "```c\nGPU\n```", "code", "protected.code");
    for (i = 0; i < 33; ++i)
        strcat(nested, "<code>");
    for (i = 0; i < 33; ++i)
        strcat(nested, "</code>");
    whole(&engine, nested, "code", "protected.unclosed-code");
    policy(&engine, "protected_tag", "x-code", "opaque", TTF_LAYER_CUSTOMER);
    expect(&engine, "<x-code>GPU</x-code>", 0, 1, 1, 20, "code", "protected.code", 1,
           TTF_LAYER_CUSTOMER);
    policy(&engine, "protected_tag", "code", "transparent", TTF_LAYER_CUSTOMER);
    expect(&engine, "<code>GPU</code>", 0, 1, 1, 6, "markup", "protected.markup", 1,
           TTF_LAYER_SYSTEM);
    policy(&engine, "protection", "markup", "0", TTF_LAYER_CUSTOMER);
    expect(&engine, "<b>", 0, 1, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "<b attr", 0, 0, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "<code title=\"GPU", 0, 1, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "<code title=\"GPU", 0, 0, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "<code", 0, 1, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "<em", 0, 1, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "<em title=\"GPU", 0, 1, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "</pre", 0, 1, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "</pre", 0, 0, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "<", 0, 1, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "<pr", 0, 0, 1, 3, "code", "protected.code", 0, TTF_LAYER_SYSTEM);
    expect(&engine, "<pr", 0, 1, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    expect(&engine, "<pre", 0, 1, 1, 4, "code", "protected.unclosed-code", 1,
           TTF_LAYER_SYSTEM);
    expect(&engine, "<pre title=\"GPU", 0, 1, 1, 15, "code", "protected.unclosed-code", 1,
           TTF_LAYER_SYSTEM);
    expect(&engine, "<x-code", 0, 1, 1, 7, "code", "protected.unclosed-code", 1,
           TTF_LAYER_CUSTOMER);
    expect(&engine, "<pre>GPU</pre>", 0, 1, 1, 14, "code", "protected.code", 1, TTF_LAYER_SYSTEM);
    policy(&engine, "protection", "code", "0", TTF_LAYER_CUSTOMER);
    expect(&engine, "`GPU`", 0, 1, 0, 0, "", "", 0, TTF_LAYER_COMMON);
    ttf_resource_destroy(&engine);
    return failures ? 1 : 0;
}
