"""ABI, live callback transport, policies, validation and worker isolation."""

import ctypes
import json
from pathlib import Path
import subprocess
import sys
import threading
import urllib.error
import urllib.request

library, resources, cli, layout = map(lambda p: str(Path(p).resolve()), sys.argv[1:])
root = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(root / "tools/web"))
import native
from server import Handler, Server
from workbench import validate

# Detect packing/alignment drift before executing any native operation.
for line in subprocess.check_output([layout], text=True).splitlines():
    key, expected = line.split()
    parts = key.split(".")
    kind = getattr(native, parts[0])
    actual = getattr(kind, parts[1]).offset if len(parts) > 1 else ctypes.sizeof(kind)
    assert actual == int(expected), (key, actual, expected)

server = Server(("127.0.0.1", 0), Handler)
server.library = Path(library)
server.resources = Path(resources)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
base = f"http://127.0.0.1:{server.server_port}"
checks = 0


def post(path, body):
    request = urllib.request.Request(
        base + path,
        data=json.dumps(body).encode(),
        headers={"Content-Type": "application/json"},
    )
    return urllib.request.urlopen(request, timeout=30)


def run(text, mode="normalize", opts=None, expected=None):
    global checks
    body = dict(mode=mode, text=text, options=opts or {})
    if expected is not None:
        body["expected"] = expected
    with post("/api/run", body) as response:
        events = [json.loads(line) for line in response]
    assert events[-1] == dict(event="end", ok=True), events
    assert all(e["event"] != "error" for e in events), events
    result = next(e for e in events if e["event"] == "result")
    checks += 1
    return result, events


try:
    with urllib.request.urlopen(base + "/api/catalog") as response:
        catalog = json.load(response)
    assert catalog["abi_version"] == 1 and len(catalog["samples"]) >= 8
    for sample in catalog["samples"]:
        result, _ = run(
            sample["text"], opts=sample.get("options"), expected=sample.get("expected")
        )
        assert result["expected_match"] is not False, (sample, result)
    text = "1st는 첫 번째입니다.\nPlease go to the 1st floor"
    result, events = run(text, "verify")
    assert (
        result["verification"]["passed"]
        and len(result["verification"]["variants"]) == 4
    )
    cli_output = subprocess.check_output(
        [cli, "normalize", "--resources", resources, text]
    ).decode()
    assert result["text"] + "\n" == cli_output
    assert any(
        s["context_lang"] == "ko-KR" and s["source_lang"] == "en-US"
        for s in result["spans"]
    )
    assert result["tokens"][0]["surface"] == "1"
    result, _ = run(
        "Apple은 사과입니다.",
        opts={
            "user_lexicon": [dict(word="Apple", pronunciation="사용자애플")],
            "customer_tsv": "lexicon\tko-KR\ten-US:apple\t고객애플\n",
        },
    )
    assert result["text"] == "사용자애플은 사과입니다."
    assert result["spans"][0]["pronunciation_layer"] == "user"
    assert run("Apple은 사과입니다.")[0]["text"] == "애플은 사과입니다."
    assert (
        run("Apple은 사과입니다.", opts={"render_policy": "preserve"})[0]["text"]
        == "Apple은 사과입니다."
    )
    result, _ = run("WiFi를 사용합니다.", opts={"remove_system_words": ["wifi"]})
    assert result["text"] and "WiFi" not in result["text"]
    assert result["spans"][0]["lexicon"] != "lexicon:system"
    assert run("")[0]["text"] == ""
    assert run("3개", expected="잘못된 정답")[0]["expected_match"] is False
    # First callback must arrive through HTTP before all simulated writes finish.
    body = dict(
        mode="stream",
        text="Apple은 사과입니다.\n가격은 12,500원입니다.",
        options={"chunk_bytes": 7, "delay_ms": 30},
    )
    with post("/api/run", body) as response:
        events = []
        seen_first = False
        for line in response:
            event = json.loads(line)
            events.append(event)
            if event["event"] == "segment" and not seen_first:
                assert event["stage"] == "write" and event["input_end"] < len(
                    body["text"].encode()
                )
                assert not any(e["event"] == "end" for e in events)
                seen_first = True
        assert seen_first and events[-1]["event"] == "end"
    assert (
        "".join(e["text"] for e in events if e["event"] == "segment")
        == run(body["text"])[0]["text"]
    )
    checks += 1
    # Errors from the worker are terminal, with no successful end event.
    with post(
        "/api/run",
        dict(mode="normalize", text="Apple", options={"customer_tsv": "malformed row"}),
    ) as r:
        errors = [json.loads(x) for x in r]
    assert errors[-1]["event"] == "error" and not any(
        x["event"] == "end" for x in errors
    )
    with post("/api/corpus", {"corpus": "protected"}) as r:
        corpus = [json.loads(x) for x in r]
    assert corpus[-2]["passed"] == 40 and corpus[-2]["failed"] == 0
    checks += 1
    for body in [
        dict(mode="verify", text="x", options={"chunk_bytes": 0}),
        dict(mode="normalize", text="\0"),
        dict(mode="normalize", text="x" * 16385),
        dict(mode="normalize", text="x", options={"default_language": "../x"}),
        dict(mode="normalize", text="x", options={"user_lexicon": "x"}),
    ]:
        try:
            post("/api/run", body)
        except urllib.error.HTTPError as e:
            assert e.code == 400
            checks += 1
        else:
            raise AssertionError("invalid request accepted")
    request = urllib.request.Request(
        base + "/api/run",
        data=b"{}",
        headers={
            "Content-Type": "application/json",
            "Origin": "https://foreign.invalid",
        },
    )
    try:
        urllib.request.urlopen(request)
    except urllib.error.HTTPError as e:
        assert e.code == 403
        checks += 1
    else:
        raise AssertionError("cross-origin accepted")
    try:
        urllib.request.urlopen(base + "/../native.py")
    except urllib.error.HTTPError as e:
        assert e.code == 404
        checks += 1
    else:
        raise AssertionError("filesystem path exposed")
finally:
    server.shutdown()
    server.server_close()
    thread.join()
print(f"{checks} web API/ABI/policy/corpus/early-callback checks passed")
