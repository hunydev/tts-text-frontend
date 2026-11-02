"""Small ctypes adapter for ABI v1; all callback data are copied synchronously.

Run only in the isolated worker. No web request can select a library/resource path.
"""

import ctypes as C
import os
import tempfile
from pathlib import Path


def cmu_runtime():
    """The CMUdict runtime export loaded by english_dictionary=cmu (2026-10-03)."""
    explicit = os.environ.get("TTF_CMU_RUNTIME")
    if explicit:
        return Path(explicit)
    return (Path(__file__).resolve().parents[2] /
            "benchmark/generated/cmudict-runtime-full/pronunciation.tsv")

Z = C.c_size_t
I = C.c_int
P = C.c_void_p


class Semantic(C.Structure):
    _fields_ = [
        ("struct_size", C.c_uint32),
        ("type", I),
        ("lexical_class", I),
        ("input_begin", Z),
        ("input_end", Z),
        ("value", C.c_char * 96),
        ("secondary", C.c_char * 96),
        ("unit", C.c_char * 32),
        ("source_lang", C.c_char * 24),
        ("context_lang", C.c_char * 24),
        ("render_lang", C.c_char * 24),
        ("rule", C.c_char * 64),
        ("layer", I),
    ]


class Span(C.Structure):
    _fields_ = [
        ("begin", Z),
        ("end", Z),
        ("input_begin", Z),
        ("input_end", Z),
        ("script", I),
        ("semantic", Semantic),
        ("context_trace", C.c_char * 192),
        ("lexicon", C.c_char * 64),
        ("pronunciation_layer", I),
        ("verbalizer", C.c_char * 24),
    ]


class Result(C.Structure):
    _fields_ = [
        ("text", P),
        ("length", Z),
        ("spans", C.POINTER(Span)),
        ("span_count", Z),
    ]


class Config(C.Structure):
    _fields_ = [
        ("struct_size", C.c_uint32),
        ("abi_version", C.c_uint32),
        ("resource_root", C.c_char_p),
        ("default_language", C.c_char_p),
        ("render_policy", I),
        ("first_segment_target", Z),
        ("normal_segment_target", Z),
        ("max_segment_length", Z),
        ("max_pending_bytes", Z),
        ("sentence_split", I),
        ("phrase_split", I),
    ]


class FrontendOptions(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("abi_version", C.c_uint32),
                ("input_format", I), ("output_mode", I), ("unreadable_policy", I),
                ("max_control_bytes", Z), ("hanja", I), ("kana", I)]


class ResourceDiagnostic(C.Structure):
    _fields_ = [
        ("struct_size", C.c_uint32), ("layer", I), ("line", Z),
        ("path", C.c_char * 512), ("kind", C.c_char * 64),
        ("language", C.c_char * 24), ("key", C.c_char * 256),
        ("message", C.c_char * 256),
    ]


class ResourceValidationReport(C.Structure):
    _fields_ = [
        ("struct_size", C.c_uint32), ("records", Z),
        ("validated_records", Z), ("unvalidated_records", Z),
    ]


class Segment(C.Structure):
    _fields_ = Result._fields_ + [("input_begin", Z), ("input_end", Z)]


class TraceOptions(C.Structure):
    _fields_ = [("struct_size", C.c_uint32), ("max_events", Z), ("max_bytes", Z)]


class TraceEvent(C.Structure):
    _fields_ = [
        ("struct_size", C.c_uint32), ("stage", I), ("disposition", I),
        ("input_begin", Z), ("input_end", Z), ("output_begin", Z), ("output_end", Z),
        ("part_begin", Z), ("part_end", Z), ("phone_index", Z),
        ("layer", I), ("layer_known", I), ("declaration_order", Z),
    ] + [(key, C.c_char_p) for key in
         ("language", "resource_language", "kind", "key", "value", "text", "detail")]


class TracedResult(C.Structure):
    _fields_ = [("result", Result), ("events", C.POINTER(TraceEvent)), ("event_count", Z)]


