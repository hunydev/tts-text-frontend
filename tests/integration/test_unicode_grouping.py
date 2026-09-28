"""Exact-value Unicode grouping, preservation and arbitrary-byte contracts.

Original written references and Decimal arithmetic are independent of the engine.
The typography characters are literal input bytes: no preprocessing changes input
positions, code literals or nonnumeric whitespace before tokenization.
"""
import argparse
from decimal import Decimal, localcontext
import importlib.util
import json
from pathlib import Path
import random
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SEPARATORS = ("\u00a0", "\u202f", "\u2009")
PROFILES = ((), ("--first-target", "1", "--normal-target", "3", "--max-length", "12"))


def stream(args, text, language="ko-KR", profile=(), chunk=7, overlay=()):
    command = [str(args.cli), "stream-json", "--resources", str(args.resources),
               "--language", language, "--chunk-bytes", str(chunk), *profile, *overlay, "-"]
    run = subprocess.run(command, input=text.encode(), capture_output=True, timeout=15)
    assert run.returncode == 0, (text, run.stderr)
    events = [json.loads(line) for line in run.stdout.decode().splitlines()]
    assert events[-1]["event"] == "end", events
    segments = [event for event in events if event["event"] == "segment"]
    return {"text": "".join(event["text"] for event in segments),
            "spans": [span for event in segments for span in event["spans"]]}


def check_span(result, text, semantic, value, unit="", secondary=""):
    assert len(result["spans"]) == 1, (text, result)
    span = result["spans"][0]
    assert (span["input_begin"], span["input_end"]) == (0, len(text.encode())), (text, span)
    assert (span["semantic_type"], span["value"], span["unit"], span["secondary"]) == (
        semantic, value, unit, secondary), (text, span, semantic, value, unit, secondary)
    return span


def references(separator):
    # language, input, spoken text, type, primary value, unit, secondary value.
    s = separator
    return (
        ("ko-KR", f"1{s}250원", "천이백오십 원", "CURRENCY", "1250", "KRW", ""),
        ("ko-KR", f"₩1{s}250", "천이백오십 원", "CURRENCY", "1250", "KRW", ""),
        ("ko-KR", f"₩{s}1{s}250", "천이백오십 원", "CURRENCY", "1250", "KRW", ""),
        ("en-US", f"1{s}250{s}kg", "one thousand two hundred fifty kilograms", "MEASURE", "1250", "kg", ""),
        ("en-US", f"1{s}250{s}US dollars", "one thousand two hundred fifty dollars", "CURRENCY", "1250", "USD", ""),
        ("ko-KR", f"-₩1{s}250", "마이너스 천이백오십 원", "CURRENCY", "-1250", "KRW", ""),
        ("en-US", f"$1{s}250", "one thousand two hundred fifty dollars", "CURRENCY", "1250", "USD", ""),
        ("en-US", f"1{s}250 dollars", "one thousand two hundred fifty dollars", "CURRENCY", "1250", "USD", ""),
        ("ko-KR", f"1{s}250kg", "천이백오십 킬로그램", "MEASURE", "1250", "kg", ""),
        ("en-US", f"1{s}250km", "one thousand two hundred fifty kilometers", "MEASURE", "1250", "km", ""),
        ("ko-KR", f"1{s}250", "천이백오십", "CARDINAL", "1250", "", ""),
        ("en-US", f"12{s}345{s}678", "twelve million three hundred forty five thousand six hundred seventy eight",
         "CARDINAL", "12345678", "", ""),
        ("ko-KR", f"-1{s}250.50%", "마이너스 천이백오십 점 오영 퍼센트", "PERCENT", "-1250.50", "", ""),
        ("ko-KR", f"1{s}250e-2kg", "십이 점 오 킬로그램", "MEASURE", "12.5", "kg", ""),
        ("ko-KR", f"+1{s}250e+2원", "플러스 십이만 오천 원", "CURRENCY", "+125000", "KRW", ""),
        ("ko-KR", f"1{s}250~2{s}500원", "천이백오십 원에서 이천오백 원", "RANGE", "1250", "KRW", "2500"),
        ("en-US", f"$1{s}250~$2{s}500", "one thousand two hundred fifty to two thousand five hundred dollars",
         "RANGE", "1250", "USD", "2500"),
        ("ko-KR", f"1{s}250{s}~{s}2{s}500원", "천이백오십 원에서 이천오백 원", "RANGE", "1250", "KRW", "2500"),
        ("ko-KR", f"1{s}250kg~2{s}500kg", "천이백오십 킬로그램에서 이천오백 킬로그램",
         "RANGE", "1250", "kg", "2500"),
    )


