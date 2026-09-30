# 브랜드 발음 사전 (v0.24)

한국어 문장 속 로마자 회사·브랜드 이름을 사전으로 읽는다. 선택 팩이며 기본값은 꺼짐이다.

- 파일: `resources/domains/brands/ko-KR/lexicon.tsv` (6,019항목)
- 켜기: CLI `--domain` 또는 웹 실험 설정의 "브랜드 발음 사전"
- 도메인 층으로 읽으므로 사용자·고객 사전이 우선한다.
- 엔진 시작 시간: 11ms → 35ms (켰을 때만)

| 입력 | 끔 | 켬 |
|---|---|---|
| `Samsonite 캐리어` | 샘소닛 캐리어 | 샘소나이트 캐리어 |
| `Ericsson과 협력` | 어릭슨과 협력 | 에릭슨과 협력 |
| `WAK BURGER 신메뉴` | 더블유에이케이 비유알지이알 신메뉴 | 왁버거 신메뉴 |
| `TAFELRUNDE에서` | 티에이에프이엘알유엔디이에서 | 타펠룬데에서 |

## 1. 출처

| 출처 | 라이선스 | 쓰는 것 | 항목 |
|---|---|---|---|
| Wikidata 회사·브랜드 | CC0 | 영어 이름과 한국어 레이블 | 4,817 |
| 공정거래위원회 가맹정보 브랜드 목록 (2017~2025) | 이용허락범위 제한 없음 | `코드런(CODE RUN)`처럼 한글과 로마자를 함께 등록한 브랜드명 | 1,202 |

두 사전 모두 `lexicons/ko-KR/brand-*.jsonl` 원본에서 `manage.py build`로 만든다. 검토 상태는 모두 pending이다.

다음 출처는 팩에 넣지 않는다. 수집 결과는 `benchmark/generated/` 아래에만 둔다.

- **OpenDART**
  - 약관상 저작권은 금융감독원에 있고, 재배포 허용 여부가 명시되어 있지 않다.
  - 상장사 영문명·국문명 쌍 989건은 측정용으로만 쓴다.
- **KIPRIS Plus**
  - 원정보의 제3자 제공과 2차 저작물 배포를 금지한다.
  - Wikidata 검토 대기 항목을 교차 확인하는 데만 쓴다.

## 2. 고르는 방법

엔진이 지금 로마자 이름을 읽은 결과와 한글 표기를 자모 단위로 비교한다(편집거리 기반 유사도). 번역된 이름을 걸러내는 것이 목적이다.

| 출처 | 채택 | 검토 대기 |
|---|---|---|
| Wikidata | 0.6 이상 | 0.4~0.6 (레이블만) |
| 공정위 | 0.45 이상 | 여러 단어인데 단어별 대응이 약한 것 |

공정위는 브랜드 소유자가 직접 등록한 한글 표기라서 기준을 낮췄다.

두 출처 모두 다음은 넣지 않는다.

- 번역된 이름: `China Mobile` 중국이동, `Bread Farm` 빵농장
- 엔진이 이미 같게 읽는 이름
- 사전 키 문법(`^[a-z]+( [a-z]+){0,7}$`)을 벗어나는 표기
- 시스템 사전에 이미 있는 항목
- 같은 키에 읽기가 여럿인 것 (검토 대기로 보냄)

출처별로 더 거르는 것도 있다.

- **Wikidata:** 대문자로만 쓴 표기를 뺀다. `GT`, `SNT` 같은 약어는 글자로 읽어야 하기 때문이다.
- **공정위:**
  - 대문자 네 글자 이하의 한 단어는 뺀다(`TOT`, `NBM`).
  - 영어 문장에서 흔한 한 단어는 뺀다. `Handsome` → 한섬처럼 일반 단어를 덮어쓰지 않기 위해서다.
  - 대문자 단어는 글자가 아니라 단어로 비교한다(`CODE RUN` → 코드런).
  - Wikidata 팩에 이미 있는 키는 넣지 않는다. 읽기가 다르면 검토 대기로 보낸다.

마지막으로, 팩을 도메인 층으로 올린 엔진에서 모든 항목이 실제로 그 읽기로 나오는지 확인한다. 다른 규칙이 먼저 잡는 항목은 뺀다. 공정위 쪽은 `A-FREE CAFE`, `A.OK COFFEE`처럼 문장부호가 든 표기 138건이 이 단계에서 빠졌다.

## 3. 다시 만들기

```bash
python3 tools/lexicon/brands/collect_wikidata.py benchmark/generated/brand-lexicon/wikidata
python3 tools/lexicon/brands/build_pack.py benchmark/generated/brand-lexicon/wikidata \
  benchmark/generated/brand-lexicon/pack-report --library build/libtts_text_frontend.so
python3 tools/lexicon/brands/collect_ftc.py benchmark/generated/brand-lexicon/ftc 2017 2018 2019 2020 2021 2022 2023 2024 2025
python3 tools/lexicon/brands/build_ftc.py benchmark/generated/brand-lexicon/ftc \
  benchmark/generated/brand-lexicon/ftc-report --library build/libtts_text_frontend.so
```

- 공정위 수집에는 공공데이터포털 인증키가 필요하다. `~/.config/ttf/ftc_api_key`에 두면 되고, 스크립트는 키와 요청 URL을 출력하지 않는다.
- `build_ftc.py`는 흔한 영어 단어를 판정하려고 영어 평가 코퍼스(`benchmark/generated/en-eval`)를 쓴다. 다른 파일을 쓰려면 `--english-corpus`로 지정한다.
- `build_ftc.py`는 반드시 `build_pack.py` 다음에 실행한다.

## 4. 한계

- 레이블과 등록명은 발음의 권위가 아니다. 소유자 표기를 따르므로 `Refresh Bowl` → 리프레쉬보울처럼 외래어 표기법과 다를 수 있다.
- 무작위 60건을 확인한 Wikidata 정밀도는 약 95%다.
- 검토 대기는 Wikidata 1,421건, 공정위 69건이다.
- 계약은 `tests/evaluation/ko_brand_lexicon_v24.jsonl` 14건이다. 로딩, 여러 단어 조회, 조사 호응, 대문자 단어, 약어 보호를 확인하며, 구현 전에 작성했다.
