"""Independent regressions for inert phrase keys and complete lexical bounds.

These synthetic cases verify routing, resource and streaming contracts. They do
not assign reference pronunciations to unsupported compounds or unknown words.
"""
import subprocess
import sys
import tempfile
from pathlib import Path

cli, resources = sys.argv[1:]
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "evaluation"))
from evaluate import parse_stream  # noqa: E402

profiles = [(), ("--first-target", "1", "--normal-target", "3", "--max-length", "12")]
chunks = (1, 7, 4096)
checks = 0


def run(text, chunk, profile=(), options=()):
    result = subprocess.run([cli, "stream-json", "--resources", resources,
                             "--chunk-bytes", str(chunk), *profile, *options, "-"],
                            input=text.encode(), capture_output=True, timeout=10)
    assert result.returncode == 0, (text, result.stderr)
    return parse_stream(result.stdout.decode(), text)


def validate(text, profile=(), options=(), expected=None, phrase=None):
    """Compare complete spans and boundaries across chunks within one config."""
    global checks
    previous = None
    for chunk in chunks:
        segments = run(text, chunk, profile, options)
        output = "".join(segment["text"] for segment in segments)
        spans = [span for segment in segments for span in segment["spans"]]
        hits = [span for span in spans if span["rule"] == "lexicon.phrase"]
        if phrase is False:
            assert not hits, (text, profile, chunk, hits)
        elif isinstance(phrase, str):
            assert len(hits) == 1, (text, profile, chunk, hits)
            hit = hits[0]
            assert text.encode()[hit["input_begin"]:hit["input_end"]].decode() == phrase, hit
            assert hit["semantic_type"] == "WORD", hit
            assert hit["source_lang"] == "en-US", hit
            assert hit["context_lang"] == hit["render_lang"] == "ko-KR", hit
        if expected is not None:
            assert output == expected, (text, profile, chunk, output, expected)
        if previous is not None:
            assert segments == previous, (text, profile, "chunk-dependent text, spans or boundaries")
        previous = segments
        checks += 1
    batch = subprocess.run([cli, "normalize", "--resources", resources,
                            *profile, *options, "--", text],
                           capture_output=True, timeout=10)
    assert batch.returncode == 0, (text, batch.stderr)
    assert batch.stdout.decode() == output + "\n", (text, "batch/stream mismatch")
    return previous


with tempfile.TemporaryDirectory() as directory:
    overlay = Path(directory) / "phrases.tsv"
    # These records cannot participate in foreign rendering. Formerly their
    # prefix lookahead changed tiny segment cuts and changed Google to 구글.
    for source, target in (("xx-ZZ", "ko-KR"), ("en-US", "xx-ZZ"),
                           ("en-US", "en-US"), ("ko-KR", "en-US")):
        overlay.write_text(f"lexicon\t{target}\t{source}:Google Microsoft\t미사용발음\n",
                           encoding="utf-8")
        for profile in profiles:
            for chunk in chunks:
                text = "Google Microsoft를 소개합니다."
                baseline = run(text, chunk, profile)
                overlaid = run(text, chunk, profile, ("--customer", str(overlay)))
                assert overlaid == baseline, (source, target, profile, chunk, baseline, overlaid)
                checks += 1

    # Invalid schema and unsupported word separators must report authoring
    # errors instead of accepting keys that can never match the tokenizer.
    malformed = ("New York", ":New York", "en-US:", "en-US:New\u00a0York",
                 "en-US:New\vYork", "en-US:New\fYork", "en-US:New York 2",
                 "en-US:New-York City", "en-US:New York 서울",
                 "en-US:" + " ".join(["a"] * 9), "en-US:" + "a " * 47 + "aa")
    for key in malformed:
        overlay.write_text(f"lexicon\tko-KR\t{key}\t거부대상\n", encoding="utf-8")
        result = subprocess.run([cli, "normalize", "--resources", resources,
                                 "--customer", str(overlay), "test"],
                                capture_output=True, timeout=10)
        assert result.returncode != 0 and not result.stdout, (key, result.stdout, result.stderr)
        checks += 1

# Script/category boundaries also occur inside alphanumeric or punctuated
# terms. A registered name must not consume just one of those term fragments.
connectors = ("-", "'", "’", "_", "+", ".", "@", "/", "\\", "‐", "‑")
negative = ["After Effects3를 소개합니다.", "123After Effects를 소개합니다."]
negative += [f"After Effects{mark}Pro를 소개합니다." for mark in connectors]
negative += [f"foo{mark}After Effects를 소개합니다." for mark in connectors]
negative += ["New\nYork를 소개합니다.", "New, York를 소개합니다.",
             "New Yorkshire를 소개합니다."]
for text in negative:
    for profile in profiles:
        validate(text, profile, phrase=False)

# Closing punctuation followed by Korean grammar is not a continuation of an
# English word. Ordinary bounded names still get a complete foreign-word span.
for text, expected in (
    ("After Effects를 소개합니다.", "애프터 이펙트를 소개합니다."),
    ("'After Effects'를 소개합니다.", "'애프터 이펙트'를 소개합니다."),
    ("After Effects’를 소개합니다.", "애프터 이펙트’를 소개합니다."),
):
    for profile in profiles:
        validate(text, profile, expected=expected, phrase="After Effects")

# Phrase lookahead and consumption must respect literal boundaries, including
# tags whose ordinary body is normalized by a separate part of the frontend.
protected = [
    ("New <code>York</code>를 소개합니다.", "<code>York</code>", "code"),
    ("Bank of `America`를 소개합니다.", "`America`", "code"),
    ("New <em>York</em>를 소개합니다.", "<em>", "markup"),
    ("파일 /tmp/New/York를 확인합니다.", "/tmp/New/York", "path"),
    ('파일 "/tmp/New York"를 확인합니다.', '"/tmp/New York"', "path"),
]
for text, surface, kind in protected:
    for profile in profiles:
        segments = validate(text, profile, phrase=False)
        matches = []
        for segment in segments:
            for span in segment["spans"]:
                raw = text.encode()[span["input_begin"]:span["input_end"]].decode()
                if raw == surface:
                    output = segment["text"].encode()[span["begin"]:span["end"]].decode()
                    matches.append((span, output))
        assert len(matches) == 1, (text, matches)
        span, output = matches[0]
        assert output == surface and span["unit"] == kind, (text, span, output)
        assert span["source_lang"] == span["context_lang"] == span["render_lang"] == "und", span

print(f"{checks} multiword inert-key, lexical-boundary, resource-schema and protected-stream checks passed")
