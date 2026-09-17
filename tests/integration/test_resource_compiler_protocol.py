#!/usr/bin/env python3
"""Compiler protocol rejection, atomic artifacts and deployment context parity.

Subprocess responses are mocked in-process, without executable shell fixtures, so
protocol tests also run on Windows. Context tests invoke the actual C validator.
"""
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from unittest.mock import patch


CLI, RESOURCES, COMPILER = (path.resolve() for path in map(Path, sys.argv[1:4]))
SPEC = importlib.util.spec_from_file_location("ttf_resource_compiler_protocol_target", COMPILER)
COMPILER_MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMPILER_MODULE)
CHECKS = 0


def require(condition, detail=None):
    global CHECKS
    CHECKS += 1
    assert condition, detail


def success_report():
    return {"version": 1, "event": "resource-validation", "status": "success",
            "records": 1, "validated_records": 1, "unvalidated_records": 0,
            "diagnostic": {"line": 0, "path": "", "kind": "", "language": "",
                           "key": "", "layer": "common", "message": ""}}


def mutation(path, value, *, missing=False):
    report = success_report()
    parent = report
    for key in path[:-1]:
        parent = parent[key]
    if missing:
        del parent[path[-1]]
    else:
        parent[path[-1]] = value
    return report


def invoke(command):
    return subprocess.run([str(argument) for argument in command], capture_output=True,
                          text=True, encoding="utf-8", timeout=30)


def compile_command(source, destination, *options):
    return [sys.executable, COMPILER, source, destination, "--ttf", CLI,
            "--resources", RESOURCES, *options]


