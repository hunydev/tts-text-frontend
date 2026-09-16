"""Multiword foreign lexicon text, semantic spans, barriers and layering."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile

cli, resources = sys.argv[1:]
resources = str(Path(resources).resolve())
root = Path(__file__).resolve().parents[2]
profiles = ([], ["--first-target", "1", "--normal-target", "3", "--max-length", "12"])
checks = 0


def stream(text, language="ko-KR", chunk=1, profile=(), overlay=()):
    command = [cli, "stream-json", "--resources", resources, "--language", language,
               "--chunk-bytes", str(chunk), *profile, *overlay, "-"]
    run = subprocess.run(command, input=text.encode(), capture_output=True, timeout=10)
    assert run.returncode == 0, (text, command, run.stderr)
    events = [json.loads(line) for line in run.stdout.decode().splitlines()]
    assert events[-1]["event"] == "end", events
    segments = [event for event in events if event["event"] == "segment"]
    spans = []
    raw = text.encode()
    for segment in segments:
        output = segment["text"].encode()
        for span in segment["spans"]:
            assert 0 <= span["begin"] <= span["end"] <= len(output), span
            assert 0 <= span["input_begin"] <= span["input_end"] <= len(raw), span
            spans.append((raw[span["input_begin"]:span["input_end"]].decode(),
                          output[span["begin"]:span["end"]].decode(), span))
    return "".join(event["text"] for event in segments), spans, events


for line in (root / "tests/evaluation/multiword_v8.jsonl").read_text().splitlines():
    case = json.loads(line)
    for profile in profiles:
        batch = subprocess.run([cli, "normalize", "--resources", resources, "--language",
                                case.get("default_language", "ko-KR"), *profile, case["text"]],
                               capture_output=True, timeout=10)
        assert batch.returncode == 0 and batch.stdout.decode() == case["expected"] + "\n", (
            case["id"], profile, batch.stdout, batch.stderr)
        previous = None
        for chunk in (1, 7, 4096):
            output, spans, events = stream(case["text"], case.get("default_language", "ko-KR"),
                                            chunk, profile)
            assert output == case["expected"], (case["id"], profile, chunk, output)
            for expected in case.get("expected_spans", []):
                matching = [(normalized, span) for surface, normalized, span in spans
                            if surface == expected["surface"]]
                assert len(matching) == 1, (case["id"], expected, spans)
                normalized, span = matching[0]
                for key, value in expected.items():
                    if key == "surface":
                        continue
                    assert (normalized if key == "normalized" else span[key]) == value, (
                        case["id"], key, normalized, span, value)
                if expected.get("lexicon") == "lexicon:system":
                    assert span["rule"] == "lexicon.phrase", span
                    canonical = " ".join(expected["surface"].lower().split())
                    assert span["value"] == canonical and not span["secondary"], span
                    assert span["resource_layer"] == "system", span
            if case["category"] in ("multiword/english-preserve", "multiword/title-preserve"):
                assert all(span["rule"] != "lexicon.phrase" for _, _, span in spans), spans
            if previous is not None:
                assert events == previous, (case["id"], profile, chunk, "event mismatch")
            previous = events
            checks += 1

with tempfile.TemporaryDirectory(prefix="ttf-multiword-") as temp:
    temp = Path(temp)
    domain = temp / "domain.tsv"
    customer = temp / "customer.tsv"
    domain.write_text("lexicon\tko-KR\ten-US:adobe premiere pro\t도메인편집기\n"
                      "lexicon\tko-KR\ten-US:pro\t프로\n", encoding="utf-8")
    customer.write_text("lexicon\tko-KR\ten-US:Adobe  Premiere\t고객편집기\n"
                        "lexicon\tko-KR\ten-US:apple\t고객애플\n", encoding="utf-8")
    overlay = ("--domain", str(domain), "--customer", str(customer))
    for profile in profiles:
        for chunk in (1, 7, 4096):
            output, spans, _ = stream("Adobe Premiere Pro를 사용합니다.", chunk=chunk,
                                      profile=profile, overlay=("--domain", str(domain)))
            assert output == "도메인편집기를 사용합니다.", (output, spans)
            assert spans[0][2]["pronunciation_layer"] == "domain", spans
            output, spans, _ = stream("Adobe Premiere Pro를 사용합니다.", chunk=chunk,
                                      profile=profile, overlay=overlay)
            assert output == "고객편집기 프로를 사용합니다.", (output, spans)
            assert spans[0][0] == "Adobe Premiere" and spans[0][2]["pronunciation_layer"] == "customer"
            output, spans, _ = stream("Apple Watch를 소개합니다.", chunk=chunk,
                                      profile=profile, overlay=overlay)
            assert output == "고객애플 워치를 소개합니다.", (output, spans)
            assert spans[0][0] == "Apple" and spans[0][2]["pronunciation_layer"] == "customer"
            checks += 3
    customer.write_text(customer.read_text() +
                        "lexicon\tko-KR\ten-US:adobe premiere pro\t고객완전편집기\n",
                        encoding="utf-8")
    output, spans, _ = stream("Adobe Premiere Pro를 사용합니다.", overlay=overlay)
    assert output == "고객완전편집기를 사용합니다.", output
    assert spans[0][0] == "Adobe Premiere Pro" and spans[0][2]["rule"] == "lexicon.phrase"
    checks += 1

    # Synthetic names isolate matching from the accuracy of unknown-word G2P.
    # Each barrier must prevent the combined marker/semantic span. Output can
    # still follow the ordinary single-word/context policy on either side.
    customer.write_text("lexicon\tko-KR\ten-US:alpha beta\t통합발음\n"
                        "lexicon\tko-KR\ten-US:alpha\t알파\n"
                        "lexicon\tko-KR\ten-US:beta\t베타\n", encoding="utf-8")
    for middle in (", ", "\n", "-", " / ", " 12 ", "\u00a0", "<code> </code>",
                   " `x` ", " " * 250):
        text = "한국어 Alpha" + middle + "Beta는 설명입니다."
        for profile in profiles:
            previous = None
            for chunk in (1, 7, 4096):
                output, spans, events = stream(text, chunk=chunk, profile=profile,
                                                overlay=("--customer", str(customer)))
                assert "통합발음" not in output, (text, output)
                assert all(span["rule"] != "lexicon.phrase" for _, _, span in spans), spans
                if previous is not None:
                    assert events == previous, (text, profile, chunk)
                previous = events
                checks += 1
    # Exact raw input bound: 5 + 246 spaces + 4 = 255 bytes still matches;
    # one additional byte is outside the bounded phrase grammar.
    for count, matched in ((246, True), (247, False)):
        text = "Alpha" + " " * count + "Beta는 설명입니다."
        for profile in profiles:
            output, spans, _ = stream(text, profile=profile,
                                      overlay=("--customer", str(customer)))
            assert ("통합발음" in output) == matched, (count, output)
            assert any(span["rule"] == "lexicon.phrase" for _, _, span in spans) == matched, spans
            checks += 1
    for text in ("Alpha BetaMax는 설명입니다.", "AlphaBeta는 설명입니다.",
                 "XAlpha Beta는 설명입니다.", "Alpha Beta3는 설명입니다.",
                 "Alpha Beta-Pro는 설명입니다.", "Alpha Beta's는 설명입니다.",
                 "Alpha Beta’s는 설명입니다.", "Alpha Beta_Pro는 설명입니다.",
                 "Alpha Beta+Pro는 설명입니다."):
        output, spans, _ = stream(text, overlay=("--customer", str(customer)))
        assert "통합발음" not in output and all(span["rule"] != "lexicon.phrase"
                                            for _, _, span in spans), (text, spans)
        checks += 1
    for key in ("alpha-beta gamma", "alpha2 beta", "a b c d e f g h i", "a" * 94 + " bb"):
        customer.write_text(f"lexicon\tko-KR\ten-US:{key}\t잘못된읽기\n", encoding="utf-8")
        run = subprocess.run([cli, "normalize", "--resources", resources, "--customer",
                              str(customer), "Alpha Beta는 설명입니다."], capture_output=True,
                             timeout=10)
        assert run.returncode != 0, (key, run.stdout)
        checks += 1

print(f"{checks} multiword text/IR/layer/barrier/chunk/resource checks")
