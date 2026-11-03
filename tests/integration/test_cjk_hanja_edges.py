"""Independently specified Han/index contracts beyond the immutable 86 cases."""
from pathlib import Path
import json
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/web"))
from native import Engine, flatten

library, resources, cli = map(lambda s: Path(s).resolve(), sys.argv[1:])
# Unresolved characters stay as written here (preserve; the default is drop since 2026-11-03).
options = dict(hanja="korean", kana="korean", output_mode="speech", detailed_trace=True,
               unreadable="preserve")

def check(text, expected, extra=None):
    engine = Engine(library, resources, dict(options, **(extra or {})))
    try:
        raw = text.encode()
        result = engine.normalize(raw)
        assert result["text"] == expected, (text, expected, result)
        assert flatten(engine.stream(raw, [1] * len(raw))) == result, text
        for _ in range(3):
            assert engine.normalize(raw) == result, text
        return result
    finally:
        engine.close()

for text, expected in [("張勳", "장훈"), ("樂樂樂", "낙락락"), ("女Ж女", "여Ж여"),
                       ("女𠮷女", "여𠮷여"), ("音樂𠮷", "음악𠮷"),
                       ("音\ufe0f樂", "음악"), ("女\U000e0100子", "여자"),
                       ("𠮷\U000e0100", "𠮷\U000e0100")]:
    check(text, expected)
check("音樂𠮷", "음악", dict(unreadable="drop"))
result = check("音樂", "음악")
assert result["spans"][0]["lexicon"] == "lexicon:system"
assert result["spans"][0]["rule"] == "cjk.hanja.word"
assert result["spans"][0]["source_lang"] == "und"
assert result["spans"][0]["pronunciation_layer"] == "system"
assert any(e["kind"] == "cjk_word" and e["key"] == "音樂" and e["value"] == "음악"
           for e in result["trace"])
result = check("女子", "여자")
assert any(e["kind"] == "hanja_reading" and e["key"] == "女" for e in result["trace"])
assert any(e["detail"] == "cjk-initial-variant" and e["value"] == "여" for e in result["trace"])

# Higher layer before longest match; then longest, specificity and first declaration.
check("音樂", "소리락", dict(customer_tsv="cjk_word\tko-KR\t音\t소리\n"))
check("音樂", "노래", dict(customer_tsv="cjk_word\tko-KR\t音\t소리\n"
      "cjk_word\tko-KR\t音樂\t노래\ncjk_word\tko-KR\t音樂\t다른\n"))
check("音樂", "고객", dict(customer_tsv="cjk_word\t*\t音樂\t공통\n"
      "cjk_word\tko-KR\t音樂\t고객\n"))
check("樂樂", "악악", dict(customer_tsv="hanja_reading\tko-KR\t樂\t악|락\n"))
check("女かた", "가타", dict(customer_tsv="cjk_word\tko-KR\t女か\t가\n"))
check("女かた", "여かた", dict(kana="passthrough", customer_tsv="cjk_word\tko-KR\t女か\t가\n"))
check("This is 音樂.", "This is 音樂.", dict(default_language="en-US"))
check("[[女子]]{녀자}", "녀자", dict(input_format="annotated"))

for filename in ("hanja_readings.tsv", "cjk_words.tsv"):
    proc = subprocess.run([str(cli), "validate-resource", "--resources", str(resources),
        "--require-complete-validation", str(resources / "languages/ko-KR/cjk" / filename)],
        check=True, capture_output=True, text=True)
    assert json.loads(proc.stdout)["unvalidated_records"] == 0
for row in ("hanja_reading\tko-KR\t女\t녀|\n", "hanja_reading\tko-KR\t女子\t녀\n",
            "cjk_word\tko-KR\t女子女子女子女\t여자\n", "cjk_word\tko-KR\tA女\t여자\n",
            "cjk_word\tko-KR\t女子\tUNK\n"):
    try:
        engine = Engine(library, resources, dict(options, customer_tsv=row))
    except ValueError as error:
        assert "invalid resource" in str(error), error
    else:
        engine.close()
        raise AssertionError("invalid Han resource accepted")
print("Han/word trace, variants, layers, longest match, selectors, mixed options, determinism passed")
