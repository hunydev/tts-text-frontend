"""Clock references derived from time meaning and explicit English text policy.

References below were authored before running the changed candidate. Midnight is
not 'zero o'clock'. Full 24-hour values above twelve use the project choice
'hundred hours'; this is a supported text style, not a uniquely correct utterance.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

cli, resources = sys.argv[1:]
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "evaluation"))
from evaluate import check_spans, parse_stream  # noqa: E402

# surface, expected text, normalized semantic TIME value; None = unsupported/raw.
REFERENCES = (
    ("0:00", "midnight", "00:00"),
    ("00:00", "midnight", "00:00"),
    ("00:00:00", "midnight", "00:00:00"),
    ("00:00:01", "one second after midnight", "00:00:01"),
    ("00:00:59", "fifty nine seconds after midnight", "00:00:59"),
    ("13:00", "thirteen hundred hours", "13:00"),
    ("14:00", "fourteen hundred hours", "14:00"),
    ("23:00", "twenty three hundred hours", "23:00"),
    ("13:00:01", "thirteen hundred hours and one second", "13:00:01"),
    ("23:00:00", "twenty three hundred hours and zero seconds", "23:00:00"),
    ("00:30", "zero thirty", "00:30"),
    ("00:05", "zero oh five", "00:05"),
    ("14:30", "fourteen thirty", "14:30"),
    ("14:30:05", "fourteen thirty and five seconds", "14:30:05"),
    ("09:00", "nine o'clock", "09:00"),
    ("09:00:01", "nine o'clock and one second", "09:00:01"),
    ("12:00", "twelve o'clock", "12:00"),
    # Was twelve o'clock a m / p m: a meridiem replaces o'clock (2026-10-07).
    ("12:00 AM", "twelve a m", "12:00"),
    ("12:00 PM", "twelve p m", "12:00"),
    ("9:05 AM", "nine oh five a m", "09:05"),
    ("24:00", "24:00", None),  # Existing unsupported end-of-day notation policy.
    ("00:00 AM", "00:00 AM", None),  # Mixed 00-hour and 12-hour suffix.
    ("23:00 PM", "23:00 PM", None),
    ("12:60", "12:60", None),
)
profiles = [(), ("--first-target", "1", "--normal-target", "3", "--max-length", "12")]
checks = 0


def check(text, expected, span=None, options=(), profile=()):
    global checks
    base = ["--resources", resources, "--language", "en-US", *profile, *options]
    run = subprocess.run([cli, "normalize", *base, "--", text], capture_output=True, timeout=10)
    assert run.returncode == 0, (text, run.stderr)
    assert run.stdout.decode() == expected + "\n", (text, expected, run.stdout)
    previous = None
    for chunk in (1, 7, 4096):
        run = subprocess.run([cli, "stream-json", *base, "--chunk-bytes", str(chunk), "-"],
                             input=text.encode(), capture_output=True, timeout=10)
        assert run.returncode == 0, (text, run.stderr)
        segments = parse_stream(run.stdout.decode(), text)
        assert "".join(segment["text"] for segment in segments) == expected, (text, segments)
        if span:
            failures = check_spans(dict(text=text, expected_spans=[span]), segments)
            assert not failures, (text, failures)
        if previous is not None:
            assert previous == segments, (text, "chunk-dependent clock output or metadata")
        previous = segments
        checks += 1


for surface, expected, value in REFERENCES:
    span = None if value is None else dict(surface=surface, normalized=expected, value=value,
                                           semantic_type="TIME", context_lang="en-US",
                                           render_lang="en-US", verbalizer="en-US")
    for profile in profiles:
        check(surface, expected, span=span, profile=profile)

check("The meeting starts at 00:00.", "The meeting starts at midnight.",
      dict(surface="00:00", normalized="midnight", semantic_type="TIME", value="00:00"))
check("The meeting starts at 23:00.", "The meeting starts at twenty three hundred hours.",
      dict(surface="23:00", normalized="twenty three hundred hours", semantic_type="TIME",
           value="23:00"))

# Each key is editable text data. Customer data may override a domain reading;
# separate fresh engines prove these choices do not mutate the default engine.
with tempfile.TemporaryDirectory(prefix="ttf-clock-reference-") as directory:
    domain = Path(directory) / "domain.tsv"
    customer = Path(directory) / "customer.tsv"
    domain.write_text("clock\ten-US\tmidnight\tzero hundred hours\n", encoding="utf-8")
    customer.write_text("clock\ten-US\tmidnight\tthe start of the day\n"
                        "clock\ten-US\tafter\tpast\n"
                        "clock\ten-US\th24_full_hour\thundred\n", encoding="utf-8")
    check("00:00", "zero hundred hours", options=("--domain", str(domain)))
    for profile in profiles:
        options = ("--domain", str(domain), "--customer", str(customer))
        check("00:00", "the start of the day", options=options, profile=profile)
        check("00:00:01", "one second past the start of the day", options=options, profile=profile)
        check("14:00", "fourteen hundred", options=options, profile=profile)
    check("00:00", "midnight")

print(f"{checks} English clock reference/metadata/chunk/resource checks passed")
