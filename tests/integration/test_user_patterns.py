#!/usr/bin/env python3
"""Independent v0.20 written-output, annotation and streaming contracts.

The JSONL text references were authored before executing the candidate, under
explicit product reading policies. They are not human-approved linguistic gold.
Extra errors, overrides and ABI checks have independent expectations below.
No audio synthesis, third-party corpus, or candidate-derived gold is used.
"""
import argparse
import ctypes as C
import importlib.util
import json
import random
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CORPUS = ROOT / "tests/evaluation/user_patterns_v20.jsonl"
PROFILES = ((), ("--first-target", "1", "--normal-target", "3", "--max-length", "12"))
EXTRA_ALIASES = (
    (r"A{가\{나\}다}", "가{나}다"),
    (r"A{가\\나}", "가\\나"),
    (r"A{\[첫\]}", "[첫]"),
    (r"A{1\{@sino\}}", "1{@sino}"),
    ("[[두 낱말]]{한 문장}", "한 문장"),
    ("[[Apple pie]]{애플 파이}는", "애플 파이는"),
    ("k7{커스텀}", "커스텀"),
    ("1{2{@sino}}", None),  # Raw nesting is a syntax error, never recursive TN.
)
ERRORS = (
    "1{}", "{첫}", "1{{첫}}", "1{첫", "1}", "1{첫}}", r"1{\q}",
    "1{@unknown}", "1{@native bad}", "-1{@native}", "100{@native}",
    "1.5{@native}", "漢{@alphabet}", "[[두 낱말]]", "[[두 낱말]{첫}",
)
EXTRA_PATTERNS = (
    ("ko-KR", "20°F", "화씨 이십 도"),
    ("ko-KR", "건물 20°F", "건물 화씨 이십 도"),
    ("en-US", "1°F", "one degree Fahrenheit"),
    ("en-US", "2°F", "two degrees Fahrenheit"),
)
EXTRA_CHARACTERS = (
    ("가1️⃣나", "preserve", "가1️⃣나"),
    ("가1️⃣나", "drop", "가 나"),
    ("🙂", "drop", ""),
)
EXACT_ALIAS_WITH_DROP = ("A{🙂漢}", "🙂漢")
EXTRA_EN_ALNUM = (
    ("k7", "kay seven"), ("K7", "kay seven"),
    ("top100", "top one hundred"), ("top007", "top zero zero seven"),
    ("GPT-4", "gee pee tee four"),
)


def rows():
    data = [json.loads(line) for line in CORPUS.read_text(encoding="utf-8").splitlines()]
    assert len(data) == 51
    return data


def options(row):
    return ["--language", row["default_language"],
            "--output-mode", row.get("output_mode", "speech"),
            "--input-format", row.get("input_format", "plain"),
            "--unreadable", row.get("unreadable", "preserve")]


def stream(cli, resources, row, chunk, profile=(), extra=(), success=True, traced=False):
    cmd = [str(cli), "stream-json", "--resources", str(resources), *options(row),
           "--chunk-bytes", str(chunk), *profile, *extra, *(["--trace"] if traced else []), "-"]
    run = subprocess.run(cmd, input=row["text"].encode(), capture_output=True,
                         timeout=20, check=False)
    events = [json.loads(line) for line in run.stdout.splitlines()]
    if not success:
        assert run.returncode != 0, (row, run.stdout, run.stderr)
        assert not any(event.get("event") == "end" for event in events), events
        return events
    assert run.returncode == 0, (row["id"], run.stderr.decode(errors="replace"))
    assert events and events[-1]["event"] == "end", (row["id"], events[-1:])
    spans, output, trace, offset = [], [], [], 0
    for event in events:
        if event["event"] != "segment":
            continue
        spans.extend(dict(span, begin=span["begin"] + offset, end=span["end"] + offset)
                     for span in event["spans"])
        if traced:
            assert event["trace_version"] == 1
            trace.extend(dict(item, output_begin=item["output_begin"] + offset,
                              output_end=item["output_end"] + offset) for item in event["trace"])
        offset += len(event["text"].encode())
        output.append(event["text"])
    result = dict(text="".join(output), spans=spans)
    if traced:
        result["trace"] = trace
    return result


def row_for(text, expected=None, annotated=True):
    return dict(id=text, text=text, expected=expected, default_language="ko-KR",
                output_mode="speech", input_format="annotated" if annotated else "plain",
                unreadable="preserve")


