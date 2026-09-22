"""Independent pronunciation alias precedence and unchanged semantic contracts.

User > Customer > Domain > System applies before preferring an exact spelling
over a verbalized/canonical alias within the same layer. These references are
original policy fixtures, not generated from candidate output. CLI mode covers
resource layers; --library also tests the user API and every two-way byte split
through the existing native adapter.
"""

import argparse
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PROFILES = ({}, {"first_target": 1, "normal_target": 3, "max_length": 12})
CASES = (
    {"id": "ordinal", "text": "1st는 첫 번째입니다.", "surface": "1st",
     "raw": "1st", "canonical": "first", "type": "ORDINAL", "value": "1",
     "secondary": "", "prefix": "", "suffix": "{은/는} 첫 번째입니다."},
    {"id": "dotted-particle", "text": "U.S.는 표기입니다.", "surface": "U.S.",
     "raw": "U.S.", "canonical": "US", "type": "INITIALISM", "value": "US",
     "secondary": "dotted", "prefix": "", "suffix": "{은/는} 표기입니다."},
    {"id": "dotted-terminal", "text": "표기는 U.S.", "surface": "U.S",
     "raw": "U.S.", "canonical": "US", "type": "INITIALISM", "value": "US",
     "secondary": "dotted", "prefix": "표기는 ", "suffix": "."},
)
IR_FIELDS = ("input_begin", "input_end", "semantic_type", "value", "secondary",
             "unit", "source_lang", "context_lang", "render_lang", "rule", "resource_layer")


def overlay(path, rows):
    path.write_text("".join("lexicon\tko-KR\ten-US:" + key + "\t" + value + "\n"
                            for key, value in rows), encoding="utf-8")
    return path


def flatten(segments):
    output = {"text": "".join(s["text"] for s in segments), "spans": []}
    base = 0
    for segment in segments:
        for span in segment["spans"]:
            output["spans"].append(dict(span, begin=span["begin"] + base,
                                         end=span["end"] + base))
        base += len(segment["text"].encode())
    return output


def cli_options(profile, overrides):
    options = []
    for key, flag in (("first_target", "--first-target"),
                      ("normal_target", "--normal-target"), ("max_length", "--max-length")):
        if key in profile:
            options.extend((flag, str(profile[key])))
    for path, layer in overrides:
        assert layer in (2, 3)
        options.extend(("--domain" if layer == 2 else "--customer", str(path)))
    return options


def cli_result(cli, resources, case, profile, overrides, chunk):
    run = subprocess.run(
        [str(cli), "stream-json", "--resources", str(resources),
         *cli_options(profile, overrides), "--chunk-bytes", str(chunk), "-"],
        input=case["text"].encode(), capture_output=True, timeout=15,
    )
    assert run.returncode == 0, (case["id"], run.stderr.decode(errors="replace"))
    events = [json.loads(line) for line in run.stdout.splitlines()]
    assert events and events[-1]["event"] == "end", events
    return flatten([row for row in events if row["event"] == "segment"])


def selected_span(result, case):
    source = case["text"].encode()
    begin = source.index(case["surface"].encode())
    end = begin + len(case["surface"].encode())
    selected = [s for s in result["spans"] if (s["input_begin"], s["input_end"]) == (begin, end)]
    assert len(selected) == 1, (case["id"], selected, result)
    span = selected[0]
    assert tuple(span[k] for k in ("semantic_type", "value", "secondary", "unit")) == (
        case["type"], case["value"], case["secondary"], ""), span
    assert tuple(span[k] for k in ("source_lang", "context_lang", "render_lang")) == (
        "en-US", "ko-KR", "ko-KR"), span
    if case["id"] == "dotted-terminal":
        periods = [s for s in result["spans"] if (s["input_begin"], s["input_end"]) ==
                   (len(source) - 1, len(source))]
        assert len(periods) == 1 and periods[0]["semantic_type"] == "TEXT", periods
        output = result["text"].encode()
        assert output[periods[0]["begin"]:periods[0]["end"]] == b".", periods
    return span


def particle(text, suffix):
    # A particle written against a reading follows that reading: 은 after a final
    # consonant, 는 after a vowel. Applied here independently of the engine.
    if "{은/는}" not in suffix:
        return suffix
    last = text[-1]
    consonant = "가" <= last <= "힣" and (ord(last) - 0xAC00) % 28
    return suffix.replace("{은/는}", "은" if consonant else "는")


