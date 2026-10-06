"""Jev evaluation sets for the /jev-eval page: sentences, TN outputs, scores.

A set is a directory under benchmark/generated/jev-eval/<set>/ made by
select_representative.py --sentences-only: sentences.jsonl ({id, category,
category_en, text}) and meta.json. No reference reading is used. Each TN to
compare (this engine, a legacy TN, ...) stores its output for every sentence in
systems/<name>.json; scoring asks Jev (tools/web/jev.py) whether each output
is a correct way to read the sentence aloud and keeps the result next to the
outputs. Pass = noul >= 0.7 and no digits, Latin letters or symbols left.

    python3 jev_eval.py normalize LIBRARY RESOURCES SENTENCES.jsonl   # this engine's outputs as JSON
"""
import datetime
import hashlib
import json
import re
import sys
import threading
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
import jev  # noqa: E402
import value_check  # noqa: E402

DEFAULT_ROOT = ROOT / "benchmark/generated/jev-eval"
CACHE = ROOT / "benchmark/generated/jev-cache/ko-KR.jsonl"
NAME = re.compile(r"^[A-Za-z0-9][A-Za-z0-9 ._-]{0,47}$")
MAX_OUTPUT = 4000
SCORING = threading.Lock()
LABELING = threading.Lock()
# Long digit strings read one by one: Jev cannot compare them digit by digit, so the value
# check decides (also marked ※ on the page).
WEAK = {"business_number", "id_card_number", "generated_number", "personal_id_number", "finance_bank",
        "phone_number"}
VERDICT_VERSION = 3
LABELS = ("correct", "wrong")


# A number written before one of these is an amount or a count, not a code (500만 원, 2020년).
AMOUNT_AFTER = set("만억천원명개년월일시분초세살회층배점위건대곳권장%")
# A code named just before the number (우편번호 51393, 제품코드 4047D).
CODE_CUE = re.compile(r"번호|코드|우편|계좌|비밀|핀|송장|주문|예약|사번|학번|아이디|ID|No", re.I)


def codes(value):
    """(form, after, before) of the code-like numbers: three or more digits, no , . ~ /, not
    before 만 원 명 년 …"""
    n = len(value.get("tokens", []))
    rows = zip(value.get("tokens", []), value.get("forms", []), value.get("after") or [""] * n,
               value.get("before") or [""] * n)
    return [(form, nxt, prev) for token, form, nxt, prev in rows
            if sum(c.isdigit() for c in token) >= 3 and not set(token) & set(",.~/") and nxt not in AMOUNT_AFTER]


def codes_read_acceptably(value):
    """Codes read one digit at a time, or as one number when 번 follows (예약번호 129번
    백이십구 번: the project owner's judgements, 2026-10-06), and letters in identifiers
    spelled (csk01 크스크영일 is wrong)."""
    return (all(form == "digits" or nxt == "번" for form, nxt, prev in codes(value)) and
            all(x["spelled"] for x in value.get("latin", [])))


def read_digit_by_digit(value):
    """Kept for version 2 callers: codes acceptable as above."""
    return codes_read_acceptably(value)


def verdict(noul, unread, value, weak):
    """pass, review or fail (2026-10-06): the value check and Jev each judge what they can.

    Code compares the numbers; Jev judges sense and convention, with its uncertain middle
    band left to a person (docs.typesafe.ai: route 0.3-0.7 to review). A matching value whose
    codes are read acceptably passes whatever Jev says, since Jev cannot compare digits: in
    code categories, and elsewhere when the code is named before it (우편번호 51393). A code
    read as an amount without 번 (비밀번호 8469를 팔천사백육십구) goes by Jev.

    Version 3 follows the project owner's first 100 judgements (Jev 0.03-0.10, 2026-10-06):
    92 correct, 8 wrong; version 2 agreed on 83, Jev alone at 0.7 on 8.
    """
    if unread or value["status"] == "mismatch":
        return "fail"
    if value["status"] == "ok" and codes_read_acceptably(value):
        cued = any(CODE_CUE.search(prev) for form, nxt, prev in codes(value))
        if weak or cued:
            return "pass"
    if noul >= jev.PASS:
        return "pass"
    return "fail" if noul < jev.REVIEW else "review"


