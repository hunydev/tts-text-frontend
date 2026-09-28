# TTF Lab — 웹 테스트 도구 (v0.9)

실제 C 공유 라이브러리의 출력 텍스트와 메타데이터를 검증하는 개발 도구입니다.
음성 합성, 녹음, 청취 평가는 수행하지 않습니다. 소스는 같은 저장소의
`tools/web/`와 `tests/web/`에서 관리합니다.

## 실행

Python 3.10 이상과 빌드된 native 공유 라이브러리가 필요합니다. 웹 서버와 UI는
Python 표준 라이브러리와 정적 HTML/CSS/JavaScript만 사용합니다. Node/npm은
선택적인 브라우저 자동화 테스트에만 필요합니다.

```sh
cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-web --parallel 2
python3 tools/web/server.py --host 0.0.0.0 --port 8000 \
  --library build-web/libtts_text_frontend.so --resources resources
```

로컬 접속은 `http://127.0.0.1:8000/`입니다. 기본 host는 `127.0.0.1`이며 외부
VM 포트 연결이 필요할 때 위와 같이 `0.0.0.0`을 명시합니다.
현재 VM은 `ttf-workbench.service` 사용자 systemd 서비스로 8000 포트에서 실행합니다.
`tools/web/ttf-workbench.service.example`의 경로를 자신의 checkout에 맞게 수정하면
같은 구성을 재현할 수 있습니다.

```sh
systemctl --user status ttf-workbench.service
systemctl --user restart ttf-workbench.service
systemctl --user stop ttf-workbench.service
```

