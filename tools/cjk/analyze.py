#!/usr/bin/env python3
"""Unihan K-source / kHangul coverage measurement.

Read-only analysis for docs/cjk-korean-reading.ko.md 작업 0.
Source: Unicode Unihan database (Unicode License). No network at run time.
"""
import collections
import json
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parent
UNIHAN = ROOT / "unihan"

# UAX #38. KC (Korean History Online database) and KU (moved/removed reference)
# are NOT KS standards and fall outside the design's stated repertoire.
KS_SOURCES = {
    "K0": "KS X 1001:2004",
    "K1": "KS X 1002:2001",
    "K2": "KS X 1027-1:2011",
    "K3": "KS X 1027-2:2011",
    "K4": "KS X 1027-3:2011",
    "K5": "KS X 1027-4:2011",
    "K6": "KS X 1027-5:2021",
}
NON_KS_SOURCES = {"KC": "Korean History Online database", "KU": "moved/removed reference"}

# UAX #38 kHangul source letters.
TAG_MEANING = {
    "0": "KS X 1001",
    "1": "KS X 1002",
    "E": "한문 교육용 기초 한자",
    "N": "인명용 한자",
    "X": "현재 인코딩에서 제거된 구 K-source",
}


def field(filename, name):
    """Yield (codepoint:int, value:str) for one Unihan field."""
    path = UNIHAN / filename
    marker = "\t" + name + "\t"
    with path.open(encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("#") or marker not in line:
                continue
            cp, _, value = line.rstrip("\n").split("\t", 2)
            yield int(cp[2:], 16), value


def unicode_version():
    with (UNIHAN / "Unihan_IRGSources.txt").open(encoding="utf-8") as handle:
        for line in handle:
            if line.startswith("# Unicode Version"):
                return line.split()[-1]
    return "unknown"


def main():
    # --- repertoire -------------------------------------------------------
    sources = collections.defaultdict(set)   # "K0" -> {cp}
    for cp, value in field("Unihan_IRGSources.txt", "kIRG_KSource"):
        for part in value.split():
            sources[part.split("-")[0]].add(cp)

    ks = set().union(*(sources[k] for k in KS_SOURCES))
    non_ks = set().union(*(sources[k] for k in NON_KS_SOURCES if k in sources))
    non_ks_only = non_ks - ks

    # --- readings ---------------------------------------------------------
    readings = {}        # cp -> [(syllable, tags)]
    for cp, value in field("Unihan_Readings.txt", "kHangul"):
        parsed = []
        for part in value.split():
            syllable, _, tags = part.partition(":")
            parsed.append((syllable, tags))
        readings[cp] = parsed

    # A reading whose every tag is X refers to a removed source only.
    def usable(entries):
        return [(s, t) for s, t in entries if t.replace("X", "")]

    covered = {cp for cp in ks if usable(readings.get(cp, []))}
    missing = ks - covered

    # --- compatibility ideographs ----------------------------------------
    compat = {}
    for cp, value in field("Unihan_IRGSources.txt", "kCompatibilityVariant"):
        compat[cp] = int(value[2:], 16)
    compat_into_ks = {c: t for c, t in compat.items() if t in ks}
    compat_target_covered = {c: t for c, t in compat_into_ks.items() if t in covered}

    # --- distributions ----------------------------------------------------
    counts = collections.Counter(len(usable(readings.get(cp, []))) for cp in ks)
    tag_hist = collections.Counter()
    for cp in ks:
        for _, tags in usable(readings.get(cp, [])):
            tag_hist[tags or "(none)"] += 1

    per_source = {}
    for key, label in KS_SOURCES.items():
        members = sources[key]
        got = {cp for cp in members if usable(readings.get(cp, []))}
        per_source[key] = dict(
            standard=label,
            chars=len(members),
            with_reading=len(got),
            missing=len(members) - len(got),
            coverage=round(100.0 * len(got) / len(members), 2) if members else 0.0,
        )

    # Cumulative coverage as the repertoire widens K0 -> K0..K6.
    cumulative = []
    seen = set()
    for key in ("K0", "K1", "K2", "K3", "K4", "K5", "K6"):
        seen |= sources[key]
        got = {cp for cp in seen if usable(readings.get(cp, []))}
        cumulative.append(
            dict(
                through=key,
                chars=len(seen),
                with_reading=len(got),
                missing=len(seen) - len(got),
                coverage=round(100.0 * len(got) / len(seen), 2),
            )
        )

    # Readings outside the KS repertoire (would be lost by the K0..K6 filter).
    outside = {cp for cp in readings if cp not in ks and usable(readings[cp])}

    # Plane distribution of the repertoire (UTF-8 4-byte concern).
    planes = collections.Counter(cp // 0x10000 for cp in ks)

    report = dict(
        unicode_version=unicode_version(),
        source="Unicode Unihan database (Unicode License)",
        repertoire=dict(
            ks_k0_k6=len(ks),
            non_ks_excluded=len(non_ks_only),
            non_ks_detail={
                k: dict(label=NON_KS_SOURCES[k], chars=len(sources[k]),
                        not_in_ks=len(sources[k] - ks))
                for k in NON_KS_SOURCES if k in sources
            },
        ),
        coverage=dict(
            with_reading=len(covered),
            missing=len(missing),
            percent=round(100.0 * len(covered) / len(ks), 2),
        ),
        per_source=per_source,
        cumulative=cumulative,
        reading_count_distribution={str(k): v for k, v in sorted(counts.items())},
        tag_distribution=dict(sorted(tag_hist.items(), key=lambda x: -x[1])),
        tag_meaning=TAG_MEANING,
        compatibility=dict(
            total=len(compat),
            folds_into_ks=len(compat_into_ks),
            fold_target_has_reading=len(compat_target_covered),
        ),
        readings_outside_ks=len(outside),
        planes={f"plane{p}": n for p, n in sorted(planes.items())},
    )
    (ROOT / "coverage-report.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8", newline="\n"
    )

    # Missing-character sample for manual inspection.
    sample = sorted(missing)[:40]
    (ROOT / "missing-sample.txt").write_text(
        "\n".join(f"U+{cp:04X}\t{chr(cp)}\t{';'.join(k for k in KS_SOURCES if cp in sources[k])}"
                  for cp in sample),
        encoding="utf-8", newline="\n",
    )
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