class TracedSegment(C.Structure):
    _fields_ = [("segment", Segment), ("events", C.POINTER(TraceEvent)), ("event_count", Z)]


TracedCallback = C.CFUNCTYPE(I, C.POINTER(TracedSegment), P)


class Token(C.Structure):
    _fields_ = [("begin", Z), ("end", Z), ("script", I)]


Callback = C.CFUNCTYPE(I, C.POINTER(Segment), P)


def string(value):
    return value.decode("utf-8")


def named_dictionaries(text):
    """Split `[NAME]` sections; the rows under each header are that dictionary's TSV."""
    sections = []
    for line in text.splitlines():
        if line.startswith("[") and line.rstrip().endswith("]"):
            sections.append([line.strip()[1:-1].strip(), []])
        elif line.strip() and not line.lstrip().startswith("#"):
            if not sections:
                raise ValueError("이름 있는 사전: 첫 행 앞에 [이름] 줄이 필요합니다.")
            sections[-1][1].append(line)
        elif sections:
            sections[-1][1].append(line)
    return [(name, "\n".join(rows) + "\n") for name, rows in sections]


class Engine:
    def __init__(self, library, resources, options, overrides=()):
        self.lib = C.CDLL(str(library))
        definitions = {
            "ttf_config_init": (None, [C.POINTER(Config)]),
            "ttf_engine_freeze": (I, [P]),
            "ttf_engine_destroy": (None, [P]),
            "ttf_normalize": (I, [P, C.c_char_p, Z, C.POINTER(Result)]),
            "ttf_result_free": (None, [C.POINTER(Result)]),
            "ttf_lexicon_add": (
                I,
                [P, I, C.c_char_p, C.c_char_p, C.c_char_p, C.c_char_p],
            ),
            "ttf_stream_create": (I, [P, Callback, P, C.POINTER(P)]),
            "ttf_stream_write": (I, [P, C.c_char_p, Z]),
            "ttf_stream_flush": (I, [P]),
            "ttf_stream_destroy": (None, [P]),
            "ttf_tokenize": (
                I,
                [C.c_char_p, Z, C.POINTER(C.POINTER(Token)), C.POINTER(Z)],
            ),
            "ttf_tokens_free": (None, [C.POINTER(Token)]),
            "ttf_status_string": (C.c_char_p, [I]),
            "ttf_script_name": (C.c_char_p, [I]),
            "ttf_semantic_type_name": (C.c_char_p, [I]),
            "ttf_layer_name": (C.c_char_p, [I]),
            # Resource validation, frontend options, traces and dictionaries: every library
            # built since 2026-10-27 has them; older snapshots are no longer loaded.
            "ttf_resource_diagnostic_init": (None, [C.POINTER(ResourceDiagnostic)]),
            "ttf_resource_validation_report_init": (None, [C.POINTER(ResourceValidationReport)]),
            "ttf_engine_create_ex": (I, [C.POINTER(Config), C.POINTER(P), C.POINTER(ResourceDiagnostic)]),
            "ttf_resource_load_ex": (I, [P, C.c_char_p, I, C.POINTER(ResourceDiagnostic)]),
            "ttf_engine_validate_resources": (I, [P, C.POINTER(ResourceValidationReport), C.POINTER(ResourceDiagnostic)]),
            "ttf_frontend_options_init_sized": (None, [C.POINTER(FrontendOptions), Z]),
            "ttf_engine_set_frontend_options": (I, [P, C.POINTER(FrontendOptions)]),
            "ttf_engine_set_option": (I, [P, C.c_char_p, C.c_char_p]),
            "ttf_trace_options_init": (None, [C.POINTER(TraceOptions)]),
            "ttf_normalize_traced": (I, [P, C.c_char_p, Z, C.POINTER(TraceOptions), C.POINTER(TracedResult)]),
            "ttf_traced_result_free": (None, [C.POINTER(TracedResult)]),
            "ttf_stream_create_traced": (I, [P, C.POINTER(TraceOptions), TracedCallback, P, C.POINTER(P)]),
            "ttf_trace_stage_name": (C.c_char_p, [I]),
            "ttf_trace_disposition_name": (C.c_char_p, [I]),
            "ttf_dict_create": (I, [C.POINTER(P)]),
            "ttf_dict_destroy": (None, [P]),
            "ttf_dict_load": (I, [P, C.c_char_p]),
            "ttf_dict_size": (Z, [P]),
            "ttf_normalize_with": (I, [P, P, C.c_char_p, Z, C.POINTER(Result)]),
            "ttf_normalize_traced_with": (
                I, [P, P, C.c_char_p, Z, C.POINTER(TraceOptions), C.POINTER(TracedResult)]),
            "ttf_stream_set_dict": (I, [P, P]),
            "ttf_engine_add_dictionary": (I, [P, C.c_char_p, P]),
        }
        missing = [name for name in definitions if not hasattr(self.lib, name)]
        if missing:
            raise ValueError("This native library predates the workbench ABI (missing "
                             + ", ".join(missing[:3]) + "); rebuild it from this checkout")
        self.detailed_trace = bool(options.get("detailed_trace", False))
        self.resource_validation = None
        for name, (result, args) in definitions.items():
            fn = getattr(self.lib, name)
            fn.restype = result
            fn.argtypes = args
        self.handle = P()
        self.dict = P()
        cfg = Config()
        self.lib.ttf_config_init(C.byref(cfg))
        cfg.resource_root = str(resources).encode()
        cfg.default_language = options.get("default_language", "ko-KR").encode()
        cfg.render_policy = {"preserve": 0, "phonetic": 1, "semantic": 2}[
            options.get("render_policy", "phonetic")
        ]
        for key, field in [
            ("first_target", "first_segment_target"),
            ("normal_target", "normal_segment_target"),
            ("max_length", "max_segment_length"),
            ("max_pending_bytes", "max_pending_bytes"),
            ("sentence_split", "sentence_split"),
            ("phrase_split", "phrase_split"),
        ]:
            if key in options:
                setattr(cfg, field, options[key])
        diagnostic = self.diagnostic()
        self.check(self.lib.ttf_engine_create_ex(C.byref(cfg), C.byref(self.handle),
                                                 C.byref(diagnostic)), diagnostic)
        try:
            frontend = FrontendOptions()
            self.lib.ttf_frontend_options_init_sized(C.byref(frontend), C.sizeof(frontend))
            frontend.hanja = {"passthrough": 0, "korean": 1}[options.get("hanja", "passthrough")]
            frontend.kana = {"passthrough": 0, "korean": 1}[options.get("kana", "passthrough")]
            frontend.input_format = {"plain": 0, "annotated": 1}[options.get("input_format", "plain")]
            frontend.output_mode = {"text": 0, "speech": 1}[options.get("output_mode", "text")]
            frontend.unreadable_policy = {"preserve": 0, "drop": 1, "read": 2}[
                options.get("unreadable", "preserve")]
            frontend.max_control_bytes = options.get("max_control_bytes", 4096)
            self.check(self.lib.ttf_engine_set_frontend_options(self.handle, C.byref(frontend)))
            for name, default in (("brackets", "read"), ("unknown_foreign", "guess"),
                                  ("decorative", "follow"), ("initialism", "spell"),
                                  ("spacing", "orthographic"), ("matrix", "auto"),
                                  ("foreign_sentence", "keep"),
                                  ("symbol_fallback", "on"), ("invalid_control", "ignore"), ("numeric_fallback", "read"),
                                  ("particle_agreement", "off"), ("pronunciation", "orthographic")):
                if options.get(name, default) != default:
                    self.check(self.lib.ttf_engine_set_option(
                        self.handle, name.encode(), options[name].encode()))
            if options.get("domain") in ("finance", "finance-vocabulary"):
                self.load(resources / "domains/finance/ko-KR/lexicon.tsv", 2)
            if options.get("domain") == "finance":
                self.load(resources / "domains/finance/ko-KR/rules.tsv", 2)
            if options.get("brand_lexicon") == "on":
                self.load(resources / "domains/brands/ko-KR/lexicon.tsv", 2)
            # auto: on when an English sentence is read in Korean letters (foreign_sentence=localize,
            # 2026-10-28), off otherwise; a missing export then leaves the loanword readings alone.
            cmu = options.get("english_dictionary", "auto")
            if cmu == "auto" and options.get("foreign_sentence") == "localize" and cmu_runtime().is_file():
                cmu = "cmu"
            if cmu == "cmu":
                # Optional CMUdict pronunciations (BSD-style, not in Git): the importer's local
                # runtime export, or TTF_CMU_RUNTIME. Authored lexicons still win over it.
                path = cmu_runtime()
                if not path.is_file():
                    raise ValueError("CMU pronunciation export not found: " + str(path) +
                                     " (see benchmark/importers/cmudict/README.md)")
                self.load(path, 2)
            for path, layer in overrides:
                self.load(path, layer)
            if options.get("customer_tsv"):
                with tempfile.TemporaryDirectory(
                    prefix="ttf-web-resource-"
                ) as directory:
                    path = Path(directory) / "customer.tsv"
                    path.write_text(options["customer_tsv"], encoding="utf-8")
                    self.load(path, 3)
            for row in options.get("user_lexicon", []):
                self.check(
                    self.lib.ttf_lexicon_add(
                        self.handle,
                        4,
                        b"en-US",
                        b"ko-KR",
                        row["word"].encode(),
                        row["pronunciation"].encode(),
                    )
                )
            diagnostic = self.diagnostic()
            report = ResourceValidationReport()
            self.lib.ttf_resource_validation_report_init(C.byref(report))
            self.check(self.lib.ttf_engine_validate_resources(self.handle,
                       C.byref(report), C.byref(diagnostic)), diagnostic)
            self.resource_validation = {key: int(getattr(report, key)) for key in
                ("records", "validated_records", "unvalidated_records")}
            for name, rows in named_dictionaries(options.get("named_dict_tsv", "")):
                self.add_dictionary(name, rows)
            self.check(self.lib.ttf_engine_freeze(self.handle))
            if options.get("request_dict_tsv", "").strip():
                # A dictionary of this run on the frozen engine (docs/request-dictionary.ko.md).
                self.check(self.lib.ttf_dict_create(C.byref(self.dict)))
                with tempfile.TemporaryDirectory(prefix="ttf-web-request-") as directory:
                    path = Path(directory) / "request.tsv"
                    path.write_text(options["request_dict_tsv"], encoding="utf-8")
                    status = self.lib.ttf_dict_load(self.dict, str(path).encode())
                if status:
                    raise ValueError("요청 사전: " + string(self.lib.ttf_status_string(status)) +
                                     " (exact·lexicon 행만, 탭으로 나눈 4칸)")
        except BaseException:
            self.close()
            raise

    def add_dictionary(self, name, rows):
        # The engine owns a dictionary once it is added; until then it is ours to destroy.
        dictionary = P()
        self.check(self.lib.ttf_dict_create(C.byref(dictionary)))
        try:
            with tempfile.TemporaryDirectory(prefix="ttf-web-named-") as directory:
                path = Path(directory) / "named.tsv"
                path.write_text(rows, encoding="utf-8")
                status = self.lib.ttf_dict_load(dictionary, str(path).encode())
            if status:
                raise ValueError(f"이름 있는 사전 [{name}]: " +
                                 string(self.lib.ttf_status_string(status)) +
                                 " (exact·lexicon 행만, 탭으로 나눈 4칸)")
            if self.lib.ttf_engine_add_dictionary(self.handle, name.encode(), dictionary):
                raise ValueError(f"이름 있는 사전 [{name}]: 이름은 영문·숫자·_·- 64자 이내, 한 번씩")
        except BaseException:
            self.lib.ttf_dict_destroy(dictionary)
            raise

    def diagnostic(self):
        diagnostic = ResourceDiagnostic()
        self.lib.ttf_resource_diagnostic_init(C.byref(diagnostic))
        return diagnostic

    def check(self, status, diagnostic=None):
        if status:
            message = "Native frontend: " + string(self.lib.ttf_status_string(status))
            if diagnostic is not None:
                location = string(diagnostic.path) or "effective resources"
                # Worker paths are temporary implementation details; file and line
                # identify the editable resource without exposing VM directories.
                location = Path(location).name
                message += f" · {location}:{diagnostic.line}"
                identity = "/".join(string(getattr(diagnostic, key))
                                    for key in ("kind", "language", "key"))
                message += f" [{identity}] {string(diagnostic.message)}"
            raise ValueError(message)

    def load(self, path, layer):
        diagnostic = self.diagnostic()
        self.check(self.lib.ttf_resource_load_ex(self.handle, str(path).encode(),
                                                 layer, C.byref(diagnostic)), diagnostic)

    def close(self):
        if self.dict:
            self.lib.ttf_dict_destroy(self.dict)
            self.dict = P()
        if self.handle:
            self.lib.ttf_engine_destroy(self.handle)
            self.handle = P()

    def spans(self, spans, count, source, output):
        result = []
        for i in range(count):
            span = spans[i]
            sem = span.semantic
            row = {
                k: int(getattr(span, k))
                for k in ["begin", "end", "input_begin", "input_end"]
            }
            row.update(
                script=string(self.lib.ttf_script_name(span.script)),
                semantic_type=string(self.lib.ttf_semantic_type_name(sem.type)),
                resource_layer=string(self.lib.ttf_layer_name(sem.layer)),
                pronunciation_layer=string(
                    self.lib.ttf_layer_name(span.pronunciation_layer)
                ),
                surface=source[span.input_begin : span.input_end].decode(),
                normalized=output[span.begin : span.end].decode(),
            )
            for k in [
                "source_lang",
                "context_lang",
                "render_lang",
                "value",
                "secondary",
                "unit",
                "rule",
            ]:
                row[k] = string(getattr(sem, k))
            for k in ["context_trace", "lexicon", "verbalizer"]:
                row[k] = string(getattr(span, k))
            result.append(row)
        return result

    def trace_events(self, events, count):
        rows = []
        for index in range(count):
            event = events[index]
            row = {key: int(getattr(event, key)) for key in
                   ("input_begin", "input_end", "output_begin", "output_end")}
            for key in ("part_begin", "part_end", "phone_index", "declaration_order"):
                value = int(getattr(event, key))
                row[key] = None if value == Z(-1).value else value
            row.update(stage=string(self.lib.ttf_trace_stage_name(event.stage)),
                       disposition=string(self.lib.ttf_trace_disposition_name(event.disposition)),
                       layer_known=bool(event.layer_known),
                       layer=string(self.lib.ttf_layer_name(event.layer)) if event.layer_known else "unknown")
            for key in ("language", "resource_language", "kind", "key", "value", "text", "detail"):
                row[key] = string(getattr(event, key) or b"")
            rows.append(row)
        return rows

    def normalize(self, source):
        result = Result()
        traced = TracedResult()
        if self.detailed_trace:
            if self.dict:
                self.check(self.lib.ttf_normalize_traced_with(self.handle, self.dict, source,
                                                              len(source), None, C.byref(traced)))
            else:
                self.check(self.lib.ttf_normalize_traced(self.handle, source, len(source),
                                                        None, C.byref(traced)))
            result = traced.result
        else:
            if self.dict:
                self.check(self.lib.ttf_normalize_with(self.handle, self.dict, source, len(source),
                                                       C.byref(result)))
            else:
                self.check(self.lib.ttf_normalize(self.handle, source, len(source), C.byref(result)))
        try:
            output = C.string_at(result.text, result.length)
            row = {
                "text": output.decode(),
                "spans": self.spans(result.spans, result.span_count, source, output),
            }
            if self.detailed_trace:
                row["trace_version"] = 1
                row["trace"] = self.trace_events(traced.events, traced.event_count)
            return row
        finally:
            if self.detailed_trace:
                self.lib.ttf_traced_result_free(C.byref(traced))
            else:
                self.lib.ttf_result_free(C.byref(result))

    def tokens(self, source):
        tokens = C.POINTER(Token)()
        count = Z()
        self.check(
            self.lib.ttf_tokenize(source, len(source), C.byref(tokens), C.byref(count))
        )
        try:
            return [
                dict(
                    begin=tokens[i].begin,
                    end=tokens[i].end,
                    script=string(self.lib.ttf_script_name(tokens[i].script)),
                    surface=source[tokens[i].begin : tokens[i].end].decode(),
                )
                for i in range(count.value)
            ]
        finally:
            self.lib.ttf_tokens_free(tokens)

    def stream(self, source, sizes, emit=lambda event: None, delay_ms=0):
        import time

        started = time.monotonic()
        segments = []
        errors = []
        stream = P()
        stage = "write"
        write_index = 0

        def now():
            return round((time.monotonic() - started) * 1000, 3)

        def collect(pointer, _):
            try:
                detail = pointer.contents
                seg = detail.segment if self.detailed_trace else detail
                output = C.string_at(seg.text, seg.length)
                row = dict(
                    sequence=len(segments),
                    input_begin=seg.input_begin,
                    input_end=seg.input_end,
                    text=output.decode(),
                    spans=self.spans(seg.spans, seg.span_count, source, output),
                )
                if self.detailed_trace:
                    row["trace_version"] = 1
                    row["trace"] = self.trace_events(detail.events, detail.event_count)
                segments.append(row)
                emit(
                    dict(
                        row,
                        event="segment",
                        elapsed_ms=now(),
                        stage=stage,
                        write_index=write_index,
                    )
                )
                return 0
            except BaseException as exc:
                errors.append(exc)
                return 1

        callback = (TracedCallback if self.detailed_trace else Callback)(collect)
        if self.detailed_trace:
            self.check(self.lib.ttf_stream_create_traced(self.handle, None, callback, None, C.byref(stream)))
        else:
            self.check(self.lib.ttf_stream_create(self.handle, callback, None, C.byref(stream)))
        if self.dict:
            self.check(self.lib.ttf_stream_set_dict(stream, self.dict))
        try:
            at = 0
            for write_index, size in enumerate(sizes):
                if at >= len(source):
                    break
                end = min(at + size, len(source))
                emit(
                    dict(
                        event="chunk",
                        sequence=write_index,
                        begin=at,
                        end=end,
                        elapsed_ms=now(),
                    )
                )
                self.check(self.lib.ttf_stream_write(stream, source[at:end], end - at))
                at = end
                if delay_ms and at < len(source):
                    time.sleep(delay_ms / 1000)
            if at != len(source):
                raise ValueError("Incomplete test chunk schedule")
            stage = "flush"
            self.check(self.lib.ttf_stream_flush(stream))
            if errors:
                raise errors[0]
            return segments
        finally:
            self.lib.ttf_stream_destroy(stream)


def flatten(segments):
    offset = 0
    spans = []
    trace = []
    for seg in segments:
        for row in seg["spans"]:
            spans.append(
                dict(row, begin=row["begin"] + offset, end=row["end"] + offset)
            )
        for event in seg.get("trace", []):
            trace.append(dict(event, output_begin=event["output_begin"] + offset,
                              output_end=event["output_end"] + offset))
        offset += len(seg["text"].encode())
    result = dict(text="".join(s["text"] for s in segments), spans=spans)
    if any("trace" in seg for seg in segments):
        result.update(trace_version=1, trace=trace)
    return result
