#!/usr/bin/env python3
"""Reference adapter contract, UTF-8 byte routing and real offline synthesis."""
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import wave

ROOT, TTF, RESOURCES = map(Path, sys.argv[1:4])
ESPEAK = sys.argv[4] if len(sys.argv) > 4 else None
spec = importlib.util.spec_from_file_location("ttf_bridge", ROOT / "tools/tts_bridge/bridge.py")
bridge = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = bridge
spec.loader.exec_module(bridge)


def pcm(path, rate=22050):
    with wave.open(str(path), "wb") as audio:
        audio.setnchannels(1)
        audio.setsampwidth(2)
        audio.setframerate(rate)
        audio.writeframes(struct.pack("<hhhh", 0, 100, -100, 0))


def sample():
    # Render language, not the English source, must select the Korean voice.
    first, second = "애플은 ", "Apple."
    a = len(first.encode("utf-8"))
    spans = [dict(begin=0, end=a, input_begin=0, input_end=9, source_lang="en-US", render_lang="ko-KR"),
             dict(begin=a, end=a+len(second), input_begin=9, input_end=15, source_lang="en-US", render_lang="en-US")]
    return [dict(version=1, event="segment", sequence=0, input_begin=0, input_end=15,
                 text=first+second, spans=spans), dict(version=1, event="end", segment_count=1, input_bytes=15)]


def lines(events):
    return [json.dumps(event, ensure_ascii=False) + "\n" for event in events]


class StubBackend:
    name = "test-pcm"

    def __init__(self, fail=False, different_rate=False):
        self.jobs, self.fail, self.different_rate = [], fail, different_rate

    def synthesize(self, job, output):
        self.jobs.append(job)
        if self.fail:
            raise ValueError("test backend failure")
        pcm(output, 16000 if self.different_rate and job.sequence else 22050)


