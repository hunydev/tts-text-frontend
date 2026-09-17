# External text normalization benchmarks

반복 가능한 **출력 텍스트** 품질 개선 도구다. 음성 합성·청취·화자 유사도 평가는
포함하지 않는다. 외부 reference와의 일치율은 인간이 검수한 정답률과 다르다.
현재 측정 결과와 범위는 [실험 기록](../docs/research/external-benchmark-results.md),
이용 조건은 [THIRD_PARTY](../docs/THIRD_PARTY.md)를 먼저 확인한다.

Python 3.10+ 표준 라이브러리와 빌드한 C frontend를 사용한다. import/evaluate는
독립된 도구이며, importer를 실행한다고 제품 사전이나 정답 corpus가 바뀌지 않는다.

```text
benchmark/
  common.py                 # UTF-8 span schema, validation, hashes
  importers/
    sproat/                 # original TSV, bounded optional fetch
    ksponspeech/            # authorized local dual transcriptions
    coreaspeech/            # authorized local text export; optional NC benchmark
    cmudict/                # pinned pronunciation archive and explicit native export
  evaluators/
    evaluate.py             # contextual sentence/span/class agreement
    compare.py              # fixed-reference before/after differences
  manifests/sources.json    # source revisions and rights boundaries
  generated/                # ignored local data/results; never commit
```

## 자료별 계약

| 입력 | 가져오기 결과 | 해석 |
| --- | --- | --- |
| Sproat original TSV | 문장 및 semiotic span `dataset-reference` | Kestrel 생성 reference; 사람이 검수한 gold 아님 |
| Kspon dual transcript | 선별 span `candidate`, 문장 `expected: null` | 방언·발음 변화 전체를 TN 정답으로 승격하지 않음 |
| Corea Numeric text1/text2 | 별개 입력 `unlabeled` | 두 입력은 written/spoken 대응 쌍이 아님 |
| 별도 Corea reference | `provided-reference` | 외부에서 주어진 reference라는 뜻; 검수 완료 인증 아님 |
| CMUdict | ARPABET와 복수 발음 archive | 영어 발음 자료; 한국어 TN gold가 아님 |

Kspon/Corea는 사용자가 적법하게 확보한 로컬 텍스트만 처리한다. 권한 확인 플래그는
이용 권한을 부여하지 않는다. 특히 Corea의 비상업 조건 및 공식 카드의 더 엄격한
제한이 해결되기 전에는 상업 제품 개발에 사용할 권리가 확보되었다고 보지 않는다.
직접 리소스화하거나 제품 CI에 다운로드하지 않는다. 데이터와 실패 원문을 포함한
리포트에는 원본의 이용 조건이 계속 적용된다.

## 1. 가져오기

각 형식·옵션·license evidence는 해당 가이드를 따른다.

- [Sproat/Jaitly](importers/sproat/README.md): 원본 3열 TSV와 `<eos>`를 읽는다.
  별도 Kaggle competition CSV와 호환된다고 가정하지 않는다.
- [KsponSpeech](importers/ksponspeech/README.md): CP949/EUC-KR/UTF-8를 명시하고
  `(written)/(spoken)`에서 선택한 TN 후보만 남긴다.
- [CoreaSpeech](importers/coreaspeech/README.md): Numeric의 두 발화를 분리하고,
  학습 데이터의 자동 생성 `N2gkPlus`를 정답으로 사용하지 않는다.
- [CMUdict](importers/cmudict/README.md): revision과 원본 해시를 검증한다.
  강세와 발음 대안을 보존하고, 명시적 export 시에만 native TSV/TTFB를 만든다.

예를 들어 권리가 확인된 로컬 Sproat 파일을 변환한다.

```sh
python3 benchmark/importers/sproat/import_sproat.py \
  /path/en_with_types/output-00099-of-00100 \
  benchmark/generated/sproat-test.jsonl \
  --source-file en_with_types/output-00099-of-00100 --limit 1000
```

