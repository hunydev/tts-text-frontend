"""Offline importer/adapter contracts; optional pinned-data and native smoke checks."""

from __future__ import annotations

import argparse
import importlib.util
import json
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / "benchmark/importers/cmudict/import_cmudict.py"
SPEC = importlib.util.spec_from_file_location("cmudict_importer", TOOL)
importer = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(importer)

# Hand-authored parser fixtures, not an upstream data extract or Korean gold set.
PHONE_CLASSES = "AE vowel\nAH vowel\nB stop\nK stop\nS fricative\n"
SYMBOLS = "AE\nAE0\nAE1\nAE2\nAH\nAH0\nAH1\nAH2\nB\nK\nS\n"
DICTIONARY = (
    ";;; hand-authored test fixture\n"
    "zorbax AE1 B AH0 K S # parser note\n"
    "zorbax(2) AH2 B AH1 K S\n"
    "zorbax(10) AH2 B AH1 K S\n"
    "apple AH1\n"
    "zorbax's AE1 S\n"
    "zorbax-alpha AE1\n"
)
PROVENANCE = {
    "source_url": "https://example.invalid/synthetic-parser-fixture",
    "revision": "synthetic-fixture-v1",
    "source_file": "synthetic.dict",
    "source_sha256": "0" * 64,
    "license": "repository test fixture",
}


def reject(callback, *arguments):
    try:
        callback(*arguments)
    except (ValueError, OSError):
        return
    raise AssertionError("invalid input was accepted")


def native_check(cli: Path, resources: Path, bundle: Path):
    text = "zorbax의 결과입니다. Apple의 결과입니다.\nApple released the iPhone.\n"
    result = subprocess.run(
        [
            str(cli),
            "stream-json",
            "--resources",
            str(resources),
            "--customer",
            str(bundle / "pronunciation.bin"),
            "-",
        ],
        input=text.encode("utf-8"),
        capture_output=True,
        timeout=30,
        check=False,
    )
    assert result.returncode == 0, result.stderr.decode()
    all_events = [json.loads(line) for line in result.stdout.splitlines()]
    assert all_events[-1]["event"] == "end", all_events
    events = [event for event in all_events if event["event"] == "segment"]
    spans = [span for event in events for span in event["spans"]]
    dictionary = [
        span for span in spans if span["lexicon"] == "pronunciation-dictionary"
    ]
    assert len(dictionary) == 1, spans
    assert dictionary[0]["source_lang"] == "en-US", dictionary
    assert dictionary[0]["context_lang"] == "ko-KR", dictionary
    assert dictionary[0]["render_lang"] == "ko-KR", dictionary
    output = "".join(event["text"] for event in events)
    assert "zorbax" not in output, output
    assert "애플의 결과입니다." in output, output  # Direct target lexicon wins.
    assert "Apple released the iPhone." in output, output
    for event in events:
        raw = event["text"].encode("utf-8")
        for span in event["spans"]:
            if span["lexicon"] == "pronunciation-dictionary":
                rendered = raw[span["begin"] : span["end"]].decode("utf-8")
                assert rendered and all("가" <= c <= "힣" for c in rendered), rendered
    # No assertion of this mapper prototype's spelling as a reviewed TTS gold.


def native_variants(cli: Path, resources: Path, bundle: Path, directory: Path):
    """Explicit selection changes the source phones; source labels do not rank variants."""
    def normalize(*overlays):
        command = [str(cli), "normalize", "--resources", str(resources)]
        for layer, path in overlays:
            command.extend(["--" + layer, str(path)])
        command.append("zorbax의 결과입니다.")
        result = subprocess.run(command, capture_output=True, timeout=30)
        assert result.returncode == 0, result.stderr.decode()
        return result.stdout.decode()

    pronunciation = bundle / "pronunciation.bin"
    choose = directory / "choose.tsv"
    choose.write_text("pronunciation_choice\ten-US\tzorbax\t2\n", encoding="utf-8")
    reset = directory / "reset.tsv"
    reset.write_text("pronunciation_choice\ten-US\tzorbax\tbase\n", encoding="utf-8")
    reference = directory / "alternate-as-base.tsv"
    reference.write_text("pronunciation\ten-US\tzorbax\tAH2 B AH1 K S\n", encoding="utf-8")
    base = normalize(("domain", pronunciation))
    alternate = normalize(("domain", pronunciation), ("customer", choose))
    assert alternate == normalize(("customer", reference))
    assert alternate != base
    assert normalize(("domain", pronunciation), ("domain", choose), ("customer", reset)) == base
    # A customer base must dominate a lower-domain policy, even when its variant
    # is stored with the higher-priority dictionary resource.
    assert normalize(("domain", choose), ("customer", pronunciation)) == base


