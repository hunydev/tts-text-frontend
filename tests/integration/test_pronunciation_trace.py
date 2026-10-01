"""Source pronunciation resource provenance, independent of target renderer policy.

Synthetic phone/word associations deliberately isolate resource precedence from
linguistic dictionary accuracy. Optional native mode checks user overrides and
all two-part UTF-8 byte splits; CLI mode also runs in sanitizer jobs.
"""
import argparse
import importlib.util
import tempfile
from pathlib import Path

from test_english_dates import PROFILES, exact_span, stream
from test_pronunciation_aliases import flatten

ROOT = Path(__file__).resolve().parents[2]
# text, surface, expected text, source route, source/lexicon layer, overlays.
SCENARIOS = (
    ("us는 표기입니다.", "us", "어스는 표기입니다.", "pronunciation-dictionary", "domain",
     ((2, "pronunciation\ten-US\tus\tAH S\n"),)),
    ("abacus는 표기입니다.", "abacus", "어스는 표기입니다.", "pronunciation-dictionary", "customer",
     ((3, "pronunciation\ten-US\tabacus\tAH S\n"),)),
    ("abacus는 표기입니다.", "abacus", "비는 표기입니다.", "pronunciation-dictionary", "customer",
     ((2, "pronunciation\ten-US\tabacus\tAH S\n"),
      (3, "pronunciation\ten-US\tabacus\tB IY\n"))),
    ("U.S.는 표기입니다.", "U.S.", "어스는 표기입니다.", "pronunciation-dictionary", "customer",
     ((3, "initialism_pronunciation\ten-US\tus\tAH S\n"),)),
    # An ordinary dictionary homograph is not consulted for a dotted initialism.
    ("U.S.는 표기입니다.", "U.S.", "유에스는 표기입니다.", "initialism", "common",
     ((3, "pronunciation\ten-US\tus\tAH S\n"),)),
    ("baba는 표기입니다.", "baba", "배배는 표기입니다.", "grapheme-g2p-prototype", "system", ()),
    ("baba는 표기입니다.", "baba", "바바는 표기입니다.", "grapheme-g2p-prototype", "domain",
     ((2, "g2p\ten-US\tba\tB AA\n"),)),
    ("baba는 표기입니다.", "baba", "비비는 표기입니다.", "grapheme-g2p-prototype", "customer",
     ((2, "g2p\ten-US\tba\tB AA\n"), (3, "g2p\ten-US\tba\tB IY\n"))),
    # Unused higher-layer entries must not raise the source provenance layer.
    ("baba는 표기입니다.", "baba", "바바는 표기입니다.", "grapheme-g2p-prototype", "domain",
     ((2, "g2p\ten-US\tba\tB AA\n"), (3, "g2p\ten-US\tb\tB IY\n"))),
    # Multiple consumed source chunks report their highest contributing layer.
    ("babi는 표기입니다.", "babi", "바비는 표기입니다.", "grapheme-g2p-prototype", "customer",
     ((2, "g2p\ten-US\tba\tB AA\n"), (3, "g2p\ten-US\tbi\tB IY\n"))),
    # Target phone mapping can change output, but is not a source resource.
    ("baba는 표기입니다.", "baba", "버버는 표기입니다.", "grapheme-g2p-prototype", "domain",
     ((2, "g2p\ten-US\tba\tB AA\n"), (3, "phone_vowel\tko-KR\tAA\t4\n"))),
    ("U.S.는 표기입니다.", "U.S.", "고객에스는 표기입니다.", "initialism", "common",
     ((3, "phone_letter\tko-KR\tletter.U\t고객\n"),)),
    # Korean letter codes take their English digits from the lexicon number
    # words since 2026-10-01 (ABC127 원투세븐, was 원투세번 from digit phones), so a
    # customer reading of the word reaches the code. Was: digit_pronunciation 9 AH.
    ("QR-9는 표기입니다.", "QR-9", "큐알어는 표기입니다.", "alnum:composed", "customer",
     ((3, "lexicon\tko-KR\ten-US:nine\t어\n"),)),
    # A rejected source dictionary must not leave its layer on the fallback.
    ("baba는 표기입니다.", "baba", "브아브아는 표기입니다.", "grapheme-fallback", "common",
     ((3, "pronunciation\ten-US\tbaba\tAH UNMAPPED\n"),)),
    # Target pronunciation lexicons still precede every source phone dictionary.
    ("Apple은 사과입니다.", "Apple", "애플은 사과입니다.", "lexicon:system", "system",
     ((3, "pronunciation\ten-US\tapple\tAH S\n"),)),
    ("abacus는 표기입니다.", "abacus", "관용형은 표기입니다.", "lexicon:domain", "domain",
     ((2, "lexicon\tko-KR\ten-US:abacus\t관용형\n"),
      (3, "pronunciation\ten-US\tabacus\tAH S\n"))),
)


