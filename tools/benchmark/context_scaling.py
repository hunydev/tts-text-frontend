#!/usr/bin/env python3
r"""Reproduce synthetic whole-unit context-scaling measurements, without audio.

Example:
  python3 tools/benchmark/context_scaling.py \
    --before benchmark/generated/baseline-0.18.0/ttf \
    --before-resources benchmark/generated/baseline-0.18.0/resources \
    --after build/ttf --after-resources resources --output /tmp/context-scaling.json

The default stdin transport uses plain `ttf stream`, a 65536-byte input chunk,
no inspection JSON, and segment targets larger than the entire source. These
unpunctuated inputs remain one context unit until flush. Optional `normalize`
uses an argv text argument and includes the CLI's final newline in its reference;
large argv strings may exceed platform limits (notably Windows).

Each measured process includes CLI startup, resource loading, normalization and
output. Timings are local observations, not TTFB, a hardware-independent score,
or evidence of general linear complexity. Counter suffixes deliberately exercise
partial-token consumption and its remaining cache invalidation cost. Expected
texts are authored from explicit existing digit/counter/preservation contracts;
they are never learned from either executable's output.

Timeouts are recorded observations, and do not alone cause a nonzero exit code.
Process errors or incorrect output return 1 after writing the complete report.
Use --require-after-complete to additionally fail on an after-side timeout.
GNU time RSS is optional on Linux; other environments retain wall-time results.
The output must be outside the measured resource directories. Inputs are hashed
again after measurement; any change invalidates the comparison and returns 1.
"""
import argparse
import datetime
import hashlib
import json
import math
import os
import platform
import signal
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def positive_int(value):
    parsed = int(value)
    if parsed < 1:
        raise argparse.ArgumentTypeError("must be a positive integer")
    return parsed


def positive_float(value):
    parsed = float(value)
    if not math.isfinite(parsed) or parsed <= 0:
        raise argparse.ArgumentTypeError("must be a finite positive number")
    return parsed


