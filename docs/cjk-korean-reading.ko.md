# CJK 문자의 한국어 표기 정규화 (한자 · 가나)

상태: **설계 확정, 작업 0·4 준비 완료, 구현 착수 보류.** 2026-09-23.
대상 릴리스: v0.20.0 이후. public ABI v1 유지를 전제로 한다.
측정 기준: Unicode **18.0.0** (Unihan, 2026-07-31).

이 문서는 저장소 조사와 실측을 근거로 작성했다. 설계 쟁점은 모두 정리했고
(결정 내역은 §16), 착수 전 준비 산출물은 §17에 있다.
**구현 승인 전까지 제품 코드 변경은 없다.**

---

## 1. 범위

Unicode Han 문자와 일본어 가나를, **사용자가 명시적으로 선택했을 때만**
한국어 정규 표기(Hangul orthography)로 정규화한다.

두 기능은 **목적이 서로 다르다**. 이것은 의도된 설계이며 버그가 아니다.

| 입력 | 옵션 | 출력 | 근거 |
|---|---|---|---|
| `東京` | `hanja=korean` | `동경` | 한국 한자음 |
| `とうきょう` | `kana=korean` | `도쿄` | 일본어 → 한국어 외래어 표기 |

같은 대상이라도 **입력 표기가 다르면 출력이 다르다.** 한자 기능은 번역기가
아니고(`北京 → 북경`이지 `베이징`이 아니다), 가나 기능은 일본어 TN이 아니다
(형태소 분석·읽기 추정을 하지 않는다).

### 의도와 성공 기준

**이 기능의 목적은 발성 가능 범위의 확장이지, 일본어/중국어 처리 능력이 아니다.**

메인은 한국어다. 한국어 음운 구조가 한자·일본어 표기를 읽어낼 수 있으므로,
"한글/영어까지" 이던 발성 범위를 **"한자/가나까지는 읽기는 한다"** 로 넓히는 것이
전부다.

| | 기준 |
|---|---|
| ✅ 성공 | 한국어 문장에 섞인 CJK 조각이 **묵음·깨짐 대신 읽힌다** |
| ✅ 허용 | 부분 변환. 문장의 일부만 변환되어도 된다 |
| ✅ 허용 | 미해결 문자가 원문으로 남는 것 |
| ❌ 목표 아님 | 일본어/중국어 문장 전체의 정확한 처리 |
| ❌ 목표 아님 | repertoire 커버리지 수치 자체 |

따라서 `--kana korean` 만 켠 상태에서 `東京とうきょう → 東京도쿄` 처럼 절반만
변환되는 것은 **정상 동작이자 허용된 중간 상태**다. 완결성을 위해 범위를 늘리지
않는다.

이 기준은 기능별 기본값의 근거이기도 하다. 한자는 downstream 다국어 TTS가
중국어로 읽을 수 있으므로 `passthrough`가 실질적 선택지지만, 가나는 한국어 TTS가
아예 읽지 못한다 — 현재 `--unreadable drop` 에서 그냥 사라지는 구간이 바로
이 기능이 개선하는 대상이다.

### 범위 밖

- 일본어 TN (한자 읽기 추정, 형태소 분석, 오쿠리가나 처리)
- 중국어 transliteration (`北京 → 베이징`)
- 실제 음성 표면 발음 (`국립 → 궁닙`). 이것은 기존 G2P/발음 계층의 책임이다.
- 신규 language pack 추가. 두 기능 모두 **ko-KR의 스크립트 처리 규칙**이다.
- **CJK 커버리지 확대를 위한 후속 범위 확장.** 사전을 키우는 것은 별도 판단이다.

```
Han / Kana
    ↓
[ 이 문서의 범위 ]  CJK → Hangul orthography
    ↓
기존 Korean G2P / 발음 계층
    ↓
실제 pronunciation
```

---

## 2. 현재 구조 (조사 결과)

### 2.1 파이프라인

```
ttf_normalize (src/core/normalize.c:709)
  └ ttf_next_boundary (src/segmenter/segmenter.c:349)      문장/구 경계
      └ ttf_process_unit (src/core/normalize.c:210)        batch·streaming 공용
          ├ ttf_tokenize_protected                          보호 리터럴 + 스크립트 런 토큰화
          ├ context_pack / script_pack                      언어 라우팅 (점수 기반)
          ├ ttf_classify_universal + pack->classify          Semantic IR
          ├ ttf_rule_match (src/rule/rule.c:25)              exact/pattern 치환
          ├ ttf_phrase_match / ttf_foreign_lookup            다어절·외래어 렉시콘
          ├ renderer->verbalize                              IR → 텍스트
          └ ttf_speech_output (speech 모드)                   코드포인트 단위 최종 패스
```

### 2.2 확인한 사실

| 항목 | 상태 | 출처 |
|---|---|---|
| Han·가나 스크립트 분류 | **없음.** 둘 다 `TTF_SCRIPT_OTHER` | `src/tokenizer/tokenizer.c` `ttf_script_of` |
| 토큰 병합 | 연속 `OTHER`는 **한 토큰으로 병합** | 같은 파일 `ttf_tokenize` |
| source pack | ko=HANGUL만, en=LATIN만 채점 → CJK는 `source=NULL` | `languages/*/pack.c` `script_score` |
| 기본 verbalize | `default: append(surface)` (identity) | `languages/ko-KR/pack.c:307` |
| 옵션 struct | `ttf_frontend_options_t`, `struct_size`/`abi_version` 보유 | `include/tts_text_frontend/ttf_frontend_options.h` |
| 리소스 저장 | `(kind, lang, key)` FNV 해시, 1021 버킷, 레이어+order 우선 | `src/resource/resource.c` |
| 규칙 매칭 | **`e->rules` 연결 리스트 전수 선형 스캔** | `src/rule/rule.c:25` |
| 레이어 우선순위 | `COMMON < SYSTEM < DOMAIN < CUSTOMER < USER` 전역 정책 | `include/tts_text_frontend/ttf.h` |
| 미판독 문자 | speech 모드 최종 패스 끝 `unknown:` → preserve/drop | `src/core/speech_output.c:193` |
| 구 경계 | 공백에서만 자름 | `src/segmenter/segmenter.c` |
| 생성 데이터 관례 | 생성 TSV를 커밋 + CI `build --check`로 소스 일치 검증 | `CMakeLists.txt:227`, `.github/workflows/ci.yml:62` |

### 2.3 실측 (v0.20 빌드, `build/ttf`)

**현재 기본 동작이 이미 PASSTHROUGH다.**

```
$ ttf normalize --output-mode text   "오늘 中國 시장이 열렸습니다."   → 오늘 中國 시장이 열렸습니다.
$ ttf normalize --output-mode speech "大韓民國 Korea 中国 日本"       → 大韓民國 Korea 中国 日本
$ ttf normalize --output-mode speech --unreadable drop "오늘 中國 시장이…"
                                                                    → 오늘 시장이 열렸습니다.
```

가나도 동일하다.

```
$ ttf inspect "오늘 とうきょう 와 サッポロ 여행"
surface="とうきょう"  script=OTHER  source=und  semantic=WORD  rule=identity  final="とうきょう"
surface="サッポロ"    script=OTHER  source=und  semantic=WORD  rule=identity  final="サッポロ"
```

**기존 `exact` 규칙만으로 한자어 치환이 코드 변경 없이 동작한다** (개념 증명).

```
$ printf 'exact\tko-KR\t大韓民國\t대한민국\n' > /tmp/probe.tsv
$ ttf normalize --customer /tmp/probe.tsv "오늘 大韓民國 시장"     → 오늘 대한민국 시장
$ ttf inspect   --customer /tmp/probe.tsv "大韓民國"
  semantic=CUSTOM rule=exact:大韓民國 layer=customer verbalizer=ko-KR input=[0,12)
```

**부분 매치도 안전하다.** `山`만 등록하고 `山水` 입력:

```
normalized="산水"    # span 2개: [0,3) 산(CUSTOM), [3,6) 水(WORD, identity)
```

즉 "단어 사전 우선 → 글자 fallback → 미해결 원문 보존"이 기존 매처의 최장일치와
레이어 규칙만으로 성립한다. **새 매칭 엔진을 만들 필요가 없다.**

### 2.4 ⚠️ 측정된 성능 함정

`ttf_rule_match`는 모든 토큰마다 전체 규칙 리스트를 선형 스캔한다.
한자 18,000자를 `exact` 규칙으로 넣고, **한자가 하나도 없는** 순수 한국어 13KB를 처리:

| | 시간 |
|---|---|
| 기준 (규칙 추가 없음) | **0.153 s** |
| `exact` 규칙 18,000개 | **1.319 s** |

**8.6배 회귀.** 출력이 같아도 처리량이 1/8이면 배포할 수 없다.

> **결정: 한자 독음 데이터를 `exact`/`pattern` 규칙 리스트에 싣지 않는다.**
> 개념 증명용으로만 쓰고, 제품 데이터는 전용 인덱스로 간다 (§5).

가나는 현재 301행(초안 306행)이라 단독 측정에서는 이 문제가 없었다. 그래도 같은 스캐너를 공유하므로 동일 구조로 간다.

---

## 3. 공개 인터페이스

### 3.1 옵션 (결정: 분리 옵션 / 통합 구현)

```c
typedef enum {
    TTF_HANJA_PASSTHROUGH = 0,   /* 기본값 — 기존 동작과 동일 */
    TTF_HANJA_KOREAN      = 1
} ttf_hanja_mode_t;

typedef enum {
    TTF_KANA_PASSTHROUGH = 0,    /* 기본값 — 기존 동작과 동일 */
    TTF_KANA_KOREAN      = 1
} ttf_kana_mode_t;
```

CLI는 기존 `--unreadable preserve|drop` 형태를 따른다.

```
--hanja passthrough|korean
--kana  passthrough|korean
```

웹 워크벤치는 `tools/web/workbench.py`의 `DEFAULTS`에 두 항목, `validate()`의
enum 검사 목록에 두 줄.

**옵션을 분리하는 이유**: 한자는 "중국어/일본어 TTS에 그대로 넘길지"라는 downstream
판단이 걸려 있어 `passthrough`가 실질적 선택지다. 가나는 한국어 TTS가 읽지 못하므로
사실상 `korean`만 유용하다. 두 옵션의 합리적 기본값이 장래에 갈릴 수 있다.

**단 v1에서는 둘 다 `passthrough`가 기본값이다** (§9 zero-impact).

### 3.2 혼재 입력 계약

통합 스캐너는 런을 훑으면서 **코드포인트마다 자기 스크립트의 옵션**을 본다.
꺼진 스크립트는 원문 바이트를 그대로 흘리고 단어 경계만 끊는다.

| `--hanja` | `--kana` | `東京とうきょう` |
|---|---|---|
| passthrough | passthrough | `東京とうきょう` |
| korean | passthrough | `동경とうきょう` |
| passthrough | korean | `東京도쿄` |
| korean | korean | `동경도쿄` |

### 3.3 ⚠️ ABI 문제

`ttf_engine_set_frontend_options` (`src/core/frontend_options.c:21`)는
`options->struct_size != sizeof(*options)`로 **정확일치**를 요구한다.
필드를 추가하면 기존에 컴파일된 호출자가 전부 `TTF_EINVAL`을 받는다.
배포된 `build-web/libtts_text_frontend.so`와 CLI가 정확히 이 관계다.

| 안 | 내용 | 평가 |
|---|---|---|
| **A** | 필드 추가 + 검사를 "v0.20 크기 이상이면 허용, 부족한 꼬리는 기본값" 으로 완화 | **채택.** 옵션이 한 곳에 모이고 구버전 바이너리도 동작 |
| B | 새 struct + `ttf_engine_set_script_options` 신설 | 가장 보수적. 옵션이 두 군데로 갈라짐 |
| C | 리소스 값으로만 제어 | API 변경 0. 요청별 전환 불가 |

> **결정: A.** `ttf_frontend_options_t`에 두 필드를 추가하고,
> `ttf_engine_set_frontend_options`의 크기 검사를 정확일치에서
> **"v0.20 크기 이상"** 으로 완화한다. 부족한 꼬리는 `passthrough` 기본값으로 채운다.
> `abi_version`은 1을 유지한다.

이는 `struct_size` 정확일치라는 기존 계약의 **의도적 완화**이므로 다음을 지킨다.

- v0.20 크기로 호출한 구버전 바이너리는 **기존과 완전히 동일하게** 동작해야 한다
  (두 옵션 모두 `passthrough`).
- 알 수 없이 **큰** `struct_size`는 계속 거부한다.
- 이 완화 자체에 대한 계약 테스트를 추가한다 (구버전 크기 / 신버전 크기 /
  과대 크기 3케이스).

참고로 이 프로젝트는 과거 `ttf_config_t`를 확장하지 않고 `ttf_frontend_options_t`를
따로 만든 전례가 있다(= B를 택한 적이 있다). 이번에는 옵션 분산을 피하기 위해 A로 간다.

---

## 4. 통합 스캐너

### 4.1 왜 통합인가

실제 일본어 텍스트는 漢字かな混じり다. 그리고 토크나이저가 연속 `OTHER`를
병합하므로 **한자와 가나가 한 토큰 안에 섞여 들어온다.**

```
"東京とうきょう"   → [OTHER "東京とうきょう"] 1토큰
"サッポロビール"   → [OTHER "サッポロビール"] 1토큰
```

두 기능을 독립 단계로 만들면 같은 토큰을 두 번 스캔하거나 경계 처리가 꼬인다.
하나의 스캐너가 코드포인트 단위로 훑으면서 위치별 스크립트에 따라 테이블을
고르는 구조가 맞다.

### 4.2 알고리즘

```
scan_cjk_run(run):
    pos = run.begin
    word_initial = true
    while pos < run.end:
        script = classify(cp_at(pos))          # han | kana | other

        if not enabled(script):                # 옵션이 꺼진 스크립트
            emit_source_bytes(pos, width)
            pos += width
            word_initial = true                # 보존 구간 뒤는 다시 어두
            continue

        # 1. 단어 사전 최장일치 (user → domain → system)
        hit = lookup_word(pos, script)
        if hit:
            emit(hit.hangul)
            pos += hit.byte_length
            word_initial = false
            continue

        # 2. 글자/시퀀스 테이블 최장일치
        hit = lookup_reading(pos, script)      # hanja_reading | kana_reading
        if hit:
            emit(apply_initial_variant(hit, word_initial, script))
            pos += hit.byte_length
            word_initial = false
            continue

        # 3. 미해결 → 원문 코드포인트 보존
        emit_source_bytes(pos, width)
        trace("cjk-unresolved", cp)
        pos += width
        word_initial = true
```