def verify(result, text, surface, expected, path, layer):
    assert result["text"] == expected, (text, expected, result)
    span = exact_span(result, text, surface)
    assert span["lexicon"] == path, (text, path, span)
    assert span["pronunciation_layer"] == layer, (text, layer, span)
    assert tuple(span[k] for k in ("source_lang", "context_lang", "render_lang")) == (
        "en-US", "ko-KR", "ko-KR"), span


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    args.cli, args.resources = args.cli.resolve(), args.resources.resolve()
    checks = byte_checks = 0
    native = None
    if args.library:
        spec = importlib.util.spec_from_file_location("trace_native", ROOT / "tools/web/native.py")
        native = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(native)
    with tempfile.TemporaryDirectory(prefix="ttf-pron-trace-") as directory:
        temp = Path(directory)
        for case_index, (text, surface, expected, path, layer, rows) in enumerate(SCENARIOS):
            overlay, native_overlay = [], []
            for index, (resource_layer, content) in enumerate(rows):
                resource = temp / f"{case_index}-{index}.tsv"
                resource.write_text(content, encoding="utf-8")
                overlay.extend(("--domain" if resource_layer == 2 else "--customer", str(resource)))
                native_overlay.append((resource, resource_layer))
            for profile in PROFILES:
                baseline = None
                for chunk in (1, 7, 4096):
                    actual = stream(args.cli, args.resources, text, "ko-KR", chunk, profile, overlay)
                    verify(actual, text, surface, expected, path, layer)
                    if baseline is not None:
                        assert actual == baseline, (case_index, chunk, actual, baseline)
                    baseline = actual
                    checks += 1
                if native:
                    options = {} if not profile else {"first_target": 1, "normal_target": 3, "max_length": 12}
                    engine = native.Engine(args.library.resolve(), args.resources, options, native_overlay)
                    try:
                        source = text.encode()
                        batch = engine.normalize(source)
                        verify(batch, text, surface, expected, path, layer)
                        for split in range(1, len(source)):
                            actual = flatten(engine.stream(source, [split, len(source) - split]))
                            assert actual == batch, (case_index, split, actual, batch)
                            byte_checks += 1
                    finally:
                        engine.close()
        if native:
            fixture = temp / "all-layers.tsv"
            fixture.write_text("pronunciation\ten-US\tabacus\tAH S\n", encoding="utf-8")
            # The resource API accepts every layer, including common and user.
            for index, name in enumerate(("common", "system", "domain", "customer", "user")):
                engine = native.Engine(args.library.resolve(), args.resources, {}, ((fixture, index),))
                try:
                    verify(engine.normalize("abacus는 표기입니다.".encode()), "abacus는 표기입니다.",
                           "abacus", "어스는 표기입니다.", "pronunciation-dictionary", name)
                    checks += 1
                finally:
                    engine.close()
            engine = native.Engine(args.library.resolve(), args.resources,
                {"user_lexicon": [{"word": "abacus", "pronunciation": "사용자"}]}, ((fixture, 3),))
            try:
                verify(engine.normalize("abacus는 표기입니다.".encode()), "abacus는 표기입니다.",
                       "abacus", "사용자는 표기입니다.", "lexicon:user", "user")
                checks += 1
            finally:
                engine.close()
    print(f"pronunciation trace: {checks} checks, {byte_checks} byte split comparisons")


if __name__ == "__main__":
    main()
