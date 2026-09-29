#!/usr/bin/env python3
"""Context-cache compatibility and independent text/stream contracts.

Text expectations are independently authored: the original 30 cases precede
initial v0.19 execution; separate partial-token cases precede the adaptive-cache
candidate run. JSON fixtures record only v0.18 metadata as an explicitly
identified compatibility oracle, not linguistic gold. No external corpus is included.
"""
import argparse
import importlib.util
import json
import random
import subprocess
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REFERENCE = Path(__file__).with_name("context_cache_v18_reference.json")
PARTIAL_REFERENCE = Path(__file__).with_name("context_cache_partial_v18_reference.json")
PROFILES = ((), ("--first-target", "1", "--normal-target", "3", "--max-length", "12"))
# The v0.18 metadata oracle describes the per-token router, kept as matrix=legacy.
# The default sentence-language router is checked against the same independent
# text expectations and for chunk-invariant spans (auto_checks).
LEGACY = ("--matrix", "legacy")


def case(identifier, language, text, expected=None, **options):
    return dict(id=identifier, language=language, text=text,
                expected=text if expected is None else expected, options=options)


CASES = [
    case("ordinal-context", "ko-KR", "1st는 첫 번째입니다. Please go to the 1st floor.",
         "퍼스트는 첫 번째입니다. Please go to the first floor."),
    case("apple-context", "ko-KR", "Apple은 사과입니다. Apple released the iPhone.",
         "애플은 사과입니다. Apple released the iPhone."),
    case("attached-counter-tail", "ko-KR", "3개입니다. Apple은 사과입니다.",
         "세 개입니다. 애플은 사과입니다."),
    case("attached-clock-tail", "ko-KR", "3시입니다. GPU를 사용합니다.",
         "세 시입니다. 지피유를 사용합니다."),
    case("attached-scaled-money", "ko-KR", "3만원입니다. OpenAI가 발표했습니다.",
         "삼만 원입니다. 오픈에이아이가 발표했습니다."),
    case("quantity-tail-en-clause", "ko-KR", "3kg입니다. Please go to the 1st floor.",
         "삼 킬로그램입니다. Please go to the first floor."),
    case("quoted-english-clause", "ko-KR", '그는 "Please go to the 1st floor."라고 말했습니다.',
         '그는 "Please go to the first floor."라고 말했습니다.'),
    case("quoted-single-word", "ko-KR", '회사 "Apple"은 안내합니다.'),
    case("mixed-newline", "ko-KR", "Apple은 사과입니다.\nPlease go to the 1st floor.",
         "애플은 사과입니다.\nPlease go to the first floor."),
    case("numeric-label", "ko-KR", "Phone:+12125550100",
         "Phone:plus one two one two five five five zero one zero zero"),
    case("financial-label", "ko-KR", "Account:0012", "Account:zero zero one two"),
    case("neutral-numbers", "en-US", "7 7 7", "seven seven seven"),
    case("neutral-numbers-ko", "ko-KR", "7 7 7", "칠 칠 칠"),
    case("all-whitespace", "en-US", " \t  "),
    case("imperative-first", "en-US", "go home please"),
    case("imperative-middle", "en-US", "home go please"),
    case("polite-before-imperative", "en-US", "home please go"),
    case("subject-before-imperative", "en-US", "we go home"),
    case("subject-after-imperative", "en-US", "home go we"),
    case("trimmed-run", "en-US", "  \t go home please \t "),
    case("finite-predicate", "en-US", "Apple is red"),
    case("phrase-saturation", "en-US", "alpha beta gamma delta epsilon zeta eta theta iota kappa lambda mu"),
    case("script-boundaries", "ko-KR", "가 나 다 라 Apple", "가 나 다 라 애플"),
    case("punctuation-boundaries", "en-US", "go; please: home! we, go?"),
    case("line-boundaries", "en-US", "go\r\nplease home\nwe go home"),
    case("quoted-trimmed-run", "en-US", 'home "  please go  " we'),
    case("nested-quotes", "en-US", 'home “please «go home» we” go'),
    case("layered-order", "en-US", "home please go", domain_tsv=
         "context\ten-US\tphrase_ceiling\t12\ncontext\ten-US\tclause_weight\t77\n",
         customer_tsv="context\ten-US\tclause_weight\t19\ncontext_word\ten-US\tgo\t0\n"),
    case("zero-saturation", "en-US", "we go home", customer_tsv=
         "context\ten-US\tphrase_ceiling\t0\ncontext\ten-US\tlatin_weight\t0\n"),
    case("layered-ko-weights", "ko-KR", "가 나 다 Apple은 안내입니다.", "가 나 다 애플은 안내입니다.",
         domain_tsv="context\tko-KR\thangul_weight\t12\ncontext\tko-KR\tparticle_weight\t70\n",
         customer_tsv="context\tko-KR\thangul_weight\t3\ncontext\tko-KR\tclause_floor\t23\n"
         "context\tko-KR\tparticle_weight\t29\ncontext\tko-KR\tscript_weight\t11\n"),
]

