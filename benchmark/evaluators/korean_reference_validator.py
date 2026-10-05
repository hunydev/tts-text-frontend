#!/usr/bin/env python3
"""Engine-independent validation of provided Korean references.

Reads only an import's source text and provided references, never an engine's
output, so the verdict is identical for every engine compared against the same
import (this project, a legacy TN, ...). Writes a review file of `excluded`
entries that benchmark/evaluators/korean_release.py accepts with --reviews; the
raw headline is unchanged and `adjusted_sentences` reports the score without
these references.

The checks are deliberately conservative. A reading is accepted when ANY
legitimate interpretation fits: Sino-Korean, native Korean, digit by digit,
mixed, ordinal 째, 유월/시월, or English number words. Policy variants such as
서른/삼십 or 다시/no hyphen word are therefore never errors, and a shape the
checks do not understand is not judged rather than flagged.

  python3 benchmark/evaluators/korean_reference_validator.py \\
    --import-dir benchmark/generated/ko-release/import-v1 \\
    --reviews-out benchmark/generated/ko-release/reviews/reference-errors.jsonl
"""
import argparse
import collections
import datetime
import difflib
import json
import re
import sys
from pathlib import Path

VERSION = "korean-reference-validator/2"
SINO_DIGIT = {"영": 0, "공": 0, "일": 1, "이": 2, "삼": 3, "사": 4, "오": 5,
              "육": 6, "륙": 6, "칠": 7, "팔": 8, "구": 9}
SMALL = {"십": 10, "백": 100, "천": 1000}
BIG = {"만": 10 ** 4, "억": 10 ** 8, "조": 10 ** 12, "경": 10 ** 16}
NATIVE_ONES = {"하나": 1, "한": 1, "둘": 2, "두": 2, "셋": 3, "세": 3, "석": 3, "서": 3,
               "넷": 4, "네": 4, "너": 4, "다섯": 5, "닷": 5, "여섯": 6, "엿": 6,
               "일곱": 7, "여덟": 8, "아홉": 9}
NATIVE_TENS = {"열": 10, "스물": 20, "스무": 20, "서른": 30, "마흔": 40, "쉰": 50,
               "예순": 60, "일흔": 70, "여든": 80, "아흔": 90}
DIGIT_WORDS = {"영": "0", "공": "0", "빵": "0", "일": "1", "하나": "1", "이": "2", "둘": "2",
               "삼": "3", "셋": "3", "사": "4", "넷": "4", "오": "5", "다섯": "5", "육": "6",
               "륙": "6", "여섯": "6", "칠": "7", "일곱": "7", "팔": "8", "여덟": "8",
               "구": "9", "아홉": "9"}
SEPARATORS = ("다시", "에", "대", "의", "하이픈", "빼기")
# English number words in Hangul (31데이 → 써리 원 데이) are a legitimate reading.
ENGLISH_NUMBERS = ("제로", "원", "투", "쓰리", "스리", "트리", "포", "파이브", "식스", "씩스",
                   "세븐", "에잇", "에이트", "나인", "텐", "일레븐", "트웰브", "써틴", "서틴",
                   "포틴", "피프틴", "식스틴", "세븐틴", "에이틴", "나인틴", "트웬티", "투엔티",
                   "트웨니", "투애니", "써티", "서티", "써리", "포티", "피프티", "식스티", "세븐티",
                   "에이티", "나인티", "헌드레드", "사우전드", "싸우전드", "밀리언", "오")
NUMBER_SYLLABLES = set("".join(SINO_DIGIT) + "".join(SMALL) + "".join(BIG) +
                       "".join(NATIVE_ONES) + "".join(NATIVE_TENS) + "".join(DIGIT_WORDS))
