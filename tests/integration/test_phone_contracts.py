#!/usr/bin/env python3
"""Independent telephone/identifier text and IR regression contracts.

References implement the explicit product policy (digit reading, KO 공 / EN
zero, plus, extension, group spacing), not a claim of one universal spoken form.
Number allocation/reachability is deliberately not inferred by this test.
NANPA reserves 555-0100..0199 for fictitious entertainment numbers:
https://nanpa.com/numbering/555-line-numbers
The international digit ceiling follows ITU-T E.164; extension is separate:
https://www.itu.int/rec/T-REC-E.164
"""
import argparse
import importlib.util
import json
import random
import subprocess
import tempfile
from pathlib import Path

from test_english_dates import exact_span, stream

ROOT = Path(__file__).resolve().parents[2]
EN_DIGITS = ("zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine")
KO_DIGITS = tuple("공일이삼사오육칠팔구")
PROFILES = ((),) + tuple(("--first-target", str(n), "--normal-target", str(n),
                         "--max-length", str(2 * n)) for n in range(1, 5))


def spoken_phone(value, extension, language):
    """Render the independently specified value; never parse candidate output."""
    digits = EN_DIGITS if language == "en-US" else KO_DIGITS
    joiner = " " if language == "en-US" else ""
    prefix = ("plus " if language == "en-US" else "플러스 ") if value.startswith("+") else ""
    groups = value.lstrip("+").split("-")
    result = prefix + " ".join(joiner.join(digits[int(c)] for c in group) for group in groups)
    if extension:
        result += (" extension " if language == "en-US" else " 내선 ")
        result += joiner.join(digits[int(c)] for c in extension)
    return result


def phone(case_id, language, surface, value, unit, extension="", prefix="", suffix=""):
    return dict(id=case_id, category="phone/international" if unit == "international" else "phone/national",
                text=prefix + surface + suffix,
                expected=prefix + spoken_phone(value, extension, language) + suffix,
                default_language=language,
                expected_spans=[dict(surface=surface, semantic_type="PHONE", value=value,
                                     secondary=extension, unit=unit, source_lang="und",
                                     context_lang=language)])


