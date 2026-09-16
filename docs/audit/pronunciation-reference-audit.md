# 외래어 발음·콘텐츠 기준값 감사

## 범위와 결론

감사 시작 기준은 `ca1d765`의 파일이다. 아래 수치는 **읽고 검토한 항목 수**이며,
정답으로 승인한 항목 수나 실사용 정확도가 아니다. 감사는 출력 텍스트·문맥·기준값의
근거를 대상으로 했으며 음성을 합성하거나 청취하지 않았다. 이 문서 작성자는 개발
에이전트이며, 독립적인 한국어·영어 전문가나 고객의 승인으로 표시하지 않는다.

| 검토 대상 | 항목 수 | 현재 근거의 성격 |
| --- | ---: | --- |
| `tests/golden/foreign-pronunciation/cases.tsv` | 30 | 사전 조회·외래어 렌더링 개발 회귀값 |
| `tests/evaluation/content_v7.jsonl` | 40 | 콘텐츠 이름·발음·제목 문맥의 합성 개발 사례 |
| `tests/evaluation/multiword_v8.jsonl` | 30 | 15개 발음/공백 사례, 15개 영어·제목·리터럴 보존 사례 |
| `tests/evaluation/protected_cases.jsonl` | 40 | 리터럴 보존과 주변 문장 정규화 계약 |
| 위 네 코퍼스 합계 | **140** | 전 항목 검토, 전 항목 언어학적 승인이라는 뜻이 아님 |
| 한국어 직접 발음 사전 | **215** | 프로젝트가 작성한 초기 선택값, 독립 심의 없음 |
| 영어 발음 사전 | 18 | 단어 8개와 숫자 10개를 위한 작은 발음 기호 사전 |
| 영어 grapheme → phone 표 | 49 | 탐욕적 최장 일치 방식의 초기 fallback 표 |

수량 범위·월 읽기 등 전체 기준값의 구조 검사는 별도
[`reference_reviews.json`](../../tests/evaluation/reference_reviews.json)과
[`audit_references.py`](../../tools/evaluation/audit_references.py)에 기록한다.
파일별 기본 분류를 상속하더라도 각 사례의 `review_status`는 독립 검토 대기 상태다.

**발견한 핵심 문제:** 기준값을 구현 전에 작성하고 고정하는 것만으로 기준값이
옳아지지는 않는다. 잘못된 조사, 부자연스러운 합성 문장, 임시 발음값도 그대로
회귀 테스트에 고정되어 있었다. 사전과 기대값을 같은 개발자가 작성한 경우 사전
적용 여부는 검증할 수 있지만 사전 발음 자체의 정확성을 독립 검증한 것은 아니다.

## 1. 명백한 문장 오류: `content_v7`의 7개 조사

감사 시작 시 아래 7개 사례가 받침 있는 한글 발음 뒤에 `를`을 붙인 기대값을
가지고 있었다. `Photoshop`의 표기 선택과 별개로 `포토샵를`이라는 결합은 잘못이다.

| 사례 ID | 감사 시작 당시 기대값 일부 | 자연스러운 입력을 만드는 방법 | 그 입력의 기대값 일부 |
| --- | --- | --- | --- |
| `content-v7-thumbnail` | `섬네일를 소개합니다.` | `thumbnail을 소개합니다.` | `섬네일을 소개합니다.` |
| `content-v7-timeline` | `타임라인를 소개합니다.` | `timeline을 소개합니다.` | `타임라인을 소개합니다.` |
| `content-v7-keyframe` | `키프레임를 소개합니다.` | `keyframe을 소개합니다.` | `키프레임을 소개합니다.` |
| `content-v7-codec` | `코덱를 소개합니다.` | `codec을 소개합니다.` | `코덱을 소개합니다.` |
| `content-v7-livestream` | `라이브스트림를 소개합니다.` | `livestream을 소개합니다.` | `라이브스트림을 소개합니다.` |
| `content-v7-photoshop` | `포토샵를 사용합니다.` | `Photoshop을 사용합니다.` | `포토샵을 사용합니다.` |
| `content-v7-capcut` | `캡컷를 사용합니다.` | `CapCut을 사용합니다.` | `캡컷을 사용합니다.` |

