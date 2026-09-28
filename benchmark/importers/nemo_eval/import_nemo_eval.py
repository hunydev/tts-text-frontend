#!/usr/bin/env python3
"""Import the NeMo text-processing English evaluation sets into benchmark schema v1.

Reads GoogleTN.json, LibriTTS.json and EngConf.txt from a local checkout of
NVIDIA/NeMo-text-processing `data/` (CC BY 4.0 per its data/README.md). Nothing
is vendored; the output stays under benchmark/generated/ or outside the tree.

  python3 benchmark/importers/nemo_eval/import_nemo_eval.py \\
    --data-dir /path/NeMo-text-processing/data --revision COMMIT \\
    --out benchmark/generated/en-eval/nemo-eval.jsonl

Every sentence is imported; English-only scoping is the evaluator's job so the
excluded sentences stay visible. EngConf lists every acceptable reading (~~1)
and known-wrong readings (~~0) of one input; the first acceptable one is
`expected`, all of them are `accepted_references`.
"""
import argparse
import json
import os
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from benchmark.common import ensure_output, sha256, validate  # noqa: E402

REPOSITORY = "https://github.com/NVIDIA/NeMo-text-processing"
FILES = {"GoogleTN.json": "nemo-googletn", "LibriTTS.json": "nemo-libritts",
         "EngConf.txt": "nemo-engconf"}


def record(dataset, name, revision, digest, line, text, expected, **extra):
    row = dict(schema_version=1, id="%s:en-US:L%d" % (dataset, line), dataset=dataset,
               split="evaluation", language="en-US", text=text, expected=expected,
               reference_status="dataset-reference", usage="benchmark-only",
               provenance=dict(source_url="%s/blob/%s/data/%s" % (REPOSITORY, revision, name),
                               revision=revision, source_file="data/" + name,
                               source_sha256=digest, license="CC-BY-4.0",
                               license_evidence="data/README.md at the same revision",
                               license_url="https://creativecommons.org/licenses/by/4.0/",
                               source_line=line),
               spans=[], **extra)
    return validate(row)


def json_lines(path, dataset, revision):
    digest = sha256(path)
    with path.open(encoding="utf-8") as handle:
        for line, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            data = json.loads(raw)
            if not isinstance(data.get("text"), str) or not isinstance(data.get("gt_normalized"), str):
                raise ValueError("%s:%d: text and gt_normalized are required" % (path.name, line))
            yield record(dataset, path.name, revision, digest, line, data["text"], data["gt_normalized"])


def engconf(path, dataset, revision):
    """Blocks of `input~~RAW` followed by `reading~~1` (acceptable) or `reading~~0` (wrong)."""
    digest = sha256(path)
    block, start = [], None
    lines = path.read_text(encoding="utf-8").split("\n") + [""]
    for number, raw in enumerate(lines, 1):
        if raw.startswith("#"):
            continue
        if raw.strip():
            match = re.fullmatch(r"(.*)~~(RAW|0|1)", raw)
            if not match:
                raise ValueError("%s:%d: expected TEXT~~RAW, TEXT~~0 or TEXT~~1" % (path.name, number))
            if not block:
                if match.group(2) != "RAW":
                    raise ValueError("%s:%d: a block must start with ~~RAW" % (path.name, number))
                start = number
            elif match.group(2) == "RAW":
                raise ValueError("%s:%d: a block has one ~~RAW line" % (path.name, number))
            block.append(match.groups())
            continue
        if block:
            accepted = [text for text, label in block if label == "1"]
            rejected = [text for text, label in block if label == "0"]
            yield record(dataset, path.name, revision, digest, start, block[0][0],
                         accepted[0] if accepted else None,
                         accepted_references=accepted, rejected_references=rejected)
            block = []


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--data-dir", type=Path, required=True)
    parser.add_argument("--revision", required=True, help="NeMo-text-processing commit of --data-dir")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    if not re.fullmatch(r"[0-9a-f]{40}", args.revision):
        parser.error("--revision must be a full commit hash")
    out = ensure_output(args.out)
    if out.exists():
        parser.error("refusing to overwrite %s" % out)
    counts = {}
    out.parent.mkdir(parents=True, exist_ok=True)
    handle, temporary = tempfile.mkstemp(dir=out.parent, prefix=".nemo-eval-")
    try:
        with os.fdopen(handle, "w", encoding="utf-8", newline="\n") as stream:
            for name, dataset in FILES.items():
                path = args.data_dir / name
                rows = engconf(path, dataset, args.revision) if name.endswith(".txt") \
                    else json_lines(path, dataset, args.revision)
                counts[dataset] = 0
                for row in rows:
                    stream.write(json.dumps(row, ensure_ascii=False) + "\n")
                    counts[dataset] += 1
        os.replace(temporary, out)
    except BaseException:
        os.unlink(temporary)
        raise
    manifest = dict(schema_version=1, repository=REPOSITORY, revision=args.revision,
                    files={name: sha256(args.data_dir / name) for name in FILES},
                    counts=counts, records_sha256=sha256(out))
    out.with_suffix(".manifest.json").write_text(json.dumps(manifest, indent=2) + "\n",
                                                 encoding="utf-8", newline="\n")
    print(json.dumps(manifest["counts"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