REFERENCES = [
    phone("en-international-spaces", "en-US", "+1 212 555 0100", "+1-212-555-0100", "international",
          prefix="Call me at ", suffix="."),
    phone("en-international-hyphens", "en-US", "+1-212-555-0100", "+1-212-555-0100", "international",
          prefix="Phone: ", suffix="."),
    phone("en-international-compact", "en-US", "+12125550100", "+12125550100", "international",
          prefix="Phone: "),
    phone("en-international-parentheses", "en-US", "+1 (212) 555-0100", "+1-212-555-0100", "international",
          prefix="Call ", suffix="."),
    phone("en-international-tabs", "en-US", "+1\t212\t555\t0100", "+1-212-555-0100", "international"),
    phone("en-national-parentheses", "en-US", "(212) 555-0100", "212-555-0100", "national"),
    phone("en-national-compact-labeled", "en-US", "2125550100", "2125550100", "national", prefix="Phone: "),
    phone("en-national-spaced-labeled", "en-US", "212 555 0100", "212-555-0100", "national", prefix="Call "),
    phone("en-legacy-phone", "en-US", "212-555-0100", "212-555-0100", ""),
    phone("en-legacy-with-extension", "en-US", "212-555-0100 ext. 0042", "212-555-0100",
          "national", "0042"),
    phone("en-extension-period", "en-US", "+1 (212) 555-0100 ext. 0042", "+1-212-555-0100", "international",
          "0042", prefix="Call ", suffix="."),
    phone("en-extension-word", "en-US", "+1 212 555 0100 ext 0042", "+1-212-555-0100", "international", "0042"),
    phone("en-extension-x", "en-US", "+12125550100 x0042", "+12125550100", "international", "0042", prefix="Phone: "),
    phone("en-extension-eight-digits", "en-US", "+12125550100 ext. 00000001", "+12125550100",
          "international", "00000001"),
    phone("ko-international-spaces", "ko-KR", "+82 10-1234-5678", "+82-10-1234-5678", "international",
          prefix="문의는 ", suffix="입니다."),
    phone("ko-international-hyphens", "ko-KR", "+82-2-123-4567", "+82-2-123-4567", "international",
          prefix="전화: "),
    phone("ko-international-compact", "ko-KR", "+821012345678", "+821012345678", "international", prefix="전화: "),
    phone("ko-international-parentheses", "ko-KR", "+82 (2) 123-4567", "+82-2-123-4567", "international"),
    phone("ko-national-compact-labeled", "ko-KR", "01012345678", "01012345678", "national", prefix="전화: "),
    phone("ko-national-spaced-labeled", "ko-KR", "010 1234 5678", "010-1234-5678", "national", prefix="연락처: "),
    phone("ko-service-number-labeled", "ko-KR", "1588-0000", "1588-0000", "national", prefix="문의: "),
    phone("ko-legacy-phone", "ko-KR", "010-1234-5678", "010-1234-5678", ""),
    phone("ko-legacy-short-area", "ko-KR", "02-123-4567", "02-123-4567", "", prefix="전화: "),
    phone("ko-legacy-with-extension", "ko-KR", "02-123-4567 내선 0042", "02-123-4567",
          "national", "0042", prefix="전화: "),
    phone("ko-extension", "ko-KR", "+82 2-123-4567 내선 0042", "+82-2-123-4567", "international",
          "0042", prefix="전화: ", suffix="."),
    phone("en-minimum-international", "en-US", "+1234567", "+1234567", "international", prefix="Phone: "),
    phone("en-maximum-international", "en-US", "+123456789012345", "+123456789012345", "international", prefix="Phone: "),
    # Shape normalization is not country-code allocation validation.
    phone("en-leading-zero-shape", "en-US", "+0000001", "+0000001", "international", prefix="Phone: "),
]