class BridgeTests(unittest.TestCase):
    def test_routes_byte_offsets_by_render_language(self):
        jobs = list(bridge.speech_jobs(lines(sample())))
        self.assertEqual([job.language for job in jobs], ["ko-KR", "en-US"])
        self.assertEqual([job.text for job in jobs], ["애플은 ", "Apple."])
        self.assertEqual([job.sequence for job in jobs], [0, 1])

    def test_coalesces_same_voice(self):
        events = sample()
        events[0]["spans"][1]["render_lang"] = "ko-KR"
        jobs = list(bridge.speech_jobs(lines(events)))
        self.assertEqual(len(jobs), 1)
        self.assertEqual(jobs[0].text, events[0]["text"])

    def test_rejects_incomplete_and_malformed_protocol(self):
        invalid = [sample()[:-1], sample()+sample(), [dict(version=2, event="end")]]
        for field, value in [("sequence", 3), ("input_begin", 2), ("version", True)]:
            events = sample()
            events[0][field] = value
            invalid.append(events)
        for field, value in [("end", 1), ("input_end", 999), ("render_lang", "")]:
            events = sample()
            events[0]["spans"][0][field] = value
            invalid.append(events)
        for events in invalid:
            with self.subTest(events=events), self.assertRaises((ValueError, UnicodeError)):
                list(bridge.speech_jobs(lines(events)))

    def test_manifest_and_wav_join(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)/"output"
            backend = StubBackend()
            result = bridge.synthesize(lines(sample()), directory, backend)
            self.assertEqual(result["status"], "complete")
            self.assertEqual(result["human_review"], "pending")
            self.assertEqual(bridge.wav_info(directory/"combined.wav")["frames"], 8)
            self.assertIsNotNone(result["first_audio_file_ms"])
            with self.assertRaises(FileExistsError):
                bridge.synthesize(lines(sample()), directory, backend)

    def test_failure_manifest_not_success(self):
        with tempfile.TemporaryDirectory() as temporary:
            for name, backend in [("backend", StubBackend(fail=True)), ("format", StubBackend(different_rate=True))]:
                directory = Path(temporary)/name
                with self.assertRaises(ValueError):
                    bridge.synthesize(lines(sample()), directory, backend)
                manifest = json.loads((directory/"manifest.json").read_text())
                self.assertEqual(manifest["status"], "failed")
                self.assertIsNone(manifest["combined_wav"])

    def test_command_backend_literal_stdin(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)/"{language}"
            directory.mkdir()
            adapter = Path(temporary)/"adapter.py"
            adapter.write_text("import pathlib,sys,wave\ntext=sys.stdin.read()\n"
                               "pathlib.Path(sys.argv[1]+'.txt').write_text(text)\n"
                               "with wave.open(sys.argv[1],'wb') as w:\n"
                               " w.setparams((1,2,22050,0,'NONE','not compressed')); w.writeframes(b'\\0\\0'*8)\n")
            output = directory/"out.wav"
            text = '-v fake; $(touch NEVER) "hello" 한글'
            job = bridge.SpeechJob(0, 0, "ko-KR", text, 0, len(text.encode()))
            bridge.CommandBackend([sys.executable, str(adapter), "{output}"]).synthesize(job, output)
            self.assertEqual(Path(str(output)+".txt").read_text(), text)
            self.assertEqual(bridge.wav_info(output)["frames"], 8)

    def test_rejects_malformed_pcm_payloads(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)/"audio.wav"
            for payload in (b"RI", b"RIFF\x20\x00\x00\x00WAVE"):
                path.write_bytes(payload)
                with self.assertRaises((ValueError, wave.Error)):
                    bridge.wav_info(path)
            pcm(path)
            valid = bytearray(path.read_bytes())
            path.write_bytes(valid[:-2])
            with self.assertRaises(ValueError):
                bridge.wav_info(path)
            # Three data bytes cannot represent complete 16-bit mono samples.
            struct.pack_into("<I", valid, 40, 3)
            struct.pack_into("<I", valid, 4, 40)
            path.write_bytes(valid[:47] + b"\0")
            with self.assertRaisesRegex(ValueError, "incomplete final PCM frame"):
                bridge.wav_info(path)

    def test_frontend_protocol_to_adapter(self):
        text = "Apple은 사과입니다. Please go to the 1st floor."
        process = subprocess.run([str(TTF), "stream-json", "--resources", str(RESOURCES),
                                  "--chunk-bytes", "1", "-"], input=text, encoding="utf-8",
                                 capture_output=True, check=True)
        with tempfile.TemporaryDirectory() as temporary:
            result = bridge.synthesize(process.stdout.splitlines(), Path(temporary)/"audio", StubBackend())
            self.assertEqual(result["status"], "complete")
            self.assertEqual({job["language"] for job in result["jobs"]}, {"ko-KR", "en-US"})
            self.assertIn("애플은 사과입니다.", "".join(job["text"] for job in result["jobs"]))
            self.assertIn("first floor", "".join(job["text"] for job in result["jobs"]))

    @unittest.skipUnless(ESPEAK, "optional eSpeak NG not installed")
    def test_real_offline_tts(self):
        text = "Apple은 사과입니다. Please go to the 1st floor."
        process = subprocess.run([str(TTF), "stream-json", "--resources", str(RESOURCES), "-"],
                                 input=text, encoding="utf-8", capture_output=True, check=True)
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)/"audio"
            result = bridge.synthesize(process.stdout.splitlines(), directory, bridge.EspeakBackend(ESPEAK))
            self.assertEqual(result["status"], "complete")
            info = bridge.wav_info(directory/"combined.wav")
            self.assertGreater(info["frames"], 100)
            self.assertGreater(info["duration_seconds"], 0)
            with wave.open(str(directory/"combined.wav"), "rb") as audio:
                self.assertTrue(any(audio.readframes(info["frames"])))


if __name__ == "__main__":
    unittest.main(argv=[sys.argv[0]], verbosity=2)
