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
from workbench import stream_sizes, validate

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
    assert catalog["defaults"]["detailed_trace"] is True
    assert {key: catalog["defaults"][key] for key in (
        "input_format", "output_mode", "unreadable", "max_control_bytes"
    )} == dict(input_format="annotated", output_mode="speech", unreadable="preserve", max_control_bytes=4096)
    assert catalog["defaults"]["invalid_control"] == "ignore"
    assert next(item for item in catalog["corpora"] if item["id"] == "user-patterns-v20")["count"] == 51
    assert catalog["reference_summary"]["human_approved_cases"] == 0
    assert catalog["reference_summary"]["pending_independent_review"] == catalog["reference_summary"]["cases"]
    assert all(item["reference_review"]["review_status"] == "pending-independent-review"
               for item in catalog["samples"] + catalog["corpora"])
    # Every help example is the engine's actual output under the named value.
    help_cases = 0
    for group in catalog["help"]["controls"]:
        for item in group["rows"]:
            assert run(item["example"], opts={"input_format": "annotated"})[0]["text"] == item["output"], item
            help_cases += 1
    for item in catalog["help"]["options"]:
        assert item["key"] in catalog["defaults"], item
        for example in item["examples"]:
            for value, output in example["outputs"].items():
                actual = run(example["text"], opts={item["key"]: value})[0]["text"]
                assert actual == output, (item["id"], value, actual, output)
                help_cases += 1
    assert help_cases > 60
    assert catalog["samples"][0]["text"] == "1~2만 원"
    assert catalog["samples"][0]["expected"] == "일에서 이만 원"
    phone_samples = [sample for sample in catalog["samples"] if sample["id"].startswith("phone-v18-")]
    assert len(phone_samples) >= 4 and all(sample["options"]["default_language"] == "en-US"
                                          for sample in phone_samples[:4])
    for sample in catalog["samples"]:
        result, _ = run(
            sample["text"], opts=sample.get("options"), expected=sample.get("expected")
        )
        assert result["expected_match"] is not False, (sample, result)
        assert result["reference_review"]["human_approved"] is False
    with urllib.request.urlopen(base + "/api/lexicon") as response:
        dictionary = json.load(response)
    assert dictionary["summary"]["entries"] >= 374
    assert dictionary["summary"]["scopes"]["finance"] == 46
    assert dictionary["summary"]["review_status"].get("human-reviewed", 0) == 0
    assert all(row["sources"] and row["source_file"].startswith("lexicons/") for row in dictionary["entries"])
    checks += 1
    result, _ = run("Apple의 정보입니다. foobarquux의 정보입니다. Please use foobarquux.")
    coverage = result["lexicon_coverage"]
    assert coverage["routes"] == {"direct-lexicon": 1, "fallback": 1}, coverage
    assert coverage["eligible_foreign_spans"] == 2
    assert coverage["review_candidates"][0]["canonical_key"] == "foobarquux"
    result, _ = run("ABS의 정보입니다. 1-2", opts={"domain": "finance-vocabulary"})
    assert result["text"] == "에이비에스의 정보입니다. 일 다시 이"  # hyphen_digits identifier policy, 2026-10-01; was 일에서 이
    assert any(s["lexicon"] == "lexicon:domain" for s in result["spans"])
    assert result["lexicon_coverage"]["terms"][0]["entry_id"] == "finance-abs"
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
    phone_text = "Call +1 (212) 555-0100 ext. 0042."
    phone_expected = ("Call plus one two one two five five five zero one zero zero "
                      "extension zero zero four two.")
    phone_result, _ = run(phone_text, "verify", {"default_language": "en-US", "chunk_bytes": 1}, phone_expected)
    assert phone_result["expected_match"] and phone_result["verification"]["passed"], phone_result
    phone_spans = [span for span in phone_result["spans"] if span["semantic_type"] == "PHONE"]
    assert len(phone_spans) == 1, phone_result
    assert (phone_spans[0]["value"], phone_spans[0]["secondary"], phone_spans[0]["unit"]) == (
        "+1-212-555-0100", "0042", "international"), phone_spans
    assert phone_text.encode()[phone_spans[0]["input_begin"]:phone_spans[0]["input_end"]].decode() == \
        "+1 (212) 555-0100 ext. 0042", phone_spans
    phone_result, _ = run("문의: 1588-0000", "verify", expected="문의: 일오팔팔 공공공공")
    assert phone_result["expected_match"] and phone_result["verification"]["passed"], phone_result
    assert any(span["semantic_type"] == "PHONE" and span["value"] == "1588-0000"
               and span["unit"] == "national" for span in phone_result["spans"]), phone_result
    account, _ = run("Account: 212-555-0100", opts={"default_language": "en-US"})
    assert any(span["semantic_type"] == "DIGIT_SEQUENCE" and span["unit"] == "financial_identifier"
               for span in account["spans"]) and not any(span["semantic_type"] == "PHONE"
                                                        for span in account["spans"]), account
    amount, _ = run("+1250000 x 2", opts={"default_language": "en-US"})
    assert any(span["semantic_type"] == "CARDINAL" and span["value"] == "+1250000"
               for span in amount["spans"]) and not any(span["semantic_type"] == "PHONE"
                                                       for span in amount["spans"]), amount
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
    # Frontend options traverse HTTP -> worker -> native, and remain request-local.
    for source, expected in (
        ("1{첫}번째", "첫번째"), ("21{@native}", "스물하나"),
        ("-12{@sino}", "마이너스 십이"), ("ABC{@alphabet}마트", "에이비씨마트"),
        ("[[Bank of America]]{지정 이름}", "지정 이름"),
        (r"A{\{이름\}}", "{이름}"), (r"\{첫\}", "{첫}"),
    ):
        result, _ = run(source, "verify", {"input_format": "annotated", "chunk_bytes": 1}, expected)
        assert result["expected_match"] and result["verification"]["passed"], result
    assert run("a1@b2.com")[0]["text"] == "에이일 앳 비이 닷 컴"
    assert run("a1@b2.com", opts={"output_mode": "text"})[0]["text"] == "a1@b2.com"
    # Unnamed characters follow the unreadable policy; v0.24 names emoji by default.
    assert run("가👨‍👩‍👧‍👦漢나", opts={"unreadable": "drop", "symbol_fallback": "off"})[0]["text"] == "가 나"
    assert run("가👨‍👩‍👧‍👦漢나", opts={"symbol_fallback": "off"})[0]["text"] == "가👨‍👩‍👧‍👦漢나"
    assert run("가🙂나")[0]["text"] == "가 살짝 미소 짓는 얼굴 나"
    assert run("가🙂나", opts={"output_mode": "text", "unreadable": "drop"})[0]["text"] == "가🙂나"
    for source, extra, message in (
        ("1{", {}, "invalid argument"),
        ("1{@unknown}", {}, "invalid argument"),
        ("A{a{b}}", {}, "invalid argument"),
        (r"A{\q}", {}, "invalid argument"),
        ("1{첫}", {"max_control_bytes": 4}, "limit"),
    ):
        with post("/api/run", dict(mode="stream", text=source, options={
            "input_format": "annotated", "chunk_bytes": 1, "invalid_control": "error", **extra
        })) as response:
            invalid = [json.loads(line) for line in response]
        assert invalid[-1]["event"] == "error" and message in invalid[-1]["message"], invalid
        assert not any(event["event"] in ("result", "segment", "end") for event in invalid), invalid
        checks += 1
    # By default an invalid control is ignored and the rest is read (2026-10-01).
    assert run("45821{@digit}은 예약 번호이다.")[0]["text"] == "사오팔이일은 예약 번호이다."  # five-digit policy 2026-10-02
    assert run("1{")[0]["text"] == "일{"
    checks += 2
    assert run("3개", expected="잘못된 정답")[0]["expected_match"] is False
    # Detailed evidence is enabled by default. Disabling it preserves the old
    # text/span transport, without trace fields or pointers leaking into JSON.
    trace_text = "Apple은 사과입니다. baba는 표기입니다."
    traced, _ = run(trace_text)
    legacy, _ = run(trace_text, opts={"detailed_trace": False})
    assert traced["trace_version"] == 1 and traced["trace"]
    assert "trace" not in legacy and "trace_version" not in legacy
    assert traced["text"] == legacy["text"] and traced["spans"] == legacy["spans"]
    assert any(event["stage"] == "lexicon" and event["key"] == "en-US:apple"
               and event["layer"] == "system" for event in traced["trace"])
    trace_options = {"customer_tsv": (
        "phone_vowel\tko-KR\tAE\t4\n"
        "phone_onset\tko-KR\tAE\t0\n"
        "g2p\ten-US\tbxxx\tB IY\n"
    ), "chunk_bytes": 1, "chunk_sizes": [1, 2, 3]}
    traced, _ = run("baba는 표기입니다.", opts=trace_options)
    assert traced["text"] == "버버는 표기입니다."
    source_rows = [event for event in traced["trace"] if event["kind"] == "g2p"]
    assert len(source_rows) == 4 and all(event["layer"] == "system" and
                                       event["stage"] == "source-pronunciation" and
                                       event["key"] in ("b", "a") for event in source_rows), traced
    target_rows = [event for event in traced["trace"] if event["kind"] == "phone_vowel"]
    assert len(target_rows) == 2 and all(event["layer"] == "customer" and
                                       event["stage"] == "target-phonetic" and
                                       event["key"] == "AE" for event in target_rows), traced
    assert not any(event["kind"] == "phone_onset" and event["key"] == "AE"
                   for event in traced["trace"]), traced
    streamed, stream_events = run("baba는 표기입니다.", "stream", trace_options)
    assert streamed["text"] == traced["text"] and streamed["spans"] == traced["spans"]
    assert streamed["trace"] == traced["trace"]
    assert all(event["trace_version"] == 1 and isinstance(event["trace"], list)
               for event in stream_events if event["event"] == "segment")
    verified, _ = run("baba는 표기입니다.", "verify", trace_options)
    assert verified["verification"]["passed"] and all(
        item["trace_match"] for item in verified["verification"]["variants"]), verified
    legacy, legacy_events = run(trace_text, "stream", {"detailed_trace": False, "chunk_bytes": 1})
    assert "trace" not in legacy and "trace_version" not in legacy
    assert all("trace" not in event and "trace_version" not in event
               for event in legacy_events if event["event"] == "segment")
    variant_rows = ("pronunciation\ten-US\tabacus\tAH S\n"
                    "pronunciation_variant\ten-US\tabacus#alternate\tB IY\n"
                    "pronunciation_choice\ten-US\tabacus\talternate\n")
    variant, _ = run("abacus는 표기입니다.", opts={"customer_tsv": variant_rows})
    assert variant["text"] == "비는 표기입니다."
    selected = [event for event in variant["trace"] if event["kind"] == "pronunciation_variant"]
    assert len(selected) == 1 and selected[0]["key"] == "abacus#alternate" and \
        selected[0]["layer"] == "customer" and selected[0]["value"] == "B IY", variant
    assert any(event["kind"] == "pronunciation_choice" and event["value"] == "alternate"
               for event in variant["trace"]), variant
    for invalid_choice in ("", "missing"):
        with post("/api/run", dict(mode="normalize", text="abacus는 표기입니다.", options={
            "customer_tsv": "pronunciation\ten-US\tabacus\tAH S\n"
                            f"pronunciation_choice\ten-US\tabacus\t{invalid_choice}\n"
        })) as response:
            invalid = [json.loads(line) for line in response]
        assert invalid[-1]["event"] == "error" and "invalid resource" in invalid[-1]["message"], invalid
        assert not any(event["event"] in ("result", "segment", "end") for event in invalid), invalid
        checks += 1
    fallback, _ = run("baba는 표기입니다.", opts={
        "customer_tsv": "pronunciation\ten-US\tbaba\tAH UNMAPPED\n"
    })
    assert fallback["text"] == "브아브아는 표기입니다."
    assert any(event["detail"] == "target-phonetic-failed" and event["disposition"] == "rejected"
               for event in fallback["trace"]), fallback
    assert any(event["stage"] == "target-grapheme" and event["disposition"] == "selected"
               for event in fallback["trace"]), fallback
    # A supplied sequence describes real native writes, including split UTF-8.
    text = "Apple은 사과입니다.\nBank of America는 은행입니다."
    custom = {"chunk_sizes": [1, 5, 2], "chunk_bytes": 65536}
    result, events = run(text, "stream", custom)
    writes = [e for e in events if e["event"] == "chunk"]
    assert [e["end"] - e["begin"] for e in writes] == [1, 5, 2, len(text.encode()) - 8]
    assert result["text"] == "애플은 사과입니다.\n뱅크 오브 아메리카는 은행입니다."
    result, events = run(text, "verify", custom)
    assert result["verification"]["passed"]
    assert result["verification"]["variants"][-1]["label"] == "custom writes"
    assert result["verification"]["variants"][-1]["chunk_count"] == 4
    result, _ = run("Apple", "stream", {"chunk_sizes": [65536, 5]})
    assert result["text"] == run("Apple")[0]["text"]
    assert stream_sizes(0, custom) == []
    # The total delay uses the actual custom schedule and excludes final sleep.
    validate(
        dict(
            mode="stream",
            text="x" * 81,
            options={"chunk_sizes": [1] * 81, "delay_ms": 100},
        )
    )
    checks += 1
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
    # The worker exposes actionable C policy diagnostics before normalization.
    with post("/api/run", dict(mode="normalize", text="3번 반복합니다.", options={
        "customer_tsv": "# editable policy\ncontext\tko-KR\tclause_weight\t9999\n"
    })) as response:
        invalid = [json.loads(line) for line in response]
    assert invalid[-1]["event"] == "error", invalid
    assert "customer.tsv:2" in invalid[-1]["message"], invalid
    assert "context/ko-KR/clause_weight" in invalid[-1]["message"], invalid
    assert not any(event["event"] in ("result", "end", "segment") for event in invalid)
    baseline, _ = run("Apple은 사과입니다.")
    unknown, _ = run("Apple은 사과입니다.", opts={
        "customer_tsv": "future.policy\t*\tkey\tvalue\n"
    })
    report = unknown["resource_validation"]
    assert report["records"] == report["validated_records"] + report["unvalidated_records"]
    assert report["unvalidated_records"] == baseline["resource_validation"]["unvalidated_records"] + 1
    assert unknown["text"] == baseline["text"] == "애플은 사과입니다."
    checks += 2
    with post("/api/corpus", {"corpus": "protected"}) as r:
        corpus = [json.loads(x) for x in r]
    assert corpus[-2]["passed"] == 40 and corpus[-2]["failed"] == 0
    assert corpus[-2]["human_approved_cases"] == 0
    assert all(row["reference_review"]["classification"] == "preservation-contract"
               for row in corpus[-2]["cases"])
    checks += 1
    with post("/api/corpus", {"corpus": "multiword-v8"}) as r:
        corpus = [json.loads(x) for x in r]
    assert corpus[-2]["passed"] == 30 and corpus[-2]["failed"] == 0
    assert corpus[-2]["pending_independent_review"] == 30
    assert next(row for row in corpus[-2]["cases"] if row["id"] == "multiword-after-effects")["reference_review"]["classification"] == "reference-policy"
    checks += 1
    with post("/api/corpus", {"corpus": "phone-v18"}) as response:
        corpus = [json.loads(line) for line in response]
    assert corpus[-2]["passed"] == 55 and corpus[-2]["failed"] == 0, corpus[-2]
    assert corpus[-2]["pending_independent_review"] == 55 and corpus[-2]["human_approved_cases"] == 0
    checks += 1
    with post("/api/corpus", {"corpus": "user-patterns-v20"}) as response:
        corpus = [json.loads(line) for line in response]
    assert corpus[-2]["passed"] == 51 and corpus[-2]["failed"] == 0, corpus[-2]
    assert corpus[-2]["pending_independent_review"] == 51 and corpus[-2]["human_approved_cases"] == 0
    checks += 1
    for body in [
        dict(mode="verify", text="x", options={"chunk_bytes": 0}),
        dict(mode="normalize", text="\0"),
        dict(mode="normalize", text="x" * 16385),
        dict(mode="normalize", text="x", options={"default_language": "../x"}),
        dict(mode="normalize", text="x", options={"user_lexicon": "x"}),
        dict(mode="normalize", text="x", options={"detailed_trace": "false"}),
        dict(mode="normalize", text="x", options={"detailed_trace": 1}),
        dict(mode="normalize", text="x", options={"input_format": "html"}),
        dict(mode="normalize", text="x", options={"output_mode": "audio"}),
        dict(mode="normalize", text="x", options={"unreadable": "guess"}),
        dict(mode="normalize", text="x", options={"max_control_bytes": 0}),
        dict(mode="normalize", text="x", options={"max_control_bytes": True}),
        dict(mode="normalize", text="x", options={"max_control_bytes": 16385}),
        dict(mode="stream", text="x", options={"chunk_sizes": [0]}),
        dict(mode="stream", text="x", options={"chunk_sizes": [True]}),
        dict(mode="stream", text="x", options={"chunk_sizes": [1] * 257}),
        dict(mode="stream", text="x", options={"chunk_sizes": "1,2"}),
        dict(
            mode="stream",
            text="x" * 1000,
            options={"chunk_sizes": [1] * 100, "chunk_bytes": 65536, "delay_ms": 100},
        ),
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
    # Jev judgement: the key stays on the server, requests are validated,
    # repeated pairs come from the cache, and no network is used here.
    import io
    import jev

    sent = []

    class FakeResponse(io.BytesIO):
        def __enter__(self):
            return self

        def __exit__(self, *exc):
            return False

    def fake_open(request, timeout):
        payload = json.loads(request.data)
        sent.append((request.get_header("Authorization"), payload))
        assert payload["state"]["written"] and payload["state"]["spoken"]
        return FakeResponse(json.dumps({"model": "jev-test", "answers": {
            "reading_correct": {"type": "noul", "noul": 0.8123}}}).encode())

    server.judge = jev.Judge(key="secret-test-key", opener=fake_open)
    body = dict(text="3만 원", normalized="삼만 원", language="ko-KR")
    for expect_cached in (False, True):
        with post("/api/judge", body) as response:
            judged = json.load(response)
        assert judged["noul"] == 0.8123 and judged["cached"] is expect_cached, judged
        assert "secret-test-key" not in json.dumps(judged)
        checks += 1
    assert len(sent) == 1 and sent[0][0] == "Bearer secret-test-key", sent
    assert judged["unread"] == [], judged
    assert jev.unread("코로나 십구로 인해 30~40명, KB 5%", "ko-KR") == ["30~40", "KB", "5", "%"]
    assert jev.unread("삼만 원", "ko-KR") == [] and jev.unread("three dollars", "en-US") == []
    checks += 1
    for bad in (dict(body, language="xx-XX"), dict(body, normalized=""), dict(body, text="가" * 2001)):
        try:
            post("/api/judge", bad)
        except urllib.error.HTTPError as e:
            assert e.code == 400, e.code
            checks += 1
        else:
            raise AssertionError("invalid judge request accepted")
    server.judge = jev.Judge(key=None, opener=fake_open)
    server.judge.key = None
    try:
        post("/api/judge", body)
    except urllib.error.HTTPError as e:
        assert e.code == 503
        checks += 1
    else:
        raise AssertionError("judge without a key answered")
    # Jev evaluation page: a sentence set, saved outputs, engine fill and a
    # streamed scoring run with a fake Jev batch.
    import tempfile as _tempfile
    eval_root = Path(_tempfile.mkdtemp(prefix="ttf-jev-eval-"))
    (eval_root / "t1").mkdir()
    sentences = [dict(id="s%d" % i, category="통화-금액", category_en="currency_amount", text=t)
                 for i, t in enumerate(["3만 원을 냈다.", "5개를 샀다.", "오늘 2시에 만나요."])]
    (eval_root / "t1/sentences.jsonl").write_text(
        "".join(json.dumps(r, ensure_ascii=False) + "\n" for r in sentences), encoding="utf-8")
    (eval_root / "t1/meta.json").write_text(json.dumps(dict(source="fixture", per_category=3)), encoding="utf-8")
    server.jev_eval_root = eval_root
    batches = []

    def fake_batch(pairs, language, cache, progress=None):
        batches.append(list(pairs))
        if progress:
            progress(0, len(pairs))
            progress(len(pairs), len(pairs))
        return {pair: (0.9, "jev-test") for pair in pairs}, len(pairs)

    server.jev_batch = fake_batch
    with urllib.request.urlopen(base + "/api/jev-eval/sets") as response:
        assert [x["set_id"] for x in json.load(response)["sets"]] == ["t1"]
    with urllib.request.urlopen(base + "/api/jev-eval/set?set=t1") as response:
        assert len(json.load(response)["rows"]) == 3
    with post("/api/jev-eval/save", dict(set="t1", system="legacy-tn",
                                          outputs={"s0": "삼만 원을 냈다.", "s1": "5개를 샀다."})) as response:
        assert json.load(response)["summary"]["overall"]["filled"] == 2
    for bad in (dict(set="t1", system="legacy-tn", outputs={"zz": "x"}),
                dict(set="t1", system="../x", outputs={}), dict(set="../t1", system="a", outputs={})):
        try:
            post("/api/jev-eval/save", bad)
        except urllib.error.HTTPError as e:
            assert e.code in (400, 404), e.code
            checks += 1
        else:
            raise AssertionError("invalid jev-eval save accepted")
    with post("/api/jev-eval/fill", dict(set="t1")) as response:
        filled_outputs = json.load(response)["outputs"]
    assert set(filled_outputs) == {"s0", "s1", "s2"} and all(filled_outputs.values()), filled_outputs
    with post("/api/jev-eval/score", dict(set="t1", system="legacy-tn")) as response:
        events = [json.loads(line) for line in response]
    assert events[0]["event"] == "progress" and events[-1]["event"] == "result", events
    overall = events[-1]["summary"]["overall"]
    assert (overall["scored"], overall["passed"], overall["unread"]) == (2, 1, 1), overall  # '5개' left unread
    assert len(batches[-1]) == 2
    with urllib.request.urlopen(base + "/api/jev-eval/system?set=t1&system=legacy-tn") as response:
        stored = json.load(response)
    assert stored["scores"]["s0"]["passed"] and not stored["scores"]["s1"]["passed"], stored["scores"]
    with urllib.request.urlopen(base + "/api/jev-eval/systems?set=t1") as response:
        assert json.load(response)["systems"][0]["summary"]["overall"]["pass_rate"] == 0.5
    # One row with an edited output: only that row is scored, others keep theirs.
    with post("/api/jev-eval/score", dict(set="t1", system="legacy-tn", ids=["s1"],
                                           patch={"s1": "다섯 개를 샀다."})) as response:
        events = [json.loads(line) for line in response]
    assert events[-1]["event"] == "result" and batches[-1] == [("5개를 샀다.", "다섯 개를 샀다.")], batches[-1]
    assert events[-1]["summary"]["overall"]["passed"] == 2 and set(events[-1]["scores"]) == {"s1"}
    with urllib.request.urlopen(base + "/api/jev-eval/system?set=t1&system=legacy-tn") as response:
        stored = json.load(response)
    assert stored["scores"]["s0"]["passed"] and stored["outputs"]["s1"] == "다섯 개를 샀다."
    # A system that was never saved can be scored row by row.
    with post("/api/jev-eval/score", dict(set="t1", system="new-tn", ids=["s2"],
                                           patch={"s2": "오늘 두 시에 만나요."})) as response:
        assert [json.loads(line) for line in response][-1]["summary"]["overall"]["scored"] == 1
    for bad in (dict(set="t1", system="legacy-tn", ids=["zz"]), dict(set="t1", system="legacy-tn", patch={"zz": "x"})):
        with post("/api/jev-eval/score", bad) as response:
            assert json.loads(response.read().splitlines()[-1])["event"] == "error"
        checks += 1
    with urllib.request.urlopen(base + "/jev-eval") as response:
        assert b"jev-eval.js" in response.read()
    checks += 6
    limited = jev.Judge(key="k", opener=fake_open, clock=lambda: 0.0)
    for n in range(jev.PER_CLIENT[0]):
        limited.score("%d원" % n, "원", "ko-KR", "client")
    try:
        limited.score("마지막", "마지막", "ko-KR", "client")
    except jev.JudgeError as e:
        assert e.status == 429
        checks += 1
    else:
        raise AssertionError("per-client limit not applied")
finally:
    server.shutdown()
    server.server_close()
    thread.join()
print(f"{checks} web API/ABI/policy/corpus/early-callback checks passed")
