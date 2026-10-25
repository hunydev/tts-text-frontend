# 멀티스레드 사용법 (2026-10-25)

## 규칙

| 단계 | 호출 | 스레드 |
|---|---|---|
| 엔진 준비 | `ttf_engine_create` → `ttf_resource_load`·`ttf_lexicon_add`·`ttf_engine_set_frontend_options` → `ttf_engine_freeze` | 한 스레드에서 한 번. 고정(freeze) 전에는 동시 호출 금지 |
| 정규화 | `ttf_normalize(engine, …)` | **고정된 엔진 하나를 모든 스레드가 함께 쓴다.** 스레드별 엔진이나 락이 필요 없다 |
| 결과 해제 | `ttf_result_free` | 결과를 받은 스레드가 해제 |
| 스트림 | `ttf_stream_create` → `write`/`flush` → `destroy` | **스트림 하나는 한 스레드만.** 스트림끼리는 동시에 돌아도 된다(같은 엔진 공유) |
| 요청 사전 | `ttf_dict_create` → `add`/`load` → `ttf_normalize_with`·`ttf_stream_set_dict` → `destroy` | **요청이 쓰는 동안 바꾸지 않는다.** 아무도 바꾸지 않는 사전은 여러 요청이 함께 써도 된다 ([request-dictionary.ko.md](request-dictionary.ko.md)) |
| 엔진 해제 | `ttf_engine_destroy` | 모든 요청·스트림이 끝난 뒤 한 번 |

- 고정 뒤에는 리소스·옵션을 바꿀 수 없다(`TTF_ESTATE`). 요청 언어(`default_language`)와 프런트엔드 옵션은 엔진 단위이므로, 조합마다 엔진을 하나씩 만들어 고정해 두고 요청을 그 엔진으로 보낸다.
- 고객사·요청마다 다른 낱말은 엔진을 새로 만들지 않고 요청 사전으로 준다(4스레드가 각자 다른 사전을 쓰는 시험, ThreadSanitizer 경고 0건).
- 서비스 중 엔진 사전을 바꾸려면 새 엔진을 만들어 고정하고, 새 요청부터 새 엔진으로 보낸 뒤, 옛 엔진은 진행 중인 요청이 끝나면 해제한다.
- 콜백(스트림)에서 받은 데이터는 콜백 안에서 복사한다. 같은 스트림으로 다시 들어오는 호출은 지원하지 않는다.

```c
/* 시작할 때 한 번 */
ttf_engine_t *ko;
ttf_config_init(&config); config.resource_root = "resources"; config.default_language = "ko-KR";
ttf_engine_create(&config, &ko);
ttf_resource_load(ko, "user-dict.tsv", TTF_LAYER_USER);   /* 사용자 사전 */
ttf_frontend_options_init(&options); options.output_mode = TTF_OUTPUT_SPEECH;
ttf_engine_set_frontend_options(ko, &options);
ttf_engine_freeze(ko);

/* 요청마다, 아무 스레드에서나 */
ttf_result_t result;
if (ttf_normalize(ko, text, length, &result) == TTF_OK) { /* result.text */ ttf_result_free(&result); }

/* 끝낼 때 한 번 (요청이 모두 끝난 뒤) */
ttf_engine_destroy(ko);
```

## 검증

- ThreadSanitizer 빌드로 4스레드가 엔진 하나를 공유해 v3 한국어 300문장(사전 없이, 10만 개 사전 포함 두 경우)과 사전 단어 문장 200개를 처리: 데이터 경합 경고 0건, 출력 불일치 0건.
- CI의 `concurrent_shared_engine` 시험(8스레드 800요청)도 같은 빌드에서 통과.

## 다중 채널 동시 처리 (스레드만, 2026-10-25)

채널 하나 = 스레드 하나. 각 채널이 문장 997개(v3 한국어 중 cp949로 쓸 수 있는 것)를 차례로 요청한다. 두 엔진 모두 C에서 직접 호출, VM 2코어, 3회 중앙값. 지연은 요청 하나의 시간. 출력 불일치·크래시 0건.

| 채널 | TTF 문장/초 | TTF p50 / p95 / p99 | xvoice5 락 문장/초 | 락 p50 / p99 | xvoice5 락 없음 문장/초 | 락 없음 p50 / p99 |
|---|---|---|---|---|---|---|
| 1 | 3,556 | 0.27 / 0.51 / 0.83ms | 1,043 | 0.90 / 2.9ms | 1,022 | 0.91 / 2.4ms |
| 2 | 7,297 | 0.24 / 0.40 / 0.70ms | 1,009 | 0.95 / 2.5ms | 1,974 | 0.89 / 2.1ms |
| 4 | 7,406 | 0.26 / 4.2 / 4.5ms | 975 | 0.96 / 8.4ms | 1,963 | 1.02 / 8.7ms |
| 8 | 7,495 | 0.26 / 12.2 / 12.6ms | 954 | 0.99 / 148ms | 1,782 | 1.07 / 17ms |
| 16 | 6,505 | 0.29 / 28 / 32ms | 978 | 0.97 / 324ms | 1,881 | 1.05 / 38ms |

- TTF는 엔진 하나를 모든 채널이 공유하고, 코어 수까지 처리량이 늘어 초당 약 7,400문장에서 멈춘다. 채널이 코어보다 많으면 요청이 CPU를 기다리므로 p95·p99가 채널 수에 비례해 늘고, p50(처리 자체)은 그대로다. 지연을 짧게 두려면 동시 처리 채널을 코어 수 정도로 두고 나머지는 대기열로 받는다.
- xvoice5를 래퍼처럼 전역 락으로 부르면 채널 수와 상관없이 초당 약 1,000문장이고, 락이 공정하지 않아 8채널 이상에서 p99 148–324ms, 최대 2.9초까지 밀린다.
- xvoice5를 락 없이 동시에 부르면 2코어에서 초당 약 2,000문장이고 이번 시험(약 9만 3천 건)에서는 출력이 어긋나지 않았다. 다만 재진입 가능하다는 보장이 없고 바이너리라 ThreadSanitizer로 검사할 수 없다.

## 성능 (VM 2코어, v3 한국어 1,000문장, 3회 중앙값, 출력 불일치 0건)

| | 1 | 2 | 4 | 8 |
|---|---|---|---|---|
| TTF 스레드 (엔진 하나 공유) | 2,943문장/초 | 6,965 | 5,270 | 6,679 |
| TTF 스레드 + 백만 개 사전 (1회) | 3,127 | 5,940 | 5,955 | — |
| xvoice5 스레드 (전역 락) | 918 | 705 | 663 | — |
| xvoice5 프로세스 (fork, 엔진 사본) | 877 | 1,456 | 1,651 | — |

- TTF는 코어 수(2)까지 거의 그대로 늘고, 그 뒤로는 평평하다. 사전이 백만 개여도 같다.
- xvoice5는 `text2normal_ex`의 재진입이 보장되지 않아 래퍼가 전역 락으로 직렬화하므로 스레드를 늘리면 오히려 느려진다. 병렬은 프로세스를 여러 개 띄워야 하고(엔진·사전이 프로세스마다 따로), 그래도 TTF 한 스레드보다 느리다.
- TTF는 C에서 직접, xvoice5는 패키지 래퍼(Python, ctypes)로 쟀다. xvoice5는 C 진입점에 락이 없어 Python 밖에서 부를 때도 같은 직렬화가 필요하다.
