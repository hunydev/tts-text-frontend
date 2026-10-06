"""Deterministic number check for a Korean TN result: does `spoken` keep the values of `written`?

Jev cannot compare digits (docs.typesafe.ai Jev 1.13 jaggedness: counting and numeric
representations; one-digit errors in codes passed at 0.7 in 12-36% of cases, 2026-10-06), so
values are checked in code, as covering grammars do in text normalization (Zhang, Sproat et
al. 2019): every number in `written` becomes the set of its acceptable Korean readings (Sino,
digit by digit with Sino or native names, native, English numerals, decimals, fractions,
compressed ranges), the Hangul of `written` anchors them, units and symbols get their Korean
names or a wildcard that holds no number syllable, and the whole is matched against `spoken`
with spacing ignored.

Measured 2026-10-06: 300 correct ko-v1 outputs all ok; 300 one-digit errors 292 mismatch,
8 unknown, none ok; 40,000 benchmark references 98.6% ok, 0.14% mismatch (about half of them
reference errors such as 1933년 천구백구십삼).

    ok        every number has an acceptable reading in its place; `forms` says per number
              whether it was read digit by digit ("digits") or as one number ("whole"), and
              `after` gives the character written after it, `before` the 12 characters before
              it, and `latin` whether each Latin run written against a digit was spelled
    mismatch  the result has the same shape but a number reads as another value
    unknown   the shapes differ (dropped or added words, a reading form not covered)

Only `written` and `spoken` are used, so any TN can be checked. It knows nothing about
which reading is right in context; Jev judges that.
"""
import re

DIGIT = ["공|영", "일", "이", "삼", "사", "오", "육|륙", "칠", "팔", "구"]
DIGIT_PLAIN = "영일이삼사오육칠팔구"
# Digit by digit, references also use native names (공 일 오 하나 둘, 2026-10-06 sample).
DIGIT_SPOKEN = ["공|영", "일|하나", "이|둘", "삼|셋", "사|넷", "오|다섯", "육|륙|여섯", "칠|일곱", "팔|여덟", "구|아홉"]
ENGLISH = ["제로|영", "원", "투", "쓰리|스리", "포", "파이브", "식스", "세븐", "에잇|에이트", "나인", "텐"]
NATIVE_UNIT = ["", "하나|한", "둘|두", "셋|세|석|서", "넷|네|너|넉", "다섯|닷", "여섯|엿", "일곱", "여덟", "아홉"]
NATIVE_TEN = ["", "열", "스물|스무", "서른", "마흔", "쉰", "예순", "일흔", "여든", "아흔"]
ENGLISH_TEEN = {11: "일레븐", 12: "트웰브|트웰프", 13: "써틴|서틴", 14: "포틴", 15: "피프틴", 16: "식스틴", 17: "세븐틴",
                18: "에이틴", 19: "나인틴"}
ENGLISH_TENS = ["", "", "트웬티|투엔티|투에니|트웨니", "써티|서티|써리", "포티|포리", "피프티", "식스티", "세븐티", "에이티",
                "나인티"]
RANGE_NATIVE = {(1, 2): "한두", (2, 3): "두세", (3, 4): "서너|세네|세넷", (4, 5): "너댓|너덧|네다섯|네댓",
                (5, 6): "대여섯|대엿|다여섯", (6, 7): "예닐곱|여섯일곱", (7, 8): "일고여덟|일여덟", (8, 9): "여덟아홉"}
LETTER = {"A": "에이", "B": "비", "C": "씨|시", "D": "디", "E": "이", "F": "에프", "G": "지|쥐", "H": "에이치|에치",
          "I": "아이", "J": "제이", "K": "케이", "L": "엘", "M": "엠", "N": "엔", "O": "오", "P": "피", "Q": "큐",
          "R": "알|아르", "S": "에스", "T": "티", "U": "유", "V": "브이|비", "W": "더블유|더블류", "X": "엑스",
          "Y": "와이", "Z": "제트|지|젯"}
