#!/usr/bin/env python3
"""Private development workbench: stdlib HTTP transport + isolated native workers."""

import argparse
import json
import mimetypes
import subprocess
import sys
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit
from workbench import ROOT, catalog, validate
import ko_reports
import en_reports
import jev
import jev_eval

sys.path.insert(0, str(ROOT / "tools/lexicon"))
from manage import inventory

STATIC = Path(__file__).with_name("static")
SLOTS = threading.BoundedSemaphore(4)
MAX_BODY = 262144
MAX_OUTPUT = 32 * 1024 * 1024


class Handler(BaseHTTPRequestHandler):
    server_version = "TTFWorkbench/1"

    def setup(self):
        super().setup()
        self.connection.settimeout(15)

    def log_message(self, *args):
        pass  # Input text is never written to access logs.

    def response_headers(self, status, kind):
        self.send_response(status)
        self.send_header("Content-Type", kind)
        self.send_header("Cache-Control", "no-store, no-transform")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "same-origin")
        self.send_header(
            "Content-Security-Policy",
            "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'",
        )

    def json(self, status, value):
        raw = json.dumps(value, ensure_ascii=False).encode()
        self.response_headers(status, "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        parsed = urlsplit(self.path)
        path = parsed.path
        if path in ("/api/ko-reports", "/api/ko-report", "/api/ko-cases", "/api/ko-spacing"):
            try:
                root = getattr(self.server, "ko_report_root", ko_reports.DEFAULT_ROOT)
                args = ko_reports.arguments(parsed.query)
                if path == "/api/ko-reports":
                    result = ko_reports.catalog(root)
                elif path == "/api/ko-report":
                    result = ko_reports.summary(root, args.get("run"))
                elif path == "/api/ko-spacing":
                    result = ko_reports.spacing(root, args.get("run"))
                else:
                    result = ko_reports.cases(root, args)
                return self.json(200, result)
            except (ValueError, TypeError, KeyError):
                return self.json(400, {"error": "보고서나 검색 조건을 확인하세요."})
            except (OSError, ko_reports.sqlite3.Error):
                return self.json(503, {"error": "보고서를 읽을 수 없습니다."})
        if path in ("/api/en-reports", "/api/en-report", "/api/en-cases"):
            try:
                root = getattr(self.server, "en_report_root", en_reports.DEFAULT_ROOT)
                args = en_reports.arguments(parsed.query)
                if path == "/api/en-reports":
                    result = en_reports.catalog(root)
                elif path == "/api/en-report":
                    result = en_reports.summary(root, args.get("run"))
                else:
                    result = en_reports.cases(root, args)
                return self.json(200, result)
            except (ValueError, TypeError, KeyError):
                return self.json(400, {"error": "보고서나 검색 조건을 확인하세요."})
            except OSError:
                return self.json(503, {"error": "보고서를 읽을 수 없습니다."})
        if path.startswith("/api/jev-eval/"):
            from urllib.parse import parse_qs
            args = {k: v[0] for k, v in parse_qs(parsed.query).items()}
            root = getattr(self.server, "jev_eval_root", jev_eval.DEFAULT_ROOT)
            try:
                if path == "/api/jev-eval/sets":
                    return self.json(200, jev_eval.catalog(root))
                if path == "/api/jev-eval/set":
                    meta, rows = jev_eval.sentences(root, args.get("set"))
                    return self.json(200, dict(meta=meta, rows=rows))
                if path == "/api/jev-eval/systems":
                    return self.json(200, jev_eval.systems(root, args.get("set")))
                if path == "/api/jev-eval/system":
                    return self.json(200, jev_eval.load_system(root, args.get("set"), args.get("system")))
            except jev_eval.EvalError as exc:
                return self.json(exc.status, {"error": str(exc)})
            return self.json(404, {"error": "Not found"})
        if path == "/api/catalog":
            return self.json(200, catalog())
        if path == "/api/health":
            return self.json(200, dict(ok=True, version=catalog()["version"]))
        if path == "/api/lexicon":
            try:
                return self.json(200, inventory(ROOT))
            except (ValueError, OSError) as error:
                return self.json(503, {"error": str(error)})
        names = {
            "/": "index.html",
            "/app.js": "app.js",
            "/style.css": "style.css",
            "/ko-quality": "ko-quality.html",
            "/ko-quality.js": "ko-quality.js",
            "/ko-quality.css": "ko-quality.css",
            "/en-quality": "en-quality.html",
            "/en-quality.js": "en-quality.js",
            "/lexicon": "lexicon.html",
            "/lexicon.js": "lexicon.js",
            "/jev-eval": "jev-eval.html",
            "/perf": "perf.html",
            "/perf.js": "perf.js",
            "/jev-eval.js": "jev-eval.js",
        }
        if path not in names:
            return self.json(404, {"error": "Not found"})
        file = STATIC / names[path]
        if not file.is_file():
            return self.json(503, {"error": "UI is not installed"})
        raw = file.read_bytes()
        kind = mimetypes.guess_type(file)[0] or "application/octet-stream"
        self.response_headers(200, kind + "; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def do_POST(self):
        path = urlsplit(self.path).path
        if path not in ("/api/run", "/api/corpus", "/api/judge", "/api/jev-eval/save", "/api/jev-eval/fill",
                        "/api/jev-eval/score"):
            return self.json(404, {"error": "Not found"})
        if (
            self.headers.get("Content-Type", "").split(";")[0].strip()
            != "application/json"
        ):
            return self.json(415, {"error": "application/json 요청이 필요합니다."})
        origin = self.headers.get("Origin")
        host = self.headers.get("X-Forwarded-Host", self.headers.get("Host", ""))
        if origin and urlsplit(origin).netloc != host:
            return self.json(403, {"error": "다른 출처의 요청은 허용되지 않습니다."})
        if path == "/api/judge":
            return self.judge()
        if path.startswith("/api/jev-eval/"):
            return self.jev_eval(path)
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= MAX_BODY:
                raise ValueError("요청 본문은 256 KiB 이내여야 합니다.")
            raw = self.rfile.read(length)
            if len(raw) != length:
                raise ValueError("요청 본문이 완전하지 않습니다.")
            request = validate(json.loads(raw), corpus=path == "/api/corpus")
        except (ValueError, TypeError, UnicodeError) as exc:
            return self.json(400, {"error": str(exc)})
        if not SLOTS.acquire(blocking=False):
            return self.json(
                429,
                {
                    "error": "동시에 최대 4개까지 실행할 수 있습니다. 잠시 후 다시 시도하세요."
                },
            )
        process = None
        timer = None
        try:
            process = subprocess.Popen(
                [
                    sys.executable,
                    "-u",
                    str(Path(__file__).with_name("worker.py")),
                    str(self.server.library),
                    str(self.server.resources),
                ],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
            )
            timer = threading.Timer(45, process.kill)
            timer.daemon = True
            timer.start()
            process.stdin.write(json.dumps(request, ensure_ascii=False).encode())
            process.stdin.close()
            self.response_headers(200, "application/x-ndjson; charset=utf-8")
            self.send_header("X-Accel-Buffering", "no")
            self.end_headers()
            ended = False
            failed = False
            total = 0
            while True:
                line = process.stdout.readline(MAX_OUTPUT - total + 1)
                if not line:
                    break
                total += len(line)
                if total > MAX_OUTPUT:
                    raise ValueError("결과 크기 제한을 초과했습니다.")
                event = json.loads(line)
                ended |= event.get("event") == "end"
                failed |= event.get("event") == "error"
                self.wfile.write(line)
                self.wfile.flush()
            code = process.wait()
            if not failed and (code or not ended):
                self.wfile.write(
                    json.dumps(
                        dict(
                            event="error",
                            message="실행 제한 시간 초과 또는 worker 종료. 입력을 줄여 다시 시도하세요.",
                        ),
                        ensure_ascii=False,
                    ).encode()
                    + b"\n"
                )
                self.wfile.flush()
        except (BrokenPipeError, ConnectionResetError, TimeoutError):
            pass
        except Exception:
            if process:
                try:
                    self.wfile.write(
                        b'{"event":"error","message":"Worker transport failed"}\n'
                    )
                    self.wfile.flush()
                except OSError:
                    pass
            else:
                self.json(503, {"error": "Worker를 시작하지 못했습니다."})
        finally:
            if timer:
                timer.cancel()
            if process:
                if process.poll() is None:
                    process.kill()
                process.wait()
                if process.stdout:
                    process.stdout.close()
                if process.stdin and not process.stdin.closed:
                    process.stdin.close()
            SLOTS.release()


    def judge(self):
        """Score one normalization result with Jev (tools/web/jev.py)."""
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 65536:
                raise ValueError("요청 본문은 64 KiB 이내여야 합니다.")
            body = json.loads(self.rfile.read(length))
            if not isinstance(body, dict):
                raise ValueError("JSON 객체가 필요합니다.")
        except (ValueError, UnicodeError):
            return self.json(400, {"error": "Jev 채점 요청을 확인하세요."})
        client = self.headers.get("X-Forwarded-For", self.client_address[0]).split(",")[0].strip()
        judge = getattr(self.server, "judge", None) or jev.Judge()
        self.server.judge = judge
        try:
            result = judge.score(body.get("text"), body.get("normalized"), body.get("language"), client,
                                 speech=body.get("output_mode", "speech") == "speech")
        except jev.JudgeError as exc:
            return self.json(exc.status, {"error": str(exc)})
        return self.json(200, result)


    def jev_eval(self, path):
        """Save TN outputs, fill them with this engine, or score them with Jev."""
        root = getattr(self.server, "jev_eval_root", jev_eval.DEFAULT_ROOT)
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if not 0 < length <= 16 * 1024 * 1024:
                raise ValueError
            body = json.loads(self.rfile.read(length))
            if not isinstance(body, dict):
                raise ValueError
        except (ValueError, UnicodeError):
            return self.json(400, {"error": "요청 본문(JSON, 16 MiB 이내)을 확인하세요."})
        try:
            if path == "/api/jev-eval/save":
                return self.json(200, jev_eval.save(root, body.get("set"), body.get("system"), body.get("outputs")))
            if path == "/api/jev-eval/fill":
                jev_eval.sentences(root, body.get("set"))
                if not SLOTS.acquire(blocking=False):
                    return self.json(429, {"error": "실행이 많습니다. 잠시 후 다시 시도하세요."})
                try:
                    done = subprocess.run(
                        [sys.executable, str(Path(__file__).with_name("jev_eval.py")), "normalize",
                         str(self.server.library), str(self.server.resources),
                         str(jev_eval.set_dir(root, body["set"]) / "sentences.jsonl")],
                        capture_output=True, timeout=600)
                finally:
                    SLOTS.release()
                if done.returncode != 0:
                    return self.json(500, {"error": "엔진 실행에 실패했습니다."})
                return self.json(200, dict(outputs=json.loads(done.stdout)))
            # score: stream progress lines, then the summary
            self.response_headers(200, "application/x-ndjson; charset=utf-8")
            self.end_headers()
            lock = threading.Lock()

            def emit(value):
                with lock:
                    self.wfile.write((json.dumps(value, ensure_ascii=False) + "\n").encode())
                    self.wfile.flush()

            try:
                result = jev_eval.score(root, body.get("set"), body.get("system"),
                                        progress=lambda d, n: emit(dict(event="progress", done=d, total=n)),
                                        batch=getattr(self.server, "jev_batch", None),
                                        ids=body.get("ids"), patch=body.get("patch"))
                emit(dict(event="result", **result))
            except jev_eval.EvalError as exc:
                emit(dict(event="error", status=exc.status, message=str(exc)))
            return None
        except jev_eval.EvalError as exc:
            return self.json(exc.status, {"error": str(exc)})
        except subprocess.TimeoutExpired:
            return self.json(504, {"error": "엔진 실행 시간이 초과되었습니다."})


class Server(ThreadingHTTPServer):
    daemon_threads = True


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--host", default="127.0.0.1")
    p.add_argument("--port", type=int, default=8000)
    p.add_argument(
        "--library", type=Path, default=ROOT / "build/libtts_text_frontend.so"
    )
    p.add_argument("--resources", type=Path, default=ROOT / "resources")
    p.add_argument("--ko-report-root", type=Path, default=ko_reports.DEFAULT_ROOT)
    p.add_argument("--en-report-root", type=Path, default=en_reports.DEFAULT_ROOT)
    args = p.parse_args()
    if not args.library.is_file() or not (args.resources / "manifest.ttfm").is_file():
        p.error("Build the shared library and supply an existing resource directory.")
    server = Server((args.host, args.port), Handler)
    server.ko_report_root = args.ko_report_root.resolve()
    server.en_report_root = args.en_report_root.resolve()
    server.library = args.library.resolve()
    server.resources = args.resources.resolve()
    print(f"TTF workbench listening on {args.host}:{server.server_port}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