exe.dev는 추가 포트 3000~9999의 HTTPS 프록시를 문서화하고 있습니다. 이 VM의
프록시 주소는 `https://tts-text-frontend.exe.xyz:8000/`이며 VM 접근 권한으로
로그인합니다. 공개 공유 설정은 변경하지 않았습니다.
[exe.dev 프록시 문서](https://exe.dev/docs/proxy.md)를 참고하세요.

## 화면별 검증

| 화면 | 확인할 수 있는 내용 |
| --- | --- |
| 정규화 | 입력/출력, 선택적인 기대 문자열과 정확히 일치하는지, 원문 토큰, Semantic IR |
| 스트리밍 | 직접 지정한 write 크기, 실제 입력 byte 범위, native 출력 콜백 순서, write/flush 단계 |
| 청크 검증 | 같은 설정의 batch와 고정/seed 기반 무작위/직접 지정 분할 비교: 텍스트·span·콜백 경계 |
| 코퍼스 검증 | 고정 기대 문자열·span assertions·청크 일치성을 함께 검사하고 실패 항목 펼쳐 보기 |

분석 표의 행을 선택하면 `source/context/render` 언어, semantic type/value/secondary/unit,
규칙 계층, 사전 계층, fallback 경로, context trace를 확인할 수 있습니다.
위치는 UTF-8 byte 기준입니다. 원문 토큰 표는 public tokenizer 결과이며
정규화 span은 보호 구간 병합과 semantic 분류를 반영하므로 행 수가 다를 수 있습니다.

JSON 저장은 입력·설정·이벤트·결과를 다운로드합니다. 코퍼스 결과도 저장할 수
있습니다. 입력 내용은 서버 접근 로그에 남기지 않으며 세션/사전 변경은 영구 저장하지
않습니다. 다운로드 파일에는 입력 텍스트가 포함됩니다.

## 유용한 실험 순서

1. **같은 1st, 다른 문맥** 예제를 실행하고 ORDINAL 행의 context 언어를 비교합니다.
2. **사용자 발음사전 우선순위**를 실행하고 pronunciation layer가 `user`인지 확인합니다.
3. **금융 도메인 숫자 정책**, **고객 규칙 덮어쓰기**를 비교합니다.
4. **WiFi 시스템 사전 제외**에서 빈 출력이나 원문 방치 없이 다음 사전/G2P 경로로
   진행하는지 확인합니다. 시스템 lexicon 제외가 pronunciation dictionary까지 삭제하지는 않습니다.
5. 스트리밍에서 입력 간격을 30~50ms로 지정하고 종료 전에 나오는 콜백을 관찰합니다.
6. 같은 입력으로 청크 검증을 실행하고 모든 variant의 텍스트·span·경계 일치를 확인합니다.
7. 금융 범위·콘텐츠 고유명사 코퍼스를 실행하고 개별 기대/실제 출력과 오류를 펼쳐 봅니다.
8. **단어 사이에서 끊긴 복합 이름**에서 `After `와 `Effects`를 나누어 전달합니다.
   출력의 `After Effects` span이 하나로 묶이고 사전 경로가 `lexicon:system`인지 확인합니다.
9. **복합 이름의 사용자 발음 우선순위**를 실행하고 `lexicon:user`와 출력 변경을 확인합니다.

### 원하는 위치에서 입력 나누기

`문장 / 구절 · 입력 청크 설정`의 **직접 지정 write 크기**에 `6, 7, 1`처럼
UTF-8 byte 길이를 입력할 수 있습니다. 해당 길이들을 순서대로 사용하고 남은 입력은
기본 청크 크기로 보냅니다. 마지막 write는 입력 길이에서 잘리며 남는 크기 항목은
무시됩니다. 빈 설정은 기존 고정 크기 분할입니다.

입력 텍스트에 커서를 놓고 **입력 커서 위치에서 나누기**를 누르면 두 번의 write로
설정합니다. 예를 들어 `After |Effects를 사용합니다.`에서 첫 write는 6 bytes입니다.
서버는 이 분할로 실제 `ttf_stream_write`를 호출합니다. 청크 검증 모드에서는 기존
고정·무작위 분할에 `custom writes` 검사를 추가합니다.

출력 콜백 위의 **실제 입력 write 목록**을 펼치면 각 호출 직전의 범위와 입력 일부를
확인할 수 있습니다. UTF-8 문자 중간에서 잘린 청크는 hex로 표시합니다. 화면 목록은
최근 128개로 제한하고 전체 이벤트는 JSON 보고서에 남깁니다. 정규화된 출력은 정상
UTF-8이어야 합니다. write 크기는 최대 256개이며 지연은 실제 write 사이 횟수로 계산합니다.

작은 구절 목표는 활용 가능한 문맥을 줄여 읽기를 바꿀 수 있습니다. 청크 검증은
**같은 설정끼리** 비교하며, 모든 가능한 입력 분할의 수학적 증명은 아닙니다.
`문장 우선` 프리셋은 sentence split을 켜고 phrase split을 끄며 최대 길이 목표를
크게 설정합니다. 이것은 엄격한 한 문장당 한 콜백 계약을 추가하지 않습니다.
최종 flush는 미완성 문장도 반환하며 줄바꿈도 경계가 될 수 있습니다.

`의미 현지화`는 native render policy 노출이며 일반 문장 번역 기능이 아닙니다.
코퍼스 모드는 예문마다 고정된 언어/리소스를 사용하므로 화면의 임의 설정을 적용하지
않습니다. 모든 제공 코퍼스는 합성 개발 데이터이고 실제 고객 정확도를 뜻하지 않습니다.

## 전송 및 실행 구조

```mermaid
flowchart LR
    UI[Browser UI] -->|JSON request| HTTP[Python HTTP server]
    HTTP -->|validated request| Worker[Isolated worker process]
    Worker --> FFI[ctypes ABI v1 adapter]
    FFI --> Native[C engine / stream]
    Native -->|synchronous callback| FFI
    FFI -->|copy text and spans| Worker
    Worker -->|NDJSON events| HTTP
    HTTP -->|flushed events| UI
```

각 요청은 별도 process와 engine을 사용합니다. 콜백 포인터의 내용을 콜백 안에서
복사하고, 정상 완료/오류/클라이언트 중단 뒤 worker를 정리합니다. 고객 TSV는
임시 파일로 로드하고 삭제하며 저장소 리소스는 바꾸지 않습니다.

- `GET /api/catalog`: 예제, 코퍼스 목록, 기본 설정과 제한
- `GET /api/health`: 가용 여부와 checkout 버전
- `POST /api/run`: `normalize`, `stream`, `verify`
- `POST /api/corpus`: 등록된 코퍼스 ID

성공 응답은 `start`, `chunk`, `segment`, `case`, `result`, `end` NDJSON 이벤트 중
해당 모드의 이벤트를 순서대로 반환합니다. 완료에는 `end.ok=true`가 필요합니다.
실행 오류는 HTTP 200 뒤에도 `error` 이벤트로 올 수 있으며 성공 종료 이벤트는
나오지 않습니다. UI는 중단·오류·완료 이벤트가 없는 연결 종료를 성공으로 표시하지 않습니다.

worker 시간은 process 시작/네트워크 비용을 제외합니다. 스트리밍의 elapsed time은
설정한 입력 대기 시간을 포함하므로 native TTFB 벤치마크로 해석하면 안 됩니다.

## 제한과 검증

이 도구는 VM 접근 제어 뒤에서 사용하는 개발용 서버입니다. 자체 계정/인증이나
공개 멀티테넌트 서비스 운영 기능은 포함하지 않습니다. 실행 파일 및 리소스 경로는
서버 시작 시 고정되며 웹 요청이 임의 파일 경로나 명령을 선택할 수 없습니다.
요청은 256 KiB, 입력은 16 KiB UTF-8, 고객 TSV는 16 KiB, 사전은 100개, 동시 worker는
4개, 요청 실행은 45초로 제한합니다. 결과 크기는 32 MiB로 제한합니다.
다른 Origin의 JSON 요청을 거부하고 입력은 HTML로 실행하지 않습니다.

`web_workbench` CTest는 C 헤더의 sizeof/offsetof와 ctypes 레이아웃, CLI 출력 일치,
정책 변경, 요청 격리, 잘못된 요청, 코퍼스 평가, 종료 전 HTTP 콜백을 검증합니다.
이 HTTP/FFI 검사는 일반 GCC/Clang 빌드에서 실행합니다. Python에 sanitizer 런타임을
주입하지 않으므로 sanitizer 구성에서는 제외하며 native 검사는 별도로 실행합니다.

선택적 브라우저 검증은 Node 20 이상을 사용합니다 (CI: Node 22).

```sh
npm ci --prefix tests/web
npx --prefix tests/web playwright install chromium
node tests/web/browser.cjs http://127.0.0.1:8000
```

브라우저 게이트는 정규화, 상세 trace, 사용자 사전, 청크 검증, live callbacks,
JSON 다운로드, 중단, 원문 HTML 미실행, 코퍼스 및 모바일 가로 넘침을 확인합니다.


## 기준값의 검토 상태 (v0.9)

코퍼스 화면은 기대 출력의 개발 정책/임시 발음/원문 보존 계약/추가 검토 분류를
보여준다. 현재 703개 golden·JSONL 사례는 모두 독립 검토 대기이며, 사람의 승인
0건이다. 기대값 일치는 언어적으로 무결한 정답 인증을 의미하지 않는다.
기본 예제는 `1~2만 원 → 만 원에서 이만 원`이며 기호 `~`와 범위를 구별한다.
[리소스 수정 안내](resource-editing-guide.ko.md)에서 고객 TSV로 범위 연결어,
수사 정책과 발음사전을 바꾸고 검증하는 방법을 설명한다.


## 사전 관리 (v0.10)

상단 **사전 관리** 링크는 `/lexicon`으로 이동한다. 374항목의 발음·근거·검토
상태를 검색하고, 선택한 항목을 실제 문장으로 시험하거나 요청별 User 발음으로
수정 시험할 수 있다. 고객 TSV와 수정 제안 JSON은 다운로드되며 서버 원본을
덮어쓰지 않는다. 원고 검사에서는 실제 span의 사전 경로와 fallback 용어를
집계하고 JSON으로 내보낸다. UI의 숫자는 발음 정확도 점수가 아니다.

금융 46약어는 **금융 발음사전** 설정으로 켠다. 이 옵션은 `finance-vocabulary`
프로필이며 기존 **금융 도메인** 예제의 숫자 정책은 바꾸지 않는다. 실제 운영은
문서 도메인에 맞게 선택한다. 원본 편집과 반영 순서는
[사전 관리 가이드](lexicon-maintenance.ko.md)를 참고한다.

## Resource policy checks (v0.16)

Customer TSV is checked by the native pack/core validators before any output callback.
Errors identify the file, physical row and resource key. Successful results show
validated and unhandled row counts, also present in downloaded JSON. These are policy
schema checks; reference correctness still requires independent review.
See [editable policy guide](resource-validation.md).

## 한국어 TN 참조 보고서

`/ko-quality`는 전체 외부 자료를 오프라인으로 평가한 완료 기록을 보여준다.
문장/구간 정확 일치와 공백 제거 일치, 카테고리·원본 주제·수정 방향별 지표,
참조 품질 경고와 오류/미채점/정렬 실패 분모, 50건 단위 사례 조회를 제공한다.
사례 행을 클릭하면 모달에서 원문·주석 참조·비교용 참조·실제 출력 전체를 확인한다.
참조 구간 목록에서는 선택 구간과 문장 전체를 함께 표시한다. 긴 텍스트는 줄바꿈과
모달 내부 스크롤로 끝까지 표시하며, 상세 IR은 펼쳐서 볼 수 있다. 닫기 버튼·Esc·
배경 클릭으로 닫으면 선택했던 사례 버튼으로 키보드 초점이 돌아간다.
같은 데이터·설정·평가기·검토 파일의 측정만 증감을 표시하며 독립 held-out 점수로
표시하지 않는다. [평가·검토 가이드](korean-reference-benchmark.ko.md)를 따른다.

보고서 기본 경로는 ignored `benchmark/generated/ko-release/reports`다.
서버의 `--ko-report-root DIR`로 변경 가능하다. API는 읽기 전용이며 외부 데이터나
사례를 Git에 포함하지 않는다. 지표·개별 검토 양식 다운로드는 서버 원본을 수정하지 않는다.

### 전화번호와 선행 0 검증 (2026-09-28)

`phone-zero-v22` corpus는 53개 text/IR 계약을 제공한다. 예제에서 서울 지역번호,
임의 앞자리 전화번호, 영어 전화번호, 맨 선행 0 숫자를 골라 normalize/stream/verify로
비교할 수 있다. 한국어 품질 페이지의 같은 이름 보고서는 기존 사전 보강 보고서와
비교한다. 참조의 영/공 또는 전화 그룹 연결어 차이는 불일치에 그대로 포함된다.
영 읽기를 원하면 고객 TSV에 `number\tko-KR\tleading_zero_zero\t영`을 실제 탭으로
작성한다. 자세한 적용 범위는 [정책 가이드](phone-zero-policy.ko.md)에 있다.

### 공백 차이 진단 (2026-09-28)

`/ko-quality`의 **공백 차이의 종류**는 수 묶음, 수사 내부, 번호 자릿수,
단어 내부, 단위·접사, 서식, 나머지 경계로 검토 대상을 나눈다. 각 유형의
경계 수와 해당 문장 수, 판단 안내, 전체 사례 모달을 제공한다. 한 문장이 여러
유형에 포함될 수 있으므로 문장 수를 합산하지 않는다. 자동 분류는 검토 후보이며
모든 참조 차이를 맞춤법 오류로 판정하지 않는다. 기존 정확/공백 제거 점수는 그대로다.

완료 보고서와 hash로 연결된 `spacing/REPORT_ID/summary.json`을 별도 생성한다.
기본 위치는 `benchmark/generated/ko-release/spacing`이며 사용자 지정 report root도
그 부모의 `spacing`을 사용한다. 진단이 없는 과거 보고서는 미생성 안내를 표시한다.
`GET /api/ko-spacing?run=REPORT_ID`는 읽기 전용이다.
[생성 명령과 정책](korean-spacing.ko.md)을 참고한다.
웹 corpus `ko-spacing-v22`는 독립 작성한 30개 계약과 같으며, 고객 TSV의
`number_spacing / ko-KR / group / compact`로 기존 붙임 정책도 시험할 수 있다.

### 나이 단위와 끊어 읽기

`counted-noun-v22`는 세·차·학년의 숫자/단위 경계와 일반 단어 오인 방지32건을
제공한다. `break-controls-v22`는 위치 제어30건이다. 예제 선택에서 **단위 경계**와
**끊어 읽기**를 고르면 필요한 plain/annotated 설정이 함께 적용된다.
`{@break:1}`은 공백, `{@break:2}`는 쉼표와 공백, `{@break:3}`은 마침표와 공백을
삽입한다. 앞뒤 원문 공백/기호는 보존하며 3단계는 출력 callback도 확정한다.
[입력 규격](input-format.md#position-breaks-v022-continuation)을 참고한다.

새 한국어 품질 보고서의 공백 진단에는 **공백 진단 범주** 선택이 있다.
나이·생년월일을 선택하면 그 범주의 공백만 다른 문장에 한정한 유형과 사례가 보인다.
공백 외 읽기 차이는 이 진단의 분모에 포함되지 않으며 기존 문장/구간 점수에서
계속 집계한다. 모든 참조 간격을 필수 맞춤법으로 자동 승인하지 않는다.