이 오류는 원문 생성 템플릿이 모든 영단어 뒤에 `를`을 붙인 데서 생겼다. 현재
frontend는 원문의 조사를 유지하므로, 같은 잘못된 원문에 자연스러운 기대값만
붙이면 **조사 자동 교정 기능을 새로 요구하는 테스트**가 된다. 자연스러운 문장
품질을 검증하려면 입력과 기대값을 함께 고치고 변경 이유를 남겨야 한다. 잘못된
입력 보존을 확인하려는 테스트는 별도의 보존 계약으로 명시해야 한다.

확인 위치는 위 JSONL의 ID와 [`docs/content-v7.md`](../content-v7.md)의
“source particles are retained” 설명이다. 후속 수정이 적용되어도 이 표는 감사
시작 때 발견한 문제의 이력으로 유지한다.

## 2. 사전 발음: 근거 충돌과 임시 선택값

### 외부 1차 자료와 대조가 필요한 항목

| 사전 키 / 현재 감사 기준값 | 발견한 근거 | 판단과 권고 |
| --- | --- | --- |
| `aftereffects → 애프터이펙츠`, `after effects → 애프터 이펙츠` | Adobe 공식 한국어 문서의 제품명은 `애프터 이펙트`이다. | 제품명 표기를 기본으로 할지 영어 음성의 복수형을 옮길지 결정한 근거가 없다. 두 키와 관련 코퍼스를 함께 검토한다. |
| `postgresql → 포스트그레에스큐엘` | PostgreSQL 공식 FAQ는 `post-GRES-que-ell`이라고 안내한다. | `...그레 에스...`로 분리한 현재값을 공식 발음의 한국어 렌더링으로 재검토한다. `포스트그레스큐엘`은 검토 후보이지 이 감사로 자동 승인된 값이 아니다. |
| `photoshop → 포토샵` | Adobe 공식 한국어 문서에서도 `포토샵`을 사용한다. | 표기 변형만 보고 `포토숍`으로 자동 교체하지 않는다. 위에서 확정한 오류는 조사 결합이다. |