GUARDS = [
    dict(id="ko-signed-currency", category="phone/negative", default_language="ko-KR",
         text="+82원", expected="플러스 팔십이 원", expected_spans=[
             dict(surface="+82원", semantic_type="CURRENCY", value="+82", unit="KRW")]),
    dict(id="ko-long-signed-currency", category="phone/negative", default_language="ko-KR",
         text="+10000000원", expected="플러스 천만 원", expected_spans=[
             dict(surface="+10000000원", semantic_type="CURRENCY", value="+10000000", unit="KRW")]),
    dict(id="ko-amount-range", category="phone/negative", default_language="ko-KR",
         text="1~2원", expected="일 원에서 이 원", expected_spans=[
             dict(surface="1~2원", semantic_type="RANGE", value="1", secondary="2", unit="KRW")]),
    dict(id="en-generic-range", category="phone/negative", default_language="en-US",
         text="3-5", expected="three to five", expected_spans=[
             dict(surface="3-5", semantic_type="RANGE", value="3", secondary="5")]),
    # A declared service prefix is an explicit cue (2026-09-23); an undeclared
    # pair such as 1500-1600 remains a range.
    dict(id="ko-unlabeled-two-group-range", category="phone/national", default_language="ko-KR",
         text="1588-0000", expected="일오팔팔 공공공공", expected_spans=[
             dict(surface="1588-0000", semantic_type="PHONE", value="1588-0000", unit="national")]),
    dict(id="ko-calendar", category="phone/negative", default_language="ko-KR",
         text="2026-09-17", expected="이천이십육 년 구 월 십칠 일", expected_spans=[
             dict(surface="2026-09-17", semantic_type="DATE", value="2026-09-17")]),
    dict(id="en-ip-preserved", category="phone/negative", default_language="en-US",
         text="192.0.2.10", expected="192.0.2.10", expected_spans=[
             dict(surface="192.0.2.10", semantic_type="IP_ADDRESS")]),
    dict(id="en-unlabeled-compact", category="phone/negative", default_language="en-US",
         text="1000001", expected="one million one", expected_spans=[
             dict(surface="1000001", semantic_type="CARDINAL", value="1000001")]),
    dict(id="en-unlabeled-spaces", category="phone/negative", default_language="en-US",
         text="212 555 0100", expected="two hundred twelve five hundred fifty five zero one zero zero",
         expected_spans=[dict(surface="212", semantic_type="CARDINAL", value="212"),
                         dict(surface="555", semantic_type="CARDINAL", value="555"),
                         dict(surface="0100", semantic_type="DIGIT_SEQUENCE", value="0100", unit="leading_zero")]),
    dict(id="en-code-preserved", category="phone/negative", default_language="en-US",
         text="`+12125550100 ext. 0042`", expected="`+12125550100 ext. 0042`", expected_spans=[]),
    dict(id="en-short-signed-number", category="phone/negative", default_language="en-US",
         text="+123456", expected="plus one hundred twenty three thousand four hundred fifty six",
         expected_spans=[dict(surface="+123456", semantic_type="CARDINAL", value="+123456")]),
    dict(id="en-large-signed-number", category="phone/negative", default_language="en-US",
         text="+1250000", expected="plus one million two hundred fifty thousand",
         expected_spans=[dict(surface="+1250000", semantic_type="CARDINAL", value="+1250000")]),
    dict(id="en-phone-label-newline", category="phone/negative", default_language="en-US",
         text="Phone:\n1000001", expected="Phone:\none million one", expected_spans=[
             dict(surface="1000001", semantic_type="CARDINAL", value="1000001")]),
    dict(id="ko-financial-identifier", category="phone/identifier", default_language="ko-KR",
         text="거래번호: 010-1234-5678", expected="거래번호: 공일공 다시 일이삼사 다시 오육칠팔",
         expected_spans=[dict(surface="010-1234-5678", semantic_type="DIGIT_SEQUENCE",
                             value="010-1234-5678", secondary="거래번호", unit="financial_identifier")]),
    dict(id="en-financial-identifier", category="phone/identifier", default_language="en-US",
         text="Account: 212-555-0100", expected="Account: two one two five five five zero one zero zero",
         expected_spans=[dict(surface="212-555-0100", semantic_type="DIGIT_SEQUENCE",
                             value="212-555-0100", secondary="account", unit="financial_identifier")]),
    dict(id="en-zero-identifier", category="phone/identifier", default_language="en-US",
         text="Account number: 00001234", expected="Account number: zero zero zero zero one two three four",
         expected_spans=[dict(surface="00001234", semantic_type="DIGIT_SEQUENCE",
                             value="00001234", secondary="account number", unit="financial_identifier")]),
]

MALFORMED = []
for case_id, surface, prefix in (
    ("short-grouped", "+123 456", ""),
    ("short-labeled", "+123456", "Phone: "),
    ("too-long", "+1234567890123456", "Phone: "),
    ("repeated-separator", "+1--212-555-0100", ""),
    ("open-parenthesis", "+1 (212 555-0100", ""),
    ("nested-parentheses", "+1 ((212)) 555-0100", ""),
    ("empty-extension", "+12125550100 ext.", ""),
    ("too-long-extension", "+12125550100 ext. 000000001", ""),
    ("unknown-attached-tail", "+12125550100xyz", "Phone: "),
    ("unsupported-nbsp", "+1\u00a0212\u00a0555\u00a00100", "Phone: "),
):
    MALFORMED.append(dict(id="malformed-" + case_id, category="phone/negative-malformed",
                          default_language="en-US", text=prefix + surface, expected=prefix + surface,
                          expected_spans=[dict(surface=surface, semantic_type="TEXT",
                                               value="", secondary="", unit="")]))

