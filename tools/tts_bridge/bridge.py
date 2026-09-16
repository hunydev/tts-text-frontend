#!/usr/bin/env python3
"""Consume version 1 frontend JSONL, route by render language, and synthesize WAVs.

This optional reference adapter runs an external speech engine. The C library
has no dependency on Python or eSpeak. Audio is produced one language run at a
time, with synchronous processing providing bounded queueing/backpressure.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
import re
from pathlib import Path
import subprocess
import sys
import time
from typing import Iterable, Protocol
import wave


@dataclass(frozen=True)
class SpeechJob:
    sequence: int
    segment_sequence: int
    language: str
    text: str
    input_begin: int
    input_end: int


class Backend(Protocol):
    name: str

    def synthesize(self, job: SpeechJob, output: Path) -> None: ...


def integer(record: dict, key: str) -> int:
    value = record.get(key)
    if type(value) is not int or value < 0:
        raise ValueError(f"{key} must be a nonnegative integer")
    return value


def speech_jobs(lines: Iterable[str]) -> Iterable[SpeechJob]:
    """Validate coverage/ordering and coalesce adjacent equal-render-language spans.

    Byte offsets are validated against UTF-8 bytes, never Python character indexes.
    Only render_lang selects the acoustic voice; source_lang is provenance.
    """
    expected_sequence = 0
    expected_input = 0
    job_sequence = 0
    ended = False
    for line_number, line in enumerate(lines, 1):
        try:
            event = json.loads(line)
        except (ValueError, UnicodeError) as error:
            raise ValueError(f"line {line_number}: invalid JSON") from error
        if not isinstance(event, dict) or type(event.get("version")) is not int or event["version"] != 1:
            raise ValueError(f"line {line_number}: unsupported event version")
        if ended:
            raise ValueError("event after end")
        if event.get("event") == "end":
            if integer(event, "segment_count") != expected_sequence or integer(event, "input_bytes") != expected_input:
                raise ValueError("end event does not match committed input")
            ended = True
            continue
        if event.get("event") != "segment":
            raise ValueError("unknown event type")
        if integer(event, "sequence") != expected_sequence:
            raise ValueError("segment sequence is not contiguous")
        begin, end = integer(event, "input_begin"), integer(event, "input_end")
        if begin != expected_input or end <= begin:
            raise ValueError("segment input coverage is not contiguous")
        text, spans = event.get("text"), event.get("spans")
        if not isinstance(text, str) or not isinstance(spans, list) or not spans:
            raise ValueError("segment must contain text and nonempty spans")
        raw = text.encode("utf-8")
        output_cursor, input_cursor = 0, begin
        runs: list[tuple[str, str, int, int]] = []
        for span in spans:
            if not isinstance(span, dict):
                raise ValueError("invalid span")
            a, b = integer(span, "begin"), integer(span, "end")
            ia, ib = integer(span, "input_begin"), integer(span, "input_end")
            language = span.get("render_lang")
            if not isinstance(language, str) or not language or len(language) >= 24:
                raise ValueError("missing or invalid render language")
            if a != output_cursor or b < a or b > len(raw) or ia != input_cursor or ib <= ia or ib > end:
                raise ValueError("invalid span coverage")
            # Decoding each slice also validates that offsets are codepoint boundaries.
            piece = raw[a:b].decode("utf-8")
            if runs and runs[-1][0] == language:
                previous = runs[-1]
                runs[-1] = (language, previous[1] + piece, previous[2], ib)
            else:
                runs.append((language, piece, ia, ib))
            output_cursor, input_cursor = b, ib
        if output_cursor != len(raw) or input_cursor != end:
            raise ValueError("spans do not cover the segment")
        for language, run_text, ia, ib in runs:
            if run_text.strip():
                yield SpeechJob(job_sequence, expected_sequence, language, run_text, ia, ib)
                job_sequence += 1
        expected_sequence += 1
        expected_input = end
    if not ended:
        raise ValueError("missing end event (frontend failed or stream was truncated)")


class EspeakBackend:
    name = "espeak-ng"

    def __init__(self, executable: str = "espeak-ng", voices: dict[str, str] | None = None,
                 timeout: float = 30.0):
        self.executable = executable
        self.voices = voices if voices is not None else {"ko-KR": "ko", "en-US": "en-us"}
        self.timeout = timeout

    def synthesize(self, job: SpeechJob, output: Path) -> None:
        if job.language not in self.voices:
            raise ValueError(f"no acoustic voice mapping for {job.language}")
        # No shell and no markup flag: input is literal text delivered via stdin.
        subprocess.run([self.executable, "-v", self.voices[job.language], "-w", str(output), "--stdin"],
                       input=job.text, encoding="utf-8", stdout=subprocess.DEVNULL,
                       stderr=subprocess.PIPE, check=True, timeout=self.timeout)


class CommandBackend:
    """External adapter receives text on stdin and writes PCM WAV to {output}."""
    name = "command"

    def __init__(self, argv: list[str], timeout: float = 30.0):
        if not isinstance(argv, list) or not argv or not all(isinstance(x, str) and x for x in argv):
            raise ValueError("command config argv must be a nonempty list of strings")
        if not any("{output}" in arg for arg in argv):
            raise ValueError("command argv must include {output}")
        self.argv, self.timeout = argv, timeout

    def synthesize(self, job: SpeechJob, output: Path) -> None:
        values = {"output": str(output), "language": job.language}
        argv = [re.sub(r"\{(output|language)\}", lambda match: values[match[1]], arg) for arg in self.argv]
        subprocess.run(argv, input=job.text, encoding="utf-8", stdout=subprocess.DEVNULL,
                       stderr=subprocess.PIPE, check=True, timeout=self.timeout)


def wav_info(path: Path) -> dict:
    try:
        audio = wave.open(str(path), "rb")
    except EOFError as error:
        raise ValueError("backend returned a truncated WAV header") from error
    with audio:
        channels, width, rate, frames = audio.getnchannels(), audio.getsampwidth(), audio.getframerate(), audio.getnframes()
        if audio.getcomptype() != "NONE" or channels < 1 or width < 1 or rate < 1:
            raise ValueError("backend must return an uncompressed PCM WAV")
        remaining = frames
        while remaining:
            data = audio.readframes(min(remaining, 8192))
            consumed = len(data) // (channels * width)
            if not consumed or len(data) % (channels * width):
                raise ValueError("backend returned truncated WAV data")
            remaining -= consumed
        if audio.readframes(1):
            raise ValueError("backend WAV contains an incomplete final PCM frame")
    return {"channels": channels, "sample_width": width, "sample_rate": rate,
            "frames": frames, "duration_seconds": frames / rate}


def join_wavs(paths: list[Path], output: Path) -> None:
    expected = None
    with wave.open(str(output), "wb") as combined:
        for path in paths:
            with wave.open(str(path), "rb") as source:
                actual = (source.getnchannels(), source.getsampwidth(), source.getframerate())
                if expected is None:
                    expected = actual
                    combined.setnchannels(actual[0])
                    combined.setsampwidth(actual[1])
                    combined.setframerate(actual[2])
                elif expected != actual:
                    raise ValueError("backend WAV formats differ; resample in the adapter")
                while True:
                    data = source.readframes(8192)
                    if not data:
                        break
                    combined.writeframesraw(data)


def synthesize(lines: Iterable[str], output_dir: Path, backend: Backend) -> dict:
    output_dir.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    manifest = {"version": 1, "backend": backend.name, "status": "processing",
                "human_review": "pending", "jobs": [], "combined_wav": None,
                "first_job_received_ms": None, "first_audio_file_ms": None}
    paths: list[Path] = []
    try:
        for job in speech_jobs(lines):
            if manifest["first_job_received_ms"] is None:
                manifest["first_job_received_ms"] = (time.perf_counter() - started) * 1000
            path = output_dir / f"audio-{job.sequence:05d}.wav"
            job_started = time.perf_counter()
            backend.synthesize(job, path)
            info = wav_info(path)
            if info["frames"] and manifest["first_audio_file_ms"] is None:
                manifest["first_audio_file_ms"] = (time.perf_counter() - started) * 1000
            paths.append(path)
            manifest["jobs"].append({**job.__dict__, "wav": path.name, **info,
                                     "synthesis_ms": (time.perf_counter() - job_started) * 1000})
        if paths:
            join_wavs(paths, output_dir / "combined.wav")
            manifest["combined_wav"] = "combined.wav"
        manifest["status"] = "complete"
    except Exception as error:
        manifest["status"] = "failed"
        manifest["error"] = str(error) or type(error).__name__
        raise
    finally:
        manifest["elapsed_ms"] = (time.perf_counter() - started) * 1000
        (output_dir / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return manifest


def positive_seconds(value: str) -> float:
    result = float(value)
    if not 0 < result < float("inf"):
        raise argparse.ArgumentTypeError("timeout must be positive and finite")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, help="JSONL file; omit to read stdin")
    parser.add_argument("--output-dir", type=Path, required=True, help="new directory for WAVs and manifest")
    parser.add_argument("--backend", choices=("espeak", "command"), default="espeak")
    parser.add_argument("--espeak", default="espeak-ng")
    parser.add_argument("--voice-map", type=Path, help="JSON object mapping render language to eSpeak voice")
    parser.add_argument("--command-config", type=Path, help='JSON object {"argv":[...,"{output}","{language}"]}')
    parser.add_argument("--timeout", type=positive_seconds, default=30.0)
    args = parser.parse_args()
    try:
        if args.backend == "command":
            if not args.command_config:
                raise ValueError("--command-config is required for command backend")
            config = json.loads(args.command_config.read_text(encoding="utf-8"))
            if not isinstance(config, dict):
                raise ValueError("command config must be an object")
            backend: Backend = CommandBackend(config.get("argv"), args.timeout)
        else:
            voices = json.loads(args.voice_map.read_text(encoding="utf-8")) if args.voice_map else None
            if voices is not None and (not isinstance(voices, dict) or not voices or
                                       not all(isinstance(k, str) and isinstance(v, str) and k and v for k, v in voices.items())):
                raise ValueError("voice map must contain nonempty language/voice strings")
            backend = EspeakBackend(args.espeak, voices, args.timeout)
        if args.input:
            with args.input.open(encoding="utf-8") as lines:
                manifest = synthesize(lines, args.output_dir, backend)
        else:
            sys.stdin.reconfigure(encoding="utf-8")
            manifest = synthesize(sys.stdin, args.output_dir, backend)
        print(json.dumps({"status": manifest["status"], "jobs": len(manifest["jobs"]),
                          "manifest": str(args.output_dir / "manifest.json")}, ensure_ascii=False))
        return 0
    except (OSError, ValueError, UnicodeError, EOFError, wave.Error, subprocess.SubprocessError) as error:
        print(f"tts bridge: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
