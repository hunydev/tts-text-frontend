# 외부 자료와 이용 조건 기록

최초 확인일: **2026-09-16**; 통화·영어 날짜 참고자료 추가 확인일: **2026-09-17**.
이 문서는 외부 코드, 데이터, 논문, 표기 규정을 구분하여
확인한 이용 조건과 현재 저장소의 사용 범위를 기록한다. 라이선스 허가서나 개별
배포에 대한 법률 판단은 아니다. 논문의 라이선스, 저장소 코드의 라이선스,
연결된 데이터의 이용 조건을 서로 대신 적용하지 않는다.

기계 판독용 목록은 [benchmark/manifests/sources.json](../benchmark/manifests/sources.json)이다.
목록은 **설명용 출처 카탈로그**다. 다운로드 지시, 접근 권한 확인, 이용 허가,
제품 리소스 manifest의 역할을 하지 않는다. 모든 항목의 자동 다운로드는 꺼져 있다.

## 현재 저장소에 포함한 범위

- 독립적으로 작성한 importer, 합성 fixture 테스트, 출처·설계 문서를 관리한다.
- 아래 upstream 코드, 대량 코퍼스, 논문 PDF, 음성, 모델, CMU 사전 전체는 vendoring하지 않았다.
- NeMo·Kspon·Corea 코드와 메타데이터를 열람했으며 해당 엔진을 제품 dependency로 넣지 않았다.
- Sproat 데이터의 제한된 표본과 CMU 전체 원본은 외부 임시 경로에서 조사했다.
  KsponSpeech/CoreaSpeech 코퍼스는 확보하거나 평가하지 않았다.
- 외부 원본·생성 corpus·reference·평가 상세 결과에는 원본 조건이 계속 적용될 수 있다.
  `benchmark/generated/` 또는 외부 로컬 경로에 두며 Git에 올리지 않는다.
- 평가 범위는 출력 텍스트와 발음 표현이다. 음성 합성·청취 평가는 포함하지 않는다.

## 코드

