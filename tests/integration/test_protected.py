"""Frozen literal references, complete span coverage and stream/resource isolation.

Fixtures are original synthetic text. This tests literal preservation and adjacent
normalization, not HTML rendering, Markdown parsing, file access or speech quality.
"""
import json
from pathlib import Path
import queue
import subprocess
import sys
import tempfile
import threading

cli, resources = sys.argv[1:]
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools" / "evaluation"))
from evaluate import check_spans, parse_stream, read_corpus  # noqa: E402

cases = read_corpus(Path(__file__).resolve().parents[1] / "evaluation" / "protected_cases.jsonl")
profiles = [(), ("--first-target", "1", "--normal-target", "3", "--max-length", "12")]
checks = 0


def stream(text, chunk=1, profile=(), options=()):
    run = subprocess.run([cli, "stream-json", "--resources", resources,
                          "--chunk-bytes", str(chunk), *profile, *options, "-"],
                         input=text.encode(), capture_output=True, timeout=10)
    assert run.returncode == 0, (text, run.stderr)
    segments = parse_stream(run.stdout.decode(), text)
    return "".join(segment["text"] for segment in segments), segments


def validate(case, profile=(), options=(), chunks=(1, 7, 4096)):
    global checks
    language = ("--language", case.get("default_language", "ko-KR"))
    batch = subprocess.run([cli, "normalize", "--resources", resources,
                            *profile, *language, *options, "--", case["text"]],
                           capture_output=True, timeout=10)
    assert batch.returncode == 0, (case["id"], batch.stderr)
    assert batch.stdout.decode() == case["expected"] + "\n", (case["id"], batch.stdout, case["expected"])
    previous = None
    for chunk in chunks:
        output, segments = stream(case["text"], chunk, profile, (*language, *options))
        assert output == case["expected"], (case["id"], profile, chunk, output, case["expected"])
        failures = check_spans(case, segments)
        assert not failures, (case["id"], profile, chunk, failures)
        if previous is not None:
            assert segments == previous, (case["id"], "chunk-dependent output, spans or boundaries")
        previous = segments
        checks += 1
    return previous


for case in cases:
    for profile in profiles:
        validate(case, profile)


def example(identifier, text, expected, spans=()):
    return dict(id=identifier, text=text, expected=expected, expected_spans=list(spans))


def literal(surface, kind, **extra):
    return dict(surface=surface, normalized=surface, semantic_type="TEXT", value="",
                unit=kind, source_lang="und", context_lang="und", render_lang="und",
                verbalizer="identity", **extra)


# A path-shaped URL query belongs to the complete URL semantic span.
for profile in profiles:
    url = "https://a.test/?file=/tmp/Apple"
    validate(example("url-query-guard", url, url,
                     [dict(surface=url, semantic_type="URL", normalized=url)]), profile)