def cli_checks(args):
    checks = 0
    for row in rows():
        # A batch command has exactly one trailing CLI newline, independent of
        # source newlines. Its reference is authored text, not the stream result.
        run = subprocess.run([str(args.cli), "normalize", "--resources", str(args.resources),
                              *options(row), "--", row["text"]], capture_output=True, timeout=20)
        assert run.returncode == 0, (row["id"], run.stderr)
        assert run.stdout.decode() == row["expected"] + "\n", (row["id"], run.stdout, row["expected"])
        checks += 1
        for profile in PROFILES:
            reference = stream(args.cli, args.resources, row, 4096, profile)
            assert reference["text"] == row["expected"], (row["id"], reference, row["expected"])
            source = row["text"].encode()
            # Source coverage remains complete when an unknown character drops
            # to a zero-length output span; all indices must remain UTF-8 cuts.
            input_at, output_at = 0, 0
            for span in reference["spans"]:
                assert span["input_begin"] == input_at and span["begin"] == output_at, (row["id"], span)
                source[span["input_begin"]:span["input_end"]].decode()
                reference["text"].encode()[span["begin"]:span["end"]].decode()
                input_at, output_at = span["input_end"], span["end"]
            assert input_at == len(source) and output_at == len(reference["text"].encode()), row["id"]
            checks += 2
            if row["id"] in ("user-v20-alnum-k7", "user-v20-alnum-top100"):
                assert len(reference["spans"]) == 1 and reference["spans"][0]["semantic_type"] == "ALNUM_TERM", reference
                checks += 1
            if row["id"].startswith("user-v20-native-") or row["id"] in (
                    "user-v20-sino-one", "user-v20-sino-positive", "user-v20-sino-negative", "user-v20-alphabet-zero"):
                annotation = reference["spans"]
                directive = row["text"].split("{")[1][:-1]
                assert len(annotation) == 1 and annotation[0]["semantic_type"] == "CUSTOM", annotation
                assert annotation[0]["unit"] == directive, annotation
                checks += 1
            for chunk in (1, 2, 7):
                actual = stream(args.cli, args.resources, row, chunk, profile)
                assert actual == reference, (row["id"], profile, chunk, reference, actual)
                checks += 1
    for text, expected in EXTRA_ALIASES:
        if expected is None:
            continue
        row = row_for(text, expected)
        for profile in PROFILES:
            actual = stream(args.cli, args.resources, row, 1, profile)
            assert actual["text"] == expected, (row, actual)
            checks += 1
    for language, text, expected in EXTRA_PATTERNS:
        row = row_for(text, expected, annotated=False)
        row["default_language"] = language
        for profile in PROFILES:
            actual = stream(args.cli, args.resources, row, 1, profile)
            assert actual["text"] == expected, (row, actual)
            spans = [span for span in actual["spans"] if span["semantic_type"] == "MEASURE"]
            assert len(spans) == 1 and spans[0]["unit"] != "floor", actual
            checks += 2
    row = row_for(*EXACT_ALIAS_WITH_DROP)
    row["unreadable"] = "drop"
    assert stream(args.cli, args.resources, row, 1)["text"] == row["expected"]
    checks += 1
    for text, unreadable, expected in EXTRA_CHARACTERS:
        row = row_for(text, expected, annotated=False)
        row["unreadable"] = unreadable
        for profile in PROFILES:
            actual = stream(args.cli, args.resources, row, 1, profile)
            assert actual["text"] == expected, (row, actual)
            assert actual["spans"][-1]["input_end"] == len(text.encode()), actual
            checks += 2
    for text, expected in EXTRA_EN_ALNUM:
        row = dict(row_for(text, expected, annotated=False), default_language="en-US")
        for profile in PROFILES:
            actual = stream(args.cli, args.resources, row, 1, profile)
            assert actual["text"] == expected, (row, actual)
            assert len(actual["spans"]) == 1 and actual["spans"][0]["semantic_type"] == "ALNUM_TERM", actual
            checks += 2
    for text in ERRORS + tuple(text for text, expected in EXTRA_ALIASES if expected is None):
        for chunk in (1, 4096):
            events = stream(args.cli, args.resources, row_for(text), chunk, success=False)
            assert not any(event.get("event") == "segment" for event in events), (text, events)
            checks += 1
    # Reserved controls are inert under the explicit plain format.
    plain = row_for("1{첫}번째", "일{첫}번째", annotated=False)
    assert stream(args.cli, args.resources, plain, 1)["text"] == plain["expected"]
    checks += 1
    # A full customer pronunciation wins over the alnum prefix/suffix policy.
    with tempfile.TemporaryDirectory(prefix="ttf-v20-overrides-") as tmp:
        path = Path(tmp) / "customer.tsv"
        path.write_text("lexicon\tko-KR\ten-US:k7\t케이세븐\n", encoding="utf-8")
        row = row_for("k7", "케이세븐", annotated=False)
        for profile in PROFILES:
            actual = stream(args.cli, args.resources, row, 1, profile, ("--customer", str(path)))
            assert actual["text"] == row["expected"], actual
            checks += 1
        path.write_text("speech_character\tko-KR\t漢\t한\n", encoding="utf-8")
        for unreadable in ("preserve", "drop"):
            row = row_for("가漢나", "가한나", annotated=False)
            row["unreadable"] = unreadable
            actual = stream(args.cli, args.resources, row, 1, extra=("--customer", str(path)))
            assert actual["text"] == row["expected"], actual
            checks += 1
        path.write_text("speech_symbol\tko-KR\t@\t골뱅이\n", encoding="utf-8")
        row = row_for("a1@b2.com", "에이일 골뱅이 비이 닷 씨오엠", annotated=False)
        assert stream(args.cli, args.resources, row, 1, extra=("--customer", str(path)))["text"] == row["expected"]
        checks += 1
    return checks + final_trace_checks(args)


