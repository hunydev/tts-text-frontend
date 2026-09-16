"""Exercise the compiler/runtime boundary through the public CLI."""
from pathlib import Path
import subprocess
import sys
import tempfile

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
print("compiled/source numeric and multiword parity, corrupt/truncated resource rejection passed")