# Separate additions: the original 30-case reference remains unchanged. These
# inputs and text expectations precede execution of the adaptive-cache change.
PARTIAL_CASES = [
    case("repeated-counter-ko-remainder", "ko-KR", "3개는 3개는 3개는 Apple은 사과입니다.",
         "세 개는 세 개는 세 개는 애플은 사과입니다."),
    case("repeated-counter-quoted-remainder", "ko-KR", '3개는 3개는 3개는 "Please go to the 1st floor."입니다.',
         '세 개는 세 개는 세 개는 "Please go to the first floor."입니다.'),
    case("repeated-counter-en-remainder", "ko-KR", "3개는 3개는 3개는 Apple released the iPhone.",
         "세 개는 세 개는 세 개는 Apple released the iPhone."),
    case("repeated-counter-next-unit", "ko-KR", "3개는 3개는 3개는\nOpenAI가 발표했습니다.",
         "세 개는 세 개는 세 개는\n오픈에이아이가 발표했습니다."),
]


def command_options(row, directory):
    options = []
    for layer in ("domain", "customer"):
        value = row["options"].get(layer + "_tsv")
        if value:
            path = directory / (layer + ".tsv")
            path.write_text(value, encoding="utf-8")
            options += ["--" + layer, str(path)]
    return options


def run_stream(cli, resources, row, profile, chunk, timeout=30):
    with tempfile.TemporaryDirectory(prefix="ttf-context-reference-") as directory:
        command = [str(cli), "stream-json", "--resources", str(resources), "--language", row["language"],
                   "--chunk-bytes", str(chunk), *profile, *command_options(row, Path(directory)), "-"]
        try:
            process = subprocess.run(command, input=row["text"].encode(), capture_output=True,
                                     timeout=timeout, check=False)
        except subprocess.TimeoutExpired:
            raise AssertionError(f"context case {row['id']} exceeded {timeout}s") from None
    assert process.returncode == 0, (row["id"], process.stderr.decode(errors="replace"))
    events = [json.loads(line) for line in process.stdout.splitlines()]
    assert events and events[-1]["event"] == "end", (row["id"], events[-1:])
    spans, text, offset = [], [], 0
    for event in events:
        if event["event"] != "segment":
            continue
        spans.extend(dict(span, begin=span["begin"] + offset, end=span["end"] + offset)
                     for span in event["spans"])
        text.append(event["text"])
        offset += len(event["text"].encode())
    return dict(text="".join(text), spans=spans)


def compatibility_checks(args, references, cases=CASES):
    checks = 0
    for row in cases:
        reference = references["cases"][row["id"]]
        assert reference["input"] == row, row["id"]
        for profile_index, profile in enumerate(PROFILES):
            expected = reference["profiles"][profile_index]
            assert expected["text"] == row["expected"], (row["id"], "invalid independent text reference")
            for chunk in (1, 2, 7, 4096):
                actual = run_stream(args.cli, args.resources, row, LEGACY + profile, chunk)
                assert actual == expected, (row["id"], profile_index, chunk, expected, actual)
                checks += 1
    return checks


def auto_checks(args, cases):
    checks = 0
    for row in cases:
        for profile in PROFILES:
            whole = run_stream(args.cli, args.resources, row, profile, 4096)
            assert whole["text"] == row["expected"], (row["id"], profile, whole["text"])
            for chunk in (1, 2, 7):
                actual = run_stream(args.cli, args.resources, row, profile, chunk)
                assert actual == whole, (row["id"], profile, chunk, whole, actual)
                checks += 1
    return checks


def large_numeric_checks(args, references):
    template = references["cases"]["neutral-numbers"]["profiles"][0]["spans"]
    number_template, space_template = template[0], template[1]
    checks = 0
    for count in (3000, 30000):
        row = case(f"numeric-{count}", "en-US", " ".join(["7"] * count), " ".join(["seven"] * count))
        limit = str(len(row["text"]) + 1)
        profile = ("--first-target", limit, "--normal-target", limit, "--max-length", limit)
        started = time.monotonic()
        result = run_stream(args.cli, args.resources, row, profile, len(row["text"]), timeout=60)
        assert result["text"] == row["expected"] and len(result["spans"]) == 2 * count - 1
        for index, span in enumerate(result["spans"]):
            item = index // 2
            original = number_template if index % 2 == 0 else space_template
            begin = item * 6 + (0 if index % 2 == 0 else 5)
            expected = dict(original, begin=begin, end=begin + (5 if index % 2 == 0 else 1),
                            input_begin=index, input_end=index + 1)
            assert span == expected, (count, index, expected, span)
            checks += 1
        print(f"context normalization: {count} numerals, {time.monotonic() - started:.3f}s; exact text/spans/context_trace", flush=True)
    return checks