**미해결 시 `?`·`UNK` 같은 문자열을 절대 삽입하지 않는다.** 통계는 trace 이벤트로만
노출한다 (`ttf_trace_emit`, 기존 인프라 재사용).

### 4.3 파이프라인 내 위치

`ttf_process_unit`에서 토큰 라우팅이 끝나고 `renderer->verbalize` 전에 수행한다.
출력은 Hangul orthography이므로 이후 단계(발음, speech 최종 패스)가 정상 동작한다.

신규 파일: `src/cjk/cjk_reading.c`, `src/cjk/cjk_reading.h`.

### 4.4 Semantic IR 표시

개념 증명에서 `exact` 규칙은 `semantic=CUSTOM rule=exact:大韓民國`을 냈다.
제품 경로에서는 더 정확한 표시가 필요하다.

```
sem->type          = TTF_SEM_WORD
sem->lexical_class = TTF_LEX_WORD
sem->rule          = "cjk.hanja.word" | "cjk.hanja.char"
                   | "cjk.kana.seq"   | "cjk.kana.char"
sem->layer         = 적중 레이어
span.lexicon       = "lexicon:<layer>"  (사전 적중 시)
```

`ttf_semantic_type_t`에 새 값을 추가하지 않는다 (ABI 변경 회피).

---

## 5. 데이터 계층과 우선순위

기존 전역 레이어 정책을 그대로 따른다. **별도 우선순위 체계를 만들지 않는다.**

```
user lexicon      TTF_LAYER_USER
   ↓
domain lexicon    TTF_LAYER_DOMAIN
   ↓
system 단어 사전   TTF_LAYER_SYSTEM     cjk_word
   ↓
문맥 규칙          (어두/두음 변이)
   ↓
글자 테이블        TTF_LAYER_SYSTEM     hanja_reading / kana_reading
   ↓
미해결 → 원문 보존
```

### 5.1 리소스 kind

```
cjk_word        ko-KR   大韓民國     대한민국
cjk_word        ko-KR   男女老少     남녀노소        ← 두음 fallback으로 못 푸는 복합어
cjk_word        ko-KR   ソウル       서울            ← 카타카나 외래어 예외
hanja_reading   ko-KR   山           산
hanja_reading   ko-KR   女           녀              ← canonical. 두음 적용 전
hanja_reading   ko-KR   樂           악|락|요        ← 복수 독음, 첫 항목이 preferred
kana_reading    ko-KR   か           가|카           ← 어두|어중. §7 참조
kana_reading    ko-KR   とう         도              ← 장음 흡수 (최장일치)
kana_reading    ko-KR   さっ         삿              ← 촉음 흡수 (최장일치)
```

사용자 override는 기존 `--customer` / `--user` TSV와 `ttf_lexicon_add`로 그대로 동작한다.

### 5.2 자료구조

| 테이블 | 엔트리 | 구조 |
|---|---|---|
| `hanja_reading` | **8,116** | **freeze 시 전용 인덱스로 분리** (§5.4) |
| `kana_*` | **301** | 동일 |
| `cjk_word` | **5** | 동일. 첫 코드포인트 → 길이 내림차순 후보 |

`cjk_word` 인덱스는 `ttf_phrase_index`가 `engine->phrases`를 만드는 방식과
동일한 패턴이다. 새 개념이 아니다. 최대 단어 길이를 6자로 제한하면 위치당
비교 횟수가 상수로 묶인다.

**`e->rules` 는 절대 쓰지 않는다** (§2.4).

### 5.4 ⚠️ CJK 행은 공용 리소스 해시에 두지 않는다 (3단계 실측으로 정정)

> **초안 오류:** 이 절의 초안은 `hanja_reading` 을 "기존 리소스 해시 (O(1))" 에
> 두었다. 3단계 성능 게이트가 이를 반증했다.

공용 해시는 모든 토큰의 문맥 점수·`speech_symbol`·`classifier_priority` 조회가
수십 번씩 훑는다. CJK 행이 같은 버킷에 섞이면 **한자가 없는 입력도 느려진다.**

| 공용 해시 (1021 버킷) | 행 수 | 버킷당 체인 | passthrough 중앙값 |
|---|---|---|---|
| CJK 이전 | 3,459 | 3.4 | 71.75 ms (1.00×) |
| CJK 포함 | 11,881 | 11.6 | 109.23 ms (**1.52×**) |
| + 단어 18,000행 | 29,881 | 29.3 | 202.00 ms (**2.82×**) |

§9 ③ 게이트(1.25×)를 넘는다. 옵션이 꺼져 있어도 행은 로드되므로
**"passthrough 는 비용 0" 이라는 §9 의 전제가 깨진다.**

#### 버킷 수만 늘리는 것은 함정이다

엔진 구조체에 버킷 배열이 **인라인으로** 들어 있고, 처리 단위마다 구조체 전체를
복사한다.

```c
src/core/internal.h     ttf_entry_t *resources[TTF_HASH_BUCKETS];
                        ttf_entry_t *phrases[TTF_HASH_BUCKETS];
                        ttf_entry_t *cjk_words[TTF_HASH_BUCKETS];
src/core/normalize.c    ttf_engine_t context_view = *e;   /* 단위마다 */
src/core/normalize.c    ttf_engine_t view = *e;           /* electronic_context */
```

1021 버킷 × 3 배열이면 단위당 약 24 KB 복사다. `TTF_HASH_BUCKETS` 를 16381 로
올리면 **단위당 약 390 KB** 가 되어 체인 문제보다 나빠진다. 인라인 `cjk_words`
배열 자체도 복사량을 8 KB 늘린다.

#### 결정: freeze 시 CJK 행을 전용 인덱스로 분리한다

- `hanja_reading`, `kana_reading`, `kana_mark`, `kana_fold`, `kana_long_after`,
  `cjk_word` 행은 **지금처럼 manifest 로 로드하고 validator 를 통과**한다.
  리소스 형식·레이어·사용자 override 는 바뀌지 않는다.
- **freeze 시점에** 이 종류의 엔트리를 공용 해시에서 떼어 CJK 전용 인덱스로 옮긴다.
  공용 해시는 3,459행으로 돌아간다.
- 전용 인덱스는 **힙에 할당하고 엔진 구조체에는 포인터 하나만** 둔다. 인라인
  `cjk_words[TTF_HASH_BUCKETS]` 배열은 제거한다.
- 인덱스는 레이어 우선순위를 `find_entry` 와 같은 규칙으로 유지한다
  (높은 layer 우선, 같은 layer 면 낮은 order 우선).
- 엔트리 자체는 옮기기만 하고 해제하지 않는다. trace 의 `layer` 와
  `declaration_order` 는 그대로 유효하다.

확인할 것:
- `ttf_engine_validate_resources` 는 freeze 후에도 호출될 수 있다
  (`ttf_resource_validation.h`). 옮긴 행이 보고서의 record 수에서 빠지면 안 된다.
- freeze 후 `ttf_resource_get(e, "hanja_reading", ...)` 같은 공개 조회가
  무엇을 돌려줄지 정한다. 공개 API 동작을 바꾸지 않으려면 전용 인덱스로
  위임하거나, 기존처럼 찾히도록 유지해야 한다.
- 엔진 destroy 에서 전용 인덱스와 옮긴 엔트리가 한 번씩만 해제되어야 한다.

**후속 개선(이번 범위 아님):** 공용 해시 자체를 힙 할당·freeze 시 크기 결정으로
바꾸면 앞으로의 리소스 증가 전반에 이롭다. 다만 모든 팩이 쓰는 핵심 저장소를
바꾸는 일이므로 CJK 작업과 분리한다.

### 5.5 CJK 리소스는 옵션이 켜졌을 때만 로드한다 (4단계 실측으로 추가)

§5.4 로 공용 해시는 깨끗해졌지만 **로드 자체의 비용**은 남았다. 옵션이 꺼져
있어도 manifest 가 8,422행을 파싱·검증하기 때문이다 (§9.2 실측).

요구사항:
- 두 옵션이 모두 `passthrough` 인 엔진은 **CJK 파일을 열지도 않는다.**
  `ttf_engine_set_frontend_options` 를 호출하지 않는 레거시 엔진도 같다.
- `kana=korean` 이면 가나 파일만, `hanja=korean` 이면 한자 파일만,
  둘 중 하나라도 켜지면 `cjk_words.tsv` 를 로드한다.
- 로드는 **freeze 안에서, frozen 표시 전에** 한다. 옵션은 freeze 전에만
  바뀔 수 있으므로 이 시점에 확정되어 있다. 공개 API 의 "freeze 후 로드는
  `TTF_ESTATE`" 계약은 바뀌지 않는다.
- layer 는 `SYSTEM`, validator 는 manifest 로드와 동일하게 적용, 로드는
  transactional 이다. 실패하면 freeze 가 깨끗하게 실패해야 한다.
- 사용자·고객이 freeze 전에 올린 `cjk_word` 등 override 는 지금처럼 공용
  해시에 들어갔다가 freeze 에서 전용 인덱스로 옮겨진다. 레이어 우선순위 불변.

구현 선택지 (세션이 판단):
- (a) `resources/manifest.ttfm` 에서 CJK 3줄을 빼고 별도 `resources/cjk.ttfm` 에
  옵션별로 등록한다. freeze 가 필요한 항목만 읽는다.
- (b) manifest 에 두되 로더가 CJK 항목을 건너뛰고 freeze 에서 조건부로 읽는다.

어느 쪽이든 확인할 것:
- manifest 를 순회하는 도구(`tools/evaluation/audit_references.py`, 컴파일 리소스
  검사, `build --check`, lexicon inventory)가 CJK 파일을 여전히 알아야 한다.
- `resource_root` 가 NULL(설치 기본값)인 엔진도 같은 위치에서 CJK 파일을 찾는다.
- `ttf_engine_validate_resources` 는 freeze 전 호출 시 CJK system 행을 포함하지
  않는다. freeze 후 korean 모드에서는 포함한다. 이 차이를 문서화한다.
- CJK system 행의 `declaration_order` 는 domain/customer 뒤가 된다.
  §9.1 이 허용하는 범위다.

#### §5.5 구현 결정

(b)를 채택했다. manifest의 세 경로를 유지하고 create에서는 건너뛰며, freeze가
확정된 옵션으로 선택한다. 예약 경로는 `languages/ko-KR/cjk/` 아래
`hanja_readings`, `kana_readings`, `cjk_words`의 `.tsv`/`.bin`/`.ttfb` 형식이다.
컴파일 검사도 manifest를 따라 세 파일을 생성하고 원본 TSV를 제거한 뒤
네 옵션 조합의 출력·trace 동일성을 검증한다. 다른 경로는 기존 eager 로드를 유지한다.

freeze의 파일별 임시 파서와 전체 transaction checkpoint를 함께 사용한다.
파일 오류·최종 engine validator 오류·인덱스 할당 실패 시 이번 freeze에서 추가한
모든 행을 해제하고 `resource_order`를 복구한다. 원래 행/포인터/옵션과 mutable
상태는 유지한다. 모든 실패 가능 작업 이후에만 인덱스를 확정한다.
리소스 수정 후 재시도/중복 freeze, 먼저 들어온 동일 SYSTEM 행의 우선순위,
고객 override, freeze 후 로드 거부를 native/ASan 테스트로 검증한다.

기본 리소스만 있는 엔진의 validation record 수:

| hanja / kana | freeze 전 | freeze 후 |
|---|---:|---:|
| passthrough / passthrough (legacy 포함) | 3,459 | 3,459 |
| korean / passthrough | 3,459 | 11,580 |
| passthrough / korean | 3,459 | 3,765 |
| korean / korean | 3,459 | 11,881 |

명시적 override 행은 전후 양쪽에 더해진다. freeze 전 지연 system 값은 조회되지
않으며, freeze 후에는 전용 인덱스에 위임한다. `resource_root=NULL`도 create 때
해결한 동일 root를 사용한다. passthrough trace의 기존 7개 declaration offset은
이제 **0**이며, 비번호 필드와 동일 layer의 상대 순서도 그대로다.

### 5.3 실측된 규모 (작업 0/4 결과)

| 테이블 | 행 수 | 파일 크기 |
|---|---|---|
| `hanja_reading` | **8,116** (K0+K1 7,744 + 호환 한자 372) | 237 KB |
| `cjk_word` | **4** | 1 KB |
| `kana_reading` 외 | **301** (초안 306) | 9 KB |

한자 18,000자를 가정했던 초안보다 **절반 이하**다. 메모리는
8,116 × (`ttf_entry_t` + 4 strdup) ≈ **1 MB** 미만.

`cjk_word`가 4건뿐인 이유는 §13.1 계약 38건 중 35건이 글자 fallback + 두음
규칙만으로 통과했기 때문이다. 필요한 4건은 각각 **글자 fallback으로 원리적으로
불가능한 경우**다.

| 엔트리 | fallback 결과 | 사유 |
|---|---|---|
| `音樂 → 음악` | 음락 | `樂`의 preferred가 `락` |
| `男女老少 → 남녀노소` | 남녀로소 | 복합어 둘째 성분의 두음. 형태소 경계 필요 (§7.4) |
| `金氏 → 김씨` | 금씨 | 성씨 관습 독음 |
| `ソウル → 서울` | (가나 전사) | 원어 복원. 규칙으로 불가 (§7.5) |

**이 파일을 커버리지 목적으로 키우지 않는다** (§1).

---

## 6. 한자 데이터

### 6.1 Repertoire — **K0 + K1 (실측으로 확정)**

> **2026-10-04 갱신 (프로젝트 소유자 결정, 선택지 D):** Unihan 표는 그대로 K0 + K1이고, 그
> 밖의 글자는 표준국어대사전 한자어에서 뽑은 별도 표 `hanja_readings_stdict.tsv`(904자,
> CC BY-SA 2.0 KR)로 읽는다. §6.5.

