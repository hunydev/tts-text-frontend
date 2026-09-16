"""Independent endpoint references for English clock intervals, text only."""
from pathlib import Path
import subprocess
import sys

cli, resources = sys.argv[1:]
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "evaluation"))
from evaluate import check_spans, parse_stream  # noqa: E402

# Freeze expected endpoints before testing the candidate classifier/verbalizer.
REFERENCES = (
    ("09:00~18:00", "nine o'clock to eighteen hundred hours", "09:00", "18:00"),
    ("09:00-18:00", "nine o'clock to eighteen hundred hours", "09:00", "18:00"),
    ("09:00 ~ 18:00", "nine o'clock to eighteen hundred hours", "09:00", "18:00"),
    ("00:00~00:30", "midnight to zero thirty", "00:00", "00:30"),
    ("23:30~00:00", "twenty three thirty to midnight", "23:30", "00:00"),
    ("09:00:01~09:00:05", "nine o'clock and one second to nine o'clock and five seconds",
     "09:00:01", "09:00:05"),
    ("00:00:01~00:00:59", "one second after midnight to fifty nine seconds after midnight",
     "00:00:01", "00:00:59"),
)
checks = 0
for text, expected, first, last in REFERENCES:
    for profile in ((), ("--first-target", "1", "--normal-target", "3", "--max-length", "12")):
        options = ["--resources", resources, "--language", "en-US", *profile]
        batch = subprocess.run([cli, "normalize", *options, "--", text], capture_output=True, timeout=10)
        assert batch.returncode == 0, (text, batch.stderr)
        assert batch.stdout.decode() == expected + "\n", (text, batch.stdout, expected)
        previous = None
        for chunk in (1, 7, 4096):
            run = subprocess.run([cli, "stream-json", *options, "--chunk-bytes", str(chunk), "-"],
                                 input=text.encode(), capture_output=True, timeout=10)
            assert run.returncode == 0, (text, run.stderr)
            segments = parse_stream(run.stdout.decode(), text)
            assert "".join(segment["text"] for segment in segments) == expected, (text, segments)
            failure = check_spans(dict(text=text, expected_spans=[dict(
                surface=text, normalized=expected, value=first, semantic_type="RANGE", unit="time",
                context_lang="en-US", render_lang="en-US")]), segments)
            assert not failure, (text, failure)
            spans = [span for segment in segments for span in segment["spans"]]
            assert len(spans) == 1 and spans[0]["secondary"] == last, (text, spans)
            if previous is not None:
                assert previous == segments, (text, "chunk-dependent interval metadata")
            previous = segments
            checks += 1
print(f"{checks} English clock-interval endpoint/IR/chunk reference checks passed")
