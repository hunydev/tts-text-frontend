# 브랜드 발음 사전 (v0.24)

한국어 문장 속 로마자 회사·브랜드 이름을 사전으로 읽는다. 선택 팩이며 기본값은 꺼짐이다.

- 파일: `resources/domains/brands/ko-KR/lexicon.tsv` (6,185항목)
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
| Wikidata 회사·브랜드 | CC0 | 영어 이름과 한국어 레이블 | 4,888 |
| 공정거래위원회 가맹정보 브랜드 목록 (2017~2026) | 이용허락범위 제한 없음 | `코드런(CODE RUN)`처럼 한글과 로마자를 함께 등록한 브랜드명 | 1,347 |

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
- 사전 키 문법을 벗어나는 표기. 글자와 공백, 연결 부호 `-` `'` `&`만 쓸 수 있다(`docs/latin-joiners.ko.md`). `.`이나 숫자가 들어가면 뺀다.
- 시스템 사전에 이미 있는 항목
- 같은 키에 읽기가 여럿인 것 (검토 대기로 보냄)

출처별로 더 거르는 것도 있다.

- **Wikidata:**
  - 대문자로만 쓴 표기를 뺀다. `GT`, `SNT` 같은 약어는 글자로 읽어야 하기 때문이다.
  - 별칭은 영어 레이블의 표기 변형일 때만 레이블의 한국어 이름을 받는다. 같은 글자이거나(`Coca Cola`/`Coca-Cola`), 엔진 읽기가 0.85 이상 비슷하거나, 한국어 이름이 레이블보다 별칭에 더 가까운 경우다(`Pagani Automobili`의 별칭 `Pagani` 파가니). 그 밖의 별칭은 다른 이름이 레이블의 한국어를 빌린 것이라 뺀다(`PlayStation Trophies` 플레이스테이션 네트워크, `Acer Aspire Predator` 에이서 프레데터).
  - 여러 단어 이름은 내용어마다 한국어의 한 조각과 대응해야 한다(0.2 이상, `the` `of` `and` `&` 제외). 대응하지 않으면 검토 대기로 보낸다.
  - 한국어 레이블 끝에 업종 단어가 붙어 있으면 뗀다(`Triumph` 트라이엄프 모터 → 트라이엄프, `Blue Note` 블루 노트 레코드 → 블루 노트). 떼는 조건은 둘이다. 뗀 쪽이 엔진 읽기에 더 가까워야 하고, 엔진 읽기 길이의 80% 이상이 남아야 한다. 그래서 `SwissAir` 스위스 항공은 그대로 둔다.
  - 다른 브랜드 팩(공정위)에 이미 있는 키는 넣지 않는다. 두 빌더를 어떤 순서로 다시 돌려도 결과가 같다.
  - `fashion_brand` 분류(Q2616791)는 통째로 뺀다. 이 분류로 받은 124행이 전부 폴란드 도시와 그 철자 변형이었다(Sosnowiec, Łowicz, Bochnia 등).
- **공정위:**
  - 대문자 네 글자 이하의 한 단어는 뺀다(`TOT`, `NBM`).
  - 영어 문장에서 흔한 한 단어는 뺀다. `Handsome` → 한섬처럼 일반 단어를 덮어쓰지 않기 위해서다.
  - 대문자 단어는 글자가 아니라 단어로 비교한다(`CODE RUN` → 코드런).
  - Wikidata 팩에 이미 있는 키는 넣지 않는다. 읽기가 다르면 검토 대기로 보낸다.

마지막으로, 팩을 도메인 층으로 올린 엔진에서 모든 항목이 실제로 그 읽기로 나오는지 확인한다. 다른 규칙이 먼저 잡는 항목은 뺀다(Wikidata 3건, 공정위 3건).

연결 부호가 든 키(`a-free cafe`, `mom's pizza`, `bowl & bowl`, `hewlett-packard`)는 v0.24 연결 부호 작업부터 들어간다. 그전에는 빌더가 부호를 공백으로 바꿔서, 실제 글과 맞지 않는 키가 생겼다.

### 검토 결정

생성된 항목을 검토해 빼거나 고친 결정은 `lexicons/reviews/decisions/brands.jsonl`에 둔다. 한 줄에 출처, 키, `drop` 또는 `fix`, 고친 읽기, 이유, 검토자, 날짜를 적는다. 두 빌더가 매번 이 결정을 적용하므로 다시 만들어도 결정이 유지된다. 고친 항목의 notes에는 수집된 원래 읽기와 이유가 남는다.