# Any number word: a mismatch is a number-shaped reading with another value.
NUMBER_WORD = ("(?:[공영일이삼사오육륙칠팔구십백천만억조점]|하나|한|둘|두|셋|세|석|서|넷|네|너|넉|다섯|닷|여섯|엿|일곱|"
               "여덟|아홉|열|스물|스무|서른|마흔|쉰|예순|일흔|여든|아흔|첫|제로|원|투|쓰리|스리|포|파이브|식스|세븐|"
               "에잇|에이트|나인|텐|유|시)+")


def sino(n):
    """Sino-Korean reading(s) of a non-negative integer as one regex."""
    if n == 0:
        return "영|공"
    out, group_names = [], ["", "만", "억", "조", "경"]
    groups = []
    while n:
        groups.append(n % 10000)
        n //= 10000
    for i in range(len(groups) - 1, -1, -1):
        g = groups[i]
        if not g:
            continue
        part = ""
        for value, name in ((1000, "천"), (100, "백"), (10, "십"), (1, "")):
            d = g // value % 10
            if d:
                digit = DIGIT[d] if name == "" else ("일?" if d == 1 else "(?:%s)" % DIGIT[d])
                part += ("(?:%s)" % digit if name == "" else digit) + name
        if i and g == 1:
            part = "일?"  # 만, 일만; 억 is usually 일억 but both are the same value
        out.append(part + group_names[i])
    return "".join(out)


def native(n):
    if not 1 <= n <= 99:
        return None
    t, u = divmod(n, 10)
    ten = NATIVE_TEN[t]
    unit = NATIVE_UNIT[u]
    if n == 20:
        return "스물|스무"
    pieces = []
    if ten:
        pieces.append("(?:%s)" % ten)
    if unit:
        pieces.append("(?:%s)" % unit)
    return "".join(pieces) + ("|첫" if n == 1 else "")


def digitwise(s, table=DIGIT_SPOKEN):
    return "".join("(?:%s)" % table[int(c)] for c in s)


def readings(s):
    """Regex alternatives for one digit string (no separators)."""
    n = int(s)
    alts = [sino(n), digitwise(s)]
    if len(s) > 1 and s[0] == "0" and n:
        alts.append(sino(n))
    nat = native(n)
    if nat:
        alts.append(nat)
    if n <= 10:
        alts.append(ENGLISH[n])
    elif n in ENGLISH_TEEN:
        alts.append(ENGLISH_TEEN[n])
    elif n < 100:
        t, u = divmod(n, 10)
        alts.append("(?:%s)%s" % (ENGLISH_TENS[t], "(?:%s)" % ENGLISH[u] if u else ""))
    if len(s) > 1:
        alts.append(digitwise(s, ENGLISH))
    if n == 6:
        alts.append("유")
    if n == 10:
        alts.append("시")
    return "(?:%s)" % "|".join("(?:%s)" % a for a in alts)


def number_pattern(token):
    """A written number token: 1,200 / 1.2 / 4.3 / 2026.09.25 / 010 / 15,213.32 / 4.000."""
    m = re.fullmatch(r"(\d{1,3}(?:,\d{3})+)\.(\d+)", token)
    if m:
        whole, frac = m.group(1).replace(",", ""), m.group(2)
        return "(?:%s점%s|%s)" % (readings(whole), digitwise(frac), ".{0,4}?".join(readings(x) for x in re.split(r"[.,]", token)))
    if re.fullmatch(r"\d{1,3}(?:\.\d{3})+", token):
        alts = [readings(token.replace(".", ""))]
        if token.count(".") == 1:
            whole, frac = token.split(".")
            alts.append("%s점%s" % (readings(whole), digitwise(frac)))
        alts.append(".{0,4}?".join(readings(x) for x in token.split(".")))
        return "(?:%s)" % "|".join("(?:%s)" % a for a in alts)
    if re.fullmatch(r"\d{1,3}(?:,\d{3})+", token):
        return "(?:%s|%s)" % (readings(token.replace(",", "")),
                              "".join(readings(p) for p in token.split(",")))
    parts = re.split(r"[.,]", token)
    joined = ".{0,4}?".join(readings(p) for p in parts)
    if token.count(".") == 1 and "," not in token:
        whole, frac = parts
        return "(?:%s점%s|%s)" % (readings(whole), digitwise(frac), joined)
    return "(?:%s)" % joined


