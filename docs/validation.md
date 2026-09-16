# Initial implementation validation

Validated on Linux on 2026-09-16 with GCC 13.3 and Clang 18.1, C11 and CMake 3.28.
The repository workflow repeats GCC, Clang and Clang ASan/UBSan configurations.

## Executed checks

* Both `libtts_text_frontend.a` and `libtts_text_frontend.so` build together.
* Shared ABI uses SONAME `libtts_text_frontend.so.1`; only public API symbols export.
* GCC Debug and Clang Release: all 17 CTest cases pass.
* Clang Debug with AddressSanitizer, UndefinedBehaviorSanitizer and leak detection:
  all 17 CTest cases pass.
* Golden corpus: 73 cases across Korean, English, code mixed, foreign pronunciation,
  streaming, domain and customer categories.
* Streaming: 717 representative comparisons (all two-chunk byte splits, one-byte
  writes, deterministic random chunking), plus all byte splits of a spaced date
  under a nine-character target.
* Generative test: 200 mixed inputs, 1,000 byte-chunk comparisons, 5,000 scanner probes.
* Frozen-engine concurrency: eight threads, 800 requests.
* Korean pack edges: numeric magnitudes, phone zero, phone mapping and dictionary-free
  grapheme fallback, including all ASCII letter names.
* Source/compiled resource equivalence, corruption/truncation rejection,
  transactional malformed resource loading and layer precedence.
* A synthetic third-language pack registers and verbalizes without core changes.
* Installed CLI works with an explicit installed resource root.

## Required examples

```text
1st는 첫 번째라는 뜻입니다.
영어로는 다음과 같은 예문이 있습니다.
Please go to the 1st floor
```

produces:

```text
퍼스트는 첫 번째라는 뜻입니다.
영어로는 다음과 같은 예문이 있습니다.
Please go to the first floor
```

| Input | Actual normalized output |
| --- | --- |
| Apple은 사과입니다. | 애플은 사과입니다. |
| Apple released the iPhone. | Apple released the iPhone. |
| GPU를 사용합니다. | 지피유를 사용합니다. |
| WiFi를 사용합니다. | 와이파이를 사용합니다. |
| OpenAI가 발표했습니다. | 오픈에이아이가 발표했습니다. |
| foobar는 예시입니다. | 푸바는 예시입니다. |
| 123-456, system | 백이십삼 다시 사백오십육 |
| 123-456, finance | 일이삼 사오육 |
| 123-456, customer | 일이삼 번에 사오육 |

`ttf inspect` exposes source/context/render tags, IR value, rule and pronunciation
layers, context scores, lexicon/fallback provenance and byte offsets. These tests
verify behavior and integration boundaries, not linguistic quality over unrestricted
input. See the architecture milestone table and Korean coverage document for limits.

## 0.2.0 integration checkpoint

On 2026-09-16, GCC Debug, Clang Release and Clang ASan/UBSan each passed all
**21 CTest tests**. The added checks cover the JSONL protocol (including first
segment delivery before pipe EOF), benchmark accounting, evaluation reports and
real reference speech synthesis with eSpeak NG 1.51. Public ABI headers, core
sources and language resources are unchanged from the initial implementation.

The separate listening corpus has 21 cases. All 21 normalized strings match their
expected values and all 21 produce readable, nonempty PCM WAVs. One case uses the
prototype G2P path. Human listening scores remain pending; successful audio
production is not a quality approval. The HTML/JSON report, CSV worksheet and WAVs
are generated artifacts, also published by the `tts-integration` CI job.

### Measured frontend baseline

Clang 18.1 Release, Linux VM, warm frozen engine, 64-byte writes, default segment
targets. Small workload is `examples/code_mixed.txt`; large is that file repeated
256 times. These are local workload measurements, not latency guarantees.

| Workload | Input bytes | Iterations | First callback p50 / p95 | Total p50 / p95 | Throughput |
| --- | ---: | ---: | ---: | ---: | ---: |
| Small | 194 | 100 | 0.067 / 0.115 ms | 0.325 / 0.546 ms | 0.533 MiB/s |
| Repeated | 49,664 | 20 | 0.039 / 0.044 ms | 49.321 / 51.672 ms | 0.953 MiB/s |

Every measured iteration delivered its first callback after a submitted input
extent of 64 bytes (37 committed bytes), before the caller supplied all remaining
input. Exact batch/stream comparison runs before timing; every timed run verifies
output length, hash and callback accounting. Timing excludes engine setup and
acoustic synthesis and must not be described as audio TTFB. See
[benchmark definitions](benchmark.md) and [audio integration timing](tts-integration.md).
