"""Original scaled-money references and independent Decimal/streaming checks.

No external corpus sentence or engine-produced expected text is used here.
The optional --library check exercises every two-way UTF-8 byte split; omit it
when the CLI is instrumented with ASan so Python need not load an ASan library.
"""

import argparse
import importlib.util
import json
import random
import subprocess
import tempfile
from decimal import Decimal, localcontext
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROFILES = ((), ("--first-target", "1", "--normal-target", "3", "--max-length", "12"))

# language, written, independently authored spoken text, amount surface,
# coefficient, power of ten, ISO currency. Spoken references are deliberately
# distinct from the independent arithmetic oracle below.
REFERENCES = (
    (
        "en-US",
        "$2.75 million",
        "two point seven five million dollars",
        "$2.75 million",
        "2.75",
        6,
        "USD",
    ),
    ("en-US", "$8M", "eight million dollars", "$8M", "8", 6, "USD"),
    ("en-US", "-US$2M", "minus two million dollars", "-US$2M", "-2", 6, "USD"),
    ("en-US", "+$2M", "plus two million dollars", "+$2M", "+2", 6, "USD"),
    (
        "en-US",
        "2.75 million USD",
        "two point seven five million dollars",
        "2.75 million USD",
        "2.75",
        6,
        "USD",
    ),
    (
        "en-US",
        "1.5mn EUR",
        "one point five million euros",
        "1.5mn EUR",
        "1.5",
        6,
        "EUR",
    ),
    ("en-US", "US$2M USD", "two million dollars", "US$2M USD", "2", 6, "USD"),
    ("en-US", "$2.5bn", "two point five billion dollars", "$2.5bn", "2.5", 9, "USD"),
    (
        "en-US",
        "€1.25billion",
        "one point two five billion euros",
        "€1.25billion",
        "1.25",
        9,
        "EUR",
    ),
    (
        "en-US",
        "US$12 thousand",
        "twelve thousand dollars",
        "US$12 thousand",
        "12",
        3,
        "USD",
    ),
    (
        "en-US",
        "$-2.5 million",
        "minus two point five million dollars",
        "$-2.5 million",
        "-2.5",
        6,
        "USD",
    ),
    (
        "en-US",
        "$+3 million",
        "plus three million dollars",
        "$+3 million",
        "+3",
        6,
        "USD",
    ),
    ("en-US", "$0 million", "zero million dollars", "$0 million", "0", 6, "USD"),
    (
        "en-US",
        "$1.00 million",
        "one million dollars",
        "$1.00 million",
        "1.00",
        6,
        "USD",
    ),
    (
        "en-US",
        "$0.00000001 million",
        "zero point zero zero zero zero zero zero zero one million dollars",
        "$0.00000001 million",
        "0.00000001",
        6,
        "USD",
    ),
    (
        "en-US",
        "$0.000001 million",
        "zero point zero zero zero zero zero one million dollars",
        "$0.000001 million",
        "0.000001",
        6,
        "USD",
    ),
    (
        "en-US",
        "$0.001 thousand",
        "zero point zero zero one thousand dollars",
        "$0.001 thousand",
        "0.001",
        3,
        "USD",
    ),
    (
        "en-US",
        "$0.000000001 million",
        "zero point zero zero zero zero zero zero zero zero one million dollars",
        "$0.000000001 million",
        "0.000000001",
        6,
        "USD",
    ),
    (
        "en-US",
        "$1.000000001 billion",
        "one point zero zero zero zero zero zero zero zero one billion dollars",
        "$1.000000001 billion",
        "1.000000001",
        9,
        "USD",
    ),
    (
        "en-US",
        "The budget is $4.25 million.",
        "The budget is four point two five million dollars.",
        "$4.25 million",
        "4.25",
        6,
        "USD",
    ),
    (
        "ko-KR",
        "금액은 $2.75 million입니다.",
        "금액은 이백칠십오만 달러입니다.",
        "$2.75 million",
        "2.75",
        6,
        "USD",
    ),
    ("ko-KR", "금액은 $8M입니다.", "금액은 팔백만 달러입니다.", "$8M", "8", 6, "USD"),
    (
        "ko-KR",
        "금액은 $2M달러입니다.",
        "금액은 이백만 달러입니다.",
        "$2M달러",
        "2",
        6,
        "USD",
    ),
    (
        "ko-KR",
        "금액은 $2.5bn입니다.",
        "금액은 이십오억 달러입니다.",
        "$2.5bn",
        "2.5",
        9,
        "USD",
    ),
    (
        "ko-KR",
        "금액은 €1.25billion입니다.",
        "금액은 십이억 오천만 유로입니다.",
        "€1.25billion",
        "1.25",
        9,
        "EUR",
    ),
    (
        "ko-KR",
        "금액은 $-2.5 million입니다.",
        "금액은 마이너스 이백오십만 달러입니다.",
        "$-2.5 million",
        "-2.5",
        6,
        "USD",
    ),
    (
        "ko-KR",
        "금액은 $0.000001 million입니다.",
        "금액은 일 달러입니다.",
        "$0.000001 million",
        "0.000001",
        6,
        "USD",
    ),
)

