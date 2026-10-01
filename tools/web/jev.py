"""TypeSafe Jev judgement of a normalization result for the web workbench.

One Noul question (https://docs.typesafe.ai/primitives/noul): is `spoken` a
correct way to read `written` aloud? Several readings can be right (서른 명에서
마흔 명 / 삼사십 명), which one reference sentence cannot show; the score says
how likely Jev finds this one acceptable.

Calibration (2026-10-01, jev-1.13.0, 40 Korean sentences the engine reads as
the reference does): correct outputs median 0.81, accepted variants 0.81, the
same sentence with one number wrong 0.15 (AUC 0.97), with one word dropped
0.37 (AUC 0.89). English uses the same question but is not calibrated.

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
QUESTION_ID = "spoken-reading-correct-v1"
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


def context(language):
    return ("%s text-to-speech normalization: `written` is the input text; `spoken` is what the synthesizer "
            "will say, with numbers, dates, units, symbols, abbreviations and foreign words written out as "
            "%s speech." % (LANGUAGES[language], LANGUAGES[language]))


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