def final_trace_checks(args):
    checks = 0
    for cluster in ("🙂", "漢", "1️⃣"):
        for policy in ("preserve", "drop"):
            row = row_for("가" + cluster + "나", annotated=False)
            row["unreadable"] = policy
            # A removed character between two words leaves one separating space.
            row["expected"] = "가" + (cluster if policy == "preserve" else " ") + "나"
            for profile in PROFILES:
                reference = stream(args.cli, args.resources, row, 4096, profile, traced=True)
                assert reference["text"] == row["expected"]
                selected = [event for event in reference["trace"]
                            if event["detail"] == "final-unreadable-" + ("preserved" if policy == "preserve" else "dropped")]
                assert len(selected) == 1, reference
                event = selected[0]
                width = len(cluster.encode())
                assert (event["input_begin"], event["input_end"]) == (3, 3 + width), event
                assert (event["output_begin"], event["output_end"]) == (3, 3 + (width if policy == "preserve" else 1)), event
                assert (event["part_begin"], event["part_end"]) == (0, width), event
                assert event["stage"] == "policy" and event["disposition"] == "selected", event
                assert not event["layer_known"] and event["declaration_order"] is None, event
                assert stream(args.cli, args.resources, row, 1, profile, traced=True) == reference
                checks += 7
    with tempfile.TemporaryDirectory(prefix="ttf-v20-keycap-") as tmp:
        path = Path(tmp) / "customer.tsv"
        path.write_text("speech_character\tko-KR\t1️⃣\t키캡일\n", encoding="utf-8")
        for policy in ("preserve", "drop"):
            row = row_for("가1️⃣나", "가키캡일나", annotated=False)
            row["unreadable"] = policy
            reference = stream(args.cli, args.resources, row, 4096,
                               extra=("--customer", str(path)), traced=True)
            assert reference["text"] == row["expected"], reference
            consumed = [event for event in reference["trace"] if event["kind"] == "speech_character"]
            assert len(consumed) == 1, reference
            event = consumed[0]
            assert (event["key"], event["value"], event["detail"]) == ("1️⃣", "키캡일", "final-cluster-mapped"), event
            assert event["layer_known"] and event["layer"] == "customer" and event["resource_language"] == "ko-KR", event
            assert isinstance(event["declaration_order"], int), event
            assert (event["input_begin"], event["input_end"], event["output_begin"], event["output_end"]) == (3, 10, 3, 12), event
            assert (event["part_begin"], event["part_end"]) == (0, 7), event
            assert not any(item["detail"].startswith("final-unreadable-") for item in reference["trace"]), reference
            assert stream(args.cli, args.resources, row, 1,
                          extra=("--customer", str(path)), traced=True) == reference
            checks += 9
        # Equal-valued overrides still need truthful consumed-row provenance.
        # Forced alphabet rendering must never invoke dictionary/G2P fallback.
        path.write_text("speech_symbol\tko-KR\t@\t골뱅이\n"
                        "native_standalone\tko-KR\t20\t스물\n"
                        "phone_letter\tko-KR\tletter.A\t에이\n", encoding="utf-8")
        for text, expected, kind, key, part in (
            ("a1@b2.com", "에이일 골뱅이 비이 닷 씨오엠", "speech_symbol", "@", (2, 3)),
            ("21{@native}", "스물하나", "native_standalone", "20", None),
            ("ABC{@alphabet}", "에이비씨", "phone_letter", "letter.A", (0, 1)),
        ):
            row = row_for(text, expected, annotated="{@" in text)
            reference = stream(args.cli, args.resources, row, 4096,
                               extra=("--customer", str(path)), traced=True)
            assert reference["text"] == expected, reference
            consumed = [event for event in reference["trace"] if event["kind"] == kind and event["key"] == key]
            assert len(consumed) == 1, reference
            event = consumed[0]
            assert event["layer_known"] and event["layer"] == "customer" and event["resource_language"] == "ko-KR", event
            assert event["stage"] == "policy" and event["disposition"] == "selected", event
            if part is not None:
                assert (event["part_begin"], event["part_end"]) == part, event
            if kind == "native_standalone":
                assert {item["key"] for item in reference["trace"] if item["kind"] == kind} == {"20", "1"}, reference
            assert not any(item["kind"] in ("g2p", "pronunciation", "pronunciation_variant")
                           for item in reference["trace"]), reference
            assert stream(args.cli, args.resources, row, 1,
                          extra=("--customer", str(path)), traced=True) == reference
            checks += 7
    return checks