2026-09-30에는 Jev 0.5 미만 239건을 검토했다. 76건을 결정했고(삭제 42, 수정 34), 나머지는 유지했다.

- **삭제:**
  - 다른 이름이나 번역: `complex media` 컴플렉스 네트웍스, `artchicken` 예술치킨
  - 폴란드 지명 별칭: `elk` 에우크, `bendin` 벵진
  - 일반 영어 단어·구가 키가 된 것: `being`, `shaken`, `i've`, `the on`, `more info`, `lays`, `wanderer`
- **수정:**
  - 붙은 설명어 떼기: `four seasons` 포시즌스 호텔 → 포시즌스, `lilliput` 키즈카페릴리펏 → 릴리펏
  - 틀린 읽기 고치기: `hilti` 히트 → 힐티, `naver clova` 네이버 클라우드 → 네이버 클로바, `nippon paint` 일본 페인트 → 닛폰 페인트
- **유지:** Jev가 소유자 표기나 한국어 로마자 표기를 낮게 본 경우다. 예: 공정위 말장난 상호 `Who seek` 후식·`Meatgo` 믿고, 로마자 표기 `Baljunso` 발전소·`Bonjuk` 본죽.

2차 검토에서는 두 가지를 봤다. 결정은 누적 124건이다.

1. **흔한 영어 단어가 키가 된 단어 130건:** 대부분 그대로 둔다. 브랜드 읽기가 그 단어의 읽기와도 같아서, 사전이 없을 때의 엔진 읽기(`time` 팀, `clear` 클리)보다 낫기 때문이다(`time` 타임, `human` 휴먼, `clear` 클리어). 브랜드 표기가 단어 읽기와 다른 경우만 단어 읽기로 고쳤다(`solid` 쏠리드 → 솔리드, `arctic` 아틱 → 아크틱, `verbatim` 버바팀 → 버베이텀). 공정위의 `empower` 이엠파워, `esteem` 이스팀은 뺐다.
2. **Jev 0.5~0.65 구간 446건:** 틀린 항목은 이런 유형이었다.
   - 번역: `bristol cars` 브리스톨 자동차, `linux documentation project` 리눅스 문서화 프로젝트
   - 붙거나 빠진 말: `maotai` 마오타이주, `shinhwa company` 신화컴퍼니의, `nudie jeans` 누디진
   - 다른 언어식 읽기: `rushydro` 루스기드로(러시아어), `hitachi zosen` 히타치 조선(한자음)
   - 복수형 누락: `the hundreds` 더헌드레드
   Jev는 복수형 차이를 거의 구분하지 못한다. 그래서 복수형을 바로잡은 항목(`spaces` 스페이시스, `dean foods` 딘 푸즈)은 점수가 오히려 내려갔다.

## KIPRIS 교차 확인 검토 (2026-10-04)

검토 대기 항목 가운데 KIPRIS 상표 표기(한글·영문 병기 상표명, 권리자명)와 Wikidata 한국어 레이블이
일치한 227건을 하나씩 검토했다(검토자 Claude, 프로젝트 소유자 위임). KIPRIS 자료는 팩에 넣지 않고,
읽기는 Wikidata(또는 공정위) 표기를 쓴다. 결정은 `lexicons/reviews/decisions/brands.jsonl`의
`accept` 행으로 남고 빌더가 다시 만들 때도 지킨다.

- 그대로 채택 212건: Dyson 다이슨, Accenture 액센츄어, Bacchus 박카스, Carrefour 까르푸, Laneige 라네즈,
  Lock & Lock 락앤락, Domino's Pizza 도미노피자, Kumho Tire 금호타이어. 대문자 표기라도 단어로 읽는
  브랜드(POSCO 포스코, IMAX 아이맥스, SPAO 스파오)는 검토로 채택하면 대문자 규칙의 예외가 된다.
- 읽기를 고쳐 채택 12건: 레이블이 다른 이름이거나 단어를 더하고 빼거나 번역한 경우다(Fremantle
  프리맨틀미디어 → 프리맨틀, Merrill 메릴린치 → 메릴, Hyundai Steel 현대제철 → 현대스틸, Korail Retail
  코레일유통 → 코레일 리테일, Pelicana Chicken 페리카나 → 페리카나 치킨, Wemakeprice 위메프 →
  위메이크프라이스).