`--limit`은 문장 수다. shard·split·선택 범위를 반드시 기록한다. 이번에 사용한
bounded fetch는 **training shard 58의 prefix**이므로 논문의 test 성능이 아니다.
대규모 데이터는 도구가 자동으로 전체 다운로드하지 않는다.

## 2. 같은 입력으로 기준 결과 고정

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# 리소스를 변경하기 전에 library와 resource snapshot을 함께 보관한다.
mkdir -p benchmark/generated/baseline
cp -L build/libtts_text_frontend.so benchmark/generated/baseline/
cp -a resources benchmark/generated/baseline/resources

python3 benchmark/evaluators/evaluate.py \
  --corpus benchmark/generated/sproat-test.jsonl \
  --library benchmark/generated/baseline/libtts_text_frontend.so \
  --resources benchmark/generated/baseline/resources \
  --limit 0 --chunk-bytes 1 7 64 \
  --output-dir benchmark/generated/before
```

`--library`는 C batch API를 직접 호출하고 language별 frozen engine을 재사용한다.
`--ttf build/ttf`도 지원하지만 CLI의 one-write stream 결과가 기준이다. backend가
다른 실행을 before/after 비교할 수 없다. sanitizer 테스트는 CLI backend를 사용한다.
`--domain FILE`은 명시적으로 domain layer를 추가한다. 평가 기본값은 1,000문장의
결정적 prefix이며 `--limit 0`만 전체를 읽는다. 출력 디렉터리는 새 경로여야 한다.

### 지표와 분모

- **sentence exact**: reference가 있는 모든 문장의 원문 그대로 UTF-8 문자열 일치.
  case/공백/구두점을 보정하지 않는다. reference가 없는 문장은 분모에서 제외한다.
- **span exact**: 문장 전체를 정규화한 native metadata를 source byte span에 정렬한다.
  reference 경계를 넘는 native span을 임의로 잘라 맞추지 않는다. 정렬 실패도 오답이다.
- **semiotic class**: 원본 class별 total/exact/unaligned와 micro/macro 비율을 낸다.
  PLAIN/PUNCT 포함 전체와 제외한 `tn_only_reference_spans`를 함께 기록한다.
- **spoken projection**: 별도 보조 지표다. 명시적으로 empty reference인 PUNCT의
  변경되지 않은 출력만 제외하고, 완전히 정렬된 span을 한 칸으로 연결한다.
  정렬 불가 문장도 labeled 분모에 남겨 실패로 센다. 숫자·대소문자·단어를 고치지 않는다.
  원문의 whitespace gap이 비공백 출력으로 바뀌면 projection을 만들지 않는다.
- **candidate agreement**: dataset/provided-reference accuracy와 섞지 않는다.
  `unlabeled`에는 정답을 생성하지 않는다.
- **chunk parity**: 지정한 각 UTF-8 byte chunk에서 텍스트와 모든 native span metadata가
  batch와 동일한지 검사한다. 데이터 정답 일치와 별개의 지표다.

Sproat는 토큰 사이 원래 공백을 제공하지 않아 written 토큰을 한 칸으로 재구성한다.
구두점 `sil`과 우리 엔진의 구두점 보존 정책 때문에 native sentence 점수가 낮아질 수
있다. 이 차이를 숨기려고 기본 정답을 변경하지 않는다. 보조 projection도 인간의
언어적 정확성을 판단하는 함수가 아니다.

출력 파일:

| 파일 | 내용 |
| --- | --- |
| `summary.json` | class별 분모와 점수, usage, source/library/resource/evaluator 해시 |
| `results.jsonl` | case별 출력과 reference, 정렬 결과, chunk parity |
| `failures.jsonl` | 실패 span과 class, native semantic type/rule/lexicon 경로 |

평가 mismatch는 정상적인 측정 결과이므로 도구 exit code는 0이다. 입력/권한 플래그/
실행 오류는 2이며, engine별 실패는 summary의 `engine_errors`에도 남는다. 완료 summary가
없는 부분 출력은 평가 결과로 승인하지 않는다. 점수뿐 아니라 engine/chunk 오류도 확인한다.

## 3. 실패 검토 → 변경 → 동일 benchmark 재실행

실패를 `잘못된 값·단위`, `미지원 구문`, `읽기 정책 차이`, `reference 의심`으로
분류한다. 외부 엔진 출력이나 후보 mapper 출력으로 정답을 자동 갱신하지 않는다.
새 규칙은 독립 작성한 의미 보존 테스트, 보호 구문, 사용자 override, chunk 경계에서
검증한다. source corpus 자체를 runtime exact 치환표로 복사하지 않는다.

```sh
# 규칙 수정 후 현재 엔진을 다시 빌드한다.
cmake --build build --parallel
ctest --test-dir build --output-on-failure
python3 benchmark/evaluators/evaluate.py \
  --corpus benchmark/generated/sproat-test.jsonl \
  --library build/libtts_text_frontend.so --resources resources \
  --limit 0 --chunk-bytes 1 7 64 \
  --output-dir benchmark/generated/after