def invalids(separator):
    s = separator
    return (
        f"1{s}25원", f"1{s}2500kg", f"1234{s}567원", f"1{s}{s}250원",
        f"1{s}250,000원", f"1,250{s}000원",
        f"1.25{s}000원", f"1e2{s}000원", f"1{s}250e999원", f"1{s}250e+원",
        f"$1{s}25", f"1{s}25~2{s}500원", f"₩1{s}25~₩2{s}500",
        f"1{s}250~2{s}50원", f"1{s}250kg~2{s}50kg",
        f"2026-09-1{s}700", f"12:3{s}000", f"010-12{s}345-6789", f"1.2.3{s}456", f"192.168.0.1{s}000",
        f"September 1{s}250", f"September 17, 2{s}026", f"1{s}250 September",
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cli", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    args.cli = args.cli.resolve()
    args.resources = args.resources.resolve()
    checks = 0
    for separator in SEPARATORS:
        for language, text, expected, semantic, value, unit, secondary in references(separator):
            for profile in PROFILES:
                for chunk in (1, 7, 64):
                    result = stream(args, text, language, profile, chunk)
                    assert result["text"] == expected, (text, expected, result)
                    check_span(result, text, semantic, value, unit, secondary)
                    checks += 1
        for text in invalids(separator):
            for profile in PROFILES:
                for chunk in (1, 7, 64):
                    result = stream(args, text, language="en-US" if "September" in text else "ko-KR",
                                    profile=profile, chunk=chunk)
                    assert result["text"] == text, (text, result)
                    check_span(result, text, "TEXT", "")
                    checks += 1

    # Mixed space types are invalid, including combinations whose individual
    # runs would each be well-formed numbers. ASCII spaces/tabs remain separators
    # between independent numbers and are never unconditional grouping.
    for first in SEPARATORS:
        for second in SEPARATORS:
            if first == second:
                continue
            text = f"1{first}250{second}000원"
            result = stream(args, text, profile=PROFILES[1], chunk=1)
            assert result["text"] == text, (text, result)
            check_span(result, text, "TEXT", "")
            checks += 1
    for gap in (" ", "\t", "\n"):
        text = f"1{gap}250원"
        result = stream(args, text, profile=PROFILES[1], chunk=1)
        assert result["text"] == f"일{gap}이백오십 원", (text, result)
        assert [span["value"] for span in result["spans"] if span["value"]] == ["1", "250"]
        checks += 1
    for separator in SEPARATORS:
        for text in (f"`1{separator}250원`", f'"/tmp/1{separator}250.txt"',
                     f'<code>1{separator}250원</code>'):
            for profile in PROFILES:
                result = stream(args, text, profile=profile, chunk=1)
                assert result["text"] == text, (text, result)
                checks += 1
        text = f"가나다{separator}라마바"
        result = stream(args, text, chunk=1)
        assert result["text"] == text, (text, result)
        checks += 1

    # Independent Decimal reference: no floating-point rounding and no loss of
    # group separators in the original offsets. A seeded generator varies the
    # complete integer group count, leading sign, fraction and decimal exponent.
    rng = random.Random(271828)
    for index in range(90):
        separator = SEPARATORS[index % 3]
        raw = rng.randrange(1000, 10**15)
        grouped = format(raw, ",").replace(",", separator)
        sign = rng.choice(("", "-", "+"))
        fraction = f".{rng.randrange(10000):04d}"
        exponent = rng.randrange(-10, 11)
        flat = f"{sign}{raw}{fraction}e{exponent:+d}"
        text = f"{sign}{grouped}{fraction}e{exponent:+d}원"
        with localcontext() as context:
            context.prec = 200
            exact = format(Decimal(flat), "f")
        if "." in exact:
            exact = exact.rstrip("0").rstrip(".")
        if sign == "+":
            exact = "+" + exact
        result = stream(args, text, profile=PROFILES[index % 2], chunk=1 + index % 9)
        check_span(result, text, "CURRENCY", exact, "KRW")
        checks += 1

    # Customer overlays control separators; disabled syntax remains visible as
    # one TEXT span. A new eligible Unicode whitespace can be enabled as data.
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "grouping.tsv"
        for separator in SEPARATORS:
            path.write_text(f"numeric_group_separator\t*\t{separator}\t0\n", encoding="utf-8")
            for profile in PROFILES:
                text = f"1{separator}250원"
                result = stream(args, text, profile=profile, chunk=1,
                                overlay=("--customer", str(path)))
                assert result["text"] == text, (text, result)
                check_span(result, text, "TEXT", "")
                checks += 1
        path.write_text("numeric_group_separator\t*\t\u2007\t3\n", encoding="utf-8")
        for profile in PROFILES:
            text = "1\u2007250"
            result = stream(args, text, profile=profile, chunk=1,
                            overlay=("--customer", str(path)))
            assert result["text"] == "천이백오십", result
            span = check_span(result, text, "CARDINAL", "1250")
            assert span["resource_layer"] == "customer", span
            checks += 1

    native_checks = 0
    if args.library:
        spec = importlib.util.spec_from_file_location("ttf_native", ROOT / "tools/web/native.py")
        native = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(native)
        for tiny in (False, True):
            for language in ("ko-KR", "en-US"):
                options = {"default_language": language}
                if tiny:
                    options.update(first_target=1, normal_target=3, max_length=12)
                engine = native.Engine(args.library.resolve(), args.resources, options)
                try:
                    for separator in SEPARATORS:
                        tokens = engine.tokens(f"1{separator}250원".encode())
                        assert [t["script"] for t in tokens] == ["DIGIT", "WHITESPACE", "DIGIT", "HANGUL"]
                        end = 0
                        for token in tokens:
                            assert token["begin"] == end
                            end += len(token["surface"].encode())
                            assert token["end"] == end
                        fixtures = [row[1] for row in references(separator) if row[0] == language]
                        if language == "ko-KR":
                            fixtures += list(invalids(separator))
                            fixtures += [f"가격은 1{separator}250원입니다. 수량은 3개입니다.",
                                         f"1{separator}", f"1{separator}{separator}",
                                         f"1{separator}{separator} 250원", f"`1{separator}250원`"]
                        for text in fixtures:
                            source = text.encode()
                            batch = engine.normalize(source)
                            for split in range(1, len(source)):
                                actual = native.flatten(engine.stream(source, [split, len(source) - split]))
                                assert actual == batch, (text, split, tiny, batch, actual)
                                native_checks += 1
                finally:
                    engine.close()
    print(f"{checks} Unicode grouping value/preservation/resource checks; "
          f"{native_checks} every-two-way-byte-split comparisons passed")


if __name__ == "__main__":
    main()
