# 한국어 문장 속 영어 단어

한국어 문장에 섞인 영어 단어를 한국어 관례대로 읽는다(2026-10-03, 프로젝트 소유자 요청).
영어 문장은 영어로 둔다.

## 1. 모음이 없는 짧은 소문자 단어는 알파벳으로 읽는다

`tv` 티브이, `sns` 에스엔에스, `kbs` 케이비에스, `www` 더블유더블유더블유. 이전에는 철자 규칙으로
추측해 트브, 스느스, 우우우가 됐다.

- 2~6글자이고 모음 글자(a e i o u)가 없으면 약어로 본다. `y`는 첫 글자가 아니면 모음으로 센다
  (`sky`·`gym`은 단어, `ytn`은 약어).
- 선언된 약어(`Mr`, `Dr`)는 그 읽기를 따른다. 한글이 있는 문장에만 적용한다
  (`languages/en-US/pack.c`).

## 2. 자주 쓰는 영어 단어 283개

`lexicons/ko-KR/english-common-v24.jsonl`(분류 common, 기본 사전). 외래어 표기법 영어 세칙과 굳은
관례 표기로 직접 작성했다: of 오브, with 위드, you 유, me 미, Korea 코리아, Life 라이프, Good 굿,
School 스쿨, Time 타임, James 제임스, Guam 괌, ost 오에스티, iot 아이오티, haha 하하.

- 단어 선정에는 한국어 벤치마크와 국립국어원 개체명 사전 2022 예문(로컬 전용)의 빈도를 썼다.
  철자 추측이 틀린 단어만 골랐고, 읽기를 말뭉치에서 가져오지 않았다.
- 모든 항목은 `lexicon_case lower`다. 전부 대문자로 쓴 표기는 알파벳으로 읽는다
  (`AS` 에이에스, `IS` 아이에스, `HMM` 에이치엠엠). 이 블록은 `tools/lexicon/english/author_common.py`가
  `resources/languages/ko-KR/rules/numeric.tsv`에 만든다.
- 코드 판정(`ko:alnum-identifier`)은 대문자 글자 부분(`EBS8102`, `AIR4930`)과 다섯 자리 이상 숫자
  (`you501073`)를 계속 코드로 읽는다.

## 3. CMU 영어 발음 사전 (선택)

웹 옵션 `영어 발음 사전`(`english_dictionary=cmu`)을 켜면 사전에 없는 영어 단어를 CMU 발음 사전
(약 11만 7천 단어)의 발음으로 읽는다(Helsinki 헬신키 → 헬싱키, Paul 패얼 → 폴).

- 기본은 끔이다. 미국식 발음을 옮기므로 관례 표기와 어긋나는 경우가 있다(Korea 코리어, Golf 갈프).
  직접 작성한 사전(위 283개, 지명, 브랜드)과 사용자·고객 사전이 우선하므로 Korea는 코리아로 남는다.
- 사전 파일은 저장소에 넣지 않는다. CMU importer의 runtime export
  (`benchmark/generated/cmudict-runtime-full/pronunciation.tsv`, 또는 환경 변수 `TTF_CMU_RUNTIME`)를
  도메인 층으로 불러온다. 만드는 방법은 `benchmark/importers/cmudict/README.md`에 있다.
  CMU 라이선스(BSD 형식)는 재배포 시 원본 `COPYING` 고지를 요구한다.
- 켜면 엔진 시작이 약 0.6초 늘어난다. C API·CLI에서는 같은 파일을 `--domain`으로 불러오면 된다.
- 국립국어원 개체명 사전 2022(로컬 전용)의 개체 표기 쌍 288개에서 CMU를 켜면 일치가 +19, −5였다
  (2026-10-03 측정, 이 문서의 단어 사전을 넣기 전).

## 측정

- 한국어 벤치마크 +80, −0. 영어 벤치마크는 출력 변화가 없다.
- 국립국어원 개체명 사전 2022(로컬 전용)의 개체 표기 쌍 288개 중 엔진 읽기 일치가 80개에서
  92개가 됐다(로마자 지명 사전 포함). 로마자가 든 예문 16,423개 중 1,185개가 바뀌었다.

## 다시 만들기

```bash
python3 tools/lexicon/english/author_common.py
```