def judged(row, output, noul, model):
    left = jev.unread(output, "ko-KR")
    value = value_check.check(row["text"], output)
    v = verdict(noul, left, value, row["category_en"] in WEAK)
    return dict(noul=noul, model=model, unread=left, output=output, value=value, verdict=v,
                passed=v == "pass", jev_passed=noul >= jev.PASS and not left)


class EvalError(Exception):
    def __init__(self, status, message):
        super().__init__(message)
        self.status = status


def set_dir(root, set_id):
    if not isinstance(set_id, str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}", set_id):
        raise EvalError(400, "잘못된 평가셋 ID")
    path = Path(root) / set_id
    if path.is_symlink() or not (path / "sentences.jsonl").is_file() or not (path / "meta.json").is_file():
        raise EvalError(404, "평가셋을 찾을 수 없습니다")
    return path


def sentences(root, set_id):
    path = set_dir(root, set_id)
    rows = [json.loads(l) for l in (path / "sentences.jsonl").read_text(encoding="utf-8").splitlines() if l.strip()]
    return json.loads((path / "meta.json").read_text(encoding="utf-8")), rows


def catalog(root):
    root = Path(root)
    sets = []
    if root.is_dir():
        for path in sorted(root.iterdir()):
            try:
                meta, rows = sentences(root, path.name)
            except EvalError:
                continue
            sets.append(dict(set_id=path.name, sentences=len(rows), source=meta.get("source"),
                             per_category=meta.get("per_category")))
    return dict(sets=sets)


def system_path(root, set_id, name):
    if not isinstance(name, str) or not NAME.match(name):
        raise EvalError(400, "시스템 이름은 영문·숫자로 시작하는 48자 이내(공백 . _ - 허용)입니다")
    return set_dir(root, set_id) / "systems" / (re.sub(r"[^A-Za-z0-9._-]", "_", name) + ".json")


def load_system(root, set_id, name):
    path = system_path(root, set_id, name)
    if not path.is_file():
        raise EvalError(404, "저장된 결과가 없습니다")
    return refresh(path, sentences(root, set_id)[1])


def refresh(path, rows):
    """Re-judge stored scores when the verdict rules changed; no Jev call, the scores stay."""
    data = json.loads(path.read_text(encoding="utf-8"))
    if data.get("verdict_version") == VERDICT_VERSION:
        return data
    by_id = {r["id"]: r for r in rows}
    scores = data.get("scores", {})
    for k, s in list(scores.items()):
        if k in by_id:
            scores[k] = judged(by_id[k], s["output"], s["noul"], s["model"])
    extra = {k: v for k, v in (data.get("summary") or {}).items() if k in ("newly_scored", "question")}
    data.update(scores=scores, verdict_version=VERDICT_VERSION,
                summary=dict(summarize(rows, data.get("outputs", {}), scores), **extra))
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False) + "\n", encoding="utf-8")
    tmp.replace(path)
    return data


def summarize(rows, outputs, scores):
    def metric():
        return dict(sentences=0, filled=0, scored=0, passed=0, review=0, failed=0, jev_passed=0,
                    value_mismatch=0, value_unknown=0, total=0.0, low=0, unread=0)

    overall, cats, names = metric(), {}, {}
    for r in rows:
        m = cats.setdefault(r["category_en"], metric())
        names[r["category_en"]] = r["category"]
        for x in (overall, m):
            x["sentences"] += 1
            if outputs.get(r["id"]):
                x["filled"] += 1
            s = scores.get(r["id"])
            if s and s.get("output") == outputs.get(r["id"]):
                x["scored"] += 1
                x["passed"] += s["passed"]
                x["review"] += s.get("verdict") == "review"
                x["failed"] += s.get("verdict") == "fail"
                x["jev_passed"] += s.get("jev_passed", s["passed"])
                x["value_mismatch"] += (s.get("value") or {}).get("status") == "mismatch"
                x["value_unknown"] += (s.get("value") or {}).get("status") == "unknown"
                x["total"] += s["noul"]
                x["low"] += s["noul"] < 0.4
                x["unread"] += bool(s["unread"])

    def finish(m):
        n = m.pop("total")
        return dict(m, mean=round(n / m["scored"], 4) if m["scored"] else None,
                    pass_rate=m["passed"] / m["scored"] if m["scored"] else None,
                    jev_pass_rate=m["jev_passed"] / m["scored"] if m["scored"] else None)

    return dict(overall=finish(overall),
                categories=[dict(id=k, name=names[k], **finish(v)) for k, v in cats.items()])


