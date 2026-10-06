"""TypeSafe Jev judgement of a normalization result for the web workbench.

One Noul question (https://docs.typesafe.ai/primitives/noul): is `spoken` a
correct way to read `written` aloud? Several readings can be right (서른 명에서
마흔 명 / 삼사십 명), which one reference sentence cannot show; the score says
how likely Jev finds this one acceptable.

Calibration (2026-10-01, jev-1.13.0, 40 Korean sentences the engine reads as
the reference does): correct outputs median 0.81, accepted variants 0.81, the
same sentence with one number wrong 0.15 (AUC 0.97), with one word dropped
0.37 (AUC 0.89). Question v2 adds Korean reading conventions to the context
(see CONVENTIONS). English uses the same question but is not calibrated.

Jev does not reliably notice written forms left in the output (코로나 십구로
인해 30~40명 scored 0.78), so for Korean the result also lists any digits,
Latin letters or symbols still in `spoken` (`unread`); the page then asks for a
check whatever the score.

The API key stays on the server (~/.config/ttf/typesafe_api_key or
TYPESAFE_API_KEY) and is never sent to the browser or printed. Requests are
rate limited per client and in total, and repeated pairs are served from a
small cache.
"""
import collections
import hashlib
import json
import os
import re
import threading
import time
import urllib.error
import urllib.request
from pathlib import Path

URL = "https://api.typesafe.ai/v1/systemone"
MODEL = "jev-latest"
QUESTION_ID = "spoken-reading-correct-v3"
LANGUAGES = {"ko-KR": "Korean", "en-US": "English"}
MAX_WRITTEN = 2000
MAX_SPOKEN = 4000
PER_CLIENT = (20, 60.0)  # requests per window (seconds)
TOTAL = (600, 3600.0)


UNREAD = re.compile(r"[0-9A-Za-z]+(?:[^\s가-힣]*[0-9A-Za-z]+)*|[~%$€£¥₩@#&*+=<>/\\|^_]+")


def unread(spoken, language):
    """Written forms left in a Korean result: digits, Latin runs, symbols."""
    if language != "ko-KR":
        return []
    return list(dict.fromkeys(m.group(0) for m in UNREAD.finditer(spoken)))[:10]


def question(language):
    name = LANGUAGES[language]
    return {
        "type": "noul",
        "instructions": "`spoken` is a correct way to read `written` aloud in %s." % name,
        "criteria": {
            "true": "Every number, date, unit, symbol, abbreviation and foreign word is read as a %s speaker "
                    "would naturally say it in this context, with nothing added, left out or left unread; any "
                    "of several acceptable readings is fine." % name,
            "false": "Something is misread, read in the wrong sense (for example a unit, counter or number "
                     "type), left in its written form, dropped or added.",
        },
    }


# Korean reading conventions in the context (2026-10-01). Without them Jev read
# Sino-Korean numbers as look-alike words (2층 이 층 "this floor", 4회 사회
# "society"), doubted Korean letter names (Z 제트) and accepted codes read as
# one large amount. v2 prescribed digit-by-digit codes with 다시 and lowered
# correct phone, bus and room readings, so v3 lists the acceptable readings.
# On 67 reviewed correct outputs and 31 real errors: AUC v1 0.59, v2 0.94,
# v3 0.90; errors passing at 0.7: 29%, 0%, 0%; hard correct outputs passing:
# 49%, 6%, 15%. Value-error calibration: correct 0.82, one wrong digit 0.18.
CONVENTIONS = {
    "ko-KR": (" Korean reading conventions (several readings can be right): a digit before a counter or unit is "
              "a Sino-Korean number that can look like another word (2층 이 층, 2주 이 주, 4회 사 회, 2.5 이 점 "
              "오, 9동 구동, 2대3 이 대 삼) - judge it as that number. Personal ID, account, card, phone, PIN, "
              "postal, reservation and order numbers and other codes are read digit by digit (0 as 공 or 영), in "
              "groups or with 다시 or a pause at a hyphen; reading such a number as one large amount is wrong. "
              "Bus, line, room, building and floor numbers and counts are usually read as whole numbers (742번 "
              "칠백사십이 번, 1206호 천이백육 호). Latin letters use Korean letter names (Z 제트, K 케이). "
              "Spacing does not matter."),
}


def context(language):
    return ("%s text-to-speech normalization: `written` is the input text; `spoken` is what the synthesizer "
            "will say, with numbers, dates, units, symbols, abbreviations and foreign words written out as "
            "%s speech." % (LANGUAGES[language], LANGUAGES[language])) + CONVENTIONS.get(language, "")


class JudgeError(Exception):
    def __init__(self, status, message):
        super().__init__(message)
        self.status = status