INTEGER = re.compile(r"(\d{1,3}(?:,\d{3})+|\d+)([가-힣]*)")
DASHED = re.compile(r"\d+(?:-\d+)+")
HANGUL_WORD = re.compile(r"[가-힣]{2,}")
NOT_JUDGED_CATEGORIES = {"dialect"}          # dialect readings are the category's purpose
# v2 text check (records whose spans carry no usable offsets): punctuation a reading may
# drop or keep; ~ - / · and other symbols are readable and stay variable.
TEXT_PUNCT = re.compile(r"[\s\.,!?;:'\"“”‘’()\[\]{}<>《》〈〉「」『』…]+")
TEXT_ABBREV = re.compile(r"\([가-힣]\)")              # (사) 사단법인, (주) 주식회사
VARIABLE = "\x00"
# Particles a reference may adapt to the reading of the number right before them.
ALLOMORPHS = {frozenset(pair) for pair in (
    ("은", "는"), ("이", "가"), ("을", "를"), ("와", "과"), ("로", "으로"), ("이에요", "에요"),
    ("이라고", "라고"), ("이라는", "라는"), ("이다", "다"), ("이고", "고"), ("이며", "며"),
    ("이나", "나"), ("이랑", "랑"), ("이야", "야"), ("이었", "였"), ("이면", "면"), ("이라", "라"),
    ("이지만", "지만"), ("이니", "니"), ("아", "야"), ("이여", "여"))}


def compact(text):
    return re.sub(r"\s+", "", text or "")


def tokenize(text, vocabulary):
    """Greedy longest-first split into vocabulary words; None if anything is left."""
    words = sorted(vocabulary, key=len, reverse=True)
    out, at = [], 0
    while at < len(text):
        for word in words:
            if text.startswith(word, at):
                out.append(word)
                at += len(word)
                break
        else:
            return None
    return out


def sino(text):
    if not text:
        return None
    total = section = 0
    digit = None
    for ch in text:
        if ch in SINO_DIGIT:
            if digit is not None:
                return None
            digit = SINO_DIGIT[ch]
        elif ch in SMALL:
            section += (1 if digit is None else digit) * SMALL[ch]
            digit = None
        elif ch in BIG:
            section += digit or 0
            total += (section or 1) * BIG[ch]
            section, digit = 0, None
        else:
            return None
    return total + section + (digit or 0)


def native(text):
    for word, value in sorted(NATIVE_TENS.items(), key=lambda kv: -len(kv[0])):
        if text.startswith(word):
            rest = text[len(word):]
            if not rest:
                return value
            ones = NATIVE_ONES.get(rest)
            return value + ones if ones else None
    return NATIVE_ONES.get(text)


def interpretations(number_text, unit):
    """Every value and digit string the spoken number text can legitimately mean."""
    values, strings = set(), set()
    value = sino(number_text)
    if value is not None:
        values.add(value)
    for cut in range(1, len(number_text)):              # Sino hundreds + native tail
        head, tail = number_text[:cut], number_text[cut:]
        if head[-1] in SMALL or head[-1] in BIG:
            head_value, tail_value = sino(head), native(tail)
            if head_value is not None and tail_value is not None:
                values.add(head_value + tail_value)
    if native(number_text) is not None:
        values.add(native(number_text))
    digits = tokenize(number_text, DIGIT_WORDS)
    if digits:
        strings.add("".join(DIGIT_WORDS[d] for d in digits))
    if unit.startswith("월") and number_text in ("유", "시"):   # 유월, 시월(생·쯤·분)
        values.add(6 if number_text == "유" else 10)
    if number_text.endswith("째"):                              # 셋째 주, 첫째 날
        base = number_text[:-1]
        ordinal = 1 if base == "첫" else native(base)
        if ordinal:
            values.add(ordinal)
    if unit.startswith(("번째", "째")) and number_text == "첫":
        values.add(1)
    return values, strings