with tempfile.TemporaryDirectory(prefix="ttf-compiler-protocol-") as directory:
    root = Path(directory)
    source, output = root / "source.tsv", root / "resource.ttfb"
    raw = "exact\t*\tprotocol-sample\t검사\n".encode("utf-8")
    source.write_bytes(raw)
    previous = b"previous artifact must survive every rejected protocol"
    output.write_bytes(previous)

    malformed = [("non-object", []), ("empty", {}),
                 ("null", None)]
    for field, values in (
        ("version", (0, 2, True, 1.0, "1", None)),
        ("event", ("other-event", 1, None)),
        ("status", (0, True, None)),
        ("records", (-1, 1.0, True, "1", None)),
        ("validated_records", (-1, 1.0, True, "1", None)),
        ("unvalidated_records", (-1, 0.0, False, "0", None)),
        ("diagnostic", (None, [], "message", 0)),
    ):
        for value in values:
            malformed.append((f"{field}={value!r}", mutation((field,), value)))
        malformed.append((f"missing-{field}", mutation((field,), None, missing=True)))
    malformed.append(("counts-do-not-add-up", mutation(("records",), 2)))
    for value in (-1, 0.0, False, "0", None):
        malformed.append((f"diagnostic-line={value!r}", mutation(("diagnostic", "line"), value)))
    malformed.append(("missing-diagnostic-line", mutation(("diagnostic", "line"), None,
                                                          missing=True)))
    for field in ("path", "kind", "language", "key", "layer", "message"):
        malformed.append((f"missing-diagnostic-{field}", mutation(("diagnostic", field), None,
                                                                  missing=True)))
        for value in (None, 7, False, [], {}):
            malformed.append((f"diagnostic-{field}={value!r}",
                              mutation(("diagnostic", field), value)))

    def rejected_response(label, stdout, returncode=0, strict=False):
        process = subprocess.CompletedProcess([str(CLI)], returncode, stdout, "validator diagnostic")
        with patch.object(COMPILER_MODULE.subprocess, "run", return_value=process) as run:
            try:
                COMPILER_MODULE.compile_resource(source, output, ttf=CLI, resources=RESOURCES,
                                                  require_complete=strict)
            except ValueError:
                pass
            else:
                raise AssertionError(f"accepted invalid validator protocol: {label}")
            require(run.call_count == 1, label)
        require(output.read_bytes() == previous, (label, "existing artifact changed"))
        require(not list(root.glob(".ttfb-*")), (label, "temporary artifact leaked"))

    for label, report in malformed:
        rejected_response(label, json.dumps(report))
    for stdout in ("", "not JSON", "{}\n{}", '{"version":'):
        rejected_response(f"invalid-json-{stdout!r}", stdout)
    rejected_response("nonzero-exit-with-success", json.dumps(success_report()), returncode=1)
    rejected_response("signal-exit-with-success", json.dumps(success_report()), returncode=-15)
    failed = success_report()
    failed["status"] = "invalid resource"
    failed["diagnostic"]["message"] = "invalid policy"
    rejected_response("zero-exit-with-failure-report", json.dumps(failed))
    incomplete = success_report()
    incomplete.update(validated_records=0, unvalidated_records=1)
    rejected_response("strict-unknown-even-if-subprocess-ignored-flag", json.dumps(incomplete),
                      strict=True)

    # A valid success response is accepted, and bytes submitted to the validator
    # are exactly the snapshot wrapped in the artifact even if the source path
    # changes while the validator is running.
    captured = []
    overlays = [("domain", root / "first domain.tsv"),
                ("customer", root / "customer.tsv"),
                ("domain", root / "second domain.tsv")]

    def validated_snapshot(command, **kwargs):
        captured.extend(command)
        require(Path(command[-1]).read_bytes() == raw, "validator did not receive source snapshot")
        require(kwargs.get("encoding") == "utf-8" and kwargs.get("capture_output"), kwargs)
        source.write_text("exact\t*\tchanged-while-validating\tnew-value\n", encoding="utf-8")
        return subprocess.CompletedProcess(command, 0, json.dumps(success_report()), "")

    with patch.object(COMPILER_MODULE.subprocess, "run", side_effect=validated_snapshot):
        report = COMPILER_MODULE.compile_resource(source, output, ttf=CLI, resources=RESOURCES,
                                                  language="en-US", overrides=overlays,
                                                  require_complete=True)
    require(report == success_report(), report)
    require(output.read_bytes().startswith(b"TTFB\x01\0\0\0"), "missing compiled wrapper")
    require(output.read_bytes()[16:] == raw, "compiled bytes differ from validated snapshot")
    require(captured[captured.index("--language") + 1] == "en-US", captured)
    observed = []
    for i, argument in enumerate(captured[:-1]):
        if argument in ("--domain", "--customer"):
            observed.append((argument[2:], captured[i + 1]))
    require(observed == [(kind, str(path)) for kind, path in overlays], captured)
    require("--require-complete-validation" in captured, captured)
    require(not list(root.glob(".ttfb-*")), "success leaked temporary artifacts")

    # Non-strict mode may carry unknown namespaces, but cannot call them handled.
    source.write_bytes(raw)
    with patch.object(COMPILER_MODULE.subprocess, "run", return_value=
                      subprocess.CompletedProcess([str(CLI)], 0, json.dumps(incomplete), "")):
        report = COMPILER_MODULE.compile_resource(source, output, ttf=CLI, resources=RESOURCES)
    require(report["unvalidated_records"] == 1, report)

    # Real engine final constraints require the actual deployment context.
    source.write_text("date_policy\ten-US\tmin_year\t2200\n", encoding="utf-8")
    domain = root / "domain maximum.tsv"
    domain.write_text("date_policy\ten-US\tmax_year\t2300\n", encoding="utf-8")
    old = output.read_bytes()
    process = invoke(compile_command(source, output, "--language", "en-US"))
    require(process.returncode != 0 and "date_policy" in process.stderr, process)
    require(output.read_bytes() == old, "missing context failure replaced artifact")
    process = invoke(compile_command(source, output, "--language", "en-US", "--domain", domain,
                                     "--require-complete-validation"))
    require(process.returncode == 0, (process.stdout, process.stderr))
    compiled = json.loads(process.stdout)
    require(compiled["validation"] == "runtime" and compiled["report"]["status"] == "success",
            compiled)
    process = invoke([CLI, "validate-resource", "--resources", RESOURCES, "--language", "en-US",
                      "--domain", domain, output])
    require(process.returncode == 0, (process.stdout, process.stderr))

    # Same-layer declaration order remains observable through repeatable flags.
    lower = root / "earlier lower maximum.tsv"
    lower.write_text("date_policy\ten-US\tmax_year\t2000\n", encoding="utf-8")
    process = invoke(compile_command(source, output, "--domain", domain, "--domain", lower))
    require(process.returncode == 0, (process.stdout, process.stderr))
    old = output.read_bytes()
    process = invoke(compile_command(source, output, "--domain", lower, "--domain", domain))
    require(process.returncode != 0 and "date_policy" in process.stderr, process)
    require(output.read_bytes() == old, "reversed overlay order failure replaced artifact")

    # Customer precedence and language forwarding are exercised against C too.
    process = invoke(compile_command(source, output, "--domain", lower, "--customer", domain))
    require(process.returncode == 0, (process.stdout, process.stderr))
    old = output.read_bytes()
    process = invoke(compile_command(source, output, "--domain", domain, "--language", "xx-Unknown"))
    require(process.returncode != 0, (process.stdout, process.stderr))
    require(output.read_bytes() == old, "unsupported language failure replaced artifact")

print(f"resource compiler protocol/context: {CHECKS} assertions passed")