class Judge:
    def __init__(self, key=None, opener=urllib.request.urlopen, clock=time.monotonic):
        if key is None:
            key = os.environ.get("TYPESAFE_API_KEY")
            path = Path.home() / ".config/ttf/typesafe_api_key"
            if not key and path.exists():
                key = path.read_text().strip()
        self.key = key or None
        self.opener = opener
        self.clock = clock
        self.lock = threading.Lock()
        self.clients = collections.defaultdict(collections.deque)
        self.total = collections.deque()
        self.cache = collections.OrderedDict()

    @property
    def available(self):
        return self.key is not None

    def _admit(self, client):
        now = self.clock()
        with self.lock:
            for window, (limit, span) in ((self.clients[client], PER_CLIENT), (self.total, TOTAL)):
                while window and now - window[0] > span:
                    window.popleft()
                if len(window) >= limit:
                    raise JudgeError(429, "Jev 채점 요청이 너무 많습니다. 잠시 후 다시 시도하세요.")
            self.clients[client].append(now)
            self.total.append(now)

    def score(self, written, spoken, language, client="-", speech=True):
        if not self.available:
            raise JudgeError(503, "이 서버에는 Jev 채점이 설정되어 있지 않습니다.")
        if language not in LANGUAGES:
            raise JudgeError(400, "지원하지 않는 언어입니다.")
        if not isinstance(written, str) or not isinstance(spoken, str) or not written.strip() or not spoken.strip():
            raise JudgeError(400, "원문과 정규화 결과가 모두 필요합니다.")
        if len(written) > MAX_WRITTEN or len(spoken) > MAX_SPOKEN:
            raise JudgeError(400, "Jev 채점은 원문 %d자, 결과 %d자까지입니다." % (MAX_WRITTEN, MAX_SPOKEN))
        key = hashlib.sha256(json.dumps([language, written, spoken]).encode()).hexdigest()
        with self.lock:
            if key in self.cache:
                self.cache.move_to_end(key)
                return dict(self.cache[key], cached=True, unread=unread(spoken, language) if speech else [])
        self._admit(client)
        body = {"model": MODEL, "state": {"written": written, "spoken": spoken, "context": context(language)},
                "questions": {"reading_correct": question(language)}}
        request = urllib.request.Request(URL, data=json.dumps(body).encode(), method="POST",
                                         headers={"Authorization": "Bearer " + self.key,
                                                  "Content-Type": "application/json"})
        try:
            with self.opener(request, timeout=30) as response:
                answer = json.loads(response.read())
            value = float(answer["answers"]["reading_correct"]["noul"])
        except urllib.error.HTTPError as exc:
            raise JudgeError(429 if exc.code in (429, 529) else 502,
                             "Jev 채점 서비스가 요청을 처리하지 못했습니다 (HTTP %d)." % exc.code)
        except (urllib.error.URLError, TimeoutError, ConnectionError, ValueError, KeyError, TypeError):
            raise JudgeError(502, "Jev 채점 서비스에 연결하지 못했습니다.")
        result = dict(noul=round(value, 4), model=answer.get("model", MODEL), question=QUESTION_ID,
                      calibrated=language == "ko-KR", cached=False)
        with self.lock:
            self.cache[key] = result
            while len(self.cache) > 512:
                self.cache.popitem(last=False)
        return dict(result, unread=unread(spoken, language) if speech else [])


PASS = 0.7

# Shown on /jev-eval with the exact request (2026-10-06, project-owner request:
# the instruction Jev gets must be visible).
HISTORY = (
    "v1: 질문과 판정 기준만 보냄. 한자어 숫자를 비슷한 낱말로 읽음(2층 이 층을 '이 층'으로, 4회를 '사회'로).",
    "v2 (2026-10-01): 한국어 읽기 관례를 맥락에 추가. 번호를 한 자리씩 '다시'로 읽으라고 정해서 맞는 전화·버스·호수 읽기를 낮게 봄.",
    "v3 (2026-10-01, 현재): 맞는 읽기를 여러 개 나열. 검토한 정답 67개·오류 31개에서 AUC v1 0.59, v2 0.94, v3 0.90. "
    "0.7에서 통과한 오류 v1 29%, v2 0%, v3 0%. 숫자 하나 틀린 문장 0.18, 정답 0.82.",
    "검토 (2026-10-06): ko-v1에서 참조와 같게 읽은 3,349문장 중 29%가 0.7 미만. 번호류(한 자리씩 읽는 번호) "
    "64~92%, 그 밖 5~43%. 정답 300문장과 숫자 하나만 바꾼 오답 300문장으로 개선안을 시험: 관례 보강(v4)은 정답 "
    "인정 72%→84%, 오답 통과 6.7%→12.7%; 어절마다 묻기(v5)는 86.7%, 15.0%. 같은 오답 통과율에서 비교하면 v3와 "
    "비슷해, 질문 문구는 점수를 전체적으로 올리거나 내릴 뿐 맞고 틀림을 더 잘 가리지는 못함. 번호류는 Jev가 "
    "숫자를 자리별로 대조하지 못해 문구로 고칠 수 없음. 질문은 v3 유지.",
    "한국어 질문 시험 (2026-10-06): v3와 v4를 내용 그대로 한국어로 옮겨 같은 600쌍으로 비교. AUC v3 0.921 → "
    "한국어 0.901, v4 0.940 → 0.928. 한국어 v3는 정답 점수가 전체적으로 낮아짐(중앙값 0.79 → 0.73, 0.7에서 정답 "
    "인정 72% → 62.7%). 질문은 영어 유지.",
)