class Validator:
    def __init__(self):
        self.stats = collections.Counter()

    def aligned(self, span, raw):
        """The span's bytes are its written form and do not cut a number short."""
        begin, end = span.get("begin"), span.get("end")
        if begin is None or end is None:
            return False
        written = (span.get("written") or "").strip().encode("utf-8")
        if raw[begin:end].strip() != written:
            return False
        before, after = raw[begin - 1:begin] if begin else b"", raw[end:end + 1]
        if after in (b",", b".") and raw[end + 1:end + 2].isdigit():
            return False                                 # 8 of 8,206명
        return not (after.isdigit() or after == b"-" or before.isdigit())

    def check_span(self, span, raw):
        written = (span.get("written") or "").strip()
        spoken = compact(span.get("spoken"))
        self.stats["spans"] += 1
        if not self.aligned(span, raw):
            return None
        self.stats["spans_aligned"] += 1
        for word in HANGUL_WORD.findall(written):
            start = written.index(word)
            if start and written[start - 1].isdigit():
                continue                                 # a counter or unit after a number
            if word not in spoken:
                return "korean-text-edited", "%s → %s" % (written, span.get("spoken"))
        match = INTEGER.fullmatch(written)
        if match:
            digits, unit = match.group(1).replace(",", ""), match.group(2)
            if unit and not spoken.endswith(unit):
                return None                              # unit rewritten: not judged
            number_text = spoken[:len(spoken) - len(unit)] if unit else spoken
            if not number_text:
                return None
            self.stats["spans_value_judged"] += 1
            values, strings = interpretations(number_text, unit)
            if int(digits) in values or digits in strings:
                return None
            if tokenize(number_text, set(ENGLISH_NUMBERS) | set(DIGIT_WORDS)):
                return None                              # English-style reading
            if any(ch not in NUMBER_SYLLABLES for ch in number_text):
                return "non-number-syllable", "%s → %s" % (written, span.get("spoken"))
            return "value-mismatch", "%s → %s" % (written, span.get("spoken"))
        if DASHED.fullmatch(written):
            words = tokenize(spoken, set(DIGIT_WORDS) | set(SEPARATORS))
            if not words:
                return None                              # a group read as a number
            self.stats["spans_value_judged"] += 1
            read = "".join(DIGIT_WORDS[w] for w in words if w in DIGIT_WORDS)
            if read != written.replace("-", ""):
                return "identifier-digits", "%s → %s" % (written, span.get("spoken"))
        return None

    @staticmethod
    def check_literal(record):
        """Korean prose outside the annotated spans that the reference rewrote."""
        raw = (record.get("text") or "").encode("utf-8")
        pieces, at = [], 0
        for span in sorted(record.get("spans") or [], key=lambda s: s.get("begin") or -1):
            begin, end = span.get("begin"), span.get("end")
            if begin is None or end is None or begin < at:
                return None                              # unaligned: not judged
            pieces += [raw[at:begin].decode("utf-8", "replace"), span.get("spoken") or ""]
            at = end
        pieces.append(raw[at:].decode("utf-8", "replace"))
        built, reference = compact("".join(pieces)), compact(record.get("expected"))
        if built == reference:
            return None
        matcher = difflib.SequenceMatcher(None, built, reference, autojunk=False)
        for op, i1, i2, j1, j2 in matcher.get_opcodes():
            source, target = built[i1:i2], reference[j1:j2]
            if op == "equal" or re.search(r"[A-Za-z0-9]", source):
                continue                                 # unannotated conversion (PIN → 핀)
            if not re.search(r"[가-힣]", source + target):
                continue                                 # punctuation only
            if re.fullmatch(r"[가-힣]+", source or "가") and re.fullmatch(r"[가-힣]*", target):
                return "korean-text-edited", "%s → %s" % (source or "∅", target or "∅")
        return None

    @staticmethod
    def literal_judged(record):
        """check_literal could align every span to its source bytes."""
        at = 0
        for span in sorted(record.get("spans") or [], key=lambda s: s.get("begin") or -1):
            begin, end = span.get("begin"), span.get("end")
            if begin is None or end is None or begin < at:
                return False
            at = end
        return True

    @staticmethod
    def check_text(record):
        """Korean prose the reference dropped, added or changed, without span offsets (v2).

        Every Hangul syllable of the source must reach the reference in order. Digits,
        Latin, Han, symbols and bracketed one-syllable abbreviations are variable: any
        reading may stand there, and so may text right next to them. A particle right
        after a variable stretch may take the form its reading needs (이에요/에요)."""
        text = TEXT_PUNCT.sub("", TEXT_ABBREV.sub(VARIABLE, record.get("text") or ""))
        fixed = re.sub(VARIABLE + "+", VARIABLE,
                       "".join(ch if "가" <= ch <= "힣" else VARIABLE for ch in text))
        reference = TEXT_PUNCT.sub("", TEXT_ABBREV.sub("", record.get("expected") or ""))
        pattern = "".join("(.*?)" if ch == VARIABLE else re.escape(ch) for ch in fixed)
        if re.fullmatch(pattern, reference, re.S):
            return None
        matcher = difflib.SequenceMatcher(None, fixed, reference, autojunk=False)
        for op, i1, i2, j1, j2 in matcher.get_opcodes():
            source, target = fixed[i1:i2], reference[j1:j2]
            if op == "equal":
                continue
            if VARIABLE in source:
                # A reading replaces the variable stretch; the Korean on its edges must
                # survive (2%를 → 이 퍼센트 drops 를). Korean between two variable
                # stretches belongs to the reading (10만~20만 → 일이십만).
                lead, tail = source.split(VARIABLE)[0], source.split(VARIABLE)[-1]
                if lead and not target.startswith(lead):
                    return "korean-text-edited", "%s → %s" % (source.replace(VARIABLE, "…"), target or "∅")
                if tail and not target.endswith(tail) and tail not in ("이", "으") and not any(
                        target.endswith(form) for pair in ALLOMORPHS if tail in pair for form in pair):
                    return "korean-text-edited", "%s → %s" % (source.replace(VARIABLE, "…"), target or "∅")
                continue
            before = i1 > 0 and fixed[i1 - 1] == VARIABLE
            after = i2 < len(fixed) and fixed[i2] == VARIABLE
            if before and (frozenset((source, target)) in ALLOMORPHS or
                           (source in ("이", "으") and not target) or (target == "으" and not source)):
                continue                                 # 하나에요, 칠로 / 칠으로
            if op == "insert" and (before or after):
                continue                                 # the reading of the variable stretch
            return "korean-text-edited", "%s → %s" % (source or "∅", target or "∅")
        return None

    def check_record(self, record):
        if record.get("expected") is None:
            return None
        if record.get("category_en") in NOT_JUDGED_CATEGORIES:
            self.stats["not_judged_category"] += 1
            return None
        self.stats["records"] += 1
        raw = (record.get("text") or "").encode("utf-8")
        reasons = [v for v in (self.check_span(s, raw) for s in record.get("spans") or []) if v]
        literal = self.check_literal(record)
        if not literal and not self.literal_judged(record):
            self.stats["text_judged_unaligned"] += 1
            literal = self.check_text(record)
        if literal:
            reasons.append(literal)
        return reasons or None


