#!/usr/bin/env python3
"""Import pinned CMUdict without losing stress or pronunciation alternatives.

The optional runtime export selects the unnumbered pronunciation explicitly.
It never installs data in the frontend's default resource manifest.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCE_URL = "https://github.com/cmusphinx/cmudict"
REVISION = "74790861f652b15e4ac49015a90074ad62a27690"
LICENSE_ID = "BSD-2-Clause (see bundled upstream COPYING)"
PINNED_SHA256 = {
    "cmudict.dict": "81917843c7f44ce2b094ac63873c2c7a4cf802040792c455ba3ca406891c3d22",
    "cmudict.phones": "ffb588a5e55684723582c7256e1d2f9fadb130011392d9e59237c76e34c2cfd6",
    "cmudict.symbols": "408ccaae803641c6d7b626b6299949320c2dbca96b2220fd3fb17887b023b027",
    "LICENSE": "bd4ce8e44170a5f9f481310ca85c51de3c4f851a65e679b40e603b143bd3542a",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_source(source: Path) -> dict:
    """Accept a pinned Git checkout or an unpacked snapshot with exact hashes."""
    if (source / ".git").exists():
        revision = subprocess.run(
            ["git", "-C", str(source), "rev-parse", "HEAD"],
            check=True,
            capture_output=True,
            text=True,
            timeout=15,
        ).stdout.strip()
        if revision != REVISION:
            raise ValueError(
                f"CMUdict revision mismatch: expected {REVISION}, got {revision}"
            )
    hashes = {name: sha256(source / name) for name in PINNED_SHA256}
    for name, expected in PINNED_SHA256.items():
        if hashes[name] != expected:
            raise ValueError(
                f"{name}: source hash mismatch; use pinned CMUdict {REVISION}"
            )
    return hashes


def inventory(phones_text: str, symbols_text: str) -> tuple[dict, set]:
    """Read upstream inventories rather than guessing legal stressed tokens."""
    classes = {}
    for number, line in enumerate(phones_text.splitlines(), 1):
        if not line.strip() or line.lstrip().startswith(("#", ";;;")):
            continue
        fields = line.split()
        if len(fields) != 2 or not re.fullmatch(r"[A-Z]+", fields[0]):
            raise ValueError(f"phone inventory line {number}: expected PHONE CLASS")
        if fields[0] in classes:
            raise ValueError(f"phone inventory line {number}: duplicate phone")
        classes[fields[0]] = fields[1]
    symbols = set()
    for number, line in enumerate(symbols_text.splitlines(), 1):
        token = line.strip()
        if not token or token.startswith(("#", ";;;")):
            continue
        match = re.fullmatch(r"([A-Z]+)([012])?", token)
        if not match or match[1] not in classes or token in symbols:
            raise ValueError(
                f"symbol inventory line {number}: invalid or duplicate symbol"
            )
        if match[2] and classes[match[1]] != "vowel":
            raise ValueError(f"symbol inventory line {number}: stress on non-vowel")
        symbols.add(token)
    if not classes or not symbols:
        raise ValueError("empty CMUdict inventory")
    return classes, symbols


def parse_dictionary(text: str, classes: dict, symbols: set, provenance: dict) -> list:
    """Return one record per headword; preserve source ordering of all variants."""
    records = {}
    seen = set()
    if "\0" in text:
        raise ValueError("dictionary contains NUL")
    for number, line in enumerate(text.splitlines(), 1):
        if not line.strip() or line.lstrip().startswith((";;;", "#")):
            continue
        body, separator, comment = line.partition("#")
        fields = body.split()
        if len(fields) < 2:
            raise ValueError(f"dictionary line {number}: expected headword and phones")
        source_entry = fields[0]
        match = re.fullmatch(r"([^\s()]+)(?:\(([2-9]|[1-9][0-9]+)\))?", source_entry)
        if not match or not source_entry.isascii():
            raise ValueError(
                f"dictionary line {number}: invalid headword/variant label"
            )
        word, label = match.groups()
        if (word, label) in seen:
            raise ValueError(
                f"dictionary line {number}: duplicate headword/variant label"
            )
        seen.add((word, label))
        phones = []
        for token in fields[1:]:
            match = re.fullmatch(r"([A-Z]+)([012])?", token)
            if token not in symbols or not match or match[1] not in classes:
                raise ValueError(f"dictionary line {number}: unknown phone {token!r}")
            if match[2] and classes[match[1]] != "vowel":
                raise ValueError(f"dictionary line {number}: stress on non-vowel")
            phones.append(
                {"symbol": match[1], "stress": int(match[2]) if match[2] else None}
            )
        if word not in records:
            records[word] = {
                "schema_version": 1,
                "schema": "ttf-cmudict-pronunciation",
                "id": "cmudict:" + word,
                "language": "en-US",
                "word": word,
                "alphabet": "ARPABET",
                "reference_status": "dictionary-pronunciation",
                "provenance": dict(provenance),
                "pronunciations": [],
            }
        records[word]["pronunciations"].append(
            {
                "source_entry": source_entry,
                "variant_label": label,
                "source_line": number,
                "arpabet": fields[1:],
                "phones": phones,
                "comment": comment.strip() if separator else None,
            }
        )
    if not records:
        raise ValueError("dictionary contains no pronunciations")
    return [records[word] for word in sorted(records)]


def read_words(path: Path) -> set:
    words = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        word = line.strip()
        if not word or word.startswith("#"):
            continue
        if not word.isascii() or any(c.isspace() for c in word) or "\0" in word:
            raise ValueError("word list requires one ASCII CMUdict headword per line")
        words.add(word)
    if not words:
        raise ValueError("word list is empty")
    return words


def select_words(records: list, words: set | None) -> list:
    if words is None:
        return records
    missing = words - {record["word"] for record in records}
    if missing:
        raise ValueError("headwords absent from CMUdict: " + ", ".join(sorted(missing)))
    return [record for record in records if record["word"] in words]


def runtime_payload(records: list, variant_policy: str) -> tuple[str, dict]:
    """Adapt the bounded ASCII word subset to the current C pronunciation IR.

    Base is a deterministic source label, not a preferred/frequent pronunciation.
    Every alternate remains in pronunciations.jsonl. Stress stays in the payload.
    """
    if variant_policy != "base":
        raise ValueError("runtime export requires explicit variant policy 'base'")
    lines = [
        "# CMUdict opt-in source pronunciation resource; see COPYING and manifest.json"
    ]
    exported = 0
    skipped = {}
    skipped_pronunciations = 0
    unselected = 0
    for record in records:
        word = record["word"]
        # Restrict to words known to enter the current native WORD route. Keep
        # punctuation/number forms in archival JSONL without pretending they hit.
        reason = None
        if not re.fullmatch(r"[a-z]+", word):
            reason = "non-ascii-letter-headword"
        elif len(word.encode("ascii")) >= 96:
            reason = "key-exceeds-current-ir"
        base = next(
            (p for p in record["pronunciations"] if p["variant_label"] is None), None
        )
        if base is None:
            reason = reason or "missing-unnumbered-variant"
        phones = " ".join(base["arpabet"]) if base else ""
        if base and (len(base["arpabet"]) > 128 or len(phones.encode("ascii")) >= 1024):
            reason = reason or "pronunciation-exceeds-current-ir"
        if reason:
            skipped[reason] = skipped.get(reason, 0) + 1
            skipped_pronunciations += len(record["pronunciations"])
            continue
        lines.append(f"pronunciation\ten-US\t{word}\t{phones}")
        exported += 1
        unselected += len(record["pronunciations"]) - 1
    if not exported:
        raise ValueError(
            "no words are reachable by the conservative current-runtime adapter"
        )
    return "\n".join(lines) + "\n", {
        "variant_policy": variant_policy,
        "variant_policy_meaning": "unnumbered source entry, not frequency/POS ranking",
        "stress_preserved": True,
        "alternatives_preserved_in": "pronunciations.jsonl",
        "exported_headwords": exported,
        "unselected_runtime_alternatives": unselected,
        "skipped_headwords": sum(skipped.values()),
        "skipped_pronunciations": skipped_pronunciations,
        "skipped_by_reason": skipped,
        "adapter": "ttf-phones-v1 / source=en-US / ARPABET with stress tokens retained",
        "installed_by_default": False,
    }


def compile_ttf_resource(source: Path, destination: Path) -> None:
    compiler_path = ROOT / "tools/resource_compiler/compile_resources.py"
    spec = importlib.util.spec_from_file_location(
        "ttf_resource_compiler", compiler_path
    )
    if spec is None or spec.loader is None:
        raise ValueError("cannot find repository resource compiler")
    compiler = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(compiler)
    compiler.compile_resource(source, destination)


def write_bundle(
    records: list,
    output: Path,
    license_bytes: bytes,
    source_hashes: dict,
    variant_policy: str | None = None,
) -> dict:
    """Create a new bundle atomically; never replace an existing export."""
    if output.exists():
        raise ValueError(f"output already exists: {output}")
    if not records or not license_bytes:
        raise ValueError("nonempty records and upstream license are required")
    payload, runtime = (
        runtime_payload(records, variant_policy) if variant_policy else (None, None)
    )
    manifest = {
        "schema_version": 1,
        "schema": "ttf-cmudict-export",
        "provenance": records[0]["provenance"],
        "source_sha256": source_hashes,
        "license_file": "COPYING",
        "attribution": "CMU Pronouncing Dictionary, Carnegie Mellon University",
        "headwords": len(records),
        "pronunciations": sum(len(r["pronunciations"]) for r in records),
        "multiple_pronunciation_headwords": sum(
            len(r["pronunciations"]) > 1 for r in records
        ),
        "runtime_export": runtime,
        "files": {},
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".cmudict-", dir=output.parent) as scratch:
        stage = Path(scratch) / "bundle"
        stage.mkdir()
        with (stage / "pronunciations.jsonl").open(
            "w", encoding="utf-8", newline="\n"
        ) as handle:
            for record in records:
                handle.write(
                    json.dumps(record, ensure_ascii=False, sort_keys=True) + "\n"
                )
        (stage / "COPYING").write_bytes(license_bytes)
        (stage / "NOTICE.txt").write_text(
            "CMU Pronouncing Dictionary — Carnegie Mellon University.\n"
            "Derived from " + records[0]["provenance"]["source_url"] + "\n"
            "Revision: " + records[0]["provenance"]["revision"] + "\n"
            "Retain COPYING with redistributed source or binary resources.\n"
            "Dictionary pronunciations are not reviewed Korean TTS references.\n"
            "Runtime export, if present, selects base variants without POS disambiguation.\n",
            encoding="utf-8",
        )
        if payload is not None:
            (stage / "pronunciation.tsv").write_text(
                payload, encoding="utf-8", newline="\n"
            )
            compile_ttf_resource(
                stage / "pronunciation.tsv", stage / "pronunciation.bin"
            )
        manifest["files"] = {
            path.name: sha256(path) for path in sorted(stage.iterdir())
        }
        (stage / "manifest.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        stage.rename(output)
    return manifest


def main(argv: list | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--source", required=True, type=Path, help="pinned CMUdict checkout/snapshot"
    )
    parser.add_argument(
        "--output", required=True, type=Path, help="new output bundle directory"
    )
    parser.add_argument(
        "--words", type=Path, help="optional one-headword-per-line allowlist"
    )
    parser.add_argument(
        "--export-runtime", action="store_true", help="opt in to TSV and TTFB resource"
    )
    parser.add_argument(
        "--variant-policy",
        choices=["base"],
        help="required when exporting runtime data",
    )
    args = parser.parse_args(argv)
    if args.export_runtime != bool(args.variant_policy):
        parser.error(
            "--export-runtime and --variant-policy base must be supplied together"
        )
    try:
        source_hashes = verify_source(args.source)
        classes, symbols = inventory(
            (args.source / "cmudict.phones").read_text(encoding="utf-8"),
            (args.source / "cmudict.symbols").read_text(encoding="utf-8"),
        )
        provenance = {
            "source_url": SOURCE_URL,
            "revision": REVISION,
            "source_file": "cmudict.dict",
            "source_sha256": source_hashes["cmudict.dict"],
            "license": LICENSE_ID,
        }
        records = parse_dictionary(
            (args.source / "cmudict.dict").read_text(encoding="utf-8"),
            classes,
            symbols,
            provenance,
        )
        records = select_words(records, read_words(args.words) if args.words else None)
        manifest = write_bundle(
            records,
            args.output,
            (args.source / "LICENSE").read_bytes(),
            source_hashes,
            args.variant_policy,
        )
    except (OSError, ValueError, UnicodeError, subprocess.SubprocessError) as error:
        parser.error(str(error))
    print(
        json.dumps(
            {
                "output": str(args.output),
                "headwords": manifest["headwords"],
                "pronunciations": manifest["pronunciations"],
                "runtime_export": manifest["runtime_export"],
            },
            ensure_ascii=False,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
