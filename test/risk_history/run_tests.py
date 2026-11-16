"""本番の危険点選択・前後1分・画面遷移をスタブなしで検証する。"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix="risk-history-") as directory:
    binary = Path(directory) / "test_risk_history"
    subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    f"-I{root / 'src/modules'}", str(root / "test/risk_history/test_risk_history.cpp"),
                    str(root / "src/modules/risk_history.cpp"), "-o", str(binary)], check=True)
    subprocess.run([binary], check=True, timeout=10)
