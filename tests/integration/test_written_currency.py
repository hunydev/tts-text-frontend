"""Original written-currency references, identity guards and streaming checks.

The spoken references below are authored independently of the engine. They
assert exact currency/value/source metadata as well as text, since bare pounds
can sound identical whether the word denotes money or mass.
"""

import argparse
import importlib.util
import tempfile
from pathlib import Path

from test_scaled_currency import ROOT, PROFILES, amount_span, stream

# language, input, expected text, monetary surface, value, scale, currency
REFERENCES = (
    ("en-US", "12 dollars", "twelve dollars", "12 dollars", "12", "", "USD"),
    ("en-US", "1 dollar", "one dollar", "1 dollar", "1", "", "USD"),
    ("en-US", "2 euros", "two euros", "2 euros", "2", "", "EUR"),
    ("en-US", "1 EURO", "one euro", "1 EURO", "1", "", "EUR"),
    ("en-US", "12 US dollars", "twelve dollars", "12 US dollars", "12", "", "USD"),
    ("en-US", "12 United States dollars", "twelve dollars", "12 United States dollars", "12", "", "USD"),
    ("en-US", "2\tAustralian\t dollars", "two Australian dollars", "2\tAustralian\t dollars", "2", "", "AUD"),
    ("en-US", "1 Canadian dollar", "one Canadian dollar", "1 Canadian dollar", "1", "", "CAD"),
    ("en-US", "3 British pounds", "three pounds", "3 British pounds", "3", "", "GBP"),
    ("en-US", "1 pound sterling", "one pound", "1 pound sterling", "1", "", "GBP"),
    ("en-US", "3 pounds sterling", "three pounds", "3 pounds sterling", "3", "", "GBP"),
    ("en-US", "2 Swiss francs", "two Swiss francs", "2 Swiss francs", "2", "", "CHF"),
    ("en-US", "1 Norwegian krone", "one Norwegian krone", "1 Norwegian krone", "1", "", "NOK"),
    ("en-US", "2 Norwegian kroner", "two Norwegian kroner", "2 Norwegian kroner", "2", "", "NOK"),
    ("en-US", "3 Japanese yen", "three yen", "3 Japanese yen", "3", "", "JPY"),
    ("en-US", "4 Korean won", "four won", "4 Korean won", "4", "", "KRW"),
    ("en-US", "4 South Korean won", "four won", "4 South Korean won", "4", "", "KRW"),
    ("en-US", "2.5 million dollars", "two point five million dollars", "2.5 million dollars", "2500000", "6", "USD"),
    ("en-US", "2.5 million British pounds", "two point five million pounds", "2.5 million British pounds", "2500000", "6", "GBP"),
    ("en-US", "1.25 euros", "one euro and twenty five cents", "1.25 euros", "1.25", "", "EUR"),
    ("en-US", "-1.25 dollars", "minus one dollar and twenty five cents", "-1.25 dollars", "-1.25", "", "USD"),
    ("en-US", "+2.50 dollars", "plus two dollars and fifty cents", "+2.50 dollars", "+2.50", "", "USD"),
    ("en-US", "£3 pounds", "three pounds", "£3 pounds", "3", "", "GBP"),
    ("en-US", "It costs 3 pounds.", "It costs three pounds.", "3 pounds", "3", "", "GBP"),
    ("en-US", "I paid 3 pounds.", "I paid three pounds.", "3 pounds", "3", "", "GBP"),
    ("ko-KR", "금액은 12 dollars입니다.", "금액은 십이 달러입니다.", "12 dollars", "12", "", "USD"),
    ("ko-KR", "금액은 12 US dollars입니다.", "금액은 십이 달러입니다.", "12 US dollars", "12", "", "USD"),
    ("ko-KR", "금액은 2.5 million dollars입니다.", "금액은 이백오십만 달러입니다.", "2.5 million dollars", "2500000", "6", "USD"),
    ("ko-KR", "금액은 3 British pounds입니다.", "금액은 삼 파운드입니다.", "3 British pounds", "3", "", "GBP"),
)
GUARDS = (
    "3 pounds", "The bag weighs 3 pounds.", "The cost is 3 pounds of apples.",
    "It costs 3 pounds of apples.", "I paid 3 pounds of flour.", "I paid 3 pounds weight.",
    "check-paid 3 pounds", "12dollars", "12 dollars100", "12 dollars_foo",
    "12 dollarsé", "12 dollars€", "£2 pounds_foo", "$2 dollars100", "2~3 dollars_foo", "12 dollars+3", "12 dollars-worth", "12 dollarville",
    "12,34 dollars", "2 million million dollars", "£2 dollars", "$2 euros",
    "$2 million Canadian dollars", "2 dollars ~ 3 euros", "2 dollars ~ 3 dollars ~ 4 dollars",
    "12\ndollars", "12 British\npounds", "`12 dollars`",
)
PRESERVED = ("£2 pounds_foo", "$2 dollars100", "2~3 dollars_foo", "£2 poundsé", "£2 pounds+3")
RANGES = (
    ("2 ~ 3 dollars", "two to three dollars", "2", "3", "USD"),
    ("2 British pounds ~ 3 British pounds", "two to three pounds", "2", "3", "GBP"),
    ("2 ~ 3 British pounds", "two to three pounds", "2", "3", "GBP"),
    ("2 ~ 3 million dollars", "two million to three million dollars", "2000000", "3000000", "USD"),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    checks = 0
    for language, text, expected, surface, value, scale, unit in REFERENCES:
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(args.cli, args.resources, text, language, chunk, profile)
                assert result["text"] == expected, (text, expected, result, profile, chunk)
                span = amount_span(result, text, surface, "CURRENCY", value, scale, unit)
                assert span["source_lang"] == "en-US", (text, span)
                checks += 1
    for text, expected, first, second, unit in RANGES:
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(args.cli, args.resources, text, chunk=chunk, profile=profile)
                assert result["text"] == expected, (text, expected, result)
                amount_span(result, text, text, "RANGE", first, second, unit)
                checks += 1
    for text in GUARDS:
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(args.cli, args.resources, text, chunk=chunk, profile=profile)
                assert all(s["semantic_type"] != "CURRENCY" and
                           not (s["semantic_type"] == "RANGE" and s["unit"])
                           for s in result["spans"]), (text, result, profile, chunk)
                checks += 1
    for text in PRESERVED:
        for profile in PROFILES:
            result = stream(args.cli, args.resources, text, profile=profile)
            assert result["text"] == text, (text, result)
            assert len(result["spans"]) == 1 and result["spans"][0]["semantic_type"] == "TEXT", (text, result)
            checks += 1
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "names.tsv"
        path.write_text("currency_name\t*\tcustom dollars\tCAD:en-US\n"
                        "boundary_word\t*\tcustom\t1\n"
                        "currency_name\t*\tdollars\tAUD:en-US\n", encoding="utf-8")
        for text, expected, unit in (("2 custom dollars", "two Canadian dollars", "CAD"),
                                     ("2 dollars", "two Australian dollars", "AUD")):
            for profile in PROFILES:
                result = stream(args.cli, args.resources, text, profile=profile,
                                overlay=("--customer", str(path)))
                assert result["text"] == expected, (text, result)
                span = amount_span(result, text, text, "CURRENCY", "2", "", unit)
                assert span["resource_layer"] == "customer", (text, span)
                checks += 1
    native_checks = 0
    if args.library:
        spec = importlib.util.spec_from_file_location("ttf_native", ROOT / "tools/web/native.py")
        native = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(native)
        for tiny in (False, True):
            for language in ("en-US", "ko-KR"):
                options = {"default_language": language}
                if tiny:
                    options.update(first_target=1, normal_target=3, max_length=12)
                engine = native.Engine(args.library.resolve(), args.resources.resolve(), options)
                try:
                    cases = [(r[1], r[2]) for r in REFERENCES if r[0] == language]
                    if language == "en-US":
                        cases += [(r[0], r[1]) for r in RANGES]
                        cases += [(text, None) for text in GUARDS]
                        cases += [(text, text) for text in PRESERVED]
                    for text, expected in cases:
                        source = text.encode()
                        batch = engine.normalize(source)
                        if expected is not None:
                            assert batch["text"] == expected, (text, expected, batch)
                        for split in range(1, len(source)):
                            actual = native.flatten(engine.stream(source, [split, len(source) - split]))
                            assert actual == batch, (text, split, tiny, batch, actual)
                            native_checks += 1
                finally:
                    engine.close()
    print(f"{checks} written currency text/IR/guard/resource/chunk checks; "
          f"{native_checks} native every-two-way-byte-split checks passed")


if __name__ == "__main__":
    main()