REFERENCES.append(dict(
    id="extension-newline-boundary", category="phone/international", default_language="en-US",
    text="Phone: +12125550100\next. 0042",
    expected="Phone: " + spoken_phone("+12125550100", "", "en-US") + "\next. zero zero four two",
    expected_spans=[dict(surface="+12125550100", semantic_type="PHONE", value="+12125550100",
                         secondary="", unit="international", source_lang="und", context_lang="en-US"),
                    dict(surface="0042", semantic_type="DIGIT_SEQUENCE", value="0042", unit="leading_zero")]))


def verify(result, row):
    assert result["text"] == row["expected"], (row["id"], row["text"], row["expected"], result)
    for reference in row["expected_spans"]:
        span = exact_span(result, row["text"], reference["surface"])
        for key, value in reference.items():
            if key != "surface":
                assert span[key] == value, (row["id"], key, value, span)
        if reference["semantic_type"] == "PHONE":
            # Numeric evidence is independent of a pronunciation string. Every
            # source digit, including extension zeros, must survive the IR.
            original = "".join(c for c in reference["surface"] if "0" <= c <= "9")
            actual = "".join(c for c in span["value"] if "0" <= c <= "9") + span["secondary"]
            assert original == actual, (row["id"], original, actual)
    if row["category"].startswith("phone/negative") or row["category"] == "phone/identifier":
        assert all(span["semantic_type"] != "PHONE" for span in result["spans"]), (row["id"], result)


def native_checks(args, rows):
    spec = importlib.util.spec_from_file_location("phone_native", ROOT / "tools/web/native.py")
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    count = 0
    for row in rows:
        for tiny in (False, True):
            options = {"default_language": row["default_language"]}
            if tiny:
                options.update(first_target=1, normal_target=1, max_length=2)
            engine = native.Engine(args.library, args.resources, options)
            try:
                source = row["text"].encode()
                batch = engine.normalize(source)
                verify(batch, row)
                for split in range(1, len(source)):
                    streamed = native.flatten(engine.stream(source, [split, len(source) - split]))
                    assert streamed == batch, (row["id"], tiny, split, batch, streamed)
                    count += 1
                rng = random.Random(18000)
                for _ in range(5):
                    schedule = [rng.randint(1, 9) for _ in source]
                    streamed = native.flatten(engine.stream(source, schedule))
                    assert streamed == batch, (row["id"], tiny, schedule, batch, streamed)
                    count += 1
            finally:
                engine.close()
    return count


