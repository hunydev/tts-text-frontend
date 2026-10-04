"""Exercise the compiler/runtime boundary through the public CLI."""
from pathlib import Path
import subprocess
import sys
import tempfile
import shutil

cli, compiler, resources, domain = sys.argv[1:]

def normalize(resource, text="123-456"):
    return subprocess.run(
        [cli, "normalize", "--resources", resources, "--domain", str(resource), text],
        check=False, capture_output=True, text=True,
    )

with tempfile.TemporaryDirectory(prefix="ttf-resource-test-") as temp:
    binary = Path(temp) / "domain.ttfb"
    subprocess.run([sys.executable, compiler, domain, str(binary)], check=True)
    source = normalize(domain)
    compiled = normalize(binary)
    assert source.returncode == compiled.returncode == 0, (source.stderr, compiled.stderr)
    assert source.stdout == compiled.stdout == "일이삼 사오육\n"
    payload = bytearray(binary.read_bytes())
    payload[-1] ^= 1
    binary.write_bytes(payload)
    assert normalize(binary).returncode != 0, "corrupted payload was accepted"
    binary.write_bytes(b"TTFB\x01")
    assert normalize(binary).returncode != 0, "truncated header was accepted"
    # Canonical multiword keys must behave identically through either loader.
    phrase = Path(temp) / "phrase.tsv"
    phrase.write_text("lexicon\tko-KR\ten-US:AFTER   Effects\t도메인이펙트\n", encoding="utf-8")
    subprocess.run([sys.executable, compiler, str(phrase), str(binary)], check=True)
    text = "After\tEffects를 사용합니다."
    for resource in (phrase, binary):
        result = normalize(resource, text)
        assert result.returncode == 0 and result.stdout == "도메인이펙트를 사용합니다.\n", result
    records = []
    for resource in (phrase, binary):
        records.append(subprocess.check_output(
            [cli, "stream-json", "--resources", resources, "--domain", str(resource),
             "--chunk-bytes", "1", "-"], input=text.encode()))
    assert records[0] == records[1], "compiled multiword metadata differs"

    # Source and binary loaders only recognize LF, with one optional CR before
    # LF/EOF. Check actual runtime behavior instead of repeating the parser.
    rows = ["exact\t*\t123-456\tvalidated", "exact\t*\t654-321\tother"]
    endings = Path(temp) / "endings.tsv"
    for separator, trailing in (("\n", "\n"), ("\r\n", "\r\n"),
                                ("\n", ""), ("\r\n", "\r")):
        endings.write_bytes((separator.join(rows) + trailing).encode("utf-8"))
        subprocess.run([sys.executable, compiler, str(endings), str(binary)], check=True)
        for resource in (endings, binary):
            result = normalize(resource)
            assert result.returncode == 0 and result.stdout == "validated\n", result

    # Every additional separator recognized by str.splitlines() must not become
    # an implicit record delimiter. These two-row inputs have seven tabs in one
    # runtime record and must fail both source loading and compilation. A failed
    # compilation must leave a previous validated artifact byte-identical.
    previous = binary.read_bytes()
    for separator in ("\r", "\v", "\f", "\x1c", "\x1d", "\x1e", "\x85", "\u2028", "\u2029"):
        endings.write_bytes((separator.join(rows) + "\n").encode("utf-8"))
        compiled = subprocess.run(
            [sys.executable, compiler, str(endings), str(binary)], capture_output=True,
        )
        assert compiled.returncode != 0, ("compiler accepted non-LF records", repr(separator))
        assert b"line 1: expected four nonempty TSV fields" in compiled.stderr, compiled.stderr
        assert binary.read_bytes() == previous, "failed compile replaced an earlier artifact"
        assert normalize(endings).returncode != 0, ("runtime accepted non-LF records", repr(separator))

    # A separator inside a field/comment remains data; the compiler must not
    # reject such valid runtime resources or silently split their field values.
    for character in ("\v", "\f", "\x85", "\u2028", "\u2029"):
        expected = "before" + character + "after"
        endings.write_bytes(("# comment" + character + "not a new row\n" +
                             "exact\t*\t123-456\t" + expected + "\n").encode("utf-8"))
        subprocess.run([sys.executable, compiler, str(endings), str(binary)], check=True)
        for resource in (endings, binary):
            result = normalize(resource)
            assert result.returncode == 0 and result.stdout == expected + "\n", result
    # Manifest consumers still see all five CJK files (three generated tables,
    # the hand-authored hanja_rules.tsv and hanja_readings_stdict.tsv). Their compiled forms
    # retain conditional freeze loading and exactly the same output/trace.
    compiled_root = Path(temp) / "resources"
    shutil.copytree(resources, compiled_root)
    manifest = (compiled_root / "manifest.ttfm").read_text(encoding="utf-8")
    paths = [line for line in manifest.splitlines()
             if line.startswith("languages/ko-KR/cjk/")]
    assert len(paths) == 5, paths
    for i, relative in enumerate(paths):
        src = compiled_root / relative
        dst = src.with_suffix(".bin" if i % 2 else ".ttfb")
        subprocess.run([sys.executable, compiler, str(src), str(dst), "--ttf", cli,
                        "--resources", resources, "--require-complete-validation"], check=True)
        manifest = manifest.replace(relative, str(dst.relative_to(compiled_root)))
        src.unlink()  # No accidental fallback to the source TSV.
    (compiled_root / "manifest.ttfm").write_text(manifest, encoding="utf-8")
    for flags in ([], ["--hanja", "korean"], ["--kana", "korean"],
                  ["--hanja", "korean", "--kana", "korean"]):
        results = [subprocess.check_output(
            [cli, "inspect", "--trace", "--resources", str(root), *flags, "張勳 ソウル"])
            for root in (resources, compiled_root)]
        assert results[0] == results[1], "compiled CJK manifest changes output/trace"
print("compiled/source numeric, multiword and CJK manifest parity, line-ending grammar, "
      "failed compilation atomicity, corrupt/truncated resource rejection passed")