def file_digest(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def resource_digest(directory):
    """Sorted relative UTF-8 path + NUL + raw SHA256 bytes, for every file."""
    digest = hashlib.sha256()
    count, total = 0, 0
    for path in sorted(directory.rglob("*"), key=lambda item: item.relative_to(directory).as_posix()):
        if not path.is_file():
            continue
        relative = path.relative_to(directory).as_posix().encode("utf-8")
        digest.update(relative + b"\0" + bytes.fromhex(file_digest(path)))
        count += 1
        total += path.stat().st_size
    return dict(sha256=digest.hexdigest(), files=count, bytes=total,
                algorithm="sorted relative UTF-8 path, NUL, raw file SHA256 bytes")


def gnu_time_provider(disabled):
    if disabled or platform.system() != "Linux":
        return None
    candidate = Path("/usr/bin/time")
    if not candidate.is_file() or not os.access(candidate, os.X_OK):
        return None
    try:
        probe = subprocess.run([str(candidate), "--version"], capture_output=True, timeout=2)
        if probe.returncode == 0 and b"GNU" in probe.stdout + probe.stderr:
            return str(candidate)
    except (OSError, subprocess.TimeoutExpired):
        pass
    return None


def workloads(args):
    families = (
        ("numerals", "en-US", "7", "seven", args.numeral_counts),
        ("english-words", "en-US", "hello", "hello", args.word_counts),
        ("partial-ko-counters", "ko-KR", "3개는", "세 개는", args.counter_counts),
    )
    for family, language, source_unit, expected_unit, counts in families:
        for count in counts:
            source = " ".join([source_unit] * count)
            expected = " ".join([expected_unit] * count)
            yield dict(id=f"{family}-{count}", family=family, language=language, count=count,
                       source_unit=source_unit, expected_unit=expected_unit, separator=" ",
                       source=source.encode(), expected=expected.encode())


def terminate_process(process):
    # GNU time and its CLI child share this dedicated session on POSIX. Killing
    # only the wrapper would leave a timed-out normalizer running in the VM.
    if os.name == "posix":
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    else:
        process.kill()
    return process.communicate()


def output_difference(actual, expected):
    limit = min(len(actual), len(expected))
    at = next((index for index in range(limit) if actual[index] != expected[index]), limit)
    begin, end = max(0, at - 24), at + 48
    return dict(first_different_byte=at,
                expected_excerpt=expected[begin:end].decode("utf-8", errors="replace"),
                actual_excerpt=actual[begin:end].decode("utf-8", errors="replace"))


def run_one(cli, resources, workload, args, rss_provider):
    # Byte count is at least the Unicode character count, so these targets are
    # beyond the source under either counting convention.
    target = len(workload["source"]) + 1
    command = [str(cli), "stream" if args.transport == "stdin" else "normalize",
               "--resources", str(resources), "--language", workload["language"],
               "--first-target", str(target), "--normal-target", str(target),
               "--max-length", str(target)]
    expected = workload["expected"]
    if args.transport == "stdin":
        command += ["--chunk-bytes", "65536", "-"]
        input_bytes = workload["source"]
    else:
        command += [workload["source"].decode("utf-8")]
        input_bytes = None
        expected += os.linesep.encode()
    record = dict(segment_target=target, expected_stdout_bytes=len(expected),
                  expected_stdout_sha256=hashlib.sha256(expected).hexdigest(),
                  elapsed_seconds=None, returncode=None, timed_out=False,
                  output_match=None, stdout_bytes=None, stdout_sha256=None, peak_rss_kib=None)
    with tempfile.TemporaryDirectory(prefix="ttf-context-benchmark-") as temporary:
        rss_path = Path(temporary) / "rss.txt"
        measured = ([rss_provider, "-f", "%M", "-o", str(rss_path), *command]
                    if rss_provider else command)
        started = time.perf_counter()
        try:
            process = subprocess.Popen(measured, stdin=subprocess.PIPE if input_bytes is not None else subprocess.DEVNULL,
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                       start_new_session=os.name == "posix")
            try:
                stdout, stderr = process.communicate(input=input_bytes, timeout=args.timeout_seconds)
            except subprocess.TimeoutExpired:
                stdout, stderr = terminate_process(process)
                record["timed_out"] = True
            record["returncode"] = process.returncode
            record["stdout_bytes"] = len(stdout)
            record["stdout_sha256"] = hashlib.sha256(stdout).hexdigest()
            if record["timed_out"]:
                record["status"] = "timeout"
            elif process.returncode:
                record["status"] = "process-error"
            else:
                record["output_match"] = stdout == expected
                record["status"] = "ok" if record["output_match"] else "output-mismatch"
                if not record["output_match"]:
                    record["difference"] = output_difference(stdout, expected)
            if stderr:
                record["stderr_excerpt"] = stderr[:2048].decode("utf-8", errors="replace")
        except OSError as error:
            record["status"] = "process-error"
            record["error"] = str(error)
        record["elapsed_seconds"] = round(time.perf_counter() - started, 6)
        if rss_provider and rss_path.exists():
            for line in reversed(rss_path.read_text(errors="replace").splitlines()):
                if line.strip().isdigit():
                    record["peak_rss_kib"] = int(line.strip())
                    break
    return record


def write_report(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent,
                                         prefix=path.name + ".", suffix=".tmp", delete=False) as output:
            temporary = Path(output.name)
            json.dump(report, output, ensure_ascii=False, indent=2, allow_nan=False)
            output.write("\n")
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def comparison(workload_id, runs, repetitions):
    values = {side: [row for row in runs if row["workload"] == workload_id and row["side"] == side]
              for side in ("before", "after")}
    result = dict(workload=workload_id, speedup_ratio=None)
    medians = {}
    for side, rows in values.items():
        complete = len(rows) == repetitions and all(row["status"] == "ok" for row in rows)
        medians[side] = statistics.median(row["elapsed_seconds"] for row in rows) if complete else None
        rss = [row["peak_rss_kib"] for row in rows if row["peak_rss_kib"] is not None]
        result[side] = dict(successful_runs=sum(row["status"] == "ok" for row in rows),
                            timeouts=sum(row["status"] == "timeout" for row in rows),
                            median_elapsed_seconds=medians[side],
                            median_peak_rss_kib=statistics.median(rss) if complete and len(rss) == repetitions else None)
    # No ratio is inferred from a censored timeout or a partially successful set.
    if medians["before"] is not None and medians["after"] is not None and medians["after"] > 0:
        result["speedup_ratio"] = round(medians["before"] / medians["after"], 6)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    for side in ("before", "after"):
        parser.add_argument("--" + side, required=True, type=Path, help="TTS frontend CLI executable")
        parser.add_argument("--" + side + "-resources", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--timeout-seconds", type=positive_float, default=30)
    parser.add_argument("--repetitions", type=positive_int, default=1)
    parser.add_argument("--numeral-counts", nargs="+", type=positive_int, default=[3000, 30000])
    parser.add_argument("--word-counts", nargs="+", type=positive_int, default=[1000, 3000])
    parser.add_argument("--counter-counts", nargs="+", type=positive_int, default=[100, 300])
    parser.add_argument("--transport", choices=("stdin", "normalize"), default="stdin")
    parser.add_argument("--no-rss", action="store_true", help="disable optional Linux GNU time peak RSS")
    parser.add_argument("--require-after-complete", action="store_true")
    args = parser.parse_args()
    for name in ("numeral_counts", "word_counts", "counter_counts"):
        counts = getattr(args, name)
        if len(set(counts)) != len(counts):
            parser.error(name.replace("_", "-") + " must not contain duplicate counts")
    binaries, resources = {}, {}
    for side in ("before", "after"):
        binaries[side] = getattr(args, side).resolve()
        resources[side] = getattr(args, side + "_resources").resolve()
        if not binaries[side].is_file() or not resources[side].is_dir():
            parser.error(f"{side} requires an existing executable and resource directory")
    args.output = args.output.resolve()
    if args.output in binaries.values():
        parser.error("output cannot replace an input executable")
    for directory in resources.values():
        if args.output == directory or directory in args.output.parents:
            parser.error("output must be outside both measured resource directories")
    rss_provider = gnu_time_provider(args.no_rss)
    report = dict(schema_version=1, complete=False, started_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  benchmark="synthetic whole-unit text normalization", transport=args.transport,
                  timeout_seconds=args.timeout_seconds, repetitions=args.repetitions,
                  caveats=["Includes CLI startup, resource loading, normalization and stdout collection.",
                           "Not TTFB or audio evaluation; not a hardware-independent or general-linear claim.",
                           "Timeouts are censored observations, not completed latency measurements.",
                           "Counter suffixes exercise partial-token invalidation; this cost may remain nonlinear.",
                           "stdin uses one context unit until flush; input chunks are at most 65536 bytes.",
                           "No warm-up; each repetition starts a fresh process; before then after are interleaved."],
                  environment=dict(platform=platform.platform(), python=sys.version.split()[0], cpu_count=os.cpu_count(),
                                   rss_provider=rss_provider, rss_unit="KiB" if rss_provider else None),
                  inputs={}, workloads=[], runs=[])
    for side in ("before", "after"):
        report["inputs"][side] = dict(cli=str(binaries[side]), cli_sha256=file_digest(binaries[side]),
                                      resources=str(resources[side]), resource_digest=resource_digest(resources[side]))
    for workload in workloads(args):
        report["workloads"].append({key: value for key, value in workload.items() if key not in ("source", "expected")})
        report["workloads"][-1].update(source_bytes=len(workload["source"]),
                                        source_sha256=hashlib.sha256(workload["source"]).hexdigest(),
                                        expected_text_bytes=len(workload["expected"]),
                                        expected_text_sha256=hashlib.sha256(workload["expected"]).hexdigest())
        for repetition in range(1, args.repetitions + 1):
            for side in ("before", "after"):
                row = run_one(binaries[side], resources[side], workload, args, rss_provider)
                row.update(workload=workload["id"], side=side, repetition=repetition)
                report["runs"].append(row)
                write_report(args.output, report)  # Preserve finished observations during long comparisons.
                print(f"{side} {workload['id']} #{repetition}: {row['status']} {row['elapsed_seconds']:.3f}s"
                      + (f" peak RSS {row['peak_rss_kib']} KiB" if row["peak_rss_kib"] is not None else ""), flush=True)
    report["comparisons"] = [comparison(row["id"], report["runs"], args.repetitions) for row in report["workloads"]]
    report["input_stability"] = {}
    for side in ("before", "after"):
        stability = dict(unchanged=False)
        try:
            cli_hash = file_digest(binaries[side])
            resources_hash = resource_digest(resources[side])
            stability.update(final_cli_sha256=cli_hash, final_resource_digest=resources_hash,
                             cli_unchanged=cli_hash == report["inputs"][side]["cli_sha256"],
                             resources_unchanged=resources_hash == report["inputs"][side]["resource_digest"])
            stability["unchanged"] = stability["cli_unchanged"] and stability["resources_unchanged"]
        except OSError as error:
            stability["error"] = str(error)
        report["input_stability"][side] = stability
    inputs_unchanged = all(row["unchanged"] for row in report["input_stability"].values())
    if not inputs_unchanged:
        # Measurements remain inspectable, but a changed input is not a valid
        # before/after speed comparison.
        for row in report["comparisons"]:
            row["speedup_ratio"] = None
        print("Input binary or resources changed during measurement; comparison invalid.", file=sys.stderr)
    report["finished_utc"] = datetime.datetime.now(datetime.timezone.utc).isoformat()
    report["complete"] = True
    write_report(args.output, report)
    failed = not inputs_unchanged or any(row["status"] in ("process-error", "output-mismatch") or
                 (args.require_after_complete and row["side"] == "after" and row["timed_out"])
                 for row in report["runs"])
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