def verify(result, case, pronunciation, layer, baseline):
    expected = case["prefix"] + pronunciation + particle(pronunciation, case["suffix"])
    assert result["text"] == expected, (case["id"], expected, result)
    span = selected_span(result, case)
    assert span["lexicon"] == "lexicon:" + layer, span
    assert span["pronunciation_layer"] == layer, span
    output = result["text"].encode()
    assert output[span["begin"]:span["end"]].decode() == pronunciation, span
    assert {k: span[k] for k in IR_FIELDS} == {k: baseline[k] for k in IR_FIELDS}, (
        "pronunciation changed semantic IR", span, baseline)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    args.cli = args.cli.resolve()
    args.resources = args.resources.resolve()
    checks = 0
    byte_comparisons = 0
    native = None
    if args.library:
        spec = importlib.util.spec_from_file_location("alias_native", ROOT / "tools/web/native.py")
        native = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(native)

    with tempfile.TemporaryDirectory(prefix="ttf-aliases-") as directory:
        temp = Path(directory)
        resources = temp / "resources"
        shutil.copytree(args.resources, resources)
        system = resources / "languages/ko-KR/lexicon/alias-fixture.tsv"
        overlay(system, (("1st", "시스템원형"), ("U.S.", "시스템점형")))
        with (resources / "manifest.ttfm").open("a", encoding="utf-8") as manifest:
            manifest.write("\nlanguages/ko-KR/lexicon/alias-fixture.tsv\n")

        for case in CASES:
            raw, canonical = case["raw"], case["canonical"]
            customer = temp / "customer.tsv"
            domain = temp / "domain.tsv"
            # Expected results follow layer order, then exact-spelling preference.
            scenarios = (
                ("system-raw", (), (), "시스템원형" if raw == "1st" else "시스템점형", "system"),
                ("domain-canonical", ((canonical, "도메인별칭"),), (), "도메인별칭", "domain"),
                ("customer-canonical", (), ((canonical, "고객별칭"),), "고객별칭", "customer"),
                ("customer-raw", ((canonical, "도메인별칭"),), ((raw, "고객원형"),),
                 "고객원형", "customer"),
                ("customer-over-domain-raw", ((raw, "도메인원형"),),
                 ((canonical, "고객별칭"),), "고객별칭", "customer"),
                ("customer-tie", (), ((canonical, "고객별칭"), (raw, "고객원형")),
                 "고객원형", "customer"),
            )
            for profile in PROFILES:
                baseline = selected_span(cli_result(args.cli, resources, case, profile, (), 4096), case)
                for name, domain_rows, customer_rows, pronunciation, layer in scenarios:
                    overrides = []
                    if domain_rows:
                        overrides.append((overlay(domain, domain_rows), 2))
                    if customer_rows:
                        overrides.append((overlay(customer, customer_rows), 3))
                    baseline_stream = None
                    for chunk in (1, 7, 4096):
                        result = cli_result(args.cli, resources, case, profile, overrides, chunk)
                        verify(result, case, pronunciation, layer, baseline)
                        if baseline_stream is not None:
                            assert result == baseline_stream, (name, "CLI stream metadata drift")
                        baseline_stream = result
                        checks += 1
                    batch = subprocess.run(
                        [str(args.cli), "normalize", "--resources", str(resources),
                         *cli_options(profile, overrides), case["text"]],
                        capture_output=True, check=True, timeout=15,
                    )
                    assert batch.stdout.decode() == baseline_stream["text"] + "\n", name
                    checks += 1

                if native:
                    overlay(customer, ((raw, "고객원형"), (canonical, "고객別名")))
                    user_scenarios = (
                        ("user-canonical", ({"word": canonical, "pronunciation": "사용자별칭"},),
                         "사용자별칭"),
                        ("user-raw", ({"word": raw, "pronunciation": "사용자원형"},), "사용자원형"),
                        ("user-tie", ({"word": canonical, "pronunciation": "사용자별칭"},
                                      {"word": raw, "pronunciation": "사용자원형"}), "사용자원형"),
                    )
                    for name, user_rows, pronunciation in user_scenarios:
                        options = dict(profile, user_lexicon=user_rows)
                        engine = native.Engine(args.library.resolve(), resources, options,
                                               ((customer, 3),))
                        try:
                            source = case["text"].encode()
                            batch = engine.normalize(source)
                            verify(batch, case, pronunciation, "user", baseline)
                            checks += 1
                            for split in range(1, len(source)):
                                streamed = flatten(engine.stream(source, (split, len(source) - split)))
                                assert streamed == batch, (name, case["id"], split, batch, streamed)
                                byte_comparisons += 1
                        finally:
                            engine.close()

        # The same Korean-target aliases never replace English-context speech.
        overlay(customer, (("1st", "고객원형"), ("first", "고객별칭"),
                           ("U.S.", "고객점형"), ("US", "고객약칭")))
        for text, expected in (("Please go to the 1st floor.", "Please go to the first floor."),
                               ("The U.S. is ready.", "The u s is ready.")):
            result = cli_result(args.cli, resources, {"id": "english-guard", "text": text},
                                {}, ((customer, 3),), 1)
            assert result["text"] == expected, (text, result)
            assert not any(span["lexicon"].startswith("lexicon:") for span in result["spans"]), result
            checks += 1

    print(f"Pronunciation aliases: {checks} precedence/IR/text checks; "
          f"{byte_comparisons} arbitrary two-way byte comparisons passed")


if __name__ == "__main__":
    main()
