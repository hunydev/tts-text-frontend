#!/usr/bin/env python3
"""Fetch a bounded English archive prefix; never download the full multi-GB corpus."""

import argparse
import hashlib
import io
import json
import struct
import tarfile
import urllib.request
import zlib
from datetime import datetime, timezone
from pathlib import Path

API = "https://www.kaggle.com/api/v1/datasets/download/richardwilliamsproat/text-normalization-for-english-russian-and-polish/"
MAX_INFLATED = 16 * 1024 * 1024


def sha(data):
    return hashlib.sha256(data).hexdigest()


def inflate_bounded(data, window):
    decoder = zlib.decompressobj(window)
    output = decoder.decompress(data, MAX_INFLATED + 1)
    if len(output) > MAX_INFLATED or decoder.unconsumed_tail:
        raise ValueError("decompression limit exceeded")
    return output


def extract_prefix(data, max_lines):
    """Interpret a ZIP local member -> gzip -> tar prefix without extracting paths."""
    if len(data) < 30:
        raise ValueError("ZIP prefix too short")
    header = struct.unpack("<IHHHHHIIIHH", data[:30])
    if header[0] != 0x04034B50 or header[2] & 1 or header[3] != 8:
        raise ValueError("expected unencrypted deflate ZIP local member")
    name_len, extra_len = header[-2:]
    if data[30 : 30 + name_len] != b"en_with_types.tgz":
        raise ValueError("unexpected ZIP member")
    gzip_prefix = inflate_bounded(data[30 + name_len + extra_len :], -15)
    tar_prefix = inflate_bounded(gzip_prefix, 31)
    # The archive order is not shard order. Record the actual first file name.
    with tarfile.open(fileobj=io.BytesIO(tar_prefix), mode="r|") as archive:
        for member in archive:
            if not member.isfile():
                continue
            if (
                not member.name.startswith("en_with_types/output-")
                or ".." in member.name
            ):
                raise ValueError("unexpected first tar data member")
            stream = archive.extractfile(member)
            chunks, last_eos = [], 0
            for _ in range(max_lines):
                try:
                    line = stream.readline(1024 * 1024 + 1)
                except (tarfile.ReadError, EOFError):
                    break
                if not line:
                    break
                if len(line) > 1024 * 1024:
                    raise ValueError("source line exceeds limit")
                if not line.endswith(b"\n"):
                    break
                chunks.append(line)
                if line.rstrip(b"\r\n") == b"<eos>\t<eos>":
                    last_eos = len(chunks)
            if not last_eos:
                raise ValueError("prefix contains no complete sentence")
            return member.name, b"".join(chunks[:last_eos]), last_eos
    raise ValueError("no data member in bounded prefix")


def fetch(output, network_bytes, max_lines):
    if not 65536 <= network_bytes <= 1048576:
        raise ValueError("network cap must be between 65536 and1048576 bytes")
    if not 1 <= max_lines <= 200000:
        raise ValueError("line cap must be between1 and200000")
    output.mkdir(parents=True, exist_ok=True)
    url = API + "en_with_types.tgz"
    request = urllib.request.Request(
        url, headers={"Range": f"bytes=0-{network_bytes - 1}"}
    )
    with urllib.request.urlopen(request, timeout=30) as response:
        expected_range = f"bytes 0-{network_bytes - 1}/"
        if response.status != 206 or not response.headers.get(
            "Content-Range", ""
        ).startswith(expected_range):
            raise ValueError(
                "server did not honor bounded HTTP Range; refusing full download"
            )
        data = response.read(network_bytes + 1)
        headers = {
            key: response.headers.get(key)
            for key in ("ETag", "Content-Range", "Last-Modified")
        }
    if len(data) != network_bytes:
        raise ValueError("unexpected range body length")
    member, sample, lines = extract_prefix(data, max_lines)
    (output / "sample-prefix.tsv").write_bytes(sample)
    evidence = {}
    for name in ("README", "LICENSE"):
        with urllib.request.urlopen(API + name, timeout=30) as response:
            text = response.read(65537)
        if len(text) > 65536:
            raise ValueError(f"{name} exceeds evidence download cap")
        (output / name).write_bytes(text)
        evidence[name] = {"url": API + name, "bytes": len(text), "sha256": sha(text)}
    manifest = {
        "source_url": url,
        "revision": "kaggle-version-1",
        "source_file": member,
        "retrieved_utc": datetime.now(timezone.utc).isoformat(),
        "headers": headers,
        "network_prefix_bytes": len(data),
        "network_prefix_sha256": sha(data),
        "sample_bytes": len(sample),
        "sample_sha256": sha(sample),
        "sample_lines": lines,
        "selection": "first archive member prefix, cut at last complete EOS; not randomized or held-out",
        "gzip_crc_verified": False,
        "evidence": evidence,
    }
    (output / "fetch-manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n"
    )
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--network-bytes", type=int, default=1048576)
    parser.add_argument("--max-lines", type=int, default=50000)
    args = parser.parse_args()
    try:
        print(
            json.dumps(
                fetch(args.output, args.network_bytes, args.max_lines), sort_keys=True
            )
        )
    except (OSError, ValueError, tarfile.TarError, zlib.error) as exc:
        parser.exit(2, f"sproat fetch: {exc}\n")


if __name__ == "__main__":
    main()