def customization_checks(args):
    checks = 0
    with tempfile.TemporaryDirectory(prefix="ttf-phone-policy-") as directory:
        customer, domain = Path(directory) / "customer.tsv", Path(directory) / "domain.tsv"

        def overlaid(row, rows, domain_rows=""):
            nonlocal checks
            customer.write_text(rows, encoding="utf-8")
            overlay = ("--customer", str(customer))
            if domain_rows:
                domain.write_text(domain_rows, encoding="utf-8")
                overlay = ("--domain", str(domain), *overlay)
            for profile in (PROFILES[0], PROFILES[1], PROFILES[-1]):
                previous = None
                for chunk in (1, 7, 4096):
                    result = stream(args.cli, args.resources, row["text"], row["default_language"],
                                    chunk, profile, overlay)
                    verify(result, row)
                    if previous is not None:
                        assert result == previous, (row["id"], result, previous)
                    previous = result
                    checks += 1

        disabled = dict(id="disabled-phone-label", category="phone/negative", default_language="en-US",
                        text="Phone: 1000001", expected="Phone: one million one", expected_spans=[
                            dict(surface="1000001", semantic_type="CARDINAL", value="1000001")])
        overlaid(disabled, "phone_label\ten-US\tphone\t0\n")
        disabled_long_phone = dict(id="disabled-long-phone-label", category="phone/negative",
                                   default_language="en-US", text="Customer phone: 1000001.",
                                   expected="Customer phone: one million one.", expected_spans=[
                                       dict(surface="1000001", semantic_type="CARDINAL", value="1000001")])
        overlaid(disabled_long_phone, "phone_label\ten-US\tcustomer phone\t0\n")
        disabled_long_identifier = dict(id="disabled-long-identifier-label", category="phone/negative",
                                        default_language="en-US", text="Account number:0012",
                                        expected="Account number:zero zero one two", expected_spans=[
                                            dict(surface="0012", semantic_type="DIGIT_SEQUENCE", value="0012",
                                                 unit="leading_zero", secondary="")])
        # Disabling the cue must prevent financial_identifier, while the
        # user-selected plain leading-zero policy still retains every digit.
        overlaid(disabled_long_identifier, "identifier_label\ten-US\taccount\tdisabled\n"
                  "identifier_label\ten-US\taccount number\tdisabled\n"
                  "identifier_label\ten-US\tnumber\tdigits\n")
        # The same digit limits cover legacy hyphens, spaces and extensions.
        for case_id, surface, prefix, bound in (
            ("legacy-max", "202-555-0100", "", "national_max_digits\t8"),
            ("labeled-legacy-max", "202-555-0100", "Phone: ", "national_max_digits\t8"),
            ("legacy-extension-max", "202-555-0100 ext. 0042", "", "national_max_digits\t8"),
            ("labeled-spaces-max", "202 555 0100", "Phone: ", "national_max_digits\t8"),
            ("legacy-min", "202-555-0100", "", "national_min_digits\t11"),
        ):
            limited = dict(id=case_id, category="phone/negative", default_language="en-US",
                           text=prefix + surface, expected=prefix + surface, expected_spans=[
                               dict(surface=surface, semantic_type="TEXT", value="", secondary="", unit="")])
            overlaid(limited, "phone_policy\t*\t" + bound + "\n")
        lowered = phone("lowered-international-minimum", "en-US", "+123456", "+123456", "international",
                        prefix="Phone: ")
        overlaid(lowered, "phone_policy\t*\tinternational_min_digits\t6\n")
        for row_id, surface, policy in (
            ("lowered-international-maximum", "+12125550100", "phone_policy\t*\tinternational_max_digits\t10\n"),
            ("lowered-extension-maximum", "+12125550100 ext. 0042", "phone_policy\t*\textension_max_digits\t3\n"),
        ):
            row = dict(id=row_id, category="phone/negative-malformed", default_language="en-US",
                       text="Phone: " + surface, expected="Phone: " + surface, expected_spans=[
                           dict(surface=surface, semantic_type="TEXT", value="", secondary="", unit="")])
            overlaid(row, policy)
        custom_label = phone("customer-phone-label", "en-US", "1000001", "1000001", "national",
                             prefix="Hotline: ")
        overlaid(custom_label, "phone_label\ten-US\thotline\t1\n")
        custom_extension = phone("customer-extension-marker", "en-US", "+12125550100 desk 0042",
                                 "+12125550100", "international", "0042", prefix="Phone: ")
        overlaid(custom_extension, "phone_extension\ten-US\tdesk\t1\n")

        # Disabled marker controls only extension recognition. Do not invent a
        # new gold reading for the now ordinary trailing token/number sequence.
        for marker in ("x", "ext."):
            customer.write_text(f"phone_extension\ten-US\t{marker}\t0\n", encoding="utf-8")
            text = f"Phone: +12125550100 {marker} 0042"
            for profile in (PROFILES[0], PROFILES[1]):
                previous = None
                for chunk in (1, 7, 4096):
                    result = stream(args.cli, args.resources, text, "en-US", chunk, profile,
                                    ("--customer", str(customer)))
                    span = exact_span(result, text, "+12125550100")
                    assert (span["semantic_type"], span["value"], span["secondary"], span["unit"]) == (
                        "PHONE", "+12125550100", "", "international"), result
                    assert all(not item["secondary"] for item in result["spans"] if item["semantic_type"] == "PHONE"), result
                    if previous is not None:
                        assert result == previous, (marker, profile, chunk, result, previous)
                    previous = result
                    checks += 1

        custom_reading = phone("customer-phone-reading", "en-US", "+1 212 555 0100 ext. 0042",
                               "+1-212-555-0100", "international", "0042", prefix="Phone: ")
        custom_reading["expected"] = ("Phone: positive one / two one two / five five five / "
                                      "oh one oh oh extension number oh oh four two")
        overlaid(custom_reading, "phone_render\ten-US\tplus\tpositive \n"
                                "phone_render\ten-US\tzero\toh\n"
                                "phone_render\ten-US\tseparator\t / \n"
                                "phone_render\ten-US\textension\t extension number \n")
        for row_id, language, surface, expected, resource_rows, domain_rows in (
            ("legacy-ko-phone-render", "ko-KR", "010-1234-5678", "영일영 / 일이삼사 / 오육칠팔",
             "phone_render\tko-KR\tzero\t영\nphone_render\tko-KR\tseparator\t / \n", ""),
            ("legacy-en-phone-render", "en-US", "212-555-0100", "two one two / five five five / oh one oh oh",
             "phone_render\ten-US\tzero\toh\nphone_render\ten-US\tseparator\t / \n", ""),
            ("legacy-number-zero", "ko-KR", "010-1234-5678", "제로일제로 일이삼사 오육칠팔",
             "number\tko-KR\tphone_zero\t제로\n", ""),
            ("explicit-phone-zero-tie", "ko-KR", "010-1234-5678", "제로일제로 일이삼사 오육칠팔",
             "number\tko-KR\tphone_zero\t영\nphone_render\tko-KR\tzero\t제로\n", ""),
            ("legacy-zero-higher-layer", "ko-KR", "010-1234-5678", "영일영 일이삼사 오육칠팔",
             "number\tko-KR\tphone_zero\t영\n", "phone_render\tko-KR\tzero\t제로\n"),
        ):
            row = phone(row_id, language, surface, surface, "")
            row["expected"] = expected
            overlaid(row, resource_rows, domain_rows)

        # Both row-local schema errors and an effective contradictory interval
        # must fail before any text is normalized, with actionable coordinates.
        invalid = (
            "phone_policy\t*\tinternational_max_digits\t0\n",
            "phone_policy\t*\tinternational_max_digits\t16\n",
            "phone_policy\t*\tinternational_min_digits\t7suffix\n",
            "phone_policy\ten-US\tnational_min_digits\t7\n",
            "phone_policy\t*\tunknown_limit\t7\n",
            "phone_label\ten-US\tphone\t2\n",
            "phone_extension\ten-US\text.\ttrue\n",
            "phone_render\ten-US\tunknown_label\tword\n",
        )
        for row in invalid:
            customer.write_text("# invalid customer edit\n" + row, encoding="utf-8")
            process = subprocess.run([str(args.cli), "validate-resource", "--resources", str(args.resources), str(customer)],
                                     capture_output=True, timeout=15)
            assert process.returncode != 0, (row, process.stdout, process.stderr)
            result = json.loads(process.stdout)
            diagnostic = result["diagnostic"]
            kind, language, key, _ = row.rstrip("\n").split("\t")
            assert diagnostic["path"] == str(customer) and diagnostic["line"] == 2, diagnostic
            assert (diagnostic["kind"], diagnostic["language"], diagnostic["key"]) == (kind, language, key), diagnostic
            assert diagnostic["message"] and diagnostic["layer"] == "customer", diagnostic
            checks += 1
        customer.write_text("phone_policy\t*\tinternational_min_digits\t12\n"
                            "phone_policy\t*\tinternational_max_digits\t10\n", encoding="utf-8")
        process = subprocess.run([str(args.cli), "validate-resource", "--resources", str(args.resources), str(customer)],
                                 capture_output=True, timeout=15)
        assert process.returncode != 0, process
        result = json.loads(process.stdout)
        assert result["diagnostic"]["kind"] == "phone_policy" and result["diagnostic"]["line"] == 0, result
        checks += 1
    return checks


