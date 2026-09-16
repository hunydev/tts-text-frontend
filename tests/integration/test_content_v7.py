"""Text-only content labels, seed pronunciations and resource override contracts.

The frozen references predate candidate runs. They are synthetic project-policy
examples, not independent measurements of customer or pronunciation accuracy.
"""
from pathlib import Path
import subprocess
import sys
import tempfile

cli, resources = sys.argv[1:]
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "evaluation"))
from evaluate import check_spans, parse_stream, read_corpus  # noqa: E402

cases = read_corpus(Path(__file__).resolve().parents[1] / "evaluation" / "content_v7.jsonl")
profiles = [(), ("--first-target", "1", "--normal-target", "3", "--max-length", "12")]
checks = 0


def validate(case, profile=(), options=(), chunks=(1, 7, 4096), reference=True):
    global checks
    text = case["text"]
    language = ("--language", case.get("default_language", "ko-KR"))
    batch = subprocess.run([cli, "normalize", "--resources", resources,
                            *profile, *language, *options, "--", text],
                           capture_output=True, timeout=10)
    assert batch.returncode == 0, (case["id"], batch.stderr)
    batch_text = batch.stdout.decode()[:-1]
    if reference:
        assert batch_text == case["expected"], (case["id"], batch.stdout)
    previous = None
    for chunk in chunks:
        run = subprocess.run([cli, "stream-json", "--resources", resources,
                              "--chunk-bytes", str(chunk), *profile, *language, *options, "-"],
                             input=text.encode(), capture_output=True, timeout=10)
        assert run.returncode == 0, (case["id"], chunk, run.stderr)
        segments = parse_stream(run.stdout.decode(), text)
        assert "".join(segment["text"] for segment in segments) == batch_text, (case["id"], segments)
        failures = check_spans(case, segments) if reference else []
        assert not failures, (case["id"], profile, chunk, failures)
        if previous is not None:
            assert segments == previous, (case["id"], "chunk-dependent text, metadata or callbacks")
        previous = segments
        checks += 1
    return previous


def example(identifier, text, expected, normalized, layer="system", context="ko-KR"):
    return dict(id=identifier, text=text, expected=expected, expected_spans=[dict(
        surface="Canva", normalized=normalized, source_lang="en-US", context_lang=context,
        render_lang=context, **(dict(lexicon="lexicon:" + layer, pronunciation_layer=layer)
                              if context == "ko-KR" else {}))])


for case in cases:
    for profile in profiles:
        # Golden references apply to the default segmentation profile. Deliberately
        # tiny windows may change ordinary context routing; there we verify parity
        # and coverage. Explicit title cues additionally promise bounded retention,
        # so their exact text and language references must hold under tiny targets.
        stable_reference = not profile or case["category"] == "content/title"
        validate(case, profile, reference=stable_reference)

with tempfile.TemporaryDirectory(prefix="ttf-content-") as directory:
    domain = Path(directory) / "domain.tsv"
    customer = Path(directory) / "customer.tsv"
    domain.write_text("lexicon\tko-KR\ten-US:canva\t도메인캔바\n", encoding="utf-8")
    customer.write_text("lexicon\tko-KR\ten-US:canva\t고객캔바\n", encoding="utf-8")
    for profile in profiles:
        validate(example("domain-pronunciation", "Canva를 사용합니다.",
                         "도메인캔바를 사용합니다.", "도메인캔바", "domain"),
                 profile, ("--domain", str(domain)))
        # Invocation order cannot defeat resource layer precedence.
        for options in (("--domain", str(domain), "--customer", str(customer)),
                        ("--customer", str(customer), "--domain", str(domain))):
            validate(example("customer-pronunciation", "Canva를 사용합니다.",
                             "고객캔바를 사용합니다.", "고객캔바", "customer"), profile, options)
            validate(example("english-preservation", "Canva released the trailer.",
                             "Canva released the trailer.", "Canva", context="en-US"), profile, options)

    customer.write_text("context_cue\ten-US\t영상제목은\t0\n", encoding="utf-8")
    for profile in profiles:
        validate(example("cue-disabled", "영상제목은 Canva입니다.",
                         "영상제목은 캔바입니다.", "캔바"), profile, ("--customer", str(customer)))
        validate(example("cue-default-unchanged", "영상제목은 Canva입니다.",
                         "영상제목은 Canva입니다.", "Canva", context="en-US"), profile)

    customer.write_text("context_cue\ten-US\t전시명은\t800\n", encoding="utf-8")
    for profile in profiles:
        validate(example("customer-title-label", "전시명은 Canva입니다.",
                         "전시명은 Canva입니다.", "Canva", context="en-US"),
                 profile, ("--customer", str(customer)))
        validate(example("unregistered-title-label", "전시명은 Canva입니다.",
                         "전시명은 캔바입니다.", "캔바"), profile)

# Explicit labels stop at newlines/quotes; pronunciation defaults stay local.
for profile in profiles:
    for text in ("영상제목은\nCanva를 사용합니다.", '작품명은 "Canva를 사용합니다."'):
        validate(example("bounded-cue", text, text.replace("Canva", "캔바"), "캔바"), profile)
    validate(dict(id="ascii-casefold", text="cAnVa를 사용합니다.", expected="캔바를 사용합니다.",
                  expected_spans=[dict(surface="cAnVa", normalized="캔바", lexicon="lexicon:system",
                                       context_lang="ko-KR", source_lang="en-US")]), profile)

print(f"content v0.7: {checks} text/metadata/chunk/resource checks passed")
