#!/usr/bin/env python3
"""Validate editable policy files and compiler artifacts through the public CLI.

No foreign corpus or copied resources are committed by this test. Temporary
fixtures exercise malformed policy, final layered constraints and diagnostics.
"""
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile


CLI, RESOURCES, COMPILER = map(Path, sys.argv[1:4])
CHECKS = 0


def require(condition, detail=None):
    global CHECKS
    CHECKS += 1
    assert condition, detail


def invoke(command):
    return subprocess.run([str(argument) for argument in command], capture_output=True,
                          text=True, encoding="utf-8", timeout=30)


def validate(path, *options, resources=RESOURCES, success=True):
    process = invoke([CLI, "validate-resource", "--resources", resources, *options, path])
    require((process.returncode == 0) == success, (process.args, process.stdout, process.stderr))
    report = json.loads(process.stdout)
    require(report["version"] == 1 and report["event"] == "resource-validation", report)
    require((report["status"] == "success") == success, report)
    require(report["records"] == report["validated_records"] + report["unvalidated_records"], report)
    require(set(report["diagnostic"]) == {"line", "path", "kind", "language", "key", "layer", "message"}, report)
    require(all(isinstance(report[key], int) and report[key] >= 0
                for key in ("records", "validated_records", "unvalidated_records")), report)
    if not success:
        require(bool(report["diagnostic"]["message"]), report)
    return report


def binary_resource(path, text):
    raw = text.encode("utf-8")
    checksum = 2166136261
    for byte in raw:
        checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
    path.write_bytes(b"TTFB\x01\0\0\0" + struct.pack("<II", len(raw), checksum) + raw)


def compile_resource(source, destination, *options, success=True):
    process = invoke([sys.executable, COMPILER, source, destination, "--ttf", CLI,
                      "--resources", RESOURCES, *options])
    require((process.returncode == 0) == success, (process.args, process.stdout, process.stderr))
    if success:
        report = json.loads(process.stdout)
        require(report["version"] == 1 and report["event"] == "resource-compilation", report)
        return report
    return process