def contextual_guard_checks(args):
    """Focused policy contrasts kept outside the frozen 55-case measurement."""
    cases = (
        ("math-x", "en-US", "+1250000 x 2", "+1250000", "CARDINAL", "+1250000", ""),
        ("short-signed-range", "en-US", "+1-2", "+1-2", "RANGE", "+1", "2"),
        ("malformed-date", "en-US", "2026-09-161", "2026-09-161", "TEXT", "", ""),
        ("malformed-nested-phone-extension", "en-US", "+1 ((1..555..1 ext 0042",
         "+1 ((1..555..1 ext 0042", "TEXT", "", ""),
        ("malformed-labeled-phone-extension", "ko-KR", "전화: +1 (555..0100..1 ext 0042",
         "+1 (555..0100..1 ext 0042", "TEXT", "", ""),
        ("malformed-extra-closing-parenthesis", "en-US", "(212)) 555-0100",
         "(212)) 555-0100", "TEXT", "", ""),
        ("english-label-default-ko", "ko-KR", "Phone:+12125550100", "+12125550100",
         "PHONE", "+12125550100", ""),
        ("korean-clause-english-label", "ko-KR", "문의는 Phone:+1 212 555 0100입니다.",
         "+1 212 555 0100", "PHONE", "+1-212-555-0100", ""),
    )
    checks = 0
    for case_id, language, text, surface, semantic, value, secondary in cases:
        for profile in PROFILES:
            previous = None
            for chunk in (1, 2, 7, 4096):
                result = stream(args.cli, args.resources, text, language, chunk, profile)
                span = exact_span(result, text, surface)
                assert (span["semantic_type"], span["value"], span["secondary"]) == (
                    semantic, value, secondary), (case_id, profile, result)
                if semantic == "PHONE":
                    context = "en-US" if case_id == "english-label-default-ko" else "ko-KR"
                    assert span["source_lang"] == "und" and span["context_lang"] == context and \
                        span["render_lang"] == context, (case_id, profile, span)
                    actual = result["text"].encode()[span["begin"]:span["end"]].decode()
                    assert actual == spoken_phone(value, "", context), (case_id, profile, actual)
                else:
                    assert all(item["semantic_type"] != "PHONE" for item in result["spans"]), result
                if semantic == "TEXT":
                    assert result["text"] == text, result
                if previous is not None:
                    assert result == previous, (case_id, profile, chunk, result, previous)
                previous = result
                checks += 1
    return checks


