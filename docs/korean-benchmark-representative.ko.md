# 한국어 대표 문장 평가셋과 Jev 채점 (v1)

전체 한국어 참조 평가셋(AI Hub 159, 142,255문장)에서 카테고리별 대표 문장을 골라 만든 평가셋입니다. 기존 평가셋은 그대로 두고 따로 만들었습니다.

문장마다 참조 일치와 함께 TypeSafe Jev 점수를 매깁니다. 웹 `/ko-quality`에서 "대표 문장 v1" 보고서로 볼 수 있습니다. 같은 기준으로 다른 시스템(예: legacy TN)의 출력도 채점할 수 있습니다.

## 1. 선발

`benchmark/importers/korean_release/select_representative.py`로 뽑습니다.

1. 채점 가능하고, 구조 검토 신호와 상충 입력이 없고, 참조 오류 목록(`reviews/reference-errors-v1.jsonl`)에 없는 문장만 후보로 둡니다.
2. 입력 그룹마다, 그리고 숫자·로마자를 가린 문장 틀마다 한 문장만 남깁니다.
3. 카테고리마다 이미 고른 문장과 가장 덜 겹치는 문장을 차례로 고릅니다. 겹침은 가린 틀의 문자 3-gram으로 잽니다(Jaccard). 아직 다루지 않은 주제나 경로가 있으면 가산점을 줍니다.

카테고리당 최대 200문장이고, 사투리는 57문장 전부를 씁니다. 모두 4,057문장입니다.

```bash
K=benchmark/generated/ko-release
python3 benchmark/importers/korean_release/select_representative.py $K/import-v1 $K/import-rep-v1 \
  --reviews $K/reviews/reference-errors-v1.jsonl
```

원본 문장은 재배포 권한이 확인되지 않았습니다. 결과물은 무시 목록에 있는 `benchmark/generated/` 아래에만 둡니다.

## 2. 채점

```bash
python3 benchmark/evaluators/korean_release.py --import-dir $K/import-rep-v1 \
  --library build/libtts_text_frontend.so --resources resources --out $K/reports/<run> --jev
```

- **질문:** "`spoken`이 `written`을 소리 내어 읽는 올바른 방법인가"(noul). `tools/web/jev.py`에 있는 웹 채점과 같은 질문입니다.
- **인정 기준:** 점수가 0.7 이상이고, 출력에 숫자·로마자·기호가 남지 않은 문장입니다.
- **캐시:** 결과는 `benchmark/generated/jev-cache/ko-KR.jsonl`에 쌓입니다. 원문과 출력이 같은 쌍은 다시 과금하지 않습니다.
- **비용:** 한 번 전체 채점에 입력 토큰 약 220만 개, $0.10 정도입니다.
- **보고서 항목:** 인정률, 평균, 0.4 미만, 원문 표기 남음을 전체와 카테고리별로 냅니다. 여기에 "참조와 달라도 Jev가 인정한 문장"과 "참조와 맞는데 Jev가 인정하지 않은 문장"을 더합니다.

## 3. 다른 시스템 비교 (legacy TN)

```bash
python3 benchmark/evaluators/korean_release.py --import-dir $K/import-rep-v1 \
  --library build/libtts_text_frontend.so --resources resources --out $K/reports/<run>-legacy \
  --predictions legacy.jsonl --system legacy-tn --jev
```

- `legacy.jsonl`은 한 줄에 `{"id": ..., "actual": ...}`입니다. id는 `import-rep-v1/selection.json`에 있습니다.
- 출력이 없는 id는 엔진 오류로 셉니다.
- 평가 규칙이 같으므로 두 보고서의 `comparison_key`가 같고, 웹에서 서로 비교할 수 있습니다.
- 외부 출력에는 엔진 구간 정보가 없습니다. 그래서 구간 점수는 모두 정렬 실패로 나오고, 문장 점수와 Jev만 의미가 있습니다.

## 4. 첫 측정 (2026-10-01, `20261001-rep-v1`)

| 지표 | 값 |
|---|---|
| 참조 일치 (정책 변이 무시) | 73.48% |
| Jev 인정 | 67.46% (평균 0.71) |
| 참조와 달라도 Jev 인정 | 516문장 |
| 참조와 맞는데 Jev 불인정 | 760문장 |
| 원문 표기 남음 | 49문장 |

Jev와 참조 일치의 관계:

| 기준값 | 참조 일치 문장 통과 | 참조 불일치 문장 통과 |
|---|---|---|
| 0.5 | 91.9% | 78.2% |
| 0.7 | 74.5% | 49.4% |

구분력은 카테고리 종류에 따라 크게 다릅니다.

| 카테고리 | AUC | 참조 일치 중앙값 | 참조 불일치 중앙값 |
|---|---|---|---|
| 번호류 외 | 0.68 | 0.81 | 0.75 |
| 번호류(사업자번호·신분증번호·자동생성번호·개인고유번호·금융-은행·통신번호) | 0.56 | 0.65 | 0.61 |

참조 불일치에는 정책상 다른 정답 읽기도 섞여 있습니다. 그래서 이 AUC는 Jev의 실제 오류 구분력보다 낮게 나옵니다. 오류를 만들어 넣은 보정 시험에서는 값 오류 AUC가 0.97이었습니다(`docs/web-workbench.md`).

## 5. 한계

- **번호류:** 긴 숫자열을 한 자리씩 읽는 카테고리에서는 Jev가 자리별 대조를 하지 못합니다. 맞는 출력도 0.4~0.7로 봅니다(`121-00-98265` → 일이일 다시 공공 다시 구팔이육오가 0.51). 웹 표에서는 ※로 표시하며 참고용입니다. 이 카테고리는 참조 일치나, 숫자를 되읽어 값을 대조하는 검사로 보는 편이 맞습니다.
- **미세한 오류:** Jev는 모음 하나 차이 같은 미세한 발음 오류를 놓칠 수 있습니다.
- **성격:** Jev 점수는 사람 검수가 아닙니다. 참조 하나로는 가를 수 없는 "다른 정답 읽기"를 가늠하는 보조 지표입니다.
