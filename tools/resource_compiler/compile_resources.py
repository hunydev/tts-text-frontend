#!/usr/bin/env python3
"""Compile a UTF-8 resource using the same C validation as runtime loading.

The default mode requires a built ttf CLI (explicit --ttf, TTF_CLI, local build,
or PATH). --syntax-only is an explicit bootstrap escape hatch: its artifact is
not semantically certified and runtime validation still applies on load/freeze.
"""
import argparse
import json
import os
import pathlib
import shutil
import struct
import subprocess
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]


def validator_cli(explicit=None):
    candidate = str(explicit) if explicit else os.environ.get("TTF_CLI")
    if candidate:
        path = shutil.which(candidate) or candidate
        if not pathlib.Path(path).is_file():
            raise ValueError("C validator executable does not exist: " + candidate)
        return str(pathlib.Path(path).resolve())
    local = ROOT / "build/ttf"
    if local.is_file():
        return str(local)
    installed = shutil.which("ttf")
    if installed:
        return installed
    raise ValueError("Build ttf first or pass --ttf; --syntax-only is explicitly unvalidated")


def syntax(raw):
    if len(raw) > 16 * 1024 * 1024 - 16:
        raise ValueError("resource exceeds runtime size limit")
    text = raw.decode("utf-8")
    if "\0" in text:
        raise ValueError("NUL is not permitted")
    # LF only, with one CR before LF/EOF. Other Unicode separators remain data.
    for number, line in enumerate(text.split("\n"), 1):
        if line.endswith("\r"):
            line = line[:-1]
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 4 or not all(fields):
            raise ValueError(f"line {number}: expected four nonempty TSV fields")
        if len(fields[1].encode("utf-8")) >= 24:
            raise ValueError(f"line {number}: language tag too long")


def validate(raw, *, ttf=None, resources=None, layer="customer", require_complete=False,
             language=None, overrides=()):
    cli = validator_cli(ttf)
    # Validate the exact byte snapshot that will be wrapped, not a path which
    # another process may edit between validation and compilation.
    with tempfile.TemporaryDirectory(prefix="ttf-compile-validate-") as directory:
        source = pathlib.Path(directory) / "source.tsv"
        source.write_bytes(raw)
        command = [cli, "validate-resource", "--resource-layer", layer]
        if resources is not None:
            command.extend(["--resources", str(resources)])
        if language is not None:
            command.extend(["--language", language])
        for kind, path in overrides:
            if kind not in ("domain", "customer"):
                raise ValueError("overlay kind must be domain or customer")
            command.extend(["--" + kind, str(path)])
        if require_complete:
            command.append("--require-complete-validation")
        command.append(str(source))
        process = subprocess.run(command, capture_output=True, text=True,
                                 encoding="utf-8", timeout=120)
    try:
        report = json.loads(process.stdout)
        if (not isinstance(report, dict) or type(report.get("version")) is not int
                or report["version"] != 1 or report.get("event") != "resource-validation"
                or not isinstance(report.get("status"), str)):
            raise ValueError("invalid validation protocol")
        counts = ("records", "validated_records", "unvalidated_records")
        if any(type(report.get(key)) is not int or report[key] < 0 for key in counts):
            raise ValueError("invalid validation counts")
        if report["records"] != report["validated_records"] + report["unvalidated_records"]:
            raise ValueError("inconsistent validation counts")
        diagnostic = report.get("diagnostic")
        if (not isinstance(diagnostic, dict)
                or type(diagnostic.get("line")) is not int or diagnostic["line"] < 0
                or any(not isinstance(diagnostic.get(key), str)
                       for key in ("path", "kind", "language", "key", "layer", "message"))):
            raise ValueError("invalid validation diagnostic")
    except (json.JSONDecodeError, ValueError) as error:
        raise ValueError("C validator failed; use a current ttf executable: " +
                         process.stderr.strip()) from error
    if process.returncode or report.get("status") != "success":
        diagnostic = report.get("diagnostic", {})
        raise ValueError(f"line {diagnostic.get('line', 0)}: "
                         f"{diagnostic.get('kind', '')}/{diagnostic.get('language', '')}/"
                         f"{diagnostic.get('key', '')}: {diagnostic.get('message', report.get('status'))}")
    if require_complete and report["unvalidated_records"]:
        raise ValueError("C validator left resource records without a registered validator")
    return report


def compile_resource(source: pathlib.Path, destination: pathlib.Path, *, ttf=None,
                     resources=None, layer="customer", syntax_only=False,
                     require_complete=False, language=None, overrides=()):
    raw = source.read_bytes()
    syntax(raw)
    if syntax_only and require_complete:
        raise ValueError("syntax-only cannot require complete semantic validation")
    report = None if syntax_only else validate(raw, ttf=ttf, resources=resources,
                                              layer=layer, require_complete=require_complete,
                                              language=language, overrides=overrides)
    checksum = 2166136261
    for byte in raw:
        checksum = ((checksum ^ byte) * 16777619) & 0xFFFFFFFF
    payload = b"TTFB\x01\x00\x00\x00" + struct.pack("<II", len(raw), checksum) + raw
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=destination.parent, prefix=".ttfb-", delete=False) as file:
            temporary = pathlib.Path(file.name)
            file.write(payload)
            file.flush()
            os.fsync(file.fileno())
        os.replace(temporary, destination)
        temporary = None
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)
    return report


class Overlay(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        current = list(getattr(namespace, self.dest, None) or ())
        current.append((option_string[2:], values))
        setattr(namespace, self.dest, current)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    parser.add_argument("--ttf", type=pathlib.Path)
    parser.add_argument("--resources", type=pathlib.Path)
    parser.add_argument("--language")
    parser.add_argument("--domain", "--customer", dest="overrides", type=pathlib.Path,
                        action=Overlay, default=[], help="ordered deployment resource overlay")
    parser.add_argument("--layer", choices=("common", "system", "domain", "customer", "user"), default="customer")
    parser.add_argument("--syntax-only", action="store_true")
    parser.add_argument("--require-complete-validation", action="store_true")
    args = parser.parse_args()
    try:
        result = compile_resource(args.source, args.destination, ttf=args.ttf,
                                  resources=args.resources, layer=args.layer,
                                  syntax_only=args.syntax_only, require_complete=args.require_complete_validation,
                                  language=args.language, overrides=args.overrides)
        print(json.dumps({"version": 1, "event": "resource-compilation",
                          "validation": "syntax-only" if result is None else "runtime",
                          "report": result}, ensure_ascii=False))
    except (ValueError, UnicodeError, OSError, subprocess.SubprocessError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