def ordinary_parenthesis_checks(args):
    """Original prose/list examples: parentheses alone are not phone evidence."""
    cases = (
        ("en-US", "( 56 km/h )", "( fifty six kilometers per hour )",
         (("56 km/h", "MEASURE", "56", "km_per_hour"),)),
        ("en-US", "Volume 1 ( Example )", "Volume one ( Example )",
         (("1", "CARDINAL", "1", ""),)),
        ("en-US", "( 2003 and 2005 )", "( two thousand three and two thousand five )",
         (("2003", "CARDINAL", "2003", ""), ("2005", "CARDINAL", "2005", ""))),
        # A bracket holding a four-digit number and nothing else is the citation
        # shape, so it reads as a year from 2026-09-25 (108 spans of the external
        # English sample). These two rows keep their point, which is that a
        # bracketed number is not a phone number. "( 2003 and 2005 )" above shows
        # an ordinary parenthesis still keeping its cardinals.
        ("en-US", "1385 ( 2006 )", "one thousand three hundred eighty five ( two thousand six )",
         (("1385", "CARDINAL", "1385", ""), ("2006", "DATE", "2006", "year"))),
        ("en-US", "( 2024 )", "( twenty twenty four )",
         (("2024", "DATE", "2024", "year"),)),
        ("en-US", "[ 21st ]", "[ twenty first ]", (("21st", "ORDINAL", "21", ""),)),
        ("en-US", "Length ( 8 cm )", "Length ( eight centimeters )",
         (("8 cm", "MEASURE", "8", "cm"),)),
        ("ko-KR", "( 3개 )", "( 세 개 )", (("3개", "COUNTER", "3", "개"),)),
        ("en-US", "(1 2 3 4 5 6 7)", "(one two three four five six seven)",
         tuple((str(value), "CARDINAL", str(value), "") for value in range(1, 8))),
        ("en-US", "ISBN 978-1-4028-9462-6", "ISBN 978-1-4028-9462-6",
         (("978-1-4028-9462-6", "TEXT", "", ""),)),
        ("en-US", "ID 0-123-45678-9", "ID 0-123-45678-9",
         (("0-123-45678-9", "TEXT", "", ""),)),
    )
    checks = 0
    for language, text, expected, numbers in cases:
        row = dict(id="ordinary-parenthesis:" + text, category="phone/negative",
                   default_language=language, text=text, expected=expected, expected_spans=[
                       dict(surface=surface, semantic_type=kind, value=value, unit=unit)
                       for surface, kind, value, unit in numbers])
        for profile in PROFILES:
            previous = None
            for chunk in (1, 2, 7, 4096):
                result = stream(args.cli, args.resources, text, language, chunk, profile)
                verify(result, row)
                if previous is not None:
                    assert result == previous, (text, profile, chunk, previous, result)
                previous = result
                checks += 1
    return checks