class FrontendOptions(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("abi_version", C.c_uint32),
                ("input_format", C.c_int), ("output_mode", C.c_int),
                ("unreadable_policy", C.c_int), ("max_control_bytes", C.c_size_t)]


class Native:
    """Use the unchanged ABI structures, without relying on web option wiring."""
    def __init__(self, args, row, tiny=False, configure=True):
        self.types = load_native_types()
        t, self.lib = self.types, C.CDLL(str(args.library))
        definitions = {
            "ttf_config_init": (None, [C.POINTER(t.Config)]),
            "ttf_engine_create": (C.c_int, [C.POINTER(t.Config), C.POINTER(C.c_void_p)]),
            "ttf_engine_destroy": (None, [C.c_void_p]),
            "ttf_engine_freeze": (C.c_int, [C.c_void_p]),
            "ttf_frontend_options_init": (None, [C.POINTER(FrontendOptions)]),
            "ttf_engine_set_frontend_options": (C.c_int, [C.c_void_p, C.POINTER(FrontendOptions)]),
            "ttf_normalize": (C.c_int, [C.c_void_p, C.c_char_p, C.c_size_t, C.POINTER(t.Result)]),
            "ttf_result_free": (None, [C.POINTER(t.Result)]),
            "ttf_stream_create": (C.c_int, [C.c_void_p, t.Callback, C.c_void_p, C.POINTER(C.c_void_p)]),
            "ttf_stream_write": (C.c_int, [C.c_void_p, C.c_char_p, C.c_size_t]),
            "ttf_stream_flush": (C.c_int, [C.c_void_p]),
            "ttf_stream_destroy": (None, [C.c_void_p]),
            "ttf_lexicon_add": (C.c_int, [C.c_void_p, C.c_int, C.c_char_p, C.c_char_p, C.c_char_p, C.c_char_p]),
        }
        for name, (restype, argtypes) in definitions.items():
            fn = getattr(self.lib, name); fn.restype = restype; fn.argtypes = argtypes
        cfg = t.Config(); self.lib.ttf_config_init(C.byref(cfg))
        cfg.resource_root = str(args.resources).encode(); cfg.default_language = row["default_language"].encode()
        if tiny:
            cfg.first_segment_target, cfg.normal_segment_target, cfg.max_segment_length = 1, 3, 12
        self.handle = C.c_void_p()
        assert self.lib.ttf_engine_create(C.byref(cfg), C.byref(self.handle)) == 0
        self.opts = FrontendOptions(); self.lib.ttf_frontend_options_init(C.byref(self.opts))
        assert self.opts.input_format == 0 and self.opts.output_mode == 1 and self.opts.unreadable_policy == 0
        self.opts.input_format = row.get("input_format") == "annotated"
        self.opts.output_mode = row.get("output_mode", "speech") == "speech"
        self.opts.unreadable_policy = row.get("unreadable", "preserve") == "drop"
        if configure:
            assert self.lib.ttf_engine_set_frontend_options(self.handle, C.byref(self.opts)) == 0

    def close(self):
        self.lib.ttf_engine_destroy(self.handle)

    def freeze(self):
        assert self.lib.ttf_engine_freeze(self.handle) == 0

    @staticmethod
    def span(span, offset):
        result = {}
        for key, _ in span._fields_:
            if key == "semantic":
                result[key] = {name: getattr(span.semantic, name) for name, _ in span.semantic._fields_}
            else:
                result[key] = getattr(span, key)
        result["begin"] += offset; result["end"] += offset
        return result

    def normalize(self, source, expected_status=0):
        result = self.types.Result()
        status = self.lib.ttf_normalize(self.handle, source, len(source), C.byref(result))
        assert status == expected_status, (source, status, expected_status)
        try:
            if status:
                assert not result.text and not result.length and not result.spans and not result.span_count
                return None
            return dict(text=C.string_at(result.text, result.length).decode(),
                        spans=[self.span(result.spans[i], 0) for i in range(result.span_count)])
        finally:
            self.lib.ttf_result_free(C.byref(result))

    def stream(self, source, schedule):
        events, stream_handle, callback_errors = [], C.c_void_p(), []
        offset = 0
        @self.types.Callback
        def callback(pointer, _):
            nonlocal offset
            try:
                value = pointer.contents
                # Copy callback-owned buffers before returning to the engine.
                text = C.string_at(value.text, value.length).decode()
                spans = [self.span(value.spans[i], offset) for i in range(value.span_count)]
                events.append((text, spans)); offset += value.length
                return 0
            except Exception as error:
                callback_errors.append(error); return 1
        assert self.lib.ttf_stream_create(self.handle, callback, None, C.byref(stream_handle)) == 0
        at = 0
        try:
            for size in schedule:
                chunk = source[at:at + size]
                if not chunk:
                    break
                assert self.lib.ttf_stream_write(stream_handle, chunk, len(chunk)) == 0, (source, schedule, at)
                at += len(chunk)
            if at < len(source):
                chunk = source[at:]
                assert self.lib.ttf_stream_write(stream_handle, chunk, len(chunk)) == 0
            assert self.lib.ttf_stream_flush(stream_handle) == 0, (source, schedule)
            assert not callback_errors, callback_errors
            return dict(text="".join(text for text, _ in events), spans=[span for _, spans in events for span in spans])
        finally:
            self.lib.ttf_stream_destroy(stream_handle)


