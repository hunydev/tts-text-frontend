"""Independent English calendar/year references and streaming contracts.

Spoken references are authored here, never copied from engine output. Calendar
validity is checked independently with Python's proleptic Gregorian calendar.
The optional native test covers every two-way UTF-8 byte split, including
splits inside Korean characters. No third-party benchmark text is included.
"""

import argparse
import calendar
import datetime
import importlib.util
import json
import random
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROFILES = ((), ("--first-target", "1", "--normal-target", "3", "--max-length", "12"))
MONTHS = (
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
)

# language, complete input, spoken reference, DATE surface, value, order, unit.
REFERENCES = (
    ("en-US", "September 17, 2026", "September seventeenth twenty twenty six",
     "September 17, 2026", "2026-09-17", "mdy", ""),
    ("en-US", "17 September 2026", "the seventeenth of September twenty twenty six",
     "17 September 2026", "2026-09-17", "dmy", ""),
    ("en-US", "September 2026", "September twenty twenty six",
     "September 2026", "2026-09", "mdy", "year-month"),
    ("en-US", "September 17", "September seventeenth",
     "September 17", "--09-17", "mdy", "month-day"),
    ("en-US", "17 September", "the seventeenth of September",
     "17 September", "--09-17", "dmy", "month-day"),
    ("en-US", "September 17th, 2026", "September seventeenth twenty twenty six",
     "September 17th, 2026", "2026-09-17", "mdy", ""),
    ("en-US", "21st September 2026", "the twenty first of September twenty twenty six",
     "21st September 2026", "2026-09-21", "dmy", ""),
    ("en-US", "February 29, 2000", "February twenty ninth two thousand",
     "February 29, 2000", "2000-02-29", "mdy", ""),
    ("en-US", "February 29", "February twenty ninth",
     "February 29", "--02-29", "mdy", "month-day"),
    ("en-US", "January 1, 1900", "January first nineteen hundred",
     "January 1, 1900", "1900-01-01", "mdy", ""),
    ("en-US", "July 4, 1905", "July fourth nineteen oh five",
     "July 4, 1905", "1905-07-04", "mdy", ""),
    ("en-US", "December 31, 2005", "December thirty first two thousand five",
     "December 31, 2005", "2005-12-31", "mdy", ""),
    ("en-US", "The payment is due on September 17, 2026.",
     "The payment is due on September seventeenth twenty twenty six.",
     "September 17, 2026", "2026-09-17", "mdy", ""),
    ("en-US", "We filmed on 17 September 2026.",
     "We filmed on the seventeenth of September twenty twenty six.",
     "17 September 2026", "2026-09-17", "dmy", ""),
    ("en-US", "The report covers September 2026.",
     "The report covers September twenty twenty six.",
     "September 2026", "2026-09", "mdy", "year-month"),
    ("en-US", "Record the trailer on September 17.",
     "Record the trailer on September seventeenth.",
     "September 17", "--09-17", "mdy", "month-day"),
    ("ko-KR", "마감은 September 17, 2026입니다.",
     "마감은 이천이십육 년 구 월 십칠 일입니다.",
     "September 17, 2026", "2026-09-17", "mdy", ""),
    ("ko-KR", "공개일은 17 September 2026입니다.",
     "공개일은 이천이십육 년 구 월 십칠 일입니다.",
     "17 September 2026", "2026-09-17", "dmy", ""),
    ("ko-KR", "기간은 September 2026입니다.",
     "기간은 이천이십육 년 구 월입니다.",
     "September 2026", "2026-09", "mdy", "year-month"),
    ("ko-KR", "촬영일은 September 17입니다.",
     "촬영일은 구 월 십칠 일입니다.",
     "September 17", "--09-17", "mdy", "month-day"),
    ("ko-KR", "촬영일은 June 1입니다.",
     "촬영일은 유월 일 일입니다.",
     "June 1", "--06-01", "mdy", "month-day"),
    ("ko-KR", "촬영일은 October 1입니다.",
     "촬영일은 시월 일 일입니다.",
     "October 1", "--10-01", "mdy", "month-day"),
    ("en-US", "Sept. 17, 2026", "September seventeenth twenty twenty six",
     "Sept. 17, 2026", "2026-09-17", "mdy", ""),
    ("en-US", "17 Sep. 2026", "the seventeenth of September twenty twenty six",
     "17 Sep. 2026", "2026-09-17", "dmy", ""),
    ("en-US", "May 17, 2026", "May seventeenth twenty twenty six",
     "May 17, 2026", "2026-05-17", "mdy", ""),
    ("en-US", "september 17, 2026", "September seventeenth twenty twenty six",
     "september 17, 2026", "2026-09-17", "mdy", ""),
)