def numeric_list_work_checks(args):
    """Roomy termination guard, not a hardware-dependent speed benchmark.

    A whole-input segmentation target exercises an ordinary numeric list.
    Larger classifier-only work is checked by test_phone_scan.c: the complete
    pipeline has a separate, pre-existing context-scoring cost for huge clauses.
    Exact words verify that a speed fix does not swallow or truncate input.
    """
    for count in (3000,):
        text = " ".join(["7"] * count)
        expected = " ".join(["seven"] * count) + "\n"
        limit = str(len(text) + 1)
        try:
            process = subprocess.run(
                [str(args.cli), "normalize", "--resources", str(args.resources), "--language", "en-US",
                 "--first-target", limit, "--normal-target", limit, "--max-length", limit, text],
                capture_output=True, timeout=30, check=False)
        except subprocess.TimeoutExpired:
            raise AssertionError(f"ordinary numeric list of {count} items exceeded 30 seconds") from None
        assert process.returncode == 0 and process.stdout.decode() == expected, (
            count, process.returncode, process.stderr, len(process.stdout), len(expected))
    return 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    args.cli, args.resources = args.cli.resolve(), args.resources.resolve()
    if args.library:
        args.library = args.library.resolve()
    rows = REFERENCES + GUARDS + MALFORMED
    checks = 0
    for row in rows:
        for profile in PROFILES:
            previous = None
            normalized = subprocess.run([str(args.cli), "normalize", "--resources", str(args.resources),
                                          "--language", row["default_language"], *profile, row["text"]],
                                         capture_output=True, timeout=15, check=False)
            assert normalized.returncode == 0 and normalized.stdout.decode() == row["expected"] + "\n", (
                row["id"], profile, row["expected"], normalized.stdout, normalized.stderr)
            for chunk in (1, 2, 7, 4096):
                result = stream(args.cli, args.resources, row["text"], row["default_language"], chunk, profile)
                verify(result, row)
                if previous is not None:
                    assert result == previous, (row["id"], profile, chunk, result, previous)
                previous = result
                checks += 1
    checks += customization_checks(args)
    checks += contextual_guard_checks(args)
    checks += ordinary_parenthesis_checks(args)
    checks += numeric_list_work_checks(args)
    count = native_checks(args, rows) if args.library else 0
    print(f"phone contracts: {checks} text/IR/profile/chunk checks; {count} native split comparisons")


if __name__ == "__main__":
    main()
