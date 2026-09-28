"""Exercise the actual CLI and resource precedence, including sanitized builds."""
from pathlib import Path
import subprocess
import sys
import tempfile

cli, resources = sys.argv[1:]
with tempfile.TemporaryDirectory(prefix="ttf-speech-character-") as directory:
    root = Path(directory)
    customer, domain = root / "customer.tsv", root / "domain.tsv"
    domain.write_text("speech_character\tko-KR\tＡ\t도메인\n", encoding="utf-8")
    customer.write_text("speech_character\tko-KR\tＡ\t고객\n"
                        "speech_character\tko-KR\tＡ\t후순위\n", encoding="utf-8")
    for flags, expected in (([], "가에이나"), (["--domain", str(domain)], "가도메인나"),
                            (["--domain", str(domain), "--customer", str(customer)], "가고객나"),
                            (["--customer", str(customer), "--domain", str(domain)], "가고객나")):
        result = subprocess.run([cli, "normalize", "--resources", resources, "--output-mode",
                                 "speech", *flags, "가Ａ나"], capture_output=True, text=True, timeout=10)
        assert result.returncode == 0 and result.stdout == expected + "\n", result
    result = subprocess.run([cli, "validate-resource", "--resources", resources,
                             "--require-complete-validation", str(customer)],
                            capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result
print("system/domain/customer precedence, same-layer first declaration and resource validation passed")
