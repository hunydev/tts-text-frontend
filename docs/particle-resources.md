# 한국어 조사 자원표 편집

대상 파일은 `resources/languages/ko-KR/rules/context.tsv`다. 각 행은 **탭으로 구분한
네 필드**이며 C 재빌드 없이 고객 overlay로도 적용할 수 있다.

## 두 표의 역할

| 종류 | 키 / 값 | 사용하는 곳 |
|---|---|---|
| `particle` | 쓰인 조사 / `1` | 한국어 문맥 단서, 금액·배수 뒤의 경계, 조사 앞 공백 방지 |
| `particle_form` | 쓰인 조사 / `받침형\|모음형[\|ㄹ받침형]` | 실제로 읽은 직전 음절의 받침에 따른 조사 선택 |

`particle`의 문맥 판정은 접두사 단서를 허용한다. 하지만 **받침 교정과 금액 경계는
선언한 전체 조사와 일치해야 한다**. `나`를 추가해도 `나무`나 `나머지`를 잘라서
교정하지 않는다. 문맥 단서 자체는 형태소 분석기가 아니므로 긴 붙여 쓴 낱말과
새로운 외국어 구절은 별도로 검사한다.

교정은 앞의 읽기와 공백 없이 붙어 있고, 해당 조사 span이 아직 원문인 경우에
적용된다. `Apple`의 사용자 발음이 `사과`라면 `Apple이랑`은 `사과랑`이 된다.
이미 한 Hangul 토큰인 `사과나무`, 공백을 둔 `3 나`, 보호된 코드의 내부는 이
조사 교정 대상이 아니다. 언어 분석 전체를 대신하지 않는다.

## 2026-09-27에 추가한 쌍

| 받침 있음 (ㄹ 포함) | 받침 없음 |
|---|---|
| 이랑 | 랑 |
| 이나 | 나 |
| 이나마 | 나마 |
| 이라도 | 라도 |
| 이든 | 든 |
| 이든지 | 든지 |
| 이야말로 | 야말로 |
| 이라든지 | 라든지 |

이 여덟 쌍에는 ㄹ 예외가 없다. 기존 `으로/로`, `으로서/로서` 계열은
`으로|로|로`처럼 세 번째 필드로 ㄹ 예외를 유지한다. 서술격의 선택적 축약
`이며/며`, 호격·서술격이 겹치는 `아/야`, `이야/야`는 이 표에 추가하지 않았다.
`이랑은` 같은 조사 연쇄 및 붙여 쓴 서술어의 자동 분해도 이번 범위에 넣지 않았다.

금액에는 두 표가 모두 필요하다. `1만원라도`는 먼저 CURRENCY(value=10000,
unit=KRW)로 읽고, `원`의 받침에 따라 `만 원이라도`가 된다. 출력 문자열 전체를
고치는 exact 예외나 pronunciation lexicon 항목이 아니다.

## 안전한 수정 절차

양쪽 표기 모두 등록한다. 아래 코드 블록의 필드 사이에는 실제 탭이 있다.

```tsv
particle	ko-KR	랑	1
particle	ko-KR	이랑	1
particle_form	ko-KR	랑	이랑|랑
particle_form	ko-KR	이랑	이랑|랑
```

1. 받침·모음·ㄹ 받침 각각에서, 맞는 조사 유지와 잘못된 조사 교정을 함께 작성한다.
2. 금액·단위·사전 발음 뒤 사례, 공백·긴 낱말·코드 보호의 부정 예를 추가한다.
3. validator, 독립 계약 corpus, byte-split/trace 검사를 통과시킨다.
4. 고객 overlay에서 선택된 layer를 `inspect --trace`로 확인한다.

```sh
build/ttf validate-resource --resource-layer customer --require-complete-validation customer.tsv
build/ttf inspect --output-mode speech --customer customer.tsv --trace '1만원라도 보태세요'
ctest --test-dir build --output-on-failure -R 'particle|compiled_resources'
```

특정 표기의 교정을 끄려면 고객 표에서 `랑 → 랑|랑`, `이랑 → 이랑|이랑`처럼 양쪽
키에 동일 출력 두 개를 넣는다. `particle`은 존재 기반의 문맥/경계 표이므로 `0`으로
끄는 형식은 지원하지 않는다. 전반적인 조사 교정 비활성 옵션과 혼동하지 않는다.
공개 ABI는 그대로다. 전체 회귀 검사가 찾아낸 문자 치환 경계 때문에 Core에는 작은
보호 조건을 추가했다: 미분류 OTHER identity 또는 unreadable-cluster의 문자 이름 뒤에는
조사를 교정하지 않는다. 고객 `漢 → 한`, `1️⃣ → 키캡일`을 적용해도 `가漢나` → `가한나`,
`가1️⃣나` → `가키캡일나`라는 기존 계약을 지킨다. 문자 이름을 부여하는 것만으로 명사
경계까지 추정하지 않는 정책이다. 이 부류의 문자 뒤에 실제 조사가 붙은 경우도 자동
교정하지 않는 제한이 있으며, 별도 의미 분류/명시적 읽기가 필요하다.

## 근거와 측정

국립국어원의 [랑/이랑 설명](https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?mn_id=216&pageIndex=1&qna_seq=318773)과
[나마/이나마 설명](https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?mn_id=216&pageIndex=1&qna_seq=320391),
한국어기초사전의 [이라도](https://krdict.korean.go.kr/eng/dicSearch/SearchView?ParaWordNo=78504&nation=eng),
[라도](https://krdict.korean.go.kr/chn/dicSearch/SearchView?ParaWordNo=78508),
[든](https://krdict.korean.go.kr/eng/dicSearch/SearchView?ParaWordNo=86261&nation=eng)을
문법 참고로 확인했다(2026-09-27). 사전 정의·예문을 복사하지 않고 표와 시험 문장은
직접 작성했다. 상세 사용 범위는 [THIRD_PARTY.md](THIRD_PARTY.md)에 기록한다.

`tests/evaluation/particle_table_v21.jsonl` 72건을 변경 전에 고정했다.
`tn_cycle` 비교는 35/72 → 72/72, fixed 37 / regressed 0이다. 이는 개발자가 작성한
계약의 통과율이고, 한국어 외부 TN 자료나 독립 사람 검수에 의한 정확도가 아니다.
웹 corpus **조사 자원표 · (이)랑·(이)라도·(이)나마**에서도 같은 계약을 실행한다.