with tempfile.TemporaryDirectory(prefix="ttf-policy-validation-") as directory:
    root = Path(directory)
    source = root / '고객 "정책".tsv'
    compiled = root / "policy.ttfb"
    source.write_text("# valid but empty resource\n", encoding="utf-8")
    baseline = validate(source)
    unsupported = validate(source, "--language", "xx-Unknown", success=False)
    require("Engine freeze failed" in unsupported["diagnostic"]["message"], unsupported)

    source.write_text("future.policy\t*\tkey\tcustom\n", encoding="utf-8")
    unknown = validate(source)
    require(unknown["records"] == baseline["records"] + 1, unknown)
    require(unknown["unvalidated_records"] == baseline["unvalidated_records"] + 1, unknown)
    incomplete = validate(source, "--require-complete-validation", success=False)
    require(incomplete["unvalidated_records"] == unknown["unvalidated_records"], incomplete)

    # A failing file reports its original UTF-8 pathname, the physical source
    # line and exact resource identity; the compiled wrapper changes none of it.
    malformed = ("# comment\r\nfuture.policy\t*\tfirst\tvalid\r\n\r\n"
                 "initialism_policy\ten-US\tdotted\tinvalid\r\n")
    source.write_bytes(malformed.encode("utf-8"))
    binary_resource(compiled, malformed)
    for path in (source, compiled):
        for layer in ("common", "system", "domain", "customer", "user"):
            report = validate(path, "--resource-layer", layer, success=False)
            diagnostic = report["diagnostic"]
            require(diagnostic["path"] == str(path), diagnostic)
            require(diagnostic["line"] == 4 and diagnostic["layer"] == layer, diagnostic)
            require(diagnostic["kind"] == "initialism_policy" and
                    diagnostic["language"] == "en-US" and diagnostic["key"] == "dotted", diagnostic)

    # The compiler uses the C semantic validator and replaces the artifact only
    # after successful validation. The explicit bootstrap escape hatch is not
    # a semantic certification and cannot bypass runtime validation.
    source.write_text("initialism_policy\ten-US\tdotted\tletters\n", encoding="utf-8")
    report = compile_resource(source, compiled)
    require(report["validation"] == "runtime" and report["report"]["status"] == "success", report)
    valid_artifact = compiled.read_bytes()
    validate(compiled)
    source.write_text("date_policy\ten-US\tmin_year\t99999999999999999999999999\n", encoding="utf-8")
    failed = compile_resource(source, compiled, success=False)
    require("date_policy/en-US/min_year" in failed.stderr, failed.stderr)
    require(compiled.read_bytes() == valid_artifact, "failed compile replaced valid artifact")
    report = compile_resource(source, compiled, "--syntax-only")
    require(report["validation"] == "syntax-only" and report["report"] is None, report)
    validate(compiled, success=False)
    unvalidated_artifact = compiled.read_bytes()
    compile_resource(source, compiled, "--syntax-only", "--require-complete-validation", success=False)
    require(compiled.read_bytes() == unvalidated_artifact, "contradictory flags replaced artifact")

    # A locally valid scalar can be rejected by the effective layered relation.
    domain = root / "domain.tsv"
    domain.write_text("date_policy\ten-US\tmin_year\t2200\n", encoding="utf-8")
    source.write_text("# empty\n", encoding="utf-8")
    report = validate(source, "--domain", domain, success=False)
    require(report["diagnostic"]["line"] == 0 and
            report["diagnostic"]["kind"] == "date_policy", report)
    source.write_text("date_policy\t*\tmax_year\t2300\n", encoding="utf-8")
    validate(source, "--domain", domain, "--resource-layer", "customer")
    source.write_text("date_policy\ten-US\tmin_year\t2200\n", encoding="utf-8")
    compiled.write_bytes(valid_artifact)
    compile_resource(source, compiled, success=False)
    require(compiled.read_bytes() == valid_artifact, "final constraint failure replaced artifact")

    source.write_text("future.policy\t*\tkey\tvalue\n", encoding="utf-8")
    compile_resource(source, compiled, "--require-complete-validation", success=False)
    require(compiled.read_bytes() == valid_artifact, "unknown record replaced certified artifact")

    # A missing dependency is detected before normalization on a sparse root.
    sparse = root / "sparse-resources"
    sparse.mkdir()
    (sparse / "manifest.ttfm").write_text("# no required verbalizer labels\n", encoding="utf-8")
    report = validate(source, resources=sparse, success=False)
    require(report["diagnostic"]["line"] == 0, report)
    require(bool(report["diagnostic"]["kind"] and report["diagnostic"]["key"]), report)

    # Initial manifests use the same semantic validation and source coordinates.
    initial = root / "initial-resources"
    shutil.copytree(RESOURCES, initial)
    bad = initial / '고객 "정책".tsv'
    bad.write_text("# invalid initial policy\n\ninitialism_policy\ten-US\tdotted\tinvalid\n",
                   encoding="utf-8")
    with (initial / "manifest.ttfm").open("a", encoding="utf-8") as manifest:
        manifest.write("\n" + bad.name + "\n")
    report = validate(source, resources=initial, success=False)
    require(report["diagnostic"]["path"] == str(bad) and report["diagnostic"]["line"] == 3, report)
    require(report["diagnostic"]["layer"] == "system", report)

    report = validate(root / "absent.tsv", success=False)
    require(report["status"] == "I/O error", report)
    require(report["diagnostic"]["path"] == str(root / "absent.tsv"), report)

    # Source lookup folds ASCII case; numeric override keys use canonical
    # decimal spelling. A validator must not certify silently unreachable keys.
    for kind, language, key, value in (
        ("native", "ko-KR", "01", "특수"),
        ("ordinal_first", "ko-KR", "01", "특수"),
        ("lexical", "en-US", "Pine", "INITIALISM"),
        ("context_word", "en-US", "The", "45"),
        ("context_role", "en-US", "Use", "imperative"),
        ("pronunciation", "en-US", "Zog", "AA"),
        ("initialism_pronunciation", "en-US", "ZZ", "AA"),
        ("g2p", "en-US", "ZZ", "AA"),
    ):
        source.write_text(f"{kind}\t{language}\t{key}\t{value}\n", encoding="utf-8")
        report = validate(source, success=False)
        require(report["diagnostic"]["kind"] == kind and
                report["diagnostic"]["key"] == key and
                report["diagnostic"]["line"] == 1, report)

    # Both byte and token capacity matter: 129 short phones fit the source
    # string buffer but exceed the builtin target renderer's 128-token limit.
    for phone in ("AA", "letter.A"):
        source.write_text("pronunciation\ten-US\tzog\t" +
                          " ".join([phone] * 129) + "\n", encoding="utf-8")
        validate(source, success=False)
    source.write_text("pronunciation\ten-US\tzog\t" + " ".join(["AA"] * 128) + "\n",
                      encoding="utf-8")
    validate(source, "--require-complete-validation")
    rendered = invoke([CLI, "normalize", "--resources", RESOURCES, "--customer", source,
                       "Zog는 단어입니다."])
    require(rendered.returncode == 0 and rendered.stdout.rstrip("\n") ==
            "아" * 128 + "는 단어입니다.", (rendered.stdout, rendered.stderr))

    # The target mapper accepts a direct label for any base phone. Dictionary
    # punctuation remains valid; it is not a letters-only word key.
    source.write_text("pronunciation\ten-US\tzog\tAA\n"
                      "phone_letter\tko-KR\tAA\t특수\n"
                      "pronunciation\ten-US\td'arcy\tD AA R S IY\n"
                      "pronunciation\ten-US\tlead(2)\tL EH D\n", encoding="utf-8")
    validate(source, "--require-complete-validation")
    rendered = invoke([CLI, "normalize", "--resources", RESOURCES, "--customer", source,
                       "Zog는 단어입니다."])
    require(rendered.returncode == 0 and rendered.stdout.rstrip("\n") == "특수는 단어입니다.",
            (rendered.stdout, rendered.stderr))

    # Title expansion requires the matching period to remain inside a context
    # unit. A later overlay can supply its boundary marker before freeze.
    source.write_text("abbreviation_expansion\ten-US\tbrig\tBrigadier\n", encoding="utf-8")
    report = validate(source, success=False)
    require(report["diagnostic"]["kind"] == "abbreviation" and
            report["diagnostic"]["key"] == "brig" and
            report["diagnostic"]["line"] == 0, report)
    source.write_text("abbreviation_expansion\ten-US\tbrig\tBrigadier\n"
                      "abbreviation\ten-US\tbrig\tpreserve\n", encoding="utf-8")
    validate(source, "--require-complete-validation")
    rendered = invoke([CLI, "normalize", "--resources", RESOURCES, "--language", "en-US",
                       "--customer", source, "Rev. Rowan"])
    require(rendered.returncode == 0 and rendered.stdout.rstrip("\n") == "Reverend Rowan",
            (rendered.stdout, rendered.stderr))
    # Disabled and shadowed expansions cannot require unused boundary records.
    domain.write_text("abbreviation_expansion\ten-US\trev\tReverend\n", encoding="utf-8")
    source.write_text("abbreviation_expansion\ten-US\trev\tpreserve\n", encoding="utf-8")
    validate(source, "--domain", domain, "--require-complete-validation")
    source.write_text("abbreviation_expansion\ten-US\trev\tReverend\n"
                      "abbreviation\t*\trev\tprotected\n", encoding="utf-8")
    validate(source, "--require-complete-validation")

print(f"resource CLI/compiler: {CHECKS} assertions passed")
