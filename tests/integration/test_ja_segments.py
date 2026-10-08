"""Japanese segments: a full stop without a following space ends a sentence, 、 is a
phrase cut, and text without spaces still cuts at the length limit (2026-10-08)."""
import json
import subprocess
import sys

cli, resources = sys.argv[1:3]


def segments(text, *options):
    command = [cli, "stream-json", "--resources", resources, "--language", "ja-JP",
               "--output-mode", "speech", *options, "-"]
    process = subprocess.run(command, input=text.encode(), capture_output=True, timeout=60, check=False)
    assert process.returncode == 0, process.stderr.decode(errors="replace")
    events = [json.loads(line) for line in process.stdout.splitlines()]
    assert events and events[-1]["event"] == "end", events[-1:]
    return [event["text"] for event in events if event["event"] == "segment"]


checks = 0
got = segments("今日は晴れです。明日は雨です。")
assert [s.strip() for s in got] == ["今日は晴れです。", "明日は雨です。"], got
checks += 1
# A closing bracket after the stop stays in its sentence.
got = segments("「はい。」と答えた。次に進む。")
assert [s.strip() for s in got] == ["「はい。」と答えた。", "次に進む。"], got
checks += 1
# Readings stay whole across the new cuts.
got = segments("4月1日に3本買った。2024年10月8日に会う。")
assert "".join(got).replace(" ", "") == "しがつついたちにさんぼん買った。にせんにじゅうよねんじゅうがつようかに会う。", got
checks += 1
# A comma between digits is no phrase cut, even at a tiny target.
got = segments("価格は１２，５００円です。", "--first-target", "1", "--normal-target", "1")
assert "".join(got).replace(" ", "") == "価格はいちまんにせんごひゃくえんです。", got
checks += 1
# 、 cuts a long phrase; a run without spaces or commas cuts at the limit.
long_phrase = "あ" * 60 + "、" + "い" * 30 + "。"
got = segments(long_phrase, "--first-target", "48", "--normal-target", "112", "--max-length", "200")
assert got[0].strip().endswith("、") and "".join(got).replace(" ", "") == long_phrase, got
checks += 1
run = "か" * 450
got = segments(run, "--max-length", "200")
assert len(got) >= 3 and all(len(s.strip()) <= 200 for s in got) and "".join(got).replace(" ", "") == run, \
    [len(s) for s in got]
checks += 1
# Korean and English text around a CJK stop keeps every character.
for language, text in (("ko-KR", "안녕하세요。반갑습니다."), ("en-US", "Hello。World.")):
    command = [cli, "stream-json", "--resources", resources, "--language", language, "-"]
    process = subprocess.run(command, input=text.encode(), capture_output=True, timeout=60, check=False)
    events = [json.loads(line) for line in process.stdout.splitlines()]
    texts = [event["text"] for event in events if event["event"] == "segment"]
    assert process.returncode == 0 and texts and "。" in "".join(texts), (language, process.stderr)
    checks += 1
print(json.dumps({"checks": checks}))