> **결정: `kIRG_KSource` 의 K0 + K1, 7,744자.** 초안의 "K0~K6 전체"는 작업 0의
> 실측으로 폐기했다. 근거는 아래 커버리지다 (Unicode 18.0.0, 2026-07-31).

| 소스 | 표준 | 자수 | kHangul 보유 | 커버리지 |
|---|---|---|---|---|
| **K0** | KS X 1001:2004 | 4,888 | 4,888 | **100.00 %** |
| **K1** | KS X 1002:2001 | 2,856 | 2,856 | **100.00 %** |
| K2 | KS X 1027-1:2011 | 7,911 | 542 | 6.85 % |
| K3 | KS X 1027-2:2011 | 1,834 | 44 | 2.40 % |
| K4 | KS X 1027-3:2011 | 172 | 3 | 1.74 % |
| K5 | KS X 1027-4:2011 | 404 | 0 | **0.00 %** |
| K6 | KS X 1027-5:2021 | 599 | 11 | 1.84 % |
| **K0~K6 합집합** | | **18,664** | 8,344 | **44.71 %** |

K0+K1 은 **결측 0**이고, K2~K6 을 더하면 자수는 2.4배가 되지만 독음은 7% 미만만
따라온다. §1의 성공 기준상 커버리지 자체가 목표가 아니므로, **보강 자료를 찾지
않고 repertoire를 줄이는 쪽**으로 확정했다.

