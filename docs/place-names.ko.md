# 로마자 한국 지명 사전

`Incheon 공항` 인천 공항, `Jamsil 야구장` 잠실 야구장, `Gangnam 스타일` 강남 스타일처럼
한국어 문장에 로마자로 쓴 한국 지명을 한국어로 읽는다(2026-10-03, 프로젝트 소유자 요청).

## 출처와 고르는 방법

- Wikidata(CC0)에서 대한민국에 있고 좌표·한국어 레이블·영어 레이블이 있는 항목 29,458개를
  받는다(`tools/lexicon/places/collect_places.py`).
- 영어 레이블이 한국어 레이블의 **개정 로마자 표기**일 때만 쓴다. 영어 표기를 개정 로마자 음절로
  나눈 결과 가운데 하나가 한국어 레이블(또는 행정 접미사를 뺀 이름: 인천광역시 → 인천)과 정확히
  같아야 한다. `Gangnam District`/강남구, `Jamsil-dong`/잠실동에서는 `Gangnam` 강남, `Jamsil` 잠실도
  만든다.
- 빼는 것:
  - 영어 평가 코퍼스에 세 번 이상 나오는 단어
  - 영어 사전(CMUdict)에 있는 단어나 이름은 그 장소의 공식 영어 레이블일 때만 남긴다
    (`Seoul`은 남고, `Guam`·`hail`(Hail-ri)은 빠진다). CMUdict는 빌드할 때 로컬 사본으로 거르는
    데만 쓰고 저장소에 넣지 않는다.
  - 한 음절 읽기(성씨 Jang, Yang 등), 다른 사전이 이미 가진 키, 읽기가 둘로 갈리는 키,
    사전 없이도 같게 읽는 키
- 결과는 `lexicons/ko-KR/place-wikidata.jsonl` 9,407건(분류 `places`, 기본 사전, 검토 대기)이다.

## 측정

- 한국어 벤치마크와 영어 벤치마크는 출력 변화가 없다(벤치마크에 로마자 지명이 없다).
- 국립국어원 개체명 사전 2022(로컬 전용) 예문 중 로마자가 든 16,423문장에서 95문장이 바뀌었고,
  확인한 범위에서 모두 개선이다(Hilton Busan 힐턴 부산, Gangwon 2024 강원, www.gongju.go.kr 공주).
- 같은 자료의 개체 표기 쌍 288개 중 엔진 읽기 일치는 75개에서 80개로 늘었다.

## 다시 만들기

```bash
python3 tools/lexicon/places/collect_places.py benchmark/generated/places-wikidata
python3 tools/lexicon/places/build_places.py benchmark/generated/places-wikidata \
  --library build/libtts_text_frontend.so \
  --english-words benchmark/generated/cmudict-runtime-full/pronunciation.tsv
```

`--english-words`에는 CMUdict importer의 runtime export(`benchmark/importers/cmudict/README.md`)를 쓴다.
