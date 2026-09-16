"""Check benchmark accounting/validation and CLI errors, without latency thresholds."""
from pathlib import Path
import json
import math
import subprocess
import sys
import tempfile


bench, resources = sys.argv[1:]


def invoke(*args):
    return subprocess.run(
        [bench, "--resources", resources, *map(str, args)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )


def fnv1a64(text):
    value = 14695981039346656037
    for byte in text.encode("utf-8"):
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return f"{value:016x}"


def check_report(result, source, expected, iterations, chunk_bytes):
    assert result.returncode == 0, result.stderr
    report = json.loads(result.stdout)
    assert report["schema_version"] == 1
    assert report["metric"] == "frontend_commit_latency_not_audio_ttfb"
    assert report["clock"] in {
        "CLOCK_MONOTONIC", "QueryPerformanceCounter", "timespec_get(TIME_UTC)"
    }
    assert report["input_bytes"] == len(source.encode("utf-8"))
    assert report["output_bytes"] == len(expected.encode("utf-8"))
    assert report["output_fnv1a64"] == fnv1a64(expected)
    assert report["iterations"] == iterations
    assert report["chunk_bytes"] == chunk_bytes
    assert report["validation"] == "exact_preflight_and_fnv1a64_each_iteration"
    assert report["callbacks"] > 0
    assert report["total_callbacks"] == report["callbacks"] * iterations
    assert 0 < report["first_callback_committed_input_bytes"] <= report["input_bytes"]
    assert report["first_callback_committed_input_bytes"] <= report["first_callback_input_bytes"]
    assert report["first_callback_input_bytes"] <= report["input_bytes"]
    assert report["first_callback_before_final_input_runs"] in (0, iterations)
    assert report["first_callback_before_flush_runs"] in (0, iterations)
    assert math.isfinite(report["engine_setup_ms"]) and report["engine_setup_ms"] >= 0
    for field in ("first_callback_ms", "total_ms"):
        stats = report[field]
        values = [stats[key] for key in ("min", "p50", "p95", "max")]
        assert all(math.isfinite(value) and value >= 0 for value in values)
        assert values == sorted(values)
    for key in ("min", "p50", "p95", "max"):
        assert report["first_callback_ms"][key] <= report["total_ms"][key]
    throughput = report["throughput_mib_s"]
    assert throughput is None or (math.isfinite(throughput) and throughput > 0)
    return report


source = "Apple은 사과입니다.\nPlease go to the 1st floor.\nGPU를 사용합니다.\n" * 8
expected = "애플은 사과입니다.\nPlease go to the first floor.\n지피유를 사용합니다.\n" * 8
targets = ["--first-target", "24", "--normal-target", "40", "--max-length", "80"]
text_report = check_report(
    invoke("--text", source, "--iterations", "3", "--chunk-bytes", "7", *targets),
    source, expected, 3, 7,
)
assert text_report["first_callback_before_final_input_runs"] == 3
assert text_report["first_callback_before_flush_runs"] == 3
assert text_report["first_segment_target"] == 24
assert text_report["normal_segment_target"] == 40
assert text_report["max_segment_length"] == 80

short = "Apple은"
short_report = check_report(invoke("--text", short), short, "애플은", 20, 64)
assert short_report["callbacks"] == 1
assert short_report["first_callback_before_final_input_runs"] == 0
assert short_report["first_callback_before_flush_runs"] == 0

with tempfile.TemporaryDirectory(prefix="ttf-bench-test-") as temp:
    path = Path(temp) / "input.txt"
    path.write_bytes(source.encode("utf-8"))
    file_report = check_report(
        invoke(path, "--iterations", "2", "--chunk-bytes", "1", *targets),
        source, expected, 2, 1,
    )
    assert file_report["callbacks"] == text_report["callbacks"]
    assert file_report["first_callback_committed_input_bytes"] == text_report[
        "first_callback_committed_input_bytes"
    ]
    assert file_report["first_callback_before_final_input_runs"] == 2
    for invalid in (b"\xff", b"\xed\xa0\x80", b"\xea\xb0", b"abc\0def", b""):
        path.write_bytes(invalid)
        result = invoke(path, "--iterations", "1")
        assert result.returncode != 0 and result.stderr and not result.stdout
    result = invoke(Path(temp) / "missing.txt")
    assert result.returncode != 0 and result.stderr

for args in (
    (), ("--text",), ("--text", ""), ("--text", "x", "other.txt"),
    ("--text", "x", "--unknown"), ("--text", "x", "--iterations"),
    ("--text", "x", "--iterations", "0"), ("--text", "x", "--iterations", "-1"),
    ("--text", "x", "--iterations", "999999999999999999999999999999"),
    ("--text", "x", "--iterations", "+2"), ("--text", "x", "--chunk-bytes", "1.5"),
    ("--text", "x", "--first-target", "0"), ("--text", "x", "--normal-target", "no"),
    ("--text", "x", "--max-length", "0"), ("--text", "x", "--resources"),
    ("--text", "x", "--resources", ""),
    ("--text", "x", "--resources", "/nonexistent-ttf-benchmark-resource-root"),
):
    result = invoke(*args)
    assert result.returncode != 0 and result.stderr and not result.stdout, args

print("benchmark JSON, exact output hashes, incremental callbacks, UTF-8 chunking and errors passed")