YEAR_REFERENCES = (
    ("in 1998", "in nineteen ninety eight", "1998"),
    ("since 1905", "since nineteen oh five", "1905"),
    ("until 2005", "until two thousand five", "2005"),
    ("during 2026", "during twenty twenty six", "2026"),
    ("before 1900", "before nineteen hundred", "1900"),
    ("after 2000", "after two thousand", "2000"),
    ("year 2099", "year twenty ninety nine", "2099"),
    ("in 2010", "in twenty ten", "2010"),
    ("in 2009", "in two thousand nine", "2009"),
    ("in 1910", "in nineteen ten", "1910"),
    ("in 1000", "in one thousand", "1000"),
)

GUARDS = (
    ("2026", "two thousand twenty six", "2026", "CARDINAL"),
    ("The count is 2026.", "The count is two thousand twenty six.", "2026", "CARDINAL"),
    ("in 2026 days", "in two thousand twenty six days", "2026", "CARDINAL"),
    ("after 2026 seconds", "after two thousand twenty six seconds", "2026", "CARDINAL"),
    ("in 2026 frames", "in two thousand twenty six frames", "2026", "CARDINAL"),
    ("after 2026 tracks", "after two thousand twenty six tracks", "2026", "CARDINAL"),
    ("in 2026kg", "in two thousand twenty six kilograms", "2026kg", "MEASURE"),
    ("in $2026", "in two thousand twenty six dollars", "$2026", "CURRENCY"),
    ("in 999", "in nine hundred ninety nine", "999", "CARDINAL"),
    ("in 2100", "in two thousand one hundred", "2100", "CARDINAL"),
    ("in 1998.5", "in one thousand nine hundred ninety eight point five", "1998.5", "CARDINAL"),
    ("in 1998%", "in one thousand nine hundred ninety eight percent", "1998%", "PERCENT"),
    ("in\n1998", "in\none thousand nine hundred ninety eight", "1998", "CARDINAL"),
    ("May I record 2026 tracks?", "May I record two thousand twenty six tracks?",
     "2026", "CARDINAL"),
    ("2026-09-17", "September seventeenth two thousand twenty six", "2026-09-17", "DATE"),
    ("May. 2026", "May. two thousand twenty six", "2026", "CARDINAL"),
    ("June. 2026", "June. two thousand twenty six", "2026", "CARDINAL"),
    ("July. 2026", "July. two thousand twenty six", "2026", "CARDINAL"),
)


def stream(cli, resources, text, language="en-US", chunk=1, profile=(), overlay=()):
    command = [str(cli), "stream-json", "--resources", str(resources),
               "--language", language, "--chunk-bytes", str(chunk), *profile, *overlay, "-"]
    run = subprocess.run(command, input=text.encode(), capture_output=True,
                         timeout=15, check=False)
    assert run.returncode == 0, (command, text, run.stderr.decode(errors="replace"))
    events = [json.loads(line) for line in run.stdout.splitlines()]
    assert events and events[-1]["event"] == "end", (text, events)
    segments = [event for event in events if event["event"] == "segment"]
    offset = 0
    spans = []
    for segment in segments:
        for span in segment["spans"]:
            spans.append(dict(span, begin=span["begin"] + offset, end=span["end"] + offset))
        offset += len(segment["text"].encode())
    return {"text": "".join(segment["text"] for segment in segments), "spans": spans}


def exact_span(result, text, surface):
    source = text.encode()
    begin = source.index(surface.encode())
    end = begin + len(surface.encode())
    spans = [span for span in result["spans"]
             if (span["input_begin"], span["input_end"]) == (begin, end)]
    assert len(spans) == 1, (text, surface, result)
    return spans[0]


