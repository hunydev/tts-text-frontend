# TTS Text Frontend

Deterministic, resource-driven, multilingual and code-mixed text frontend for TTS.

C11 native library for text normalization, context routing, foreign pronunciation,
span metadata and incremental TTS callbacks. The initial packs are **ko-KR** and
**en-US**. This is a working initial implementation; pronunciation quality and
language detection are intentionally small, deterministic prototypes.

```text
1st는 첫 번째라는 뜻입니다.
영어로는 다음과 같은 예문이 있습니다.
Please go to the 1st floor
↓
퍼스트는 첫 번째라는 뜻입니다.
영어로는 다음과 같은 예문이 있습니다.
Please go to the first floor

Apple은 사과입니다.          → 애플은 사과입니다.
Apple released the iPhone.  → Apple released the iPhone.
GPU를 사용합니다.            → 지피유를 사용합니다.
WiFi를 사용합니다.           → 와이파이를 사용합니다.
```

`Apple → 애플` is pronunciation rendering. Source, context and render languages
remain distinct in the semantic IR and output metadata.

## Build and test

Linux, CMake 3.16+, and a C11 compiler are required. There are no runtime package
dependencies. Python 3 is optional for the resource compiler and its tests.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure

cmake -S . -B build-sanitize -DCMAKE_C_COMPILER=clang \
  -DTTF_ENABLE_ASAN=ON -DTTF_ENABLE_UBSAN=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

Produces `libtts_text_frontend.so`, `libtts_text_frontend.a`, `ttf`, and
`ttf_example`. Both library variants are built in the same invocation. Public
headers use opaque engine and stream handles, `ttf_` symbols, ABI versions and
structure sizes. Existing ABI v1 signatures/layouts are frozen; incompatible
changes require a new major ABI/SOVERSION. Linux is verified; portable C and Windows export annotations
prepare the project for macOS/Windows validation.

```sh
cmake --install build --prefix "$PWD/install"
install/bin/ttf normalize --resources "$PWD/install/share/tts-text-frontend/resources" \
  "Apple은 사과입니다."
```

The development default resource path points at the source tree. For packaged
installations set `TTF_DEFAULT_RESOURCE_ROOT` at configure time or pass
`config.resource_root` / `--resources`; explicit paths support relocation.

## CLI

```sh
build/ttf normalize '1st는 첫 번째라는 뜻입니다.
영어로는 다음과 같은 예문이 있습니다.
Please go to the 1st floor'
build/ttf normalize 'Apple은 사과입니다.'
build/ttf normalize 'GPU를 사용합니다.'
build/ttf normalize 'WiFi를 사용합니다.'
build/ttf normalize --language en-US 'Please go to the 1st floor.'
build/ttf inspect 'OpenAI가 발표했습니다.'
build/ttf stream input.txt
```

`inspect` includes surface text, script, source/context/render language, semantic
class and value, matched rule, resource layer, lexicon provenance, verbalizer,
per-pack context scores, input/output byte ranges and final text. `stream -` reads standard input.

## C API

```c
#include <stdio.h>
#include <string.h>
#include <tts_text_frontend/ttf.h>

int main(void) {
    ttf_config_t config;
    ttf_engine_t *engine = NULL;
    ttf_result_t result = {0};
    const char *input = "Apple은 사과입니다.";
    ttf_status_t status;
    ttf_config_init(&config);
    status = ttf_engine_create(&config, &engine);
    if (status == TTF_OK) status = ttf_engine_freeze(engine);
    if (status == TTF_OK)
        status = ttf_normalize(engine, input, strlen(input), &result);
    if (status == TTF_OK) puts(result.text);
    ttf_result_free(&result);
    ttf_engine_destroy(engine);
    return status == TTF_OK ? 0 : 1;
}
```

Initialize configuration with `ttf_config_init`. Register packs and load overrides
before `ttf_engine_freeze`. After freezing, share an engine among requests and
create a separate stream for each request. Destroy all streams before their engine.
See [API ownership and lifecycle](docs/api.md) and [the complete example](examples/basic.c).