python3 benchmark/evaluators/compare.py \
  benchmark/generated/before benchmark/generated/after \
  --output benchmark/generated/comparison.json
```

비교기는 corpus, 실제 선택 record, reference, metric policy, evaluator/adapter와
backend가 같은지 확인한다. 변경한 engine/resource 해시는 양쪽 모두 남긴다.
class별 fixed/regressed/unchanged를 계산하므로 전체 비율 상승이 퇴행을 가리지 않는다.
실패를 보고 수정한 개발 표본과 이후 새로 확보한 held-out test는 별도로 보고한다.

완료된 보고서의 실패 원인은 다음 명령으로 집계한다.

```sh
python3 benchmark/evaluators/diagnose.py benchmark/generated/after \
  > benchmark/generated/after-diagnostics.json
```

`exact`, `casefold-only-difference`, `unaligned`, `engine-error`,
`other-text-difference`, `unlabeled`를 class/reference status별로 구분한다.
대소문자만 다른 항목도 원래 exact 지표에서는 계속 실패다. Semantic class 일치는
값이 옳다는 증명이 아니며, 이 도구는 그 카테고리 분포도 따로 표시한다.
출력에는 원문·reference 문장이 들어가지 않는다.

## 공통 TN JSONL schema v1

`schema_version`, `id`, `dataset`, `split`, `language`, `text`, `expected`,
`reference_status`, `usage`, `provenance`, `spans`가 기본 필드다.
`provenance`에는 `source_url`, `revision`, `source_file`, `source_sha256`, `license`를 둔다.
`spans`는 `begin`, `end`, `written`, `spoken`, `semiotic_class`, `reference_status`를 둔다.
offset은 **입력 `text`의 UTF-8 byte 단위**이고 정렬·중복·경계·원문 일치를 검증한다.
`expected`/`spoken: null`은 정답 부재다. 빈 문자열은 실제 침묵 reference이므로 서로 다르다.
CMU pronunciation archive는 별도 schema이며 이 TN evaluator에 직접 넣지 않는다.

## 오프라인 CI와 연구 범위

```sh
ctest --test-dir build --output-on-failure -R '^external_'
```

CI에는 원작 synthetic parser/metadata/lexicon precedence/chunk 테스트만 들어간다.
외부 corpus를 받거나 업로드하지 않는다. [NeMo/Sproat 분석](../docs/research/nemo-sproat.md),
[한국어 자료 분석](../docs/research/korean-sources.md),
[CMU/국립국어원 분석](../docs/research/pronunciation-sources.md)에서
원본과 현 C architecture의 차이 및 아직 해결하지 않은 coverage를 설명한다.
