"""Kana preservation, bounded resource schema, layering and trace byte offsets."""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/web"))
from native import Engine, flatten

library, resources, cli = (Path(p).resolve() for p in sys.argv[1:])
options = dict(kana="korean", output_mode="speech", detailed_trace=True)
cases = [("っか", "っ가"), ("んか", "ん가"), ("かっん", "갓ん"),
         ("かんっ", "간っ"), ("ｯ", "ｯ"), ("ﾝ", "ﾝ"), ("ｯｶ", "ｯ가"),
         ("かー", "가"), ("か\ufe0f", "가"), ("ゔ", "ゔ"),
         ("Жか", "Ж가"), ("かЖか", "가Ж가")]
engine = Engine(library, resources, options)
try:
    for text, expected in cases:
        raw = text.encode()
        result = engine.normalize(raw)
        assert result["text"] == expected, (text, expected, result)
        assert flatten(engine.stream(raw, [1] * len(raw))) == result, text
    raw = "ｻｯﾎﾟﾛ".encode()
    result = engine.normalize(raw)
    assert result["text"] == "삿포로"
    assert len(result["spans"]) == 1
    span = result["spans"][0]
    assert span["semantic_type"] == "WORD" and span["rule"].startswith("cjk.kana.")
    assert span["source_lang"] == "und" and span["context_lang"] == span["render_lang"] == "ko-KR"
    events = [e for e in result["trace"] if e["kind"] == "kana_fold" and e["key"] == "ﾎﾟ"]
    assert len(events) == 1 and events[0]["layer"] == "system", events
    assert raw[events[0]["part_begin"]:events[0]["part_end"]].decode() == "ﾎﾟ", events
    assert any(e["detail"] == "cjk-unresolved" for e in engine.normalize("っ".encode())["trace"])
finally:
    engine.close()

for tsv, text, expected in [
    ("kana_reading\tko-KR\tか\t파\n", "かとう", "파토"),
    ("kana_long_after\tko-KR\t오:う\t0\n", "かとう", "가토우"),
    # User-approved policy: folded Katakana vowels are no longer absorbed.
    ("kana_fold\tko-KR\tﾄ\tコ\n", "ﾄｳｷｮｳ", "고우쿄우"),
]:
    engine = Engine(library, resources, dict(options, customer_tsv=tsv))
    try:
        result = engine.normalize(text.encode())
        assert result["text"] == expected, (text, expected, result)
        assert any(e["layer"] == "customer" and e["kind"].startswith("kana_")
                   for e in result["trace"]), result
    finally:
        engine.close()

for changed, text, expected in [
    (dict(unreadable="drop"), "っ", ""),
    (dict(default_language="en-US"), "This is かとう.", "This is かとう."),
    (dict(input_format="annotated"), "[[かとう]]{原文}", "原文"),
]:
    engine = Engine(library, resources, dict(options, **changed))
    try:
        assert engine.normalize(text.encode())["text"] == expected
    finally:
        engine.close()

validated = subprocess.run([str(cli), "validate-resource", "--resources", str(resources),
    "--require-complete-validation", str(resources / "languages/ko-KR/cjk/kana_readings.tsv")],
    check=True, capture_output=True, text=True)
report = json.loads(validated.stdout)
assert report["unvalidated_records"] == 0
for invalid in ["kana_reading\tko-KR\tか\tXX\n", "kana_fold\tko-KR\tﾎﾟ\tポロ\n",
                "kana_long_after\tko-KR\t오\tう\n", "kana_mark\tko-KR\tっ\tbad\n"]:
    try:
        invalid_engine = Engine(library, resources, dict(options, customer_tsv=invalid))
    except ValueError as error:
        assert "invalid resource" in str(error), error
    else:
        invalid_engine.close()
        raise AssertionError("invalid CJK resource accepted: " + invalid)
print("kana edge preservation, byte callbacks, trace, overrides and resource schema passed")