- 채택하지 않음 3건: Hermes(한국어 문장에서는 대개 패션 브랜드 에르메스), Tongrentang 동인당(한자음),
  SOLVANG 쏠뱅(같은 키가 Wikidata Solvang 솔방).
- 결과: Wikidata 팩 4,888 → 5,093건(+205), 공정위 팩 1,347 → 1,349건(+2). 4건(Honor, People,
  Dr Pepper, Yeongnam Air)은 이제 기본 엔진이 같은 읽기를 내서 넣지 않았다.
- 2026-10-04 추가분: 아직 조회하지 않은 검토 대기 167건을 조회해 21건이 일치했다. 그대로 채택
  13건(McDonald's 맥도날드, Tod's 토즈, Decca 데카, Swiss Re 스위스리 등), 읽기를 고쳐 채택 3건
  (American Airlines 아메리칸 항공 → 아메리칸 에어라인스, Korea Zinc 고려아연 → 코리아 징크, Sony Music
  Entertainment Japan → … 재팬). 번역 레이블이지만 엔진이 이미 쓰인 대로 읽는 4건(Jeju Air, Busan Bank 등)과
  Wikidata 팩에 같은 키가 있는 공정위 Pullman은 넣지 않았다. Wikidata 팩 5,108건, 공정위 팩 1,350건.
- 현재 엔진으로 빌더를 다시 돌리면 검토와 무관한 변동이 생긴다(Wikidata +113, −190; 공정위 +32, −13).
  빠지는 쪽은 대부분 엔진이 이제 같게 읽는 항목이고, 들어오는 쪽에는 검토하지 않은 레이블이 섞여
  있어(enditi engineering 비파괴 엔지니어링) 이번에는 반영하지 않았다.

## 3. 다시 만들기

```bash
python3 tools/lexicon/brands/collect_wikidata.py benchmark/generated/brand-lexicon/wikidata
python3 tools/lexicon/brands/build_pack.py benchmark/generated/brand-lexicon/wikidata \
  benchmark/generated/brand-lexicon/pack-report --library build/libtts_text_frontend.so
python3 tools/lexicon/brands/collect_ftc.py benchmark/generated/brand-lexicon/ftc 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026
python3 tools/lexicon/brands/build_ftc.py benchmark/generated/brand-lexicon/ftc \
  benchmark/generated/brand-lexicon/ftc-report --library build/libtts_text_frontend.so
```

- 공정위 수집에는 공공데이터포털 인증키가 필요하다. `~/.config/ttf/ftc_api_key`에 두면 되고, 스크립트는 키와 요청 URL을 출력하지 않는다.
- `build_ftc.py`는 흔한 영어 단어를 판정하려고 영어 평가 코퍼스(`benchmark/generated/en-eval`)를 쓴다. 다른 파일을 쓰려면 `--english-corpus`로 지정한다.
- `build_ftc.py`는 반드시 `build_pack.py` 다음에 실행한다.

## 4. 한계

- 레이블과 등록명은 발음의 권위가 아니다. 소유자 표기를 따르므로 `Refresh Bowl` → 리프레쉬보울처럼 외래어 표기법과 다를 수 있다.
- 무작위 60건을 확인한 Wikidata 정밀도는 약 95%다.
- 검토 대기는 Wikidata 1,459건, 공정위 68건이다.
- 공정위 2026년 기준 데이터(8,668행)는 2026-10-03에 받았다. 2017~2025년은 9월 30일 수집분과 같고, 2026년에서 표기 50건이 새로 들어왔다(빈플러스, 디어버거, 밀하우스, 부산리추얼커피 등).
- `F&F Entertainment`처럼 `&`로 이은 약어가 앞에 오는 이름 3건은 약어 규칙이 먼저 잡아 팩에서 빠진다.
- 별칭·단어 대응 규칙은 Jev 점수로 확인했다(`docs/lexicon-maintenance.ko.md`). 규칙으로 빠지거나 바뀐 Wikidata 항목 256건 중 133건이 Jev 0.5 미만이었다. 전체 평균은 약 5%다. Wikidata 팩의 0.5 미만은 253건에서 127건으로, 0.3 미만은 95건에서 34건으로 줄었다.
- 계약은 `tests/evaluation/ko_brand_lexicon_v24.jsonl` 22건이다. 로딩, 여러 단어 조회, 조사 호응, 대문자 단어, 약어 보호, 연결 부호를 확인하며, 구현 전에 작성했다.