def instructions():
    """What every Jev request carries, with the texts as placeholders."""
    return dict(
        endpoint=URL, model=MODEL, question_id=QUESTION_ID, pass_threshold=PASS,
        pass_rule="noul >= %.1f, and for Korean no digits, Latin letters or symbols left in spoken" % PASS,
        unread_pattern=UNREAD.pattern, history=list(HISTORY),
        languages={language: dict(
            name=name, calibrated=language == "ko-KR",
            request={"model": MODEL,
                     "state": {"written": "<원문>", "spoken": "<TN 결과>", "context": context(language)},
                     "questions": {"reading_correct": question(language)}})
            for language, name in LANGUAGES.items()})


def cache_key(written, spoken, language):
    return hashlib.sha256(json.dumps([QUESTION_ID, language, written, spoken]).encode()).hexdigest()


def batch(pairs, language, cache_path, key=None, workers=8, opener=urllib.request.urlopen, progress=None):
    """Score (written, spoken) pairs for a benchmark run; returns {pair: (noul, model)}.

    Rows already in cache_path (JSON lines keyed by question, language and both
    texts) are reused, so a rerun or another system's identical output costs
    nothing. Retries 429/529/5xx with backoff; any other failure stops the run.
    """
    from concurrent.futures import ThreadPoolExecutor
    judge = Judge(key=key, opener=opener)
    if not judge.available:
        raise JudgeError(503, "TypeSafe API key not found (~/.config/ttf/typesafe_api_key or TYPESAFE_API_KEY)")
    cache_path = Path(cache_path)
    cache_path.parent.mkdir(parents=True, exist_ok=True)
    known = {}
    if cache_path.exists():
        for line in cache_path.read_text(encoding="utf-8").splitlines():
            if line.strip():
                row = json.loads(line)
                known[row["key"]] = (row["noul"], row["model"])
    unique = list(dict.fromkeys(pairs))
    todo = [p for p in unique if cache_key(p[0], p[1], language) not in known]
    if progress:
        progress(0, len(todo))
    lock = threading.Lock()
    done = [0]

    def one(pair):
        body = {"model": MODEL, "state": {"written": pair[0], "spoken": pair[1], "context": context(language)},
                "questions": {"reading_correct": question(language)}}
        data = json.dumps(body).encode()
        delay = 1.0
        for attempt in range(8):
            request = urllib.request.Request(URL, data=data, method="POST",
                                             headers={"Authorization": "Bearer " + judge.key,
                                                      "Content-Type": "application/json"})
            try:
                with opener(request, timeout=60) as response:
                    answer = json.loads(response.read())
                value = round(float(answer["answers"]["reading_correct"]["noul"]), 4)
                model = answer.get("model", MODEL)
                break
            except urllib.error.HTTPError as exc:
                if exc.code not in (429, 500, 502, 503, 529) or attempt == 7:
                    raise JudgeError(502, "Jev HTTP %d" % exc.code)
            except (urllib.error.URLError, TimeoutError, ConnectionError):
                if attempt == 7:
                    raise JudgeError(502, "Jev unreachable")
            time.sleep(delay)
            delay = min(delay * 2, 30)
        row = dict(key=cache_key(pair[0], pair[1], language), noul=value, model=model, question=QUESTION_ID,
                   language=language)
        with lock:
            with cache_path.open("a", encoding="utf-8") as h:
                h.write(json.dumps(row) + "\n")
            known[row["key"]] = (value, model)
            done[0] += 1
            if progress and done[0] % 200 == 0:
                progress(done[0], len(todo))

    with ThreadPoolExecutor(workers) as pool:
        for future in [pool.submit(one, p) for p in todo]:
            future.result()
    if progress and todo:
        progress(len(todo), len(todo))
    return {p: known[cache_key(p[0], p[1], language)] for p in unique}, len(todo)