## Resource customization

UTF-8 TSV resources separate normalization rules from pronunciation dictionaries.
The root `manifest.ttfm` lists system files in declaration order. Load explicit
files or compiled resources into a domain/customer layer before freezing:

```c
ttf_resource_load(engine, "resources/domains/finance/ko-KR/rules.tsv", TTF_LAYER_DOMAIN);
ttf_resource_load(engine, "resources/customers/example/ko-KR/rules.tsv", TTF_LAYER_CUSTOMER);
```

The same `123-456` input has resource-selected behavior:

| Resources | Output |
| --- | --- |
| System | 백이십삼 다시 사백오십육 |
| Finance domain | 일이삼 사오육 |
| Example customer | 일이삼 번에 사오육 |

```sh
build/ttf normalize --domain resources/domains/finance/ko-KR/rules.tsv '123-456'
build/ttf normalize --domain resources/domains/finance/ko-KR/rules.tsv \
  --customer resources/customers/example/ko-KR/rules.tsv '123-456'
python3 tools/resource_compiler/compile_resources.py \
  resources/domains/finance/ko-KR/rules.tsv /tmp/finance.ttfb
build/ttf normalize --domain /tmp/finance.ttfb '123-456'
```

See [resources and priority](docs/resources.md) for the DSL and compiler boundary.

## Lexicon customization

```c
ttf_lexicon_add(engine, TTF_LAYER_USER, "en-US", "ko-KR", "Apple", "사용자애플");
ttf_lexicon_remove(engine, TTF_LAYER_USER, "en-US", "ko-KR", "Apple");
```

Pronunciation priority is **User > Customer > Domain > System**, followed by the
source pronunciation dictionary, source G2P, target phone renderer, and emergency
grapheme renderer. Removing an override reveals the next layer. Unknown Latin
words in Korean context produce speakable Hangul; this fallback is approximate.
See [lexicon](docs/lexicon.md) and [pronunciation](docs/pronunciation.md).

## Streaming

```c
static int on_segment(const ttf_segment_t *segment, void *user_data) {
    /* Send or copy text and metadata to your TTS queue here. */
    return fwrite(segment->text, 1, segment->length, (FILE *)user_data)
           != segment->length;
}
/* engine has already been frozen */
ttf_stream_t *stream = NULL;
ttf_stream_create(engine, on_segment, stdout, &stream);
ttf_stream_write(stream, "가격은 12", strlen("가격은 12"));
ttf_stream_write(stream, ",500원입니다.", strlen(",500원입니다."));
ttf_stream_flush(stream);
ttf_stream_destroy(stream);
```

Check each status in application code. Writes may end inside a UTF-8 character or
numeric expression. Pending text waits for a stable boundary; `flush` finalizes
it. The same planner is used for batch and streaming so concatenated callback text
matches batch output for the same configuration and resources. Targets for first
and subsequent segments, maximum length, sentence/phrase splitting and pending
buffer limit are configurable. Callback pointers live only during the callback.
See [streaming invariants and latency tradeoffs](docs/streaming.md).

## Architecture and scope

[Validation results](docs/validation.md) records the executed checks and required outputs.
[Architecture](docs/architecture.md) describes the six milestones and decisions.
[Language pack contract](docs/language-pack.md) explains extension without core
language branches. [Semantic IR](docs/semantic-ir.md) describes shared types.
[Korean coverage](docs/ko-coverage.md) records tested readings and linguistic limits.

Current production work remains: broader context heuristics and corpus, full
Unicode properties and numeric grammars, larger licensed pronunciation resources,
higher quality G2P/phonetic mapping, long-input performance profiling, automated ABI compatibility
checks, and Windows/macOS CI. Integration with a real TTS engine can begin using
the callback and span APIs; pronunciation quality still requires evaluation for
each voice/domain. The library does not synthesize audio.

Private proprietary project. See [LICENSE](LICENSE).