_TYPES = None

def load_native_types():
    global _TYPES
    if _TYPES is None:
        spec = importlib.util.spec_from_file_location("user_patterns_native_types", ROOT / "tools/web/native.py")
        _TYPES = importlib.util.module_from_spec(spec); spec.loader.exec_module(_TYPES)
    return _TYPES


def native_checks(args):
    if not args.library:
        return 0
    checks = 0
    native_rows = rows() + [row_for(text, expected) for text, expected in EXTRA_ALIASES if expected is not None]
    native_rows += [dict(row_for(text, expected, annotated=False), default_language=language)
                    for language, text, expected in EXTRA_PATTERNS]
    native_rows += [dict(row_for(text, expected, annotated=False), unreadable=unreadable)
                    for text, unreadable, expected in EXTRA_CHARACTERS]
    native_rows.append(dict(row_for(*EXACT_ALIAS_WITH_DROP), unreadable="drop"))
    native_rows += [dict(row_for(text, expected, annotated=False), default_language="en-US")
                    for text, expected in EXTRA_EN_ALNUM]
    for row in native_rows:
        for tiny in (False, True):
            engine = Native(args, row, tiny)
            try:
                engine.freeze(); source = row["text"].encode(); batch = engine.normalize(source)
                assert batch["text"] == row["expected"], (row["id"], batch)
                schedules = [[1] * len(source), [len(source)]]
                schedules += [[split, len(source) - split] for split in range(1, len(source))]
                rng = random.Random(20000)
                schedules += [[rng.randint(1, 13) for _ in source] for _ in range(3)]
                for schedule in schedules:
                    assert engine.stream(source, schedule) == batch, (row["id"], tiny, schedule)
                    checks += 1
            finally:
                engine.close()
    row = row_for("k7", "케이세븐", annotated=False)
    engine = Native(args, row)
    try:
        for layer, reading in ((1, "시스템"), (2, "도메인"), (3, "케이세븐"), (4, "사용자")):
            assert engine.lib.ttf_lexicon_add(engine.handle, layer, b"en-US", b"ko-KR", b"k7", reading.encode()) == 0
        engine.freeze()
        assert engine.normalize(b"k7")["text"] == "사용자"
        assert engine.lib.ttf_engine_set_frontend_options(engine.handle, C.byref(engine.opts)) == 6
        checks += 2
    finally:
        engine.close()
    engine = Native(args, row_for("k7{커스텀}", "커스텀"))
    try:
        assert engine.lib.ttf_lexicon_add(engine.handle, 4, b"en-US", b"ko-KR", b"k7", "사용자".encode()) == 0
        engine.freeze()
        assert engine.normalize("k7{커스텀}".encode())["text"] == "커스텀"
        checks += 1
    finally:
        engine.close()
    checks += native_error_checks(args)
    return checks