def range_pattern(a, b):
    """2~3 두세, 20~30 이삼십, 3~4 서너, 1770~1780 천칠백칠팔십: compressed readings of a range."""
    x, y = int(a), int(b)
    alts = []
    if (x, y) in RANGE_NATIVE:
        alts.append(RANGE_NATIVE[(x, y)])
    if len(a) == len(b) and len(a) > 2:
        d = next((i for i in range(len(a)) if a[i] != b[i]), None)
        tail = len(a) - d - 1 if d is not None else -1
        if d and 1 <= tail <= 3 and set(a[d + 1:] + b[d + 1:]) == {"0"}:
            prefix = int(a[:d]) * 10 ** (tail + 1)
            alts.append("%s(?:%s)(?:%s)%s" % (sino(prefix), DIGIT[int(a[d])], DIGIT[int(b[d])], ["십", "백", "천"][tail - 1]))
    for zeros, unit in ((1, "십"), (2, "백"), (3, "천"), (4, "만")):
        k = 10 ** zeros
        if x % k == 0 and y % k == 0 and x // k < 10 and y // k < 10 and x and y:
            alts.append("(?:%s)(?:%s)%s" % (DIGIT[x // k], DIGIT[y // k], unit))
    return alts


UNIT = {"kg": "킬로그램|킬로", "g": "그램|그람", "mg": "밀리그램", "km": "킬로미터|킬로", "m": "미터", "cm": "센티미터|센티",
        "mm": "밀리미터|밀리", "ml": "밀리리터|밀리", "l": "리터", "kcal": "킬로칼로리|칼로리", "cal": "칼로리",
        "kw": "킬로와트", "kwh": "킬로와트시", "w": "와트", "v": "볼트", "mb": "메가바이트|메가", "gb": "기가바이트|기가",
        "tb": "테라바이트|테라", "hz": "헤르츠", "mhz": "메가헤르츠", "ghz": "기가헤르츠", "cc": "씨씨|시시",
        "t": "톤", "ha": "헥타르", "kb": "킬로바이트", "mph": "마일", "km/h": "킬로미터"}
# A wildcard (a symbol or word read in some way) holds no Sino number syllable, so it never
# takes a digit from the number next to it.
FREE = "(?:[^영공일이삼사오육륙칠팔구십백천만억]{1,%d}?)?"


def latin_pattern(run, name=None):
    """A Latin run: its unit name, its letter names, or a wildcard (a word read some way).
    With `name`, the unit and letter readings are a named group, so a match tells whether
    the run was spelled (csk01 씨에스케이영일) or read some other way (크스크영일)."""
    width = 6 * len(run) + 6
    spelled = []
    if run.lower() in UNIT:
        spelled.append(UNIT[run.lower()])
    if len(run) <= 5:
        spelled.append("".join("(?:%s)" % LETTER[c] for c in run.upper()))
    if not spelled:
        return FREE % width
    group = "|".join("(?:%s)" % a for a in spelled)
    group = "(?P<%s>%s)" % (name, group) if name else "(?:%s)" % group
    return "(?:%s|%s)" % (group, FREE % width)


TOKEN = re.compile(r"(?P<space>\s+)|(?P<range>(?<![\d.,])\d+(?:,\d{3})*~\d+(?:,\d{3})*(?![\d.,]))|(?P<frac>(?<![\d/])\d+/\d+(?![\d/]))|"
                   r"(?P<num>\d+(?:[.,]\d+)*)|(?P<hangul>[가-힣]+)|(?P<latin>[A-Za-z]+)|(?P<other>.)", re.S)
SYMBOL = {"%": "퍼센트|프로", "~": "에서|부터|내지|까지|다시", "-": "다시|에서|마이너스|빼기|하이픈|대시", ":": "대|시|다시|비",
          "/": "분의|슬래시|나누기|퍼|당|에", "+": "더하기|플러스", "×": "곱하기|엑스", "=": "는|은|이퀄", ".": "점|닷",
          "°": "도", "℃": "도|섭씨", "㎡": "제곱미터|평방미터", "㎢": "제곱킬로미터", "㎞": "킬로미터|킬로", "㎏": "킬로그램|킬로",
          "㎖": "밀리리터", "ℓ": "리터", "$": "달러", "₩": "원", "€": "유로", "£": "파운드", "¥": "엔|위안", "#": "샵|번"}


TILDE = "~∼～〜"


def fold(text):
    """Fullwidth ASCII to ASCII and wave dashes to ~; other symbols (㎡, ℓ) stay as written."""
    out = []
    for c in text:
        o = ord(c)
        out.append(chr(o - 0xFEE0) if 0xFF01 <= o <= 0xFF5E else "~" if c in TILDE else " " if c == "\u3000" else c)
    return "".join(out)


def compact(text):
    return re.sub(r"\s+", "", fold(text))


def build(written, exact=True):
    """Pattern for the whole result: numbers as named groups n0, n1, ..., and Latin runs
    written against a digit as l0, l1, ... (exact patterns only)."""
    parts, numbers, latin = [], [], []
    folded = fold(written)
    for m in TOKEN.finditer(folded):
        kind, text = m.lastgroup, m.group(0)
        if kind == "space":
            continue
        if kind == "hangul":
            parts.append(re.escape(text))
        elif kind in ("num", "range"):
            name = "n%d" % len(numbers)
            numbers.append(text)
            if not exact:
                parts.append("(?P<%s>%s)" % (name, NUMBER_WORD))
            elif kind == "range":
                a, b = text.replace(",", "").split("~")
                alts = ["%s.{0,4}?%s" % (readings(a), readings(b))] + range_pattern(a, b)
                parts.append("(?P<%s>%s)" % (name, "|".join("(?:%s)" % x for x in alts)))
            else:
                parts.append("(?P<%s>%s)" % (name, number_pattern(text)))
        elif kind == "frac":
            name = "n%d" % len(numbers)
            numbers.append(text)
            a, b = text.split("/")
            body = NUMBER_WORD if not exact else "(?:%s)분의(?:%s)|%s%s%s" % (
                readings(b), readings(a), readings(a), FREE % 12, readings(b))
            parts.append("(?P<%s>%s)" % (name, body))
        elif kind == "latin":
            # Only runs written against a digit (csk01, 4047D) are tracked: identifiers.
            touching = folded[m.start() - 1:m.start()].isdigit() or folded[m.end():m.end() + 1].isdigit()
            # Words longer than five letters have no spelling alternative and are not tracked.
            spellable = text.lower() in UNIT or len(text) <= 5
            name = "l%d" % len(latin) if touching and exact and spellable else None
            if name:
                latin.append(text)
            parts.append(latin_pattern(text, name))
        elif text in SYMBOL:
            parts.append("(?:(?:%s)|%s)" % (SYMBOL[text], FREE % 6))
        else:
            parts.append(FREE % 6)
    return "".join(parts), numbers, latin


NATIVE_VALUE = [(w, v) for v, ws in enumerate(NATIVE_UNIT) if v for w in ws.split("|")] + \
    [(w, 10 * v) for v, ws in enumerate(NATIVE_TEN) if v for w in ws.split("|")] + [("스무", 20), ("첫", 1)]
ENGLISH_VALUE = [(w, v) for v, ws in enumerate(ENGLISH) for w in ws.split("|")]


def native_value(reading):
    """스물셋 23, 열 10, 마흔다섯 45; None when it is not one native number."""
    total, rest, used = 0, reading, False
    for table in (sorted([x for x in NATIVE_VALUE if x[1] >= 10], key=lambda x: -len(x[0])),
                  sorted([x for x in NATIVE_VALUE if x[1] < 10], key=lambda x: -len(x[0]))):
        for word, value in table:
            if rest.startswith(word):
                total += value; rest = rest[len(word):]; used = True
                break
    return total if used and not rest else None


def english_value(reading):
    digits, rest = "", reading
    while rest:
        for word, value in sorted(ENGLISH_VALUE, key=lambda x: -len(x[0])):
            if rest.startswith(word):
                digits += str(value); rest = rest[len(word):]
                break
        else:
            return None
    return digits or None


def digit_sequence(reading):
    words = sorted(((w, str(d)) for d, ws in enumerate(DIGIT_SPOKEN) for w in ws.split("|")), key=lambda x: -len(x[0]))
    out, rest = "", reading
    while rest:
        for w, d in words:
            if rest.startswith(w):
                out += d; rest = rest[len(w):]
                break
        else:
            return None
    return out


def value_of(reading):
    if "점" in reading:
        whole, _, frac = reading.partition("점")
        fracs = {v for v in value_of(frac) if v.isdigit()} if frac else set()
        return {"%s.%s" % (w, f) for w in value_of(whole) for f in fracs if w.isdigit()}
    out = set()
    n = native_value(reading)
    if n is not None:
        out.add(str(n))
    e = english_value(reading)
    if e:
        out.add(e)
    out |= sino_value(reading)
    d = digit_sequence(reading)
    if d:
        out.add(d)
    return out


def sino_value(reading):
    """Values a Sino reading can stand for: digit by digit, or one number (천이백 1200)."""
    out = set()
    if reading and all(c in DIGIT_PLAIN + "공륙" for c in reading):
        out.add("".join(str("영일이삼사오육칠팔구".index(c)) if c not in "공륙" else ("0" if c == "공" else "6")
                        for c in reading))
    total, group, digit = 0, 0, None
    units = {"십": 10, "백": 100, "천": 1000}
    big = {"만": 10 ** 4, "억": 10 ** 8, "조": 10 ** 12}
    ok = True
    for c in reading:
        if c in "영일이삼사오육칠팔구륙공":
            if digit is not None:
                ok = False
                break
            digit = "영일이삼사오육칠팔구".find(c) if c not in "륙공" else (6 if c == "륙" else 0)
        elif c in units:
            group += (digit if digit is not None else 1) * units[c]
            digit = None
        elif c in big:
            total += (group + (digit or 0) or 1) * big[c]
            group, digit = 0, None
        else:
            ok = False
            break
    if ok and reading:
        out.add(str(total + group + (digit or 0)))
    return out


def check(written, spoken):
    if not re.search(r"\d", written):
        return dict(status="ok", numbers=0, findings=[])
    target = compact(spoken)
    pattern, numbers, latin_runs = build(written)
    try:
        ok = re.fullmatch(pattern, target)
        if ok:
            forms = ["digits" if digit_sequence(ok.group("n%d" % i)) is not None else "whole"
                     for i in range(len(numbers))]
            folded = fold(written)
            spans = [m for m in TOKEN.finditer(folded) if m.lastgroup in ("num", "range", "frac")]
            after = [folded[m.end():m.end() + 1] for m in spans]
            before = [folded[max(0, m.start() - 12):m.start()] for m in spans]
            latin = [dict(run=run, spelled=ok.group("l%d" % i) is not None) for i, run in enumerate(latin_runs)]
            return dict(status="ok", numbers=len(numbers), findings=[], tokens=numbers, forms=forms, after=after,
                        before=before, latin=latin)
        generic, _, _ = build(written, exact=False)
        m = re.fullmatch(generic, target)
    except re.error:
        return dict(status="unknown", numbers=len(numbers), findings=[], reason="pattern")
    if not m:
        return dict(status="unknown", numbers=len(numbers), findings=[], reason="shape")
    findings = []
    for i, token in enumerate(numbers):
        got = m.group("n%d" % i)
        exact = re.fullmatch(number_pattern(token) if "~" not in token and "/" not in token else ".*", got)
        if exact:
            continue
        values = value_of(got)
        plain = token.replace(",", "")
        expected = {plain, plain.lstrip("0") or "0", re.sub(r"\.?0+$", "", plain) if "." in plain else plain,
                    plain.replace(".", "")}
        if values and not values & expected:
            findings.append(dict(written=token, spoken=got, read_as=sorted(values)))
    if findings:
        return dict(status="mismatch", numbers=len(numbers), findings=findings)
    return dict(status="unknown", numbers=len(numbers), findings=[], reason="form")