def partial_counter_checks(args, references):
    """Complete v0.18 span oracle plus exact long repeated-counter output.

    Four template counters already saturate the three-Hangul context signal.
    Only byte positions change for later identical counters. References remain
    explicit v0.18 compatibility data; no candidate output becomes a reference.
    """
    checks = compatibility_checks(args, references, PARTIAL_CASES)
    template = references["repeated_counter_template"]["result"]["spans"][:3]
    assert [(span["semantic_type"], span["value"], span["unit"]) for span in template] == [
        ("COUNTER", "3", "개"), ("WORD", "", ""), ("TEXT", "", "")]
    input_stride, output_stride = len("3개는 ".encode()), len("세 개는 ".encode())
    for count in (300, 3000):
        row = case(f"partial-counters-{count}", "ko-KR", " ".join(["3개는"] * count),
                   " ".join(["세 개는"] * count))
        limit = str(len(row["text"].encode()) + 1)
        profile = ("--first-target", limit, "--normal-target", limit, "--max-length", limit)
        result = run_stream(args.cli, args.resources, row, LEGACY + profile, len(row["text"].encode()),
                            timeout=60)
        assert result["text"] == row["expected"] and len(result["spans"]) == 3 * count - 1
        for index, span in enumerate(result["spans"]):
            item, part = divmod(index, 3)
            original = template[part]
            expected = dict(original,
                            begin=original["begin"] + item * output_stride,
                            end=original["end"] + item * output_stride,
                            input_begin=original["input_begin"] + item * input_stride,
                            input_end=original["input_end"] + item * input_stride)
            assert span == expected, (count, index, expected, span)
            checks += 1
    return checks


def native_checks(args):
    if not args.library:
        return 0
    spec = importlib.util.spec_from_file_location("context_native", ROOT / "tools/web/native.py")
    native = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(native)
    checks = 0
    for row in CASES + PARTIAL_CASES:
        for tiny in (False, True):
            options = dict(row["options"], default_language=row["language"], detailed_trace=False)
            if tiny:
                options.update(first_target=1, normal_target=3, max_length=12)
            engine = native.Engine(args.library, args.resources, options)
            try:
                source = row["text"].encode()
                batch = engine.normalize(source)
                assert batch["text"] == row["expected"]
                schedules = [[1] * len(source), [len(source)]]
                schedules += [[split, len(source) - split] for split in range(1, len(source))]
                rng = random.Random(19000)
                schedules += [[rng.randint(1, 11) for _ in source] for _ in range(3)]
                for schedule in schedules:
                    assert native.flatten(engine.stream(source, schedule)) == batch, (row["id"], tiny, schedule)
                    checks += 1
            finally:
                engine.close()
    # Whole-unit moderate input and long sentence sequence exercise distinct
    # buffering paths. Long sequence must emit before final input/flush.
    for source, options, early in (
        (("7 " * 255 + "7").encode(), dict(first_target=1000, normal_target=1000, max_length=1000), False),
        (("Apple은 사과입니다.\n" * 80).encode(), dict(), True),
    ):
        engine = native.Engine(args.library, args.resources, dict(default_language="ko-KR", detailed_trace=False, **options))
        try:
            batch, events = engine.normalize(source), []
            actual = native.flatten(engine.stream(source, [1] * len(source), emit=events.append))
            assert actual == batch
            if early:
                assert any(event["event"] == "segment" and event["stage"] == "write"
                           and event["write_index"] < len(source) - 1 for event in events)
            checks += 1
        finally:
            engine.close()
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    args.cli, args.resources = args.cli.resolve(), args.resources.resolve()
    if args.library:
        args.library = args.library.resolve()
    references = json.loads(REFERENCE.read_text(encoding="utf-8"))
    assert references["baseline_version"] == "0.18.0"
    partial_references = json.loads(PARTIAL_REFERENCE.read_text(encoding="utf-8"))
    assert partial_references["baseline_version"] == "0.18.0"
    checks = compatibility_checks(args, references)
    checks += large_numeric_checks(args, references)
    checks += partial_counter_checks(args, partial_references)
    checks += auto_checks(args, CASES + PARTIAL_CASES)
    native_count = native_checks(args)
    print(f"context cache: {checks} compatibility/large-input checks; {native_count} native split comparisons")


if __name__ == "__main__":
    main()