RANGES = (
    (
        "en-US",
        "$2M~$3M",
        "two million to three million dollars",
        "2000000",
        "3000000",
        "USD",
    ),
    (
        "en-US",
        "€2M~€3M",
        "two million to three million euros",
        "2000000",
        "3000000",
        "EUR",
    ),
    (
        "en-US",
        "$-2M~$-1M",
        "minus two million to minus one million dollars",
        "-2000000",
        "-1000000",
        "USD",
    ),
    ("ko-KR", "$2M~$3M", "이백만 달러에서 삼백만 달러", "2000000", "3000000", "USD"),
    (
        "en-US",
        "$2~3 million",
        "two million to three million dollars",
        "2000000",
        "3000000",
        "USD",
    ),
    (
        "en-US",
        "2~3 million USD",
        "two million to three million dollars",
        "2000000",
        "3000000",
        "USD",
    ),
    (
        "en-US",
        "$2 million~3 million",
        "two million to three million dollars",
        "2000000",
        "3000000",
        "USD",
    ),
    (
        "ko-KR",
        "$2~3 million",
        "이백만 달러에서 삼백만 달러",
        "2000000",
        "3000000",
        "USD",
    ),
    (
        "en-US",
        "$2~$3 million",
        "two dollars to three million dollars",
        "2",
        "3000000",
        "USD",
    ),
    ("en-US", "$2~$3000", "two dollars to three thousand dollars", "2", "3000", "USD"),
    (
        "en-US",
        "$2 million~$3",
        "two million dollars to three dollars",
        "2000000",
        "3",
        "USD",
    ),
    (
        "en-US",
        "$2~3 million USD",
        "two dollars to three million dollars",
        "2",
        "3000000",
        "USD",
    ),
    (
        "en-US",
        "$-2~$-3M",
        "minus two dollars to minus three million dollars",
        "-2",
        "-3000000",
        "USD",
    ),
    (
        "en-US",
        "$-2M~$-3",
        "minus two million dollars to minus three dollars",
        "-2000000",
        "-3",
        "USD",
    ),
)

ALIASES = (
    (3, ("thousand", "Thousand", "THOUSAND", "k", "K")),
    (6, ("million", "Million", "MILLION", "m", "M", "mn")),
    (9, ("billion", "Billion", "BILLION", "b", "B", "bn")),
    (12, ("trillion", "Trillion", "TRILLION", "t", "T", "tn")),
)

MALFORMED_SCALE_PUNCTUATION = (
    "$2 million.3",
    "$2 billion:3",
    "$2 million/budget",
    "$2 M_foo",
)


def expanded(coefficient, exponent):
    """Independent base-ten arithmetic, never binary float or engine rendering."""
    with localcontext() as context:
        context.prec = 200
        value = format(Decimal(coefficient) * (Decimal(10) ** exponent), "f")
    if "." in value:
        value = value.rstrip("0").rstrip(".")
    if coefficient.startswith("+"):
        value = "+" + value
    return value


