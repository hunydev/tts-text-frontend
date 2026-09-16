"""Exercise the compiler/runtime boundary through the public CLI."""
from pathlib import Path
import subprocess
import sys
import tempfile

cli, compiler, resources, domain = sys.argv[1:]

def normalize(resource):
    return subprocess.run(
        [cli, "normalize", "--resources", resources, "--domain", str(resource), "123-456"],
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
print("compiled/source parity and corrupt/truncated resource rejection passed")