def native_error_checks(args):
    """Fail closed, retain committed callbacks, and reject invalid ABI options."""
    checks = 0
    engine = Native(args, row_for("1{첫}"))
    try:
        setter = engine.lib.ttf_engine_set_frontend_options
        assert setter(None, C.byref(engine.opts)) == 1
        assert setter(engine.handle, None) == 1
        checks += 2
        for field, value in (("struct_size", 0), ("abi_version", 99), ("input_format", -1),
                             ("output_mode", 2), ("unreadable_policy", 2), ("max_control_bytes", 0)):
            saved = getattr(engine.opts, field)
            setattr(engine.opts, field, value)
            assert setter(engine.handle, C.byref(engine.opts)) == 1, field
            setattr(engine.opts, field, saved)
            checks += 1
        engine.freeze()
        assert engine.normalize("1{첫}".encode())["text"] == "첫"
        checks += 1
        for text in ERRORS + tuple(text for text, expected in EXTRA_ALIASES if expected is None):
            engine.normalize(text.encode(), 1)
            callbacks, handle = [], C.c_void_p()
            @engine.types.Callback
            def callback(pointer, _):
                segment = pointer.contents
                callbacks.append(C.string_at(segment.text, segment.length).decode())
                return 0
            assert engine.lib.ttf_stream_create(engine.handle, callback, None, C.byref(handle)) == 0
            try:
                error = 0
                for byte in text.encode():
                    error = engine.lib.ttf_stream_write(handle, bytes([byte]), 1)
                    if error:
                        break
                if not error:
                    error = engine.lib.ttf_stream_flush(handle)
                assert error == 1 and not callbacks, (text, error, callbacks)
                assert engine.lib.ttf_stream_write(handle, b"7", 1) == error
                assert engine.lib.ttf_stream_flush(handle) == error
                checks += 4
            finally:
                engine.lib.ttf_stream_destroy(handle)
        # The caller may synthesize earlier committed segments, but malformed
        # control bytes in a later unit are never delivered to its callback.
        callbacks, handle = [], C.c_void_p()
        @engine.types.Callback
        def callback(pointer, _):
            segment = pointer.contents
            callbacks.append(C.string_at(segment.text, segment.length).decode())
            return 0
        assert engine.lib.ttf_stream_create(engine.handle, callback, None, C.byref(handle)) == 0
        try:
            first = "안내입니다.\n".encode()
            assert engine.lib.ttf_stream_write(handle, first, len(first)) == 0
            assert callbacks and "".join(callbacks).startswith("안내입니다.")
            tail = "1{첫".encode()
            assert engine.lib.ttf_stream_write(handle, tail, len(tail)) == 0
            assert engine.lib.ttf_stream_flush(handle) == 1
            assert all("{" not in value and "첫" not in value for value in callbacks)
            assert engine.lib.ttf_stream_flush(handle) == 1
            checks += 5
        finally:
            engine.lib.ttf_stream_destroy(handle)
    finally:
        engine.close()
    # A bounded control fails at the same configured limit in batch and stream.
    engine = Native(args, row_for("1{가나다라마바사아자차카타파하}"))
    try:
        engine.opts.max_control_bytes = 12
        assert engine.lib.ttf_engine_set_frontend_options(engine.handle, C.byref(engine.opts)) == 0
        engine.freeze()
        engine.normalize("1{가나다라마바사아자차카타파하}".encode(), 7)
        callbacks, handle = [], C.c_void_p()
        @engine.types.Callback
        def callback(pointer, _):
            callbacks.append(pointer.contents.length)
            return 0
        assert engine.lib.ttf_stream_create(engine.handle, callback, None, C.byref(handle)) == 0
        try:
            error = 0
            for byte in "1{가나다라마바사아자차카타파하}".encode():
                error = engine.lib.ttf_stream_write(handle, bytes([byte]), 1)
                if error:
                    break
            if not error:
                error = engine.lib.ttf_stream_flush(handle)
            assert error == 7 and not callbacks, (error, callbacks)
            assert engine.lib.ttf_stream_write(handle, b"7", 1) == 7
            assert engine.lib.ttf_stream_flush(handle) == 7
            checks += 4
        finally:
            engine.lib.ttf_stream_destroy(handle)
    finally:
        engine.close()
    engine = Native(args, row_for("A{"))
    try:
        engine.opts.max_control_bytes = 4
        assert engine.lib.ttf_engine_set_frontend_options(engine.handle, C.byref(engine.opts)) == 0
        engine.freeze()
        # Limits count pending raw bytes, including an incomplete UTF-8 scalar,
        # and must fail on write rather than waiting for a closer or flush.
        for chunks, prior in (([b"A{", b"x", b"x", b"x"], False),
                              ([b"[[a", b"b", b"c"], False),
                              ([b"[[a", b"\xea", b"\xb0"], False),
                              ([b"A{", b"x", b"x", b"x"], True)):
            callbacks, handle = [], C.c_void_p()
            @engine.types.Callback
            def callback(pointer, _):
                item = pointer.contents
                callbacks.append(C.string_at(item.text, item.length).decode())
                return 0
            assert engine.lib.ttf_stream_create(engine.handle, callback, None, C.byref(handle)) == 0
            try:
                if prior:
                    first = "안내입니다.\n".encode()
                    assert engine.lib.ttf_stream_write(handle, first, len(first)) == 0
                    assert callbacks and "".join(callbacks).startswith("안내입니다.")
                saved = list(callbacks)
                for chunk in chunks[:-1]:
                    assert engine.lib.ttf_stream_write(handle, chunk, len(chunk)) == 0, chunks
                assert engine.lib.ttf_stream_write(handle, chunks[-1], len(chunks[-1])) == 7, chunks
                assert callbacks == saved, callbacks
                assert engine.lib.ttf_stream_write(handle, b"}", 1) == 7
                assert engine.lib.ttf_stream_flush(handle) == 7
                checks += 5
            finally:
                engine.lib.ttf_stream_destroy(handle)
        # A complete four-byte control is legal even with a much larger plain
        # remainder in this write or the write containing the closing brace.
        source = ("A{x}" + "가" * 100).encode()
        expected = engine.normalize(source)
        assert expected["text"] == "x" + "가" * 100
        for schedule in ([len(source)], [3, len(source) - 3]):
            assert engine.stream(source, schedule) == expected
            checks += 1
    finally:
        engine.close()
    # Old API users that do not opt in retain electronic-text preservation.
    engine = Native(args, row_for("a1@b2.com"), configure=False)
    try:
        engine.freeze()
        assert engine.normalize(b"a1@b2.com")["text"] == "a1@b2.com"
        checks += 1
    finally:
        engine.close()
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path); parser.add_argument("resources", type=Path)
    parser.add_argument("--library", type=Path); parser.add_argument("--native-only", action="store_true")
    args = parser.parse_args()
    args.cli, args.resources = args.cli.resolve(), args.resources.resolve()
    if args.library: args.library = args.library.resolve()
    if args.native_only and not args.library: parser.error("--native-only requires --library")
    cli_count = 0 if args.native_only else cli_checks(args)
    native_count = native_checks(args)
    print(f"user patterns: {cli_count} CLI contracts; {native_count} native split/ownership/override checks")


if __name__ == "__main__":
    main()
