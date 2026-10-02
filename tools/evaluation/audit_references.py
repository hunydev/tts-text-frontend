#!/usr/bin/env python3
"""Audit active reference coverage and known defects without running the frontend.

Exit 0 means the inventory and mechanical checks passed, not linguistic approval.
Exit 1 reports reference defects; exit 2 reports malformed/missing audit inputs.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

ACTIVE_JSONL = (
    "ko_jev_leaks2_v24",
    "ko_jev_leaks_v24",
    "ko_digit_codes_v24",
    "ko_list_break_v24",
    "ko_digit_groups_v24",
    "ko_number_labels_v24",
    "ko_alnum_models_v24",
    "ko_control_ignore_v24",
    "ko_unit_control_v24",
    "ko_score_math_v24",
    "ko_number_identifiers_v24",
    "ko_latin_joiners_v24",
    "ko_brand_lexicon_v24",
    "ko_unit_leaks_v24",
    "ko_apparel_size_v24",
    "ko_range_reading_v24",
    "unicode_fallback_v24",
    "matrix_language_v24",
    "en_number_dash_v23",
    "en_meridiem_v23",
    "ko_clothing_size_v23",
    "ko_number_letters_v23",
    "ko_speech_leaks_v23",
    "ko_vehicle_plate_v23",
    "ko_identifier_shapes_v23",
    "break_controls_v23",
    "ko_spacing_option_v23",
    "ko_reading_unit_spacing_v23",
    "en_street_title_v23",
    "en_caps_word_v23",
    "ko_covid_name_v23",
    "ko_password_label_v23",
    "ko_card_number_v23",
    "en_month_year_v23",
    "en_roman_numeral_v23",
    "ko_alnum_identifier_v23",
    "en_speech_hyphen_v23",
    "en_decade_zero_v23",
    "counted_noun_v22",
    "ko_spacing_v22",
    "phone_zero_v22",
    "identifier_fallback_v22",
    "lexical_coverage_v22",
    "counter_frequency_v22",
    "particle_table_v21", "calendar_alias_v21", "abbreviation_dotted_v21", "speech_character_width_v21", "grouped_controls_v22", "typed_controls_v22", "numeric_width_v22",
    "cases", "domain_cases_v8", "advanced_cases", "challenge_cases",
    "protected_cases", "content_v7", "finance_ranges_v7", "multiword_v8", "lexicon_v10",
    "scaled_currency_v12", "english_dates_v13", "english_forms_v14", "korean_counters_v15", "phone_cases_v18",
    "user_patterns_v20", "speech_final_gate_v21", "letters_v21", "electronic_v21", "math_hashtag_v21", "counter_band_v21", "floor_v21", "letters_paths_v21", "letter_number_sign_v21", "particle_v21", "mixed_case_v21", "phone_service_v21", "slash_letters_v21", "clock_v21", "slash_date_v21", "range_mark_v21", "multi_range_v21", "handle_v21", "year_cue_v21", "bracket_v21", "unknown_foreign_v21", "decorative_v21", "initialism_v21", "code_word_v21", "letter_label_v21", "oov_guess_v21", "oov_guess_regression_v21",
)
CLASSIFICATIONS = {
    "reference-policy", "provisional-pronunciation", "preservation-contract", "needs-review",
}
REVIEW_STATUS = "pending-independent-review"
MONTH = re.compile(r"(?<![가-힣])(?:유|시)[ \t]+월(?![가-힣])")
ZERO_CLOCK = re.compile(r"\bzero\s+o['’]clock\b", re.IGNORECASE)
AGAIN_RANGE = re.compile(
    r"(?:[영공일이삼사오육칠팔구십백천만억조경해자양간정]|원|달러|센트|그램|퍼센트|포인트)"
    r"\s+다시\s+(?:마이너스\s+|플러스\s+)?[영공일이삼사오육칠팔구십백천만억조경해자양간정]"
)
RANGE_SURFACE = re.compile(r"(?<![\d-])\d+(?:[.,]\d+)?(?:[가-힣A-Za-z%₩$]+)?\s*[-~∼～–—]\s*\d+(?![\d-])")
KNOWN_BAD_PARTICLES = {
    "content-v7-thumbnail": "섬네일를", "content-v7-timeline": "타임라인를",
    "content-v7-keyframe": "키프레임를", "content-v7-codec": "코덱를",
    "content-v7-livestream": "라이브스트림를", "content-v7-photoshop": "포토샵를",
    "content-v7-capcut": "캡컷를",
}


class AuditError(ValueError):
    pass


def unescape(value: str) -> str:
    # Match the native golden reader: backslash consumes the next character.
    return re.sub(r"\\(.)", lambda m: {"n": "\n", "r": "\r", "t": "\t"}.get(m[1], m[1]), value)


def read_cases(path: Path, kind: str) -> list[dict]:
    cases, seen = [], set()
    occurrences = Counter()
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip() or (kind == "golden-tsv" and line.startswith("#")):
            continue
        if kind == "jsonl":
            try:
                case = json.loads(line)
            except json.JSONDecodeError as error:
                raise AuditError(f"{path}:{line_number}: malformed JSON") from error
            if not isinstance(case, dict) or not all(isinstance(case.get(k), str) for k in ("id", "text", "expected")):
                raise AuditError(f"{path}:{line_number}: id/text/expected must be strings")
        elif kind == "golden-tsv":
            if line.count("\t") != 1:
                raise AuditError(f"{path}:{line_number}: golden fixture needs two TSV columns")
            text, expected = map(unescape, line.split("\t"))
            fingerprint = hashlib.sha256(text.encode()).hexdigest()[:16]
            occurrences[fingerprint] += 1
            case = dict(id=f"golden-{fingerprint}-{occurrences[fingerprint]}", text=text, expected=expected,
                        category=f"golden/{path.parent.name}")
        else:
            raise AuditError(f"{path}: unsupported format {kind!r}")
        if not case["id"] or case["id"] in seen:
            raise AuditError(f"{path}:{line_number}: missing or duplicate case id")
        if "\x00" in case["text"] or "\x00" in case["expected"]:
            raise AuditError(f"{path}:{line_number}: embedded NUL")
        seen.add(case["id"])
        cases.append(dict(case, line=line_number))
    if not cases:
        raise AuditError(f"{path}: empty active corpus")
    return cases


def validate_decision(value: object, where: str) -> dict:
    if not isinstance(value, dict):
        raise AuditError(f"{where}: review decision must be an object")
    if value.get("classification") not in CLASSIFICATIONS:
        raise AuditError(f"{where}: unknown reference classification")
    if value.get("review_status") != REVIEW_STATUS:
        raise AuditError(f"{where}: this developer audit cannot supply independent human approval")
    for field in ("reason", "reference_source"):
        if not isinstance(value.get(field), str) or not value[field].strip():
            raise AuditError(f"{where}: missing {field}")
    return value


def known_defects(case: dict) -> list[tuple[str, str]]:
    expected, original = case["expected"], case["text"]
    findings = []
    # Only expected rendering is inspected; a diagnostic sentence may contain
    # the ordinary Korean adverb 다시 without using it to verbalize an interval.
    explicit = "range" in case.get("id", "").lower() or "range" in case.get("category", "").lower()
    explicit |= any(span.get("semantic_type") == "RANGE" for span in case.get("expected_spans", []))
    # A complete, explicitly typed postal address has address-number separators,
    # not intervals. Keep the interval check for plain input, range metadata,
    # tildes and every mixed address-plus-prose case (no whole-case wildcard).
    address = re.fullmatch(r"\[\[([^\[\]{}\\]+)\]\]\{@address\}", original)
    address_only = (case.get("input_format") == "annotated" and address is not None
                    and not re.search(r"[~∼～–—]", address.group(1)))
    # Likewise a hyphen inside an explicitly typed financial identifier separates
    # digit groups; its 다시 reading is the identifier policy, not an interval.
    # Every range-shaped surface must be covered by such a typed span; explicit
    # range ids/categories/RANGE spans are still checked.
    # A declared rejection (TEXT with syntax.invalid-range/phone/date) is the
    # engine refusing the interval; speech then reads the identifier grammar.
    identifiers = [span.get("surface", "") for span in case.get("expected_spans", [])
                   if (span.get("semantic_type") == "DIGIT_SEQUENCE"
                       and span.get("unit") == "financial_identifier")
                   or (span.get("semantic_type") == "TEXT" and span.get("rule") in
                       ("syntax.invalid-range", "syntax.invalid-phone", "syntax.invalid-date"))]
    surfaces = [match.group(0).strip() for match in RANGE_SURFACE.finditer(original)]
    identifier_only = bool(surfaces) and all(
        any(surface in identifier for identifier in identifiers) for surface in surfaces)
    if (explicit or (not address_only and not identifier_only and RANGE_SURFACE.search(original))) \
            and AGAIN_RANGE.search(expected):
        findings.append(("korean-range-again", "A numeric interval is rendered with 다시; review the interval policy instead of approving this reference."))
    if MONTH.search(expected):
        findings.append(("split-korean-month", "The June/October month word is split as 유 월 or 시 월; review 육/유 and 십/시 month reading and spacing."))
    if ZERO_CLOCK.search(expected):
        findings.append(("english-zero-oclock", "zero o'clock is not the approved natural English time rendering; review midnight/12-hour policy."))
    if KNOWN_BAD_PARTICLES.get(case["id"], "\0") in expected:
        findings.append(("known-wrong-korean-particle", "This audited fixture retains 를 after a consonant-final Korean name; correct the source/reference pair or label an explicit malformed-input preservation test."))
    return findings


def audit(root: Path, inventory_path: Path | None = None) -> dict:
    root = root.resolve()
    inventory_path = inventory_path or root / "tests/evaluation/reference_reviews.json"
    try:
        inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise AuditError("Malformed reference inventory") from error
    if inventory.get("version") != 1 or inventory.get("human_approved_cases") != 0:
        raise AuditError("Unsupported audit inventory version or unsupported automatic human approval")
    specs = inventory.get("files")
    if not isinstance(specs, list):
        raise AuditError("Inventory files must be a list")
    required = {f"tests/evaluation/{name}.jsonl" for name in ACTIVE_JSONL}
    required |= {p.relative_to(root).as_posix() for p in (root / "tests/golden").glob("*/cases.tsv")}
    required_golden = {f"tests/golden/{name}/cases.tsv" for name in
                       ("ko", "en", "code-mixed", "foreign-pronunciation", "streaming", "domain", "customer")}
    required |= required_golden
    registered = [spec.get("path") for spec in specs if isinstance(spec, dict)]
    if len(registered) != len(specs) or len(set(registered)) != len(registered) or set(registered) != required:
        raise AuditError(f"Active inventory coverage mismatch: missing={sorted(required-set(registered))}; extra={sorted(set(registered)-required)}")
    files, records, findings = [], [], []
    for spec in specs:
        relative = spec["path"]
        source = root / relative
        if not source.resolve().is_relative_to(root):
            raise AuditError(f"Corpus escapes repository: {relative}")
        default = validate_decision(spec.get("default"), relative)
        category_overrides = spec.get("category_overrides", {})
        overrides = spec.get("case_overrides", {})
        if not isinstance(category_overrides, dict) or not isinstance(overrides, dict):
            raise AuditError(f"{relative}: overrides must be mappings")
        for key, value in {**category_overrides, **overrides}.items():
            validate_decision(value, f"{relative}:{key}")
        cases = read_cases(source, spec.get("format"))
        identifiers, categories = {case["id"] for case in cases}, {case.get("category", "") for case in cases}
        if set(overrides)-identifiers or set(category_overrides)-categories:
            raise AuditError(f"{relative}: stale or misspelled case/category review override")
        counts = Counter()
        for case in cases:
            decision = overrides.get(case["id"], category_overrides.get(case.get("category", ""), default))
            record = {"path": relative, "id": case["id"], "line": case["line"], **decision}
            records.append(record)
            counts[decision["classification"]] += 1
            for code, message in known_defects(case):
                findings.append(dict(path=relative, id=case["id"], line=case["line"], code=code,
                                     severity="error", message=message, text=case["text"], expected=case["expected"]))
        files.append(dict(path=relative, cases=len(cases), classifications=dict(sorted(counts.items())),
                          review_status=REVIEW_STATUS, human_approved_cases=0,
                          source_sha256=hashlib.sha256(source.read_bytes()).hexdigest()))
    all_counts = Counter(case["classification"] for case in records)
    return {"version": 1, "summary": {"files": len(files), "cases": len(records),
             "classifications": dict(sorted(all_counts.items())), "pending_independent_review": len(records),
             "human_approved_cases": 0, "reference_defects": len(findings), "structural_passed": True,
             "known_defect_checks_passed": not findings, "linguistic_accuracy": None},
            "policy_note": "Structural success and engine agreement are not independent linguistic approval. Every current case remains pending independent review.",
            "inventory_sha256": hashlib.sha256(inventory_path.read_bytes()).hexdigest(),
            "files": files, "cases": records, "findings": findings}


def self_test() -> None:
    samples = [
        (dict(id="money-range", text="가격은 1~2만원입니다.", expected="가격은 만 다시 이만 원입니다."), {"korean-range-again"}),
        (dict(id="check-again", text="금액은 1만원입니다. 다시 확인하세요.", expected="금액은 만 원입니다. 다시 확인하세요."), set()),
        (dict(id="money-range", text="1~2만 원", expected="만 원에서 이만 원"), set()),
        (dict(id="address", input_format="annotated", text="[[서울시 12-3]]{@address}", expected="서울시 십이 다시 삼"), set()),
        (dict(id="address", input_format="plain", text="[[서울시 12-3]]{@address}", expected="서울시 십이 다시 삼"), {"korean-range-again"}),
        (dict(id="money-range", input_format="annotated", text="[[서울시 12-3]]{@address}", expected="서울시 십이 다시 삼"), {"korean-range-again"}),
        (dict(id="mixed", input_format="annotated", text="[[서울시 12-3]]{@address} 1~2원", expected="서울시 십이 다시 삼 일 다시 이 원"), {"korean-range-again"}),
        (dict(id="address", input_format="annotated", text="[[서울시 12~3]]{@address}", expected="서울시 십이 다시 삼"), {"korean-range-again"}),
        (dict(id="account", text="계좌는 010-001입니다.", expected="계좌는 공일공 다시 공공일입니다.",
              expected_spans=[dict(surface="010-001", semantic_type="DIGIT_SEQUENCE", unit="financial_identifier")]), set()),
        (dict(id="account", text="계좌는 010-001입니다.", expected="계좌는 공일공 다시 공공일입니다."),
         {"korean-range-again"}),
        (dict(id="account", text="3765-38264로 입금", expected="삼칠육오 다시 삼팔이육사로 입금",
              expected_spans=[dict(surface="3765-38264", semantic_type="TEXT", rule="syntax.invalid-range")]), set()),
        (dict(id="account", text="3765-38264로 입금", expected="삼칠육오 다시 삼팔이육사로 입금",
              expected_spans=[dict(surface="3765-38264", semantic_type="TEXT", rule="identity")]),
         {"korean-range-again"}),
        (dict(id="account-range", text="계좌는 010-001입니다.", expected="계좌는 공일공 다시 공공일입니다.",
              expected_spans=[dict(surface="010-001", semantic_type="DIGIT_SEQUENCE", unit="financial_identifier")]), {"korean-range-again"}),
        (dict(id="account", text="계좌는 010-001, 기간은 3-5일", expected="계좌는 공일공 다시 공공일, 기간은 삼 다시 오 일",
              expected_spans=[dict(surface="010-001", semantic_type="DIGIT_SEQUENCE", unit="financial_identifier")]), {"korean-range-again"}),
        (dict(id="account", text="계좌는 010-001입니다.", expected="계좌는 공일공 다시 공공일입니다.",
              expected_spans=[dict(surface="010-001", semantic_type="DIGIT_SEQUENCE", unit="leading_zero")]),
         {"korean-range-again"}),
        (dict(id="month", text="2026-06-10", expected="이천이십육 년 유 월 십 일"), {"split-korean-month"}),
        (dict(id="month", text="2026-10-16", expected="이천이십육 년 시 월 십육 일"), {"split-korean-month"}),
        (dict(id="month", text="2026-10-16", expected="이천이십육 년 시월 십육 일"), set()),
        (dict(id="midnight", text="00:00", expected="zero o'clock"), {"english-zero-oclock"}),
        (dict(id="midnight", text="00:00", expected="twelve o'clock"), set()),
        (dict(id="content-v7-thumbnail", text="thumbnail를 소개합니다.", expected="섬네일를 소개합니다."), {"known-wrong-korean-particle"}),
    ]
    for sample, wanted in samples:
        actual = {code for code, _ in known_defects(sample)}
        if actual != wanted:
            raise AuditError(f"Reference audit self-test failed: {sample!r}, {actual!r}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--inventory", type=Path)
    parser.add_argument("--output", type=Path, help="Also write JSON to a new output file")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            self_test()
        report = audit(args.root, args.inventory)
        payload = json.dumps(report, ensure_ascii=False, indent=2) + "\n"
        if args.output:
            with args.output.open("x", encoding="utf-8") as target:
                target.write(payload)
        print(payload, end="")
        return 1 if report["findings"] else 0
    except (AuditError, OSError, UnicodeError, TypeError, KeyError) as error:
        print(json.dumps({"version": 1, "error": str(error)}, ensure_ascii=False), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