| 자료 / 고정 revision | 확인한 조건과 상업 이용 | 재배포·출처표시·현재 처리 |
| --- | --- | --- |
| [NVIDIA NeMo-text-processing](https://github.com/NVIDIA/NeMo-text-processing/tree/acacc21b1cb7916b10558855bc4f85957a0b2fde) — `acacc21b1cb7916b10558855bc4f85957a0b2fde` | [LICENSE](https://github.com/NVIDIA/NeMo-text-processing/blob/acacc21b1cb7916b10558855bc4f85957a0b2fde/LICENSE)는 Apache-2.0. 검토한 Python grammar/export 코드에 같은 헤더 존재. 조건 아래 상업 이용 허용. | 라이선스 사본, 적용되는 저작권·기여 표시, 변경 표시, 존재하는 NOTICE 조건 유지. 별도 NOTICE 파일은 확인 revision에 없었음. 코드·FAR 복사 없음. 파일별 데이터 조건은 아래에서 구분. |
| [Sproat split 저장소](https://github.com/rwsproat/text-normalization-data/tree/ab18388a34a9da78c77b7fa4b49e6d631ea348eb) — `ab18388a34a9da78c77b7fa4b49e6d631ea348eb` | 추적 파일 README.md와 split.sh에 별도 LICENSE 없음. 코드의 상업 이용·재배포 허가 **미확인**. | 데이터의 CC 표기를 코드로 확대하지 않음. split 정의만 참고하고 split.sh를 복사·실행하지 않음. |
| [Kspon preprocessing](https://github.com/sooftware/ksponspeech/tree/54fe0cebd890e519d424ae9d339a57040b399108) — `54fe0cebd890e519d424ae9d339a57040b399108` | [MIT LICENSE](https://github.com/sooftware/ksponspeech/blob/54fe0cebd890e519d424ae9d339a57040b399108/LICENSE), copyright 2020 Soohwan Kim. 조건 아래 상업 이용 허용. | 실질적 복사 시 저작권·허가 고지 보존. 코드 복사 없음. MIT는 AI Hub 데이터에 대한 허가가 아님. |
| [CoreaSpeech / N2gk+](https://github.com/CoreaSpeech/CoreaSpeech/tree/424229a8affea08848e2e647c0d3e471b3ef9d39) — `424229a8affea08848e2e647c0d3e471b3ef9d39` | [README](https://github.com/CoreaSpeech/CoreaSpeech/blob/424229a8affea08848e2e647c0d3e471b3ef9d39/README.md#license)는 코드 MIT를 주장하지만 해당 revision에 standalone LICENSE 없음. | 실제 vendoring 전 저작권·허가 고지와 dependencies 확인 필요. 코드·테이블·모델 복사나 실행 없음. 데이터 권한은 별개. |

Apache의 배포·변경·NOTICE·상표 조건은 [공식 Apache-2.0 원문](https://www.apache.org/licenses/LICENSE-2.0)을
기준으로 한다. 이 기록은 NeMo의 모든 dependency까지 조사했다는 뜻이 아니다.

## 데이터

### Sproat/Jaitly TN 배포

출처는 저자 저장소에서 연결한
[Kaggle 배포](https://www.kaggle.com/datasets/richardwilliamsproat/text-normalization-for-english-russian-and-polish)다.
확인 식별자는 dataset **869240**, version **1**, 배포일 **2020-09-11**이다.

배포 [LICENSE](https://www.kaggle.com/api/v1/datasets/download/richardwilliamsproat/text-normalization-for-english-russian-and-polish/LICENSE)는
CC BY-SA 4.0 제목을 담고 있으나 **7,668바이트에서 본문이 잘린다**. Kaggle metadata는
**Unknown**으로 표시한다. 이 차이를 해결되었다고 기록하지 않는다. 정식
[CC BY-SA 4.0 조건](https://creativecommons.org/licenses/by-sa/4.0/)은 상업적 공유·변형도
허용하되 출처·라이선스·변경 표시와 변형물의 동일조건 공유를 요구한다. 이는 관측한
라이선스 문구의 의미 설명이며, 모순된 배포 evidence를 대신하는 새로운 허가가 아니다.

| 근거 | SHA-256 |
| --- | --- |
| 배포 LICENSE | `fd8835cb194d0aa572b709835dfbf7b5dc336a37494e0889137b142b564d0db7` |
| 배포 [README](https://www.kaggle.com/api/v1/datasets/download/richardwilliamsproat/text-normalization-for-english-russian-and-polish/README) | `11fd3fb0ac149f332c84df031b4fb9fe1213afdf9b9134e956ea32c75b037128` |
| 확보한 shard 58 prefix | `072964db038fbf9091658de7c8d95b0f02689e22c047173a0b3f70899d60c567` |

현재 사용은 외부 경로의 제한된 텍스트 benchmark 표본이다. 코퍼스나 파생 어휘표를
제품에 섞지 않는다. `benchmark-only`는 프로젝트의 사용 범위이며 원본이 비상업 전용이라는
뜻은 아니다. 표본은 training shard의 prefix이므로 논문 held-out test로 부르지 않는다.

### NeMo 저장소의 데이터는 코드 라이선스와 구분

[data/README.md](https://github.com/NVIDIA/NeMo-text-processing/blob/acacc21b1cb7916b10558855bc4f85957a0b2fde/data/README.md)는
`EngConf.txt`, `GoogleTN.json`, `LibriTTS.json` 평가 자료를 CC BY 4.0으로 명시하고
후자의 두 원본 출처를 연결한다. Apache-2.0 코드 조건으로 덮어쓰지 않는다. 특히
`GoogleTN.json`은 위 Sproat 데이터의 파생물이라는 설명이 있어, 원본의 CC BY-SA 표기와
어떻게 연결되는지 확인 전 재배포 승인을 추정하지 않는다. LibriTTS 원본 권리 사슬도
이번 조사에서 별도로 확정하지 않았다. 이 평가 데이터는 제품에 포함하지 않는다.

또한 [roman/README.md](https://github.com/NVIDIA/NeMo-text-processing/blob/acacc21b1cb7916b10558855bc4f85957a0b2fde/nemo_text_processing/text_normalization/en/data/roman/README.md)는
Mark Kantrowitz의 `female.tsv`, `male.tsv` 원저작권과 별도 조건을 적는다. 모든 목적의
사용을 허용하되 출판물에서 credit을 요구하고, 재배포에는 해당 README 동봉을 요구한다.
이 목록도 복사하지 않았다. 이 두 예는 파일마다 권리 근거를 기록해야 하는 이유다.

### KsponSpeech: AI Hub 약정

공식 데이터는 [AI Hub dataset 123](https://www.aihub.or.kr/aihubdata/data/view.do?currMenu=115&topMenu=100&aihubDataSe=data&dataSetSn=123)이다.
이번에는 코퍼스를 받지 않았으므로 release 또는 corpus hash가 없다. preprocessing 코드
commit을 데이터 버전으로 기록하지 않는다.

[AI Hub 이용 정책](https://aihub.or.kr/intrcn/guid/usagepolicy.do?currMenu=151&topMenu=105)은
상업·비상업 연구개발을 언급하면서도 학습 모델 훈련 목적, 제3자 제공 제한, 해외 이용·
반출에 관한 별도 약정, NIA 출처표시, 판매·상업 활용의 협의 조항을 함께 둔다.
데이터 페이지의 신청 대상 제한과 실제 이용자의 약정도 확인해야 한다. 따라서
**MIT 데이터**, **무조건 허용된 상업 benchmark**로 분류하지 않는다. 텍스트만 추출하는
평가 목적도 별도로 허용 범위를 확인해야 한다. 자동 취득·재배포·제품 vendoring 없음.

### CoreaSpeech: 데이터 카드와 NC 표기의 충돌

공식 프로젝트가 연결한 다음 배포만 출처로 기록했다.

| 데이터 | 확인한 카드 revision |
| --- | --- |
| [KoreanUniversalTestset](https://huggingface.co/datasets/aanonyyy/M6A5P1Q7/blob/9829037ec950e08db7ef499656cae69c7157bd6b/README.md) | `9829037ec950e08db7ef499656cae69c7157bd6b` |
| [CoreaSpeech training](https://huggingface.co/datasets/aanonyyy/F5I9N7A1/blob/950d6a50b2b3e338d141313d11440fe47b037745/README.md) | `950d6a50b2b3e338d141313d11440fe47b037745` |

README와 논문 checklist는 데이터·Testset·checkpoints를 CC BY-NC 4.0으로 설명하지만,
공식 데이터 카드는 연구 전용이며 수정·재배포를 금지하는 더 엄격한 조건을 적는다.
정식 [CC BY-NC 4.0](https://creativecommons.org/licenses/by-nc/4.0/legalcode.en)의 비상업적
공유·변형 허용과 일치하지 않으므로 **충돌 미해결**로 남긴다. 상업적 사용 허가는
확인되지 않았다. 더 허용적인 문구만 선택해 이용하지 않는다.

코퍼스·음성·checkpoints를 확보하지 않았고 제품 포함이나 재배포도 하지 않는다.
권한이 별도로 확인된 로컬 텍스트 export만 명시적으로 처리할 수 있다.
`--allow-noncommercial` 및 `--acknowledge-data-rights`는 실행 범위와 호출자의 확인을
기록할 뿐, 권리자의 허가를 취득·검증하지 않는다. 회사 내부 benchmark라고 해서
자동으로 비상업 목적이 되는 것도 아니다. 승인된 경우에도 creator/source/license
표시, 논문 인용, 변경 표시를 유지해야 하며, 이것이 충돌 자체를 해결하지는 않는다.

### CMU Pronouncing Dictionary

고정 revision은 `74790861f652b15e4ac49015a90074ad62a27690`이다.
[README](https://github.com/cmusphinx/cmudict/blob/74790861f652b15e4ac49015a90074ad62a27690/README)는
상업 목적 사용을 명시적으로 허용하고,
[LICENSE](https://github.com/cmusphinx/cmudict/blob/74790861f652b15e4ac49015a90074ad62a27690/LICENSE)는
source/binary와 수정 여부를 구분하지 않고 재배포를 허용하되 저작권·조건·면책 보존을
요구하는 BSD 2-clause 형식이다. 상업 사용 허용과 고지 의무를 함께 기록한다.

새 importer는 선택적 TSV/TTFB export에 원본 `COPYING`, CMU 출처와 revision을 적은
`NOTICE.txt`, 입력·출력 hash 및 변형 선택 정책 manifest를 동봉한다. binary 배포 시
이 고지를 함께 전달해야 한다. 기본 runtime에 자동 설치하지 않으며 CMU 대량 데이터는
Git에 포함하지 않는다. 사전·importer 출처와 SHA-256는
[발음 자료 조사](research/pronunciation-sources.md)에 있다. 사전 발음은 검수된 한국어
TTS 정답으로 자동 승격하지 않는다.

## 논문

| 논문 | 확인한 버전 / 이용 조건 |
| --- | --- |
| [Sproat/Jaitly](https://arxiv.org/abs/1611.00068v2) | arXiv:1611.00068v2, 2017-01-24. 기술 설명과 split 해석을 인용. 이번 조사에서 논문 자체의 재배포 라이선스는 별도로 확정하지 않았고 PDF를 vendoring하지 않음. 데이터 허가와 구분. |
| [KsponSpeech](https://www.mdpi.com/2076-3417/10/19/6936) | DOI `10.3390/app10196936`, Applied Sciences 2020, 10(19), 6936. 논문 최종 페이지에 CC BY 4.0 표시. 논문 재사용에는 저자·출처·라이선스·변경 표시 조건 적용. AI Hub corpus 권리는 별개. |
| [CoreaSpeech](https://proceedings.neurips.cc/paper_files/paper/2025/file/5b221bc040a287a255a0f311598aec61-Paper-Datasets_and_Benchmarks_Track.pdf) | NeurIPS 2025 Datasets and Benchmarks Track. 논문의 데이터 라이선스 주장을 데이터 카드와 대조. 논문 PDF 자체의 재배포 라이선스는 이번 조사에서 별도 확정하지 않았고 vendoring하지 않음. |

## 국립국어원 표기 규정과 용례 데이터

공식 [표기의 원칙](https://www.korean.go.kr/front/page/pageView.do?mn_id=97&page_id=P000104),
[IPA 대조표](https://www.korean.go.kr/front/page/pageView.do?mn_id=97&page_id=P000105),
[영어 세칙](https://www.korean.go.kr/front/page/pageView.do?mn_id=97&page_id=P000124),
[인명·지명 원칙](https://www.korean.go.kr/front/page/pageView.do?page_id=P000145)을
2026-09-16 열람했다. 이 날짜는 고시 개정일이나 immutable version을 뜻하지 않는다.

[저작권 정책](https://www.korean.go.kr/front/nuri/pageView.do?mkn=3&page_id=P000189)에 따라
개별 자료의 공공누리 유형과 표시 조건을 확인해야 한다. 예를 들어 제1유형은 출처표시
조건 아래 상업 이용·변경을 허용하지만 다른 유형에는 추가 제한이 있다. 규정의 공개
열람으로 외래어 용례 DB, 사전 전체, API 응답의 상업 재배포 권한을 추정하지 않는다.
그러한 대량 자료 권한은 이번 조사에서 **미확인**이다. 규정과 독자 구현 설계만
연결했으며 용례 DB를 추출·동봉하지 않았다.

## 상세 조사와 변경 절차

### SIX 통화 코드 사실 확인 자료 (v0.12)

2026-09-17에 SIX의 [금융 데이터 표준 안내](https://www.six-group.com/en/products-services/financial-information/market-reference-data/data-standards.html)와
공식 [Current Currency & Funds List One XML](https://www.six-group.com/dam/download/financial-information/data-center/iso-currrency/lists/list-one.xml)을
직접 확인했다. USD, EUR, AUD, CAD, GBP, CHF, NOK, JPY, KRW의 코드·명칭 연결과
보조단위 자릿수만 구현 정책의 사실 확인 근거로 사용했다.

| 항목 | 기록 |
| --- | --- |
| Source | SIX, ISO 4217 maintenance agency, List One XML |
| Version | XML의 `Pblshd="2026-01-01"`; 조회일은 2026-09-17 |
| SHA-256 | `838dfb991648cf36df939edd5fe3811737962b75a32252847d239cedd1e291c9` |
| License | 이번 사실 확인에서 XML 전체의 별도 재배포 라이선스를 확정하지 않음 |
| Commercial use / redistribution | 공식 파일 전체를 상업 데이터 리소스로 재배포할 권한을 추정하지 않음 |
| Attribution | 위 원문 URL·게시일·조회일·hash를 출처 추적용으로 기록. 이것을 원본 재배포 허가나 고지 요건의 전부로 간주하지 않음 |
| Vendoring | XML·웹페이지·ISO 표준 문서 자체를 저장소나 제품에 포함하지 않음 |

제품에는 기존 TSV 형식으로 직접 작성한 소규모 통화 인식·읽기 정책이 들어간다.
전체 ISO 목록을 수입한 데이터베이스가 아니며, 한국어 읽기와 scale 별칭은 프로젝트
정책이다. 보조단위 자릿수는 TTS 읽기를 고르는 데 사용하며 결제 유효성이나 현금
반올림을 판단하지 않는다. 통화명과 영문 보조단위명의 추가 근거는
[통화 리소스 문서](currency-resources.md)에 기록했다. 공개 열람 가능성이나 코드
명칭의 사실 확인으로 다른 금융 데이터셋·benchmark의 상업 이용, 변경 또는 재배포
라이선스를 일괄 승인하지 않는다.

### Cambridge 영어 날짜 읽기 참고 (v0.13)

2026-09-17에 Cambridge Dictionary의
[English Grammar Today: Dates](https://dictionary.cambridge.org/grammar/british-grammar/dates)와
[How to say years](https://dictionaryblog.cambridge.org/2017/10/04/1066-and-all-that-how-to-say-years/)
(게시일 2017-10-04)를 직접 열람했다. 전자는 확인 시점의 웹페이지이며 고정 revision은
없다. 서수 일자, 영미 날짜 순서, 허용되는 연도 읽기 차이의 사실 확인용으로만 사용했다.

두 문서의 저작권을 별도 오픈 라이선스로 취급하지 않는다. 원문·표·코드의 상업적
복사·재배포 허가는 이번 조사에서 확인하지 않았으며 저장소에 vendoring하지 않는다.
제품의 alias·읽기 정책과 테스트 문장은 독립 작성했다. 출처 URL·조회일은 위와
[날짜 가이드](english-dates.md)에 기록했으며, 이것이 원문 재사용 허가나 모든
attribution 요건을 대신하지 않는다.

### 영어 약어·통화명 사실 확인 (v0.14)

2026-09-17 확인한 publisher 원문은 [Cambridge Dr](https://dictionary.cambridge.org/dictionary/english/dr),
[Cambridge pound](https://dictionary.cambridge.org/dictionary/english/pound),
[Dictionary.com Mr./Mrs.](https://www.dictionary.com/articles/mr-mrs)다. 고정 commit 없는
조회 시점 웹 문서이며 Dr.의 확장, Mr./Mrs.의 읽기, pound의 통화/무게 다의성만 참고했다.
웹 페이지/정의/예문/발음 표의 open license, 상업적 복사 및 재배포 허가는 확인하지
않았다. 원문 data/resource는 vendoring하지 않았고 위 URL과 조회일을 사실 근거로
기록한다. 문서 attribution 기록이 원본 사용 허가를 대신하지 않는다. 제품의 alias,
문맥 조건, 한국어 표기 및 synthetic test 문장은 프로젝트가 직접 작성했다. 한국어
title 표기는 별도 검수 대기 상태로 관리한다.

### 출처 변경 절차

근거·형식·기술적 한계는 [NeMo/Sproat](research/nemo-sproat.md),
[한국어 코퍼스](research/korean-sources.md), [CMU/국립국어원](research/pronunciation-sources.md)에 있다.
출처 revision이 바뀌거나 실제 파일을 제품에 복사할 때에는 해당 파일의 이용 조건,
원저작권 표시, 변경 내용, 동봉 고지를 다시 기록한다. 기존 research-only 검토를
새로운 vendor 승인이나 사람의 정답 검수로 재사용하지 않는다.

### v0.15 Unicode 숫자 grouping / 한국어 서수 조사

조회일 2026-09-17. 다음 자료는 사실·설계 참고로 사용했으며 원문 표, 데이터셋,
코드, 예문을 제품 resource로 복사하지 않았다. 프로젝트 테스트는 독립 작성했다.

| Source / revision | License and permitted use considered | Redistribution / attribution / vendoring |
|---|---|---|
| [Unicode Standard 17.0, chapter 6](https://www.unicode.org/versions/Unicode17.0.0/core-spec/chapter-6/) | [Unicode Terms of Use](https://www.unicode.org/copyright.html) distinguishes specification text from licensed data/software. Internal business reference is permitted under its terms; this is not a claim of unrestricted specification redistribution. | No specification text/table vendored. Public redistribution would require the applicable version-specific permission and notices; this repository records URL/version/date. |
| [UTS #35 Numbers, observed version 48.2](https://www.unicode.org/reports/tr35/tr35-numbers.html) | Technical report reference under Unicode terms. No CLDR data/code import. Software/data Unicode License v3 must not be automatically applied to all report text. | No report/data vendoring. URL/version/date attribution; actual future data import requires file-specific license and copyright notice. |
| [NIKL 329611](https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?mn_id=216&pageIndex=1&qna_seq=329611), [325931](https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?mn_id=27&pageIndex=1&qna_seq=325931), [306013](https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?mn_id=216&pageIndex=1&qna_seq=306013); web answers, no fixed commit | Facts about ordinal/count readings only. A blanket commercial reuse/open license for these entire pages was not established in this review. | No Q&A text/examples vendored; commercial copying/redistribution requires source-specific terms review. Links/date recorded; this is not institutional endorsement of project outputs. |

The implementation is original C and original policy TSV. The large-number ordinal
choice and finite context cues are project policy, separately documented and not
presented as uniquely prescribed answers. No new external benchmark corpus was committed.

### v0.18 telephone syntax references

Reviewed 2026-09-17. These sources informed numeric limits and fictional test-number
selection only. No standard text, numbering table, allocated-number dataset or code
was copied into runtime resources; tests and C implementation are original.

| Source / version | License / commercial-use assessment | Redistribution / attribution / vendoring |
|---|---|---|
| [ITU-T E.164 (02/2026), edition 7.0](https://www.itu.int/rec/T-REC-E.164-202602-I) | The publication states © ITU 2026 and requires prior written permission for reproduction. This review does not establish a commercial redistribution license for the publication or certify implementation conformance. | No PDF/text/table vendored. Source/version/date are attributed here. Any future copying requires permission review; the default 15-digit bound is project recognition policy informed by the geographic-number limit. |
| [NANPA, 555 Line Numbers](https://nanpa.com/numbering/555-line-numbers), accessed 2026-09-17, no fixed commit | Public reference on the reserved fictional 555-0100–555-0199 range. No open license for commercial copying of the entire webpage was established. | No webpage/data copied. Independently authored examples use the documented fictional range; source/date attributed. Other synthetic phone-shaped fixtures test parsing and do not claim real allocation or reachability. |

The frontend does not validate subscriber reachability or country allocations.
Seven-digit minimums, extension limits, and spoken labels are editable project
choices, not claims that E.164 defines TTS reading or every supported local format.

## 2026-09-27 조사표 참고 추가

국립국어원 온라인가나다의 [랑/이랑](https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?mn_id=216&pageIndex=1&qna_seq=318773),
[나마/이나마](https://www.korean.go.kr/front/onlineQna/onlineQnaView.do?mn_id=216&pageIndex=1&qna_seq=320391),
한국어기초사전의 [이라도](https://krdict.korean.go.kr/eng/dicSearch/SearchView?ParaWordNo=78504&nation=eng),
[라도](https://krdict.korean.go.kr/chn/dicSearch/SearchView?ParaWordNo=78508),
[든](https://krdict.korean.go.kr/eng/dicSearch/SearchView?ParaWordNo=86261&nation=eng)을
문법 사실 참고로 조회했다. 고정 commit 없는 웹 자료이며 식별자는 각 URL의 항목 번호와
조회일이다. 웹 원문/예문 전체에 대한 상업적 복제·재배포 허가나 open license는 확인하지
않았으므로 vendoring하지 않았다. 링크·조회일을 출처로 남기고, 제품 TSV 및 72개 시험 문장은
직접 작성했다. 외부 한국어 코퍼스나 사전 내용을 제품 resource로 가져온 작업이 아니다.
정책과 독립 검수 상태는 [조사 가이드](particle-resources.md)에 기록한다.


## 2026-09-27 월·요일 별칭 참고 추가

| Source / version | License / commercial-use assessment | Redistribution / attribution / vendoring |
|---|---|---|
| [RFC 5322 §3.3](https://www.rfc-editor.org/rfc/rfc5322#section-3.3), October 2008; 조회 2026-09-27 | 문서의 Copyright Statement는 IETF Trust (2008) 및 BCP 78의 권리·제한을 명시한다. 상업 제품에 원문 전체를 복제할 무제한 허가로 해석하지 않는다. 이번에는 통상적인 영문 월/요일 약자를 사실 참고로만 확인했다. | RFC 원문·ABNF·예문을 vendoring하지 않았다. 직접 작성한 TSV와 합성 시험만 포함하며 source/version을 여기 기록한다. 향후 RFC 원문을 재배포하려면 해당 문서에 적용되는 BCP 78 고지·조건을 별도로 확인해야 한다. |

프로젝트의 `date(label)` TN 문법은 RFC 이메일 날짜 구현이 아니다. 날짜와 요일의
불일치를 자동 수정하지 않는 자체 읽기 정책이며 [가이드](calendar-alias-resources.md)에 명시했다.


## CJK 한국어 표기 리소스 (2026-09-27)

| Source / version | License / commercial use | Redistribution / attribution / vendoring |
|---|---|---|
| [Unicode Unihan](https://www.unicode.org/reports/tr38/), 제공된 subset의 version header **18.0.0** | [Unicode License v3](https://www.unicode.org/license.txt). 사용·변경·상업적 배포를 고지 조건 하에 허용한다. | copyright/permission notice를 데이터 또는 동봉 문서에 유지. `resources/languages/ko-KR/cjk/UNICODE-LICENSE.txt` 동봉. 승인된 부분 추출본 `data/cjk/unihan-k-subset.txt` 및 생성 독음 TSV만 저장소에 포함하며 원본 zip은 반입하지 않는다. |
| 외래어 표기법, 문화체육관광부 고시; 제2장 표 4 및 일본어 표기 규정, 조회 2026-09-27 | [저작권법 제7조 제2호](https://www.law.go.kr/lsLinkCommonInfo.do?lsJoLnkSeq=1029423769)의 국가 고시 등 비보호 저작물 근거를 기록한다. 이 기록은 법률 자문이나 웹사이트 전체 자료에 대한 허가가 아니다. | [국립국어원 어문 규범](https://www.korean.go.kr/kornorms/main/main.do)의 고시 출처와 규정 위치를 생성 TSV 머리말에 유지한다. 자체 작성 가나 대응표/규칙을 상업용 리소스로 사용하며 웹 설명·사전·예문집은 수집하지 않는다. |

| [국립국어원 표준국어대사전](https://stdict.korean.go.kr), 2026-09-06 내려받은 JSON | [CC BY-SA 2.0 KR](https://creativecommons.org/licenses/by-sa/2.0/kr/) (사전 저작권 정책; 다중 매체와 출전 있는 용례 제외). 상업적 이용 허용, 저작자 표시, 변경물 같은 라이선스. | 한자어 표제어에서 글자 독음만 뽑은 `resources/languages/ko-KR/cjk/hanja_readings_stdict.tsv`와 고지 `STDICT-LICENSE.txt`만 포함한다. 그 파일만 CC BY-SA 2.0 KR이며 원본·뜻풀이·용례는 반입하지 않는다 (2026-10-04, 프로젝트 소유자 결정). |

Unihan version은 사용자가 준비한 subset의 header에서 고정했고 파일 SHA256은
`benchmark/manifests/sources.json`에 기록한다. 원본 zip을 다시 다운로드하여 대조했다는
의미는 아니다. 두 출처 모두 자동 다운로드를 끈다. Unihan 원본 전체 및 제3자 한자 사전은
이번 작업에 추가하지 않는다. 생성 결과·provenance는 고정 소스에서 LF로 재현한다.

## 점 표기 영문 약어 (2026-09-27)

`abbreviations-dotted.tsv`는 저장소에 이미 있던 `abbreviation_bare`의
vol/ltd/vs/etc/jr 읽기를 명시적인 점 표기에 재사용한 자체 정책이다. 외부 사전·코드·
문장을 반입하지 않았다. 30건 corpus와 경계/리소스 검사도 직접 작성했으며,
통과 결과를 독립 언어 감수나 외부 benchmark 전체 품질의 보증으로 표시하지 않는다.

### v0.21 전각 영문자 최종 문자 읽기 (2026-09-27)

- 참고: [Unicode Halfwidth and Fullwidth Forms chart](https://www.unicode.org/charts/PDF/UFF00.pdf),
  열람본 표기 Unicode **18.0**, 확인일 2026-09-27. U+FF21–FF3A / U+FF41–FF5A 문자 대응을 확인했다.
- 라이선스/재배포: 명세 chart는 Unicode Terms of Use의 출판물 조건을 따른다. 위 Unihan의
  데이터 라이선스를 chart 전체에 적용하지 않는다. PDF·그림·설명·폰트·외부 데이터셋은
  vendoring하지 않았다. 문서에 출처/버전/열람일만 기록한다.
- 제품 리소스: KO는 기존 프로젝트 phone_letter 읽기를 재사용하고 EN은 대응 ASCII를
  쓰는 자체 작성 정책104행. 계약30건도 자체 작성. 외부 발음 데이터 반입 없음;
  저작권자의 제품 리소스이며 상업 사용·재배포 허가는 저장소 LICENSE(별도 서면 허가 필요)를 따른다.
  독립 언어 검수/음성 품질 승인은 아니다.

## 사용자 제공 한국어 TN release (2026-09-28)

- Source: 업로드된 `summary.json`은 `159.숫자가 포함된 패턴 발화 데이터 / Training /
  라벨링데이터`의 스크립트 텍스트라고 기술한다. 21파일, 142,255문장. 실제 dataset ID나
  공개 다운로드 URL을 임의로 추정하지 않는다.
- Version: `benchmark/manifests/sources.json`에 summary SHA256, 로컬 import manifest에
  개별21파일 SHA256과 원본 순번을 기록한다. 업스트림 배포판과의 동일성은 검증하지 않았다.
- License / commercial use / redistribution / attribution requirements: 업로드에 이용허락
  문서가 포함되지 않아 **미확인**이다. 공개 데이터셋이거나 업로드되었다는 이유만으로
  상업 제품 리소스 사용 또는 재배포 허가를 추정하지 않는다. 출처 설명은 참조 기록에 보존한다.
- Vendoring: 원본·가공 문장·SQLite 사례 보고서는 ignored `benchmark/generated/ko-release/`
  에만 둔다. Git에는 자체 importer/evaluator/web UI와 자체 작성 합성 fixture만 관리한다.
  자동 다운로드, 제품 TN/발음 사전으로의 자동 반입은 없다.
- Evaluation: 독립 gold가 아닌 제공 참조와의 텍스트 일치도. 원본의 Training/Validation
  중복 제외 설명도 보존하고, 독립 held-out 점수로 부르지 않는다.

### 공통 발음 보강의 출처 (2026-09-28)

`lexicons/sources/usage-v22.json`은 공식 기관·제품 웹페이지 10개와 프로젝트 편집
정책의 URL·확인일을 기록한다. 후보 빈도는 위 로컬 자료에서 검토했지만 외부
문장/사전을 제품 리소스로 일괄 복사하지 않았다. 직접 고른 23개 단어의 읽기를
직접 작성했고, 그중 10개는 공식 한국어 표기와 대조했다. 13개는 검토 대기다.
웹페이지 원문·이미지·코드·테이블은 vendoring하지 않으며, 해당 사이트 전체의
상업 사용/재배포 허가를 추정하지 않는다. 원본 관리 파일의 evidence와 notes,
[보강 가이드](lexicon-coverage-v22.ko.md)에 각 선택의 범위를 남긴다.
