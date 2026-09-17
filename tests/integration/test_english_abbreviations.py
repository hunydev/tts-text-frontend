"""Original abbreviation policy and streaming contracts; no benchmark text copied."""
import argparse
import importlib.util
import subprocess
import tempfile
from pathlib import Path

from test_english_dates import PROFILES, exact_span, stream

ROOT = Path(__file__).resolve().parents[2]
# References are declared before candidate execution. Lowercase separated
# letters and expanded titles are this project's editable EN text policy.
REFERENCES = (
    ("en-US", "Dr. Rowan signed the report.", "Doctor Rowan signed the report."),
    ("en-US", "Mr. Vale recorded the trailer.", "Mister Vale recorded the trailer."),
    ("en-US", "Mrs. Quinn approved the payment.", "Missus Quinn approved the payment."),
    ("en-US", "Prof. Avery reviewed the film.", "Professor Avery reviewed the film."),
    ("en-US", "Dr. J. Rowan", "Doctor j Rowan"),
    ("en-US", "U.S. market", "u s market"),
    ("en-US", "U.K. bonds", "u k bonds"),
    ("en-US", "U.S.", "u s."),
    ("en-US", "U.S.\nU.K.", "u s.\nu k."),
    ("en-US", '"U.S."', '"u s."'),
    ("en-US", "“U.S.”", "“u s.”"),
    ("en-US", "J. R. Rowan", "j r Rowan"),
    ("en-US", "J.R. Rowan", "j r Rowan"),
    ("en-US", "J. Rowan", "j Rowan"),
    ("en-US", "NASA uses GPU.", "NASA uses GPU."),
    ("en-US", "Apple released the iPhone.", "Apple released the iPhone."),
    ("en-US", "THE FILM", "THE FILM"),
    ("en-US", "Oak Dr.", "Oak Dr."),
    ("en-US", "Dr.", "Dr."),
    ("en-US", "Dr. lower", "Dr. lower"),
    ("en-US", "Dr.\nRowan", "Dr.\nRowan"),
    ("en-US", "U.S.Beta", "U.S.Beta"),
    ("en-US", "A.", "A."),
    ("en-US", "Ms. Quinn", "Ms. Quinn"),
    ("en-US", "`U.S. Dr. Rowan`", "`U.S. Dr. Rowan`"),
    ("en-US", "https://example.test/Dr.Rowan", "https://example.test/Dr.Rowan"),
    ("ko-KR", "담당자는 Dr. Apple입니다.", "담당자는 닥터 애플입니다."),
    ("ko-KR", "담당자는 Mr. Apple입니다.", "담당자는 미스터 애플입니다."),
    ("ko-KR", "담당자는 Mrs. Apple입니다.", "담당자는 미시즈 애플입니다."),
    ("ko-KR", "담당자는 Prof. Apple입니다.", "담당자는 프로페서 애플입니다."),
    ("ko-KR", "U.S.", "유에스."),
    ("ko-KR", "표기는 U.S.", "표기는 유에스."),
    ("ko-KR", "U.S.를 사용합니다.", "유에스를 사용합니다."),
    ("ko-KR", "GPU를 사용합니다.", "지피유를 사용합니다."),
    ("ko-KR", "NASA를 확인합니다.", "나사를 확인합니다."),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("cli", type=Path)
    parser.add_argument("resources", type=Path)
    parser.add_argument("--library", type=Path)
    args = parser.parse_args()
    checks = native_checks = 0
    for language, text, expected in REFERENCES:
        for profile in PROFILES:
            for chunk in (1, 7, 64):
                result = stream(args.cli, args.resources, text, language, chunk, profile)
                assert result["text"] == expected, (text, expected, chunk, profile, result)
                checks += 1
    for text, surface, semantic, value, secondary, context in (
        ("Dr. Rowan", "Dr.", "ABBREVIATION", "dr", "title", "en-US"),
        ("U.S. market", "U.S.", "INITIALISM", "US", "dotted", "en-US"),
        ("U.S.", "U.S", "INITIALISM", "US", "dotted", "en-US"),
        ("J. R. Rowan", "J. R.", "INITIALISM", "JR", "dotted", "en-US"),
        ("GPU", "GPU", "INITIALISM", "GPU", "known", "en-US"),
        ("U.S.를 사용합니다.", "U.S.", "INITIALISM", "US", "dotted", "ko-KR"),
    ):
        result = stream(args.cli, args.resources, text, context)
        span = exact_span(result, text, surface)
        actual = tuple(span[k] for k in ("semantic_type", "value", "secondary", "source_lang", "context_lang"))
        assert actual == (semantic, value, secondary, "en-US", context), (text, span)
        checks += 1

    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory) / "abbreviations.tsv"
        overlay = ("--customer", str(path))
        for policy, text, expected in (
            ("initialism_policy\ten-US\tdotted\tpreserve\n", "U.S. market", "U.S. market"),
            ("initialism_policy\ten-US\tdotted\tnames\n", "U.S. market", "you ess market"),
            ("initialism_policy\ten-US\tknown\tletters\n", "GPU CPU", "g p u c p u"),
            ("initialism_policy\ten-US\tknown\tnames\n", "GPU", "gee pee you"),
            ("initialism_text\ten-US\tgpu\tletters\n", "GPU CPU NASA", "g p u CPU NASA"),
            ("abbreviation_expansion\ten-US\tdr\tDoc\n", "Dr. Rowan", "Doc Rowan"),
            ("abbreviation_expansion\ten-US\tdr\tpreserve\n", "Dr. Rowan", "Dr. Rowan"),
        ):
            path.write_text(policy, encoding="utf-8")
            for profile in PROFILES:
                for chunk in (1, 7, 64):
                    result = stream(args.cli, args.resources, text, chunk=chunk,
                                    profile=profile, overlay=overlay)
                    assert result["text"] == expected, (policy, text, result)
                    checks += 1
        path.write_text("initialism_policy\ten-US\tdotted\tinvalid-policy\n", encoding="utf-8")
        run = subprocess.run([str(args.cli), "normalize", "--resources", str(args.resources),
                              "--language", "en-US", *overlay, "U.S. market"],
                             capture_output=True, timeout=15, check=False)
        assert run.returncode != 0, (run.stdout, run.stderr)
        checks += 1
        # A general word dictionary must not turn dotted initials into a
        # homographic word. A separate explicit initialism entry may override.
        for policy, text, expected in (
            ("pronunciation\ten-US\tus\tAH S\n", "U.S.는 표기입니다.", "유에스는 표기입니다."),
            ("pronunciation\ten-US\tus\tAH S\n", "us는 표기입니다.", "어스는 표기입니다."),
            ("initialism_pronunciation\ten-US\tus\tAH S\n", "U.S.는 표기입니다.", "어스는 표기입니다."),
        ):
            path.write_text(policy, encoding="utf-8")
            for profile in PROFILES:
                for chunk in (1, 7, 64):
                    result = stream(args.cli, args.resources, text, language="ko-KR", chunk=chunk,
                                    profile=profile, overlay=overlay)
                    assert result["text"] == expected, (policy, text, result)
                    checks += 1

    if args.library:
        spec = importlib.util.spec_from_file_location("ttf_native", ROOT / "tools/web/native.py")
        native = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(native)
        for tiny in (False, True):
            for language in ("en-US", "ko-KR"):
                options = {"default_language": language}
                if tiny:
                    options.update(first_target=1, normal_target=3, max_length=12)
                engine = native.Engine(args.library.resolve(), args.resources.resolve(), options)
                try:
                    for lang, text, expected in REFERENCES:
                        if lang != language:
                            continue
                        source = text.encode()
                        batch = engine.normalize(source)
                        assert batch["text"] == expected, (text, batch, expected)
                        for split in range(1, len(source)):
                            actual = native.flatten(engine.stream(source, [split, len(source) - split]))
                            assert actual == batch, (text, split, tiny, batch, actual)
                            native_checks += 1
                finally:
                    engine.close()
        for options, text, expected, route in (
            ({"default_language": "ko-KR", "render_policy": "preserve"},
             "U.S.를 사용합니다.", "u s를 사용합니다.", None),
            ({"default_language": "ko-KR", "user_lexicon": [{"word": "U.S.", "pronunciation": "유에스커스텀"}]},
             "U.S.를 사용합니다.", "유에스커스텀를 사용합니다.", "user"),
            ({"default_language": "ko-KR", "user_lexicon": [{"word": "U.S.", "pronunciation": "커스텀"}]},
             "표기는 U.S.", "표기는 커스텀.", "user"),
            ({"default_language": "ko-KR", "user_lexicon": [{"word": "Dr.", "pronunciation": "닥터커스텀"}]},
             "Dr. Rowan은 담당자입니다.", None, "user"),
        ):
            engine = native.Engine(args.library.resolve(), args.resources.resolve(), options)
            try:
                result = engine.normalize(text.encode())
                if expected is not None:
                    assert result["text"] == expected, (text, result)
                if route:
                    span = next(s for s in result["spans"] if s["semantic_type"] in ("ABBREVIATION", "INITIALISM"))
                    assert span["pronunciation_layer"] == route, (text, span)
                    assert "커스텀" in result["text"], (text, result)
                native_checks += 1
                for split in range(1, len(text.encode())):
                    source = text.encode()
                    actual = native.flatten(engine.stream(source, [split, len(source) - split]))
                    assert actual == result, (text, split, result, actual)
                    native_checks += 1
            finally:
                engine.close()
    print(f"{checks} abbreviation text/IR/policy/guard checks; {native_checks} native byte/override checks passed")


if __name__ == "__main__":
    main()
