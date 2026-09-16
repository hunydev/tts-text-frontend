# 외부 자료와 이용 조건 기록

확인일: **2026-09-16**. 이 문서는 외부 코드, 데이터, 논문, 표기 규정을 구분하여
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

근거·형식·기술적 한계는 [NeMo/Sproat](research/nemo-sproat.md),
[한국어 코퍼스](research/korean-sources.md), [CMU/국립국어원](research/pronunciation-sources.md)에 있다.
출처 revision이 바뀌거나 실제 파일을 제품에 복사할 때에는 해당 파일의 이용 조건,
원저작권 표시, 변경 내용, 동봉 고지를 다시 기록한다. 기존 research-only 검토를
새로운 vendor 승인이나 사람의 정답 검수로 재사용하지 않는다.