def pinned_source_check(source: Path, directory: Path):
    words = directory / "words.txt"
    words.write_text("abacus\nread\nlead\naalborg\napple\n", encoding="utf-8")
    output = directory / "pinned-export"
    result = subprocess.run(
        [
            sys.executable,
            str(TOOL),
            "--source",
            str(source),
            "--output",
            str(output),
            "--words",
            str(words),
            "--export-runtime",
            "--variant-policy",
            "base",
        ],
        capture_output=True,
        timeout=90,
        check=False,
    )
    assert result.returncode == 0, result.stderr.decode()
    summary = json.loads(result.stdout)
    assert summary["headwords"] == 5 and summary["pronunciations"] == 8, summary
    records = {
        row["word"]: row
        for row in map(
            json.loads, (output / "pronunciations.jsonl").read_text().splitlines()
        )
    }
    assert [p["arpabet"] for p in records["read"]["pronunciations"]] == [
        ["R", "EH1", "D"],
        ["R", "IY1", "D"],
    ]
    assert [p["variant_label"] for p in records["lead"]["pronunciations"]] == [
        None,
        "2",
    ]
    assert records["aalborg"]["pronunciations"][0]["comment"] == "place, danish"
    assert (output / "COPYING").read_bytes() == (source / "LICENSE").read_bytes()
    assert summary["runtime_export"]["unselected_runtime_alternatives"] == 3
    assert (
        "pronunciation\ten-US\tread\tR EH1 D\n"
        in (output / "pronunciation.tsv").read_text()
    )
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ttf", type=Path)
    parser.add_argument("--resources", type=Path, default=ROOT / "resources")
    parser.add_argument(
        "--source", type=Path, help="optional external pinned CMUdict snapshot"
    )
    args = parser.parse_args()
    classes, symbols = importer.inventory(PHONE_CLASSES, SYMBOLS)
    records = importer.parse_dictionary(DICTIONARY, classes, symbols, PROVENANCE)
    assert [row["word"] for row in records] == [
        "apple",
        "zorbax",
        "zorbax's",
        "zorbax-alpha",
    ]
    variants = records[1]["pronunciations"]
    assert [p["variant_label"] for p in variants] == [None, "2", "10"]
    assert [p["source_line"] for p in variants] == [2, 3, 4]
    assert variants[0]["phones"][0] == {"symbol": "AE", "stress": 1}
    assert variants[0]["phones"][1] == {"symbol": "B", "stress": None}
    assert variants[1]["phones"][0]["stress"] == 2
    assert variants[1]["arpabet"] == variants[2]["arpabet"]  # No lossy deduplication.
    assert variants[0]["comment"] == "parser note"
    assert variants[1]["comment"] is None
    assert all(row["provenance"] == PROVENANCE for row in records)
    for invalid in (
        "",
        "bad",
        "bad QX1",
        "bad AE3",
        "bad B1",
        "bad(1) AE1",
        "bad AE1\nbad AH1",
        "bad(2) AE1\nbad(2) AH1",
        "bad\0 AH1",
    ):
        reject(importer.parse_dictionary, invalid, classes, symbols, PROVENANCE)
    reject(importer.inventory, "B stop\n", "B1\n")
    reject(importer.inventory, "B stop\nB stop\n", "B\n")
    reject(importer.select_words, records, {"missing"})
    reject(importer.runtime_payload, records, "first")
    assert len(importer.select_words(records, {"zorbax"})) == 1
    payload, stats = importer.runtime_payload(records, "base")
    assert stats["exported_headwords"] == 2 and stats["skipped_headwords"] == 2, stats
    assert stats["skipped_pronunciations"] == 2, stats
    assert stats["unselected_runtime_alternatives"] == 2, stats
    assert stats["stress_preserved"] is True
    assert "pronunciation\ten-US\tzorbax\tAE1 B AH0 K S\n" in payload
    assert "AH2 B AH1 K S" not in payload
    all_payload, all_stats = importer.runtime_payload(records, "all")
    assert "pronunciation_variant\ten-US\tzorbax#2\tAH2 B AH1 K S\n" in all_payload
    assert "pronunciation_variant\ten-US\tzorbax#10\tAH2 B AH1 K S\n" in all_payload
    assert "pronunciation_choice" not in all_payload
    assert all_stats["exported_runtime_alternatives"] == 2
    assert all_stats["unselected_runtime_alternatives"] == 0
    assert all_stats["runtime_choice_installed"] is False
    assert all_stats["skipped_runtime_alternatives_by_reason"] == {}
    # One unreachable alternate must not discard its valid base or a sibling.
    import copy
    overlong = copy.deepcopy(records)
    overlong[1]["pronunciations"][1]["arpabet"] = ["AH1"] * 129
    bounded_payload, bounded_stats = importer.runtime_payload(overlong, "all")
    assert "zorbax#2" not in bounded_payload and "zorbax#10" in bounded_payload
    assert bounded_stats["exported_runtime_alternatives"] == 1
    assert bounded_stats["skipped_runtime_alternatives_by_reason"] == {
        "pronunciation-exceeds-current-ir": 1
    }
    with tempfile.TemporaryDirectory(prefix="ttf-cmudict-test-") as scratch:
        directory = Path(scratch)
        output = directory / "bundle"
        manifest = importer.write_bundle(
            records, output, b"Synthetic fixture license\n", {}, "base"
        )
        assert json.loads((output / "manifest.json").read_text()) == manifest
        assert manifest["headwords"] == 4 and manifest["pronunciations"] == 6
        assert manifest["runtime_export"]["installed_by_default"] is False
        assert (output / "COPYING").read_bytes() == b"Synthetic fixture license\n"
        binary = (output / "pronunciation.bin").read_bytes()
        assert binary[:8] == b"TTFB\x01\x00\x00\x00"
        assert struct.unpack("<I", binary[8:12])[0] == len(binary) - 16
        assert binary[16:] == payload.encode("utf-8")
        for name, digest in manifest["files"].items():
            assert importer.sha256(output / name) == digest
        repeat = directory / "repeat"
        importer.write_bundle(
            records, repeat, b"Synthetic fixture license\n", {}, "base"
        )
        assert {p.name: p.read_bytes() for p in repeat.iterdir()} == {
            p.name: p.read_bytes() for p in output.iterdir()
        }
        reject(importer.write_bundle, records, output, b"license", {}, "base")
        archive = directory / "archive-only"
        importer.write_bundle(records, archive, b"license", {})
        assert not (archive / "pronunciation.bin").exists()
        assert (
            json.loads((archive / "manifest.json").read_text())["runtime_export"]
            is None
        )
        bad_source = directory / "bad-source"
        bad_source.mkdir()
        for name in importer.PINNED_SHA256:
            (bad_source / name).write_text("not the pinned source", encoding="utf-8")
        reject(importer.verify_source, bad_source)
        for options in (["--export-runtime"], ["--variant-policy", "base"], []):
            bad_output = directory / "failed-export"
            result = subprocess.run(
                [
                    sys.executable,
                    str(TOOL),
                    "--source",
                    str(bad_source),
                    "--output",
                    str(bad_output),
                    *options,
                ],
                capture_output=True,
                timeout=15,
                check=False,
            )
            assert result.returncode != 0 and result.stderr and not bad_output.exists()
        all_output = directory / "all-variants"
        all_manifest = importer.write_bundle(records, all_output, b"license", {}, "all")
        assert (all_output / "pronunciation.tsv").read_text() == all_payload
        assert all_manifest["runtime_export"]["exported_runtime_alternatives"] == 2
        if args.ttf:
            native_check(args.ttf.resolve(), args.resources.resolve(), output)
            native_check(args.ttf.resolve(), args.resources.resolve(), all_output)
            native_variants(args.ttf.resolve(), args.resources.resolve(), all_output, directory)
        if args.source:
            pinned = pinned_source_check(args.source.resolve(), directory)
            if args.ttf:
                native = subprocess.run(
                    [
                        str(args.ttf.resolve()),
                        "inspect",
                        "--resources",
                        str(args.resources.resolve()),
                        "--customer",
                        str(pinned / "pronunciation.bin"),
                        "abacus의 결과입니다.",
                    ],
                    capture_output=True,
                    timeout=30,
                    check=False,
                )
                assert (
                    native.returncode == 0
                    and b"pronunciation-dictionary" in native.stdout
                ), native
    print(
        "CMUdict parser, variant/stress preservation, provenance and opt-in runtime export: passed"
    )


if __name__ == "__main__":
    main()