def date_span(result, text, surface, value, secondary, unit):
    span = exact_span(result, text, surface)
    actual = tuple(span[field] for field in ("semantic_type", "value", "secondary", "unit"))
    assert actual == ("DATE", value, secondary, unit), (text, actual, span)
    return span


def invalid_date(result, text):
    assert result["text"] == text, (text, result)
    assert len(result["spans"]) == 1, (text, result)
    span = exact_span(result, text, text)
    actual = tuple(span[field] for field in ("semantic_type", "value", "secondary", "unit"))
    assert actual == ("TEXT", "", "", ""), (text, actual)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    checks = 0

    for language, text, expected, surface, value, secondary, unit in REFERENCES:
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(args.cli, args.resources, text, language, chunk, profile)
                assert result["text"] == expected, (text, expected, profile, chunk, result)
                span = date_span(result, text, surface, value, secondary, unit)
                assert span["source_lang"] == "en-US", (text, span)
                assert span["context_lang"] == language, (text, span)
                checks += 1

    for text, expected, value in YEAR_REFERENCES:
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(args.cli, args.resources, text, profile=profile, chunk=chunk)
                assert result["text"] == expected, (text, expected, result)
                span = date_span(result, text, value, value, "", "year")
                assert span["source_lang"] == "und", (text, span)
                checks += 1

    for text, expected, surface, semantic in GUARDS:
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(args.cli, args.resources, text, profile=profile, chunk=chunk)
                assert result["text"] == expected, (text, expected, result)
                span = exact_span(result, text, surface)
                assert span["semantic_type"] == semantic, (text, span)
                if text == "2026-09-17":
                    assert span["secondary"] == "", span
                checks += 1

    # Calendar acceptance is an independent oracle; it does not judge a date
    # valid merely because the candidate returned a DATE or produced speech.
    rng = random.Random(20260917)
    calendar_cases = {(year, 2, 29) for year in (1, 4, 1000, 1600, 1700, 1900, 2000, 2024, 2026, 2100, 9999)}
    calendar_cases |= {(2026, month, day) for month in range(1, 13) for day in (28, 29, 30, 31)}
    calendar_cases |= {(rng.randrange(1, 10000), rng.randrange(1, 13), rng.randrange(1, 32))
                       for _ in range(30)}
    for index, (year, month, day) in enumerate(sorted(calendar_cases)):
        try:
            date = datetime.date(year, month, day)
        except ValueError:
            date = None
        for order in ("mdy", "dmy"):
            text = (f"{MONTHS[month - 1]} {day}, {year:04d}" if order == "mdy"
                    else f"{day} {MONTHS[month - 1]} {year:04d}")
            result = stream(args.cli, args.resources, text, chunk=7,
                            profile=PROFILES[index % len(PROFILES)])
            if date is None:
                invalid_date(result, text)
            else:
                date_span(result, text, text, date.isoformat(), order, "")
                assert len(result["spans"]) == 1 and result["text"], (text, result)
            checks += 1

    for month in range(1, 13):
        # Missing years permit February 29; other month lengths remain known.
        last = calendar.monthrange(2000, month)[1]
        for day in (last, last + 1):
            text = f"{MONTHS[month - 1]} {day}"
            result = stream(args.cli, args.resources, text, profile=PROFILES[1])
            if day <= last:
                date_span(result, text, text, f"--{month:02d}-{day:02d}", "mdy", "month-day")
            else:
                invalid_date(result, text)
            checks += 1

    for text in (
        "September 0, 2026", "September 32, 2026", "February 30, 2024",
        "April 31, 2026", "September 17, 0000", "September 17, 10000",
        "September 1th, 2026", "September 2st, 2026", "September 3nd, 2026",
        "September 11st, 2026", "September 12nd, 2026", "September 13rd, 2026",
        "17st September 2026", "31th December 2026",
    ):
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(args.cli, args.resources, text, profile=profile, chunk=chunk)
                invalid_date(result, text)
                checks += 1

    for text in ("`September 17, 2026`", "https://example.test/September/17/2026",
                 "May I proceed?", "March forward."):
        for profile in PROFILES:
            result = stream(args.cli, args.resources, text, profile=profile)
            assert result["text"] == text, (text, result)
            assert all(span["semantic_type"] != "DATE" for span in result["spans"]), (text, result)
            checks += 1

    # A named date may join horizontal whitespace but never separate lines.
    for separator in ("\n", "\r", "\r\n", "\v", "\f"):
        for text in ("September" + separator + "17, 2026", "September 17," + separator + "2026"):
            result = stream(args.cli, args.resources, text, profile=PROFILES[1])
            assert separator in result["text"], (text, result)
            for span in result["spans"]:
                if span["semantic_type"] == "DATE":
                    surface = text.encode()[span["input_begin"]:span["input_end"]].decode()
                    assert separator not in surface, (text, span)
            checks += 1

    # Reading-style overrides change speech without changing the calendar IR.
    # Resource-only new month/year cues also carry their explicit streaming
    # boundary companion so that tiny phrase targets cannot detach the date.
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "dates.tsv"
        overlay = ("--customer", str(path))
        for policy, style, text, expected, surface, value, secondary, unit in (
            ("named_year_style", "cardinal", "September 17, 2026",
             "September seventeenth two thousand twenty six", "September 17, 2026",
             "2026-09-17", "mdy", ""),
            ("numeric_year_style", "calendar", "2026-09-17",
             "September seventeenth twenty twenty six", "2026-09-17",
             "2026-09-17", "", ""),
            ("year_style", "cardinal", "in 1998",
             "in one thousand nine hundred ninety eight", "1998", "1998", "", "year"),
        ):
            path.write_text(f"date_policy\ten-US\t{policy}\t{style}\n", encoding="utf-8")
            for profile in PROFILES:
                for chunk in (1, 7, 64):
                    result = stream(args.cli, args.resources, text, chunk=chunk,
                                    profile=profile, overlay=overlay)
                    assert result["text"] == expected, (policy, text, expected, result)
                    date_span(result, text, surface, value, secondary, unit)
                    checks += 1
            path.write_text(f"date_policy\ten-US\t{policy}\tinvalid-style\n", encoding="utf-8")
            run = subprocess.run([str(args.cli), "normalize", "--resources", str(args.resources),
                                  "--language", "en-US", *overlay, text],
                                 capture_output=True, timeout=15, check=False)
            assert run.returncode != 0, (policy, run.stdout, run.stderr)
            checks += 1

        path.write_text("month_alias\ten-US\tcustommonth\t9\n"
                        "boundary_word\ten-US\tcustommonth\t1\n"
                        "date_year_cue\ten-US\tthroughout\t1\n"
                        "boundary_word\ten-US\tthroughout\t1\n", encoding="utf-8")
        for text, expected, surface, value, secondary, unit in (
            ("custommonth 17, 2026", "September seventeenth twenty twenty six",
             "custommonth 17, 2026", "2026-09-17", "mdy", ""),
            ("throughout 1998", "throughout nineteen ninety eight", "1998", "1998", "", "year"),
        ):
            for profile in PROFILES:
                for chunk in (1, 7, 64):
                    result = stream(args.cli, args.resources, text, chunk=chunk,
                                    profile=profile, overlay=overlay)
                    assert result["text"] == expected, (text, expected, result)
                    span = date_span(result, text, surface, value, secondary, unit)
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
                    fixtures = [(row[1], row[2]) for row in REFERENCES if row[0] == language]
                    if language == "en-US":
                        fixtures += [("since 1905", "since nineteen oh five"),
                                     ("February 29, 1900", "February 29, 1900")]
                    for text, expected in fixtures:
                        source = text.encode()
                        batch = engine.normalize(source)
                        assert batch["text"] == expected, (text, batch, expected)
                        for split in range(1, len(source)):
                            actual = native.flatten(engine.stream(source, [split, len(source) - split]))
                            assert actual == batch, (text, split, tiny, batch, actual)
                            native_checks += 1
                finally:
                    engine.close()

    print(f"{checks} English date text/calendar/IR/chunk/guard checks; "
          f"{native_checks} native every-two-way-byte-split checks passed")


if __name__ == "__main__":
    main()
