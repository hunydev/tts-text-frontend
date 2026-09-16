#!/usr/bin/env python3
"""Validate a UTF-8 four-column resource and wrap it in the TTFB v1 container.
Runtime source and binary loaders share validation and precedence semantics.
The payload is deliberately transparent; an indexed format can replace it later.
"""
import argparse
import pathlib
import struct


def compile_resource(source: pathlib.Path, destination: pathlib.Path) -> None:
    raw = source.read_bytes()
    if len(raw) > 16 * 1024 * 1024 - 16:
        raise ValueError("resource exceeds runtime size limit")
    text = raw.decode("utf-8")
    if "\0" in text:
        raise ValueError("NUL is not permitted")
    for number, line in enumerate(text.splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) != 4 or not all(fields):
            raise ValueError(f"line {number}: expected four nonempty TSV fields")
        if len(fields[1].encode("utf-8")) >= 24:
            raise ValueError(f"line {number}: language tag too long")
    checksum = 2166136261
    for byte in raw:
        checksum = ((checksum ^ byte) * 16777619) & 0xFFFFFFFF
    destination.write_bytes(b"TTFB\x01\x00\x00\x00" + struct.pack("<II", len(raw), checksum) + raw)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path)
    parser.add_argument("destination", type=pathlib.Path)
    args = parser.parse_args()
    try:
        compile_resource(args.source, args.destination)
    except (ValueError, UnicodeError, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
