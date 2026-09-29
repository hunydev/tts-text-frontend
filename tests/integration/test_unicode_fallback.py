#!/usr/bin/env python3
"""Unicode spoken-fallback files: format, content and engine failure behaviour.

The committed triples in resources/unicode-fallback must decode to exactly the
reviewed <tag>.tsv rows, carry one build id, keep sequence records sorted and
terminate every name inside the pool. The engine must run without the files,
and must refuse a present but damaged triple instead of reading past it.
"""
import argparse
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
KEY_BYTES, RECORD = 60, 64


def decode(directory, tag):
    idx = (directory / (tag + ".idx")).read_bytes()
    dat = (directory / (tag + ".dat")).read_bytes()
    seq = (directory / (tag + ".seq")).read_bytes()
    assert idx[:8] == b"TTFUFIX1" and dat[:8] == b"TTFUFDT1" and seq[:8] == b"TTFUFSQ1", tag
    build_id, limit = struct.unpack("<II", idx[8:16])
    assert struct.unpack("<I", dat[8:12])[0] == build_id == struct.unpack("<I", seq[8:12])[0], tag
    pool_size = struct.unpack("<I", dat[12:16])[0]
    count = struct.unpack("<I", seq[12:16])[0]
    assert len(idx) == 16 + 4 * limit and len(dat) == 16 + pool_size and len(seq) == 16 + RECORD * count
    assert dat[-1] == 0, "pool must end with NUL"

    def name(offset):
        assert 16 <= offset < len(dat), offset
        return dat[offset:dat.index(b"\0", offset)].decode("utf-8")

    singles = {}
    for cp, offset in enumerate(struct.unpack("<%dI" % limit, idx[16:])):
        if offset:
            singles[cp] = name(offset)
    sequences, previous = {}, b""
    for k in range(count):
        record = seq[16 + k * RECORD:16 + (k + 1) * RECORD]
        key = record[:KEY_BYTES]
        assert key > previous, "sequence records must be strictly sorted"
        previous = key
        sequences[key.rstrip(b"\0").decode("utf-8")] = name(struct.unpack("<I", record[KEY_BYTES:])[0])
    return singles, sequences


def reviewed(directory, tag):
    singles, sequences = {}, {}
    lines = (directory / (tag + ".tsv")).read_text(encoding="utf-8").splitlines()[1:]
    for line in lines:
        kind, key, chars, name, source = line.split("\t")
        assert source, line
        if kind == "single":
            singles[int(key, 16)] = name
        else:
            sequences["".join(chr(int(c, 16)) for c in key.split())] = name
    return singles, sequences


def run(cli, resources, language, text, **extra):
    command = [str(cli), "normalize", "--resources", str(resources), "--language", language,
               "--output-mode", "speech", *sum((["--" + k.replace("_", "-"), v] for k, v in extra.items()), []), text]
    return subprocess.run(command, capture_output=True, text=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("cli", type=Path)
    parser.add_argument("resources", type=Path)
    args = parser.parse_args()
    directory = args.resources / "unicode-fallback"
    checks = 0
    for tag in ("ko", "en"):
        decoded = decode(directory, tag)
        assert decoded == reviewed(directory, tag), tag + ": binary and review TSV differ"
        singles, sequences = decoded
        # Letters, marks, controls and ASCII are never fallback keys.
        assert not any(cp < 0x80 or 0xAC00 <= cp <= 0xD7A3 or 0x4E00 <= cp <= 0x9FFF or
                       cp in (0x200D, 0xFE0F, 0x20E3) for cp in singles), tag
        checks += len(singles) + len(sequences)
    assert run(args.cli, args.resources, "ko-KR", "점심은 🍔 어때?").stdout.strip() == "점심은 햄버거 어때?"
    with tempfile.TemporaryDirectory(prefix="ttf-unicode-fallback-") as temp:
        copy = Path(temp) / "resources"
        shutil.copytree(args.resources, copy)
        # Without the files the engine keeps the unreadable policy.
        shutil.rmtree(copy / "unicode-fallback")
        result = run(args.cli, copy, "ko-KR", "점심은 🍔 어때?")
        assert result.returncode == 0 and result.stdout.strip() == "점심은 🍔 어때?", result
        # A damaged or mismatched triple is a resource error, never a partial read.
        shutil.copytree(args.resources / "unicode-fallback", copy / "unicode-fallback")
        dat = copy / "unicode-fallback/ko.dat"
        dat.write_bytes(dat.read_bytes()[:-1])
        assert run(args.cli, copy, "ko-KR", "점심은 🍔 어때?").returncode != 0
        shutil.copy(args.resources / "unicode-fallback/en.dat", dat)
        assert run(args.cli, copy, "ko-KR", "점심은 🍔 어때?").returncode != 0
        # symbol_fallback=off never opens the files, damaged or not.
        result = run(args.cli, copy, "ko-KR", "점심은 🍔 어때?", symbol_fallback="off")
        assert result.returncode == 0 and result.stdout.strip() == "점심은 🍔 어때?", result
        checks += 4
    print("unicode fallback: %d entries and file checks" % checks)


if __name__ == "__main__":
    main()
