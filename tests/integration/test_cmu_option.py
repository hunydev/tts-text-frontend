"""english_dictionary=cmu loads a CMUdict runtime export (synthetic here) below authored lexicons."""
import os
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/web"))
from native import Engine  # noqa: E402

library, resources = (Path(x) for x in sys.argv[1:3])
with tempfile.TemporaryDirectory() as d:
    export = Path(d) / "pronunciation.tsv"
    export.write_text("pronunciation\ten-US\thelsinki\tHH EH1 L S IH0 NG K IY0\n"
                      "pronunciation\ten-US\tworcester\tW UH1 S T ER0\n"
                      "pronunciation\ten-US\tkorea\tK AO0 R IY1 AH0\n", encoding="utf-8")
    os.environ["TTF_CMU_RUNTIME"] = str(export)
    base = dict(output_mode="speech", default_language="ko-KR")
    plain = Engine(library, resources, base).normalize("Helsinki 여행".encode())["text"]
    cmu = Engine(library, resources, dict(base, english_dictionary="cmu"))
    assert cmu.normalize("Helsinki 여행".encode())["text"] == "헬싱키 여행", cmu.normalize("Helsinki 여행".encode())
    # The spelling rules read Helsinki right too since 2026-10-05 (nk), so the
    # option is shown changing a word they cannot spell out: Worcester 우스터.
    plain = Engine(library, resources, base).normalize("Worcester 여행".encode())["text"]
    assert cmu.normalize("Worcester 여행".encode())["text"] == "우스터 여행", cmu.normalize("Worcester 여행".encode())
    assert plain != "우스터 여행", plain
    # An authored lexicon entry (korea 코리아) still wins over the dictionary.
    assert cmu.normalize("Korea 대표팀".encode())["text"] == "코리아 대표팀"
    os.environ["TTF_CMU_RUNTIME"] = str(Path(d) / "missing.tsv")
    try:
        Engine(library, resources, dict(base, english_dictionary="cmu"))
    except ValueError as exc:
        assert "CMU pronunciation export not found" in str(exc)
    else:
        raise AssertionError("missing export accepted")
print("cmu option ok")