Adobe 제품명 근거: [공식 한국어 After Effects 안내](https://blog.adobe.com/ko/publish/2022/10/27/max-2022-new-after-effects-features-shipping-in-public-beta),
[공식 Photoshop 블로그](https://blog.adobe.com/ko/topics/photoshop).
PostgreSQL 발음 근거: [프로젝트 공식 FAQ](https://www.postgresql.org/about/press/faq/).
웹의 발음 설명 텍스트만 확인했으며 링크된 음성은 평가하지 않았다. 다른 언어의
공식 명칭과 한국어 음역의 유일한 정답은 동일한 개념이 아니므로, 이 자료를 근거로
선택한 **정책과 검토자**를 남겨야 한다.

### 독립 발음 검토 전에는 임시값으로 취급할 항목

- `SQL → 에스큐엘`, `GIF → 기프`처럼 복수의 읽기 정책이 가능한 약어.
- `fourth → 포스`, `sixth → 식스스`, `eighth → 에이스`,
  `hundredth → 헌드레드스` 등 영어 서수의 한글화. 영어 서수 의미 분류와 영어
  verbalizer의 정답 여부를, 한글 음역 표기의 품질과 분리해야 한다.
- `100th → 원 헌드레드스`는 foreign golden의 고정값이지만 독립 발음 검토
  기록이 없다. 고정 회귀값이라는 이유로 표준 외래어 표기라고 설명하지 않는다.
- `credit → 크레디트`, `debit → 데빗`, `swap → 스와프` 등 금융 발음은
  기관별 용어집과 대상 서비스의 읽기 선택이 필요하다. 번역(예: 신용/차변)과
  음역을 섞어서 수정하면 안 된다.
- 사용자 최초 요구의 `CPU → 씨피유` 같은 명시적 선호는 프로젝트 계약의
  근거가 있다. 다른 표기 관행이 있다는 이유로 사용자 계약을 임의 변경하지 않는다.

215개 항목 전체가 개발자 작성 seed이다. 위 목록은 재검토 우선순위이며 나머지
항목이 전문가에게 승인되었다는 뜻은 아니다. 직접 발음 사전과 원어 발음 사전을
모두 바꾸는 경우 어느 경로가 실제로 사용되는지 `inspect`의 사전 계층을 확인한다.

## 3. fallback은 발음 정답 코퍼스로 승격하지 않는다

아래는 140개 주 검토 항목 밖에서 교차 확인한 관련 테스트다.

| 위치 | 현재 확인 대상 | 올바른 결과 해석 |
| --- | --- | --- |
| `tests/evaluation/cases.jsonl`, `unknown-foobar` | `foobar → 푸바` | 명시적으로 선택한 prototype 렌더링 회귀값. 일반 미등록 단어 발음 정확도 근거가 아님. |
| `tests/unit/test_ko_pack.c` | 직접 emergency mapper의 `wifi → 위피` | fallback 경로·결정성 테스트. 보통의 WiFi 발음이 올바르다는 판정이 아님. |
| `tests/integration/test_frontend.c` | 사전 일부 제거 후 WiFi, `foobarquux`가 비어 있지 않고 ASCII Latin을 남기지 않는지 확인 | 비어 있지 않은 발화 가능 문자 출력의 계약만 검증. 발음 정확성을 검증하지 않음. |
| `advanced-content-unknown-word`, `challenge-content-unknown-word` | `quasar → 퀘이사` | 현재는 명시적인 시스템 사전 항목이므로 미등록 단어 시험으로 집계하면 안 됨. |

회귀용 exact 문자열은 유지할 수 있지만 분류는 `provisional-pronunciation`이어야
한다. 미등록 단어 품질 평가는 사전 조회 경로를 확인한 별도 사례, 출처 있는
원어 발음, 검토된 한글 렌더링을 필요로 한다. 여기서도 평가는 출력 텍스트로만 한다.

## 4. 보존·문맥 정책을 언어학적 정답으로 설명하지 않는다

- `protected_cases.jsonl` 40개는 코드·태그·경로 원문을 유지하는 계약이다.
  `<code>GPU</code>`를 유지하는 출력이 자연스럽게 읽히는 문장이라는 판정은 아니다.
  TTS 연결 시 이런 span을 읽을지 생략할지는 별도 소비자 정책이 필요하다.
- 닫히지 않은 backtick·태그·주석 3개는 `flush()` 때 원문을 유지하는 계약이다.
  올바른 HTML/Markdown이나 자연스러운 콘텐츠의 품질 점수로 합산하지 않는다.
- `Apple 3kg`, `int GPU = 3` 같은 코드 내부 데이터는 의도적인 불투명 payload다.
  주변 문장에 `3개`를 붙인 사례도 경계 검증용 합성 입력이다.
- `content_v7`의 9개 제목 사례 및 `multiword_v8`의 제목 3개는 **명시적 제목
  라벨 뒤 영어 유지**라는 읽기 정책이다. 모든 외국어 제목이 반드시 영어로
  읽혀야 한다는 보편적 기준으로 사용할 수 없다.
- `multiword_v8`의 30개 중 영어 보존 10개, 제목 보존 3개, 리터럴 보존 2개는
  발음 사전의 한글 품질을 평가하지 않는다. 나머지 15개도 12개 seed 이름과 그
  대소문자·공백 변형이므로 15개의 독립적인 어휘 일반화 성공으로 세지 않는다.

### 범위 코퍼스에서 교차 확인한 부적절한 템플릿

`finance_ranges_v7.jsonl`의 `range-kg`는 감사 시작 시
`금액은 1kg~2kg입니다.`라는 입력을 사용했다. 질량을 금액이라고 부르는 문장이므로
금융 품질 근거로 부적절하다. 자연스러운 양의 사례는 `무게는 1kg~2kg입니다.`처럼
작성해야 한다. `range-mismatch-units`의 `금액은 1kg~2km입니다.`와
`range-mismatch-scale`의 `금액은 1만원~2kg입니다.`는 의미가 맞지 않는 입력을
보존하는 **오입력 방어 시험**으로만 분류하고, 설명 문구도 `입력값`처럼 바꿔야 한다.
금액 범위를 `다시`로 읽는 문제는 이 감사 문서로 합리화하지 않고 별도 정책·구현
수정 대상으로 기록한다.

## 5. 무엇을 어느 파일에서 바꾸는가

| 목적 | 수정 가능한 원본 리소스 | 비고 |
| --- | --- | --- |
| 일반 영어 단어·서수 한글 발음 | `resources/languages/ko-KR/lexicon/common/default.tsv` | 89개. TN 숫자 알고리즘과 분리된 발음값이다. |
| 약어·이니셜 읽기 | `resources/languages/ko-KR/lexicon/acronyms/default.tsv` | 28개. SQL/GIF 등 고객 선호를 명시한다. |
| 브랜드·기술 단어 | `lexicon/brands/default.tsv`, `lexicon/technology/default.tsv` | 각각 22/24개. 이 표의 짧은 경로는 한국어 언어 리소스 아래다. |
| 금융·콘텐츠 용어 | `lexicon/finance/default.tsv`, `lexicon/content/default.tsv` | 각각 12개. `finance` 디렉터리명만으로 TN 도메인 정책이 켜지지 않는다. |
| 고유명사 | `lexicon/proper_nouns/default.tsv` | 16개. AfterEffects 단일 토큰 등의 표기. |
| 여러 단어로 된 이름 | `lexicon/multiword/{finance,content,places_media}.tsv` | 합계 12개. After Effects처럼 띄어 쓴 키는 별도 항목이다. |
| 영어 원어 발음 기호 | `resources/languages/en-US/dictionaries/pronunciation.tsv` | 직접 KO 발음 사전이 없을 때 사용하는 작은 원어 사전. |
| 영어 G2P prototype | `resources/languages/en-US/g2p/graphemes.tsv` | 49개. 한 행을 바꾸면 여러 미등록 단어에 영향을 줄 수 있다. |
| phone → 한글 / emergency 문자 대응 | `resources/languages/ko-KR/dictionaries/phones.tsv`, `graphemes.tsv` | 음절 조립 알고리즘은 `languages/ko-KR/pack.c`에도 있으므로 모든 문제를 사전 한 줄로 고칠 수 있는 것은 아니다. |
| 제목 등 영어 문맥 라벨 | `resources/languages/en-US/rules/context.tsv` | 발음 사전 변경과 독립된 문맥 정책. |
| 코드·태그·경로 보존 | `resources/common/protected.tsv` | 보존 on/off와 opaque 태그 목록. |
| 새 시스템 사전 파일 등록 | `resources/manifest.ttfm` | 파일 경로 추가. Customer override 파일을 직접 로드하면 manifest 변경은 불필요하다. |

발음 사전은 네 열의 UTF-8 TSV다. 고객별 변경은 시스템 파일을 고치는 대신 별도
TSV를 `--customer` 또는 `ttf_resource_load(..., TTF_LAYER_CUSTOMER)`로 로드할 수
있다. 우선순위는 User > Customer > Domain > System이다. `AfterEffects`와
`After Effects`를 같은 정책으로 바꿀 때에는 두 키를 모두 검토한다.

## 6. 기준값 수정 시 남길 근거

1. 입력이 자연스러운 실제 문장인지, 오입력·경계 검증용인지 먼저 표시한다.
2. 기존 입력·기대값, 수정된 입력·기대값, 이유, 출처, 영향받는 리소스 키를 남긴다.
3. `reference-policy`, `provisional-pronunciation`, `preservation-contract`,
   `needs-review` 중 분류를 정하고 독립 검토 대기 상태를 유지한다.
4. 사람이 승인한 경우에만 검토자·날짜·근거와 함께 승인 사실을 별도로 기록한다.
5. 회귀 통과율과 언어 품질 검토 완료율을 함께 표시하되 합쳐서 “정확도”라 부르지 않는다.
6. 역사적 보고서는 당시 기준값의 기록으로 유지한다. 잘못된 기준값까지 영구히
   정답 취급하지 말고 현재 활성 코퍼스의 변경과 과거 보고서의 차이를 명시한다.

이 문서는 감사 결과이며, 그 자체로 기존 사전·native 코드·코퍼스를 변경하지 않는다.