def systems(root, set_id):
    path = set_dir(root, set_id) / "systems"
    out = []
    if path.is_dir():
        rows = sentences(root, set_id)[1]
        for p in sorted(path.glob("*.json")):
            data = refresh(p, rows)
            out.append(dict(system=data["system"], updated_at=data["updated_at"], scored_at=data.get("scored_at"),
                            summary=data.get("summary")))
    return dict(set_id=set_id, systems=out)


def save(root, set_id, name, outputs):
    meta, rows = sentences(root, set_id)
    if not isinstance(outputs, dict):
        raise EvalError(400, "outputs는 {id: 결과} 객체여야 합니다")
    ids = {r["id"] for r in rows}
    unknown = [k for k in outputs if k not in ids]
    if unknown:
        raise EvalError(400, "평가셋에 없는 id %d개 (예: %s)" % (len(unknown), unknown[0]))
    clean = {}
    for k, v in outputs.items():
        if v is None or v == "":
            continue
        if not isinstance(v, str) or len(v) > MAX_OUTPUT or "\0" in v:
            raise EvalError(400, "결과는 %d자 이내 문자열이어야 합니다 (%s)" % (MAX_OUTPUT, k))
        clean[k] = v.strip()
    path = system_path(root, set_id, name)
    previous = json.loads(path.read_text(encoding="utf-8")) if path.is_file() else {}
    scores = previous.get("scores", {})
    data = dict(system=name, set_id=set_id, updated_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                scored_at=previous.get("scored_at"), outputs=clean, scores=scores,
                summary=summarize(rows, clean, scores))
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False) + "\n", encoding="utf-8")
    tmp.replace(path)
    return dict(system=name, summary=data["summary"])