def review_entries(import_dir, reviewed_at):
    validator, entries = Validator(), []
    counts = collections.Counter()
    with (Path(import_dir) / "records.jsonl").open(encoding="utf-8") as handle:
        for line in handle:
            record = json.loads(line)
            reasons = validator.check_record(record)
            if not reasons:
                continue
            counts.update(code for code, _ in reasons)
            entries.append(dict(
                id=record["id"], record_sha256=record["record_sha256"], status="excluded",
                reviewer="auto:" + VERSION, reviewed_at=reviewed_at,
                reason="; ".join("%s: %s" % reason for reason in reasons)))
    return entries, counts, validator.stats


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--import-dir", type=Path, required=True)
    parser.add_argument("--reviews-out", type=Path, required=True)
    parser.add_argument("--reviewed-at", default=datetime.date.today().isoformat())
    args = parser.parse_args(argv)
    if args.reviews_out.exists():
        parser.error("refusing to overwrite %s" % args.reviews_out)
    entries, counts, stats = review_entries(args.import_dir, args.reviewed_at)
    args.reviews_out.parent.mkdir(parents=True, exist_ok=True)
    args.reviews_out.write_text("".join(json.dumps(e, ensure_ascii=False) + "\n" for e in entries),
                                encoding="utf-8", newline="\n")
    json.dump(dict(validator=VERSION, excluded=len(entries), reasons=dict(counts),
                   records_judged=stats["records"], spans=stats["spans"],
                   spans_aligned=stats["spans_aligned"],
                   spans_value_judged=stats["spans_value_judged"],
                   not_judged_category_records=stats["not_judged_category"],
                   text_judged_unaligned_records=stats["text_judged_unaligned"]),
              sys.stdout, ensure_ascii=False, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