def stream(cli, resources, text, language="en-US", chunk=1, profile=(), overlay=()):
    command = [
        str(cli),
        "stream-json",
        "--resources",
        str(resources),
        "--language",
        language,
        "--chunk-bytes",
        str(chunk),
        *profile,
        *overlay,
        "-",
    ]
    run = subprocess.run(
        command, input=text.encode(), capture_output=True, timeout=15, check=False
    )
    assert run.returncode == 0, (command, text, run.stderr.decode(errors="replace"))
    events = [json.loads(line) for line in run.stdout.splitlines()]
    assert events and events[-1]["event"] == "end", (text, events)
    segments = [event for event in events if event["event"] == "segment"]
    offset = 0
    spans = []
    for segment in segments:
        for span in segment["spans"]:
            spans.append(
                dict(span, begin=span["begin"] + offset, end=span["end"] + offset)
            )
        offset += len(segment["text"].encode())
    return {"text": "".join(segment["text"] for segment in segments), "spans": spans}


def amount_span(result, text, surface, semantic, value, secondary, unit):
    source = text.encode()
    begin = source.index(surface.encode())
    end = begin + len(surface.encode())
    spans = [
        span
        for span in result["spans"]
        if (span["input_begin"], span["input_end"]) == (begin, end)
    ]
    assert len(spans) == 1, (text, surface, result)
    span = spans[0]
    actual = (span["semantic_type"], span["value"], span["secondary"], span["unit"])
    assert actual == (semantic, value, secondary, unit), (text, actual, span)
    return span


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    checks = 0

    for language, text, expected, surface, coefficient, exponent, unit in REFERENCES:
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(
                    args.cli, args.resources, text, language, chunk, profile
                )
                assert result["text"] == expected, (
                    text,
                    expected,
                    result,
                    profile,
                    chunk,
                )
                amount_span(
                    result,
                    text,
                    surface,
                    "CURRENCY",
                    expanded(coefficient, exponent),
                    str(exponent),
                    unit,
                )
                checks += 1

    for language, text, expected, first, second, unit in RANGES:
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(
                    args.cli, args.resources, text, language, chunk, profile
                )
                assert result["text"] == expected, (text, expected, result)
                amount_span(result, text, text, "RANGE", first, second, unit)
                assert len(result["spans"]) == 1, (text, result)
                checks += 1

    # Generated cases independently check the arithmetic and complete source
    # coverage only; no expected spoken string is synthesized by the engine.
    rng = random.Random(49999)
    for exponent, aliases in ALIASES:
        for index, alias in enumerate(aliases):
            coefficient = f"{rng.randrange(-999, 1000)}.{rng.randrange(10**12):012d}"
            for separator in ("", " \t "):
                text = "$" + coefficient + separator + alias
                for chunk in (1, 64):
                    result = stream(
                        args.cli,
                        args.resources,
                        text,
                        chunk=chunk,
                        profile=PROFILES[index % len(PROFILES)],
                    )
                    amount_span(
                        result,
                        text,
                        text,
                        "CURRENCY",
                        expanded(coefficient, exponent),
                        str(exponent),
                        "USD",
                    )
                    assert len(result["spans"]) == 1, (text, result)
                    checks += 1

    # Values close to the 96-byte IR capacity still use exact arithmetic;
    # exponent forms here are input syntax, not a Python float approximation.
    for coefficient in ("1e88", "-1e87", "+1e87", "1e-93", "-1e-92"):
        text = "$" + coefficient + "million"
        for chunk in (1, 7, 64):
            result = stream(
                args.cli, args.resources, text, chunk=chunk, profile=PROFILES[1]
            )
            amount_span(
                result, text, text, "CURRENCY", expanded(coefficient, 6), "6", "USD"
            )
            assert len(result["spans"]) == 1, (text, result)
            checks += 1

    # No partial monetary normalization for unknown scale tails, contradictory
    # currencies/units, or malformed repeated quantities.
    for text in (
        "$2millionaire",
        "$2.5bnX",
        "$2MM",
        "$2M원",
        "$2Mkg",
        "US$2M EUR",
        "$2M~€3M",
        "$2M~$3M~",
        "$2 million~3",
        "$1million_foo",
        "2millionUSD_foo",
        "$1 million million",
        "$1e89million",
        "$1e-94million",
        "$2M+3",
        "$2M-foo",
        "-$-2M",
    ) + MALFORMED_SCALE_PUNCTUATION:
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(
                    args.cli, args.resources, text, chunk=chunk, profile=profile
                )
                assert result["text"] == text, (text, result)
                assert len(result["spans"]) == 1, (text, result)
                span = result["spans"][0]
                assert span["semantic_type"] == "TEXT" and not span["value"], (
                    text,
                    span,
                )
                assert not span["secondary"] and not span["unit"], (text, span)
                checks += 1

    # Scale aliases are scoped by money syntax: lower-case m is still meters.
    for text, expected, semantic, value, secondary, unit in (
        ("2m", "two meters", "MEASURE", "2", "", "m"),
        ("$2m", "two million dollars", "CURRENCY", "2000000", "6", "USD"),
        ("2.5JPY", "two point five yen", "CURRENCY", "2.5", "", "JPY"),
        ("1.25KRW", "one point two five won", "CURRENCY", "1.25", "", "KRW"),
        ("2JPY", "two yen", "CURRENCY", "2", "", "JPY"),
        ("1KRW", "one won", "CURRENCY", "1", "", "KRW"),
        ("-$1.05", "minus one dollar and five cents", "CURRENCY", "-1.05", "", "USD"),
    ):
        result = stream(args.cli, args.resources, text)
        assert result["text"] == expected, (text, result)
        amount_span(result, text, text, semantic, value, secondary, unit)
        checks += 1

    # The Korean compound scale grammar owns these quantities and does not
    # preserve the English scale exponent hint in secondary. Its established
    # positive-sign policy canonicalizes the value without a leading plus.
    for text, expected, value, unit in (
        ("-$2만", "마이너스 이만 달러", "-20000", "USD"),
        ("-US$2.5만", "마이너스 이만 오천 달러", "-25000", "USD"),
        ("-₩2만원", "마이너스 이만 원", "-20000", "KRW"),
        ("+A$2만", "이만 호주 달러", "20000", "AUD"),
    ):
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(args.cli, args.resources, text, "ko-KR", chunk, profile)
                assert result["text"] == expected, (text, result)
                amount_span(result, text, text, "CURRENCY", value, "", unit)
                assert len(result["spans"]) == 1, (text, result)
                checks += 1
    for text in ("-$-2만", "-₩+2만원", "$--2만"):
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(args.cli, args.resources, text, "ko-KR", chunk, profile)
                assert result["text"] == text, (text, result)
                amount_span(result, text, text, "TEXT", "", "", "")
                assert len(result["spans"]) == 1, (text, result)
                checks += 1

    # Scale aliases are complete lexical markers. An ordinary following word
    # must not be consumed because it starts with t, m, b, or k. These original
    # prose fixtures also distinguish a currency suffix from a physical unit
    # found inside an unknown word such as bcm.
    for text, expected, amounts in (
        (
            "Budget moved from $17 to $43.",
            "Budget moved from seventeen dollars to forty three dollars.",
            (("$17", "CURRENCY", "17", "USD"), ("$43", "CURRENCY", "43", "USD")),
        ),
        (
            "Save $17 monthly.",
            "Save seventeen dollars monthly.",
            (("$17", "CURRENCY", "17", "USD"),),
        ),
        (
            "Keep $17 before checkout.",
            "Keep seventeen dollars before checkout.",
            (("$17", "CURRENCY", "17", "USD"),),
        ),
        (
            "Budget is $17 total.",
            "Budget is seventeen dollars total.",
            (("$17", "CURRENCY", "17", "USD"),),
        ),
        (
            "We spent $17 keeping it open.",
            "We spent seventeen dollars keeping it open.",
            (("$17", "CURRENCY", "17", "USD"),),
        ),
        (
            "Pay $17 Monday.",
            "Pay seventeen dollars Monday.",
            (("$17", "CURRENCY", "17", "USD"),),
        ),
        ("Volume is 8 bcm.", "Volume is eight bcm.", (("8", "CARDINAL", "8", ""),)),
        (
            "Stock contains 9 boxes.",
            "Stock contains nine boxes.",
            (("9", "CARDINAL", "9", ""),),
        ),
    ):
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(
                    args.cli, args.resources, text, chunk=chunk, profile=profile
                )
                assert result["text"] == expected, (text, result)
                for surface, semantic, value, unit in amounts:
                    amount_span(result, text, surface, semantic, value, "", unit)
                checks += 1

    for text in ("`$2M`", "https://example.test/price/$2M"):
        for chunk in (1, 7, 64):
            result = stream(args.cli, args.resources, text, chunk=chunk)
            assert result["text"] == text, (text, result)
            assert all(span["semantic_type"] != "CURRENCY" for span in result["spans"])
            checks += 1

    # Only horizontal whitespace may join a coefficient, scale, or endpoint.
    for separator in ("\n", "\r", "\r\n", "\v", "\f"):
        for profile in PROFILES:
            text = "$2" + separator + "million"
            result = stream(args.cli, args.resources, text, profile=profile)
            assert result["text"] == "two dollars" + separator + "million", (
                text,
                result,
            )
            amount_span(result, text, "$2", "CURRENCY", "2", "", "USD")
            text = "$2M~" + separator + "$3M"
            result = stream(args.cli, args.resources, text, profile=profile)
            assert separator in result["text"], (text, result)
            assert all(span["semantic_type"] != "RANGE" for span in result["spans"]), (
                text,
                result,
            )
            checks += 2

    # Both a new alias and replacement of an existing alias are resource-only.
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "scale.tsv"
        path.write_text(
            "currency_scale\t*\tcustomMillion\t6\n"
            "currency_scale\t*\tM\t3\n"
            "currency_scale\t*\tcustomQuadrillion\t15\n"
            "currency_scale\t*\tcustomQuintillion\t18\n",
            encoding="utf-8",
        )
        overlay = ("--customer", str(path))
        for text, expected, coefficient, exponent in (
            ("$2customMillion", "two million dollars", "2", 6),
            ("$2M", "two thousand dollars", "2", 3),
            ("$2customQuadrillion", "two quadrillion dollars", "2", 15),
            ("$2customQuintillion", "two quintillion dollars", "2", 18),
        ):
            for chunk in (1, 7, 64):
                result = stream(
                    args.cli, args.resources, text, chunk=chunk, overlay=overlay
                )
                assert result["text"] == expected, (text, result)
                span = amount_span(
                    result,
                    text,
                    text,
                    "CURRENCY",
                    expanded(coefficient, exponent),
                    str(exponent),
                    "USD",
                )
                assert span["resource_layer"] == "customer", (text, span)
                checks += 1

    native_checks = 0
    if args.library:
        spec = importlib.util.spec_from_file_location(
            "ttf_native", ROOT / "tools/web/native.py"
        )
        native = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(native)
        for tiny in (False, True):
            for language in ("en-US", "ko-KR"):
                options = {"default_language": language}
                if tiny:
                    options.update(first_target=1, normal_target=3, max_length=12)
                engine = native.Engine(
                    args.library.resolve(), args.resources.resolve(), options
                )
                try:
                    fixtures = [
                        (row[1], row[2]) for row in REFERENCES if row[0] == language
                    ]
                    fixtures += [
                        (row[1], row[2]) for row in RANGES if row[0] == language
                    ]
                    if language == "en-US":
                        fixtures += [
                            (text, text) for text in MALFORMED_SCALE_PUNCTUATION
                        ]
                    for text, expected in fixtures:
                        source = text.encode()
                        batch = engine.normalize(source)
                        assert batch["text"] == expected, (text, batch, expected)
                        for split in range(1, len(source)):
                            actual = native.flatten(
                                engine.stream(source, [split, len(source) - split])
                            )
                            assert actual == batch, (text, split, tiny, batch, actual)
                            native_checks += 1
                finally:
                    engine.close()

    print(
        f"{checks} scaled currency text/Decimal/value/span/chunk/resource checks; "
        f"{native_checks} native every-two-way-byte-split checks passed"
    )


if __name__ == "__main__":
    main()