def score(root, set_id, name, progress=None, batch=None, ids=None, patch=None):
    """Score the saved outputs of one system; unchanged pairs come from the cache.

    ids limits scoring to those sentences (one row, or the rows edited since
    their last score) and keeps the other scores; patch {id: output} first
    updates those outputs in the saved file, creating it when needed.
    """
    if not SCORING.acquire(blocking=False):
        raise EvalError(429, "다른 Jev 평가가 진행 중입니다. 끝난 뒤 다시 시도하세요.")
    try:
        meta, rows = sentences(root, set_id)
        known = {r["id"] for r in rows}
        if patch is not None:
            if not isinstance(patch, dict) or any(k not in known for k in patch):
                raise EvalError(400, "patch는 평가셋 id의 {id: 결과} 객체여야 합니다")
            for k, v in patch.items():
                if v is not None and (not isinstance(v, str) or len(v) > MAX_OUTPUT or "\0" in v):
                    raise EvalError(400, "결과는 %d자 이내 문자열이어야 합니다 (%s)" % (MAX_OUTPUT, k))
        if ids is not None and (not isinstance(ids, list) or any(k not in known for k in ids)):
            raise EvalError(400, "ids는 평가셋 id 목록이어야 합니다")
        path = system_path(root, set_id, name)
        data = json.loads(path.read_text(encoding="utf-8")) if path.is_file() else dict(
            system=name, set_id=set_id, outputs={}, scores={}, scored_at=None)
        outputs = data["outputs"]
        for k, v in (patch or {}).items():
            if v and v.strip():
                outputs[k] = v.strip()
            else:
                outputs.pop(k, None)
        data["updated_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        wanted = set(ids) if ids is not None else None
        pairs = [(r["text"], outputs[r["id"]]) for r in rows
                 if outputs.get(r["id"]) and (wanted is None or r["id"] in wanted)]
        if not pairs:
            raise EvalError(400, "평가할 결과가 없습니다")
        batch = batch or jev.batch
        try:
            scored, fresh = batch(pairs, "ko-KR", CACHE, progress=progress)
        except jev.JudgeError as exc:
            raise EvalError(exc.status, str(exc))
        scores = {k: v for k, v in data.get("scores", {}).items() if wanted is not None and k not in wanted}
        for r in rows:
            out = outputs.get(r["id"])
            if not out or (wanted is not None and r["id"] not in wanted):
                continue
            value, model = scored[(r["text"], out)]
            scores[r["id"]] = judged(r, out, value, model)
        data["scores"] = scores
        data["verdict_version"] = VERDICT_VERSION
        data["scored_at"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
        data["summary"] = dict(summarize(rows, outputs, scores), newly_scored=fresh, question=jev.QUESTION_ID)
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp = path.with_suffix(".tmp")
        tmp.write_text(json.dumps(data, ensure_ascii=False) + "\n", encoding="utf-8")
        tmp.replace(path)
        return dict(system=name, summary=data["summary"],
                    scores={k: scores[k] for k in (wanted if wanted is not None else scores) if k in scores})
    finally:
        SCORING.release()


def labels(root, set_id):
    """A person's judgements (correct / wrong) of sentence and output pairs, for calibration."""
    path = set_dir(root, set_id) / "labels.json"
    data = json.loads(path.read_text(encoding="utf-8")) if path.is_file() else {}
    return dict(set_id=set_id, labels=list(data.values()))


def set_label(root, set_id, row_id, output, label):
    meta, rows = sentences(root, set_id)
    if row_id not in {r["id"] for r in rows}:
        raise EvalError(400, "평가셋에 없는 id입니다")
    if not isinstance(output, str) or not output.strip() or len(output) > MAX_OUTPUT or "\0" in output:
        raise EvalError(400, "결과는 %d자 이내 문자열이어야 합니다" % MAX_OUTPUT)
    if label is not None and label not in LABELS:
        raise EvalError(400, "판단은 correct, wrong, null 중 하나입니다")
    output = output.strip()
    key = row_id + "\t" + hashlib.sha256(output.encode()).hexdigest()[:16]
    path = set_dir(root, set_id) / "labels.json"
    with LABELING:
        data = json.loads(path.read_text(encoding="utf-8")) if path.is_file() else {}
        if label is None:
            data.pop(key, None)
        else:
            data[key] = dict(id=row_id, output=output, label=label,
                             at=datetime.datetime.now(datetime.timezone.utc).isoformat())
        tmp = path.with_suffix(".tmp")
        tmp.write_text(json.dumps(data, ensure_ascii=False, indent=0) + "\n", encoding="utf-8")
        tmp.replace(path)
    return dict(id=row_id, output=output, label=label, count=len(data))


def normalize_main(library, resources, sentences_path):
    sys.path.insert(0, str(ROOT))
    from native import Engine
    from benchmark.evaluators.korean_release import OPTIONS
    engine = Engine(Path(library), Path(resources), dict(OPTIONS, default_language="ko-KR", detailed_trace=False))
    out = {}
    for line in Path(sentences_path).read_text(encoding="utf-8").splitlines():
        if line.strip():
            r = json.loads(line)
            try:
                out[r["id"]] = engine.normalize(r["text"].encode())["text"]
            except (ValueError, RuntimeError, OSError):
                out[r["id"]] = ""
    sys.stdout.write(json.dumps(out, ensure_ascii=False))


if __name__ == "__main__":
    if len(sys.argv) == 5 and sys.argv[1] == "normalize":
        normalize_main(*sys.argv[2:])
    else:
        raise SystemExit(__doc__)