# Layered switches alter explicit policy only for engines loading the overlay.
# A newly created default engine must retain its original immutable resources.
with tempfile.TemporaryDirectory() as directory:
    override = Path(directory) / "protected.tsv"
    override.write_text("protection\t*\tcode\t0\n", encoding="utf-8")
    for profile in profiles:
        segments = validate(example("disable-code", "`3개`", "`세 개`"), profile,
                            ("--customer", str(override)))
        assert all(span["unit"] != "code" for segment in segments for span in segment["spans"])
    validate(example("defaults-still-on", "`3개`", "`3개`", [literal("`3개`", "code")]))

    override.write_text("protected_tag\t*\tcode\ttransparent\n", encoding="utf-8")
    for profile in profiles:
        validate(example("transparent-code", "<code>3개</code>", "<code>세 개</code>",
                         [literal("<code>", "markup"), literal("</code>", "markup")]),
                 profile, ("--customer", str(override)))

    override.write_text("protected_tag\t*\tasset\topaque\n", encoding="utf-8")
    for profile in profiles:
        validate(example("customer-opaque", "<asset>GPU 3개</asset>와 3개입니다.",
                         "<asset>GPU 3개</asset>와 세 개입니다.",
                         [literal("<asset>GPU 3개</asset>", "code", resource_layer="customer")]),
                 profile, ("--customer", str(override)))

    override.write_text("protection\t*\tpath\t0\n", encoding="utf-8")
    output, segments = stream("파일 /etc/app/config-v2.json을 확인합니다.",
                              options=("--customer", str(override)))
    assert all(span["unit"] != "path" for segment in segments for span in segment["spans"]), segments
    assert output != "파일 /etc/app/config-v2.json을 확인합니다.", output
    checks += 1

    override.write_text("protection\t*\tmarkup\t0\n", encoding="utf-8")
    output, segments = stream("<em>3개</em>", options=("--customer", str(override)))
    assert all(span["unit"] != "markup" for segment in segments for span in segment["spans"]), segments
    assert "세 개" in output, output
    checks += 1

    # Rule matching in ordinary prose must stop at the next protected atom.
    # Neither a rule spanning into code nor an exact rule inside it may replace
    # literal bytes; the final ordinary word proves this overlay was loaded.
    override.write_text("exact\t*\t예제 <code>GPU</code>\t덮어씀\n"
                        "pattern\t*\t예제 <code>{digits}</code>\t덮어씀\n"
                        "exact\t*\tGPU\t덮어씀\n"
                        "exact\t*\t대조\t변경\n", encoding="utf-8")
    for atom in ("<code>GPU</code>", "<code>123</code>"):
        for profile in profiles:
            validate(example("rule-cannot-cross-literal", f"예제 {atom} 대조",
                             f"예제 {atom} 변경", [literal(atom, "code")]),
                     profile, ("--customer", str(override)))

# Quoted explicit paths may contain spaces and Unicode, with the quotes kept
# in the same literal span. These focused tests do not widen the frozen corpus.
for atom in ('"/exports/보고서 01.csv"', "'./assets/clip 02.mp4'",
             '"C:\\reports\\July 2026\\Q1.csv"', '"/exports/미완성 파일'):
    text = f"파일 3개와 {atom}"
    for profile in profiles:
        validate(example("quoted-explicit-path", text, f"파일 세 개와 {atom}",
                         [literal(atom, "path")]), profile)

# The opaque scanner is not allowed to bypass strict UTF-8 validation.
for raw in (b"`a\xffb`", b"<code>\xed\xa0\x80</code>", b"<!--\xc0\xaf-->",
            b"/etc/\xff.json", b"`incomplete \xe3\x81"):
    for chunk in (1, 4096):
        run = subprocess.run([cli, "stream-json", "--resources", resources,
                              "--chunk-bytes", str(chunk), "-"],
                             input=raw, capture_output=True, timeout=10)
        assert run.returncode != 0, (raw, run.stdout)
        assert not any(json.loads(line).get("event") == "end" for line in run.stdout.splitlines()), run.stdout
        checks += 1

# Closed protected content can be delivered before EOF, including a UTF-8
# neighbor. Deliberately keep stdin open until the first callback is observed.
prefix = "`GPU 3개` 예제입니다.\n".encode()
suffix = "Apple은 사과입니다.".encode()
source = (prefix + suffix).decode()
expected = "`GPU 3개` 예제입니다.\n애플은 사과입니다."
process = subprocess.Popen([cli, "stream-json", "--resources", resources,
                            "--chunk-bytes", "1", "-"], stdin=subprocess.PIPE,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, bufsize=0)
first_line = queue.Queue()
reader = threading.Thread(target=lambda: first_line.put(process.stdout.readline()), daemon=True)
reader.start()
try:
    process.stdin.write(prefix)
    process.stdin.flush()
    first = first_line.get(timeout=5)
    reader.join(timeout=1)
    event = json.loads(first)
    assert event["event"] == "segment" and 0 < event["input_end"] <= len(prefix), event
    assert "`GPU 3개`" in event["text"], event
    process.stdin.write(suffix)
    process.stdin.close()
    process.stdin = None
    rest, error = process.communicate(timeout=10)
    assert process.returncode == 0, error
    segments = parse_stream((first + rest).decode(), source)
    assert "".join(segment["text"] for segment in segments) == expected, segments
    checks += 1
finally:
    if process.poll() is None:
        process.kill()
        process.wait()
    reader.join(timeout=1)

print(f"{len(cases)} frozen protected-text references; {checks} "
      "text/metadata/chunk/profile/overlay/UTF-8/early-callback checks passed")