`KC`(Korean History Online database)와 `KU`(이동·제거된 참조)는 **KS 표준이 아니므로
제외**한다 (UAX #38). 각각 2,962자 / 4자이고 전부 K0~K6 밖이다.

### 6.1.1 지원하지 않는 자형

**간체자(简体字)와 일본 신자체는 한국 표준 자형이 아니므로 지원 대상이 아니다.**

| | K-source | kHangul |
|---|---|---|
| `國` (정체) | K0 | `국:0E` ✅ |
| `国` (간체) | K2 | `국:N` — repertoire 밖 |
| `學` (정체) | K0 | `학:0E` ✅ |
| `学` (신자체/간체) | K2 | `학:N` — repertoire 밖 |
| `亚` `汉` 등 | **없음** | 없음 |

따라서 `中国 → 중国` 처럼 부분 보존된다. 이는 §1 기준상 **정상 동작**이다.
(원래 요구사항 문서의 `中国 → 중국` 예시는 이 범위에서 성립하지 않는다.)

#### repertoire를 넓혀도 해결되지 않는다 (실측)

"K2~K6 중 독음이 있는 글자만 추가"하는 절충안을 검토했다. 비용은 낮지만
**목적을 달성하지 못한다.**

| | |
|---|---|
| K2~K6 중 K0+K1 밖이면서 독음 보유 | **600자** |
| 그 구성 | 대부분 희귀 Ext-A 한자 (`㘔 㘽 㛄 㛅 㜯 …`) |
| 상용 간체자 18자 표본 중 커버 | **4자** (`国 学 会 来`) |
| 나머지 14자 (`亚 汉 发 业 对 么 买 实 个 门 这 那 没 黑`) | **K 소스 자체가 없음** |

간체자는 한국 표준 자형 체계 밖이라 K 소스를 늘려도 들어오지 않는다.
해결하려면 **별도의 간체→정체 매핑 자료**를 새로 들여와야 하고, 그것은
§1의 "커버리지 확대를 위한 후속 범위 확장"에 해당해 범위 밖이다.

> **결정: 간체자·신자체 미지원. 원문 보존이 정상 동작이다.**
> 이 판단이 바뀌는 유일한 조건은 "실제 입력에 간체자가 유의미하게 나온다"는
> 사실이며, 그때는 별도 기능으로 다룬다.

### 6.2 독음 출처

- **primary**: Unihan `kHangul`. 한 글자에 복수 독음이 있으므로 처음부터
  `codepoint → reading[]` 구조로 저장한다.
- `kKorean`은 보조 참고만. primary로 쓰지 않는다.
- 태그 `X`(현재 인코딩에서 제거된 구 K-source, UAX #38)만 달린 독음은 **쓰지 않는다.**

kHangul 태그와 이 프로젝트의 confidence 매핑:

| 태그 | 의미 | confidence |
|---|---|---|
| `0` | KS X 1001 | high |
| `1` | KS X 1002 | high |
| `E` | 한문 교육용 기초 한자 | high |
| `N` | 인명용 한자 | medium (관습 변이 가능) |
| `X` | 제거된 구 K-source | **제외** |

preferred 독음 선택 순서는 `E > 0 > 1 > N`, 동률이면 kHangul 기재 순서,
그다음 음절 코드포인트 순이다. **런타임 임의 선택은 없다.**

### 6.2.1 ⚠️ kHangul은 두음법칙 적용형을 별도 독음으로 싣는다

```
亂  난:0  란:0E      亮  량:0N 양:0       例  례:0E 예:0
了  료:0E 요:0       來  내:0  래:0E      令  령:0E 영:0
```

`난`·`양`·`예`는 독립된 독음이 아니라 **`란`·`량`·`례`의 두음 적용형**이다.
이것을 그대로 저장하면 §7.3 "canonical만 저장한다"가 깨진다.

> **생성기는 `X = 두음(Y)` 관계인 쌍을 검출해 파생형을 버리고 canonical만 남긴다.**

K0+K1의 복수 독음 351자 중 **207자가 이 두음 쌍**이고, 접고 나면 **진짜 복수
독음은 144자(전체의 1.9%)** 뿐이다. 접은 뒤 실제 분포는 1개 7,600 / 2개 138 / 3개 6.

### 6.2.2 ⚠️ 호환 한자는 자체 독음이 canonical보다 구체적이다

CJK Compatibility Ideographs 블록은 **한국어 때문에** 만들어졌다 — 같은 한자를
독음별로 중복 인코딩한 것이다.

```
U+F914 樂 = 낙      U+F95C 樂 = 락      U+F9BF 樂 = 요
U+6A02 樂 = 락|악|요   (canonical, 모든 독음)
```

**따라서 `kCompatibilityVariant`로 대상 문자의 독음을 상속시키면 안 된다.**
세 글자가 전부 같은 독음이 되어 구분이 사라진다.

> **규칙: 호환 한자는 자체 `kHangul`이 있으면 그것을 쓰고, 없을 때만 대상에서 상속한다.**

호환 한자 1,002자 중 270자가 자체 독음을 갖고, 그중 **168자는 대상의 preferred와
다르다.** 268자는 이미 K0+K1에 포함되어 일반 경로로 처리된다.
런타임 정규화는 하지 않는다 — 생성 시점에 테이블 행으로 굽는다.

### 6.3 ⚠️ 실질적 병목

1. ~~`kHangul` 커버리지가 얼마인지 모른다~~ → **작업 0에서 실측 완료. §6.1.**
2. **`kHangul`은 글자 독음만 준다. 한자어 사전은 주지 않는다.** — 다만 실측 결과
   이 부담은 예상보다 훨씬 작다. §13.1 계약 38건 중 **35건이 글자 fallback +
   두음 규칙만으로 성립**하고, 단어 엔트리가 필요한 것은 3건뿐이다 (§5.3).
3. 현재 렉시콘 인벤토리가 379항목 중 294 pending / independently human-reviewed **0**
   이다. 대량 한자어 엔트리 투입은 **리뷰 부채를 폭증**시킨다. `review_status`를
   처음부터 엄격히 관리하고 "엔진이 동의함 ≠ 언어학적 승인"이라는 기존 원칙을 유지한다.
   §1 기준상 사전을 키우는 것 자체가 목표가 아니므로 **최소 세트를 유지한다.**
   → 2026-10-05 프로젝트 소유자 결정으로 표준국어대사전 단어표를 별도 파일로 들였다 (§6.6).

### 6.4 손으로 쓰는 독음 규칙 — `hanja_rules.tsv` (2026-10-04)

생성 표(`hanja_readings.tsv`)는 손대지 않고, 글자 독음 위에 겹치는 규칙만 별도 파일에 둔다.
`hanja=korean`일 때만 함께 로드된다(§5.5와 같은 지연 로드).

| kind | 예 | 뜻 |
|---|---|---|
| `hanja_soft_after ko-KR 률 율` | 比率 비율, 旋律 선율, 分裂 분열 | 앞 음절이 받침 없음 또는 ㄴ일 때 바꿈 (한글 맞춤법 제11항 붙임 1). 法律 법률은 그대로 |
| `hanja_before_initial ko-KR 不:ㄷ 부` | 不正 부정, 不動産 부동산, 不得已 부득이 | 다음 한자 독음의 첫소리가 ㄷ·ㅈ이면 바꿈. 不安 불안, 不法 불법은 그대로 |

실측(표준국어대사전 한자어 186,498개, 로컬 평가만): 글자 독음 일치 95.41% → **95.92%**,
+958 / −13. 잃는 13개는 사자성어 가운데의 不(독불장군, 화불단행 11개)과 不字(불자), 卑劣(사전 표제
비렬) — 같은 위치에서 맞게 되는 것(도부동, 막부득이 등 175개)이 훨씬 많아 규칙을 유지한다.
사전 내용은 저장소에 넣지 않았다(라이선스 미확인).

### 6.5 범위 밖 글자 — 표준국어대사전 독음표 (2026-10-04)

K0 + K1 밖이라 원문으로 남던 글자(強·絕·豐·姊·兔·郎 등)는 표준국어대사전 한자어의 독음으로
읽는다. 선택지를 비교한 뒤 프로젝트 소유자가 D를 골랐다.

| 선택지 | 추가 글자 | 사전 한자어 효과 |
|---|---|---|
| A 현행 유지 | 0 | — |
| B Unihan 신뢰 태그(0/1/E) 글자만 | 6 | +775 단어 |
| C Unihan 독음이 있는 범위 밖 글자 전부 | 약 780 (대부분 인명용 N 태그) | +1,157 단어, 사전과 95% 일치 |
| **D 사전 한자어에서 글자 독음 추출 (채택)** | **904** | **+2,551 단어, 원문으로 남는 글자 0** |

- 생성: `tools/cjk/stdict_readings.py STDICT.zip OUT.tsv`. 한자어 표제어 중 글자 수와 한글 음절
  수가 같은 187,028개를 글자-음절로 맞춘다. Unihan 표에 있는 글자는 건너뛴다(Unihan 우선).
  어두 음절은 두음 형태일 수 있으므로 어중 독음을 우선하고, 어중에서 두음 형태로만 보이는
  독음이 Unihan 독음과 두음 관계이면 Unihan 독음으로 되돌린다(郎: 목낭청의 낭 → 랑, 新郎 신랑).
- 검증: 단어 4/5로 표를 만들고 나머지 1/5에 적용하면 389자 중 381자(97.9%)가 사전과 일치한다.
  사전 한자어 전체 일치율은 95.92% → 97.29%(+2,551, −0). 원본 zip·뜻풀이·용례는 저장소에
  넣지 않는다.
- 로드: `hanja=korean`일 때만, 다른 CJK 표와 같은 지연 로드(§5.5)와 전용 인덱스(§5.4).
- 라이선스: 이 파일만 CC BY-SA 2.0 KR이다(`STDICT-LICENSE.txt`). 재배포할 때 고지를 함께
  두고, 이 파일을 고친 결과도 같은 라이선스로 둔다. 다른 리소스와 코드는 영향받지 않는다.

### 6.6 단어 독음 — 표준국어대사전 단어표 (2026-10-05)

§6.3의 "단어 표는 최소한으로"를 프로젝트 소유자 결정으로 바꿨다. 글자 독음과 규칙으로는 틀리는
사전 한자어 4,932개를 `cjk_words_stdict.tsv`(CC BY-SA 2.0 KR, `hanja=korean`일 때만 로드)에 넣는다:
列車 열차, 汽車 기차, 樂山樂水 요산요수, 敗北 패배, 便所 변소, 遊說 유세, 省略 생략, 率直 솔직.

- 생성: `tools/cjk/stdict_readings.py STDICT.zip CHARS.tsv --words WORDS.tsv`. 엔진의 글자 독음·두음·
  `hanja_rules.tsv`·단어 일치를 파이썬으로 그대로 흉내 내고(사전 한자어 186,498개 전부에서 엔진 출력과
  일치 확인), 그 결과가 사전 독음과 다른 2~6자 단어만 넣는다. 사전에 독음이 둘 이상인 단어(行列
  행렬/항렬, 一切 일체/일절, 更新 갱신/경신)는 넣지 않는다.
- 호환 한자(2026-10-27): 사전이 호환 한자로 쓴 단어(滑稽의 滑 U+F904 골, 頻數의 數 U+F969 삭)는
  글에서는 통합 한자로 쓰이므로 통합 한자 형태로도 넣는다(같은 형태가 따로 있는 단어면 그쪽을 둔다).
  滑을 활 먼저 읽게 바꾸면서(`generate.py` `PREFERRED_FIRST`) 滑 단어 80개가 글자 규칙으로 맞게 되어
  빠지고 호환 한자 단어 37개가 들어와 4,890개다.
- 단어는 더 긴 단어 안에서도 일치하므로, 다른 사전 단어 안에서 다르게 읽히는 일이 더 많은 항목은
  뺀다(樂器 악기 → 娛樂器 오락기).
- 검증: 단어 4/5로 만든 표를 나머지 1/5의 처음 보는 단어에 적용하면 +164 / −6(나머지 37,936개는
  그대로). 전체 사전 한자어 일치율 97.29% → 99.93%(표를 만든 데이터이므로 커버리지 지표).

---


### 6.7 옵션의 적용 범위와 간체자·신자체 (2026-10-12)

프로젝트 소유자 결정: `hanja=korean`·`kana=korean`은 **한국어 문장 안의** 중국어·일본어를 읽는 옵션이다.
온전한 중국어·일본어 문장은 옵션과 상관없이 그 언어 문장으로 남는다.

- **이전 동작:** 옵션이 켜지면 중국어·일본어 문장 판정(`han_sentence`, `kana_sentence`)이 통째로 꺼졌다.
  그래서 `这是一个非常重要的问题。` → `这시일个비상중요적问题`, `そうですね。` → `소데스네`처럼
  글자 단위로 바뀌고 언어 표시도 ko-KR가 됐다.
- **지금:** 옵션이 켜져 있어도 한글 없는 문장이 다음 중 하나면 zh-CN·ja-JP 문장이다.
  - 문장 끝에 `。！？.!?`가 있다(닫는 따옴표·괄호가 뒤에 와도 된다)
  - CJK 글자가 10자 이상이다
- 그 밖의 낱말·구(`列車`, `とうきょう`)와 한국어 문장 안의 한자·가나(`王明이` → `왕명이`,
  `佐藤도` → `좌등도`)에는 그대로 독음을 적용한다. 기존 옵션 계약 135건은 모두 그대로다.
  기존 계약 입력은 모두 CJK 8자 이하이고 문장부호로 끝나는 것이 없었다.
- **간체자·신자체:** `hanja_readings_variants.tsv`(627자, kind `hanja_variant_reading`)가 간체자(国 这 会 学 …)와
  일본 신자체(気 楽 単 …)에 정자의 독음을 준다. **한글이 있는 문장 안에서만** 쓴다: `中国チーム에도` → `중국지무에도`
  (Was: `중国지무에도`). 한글 없는 `中国 日本`은 고정 계약(`cjk_reading_v22`, sha256)대로 `중国 일본`으로 남는다.
  - 원천은 `data/cjk/hanja_variants_source.txt`의 손으로 쓴 대응표(간체자·신자체 → 정자)다.
    독음은 `hanja_readings.tsv`(Unihan)의 정자 행에서만 가져온다. `tools/cjk/variants.py`로 생성한다.
  - 이미 자기 독음이 있는 글자(后 里 云 台 机 등)는 그대로 둔다. 뜻이 갈리는 글자를 합치지 않기 위해서다.
  - 자주 쓰는 글자만 넣었고 전부는 아니다. Unihan 이체자 필드(kTraditionalVariant 등)를 들이면 넓힐 수 있다.
  - 로드: `hanja=korean`일 때만, 다른 CJK 표와 같은 지연 로드.
- 계약: `mixed_lang_v72`(41건).

### 6.8 혼합 문장 2차 검토 (2026-10-13)

프로젝트 소유자가 준 두 번째 혼합 문장(요청 ko-KR, 한자·가나 독음 켬)에서 나온 문제를 고쳤다.
조사는 손대지 않는다(`佐藤는` → `좌등는`: 글쓴이가 쓴 조사다). regex는 `레젝스`로 읽고 `정규식`으로 옮기지 않는다.

- **중국어 문장 속 주소:** `邮箱是 abc@qq.com。` 같은 문장이 요청 전체를 `invalid resource`로 실패시켰다
  (ko-KR·zh-CN·en-US 요청 모두. zh-CN에 주소 기호 읽기가 없었다).
  - 이제 `abc 艾特 qq 点 com`으로 읽는다.
  - 주소 안의 기호 읽기는 새 kind `address_symbol`이다(zh-CN 22개: 艾特 点 杠 下划线 冒号 斜杠 …). 없으면 `speech_symbol`을 쓴다.
  - 주소 속 숫자는 하나씩 읽고(`163` 一六三), 글자는 그대로 둔다.
  - 그래도 읽을 수 없는 주소는 원문을 남기고 trace에 `unrenderable-address-kept-literally`를 남긴다.
    요청을 실패시키지 않는다.
- **작은따옴표 인용:** `"… 'Please send 3 files to 中国团队 before 5:30 PM' or '明日の Zoom …'?"`가 통째로 일본어
  문장이 되어 `さん files`, `ごごごじさんじゅっぷん`이 나왔다.
  - 이제 ASCII `'`도 인용으로 짝을 짓는다. 단 조건이 있다.
    - 여는 `'`는 공백·문장부호 뒤, 닫는 `'`는 공백·문장부호·끝 앞에 와야 한다.
    - 안에 공백이 있어야 한다.
    - 그 문장에 한글이 없어야 한다.
  - 짝으로 보지 않는 경우: 아포스트로피(`Alex's`, `'90s`), 한 낱말 강조(`'AI'`), 한국어 문장 속 인용(`'RM 67' 모델`).
  - `'明日の Zoom meeting starts at 9:00 AM'`은 문장 끝이 없어 6.7 정책상 온전한 일본어 문장이 아니다.
    그래서 영어로 읽고(`nine a m`) `明日の`는 원문 그대로 둔다.
- **코드 포인트:** `U+20A9`는 하나의 객체다(`syntax.codepoint`, 16진 4~6자리).
  - 한국어: `유 플러스 이공에이구`(0은 공)
  - 영어: `you plus two zero ay nine`
  - 일본어: `ユー プラス にぜろエーきゅう`
  - 이전: `유+20A9`, `Uplus twenty A nine`, `ユー+20A9`
- **영어:**
  - `1M requests` → `one million requests`(K·M·B 뒤에 복수 명사가 올 때만, `count_scale`). `3M tape`는 그대로 `three em`이다.
  - `requests/day` → `requests per day`(시간 낱말 9개, `per_word`). `input/output`은 그대로 `slash`다.
  - 단독 `≥`·`≤` → `greater than or equal to`·`less than or equal to`. 이전에는 CLDR 이름 `greater-than or equal`이었다.
  - `24/7` → `twenty four seven`(`fraction_idiom`). 단 `ratio of 24/7`은 `over`로 남는다(`fraction_idiom_not`).
  - `v2.1` → `version two point one`(`electronic_prefix`). 한국어 문장의 `v2.1.3 버전`은 `브이`로 남는다.
- **한국어로 읽는 영어 낱말:** English 잉글리시(이전 엥리시), Tech 테크(텍), Set 세트(셋), regex 레젝스(리겍스). 복합어 setup·set-up은 `셋업` 그대로(ko_latin_joiners_v24). 대문자만 쓴 `SET`은 코드 안에서 철자로 남는다(`EL2SET021`, `lexicon_case lower`).
  `lexicons/ko-KR/english-common-v26.jsonl`, 근거 `project-english-common-v26`.
- **확인:**
  - 계약 `mixed_lang2_v73`(32건)
  - 한국어 quick: changed 0 (142,255문장)
  - 영어 speech 90.83%·text 86.80%, JSUT 203/332, ja-tn-eval 97.3%, 중국어 9개 세트: 모두 그대로
  - 계약 기대값 2건은 병합 전에 고쳤다. 사유는 각 계약 notes에 있다.
    - `'明日の …'`: 처음에 `ごぜんくじ`로 썼으나 6.7 정책을 놓친 것이었다.
    - `3M tape`: 기존 읽기가 `em`이다.

## 7. 가나 데이터와 문맥 규칙

### 7.1 왜 음소를 경유하지 않는가

기존 영어→한국어 경로는 `en-US pronounce() → ARPABET → ko render_phones → 한글`이다.
가나는 이 경로를 **타지 않는다**.

- 한국어의 일본어 전사는 **외래어 표기법이 가나 표기 자체에 직접 정의**되어 있다.
  음소를 거치면 규정된 대응을 재현하지 못한다 (`つ`, `ん`, `っ`, 장음이 ARPABET에
  깔끔히 대응하지 않는다).
- ja 팩을 만들면 "일본어 TN 추가"가 된다. 범위 밖이다.

따라서 **표기 → 표기 직접 테이블**로 간다. ja pack 불필요, `script_score` 변경 불필요.

### 7.2 필요한 문맥 규칙 (작성 완료)

"1:1 단순 치환"은 성립하지 않는다. 4개 규칙이 필요하다.

| 규칙 | 내용 | 예 |
|---|---|---|
| **어두 청음화** | 런 첫 음절에서 ㅋ→ㄱ, ㅌ→ㄷ, ㅊ→ㅈ | `たなか` → **다**나**카** |
| **촉음 っ** | 직전 음절에 ㅅ 받침 결합 | `さっぽろ` → **삿**포로 |
| **발음 ん** | 직전 음절에 ㄴ 받침 결합 | `しんぶん` → **신**분 |
| **장음** | `ー`, 그리고 **히라가나** `う`/`お` 가 직전 음절의 중성을 이을 때 출력 없음 | `とうきょう` → 도쿄, `ラーメン` → 라멘 |

> **초안 수정:** 촉음·장음을 `さっ → 삿`, `とう → 도` 같은 **2글자 데이터**로 흡수하려던
> 계획은 폐기했다. 조합 폭발로 400행이 넘고, 무엇보다 **장음 판정은 직전 음절의
> 중성에 의존해서 데이터로 표현할 수 없다.** 네 규칙 모두 런타임 처리로 간다.
> 받침 결합과 어두 변이는 모두 한글 자모 인덱스 연산이라 두음법칙과 코드를 공유한다.

**결합할 앞 음절이 없거나 이미 받침이 있으면** 결합하지 않고 **원문 문자를 보존한다**
(§10). 단독 `ん`·`っ`가 사라지면 안 된다.

#### ⚠️ 장음 흡수는 히라가나에만 적용한다

진짜 구분선은 스크립트가 아니라 **일본 고유어냐, 일본어로 음차된 외래어냐**다.
`トウキョウ` 의 `ウ` 는 장음이지만 `ソウル`(서울) 의 `ウ` 는 실음절이다.
규칙으로는 구분할 수 없다.

가타카나는 외래어와 상관이 높고 장음을 `ー` 로 적으므로, **히라가나에만
`う`/`お` 흡수를 적용한다.** `ー` 는 스크립트와 무관하게 항상 장음이다.

| | 결과 | |
|---|---|---|
| `ソウル` | 소우루 → `cjk_word` 로 **서울** | ✅ 흡수했다면 `소루` 가 되어 더 나빠진다 |
| `ボウル` | 보우루 | ✅ |
| `とうきょう` | 도쿄 | ✅ |
| `ラーメン` `ｻｯﾎﾟﾛ` | 라멘 / 삿포로 | ✅ `ー` 는 그대로 동작 |
| `トウキョウ` | 도우쿄우 | ⚠️ **감수하는 손해.** 가타카나로 적은 일본 고유어 |

마지막 줄이 이 선택의 비용이다. 한국어 문장에 섞이는 가타카나는 대부분
상표·제품명이라 그런 표기는 드물고, §1 이 부분 변환을 허용한다.
필요하면 `cjk_word` 엔트리로 개별 처리한다.

### 7.2.1 실측 (작업 6 데이터 완료)

| 항목 | 값 |
|---|---|
| `kana_reading` | **210행** (히라가나·가타카나 청음/탁음/반탁음/요음) |
| `kana_mark` | 5행 (`っ` `ッ` `ん` `ン` `ー`) |
| `kana_fold` | 81행 (반각 → 전각, 탁점 결합 포함) |
| `kana_long_after` | 5행 (히라가나 장음 흡수 모음쌍) |
| **합계** | **301행 / 9 KB** |

가나 계약 **27건 전부**가 이 표 + 4규칙으로 재현된다 (§17 `simulate_kana.py`).
계약 밖 13건(`つなみ` `てんぷら` `とっとり` `きんかくじ` `ｶﾞｯｺｳ` 등)도 검증했다.

### 7.3 어두 규칙과 두음법칙의 통합

둘은 구조가 같다 — **"런 첫 위치에서만 다른 표기"**. 하나의
`apply_initial_variant()`로 처리한다. **둘 다 초성 치환이라 코드가 같다.**

| | canonical(저장) | 어두 변이(런타임) | 초성 치환 |
|---|---|---|---|
| 한자 두음 | `女 녀`, `老 로`, `歷 력` | `여`, `노`, `역` | ㄹ→ㅇ/ㄴ, ㄴ→ㅇ |
| 가나 어두 | `か 카`, `た 타`, `ち 치` | `가`, `다`, `지` | **ㅋ→ㄱ, ㅌ→ㄷ, ㅊ→ㅈ** |

`ぱ`행은 어두·어중 모두 ㅍ이므로 **ㅍ은 치환하지 않는다** (`ポ` → 포).
`つ`(쓰)도 위치와 무관하다.

두음법칙 변환 규칙:
- `ㄴ` 계열: 녀뇨뉴니 → 여요유이
- `ㄹ` 계열: 랴려례료류리 → 야여예요유이 / 라래로뢰루르 → 나내노뇌누느

**데이터에는 canonical만 저장한다.** `女 → 여`를 테이블에 넣으면 `男女`가 깨진다.

### 7.4 어두 판정의 한계 — 한자와 가나가 다르다

**가나: 런 단위 판정이 거의 정확하다.** 외래어 표기법의 "어두"는 표기된 단어의
첫머리이고, 복합어도 한 단어로 취급한다.

```
ほっかいどう (北海道)   → 홋카이도     か가 어중 → 카  ✅
きたきゅうしゅう (北九州) → 기타큐슈     た·きゅ 어중   ✅
おおさかじょう (大阪城)  → 오사카조     か 어중 → 카   ✅
```

**한자: 형태소에 민감해서 런 단위로는 부족하다.**

| 입력 | 런 첫 글자 규칙 | 정답 | |
|---|---|---|---|
| `女子` | 여자 | 여자 | ✅ |
| `男女` | 남녀 | 남녀 | ✅ |
| `老人` | 노인 | 노인 | ✅ |
| `歷史` | 역사 | 역사 | ✅ |
| `國立` | 국립 | 국립 | ✅ |
| `男女老少` | 남녀**로**소 | 남녀**노**소 | ❌ |

`男女老少` 유형(복합어 둘째 성분의 첫 음절)은 형태소 경계를 알아야 풀린다.
**형태소 분석기를 도입하지 않는다.** `cjk_word` 엔트리로 해결하고 fallback의
한계로 문서화한다.

### 7.5 ⚠️ 카타카나 외래어

카타카나는 일본어 고유어가 아니라 외래어 표기에도 쓰인다. 전사 규칙만으로는 오답이 난다.

```
コンピューター   전사 → 콤퓨터      한국어로는 → 컴퓨터
ソウル          전사 → 소우루      실제로는   → 서울
シアトル        전사 → 시아토루    실제로는   → 시애틀
```

`ソウル → 서울`은 역방향 원어 복원이라 규칙으로 못 푼다.
**`cjk_word` 예외 엔트리로만 처리한다.** 이것이 유일한 해법이다.

---

## 8. Unicode 처리

### 8.1 검출 범위

`U+4E00–9FFF`만 보면 안 된다.

**Han**
```
U+3400–4DBF   Ext A          U+4E00–9FFF     URO
U+F900–FAFF   Compat         U+2F800–2FA1F   Compat Supplement
U+2E80–2EFF   Radicals Sup   U+2F00–2FDF     Kangxi Radicals
U+20000–2A6DF Ext B     U+2A700–2B73F Ext C   U+2B740–2B81F Ext D
U+2B820–2CEAF Ext E     U+2CEB0–2EBEF Ext F   U+30000–3134F Ext G
U+31350–323AF Ext H
```

**Kana**
```
U+3040–309F   히라가나        U+30A0–30FF   가타카나
U+31F0–31FF   가타카나 음성확장  U+FF66–FF9D   반각 가타카나
U+1B000–1B16F 가나 확장 (희소)
```

`ttf_utf8_decode`가 이미 4바이트 supplementary plane을 정확히 처리하고 전 파이프라인이
코드포인트 단위이므로, **기존 추상화를 쓰기만 하면** 요구가 충족된다.

### 8.2 ⚠️ 스크립트 재분류의 부작용

`ttf_script_t`에 값을 **맨 뒤에** 추가하는 것 자체는 기존 enum 값이 안 바뀌어
ABI 안전하다. 문제는 런타임 동작이다.

```
"中國カナ"   지금:    [OTHER "中國カナ"] 1토큰
            분리 후: [HAN "中國"] [KANA "カナ"] 2토큰
```

**PASSTHROUGH에서도 `span.script`와 span 경계가 달라진다.** 출력 텍스트는 같지만
메타데이터가 바뀐다.

| 안 | 내용 | 평가 |
|---|---|---|
| **A** | 분류는 하되 **토큰 병합 규칙은 유지** (`HAN`/`KANA`/`OTHER`를 병합 판정에서 동치 취급) | **채택.** 토큰 경계 불변, `span.script`만 정확해짐 |
| B | 내부 전용 `ttf_is_han()`/`ttf_is_kana()`만 두고 `ttf_script_t` 불변 | 완전 무영향. IR 품질 개선 없음 |

> **결정: A.** `ttf_script_t`에 `TTF_SCRIPT_HAN`, `TTF_SCRIPT_KANA`를 **맨 뒤에**
> 추가한다(기존 값 불변). `ttf_tokenize`의 병합 판정에서는 `HAN`/`KANA`/`OTHER`를
> 동치로 취급해 **토큰 경계를 바꾸지 않는다.**

남는 변경은 CJK 코드포인트의 `span.script` **값 하나뿐**이다
(`OTHER` → `HAN`/`KANA`). 이것은 허용하되 영향받는 참조의 기대값 갱신을
**별도 커밋**으로 분리한다.

#### 영향 조사와 판정 규칙

**`tests/` 안에서 CJK를 포함한 파일의 `span.script` 변경만 허용한다.
출력 텍스트·그 밖의 메타데이터 변경은 회귀다.** 단, 엔진 상대적 trace 선언 번호는
§9.1의 네 조건으로 별도 판정한다. 고정 파일 목록은 허용 목록이 아니다.
2026-09-23의 7개 파일 목록은 당시 스냅샷이므로 판정 기준으로 사용하지 않는다.

`python3 tools/cjk/audit_test_sources.py --format markdown`으로 현재 목록을 갱신한다.
스크립트는 tests/의 tracked/untracked 비무시 UTF-8 소스에서 §8.1 Han/Kana
코드포인트를 찾아 파일·문자 목록을 출력한다. 생성·무시 파일 및 바이너리는 제외하며
소스 코드의 escape 문자열을 실행하지 않는다.

<!-- cjk-source-audit:start -->
조사 일자: 2026-09-27 (UTC). CJK 포함 파일: 19개.

| 파일 | Han 문자 | Kana 문자 |
|---|---|---|
| `tests/evaluation/cjk_reading_v22.jsonl` | 中 事 京 人 代 來 例 動 勞 北 史 国 國 園 外 大 女 子 字 宮 少 山 心 快 恐 慶 改 旅 日 未 本 李 東 樂 歷 氏 民 水 漢 獨 男 立 老 良 行 重 金 韓 音 龍 樂 樂 樂 𠮷 | い う お か き こ ご さ し じ す た ち っ と ど な は ば ひ ふ ぶ べ ほ ぽ ま や ゅ ょ よ ろ ん ウ サ シ ス ソ ッ ボ ポ メ ラ ル ロ ン ー ｮ ｯ ｳ ｷ ｻ ﾄ ﾎ ﾛ |
| `tests/evaluation/decorative_v21.jsonl` | 字 漢 | — |
| `tests/evaluation/numeral_char_v21.jsonl` | 一 三 二 日 | — |
| `tests/evaluation/speech_final_gate_v21.jsonl` | 字 漢 | — |
| `tests/evaluation/user_patterns_v20.jsonl` | 漢 | — |
| `tests/integration/test_cjk_hanja_edges.py` | 勳 女 子 張 樂 音 𠮷 | か た |
| `tests/integration/test_cjk_kana_edges.py` | 原 文 | う か っ と ん ゔ コ ポ ロ ー ｮ ｯ ｳ ｶ ｷ ｻ ﾄ ﾎ ﾛ ﾝ |
| `tests/integration/test_cjk_passthrough.py` | 漢 | — |
| `tests/integration/test_compiled_resources.py` | 勳 張 | ウ ソ ル |
| `tests/integration/test_particle_resources.py` | 漢 | — |
| `tests/integration/test_pronunciation_aliases.py` | 別 名 | — |
| `tests/integration/test_user_patterns.py` | 漢 | — |
| `tests/streaming/test_cjk_splits.py` | 女 少 樂 男 老 音 | — |
| `tests/streaming/test_cjk_stream.c` | 京 勳 女 子 少 張 東 樂 男 老 音 𠮷 | う き っ と ど ょ ん ウ ソ ボ ル ｮ ｳ ｷ ﾄ |
| `tests/streaming/test_random.c` | 語 | — |
| `tests/unit/test_cjk_index.c` | 山 樂 音 | ウ ソ ル |
| `tests/unit/test_cjk_lazy.c` | 山 樂 音 | か |
| `tests/unit/test_cjk_options.c` | 山 𠮷 | カ ナ |
| `tests/web/test_workbench.py` | 漢 | — |

<!-- cjk-source-audit:end -->

#### ⚠️ 단 하나의 실제 위험: `test_frontend.c`

```c
tests/integration/test_frontend.c:150   return script == TTF_SCRIPT_OTHER ? 100 : 0;
tests/integration/test_frontend.c:155   if (input->tokens[i].script == TTF_SCRIPT_OTHER)
tests/integration/test_frontend.c:191   OK(ttf_normalize(engine, "語 7", ...));
tests/integration/test_frontend.c:193   CHECK(!strcmp(result.text, "語 synthetic-number"));
```

제3자 pack 등록 검사가 **`TTF_SCRIPT_OTHER`를 100점으로 채점하고 `語`를 입력으로
쓴다.** `語`가 `TTF_SCRIPT_HAN`이 되면 이 pack이 더 이상 선택되지 않아
**A안(병합 유지)으로도 이 테스트가 깨진다.** 토큰 경계는 그대로지만
`script_score` 디스패치가 달라지기 때문이다.

> **작업 2의 선행 판단:** 이 테스트는 Han을 "아무도 claim하지 않은 스크립트"의
> 편의적 표본으로 쓴 것이다. 선택지는 (a) 테스트 입력을 여전히 `OTHER`인 문자
> (키릴·태국 문자 등)로 바꾼다, (b) §8.2 B안으로 후퇴해 `ttf_script_t`를 건드리지
> 않는다. **(a)를 권한다** — 테스트 의도를 그대로 유지하면서 한 줄 변경으로 끝난다.

### 8.2.1 기존 `speech_character` 와의 관계

이 프로젝트에는 **이미 한 글자 단위 한자 치환 수단이 있다.**

```
speech_character   ko-KR   漢   한        # docs/speech-resources.md:114 의 예시 그 자체
```

`ttf_speech_output`의 최종 문자 패스 첫 단계다 (`src/core/speech_output.c:131`).
즉 사용자는 오늘도 한자 한 글자씩 수동 등록할 수 있다. 이 기능은 그것의
체계적인 버전이다.

**우선순위:** CJK 스캐너는 정규화 단계에서 돌고 `speech_character`는 출력
최종 패스에서 돈다. `hanja=korean`이 `漢`을 이미 `한`으로 바꿨다면
`speech_character 漢` 은 발화되지 않는다. 이는 정상이며, **`passthrough`에서는
기존 `speech_character` 동작이 완전히 보존된다.** 계약으로 고정한다.

`script_score`는 건드리지 않는다. 즉 `HAN`/`KANA`에 점수를 주는 팩이 없으므로
`source`는 계속 `NULL`이고, 기존 외래어(foreign) 경로는 **타지 않는다.**

### 8.3 정규화 금지

- **입력 전체 NFKC 적용 금지.** 기존 문자에 의도치 않은 변경을 준다.
  이 프로젝트는 최근 `℃`/`㎏` 호환 문자를 개별 alias 테이블로 처리했다
  (`resources/languages/en-US/rules/numeric.tsv`). **같은 방식을 따른다.**
- 반각 가타카나(`U+FF66–FF9D`)는 생성 시점에 전각으로 접는다. **탁점·반탁점
  (`U+FF9E`/`U+FF9F`)은 앞 글자와 결합하는 별개 문자이므로 두 글자 열쇠로 접는다.**
  접지 않으면 `ﾎﾟ`가 `호` + 원문 `ﾟ`로 **잘못 읽힌다** — 보존보다 오독이 나쁘다.
  요음 소자(`ｬｭｮ`)와 소모음(`ｧｨｩｪｫ`)도 함께 접는다.
- **Variation Selector** (`U+FE00–FE0F`, `U+E0100–E01EF`): base + VS를 한 표기
  단위로 인식. PASSTHROUGH는 원문 완전 보존, KOREAN 변환 성공 시 VS는 결과에
  포함하지 않는다.

---

## 9. 회귀 조건 (zero-impact)

**네 축을 분리해서 정의한다.** 기존 요구사항은 ①만 명시했다.

| 축 | 조건 | 게이트 | 성격 |
|---|---|---|---|
| ① 출력 텍스트 | 두 옵션이 `passthrough`면 기존 corpus 결과 **바이트 동일** | 기존 corpus 전수 실행 | **필수 (hard)** |
| ② span/IR 메타데이터 | 토큰 경계 불변. CJK의 `span.script` 값 변경만 허용 | §8.2 전수 확인 후 별도 커밋 | 제한적 허용 |
| ③ 처리 성능 | `passthrough`에서 처리량이 유의미하게 떨어지지 않을 것 | **신규 게이트** | **필수 (hard)** |
| ④ 기동 비용 | `passthrough` 엔진의 create→freeze 비용이 CJK 이전과 같을 것 (§9.2) | 기존 timeout 게이트 + 기동 측정 | **필수 (hard)** |

> **결정: ①③④는 통과 필수 게이트, ②는 아래 두 가지에 한해 허용.**
> 그 밖의 메타데이터 변경은 ①과 동일하게 회귀로 본다.
>
> - CJK 코드포인트의 `span.script` 값 (§8.2 A안)
> - trace `declaration_order` 절대값 (§9.1)

### 9.1 `declaration_order` 절대값은 회귀가 아니다

`docs/detailed-pronunciation-trace.md` 가 이 필드를 이렇게 정의한다.

> `declaration_order` — 해당 엔진에서 행이 선언된 순서. **파일 경로나 파일의
> 행 번호가 아님**

즉 **엔진 상대적 일련번호**다. 앞서 로드되는 리소스가 하나만 늘어도 뒤의 모든
값이 밀리는 것이 이 필드의 본질이고, 절대값에는 의미가 없다. 의미 있는 것은
같은 layer 안에서의 **상대 순서**뿐이며, tie-break 코드도
(`src/resource/resource.c:62`, `src/rule/rule.c:66`) 같은 layer끼리만 비교한다.

절대값을 참조에 고정하면 **앞으로 모든 리소스 행 추가가 회귀로 잡힌다.**
이 프로젝트가 상시로 하는 작업이므로 그 게이트는 성립하지 않는다.

> **허용 조건 (전부 충족해야 한다):**
> 1. 출력 텍스트가 동일하다
> 2. 선택된 행의 `kind`/`language`/`key`/`value`/`layer` 가 동일하다
> 3. 같은 layer 안의 상대 순서가 보존된다
> 4. 변화가 **균일한 상수 오프셋**으로 설명된다
>
> 넷 중 하나라도 어긋나면 회귀다. 특히 2번이 바뀌면 다른 리소스가 선택된
> 것이므로 절대 통과시키지 않는다.

**후속 개선(이번 범위 아님):** `resource_order` 를 layer별 카운터로 바꾸면
system 행 추가가 domain/customer 의 번호를 밀지 않는다. tie-break 가 같은
layer끼리만 비교하므로 의미상 동등하다. 다만 `src/resource/validation.c:149`
의 진단 정렬이 함께 바뀌므로, CJK 작업과 분리해서 별도로 다룬다.

③은 §2.4에서 8.6배 회귀를 실측했기 때문에 반드시 필요하다. 이 프로젝트에는
30k-number timeout gate 전례가 있다. 게이트 형태는 그 전례를 따르되,
**시간 제한을 늘리거나 실패를 지워서 통과시키지 않는다.**

**옵션이 꺼져 있으면 스캐너 훅 자체를 건너뛴다.** 스크립트 판정조차 하지 않는다.

### 9.2 ⚠️ 기동 비용 — ③ 만으로는 부족했다 (4단계 실측으로 추가)

> **초안 오류:** ③ 을 "처리량"으로만 정의해 엔진 기동 비용을 빠뜨렸다. 3단계
> 게이트는 초기화를 제외하고 재서 1.031× 로 통과했지만, 기동은 거의 3배가 되었다.

같은 바이너리에서 리소스만 CJK 유무로 바꿔 CLI 1회 기동(`normalize "가"`)을 쟀다.

| 빌드 | CJK 없음 | CJK 포함 | 배율 |
|---|---|---|---|
| GCC | 16.5 ms | 47.5 ms | **2.89×** |
| Clang ASan+UBSan | 59.3 ms | 161.7 ms | **2.73×** |

`tests/evaluation/test_evaluation.py` 의 `evaluation_reports` 는 `cases.jsonl` 109건을
batch + 1/7/64 byte stream 으로 돌려 CLI 를 436회 이상 기동한다. 제한은 60초다.

| sanitizer `evaluation_reports` | 시간 | 결과 |
|---|---|---|
| CJK 없음 | **28.4 초** | 109건 / 실패 0 / 불일치 0 |
| CJK 포함 | **77.1 초** | 109건 / 실패 0 / 불일치 0 — **제한 초과** |

**이 corpus 는 passthrough 로 돈다.** 쓰지도 않는 8,422행을 기동마다 파싱·검증·
색인하고 있다. 결과는 같고 시간만 늘었으므로 원인은 CJK 행 로드로 확정이다.

`TTFB` 컴파일 형식은 해법이 아니다. `docs/resources.md` 가 명시하듯
"여전히 시작 시 TSV 를 파싱한다".

> **결정: CJK 리소스는 해당 옵션이 켜진 엔진에서만 로드한다 (§5.5).**
> passthrough 엔진의 기동은 CJK 이전과 같아야 한다.

게이트:
- 기존 `evaluation_reports` 60초 제한을 **그대로** sanitizer 단독 실행에서 통과
- passthrough CLI 기동이 CJK 이전 기준의 1.25× 이내 (GCC, 중앙값)
- korean 모드 기동 비용은 정보로 기록한다. 게이트가 아니다.

---

## 10. 스트리밍

기존 정책이 이미 안전하다. 세그멘터는 **공백에서만** 구를 자르고, CJK 런에는
공백이 없으므로 `大韓 | 民國`처럼 런 중간이 잘리지 않는다.
**별도 전체 문장 버퍼링을 도입하지 않는다.**

확인·확장할 것은 하나: 버퍼 끝이 CJK 런 중간일 때(`final=0`) 그 런을 확정하지 않고
대기하는지. `ttf_phrase_protect_until`이 다어절 렉시콘에 하는 보호를
`cjk_word` 최대 길이만큼 적용한다. **기존 훅의 확장이지 새 메커니즘이 아니다.**

테스트는 `tests/streaming/`의 임의 분할 패턴 재사용 (이미 arbitrary split 게이트 존재).

---

## 11. 생성기와 provenance

### 11.1 런타임에 외부 자료를 호출하지 않는다

```
[빌드 타임 — 1회]
  Unihan 추출본 ──► tools/cjk/generate.py ──► hanja_readings.tsv
  외래어표기법 대조표 ──►                  ──► kana_readings.tsv
                                          ──► cjk_words.tsv
                                          ──► report.json
                                                   │
                                                   ▼ (git 커밋)
[런타임]
  engine_create ──► manifest 일반 TSV 로드
  engine_freeze ──► 옵션별 CJK TSV 로드·검증 ──► 전용 해시 (이후 불변)
  요청 ──────────────────────────────────► 메모리 조회만
```

**런타임에 Unihan 파일도 `kHangul`이라는 개념도 존재하지 않는다.** `山 → 산`이라는
완성된 행만 있다. 엔진 준비(create/freeze) 이후 정규화 요청에는 파일 I/O나 네트워크 경로가 없다.

**폴백 독음도 전부 사전화된다.** `hanja_reading`은 8,116행이 전부 박혀 있고,
"폴백"이란 조회 순서가 나중이라는 뜻일 뿐 계산이나 유추가 아니다.
런타임에 계산하는 것은 §7.3의 어두 변이 하나뿐이며, 이는 데이터가 아니라 표기 규칙이다.

### 11.2 파일 배치

```
tools/cjk/generate.py                       통합 build / build --check
  kana_table.py                            가나 대조표 소스 (히라가나 장음 관계 5개)
  word_table.py                            승인된 단어 5행 소스
data/cjk/unihan-k-subset.txt               커밋용 CI 입력 (§11.3)
resources/languages/ko-KR/cjk/
  hanja_readings.tsv                        생성물, 커밋
  kana_readings.tsv                         생성물, 커밋
  cjk_words.tsv                             생성물, 커밋
  UNICODE-LICENSE.txt                       배포에 동봉하는 Unicode 고지
artifacts/cjk/report.json                   통계 (ignored)
```

### 11.3 CI 재현성 — **결정: A안**

생성 TSV를 커밋하고 CI가 `build --check`로 소스 일치를 검증하는 것이 이 프로젝트의
관례다(`CMakeLists.txt:227`, `.github/workflows/ci.yml:62`). 그러려면 CI가 생성을
재현할 수 있어야 하는데, Unihan.zip 전체는 "대량 외부 데이터 vendoring 금지" 정책에
걸린다.

> **결정: Unihan에서 필요한 행만 추린 중간 파일을 커밋한다.**
> `kIRG_KSource` · `kHangul` · `kCompatibilityVariant` 세 필드의 행만 추출한다.
> Unicode License가 고지 조건 하에 재배포를 허용한다.

**실측: `unihan-k-subset.txt` 31,161행 / 870 KB** (원본 8.3 MB zip, 전개 44 MB).
추정치 1~2 MB 안에 들어온다.

**선택된 K0+K1 뿐 아니라 모든 K 소스 행을 담는다.** 나중에 repertoire를 바꿀 때
원본을 다시 받지 않아도 되게 하기 위함이다 (`generate.py --sources`).

이러면 `lexicons/sources/*.json`이 하는 역할(커밋된 소스 + 커밋된 생성물 +
CI 일치 검증)과 구조가 완전히 같아진다.

검토했으나 채택하지 않은 안: Unihan 없으면 `--check` 생략(재현성 포기),
원본 zip 통째 커밋(정책 위반).

### 11.4 provenance

`tools/lexicon/manage.py` + `lexicons/*.jsonl`이 이미
`id / source_lang / term / pronunciation / review_status / evidence / notes`
스키마로 provenance를 관리한다. **같은 모델을 따른다.**

단 `manage.py`의 `canonical()`에는 ASCII 전용 게이트가 있다.

```python
if not term.isascii():
    raise ValueError("this source profile accepts ASCII English keys only")
```

코드 주석이 "The native key API is wider; the authoring gate deliberately limits
entries"라고 명시하므로, **CJK용 별도 프로파일/제너레이터를 만드는 것이 의도된
확장 방향**이다. **기존 게이트를 완화하지 않는다.**

생성 소스에는 `codepoint / reading / source / confidence`를 보존한다.

```
山    산    UNIHAN_KHANGUL     high
某    모    DICTIONARY         high
ソウル 서울  LOANWORD_OVERRIDE  high
```

### 11.5 생성기 단계

```
1. Unihan 추출본 파싱 → 선택된 K 소스 repertoire (기본 K0+K1)
2. kHangul → reading[] 추출
3. kCompatibilityVariant 로 호환 한자 접기
4. 가나 대조표 파싱 → 촉음·장음 결합 키 전개
5. overrides.tsv 적용
6. 충돌 검출
7. 미해결 리포트
8. compact 런타임 테이블 출력
9. 통계: repertoire / resolved / 복수독음 / 단어엔트리 / 미해결 / source별 건수
```

---

## 12. 라이선스

**외부 자료 반입 전에 먼저 기록한다** (프로젝트 규칙).

| 자료 | 조건 | 처리 |
|---|---|---|
| Unicode Unihan | Unicode License. 고지 조건 하 재배포 허용 | `docs/THIRD_PARTY.md` 표에 항목 추가 + `benchmark/manifests/sources.json` 등록. 추출본만 커밋 |
| 외래어 표기법 (문화체육관광부 고시) 제2장 표 4 · 제4장 제2절 | **저작권법 제7조 제2호**가 "국가 또는 지방자치단체의 고시·공고·훈령 그 밖에 이와 유사한 것"을 보호받지 못하는 저작물로 규정. 이 문서의 다른 항목과 마찬가지로 **법률 판단이 아니라 확인한 근거의 기록**이다. | `kana_readings.tsv` 머리말과 THIRD_PARTY.md에 출처·근거 기록 |
| 국립국어원 표준국어대사전 (2026-09-06 내려받은 JSON) | **CC BY-SA 2.0 KR** (사전 저작권 정책; 다중 매체와 출전 있는 용례 제외). 상업적 이용 허용, 저작자 표시, 변경 시 같은 라이선스 | 글자 독음만 추출한 `hanja_readings_stdict.tsv`와 `STDICT-LICENSE.txt`만 커밋. 원본·뜻풀이·용례는 반입하지 않음. 그 파일만 CC BY-SA (§6.5) |
| 기타 한자 사전 | **미확인** | 라이선스 확인 전 어떤 것도 반입 금지. 웹 스크레이핑 금지 |

---

## 13. 테스트

프로젝트 관례(`tests/evaluation/*.jsonl` 계약 + `tools/quality/tn_cycle.py` 사이클 +
CTest)를 따른다. **구현 전에 기대 출력을 독립적으로 작성한다** (AGENTS.md 강제 조항).
**gold를 구현에 맞춰 고쳐쓰지 않는다.**

```
tests/evaluation/cjk_reading_v22.jsonl     신규 계약
tests/integration/test_cjk_reading.py      CLI 계약
tests/unit/                                스크립트 판정·코드포인트 경계
```

### 13.1 계약 항목

**한자**
```
기본 글자    山→산  水→수  人→인  大→대  國→국
한자어      漢字→한자  中國→중국  大韓民國→대한민국  音樂→음악
두음법칙    女子→여자  老人→노인  歷史→역사  男女→남녀
복합어      男女老少→남녀노소   (cjk_word 엔트리 필요)
한글 혼합    오늘 中國 시장 → 오늘 중국 시장
영문 혼합    Korea 大韓民國 API → Korea 대한민국 API
번역 아님    東京→동경(≠도쿄)  北京→북경(≠베이징)
미지원 한자   원문 보존 (𠮷 등)
복수 독음    동일 입력 → 동일 출력 (determinism)
```

**가나**
```
기본        すし→스시  ちば→지바
어두 청음    たなか→다나카  かとう→가토
촉음        さっぽろ→삿포로  ほっかいどう→홋카이도
발음 ん      しんぶん→신분
장음        とうきょう→도쿄  おおさか→오사카  ラーメン→라멘
요음        きたきゅうしゅう→기타큐슈
카타카나     サッポロ→삿포로
외래어 예외   ソウル→서울  (cjk_word 엔트리)
```

**공통**
```
supplementary plane   4바이트 문자에서 byte offset이 깨지지 않을 것
호환 한자             자체 kHangul 우선, 없을 때만 canonical 독음 상속 (§6.2.2)
Variation Selector    base+VS 한 단위 처리
스트리밍              임의 분할에서 동일 결과
PASSTHROUGH           기존 corpus 전수 동일
```

### 13.2 평가 기준 — 무엇을 재는가

§1의 성공 기준에 따라 **일본어/중국어 정확도를 재지 않는다.** 평가 대상은
**한국어 문장에 섞인 CJK 조각**이다.

| 재는 것 | 재지 않는 것 |
|---|---|
| 한국어 문맥에 섞인 CJK가 읽히는가 | 일본어 문장 전체의 정확도 |
| 변환 결과가 한국어 정규 표기인가 | repertoire 커버리지 비율 |
| 미해결이 안전하게 원문 보존되는가 | 미해결 건수를 0으로 만드는 것 |
| `passthrough`에서 기존 결과가 불변인가 | — |

**미해결 건수는 품질 지표가 아니라 관측값이다.** trace/report로 노출하되,
이 숫자를 줄이기 위해 범위를 늘리지 않는다.

평가 corpus는 기존 한국어 corpus에 CJK를 포함한 문장을 더하는 형태로 만든다.
일본어 corpus를 새로 확보하지 않는다.

### 13.3 이 프로젝트 특성상 추가로 필요한 것

- **옵션 조합 4가지 전부** (§3.2 표). 특히 혼재 토큰 `東京とうきょう`.
- **`--unreadable drop` × `korean` 조합.** 변환된 CJK는 이제 한글이라 drop 대상이
  아니고, **미해결 문자만** drop 대상이 된다. 명시적 계약이어야 한다.
- **성능 게이트** (§9 ③).

---

## 14. 작업 순서

AGENTS.md의 "한 번에 한 규칙군" 원칙에 맞춰 쪼갰다. 각 단계가 독립 커밋 가능하다.

| # | 작업 | 산출물 | 선행 | 상태 |
|---|---|---|---|---|
| 0 | Unihan 커버리지 실측 | K0~K6 자수, kHangul 결측, 복수독음 분포 | — | ✅ **완료** (§6.1) |
| 1 | 라이선스 기록 | THIRD_PARTY.md + sources.json. 대조표 조건 확인(§15-2) | — | 완료 (1단계) |
| 2 | 스크립트 판정 | HAN/KANA 추가, 병합 불변, OTHER 표본 교체, §8.2 동적 조사 | — | 완료 (1단계) |
| 3 | 옵션 배선 | 두 enum, 크기 검사 완화 + 3케이스 계약, CLI, 워크벤치 | — | 완료 (1단계) |
| 4 | 생성기 | 세 TSV 통합 build / --check + subset | 0, 1 | 완료 (4단계, LF 바이트 검증) |
| 5 | 통합 스캐너 골격 | `src/cjk/cjk_reading.c`, 미해결 보존 | 2, 3 | 완료 (2단계) |
| 6 | 가나 계층 | `kana_reading` + 어두 변이 | 4, 5 | 완료 (2단계); 3단계 가나 30/30 |
| 7 | 한자 글자 계층 | `hanja_reading` + 두음 (어두 변이 공유) | 4, 5 | 완료 (3단계); 86/86, 전용 힙으로 성능 회복 (§18) |
| 8 | 단어 사전 계층 | `cjk_word` + 전용 인덱스 | 6, 7 | 완료 (3단계); 5행, 전용 heap/18k/layer/ASan 통과 |
| 9 | 스트리밍 보호 | `ttf_phrase_protect_until` 확장 | 8 | 완료 (4단계); 2,488 parity + flush 전 콜백 |
| 10 | 회귀·성능 게이트 | §9 네 축 | 전체 | 완료: 지연 로드로 기동 회복, 86/86·회귀0·1.25×·sanitizer 잔여24/24 통과 (§18) |
| 11 | 문서 | 이 문서 갱신 + HANDOFF | 전체 | 완료: §5.5 로드 계약·record 수·재현 명령·완료 증거 기록 |

테스트 계약(§13)은 AGENTS.md 요구대로 **구현 전에** 80건을 저작해 두었다 (§17).

**가나(6)를 한자(7)보다 먼저 한다.** 데이터가 250행이라 스캐너 골격을 적은
비용으로 검증할 수 있고, 어두 변이 메커니즘을 먼저 확정하면 두음법칙이 그걸
재사용한다.

---

## 15. 미결 쟁점

| # | 쟁점 | 선택지 | 상태 |
|---|---|---|---|
| ~~1~~ | ~~간체자·신자체 처리~~ | — | ✅ **결정: 미지원.** K2~K6 확장으로도 해결 불가함을 실측 (§6.1.1). 뒤집는 조건은 "실제 입력에 간체자가 나온다"뿐 |
| ~~2~~ | ~~외래어 표기법 대조표 이용 조건~~ | — | ✅ **해소.** 문화체육관광부 고시 → 저작권법 제7조 제2호의 보호받지 못하는 저작물 (§12) |
| 5 | `test_frontend.c` 의 `TTF_SCRIPT_OTHER` 표본 | (a) 입력을 키릴·태국 문자로 교체 / (b) §8.2 B안 후퇴 | **(a).** 작업 2에서 한 줄 변경. 별도 판단 불요 (§8.2) |
| 3 | 계약 스키마의 `hanja`/`kana` 필드 | 러너 확장 | 순수 배선. 작업 3에서 함께 처리한다. 별도 판단 불요 |
| ~~4~~ | ~~`provenance.json` 커밋 여부~~ | — | ✅ **결정: `artifacts/`(ignored).** 커밋된 subset + 생성기로 바이트 동일하게 재생성되므로 1.86 MB를 저장소에 둘 이유가 없다 |

**구현 착수를 막는 미결 쟁점은 없다.** 3과 5는 해당 작업 단계에서 배선하면 되는
순수 엔지니어링이며 별도 승인을 요구하지 않는다.

### 실측으로 검증된 가정 (작업 0 완료)

| 가정 | 결과 |
|---|---|
| `kHangul`이 K0~K6을 실용적으로 덮는다 | ❌ **뒤집힘.** 44.71%. → repertoire를 K0+K1(100%)로 축소 확정 (§6.1) |
| 한자어 사전이 품질 상한을 결정한다 | ⚠️ **약화됨.** 계약 38건 중 35건이 글자 fallback으로 통과. 필요한 단어 엔트리는 4건 (§5.3) |
| 복수 독음이 상당수일 것이다 | ❌ **뒤집힘.** 두음 쌍을 접으면 진짜 복수 독음은 144자(1.9%) (§6.2.1) |
| 호환 한자는 대상 문자로 접으면 된다 | ❌ **뒤집힘.** 자체 독음이 더 구체적이다 (§6.2.2) |
| Unihan 부분 추출본이 1~2 MB | ✅ 870 KB (§11.3) |

### 아직 남은 가정

| 가정 | 확인 시점 | 뒤집혔을 때 |
|---|---|---|
| ~~기존 corpus에 CJK 참조가 거의 없다~~ | 작업 2 | ✅ **확인.** 7파일·한자 5자·가나 0건. 단 `test_frontend.c` 1건이 실제로 깨진다 (§8.2) |
| 두음 쌍 자동 검출이 진짜 복수 독음을 오검출하지 않는다 | 작업 4 검수 | 207건 중 표본 검수 필요. 오검출 시 override로 예외 처리 |
| `cjk_word` 인덱스가 성능 게이트를 통과한다 | 작업 8 | 엔트리가 4건이라 사실상 위험 없음 |

---

## 16. 결정 기록

| 날짜 | 결정 |
|---|---|
| 2026-09-23 | 한자·가나를 **하나의 통합 스캐너**로 구현한다 |
| 2026-09-23 | 옵션은 `--hanja` / `--kana` **분리**한다 (B안) |
| 2026-09-23 | `東京→동경` 과 `とうきょう→도쿄` 의 불일치는 **의도된 계약**이다 |
| 2026-09-23 | Unihan은 **필요한 행만 추린 중간 파일을 커밋**한다 (A안, §11.3) |
| 2026-09-23 | 가나는 ja pack 없이 **표기→표기 직접 테이블**로 간다 (§7.1) |
| 2026-09-23 | 한자 데이터를 `exact`/`pattern` 규칙 리스트에 싣지 않는다 (§2.4) |
| 2026-09-23 | 옵션 ABI: `ttf_frontend_options_t` 확장 + 크기 검사 완화 (§3.3 A안) |
| 2026-09-23 | `TTF_SCRIPT_HAN`/`TTF_SCRIPT_KANA` 추가, **토큰 병합 규칙은 불변** (§8.2 A안) |
| 2026-09-23 | zero-impact: 출력 텍스트·성능은 필수 게이트, 메타데이터는 `span.script` 값만 허용 (§9) |
| 2026-09-23 | **목적은 발성 범위 확장.** 부분 변환 허용, 커버리지는 목표가 아니다 (§1) |
| 2026-09-23 | 착수 순서: **가나 먼저**, 한자 나중 (§14) |
| 2026-09-23 | **repertoire = K0 + K1 (7,744자, kHangul 100%).** K2~K6 제외 (§6.1) |
| 2026-09-23 | KC·KU 제외 — KS 표준이 아니다 (§6.1) |
| 2026-09-23 | 간체자·신자체 미지원. 원문 보존이 정상 동작 (§6.1.1) |
| 2026-09-23 | 생성기가 kHangul의 두음 적용형을 접는다 (§6.2.1) |
| 2026-09-23 | 호환 한자는 자체 독음 우선, 대상 상속은 fallback (§6.2.2) |
| 2026-09-23 | preferred 순서 `E > 0 > 1 > N`, 태그 `X`는 제외 (§6.2) |
| 2026-09-23 | 두음 접기는 **파생형이 더 상위 태그면 접지 않는다** — 怒/異/諾/撓 오검출 방지 (§6.2.1) |
| 2026-09-23 | 가나 촉음·장음은 데이터가 아니라 **런타임 결합 규칙**으로 처리 (§7.2) |
| 2026-09-23 | 가나 어두 변이 = 초성 ㅋ→ㄱ, ㅌ→ㄷ, ㅊ→ㅈ. ㅍ은 불변 (§7.3) |
| 2026-09-23 | 반각 탁점은 두 글자 열쇠로 접는다 — 미접기 시 오독 (§8.3) |
| 2026-09-23 | 외래어 표기법 이용 조건 해소: 국가 고시 (§12) |
| 2026-09-23 | 기존 corpus CJK 영향 조사 완료 — 7파일, 한자 5자, 가나 0 (§8.2) |
| 2026-09-27 | **CJK 행을 공용 해시에서 분리해 freeze 시 전용 인덱스로.** passthrough 1.52× 실측 (§5.4) |
| 2026-09-28 | **CJK 리소스는 옵션이 켜진 엔진에서만 로드.** 기동 2.89×, sanitizer 평가 28.4→77.1초 실측 (§5.5, §9.2) |
| 2026-09-28 | 회귀 게이트에 ④ 기동 비용 추가 (§9) |
| 2026-10-05 | **사전 단어표 `cjk_words_stdict.tsv` 4,932개 (§6.3 최소 원칙 변경, 프로젝트 소유자 결정).** 사전 한자어 97.29 → 99.93% (§6.6) |
| 2026-10-04 | **범위 밖 글자는 표준국어대사전 독음표로 (선택지 D, 프로젝트 소유자 결정).** 904자, CC BY-SA 2.0 KR 별도 파일. 사전 한자어 95.92 → 97.29% (§6.5) |
| 2026-10-04 | **손으로 쓰는 독음 규칙 `hanja_rules.tsv`.** 렬/률 → 열/율, 不 → 부 (ㄷ·ㅈ 앞). 사전 한자어 95.41 → 95.92% (§6.4) |

---

## 17. 착수 전 준비 산출물 (2026-09-23)

저장소 밖 스크래치패드에서 작성했다. 저장소에 넣을 위치는 §11.2를 따른다.

| 산출물 | 내용 |
|---|---|
| `analyze.py` | Unihan K-source / kHangul 커버리지 측정 (작업 0) |
| `coverage-report.json` | §6.1 표의 원본 수치 |
| `generate.py` | 작업 4의 생성기 초안. `build` / `subset` 두 명령 |
| `hanja_readings.tsv` | 8,116행 / 237 KB. canonical 독음만 |
| `provenance.json` | 1.86 MB. codepoint·reading·source·confidence·k_sources |
| `cjk_words.tsv` | 4행. 각 행마다 필요 사유 기록 |
| `unihan-k-subset.txt` | 31,161행 / 870 KB. CI 재현용 (§11.3) |
| `kana_table.py` | 가나 대조표 생성기 |
| `kana_readings.tsv` | **306행 / 9 KB.** 어중형만, 어두형은 런타임 파생 |
| `cjk_reading_v22.jsonl` | **계약 85건.** 구현 전 저작 |
| `simulate.py` / `simulate_kana.py` | 시뮬레이터. 계약 교차검증용 |

계약 85건을 시뮬레이션과 교차검증해 **불일치 0**이다 (한자 경로 · 가나 26건).
이는 기대값의 내부 정합성 확인이며, 구현 결과를 보고 기대값을 맞춘 것이 아니다.

> 위 목록·85건 대조는 착수 전 스냅샷이다. 사용자 정정 후 현재 표는 301행,
> 계약은 86건(가나 30건)이며, 실행 검증 결과는 §18을 따른다. 시뮬레이터는 정답이 아니다.

### 17.1 검증이 잡아낸 실제 결함

문서상 타당해 보였지만 데이터를 돌려보고서야 드러난 것들이다. 구현 단계에서
같은 실수를 반복하지 않도록 남긴다.

| # | 결함 | 증상 | 조치 |
|---|---|---|---|
| 1 | 두음 접기를 모양만 보고 판정 | `怒`가 `로`, `異`가 `리`, `諾`이 `락`이 되어 **흔한 독음이 삭제됨** | 파생형이 더 상위 태그면 접지 않는다 (§6.2.1) |
| 2 | 호환 한자를 대상 문자에서 상속 | `U+F914/F95C/F9BF` 樂이 전부 `낙`이 되어 구분 소실 | 자체 독음 우선 (§6.2.2) |
| 3 | 결합할 앞 음절이 없는 `ん`·`っ` | **문자가 사라짐** — 보존 정책 위반 | 결합 실패 시 원문 보존 (§7.2) |
| 4 | 반각 탁점 미접기 | `ﾎﾟ`가 `호`+`ﾟ`로 **오독** | 두 글자 열쇠로 접기 (§8.3) |
| 5 | 반각 요음 소자 누락 | `ﾄｳｷｮｳ` → `도우키ｮ우` | fold 표에 `ｬｭｮ`·`ｧｨｩｪｫ` 추가 |

1·2는 설계 초안대로 구현했으면 **잘못된 테이블이 배포되었을 것**이다.
3은 §10의 보존 계약을 정면으로 위반한다.

**두음 접기 207건은 전수 검수하지 않았다.** 대신 태그 관계로 위험 신호를 매겨
**5건**만 사람이 확인했고, 그중 4건이 실제 오류였다. 나머지 202건은
"파생형이 원형보다 잘 입증될 수 없다"는 원칙이 성립한다는 가정 위에 있다.


## 18. 구현 검증 기록

### 1단계 완료 (2026-09-27)

GCC Debug 전체 CTest 153/153, ABI 크기 계약 3/3, 기존 corpus + particle locality
2,235건 회귀 0. CJK span.script 변화 9곳은 §8.2 범위 안이다.

### 2단계 완료 / §9.1 적용 (2026-09-27)

가나 규칙 27/27 + 실행한 pending 3건(소우루/우돈/東京도쿄).
관련 CTest 6/6, trace 비교 단위 검사 5/5, 기본값 2,235건 회귀 0.
이전 7건의 trace 선언 번호는 +301 균일 이동이며 비번호 필드 모두 동일,
같은 layer 내 상대 순서·동률 보존을 확인했다. 원본 캡처는 변경하지 않고
비교 도구가 네 조건을 검사하도록 했다. `d9d3342`로 별도 커밋했다.
엔진 resource_order 카운터는 그대로이며 layer별 카운터는 이번 범위에서 제외한다.

### 3단계 이전 측정: 기능 통과, 성능 gate 실패 (2026-09-27)

사용자가 두 참조 오류를 수정했다. 미해결 drop은 기존 v20의 인접 단어 분리 정책에
따라 `山𠮷水 → 산 수`다. 보존 계약은 영어 약어 읽기를 배제해 `가𠮷나 → 가𠮷나`다.
이전 2건은 구현 결함이 아니었으며 drop/약어 코드를 바꾸지 않았다.
수정된 86건의 SHA256은
`721c212c3ab21c156578725b917de8c6b0c8e9dc8968ce75af85805d6a7e558e`.

- 빌드 성공. 계약 **86/86**, 가나 **30/30**, pending 0.
- 추가 CTest **8/8**, 18,000행 전용 인덱스·rule 개수 불변·레이어·ABI·validator·
  compiled/source parity·한자/가나 edge·byte callback·trace 모두 통과.
- 기본값 **2,235건 회귀 0**. script 변경9곳 및 선언 번호 이동7건은 §8.2/§9.1 충족.
- 한자8,116행/단어5행 연결, e->rules와 별개 인덱스, preferred 고정,
  가나와 공유하는 두음 변이 함수. 일반 리소스 lookup의 비용 증가가 남았다.

§2.4/§9 성능 gate: 13,776bytes 순수 한글, passthrough, 7라운드×3회 중앙값.
측정 전에 고정한 **1.25배** 제한은 유지했다.

| 조건 | 중앙값 | baseline 대비 |
|---|---:|---:|
| 기존 | 71.75ms | 1.000 |
| CJK 후보 | 109.23ms | **1.522 (실패)** |
| 후보 + 단어18,000행 | 202.00ms | **2.815 (실패)** |

원인 분리: 같은 현재 바이너리에서 리소스만 변경해도 76.13→106.53ms(1.399배).
공통 resource hash 1,021 buckets에 평균 3.39→11.64→29.27행이 연결된다.
find_entry가 bucket 전체를 순회하므로 CJK 외 lookup도 충돌 비용을 부담한다.
리소스 추가만으로 저하가 재현됐으며 해당 비용을 개선할 필요가 있다.
원래 실패한 gate를 진단 수치로 대체하거나 임계값을 늘리지 않았다.

**당시 3단계 미완료, 4단계 미착수로 중단했다.** 단계 실패 시 중단 지시에 따라 성능 측정 이후
추가 구현·전체 suite/sanitizer를 실행하지 않았다. 3단계가 통과하면 기존
phrase 보호 훅의 확장, 생성물 --check, 전체 회귀·문서 완료 순서로 재개한다.
CJK 입력 규격·읽기 범위는 확장하지 않는다. 정확한 명령과 추가 정적 확인 사항은
[HANDOFF](HANDOFF.md), 모든 측정은 `artifacts/tn/cjk-v22/stage3-revised/`에 보존했다.
새 커밋·push·배포·서비스 재시작은 없으며 커밋 범위는 전체 완료 후 논의한다.


### 3단계 완료: §5.4 전용 힙 인덱스 (2026-09-27)

설계 정정대로 여섯 CJK kind를 로드·검증한 뒤 freeze에서 공용 hash에서 분리했다.
엔진은 `ttf_cjk_index_t *cjk` 하나를 소유한다. 전용 owning bucket에는 기존 엔트리를
옮기고, word bucket은 그 엔트리와 공용 user lexicon 행을 빌린다. `e->rules`와
독립이다. `TTF_HASH_BUCKETS`는 1,021 그대로이며 공용 해시 전체의 힙 전환은 하지 않았다.

- 기본 11,881행 = 공용 **3,459** + 분리 **8,422**. 엔진 크기 **17,320 bytes**.
- freeze 전후 validation `records`/`validated_records`/`unvalidated_records` 동일.
  모든 행의 실제 조회 결과 포인터·value·layer·order도 동일. 반복 freeze는 같은 인덱스.
- 18k 추가·wildcard·같은 layer의 선언 tie·user override 테스트에서도 같은 계약 확인.
- 계약 **86/86**, 가나 **30/30**, pending0. 관련 GCC **9/9**.
- ASan/UBSan index/ABI/validator **3/3**, 누수·이중 해제·UB 검출 0.
- 같은 성능 gate 재측정: 기준72.73ms, 후보74.40ms(**1.023×**),
  18k 추가74.79ms(**1.028×**). 두 옵션 korean 정보용73.93ms (CJK 없는 같은 입력).
- `artifacts/tn/cjk-v22/stage3-detached/`와 `/tmp/ttf-cjk-detach-*`에 증거를 보관했다.
  3단계 완료 보고 후 4단계로 진행했다.

### 4단계 이전 검증: 기동 게이트 추가 전 (2026-09-27)

기존 phrase 보호 훅에서 CJK word 최대 길이(최대6cp)만큼 보호한다. 꺼진 스크립트,
공백, 비CJK 문자에서 멈추고 variation selector는 앞 글자와 묶는다. 전체 문장 버퍼를
추가하지 않았으며 긴 불가분 런은 기존 `max_pending_bytes` 한도를 따른다.

- CJK CTest **8/8**: ABI, 전용 index, native streaming, 생성물, 86계약, 한자/가나 edge,
  arbitrary byte split. ABI 크기 3/3 및 v0.20 초기화 canary 유지.
- 86건 × 두 segment profile: 모든 두 조각 byte boundary, 1byte씩, 고정 seed random
  **2,488회** 텍스트·span·상세 trace 완전 일치. UTF-8/반각 탁점/VS/6자 사용자 단어 포함.
  문장/구 callback이 write 도중 flush 전에 발생함을 별도 검증했다.
- 기본값 기존 **2,235건 회귀0**. 허용 script9곳, 선언 번호7건 모두 **+8,422**;
  비번호 필드·같은 layer의 상대 순서 동일. 기대값이나 실패 기록을 지우지 않았다.
- `tn_cycle.py`: 고정 1단계 8/86 → 86/86, **fixed78 / regressed0 / remaining0**.
  각 계약의 1/7/64 byte stream 결과도 일치. 실제 외부 언어 품질의 통계로 해석하지 않는다.
- 생성기: 세 TSV의 데이터 행은 그대로 8,116/301/5. 단어 목록의 소스를 Python에
  명시하고 `build --check`를 CTest/CI에 추가했다. LF 바이트 동일성, CRLF 거부,
  check가 대상 파일을 변경하지 않음을 확인했다. 자동 다운로드 없음.
- 마지막 코드로 성능 gate를 다시 실행했다 (아래 표). 기존 1.25배 기준 유지.

| 조건 (같은 13,776-byte 순수 한글 입력) | 중앙값 | baseline 대비 |
|---|---:|---:|
| 고정 pre-CJK 기준 | 73.34ms | 1.000 |
| 최종 passthrough | 75.59ms | **1.031, 통과** |
| 최종 + 단어18,000행 | 74.47ms | **1.015, 통과** |
| hanja=korean / kana=korean (정보용, gate 아님) | 74.80ms | — |

설정/로드/freeze/Python span 변환은 측정에서 제외한다. 워밍업2회 후 7라운드×3회,
실행 순서 교대·중앙값은 기존 gate와 같다. 작은 순서 역전은 단일 VM의 측정 변동이며,
18k 추가가 더 빠르다는 결론은 내리지 않는다. 원래 실패 수치도 §18 위에 보존했다.
전체 GCC **161/161 통과**, 1,919.35초. 종료 후 Clang Debug ASan+UBSan을 순차 실행했다.
그러나 **119개 통과 후 `evaluation_reports`가 실패**하여 4단계를 중단했다.
144개 중 24개는 미실행이며 전체 sanitizer를 통과한 상태가 아니다.

실패는 `tests/evaluation/test_evaluation.py:29-32`의 60초 subprocess 제한이다.
첫 `cases.jsonl` 109건 평가(최소436 CLI: batch + 1/7/64byte stream)가 60초를 초과했다.
같은 GCC test는 24.67초에 통과했다. 실패 시점까지 메모리 오류/누수/UB 진단은 없지만
남은 검사를 실행하지 않았으므로 전체 안전성 검증 완료를 주장하지 않는다.
이는 이미 통과한 **30k-number 60초 게이트와 별개**다.

로드/검증/freeze 반복 비용이 유력한 원인 후보이나 추가 분리 측정은 하지 않았다.
위의 성능 gate는 초기화를 제외한 normalization 처리량만 증명한다.
제한을 늘리거나 기대값/실패를 지우지 않았고, 실패 후 새 테스트와 수정을 진행하지 않았다.
당시 커밋·push·배포·서비스 재시작 없이 멈췄다. 이후 사용자의 원인 확정과
§5.5 정정으로 해결한 결과는 아래에 있다. 당시 증거는
`artifacts/tn/cjk-v22/stage4/{status.json,gcc-all.log,asan-all.log,asan-LastTest.log}`에 있다.


### 4단계 완료: §5.5 조건부 로드 및 §9.2 기동 게이트 (2026-09-27)

사용자가 확인한 원인에 따라 CJK system 리소스를 freeze에서 옵션별로 로드했다.
진단 재수행·TTFB 형식 변경·평가 러너 변경·시간 제한 상향·기대값 수정은 없다.
§5.5의 (b)를 선택했으며 세 경로를 manifest에 유지했다. 파일/validator 오류 때는
이번 freeze의 행·선언 번호를 rollback하고, 마지막 index 할당 성공 후 확정한다.

| 게이트 | 결과 |
|---|---|
| 고정 계약 | **86/86**, 가나 **30/30**, pending0 (GCC 및 ASan CLI) |
| 기본 corpus | **2,235건 출력 회귀0**, script 허용9곳 외 변화0, declaration offset0 |
| GCC 기동 (25회 중앙값) | 기준16.5ms → **17.972ms / 1.089×**, 한도1.25 통과 |
| warm 처리 시간 | 기준71.910ms → **74.632ms / 1.038×**, 한도1.25 통과 |
| 단어18,000행 warm | **74.910ms / 1.042×**, 공용 경로 분리 유지 |
| sanitizer evaluation_reports 단독 | **30.37초**, 원래60초 한도 유지 |

기동 검사 변경(2026-10-27, 소유자 결정): `check_startup.py`는 고정 기준(16.5ms) 대신 이전 빌드와 바꾼 빌드를
번갈아 띄워 passthrough 중앙값 비율(한도 1.25)로 판정한다. 16.5ms는 7개 언어 팩 이전 값이라 그 뒤 모든 빌드가
실패했고, 공유 VM에서는 같은 빌드가 한 시간 사이 55ms와 100ms로 재졌다. `check_throughput.py`와 같은 방식이다.
| 이전 미실행 sanitizer | **24/24**, 276.04초, 실패/메모리/누수/UB 진단0 |
| 이번 영향 범위 GCC | **CJK9/9 + 기타23/23**; 이전 전체161/161을 불필요하게 재실행하지 않음 |
| 이번 영향 범위 sanitizer | 새 lazy/소유권/ABI/validator/trace/동시읽기 **7/7** |

기동 정보값(게이트 아님): hanja43.889ms, kana19.065ms, 둘 다44.704ms.
둘 다 korean/비CJK 입력의 warm 중앙값은74.998ms다. 성능 검사는 전체 suite나
sanitizer와 병행하지 않았다. 신규 테스트 포함 전체 등록 수는 GCC162/ASan145이나,
이번 턴에 전체를 처음부터 돌린 145/145 결과라고 표현하지 않는다.

추가 검증: disabled 파일 누락/invalid 내용 무시, 두 옵션4조합, setter 없는 legacy,
NULL resource_root, 마지막 파일 실패 후 rollback, final validator 실패 후 rollback,
재시도/중복freeze, 앞선 SYSTEM 행/고객 우선순위, freeze 후 load 거부. 기존 public
조회 포인터와 declaration/layer는 유지했다. `ttf_engine_validate_resources` 전후
행 수 차이는 §5.5 및 API/리소스 문서에 명시했다. ASan은 이 native 경로를 검증한다.

manifest 유지로 lexicon inventory, reference 검사, build --check가 그대로 통과한다.
컴파일 검사에서는 세 파일을 runtime validator로 검증해 .bin/.ttfb로 만든 뒤
원본 TSV를 삭제하고 네 옵션의 출력/trace 동일성을 확인했다.
스트리밍 2,488회 parity 및 flush 이전 콜백 검증도 재통과했다.

최종 증거: `artifacts/tn/cjk-v22/stage4-lazy/` (status, 계약/회귀/기동/처리량,
GCC/ASan 로그, 재개24개 이름 및 selection JSON), `stage4-lazy-candidate/` (고정 snapshot).
이전 실패 자료는 `stage4/`에 그대로 보존했다. 재현 명령은 [HANDOFF](HANDOFF.md)에 있다.
**작업1~11 및 단계1~4 완료.** 커밋·push·배포는 미실행이며 커밋 범위 논의가 다음 순서다.

## 19. 언어 고정 요청의 중국어·일본어 읽기 (2026-10-26)

소유자 요청(2026-10-26): 한자 읽기와 가나 읽기를 켜고 xvoice5와 중국어·일본어 발화를 비교한다.
한국어 전용 TTS에 보내는 요청은 언어를 고정하므로(`--matrix declared`) 그 조건을 기준으로 한다.

| 조건 | 문장 판정 | 읽기 |
|---|---|---|
| 한글이 있는 문장 | 한국어 | 정자 음, 변이형(간체·신자체·이체) 음 |
| declared, 가나 있음, 한글 없음 | 일본어 | 변이형 음, `hanja_japanese_reading` 먼저(予 예, 体 체, 芸 예, 欠 결) |
| declared, 한글·가나 없음 | 중국어 | 변이형 음, `hanja_chinese_reading` 먼저(几 기, 机 기, 价 가, 广 광, 台 대, 适 적) |
| auto, 한글 없음 | zh/ja 팩으로 라우팅 | 바꾸지 않음(2026-10-12 결정 유지) |

- **변이형 표:** `tools/cjk/variants.py`가 `data/cjk/hanja_variants_source.txt`에서 만든다(627 → 1,582행).
  - 한 쌍: 변이형 → 정자 (기존 방식).
  - `글자<TAB>=읽기`: Unihan에 한국 음이 없는 빈출 글자의 프로젝트 읽기(你 니, 吗 마, 吧 파, 哪 나, 她 타).
  - 자동: K0/K1 밖이라 hanja_readings.tsv에 없는 글자의 Unihan kHangul(青 청, 教 교, 插 삽).
  - 쌍 후보는 zh·ja 평가 입력에서 읽기가 없던 글자의 빈도로 골랐다(정답은 보지 않음).
- **가나 이어짐:** 일본어 문장에서 읽은 한자 바로 뒤 히라가나는 같은 낱말이라 っ가 한자 음절의 받침이 되고 낱말 첫머리 규칙(ㄱ/ㄷ)을 쓰지 않는다(待って 댓테, 行きます 행키마스). 히라가나 뒤 가타카나는 새 낱말(のチケット 노지켓토). 한국어 문장과 auto에서는 예전처럼 문자 종류가 바뀌면 새 낱말이다(東京とうきょう 동경도쿄, 계약 combo-both-on).
- **비교(xvoice5, 같은 입력):**

| 세트 | 범주 | TTF | xvoice5 |
|---|---|---|---|
| 개발 120문장 | 한자 섞인 한국어 / 가나 섞인 한국어 / 중국어 / 일본어 | 30 / 30 / 30 / 30 | 21 / 11 / 5 / 0 |
| held-out 110문장 | 중국어 40 / 일본어 40 / 한자 15 / 가나 15 | 38 / 40 / 15 / 15 | 8 / 0 / 10 / 4 |

  개발 세트는 고치는 데 쓴 세트라 TTF 120/120은 맞춤 결과다. held-out은 코드를 고정한 뒤 처음 돌렸다.
  held-out에서 TTF가 틀린 두 문장: 咖啡의 啡(읽기 없음), 滑雪의 滑(표의 첫 읽기 골, 흔한 뜻은 활).
  xvoice5는 cp949 밖 글자(간체·신자체)를 소리 없이 지우고, 두음법칙을 쓰지 않으며(로동, 래년), 가나를 된소리·장음으로 적는다(아리가또우).
- **계약:** `cjk_declared_v120`(16). 읽기 옵션이 꺼진 출력과 한국어 릴리스 벤치마크는 바뀌지 않았다.
