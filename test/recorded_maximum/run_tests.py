"""本番の最高値記録をスタブなしで検証する。"""

import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="recorded-maximum-") as directory:
    binary = Path(directory) / "test_recorded_maximum"
    subprocess.run([
        os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
        f"-I{root / 'src/modules'}", str(root / "test/recorded_maximum/test_recorded_maximum.cpp"),
        "-o", str(binary),
    ], check=True)
    subprocess.run([binary], check=True, timeout=5)
