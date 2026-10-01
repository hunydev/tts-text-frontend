# 한국어 대표 원문과 Jev TN 비교 (v1)

전체 한국어 참조 평가셋(AI Hub 159, 142,255문장)에서 카테고리별 대표 원문을 뽑아 두 가지 용도로 씁니다. 기존 평가셋은 그대로 둡니다.

1. **Jev TN 비교 (`/jev-eval`):** 참조 정답 없이 여러 TN의 결과를 Jev로 채점해 비교합니다.
2. **참조 일치 보고서 (`/ko-quality`):** 대표 문장만으로 만드는 참조 일치 보고서입니다. Jev는 쓰지 않습니다.

## 1. 대표 원문 선발

`benchmark/importers/korean_release/select_representative.py`로 뽑습니다. 카테고리마다 다음 순서로 고릅니다.

1. 입력 그룹마다, 그리고 숫자·로마자를 가린 문장 틀마다 한 문장만 남깁니다.
2. 이미 고른 문장과 가장 덜 겹치는 문장을 차례로 고릅니다. 겹침은 가린 틀의 문자 3-gram으로 잽니다(Jaccard). 아직 다루지 않은 주제나 경로가 있으면 가산점을 줍니다.

카테고리당 최대 200문장이고 사투리는 57문장 전부입니다. 모두 4,057문장입니다.

```bash
K=benchmark/generated/ko-release
# Jev 비교용 원문: 참조를 쓰지 않음
python3 benchmark/importers/korean_release/select_representative.py $K/import-v1 benchmark/generated/jev-eval/ko-v1 --sentences-only
# 참조 일치 보고서용: 채점 가능·검토 신호 없음·참조 오류 목록 제외
python3 benchmark/importers/korean_release/select_representative.py $K/import-v1 $K/import-rep-v1 \
  --reviews $K/reviews/reference-errors-v1.jsonl
```

원본 문장은 재배포 권한이 확인되지 않았습니다. 결과물은 무시 목록에 있는 `benchmark/generated/` 아래에만 둡니다.

## 2. Jev TN 비교 페이지 (`/jev-eval`)

**표 구성:** 카테고리 | 원문 | TN 결과 | Jev 평가(점수와 평가 버튼)

**TN 결과 채우기:** 결과는 시스템 이름별로 서버에 저장합니다(`benchmark/generated/jev-eval/<set>/systems/<이름>.json`). 채우는 방법은 세 가지입니다.

- 칸에 직접 입력합니다.
- **결과 가져오기**로 파일을 불러옵니다. 형식은 다음 중 하나입니다.
  - JSONL `{"id", "actual"}`
  - JSON `{id: 결과}`
  - TSV `id⇥결과`
  - 원문 순서대로 한 줄씩 적은 TXT
- **우리 TN으로 채우기**를 누릅니다. 벤치마크와 같은 speech 옵션으로 이 엔진의 결과를 채웁니다.

다른 TN에 넣을 원문은 **원문 내보내기**로 받습니다(JSONL·TSV·TXT).

**평가:**

| 버튼 | 대상 |
|---|---|
| 전체 평가 | 채워진 모든 문장 |
| 수정된 내용만 평가 | 아직 채점하지 않았거나 채점 뒤 결과가 바뀐 문장만. 다른 문장의 점수는 그대로 둡니다 |
| 행의 평가 버튼 | 그 문장 하나만 |

- **인정 기준:** Jev 질문은 "결과가 원문을 소리 내어 읽는 올바른 방법인가"(noul, `tools/web/jev.py`)입니다. 점수가 0.7 이상이고 결과에 숫자·로마자·기호가 남지 않으면 인정합니다.
- **비교 표:** 시스템마다 전체와 카테고리별 인정률을 파란색으로 보여줍니다.
- **문장 표 정렬:** 원문 순서, Jev 점수 낮은 순, 높은 순 중에서 고릅니다. 낮은 순으로 두면 가장 나쁜 결과부터 고칠 수 있습니다. 평가하지 않은 문장은 맨 뒤에 둡니다. 카테고리와 보기(빈 결과·불인정·인정) 필터와 함께 쓸 수 있습니다.
- **캐시와 비용:** 같은 원문·결과 쌍은 `benchmark/generated/jev-cache/ko-KR.jsonl` 캐시에서 답하므로 다시 과금하지 않습니다. 처음 전체를 채점하면 약 $0.10입니다. Jev 평가는 서버 전체에서 한 번에 하나만 실행합니다.

## 3. 참조 일치 보고서와 외부 출력

```bash
python3 benchmark/evaluators/korean_release.py --import-dir $K/import-rep-v1 \
  --library build/libtts_text_frontend.so --resources resources --out $K/reports/<run> \
  [--predictions legacy.jsonl --system legacy-tn]
```

- `--predictions`는 한 줄에 `{"id", "actual"}`인 다른 시스템의 출력을 같은 규칙으로 채점합니다.
- 외부 출력에는 엔진 구간 정보가 없습니다. 그래서 문장 점수만 의미가 있습니다.

## 4. Jev의 한계

첫 측정(4,057문장, 우리 TN)에서 Jev 인정과 참조 일치의 관계는 카테고리 종류에 따라 크게 달랐습니다.

| 카테고리 | Jev 점수로 참조 일치를 구분한 정도(AUC) |
|---|---|
| 번호류 외 | 0.68 |
| 번호류(사업자번호·신분증번호·자동생성번호·개인고유번호·금융-은행·통신번호) | 0.56 |

- **번호류:** 긴 숫자열을 한 자리씩 읽는 카테고리에서는 Jev가 자리별 대조를 하지 못합니다. 맞는 출력도 0.4~0.7로 봅니다. 예: `121-00-98265` → 일이일 다시 공공 다시 구팔이육오가 0.51. 페이지에서는 ※로 표시하며 참고용입니다.
- **번호류 외:** 참조와 다르지만 맞는 읽기를 인정하는 데 쓸모가 있습니다.
- **미세한 오류:** 모음 하나 차이 같은 미세한 발음 오류는 놓칠 수 있습니다.
- **성격:** Jev 점수는 사람 검수가 아닙니다.
