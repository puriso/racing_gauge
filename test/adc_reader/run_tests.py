import argparse
import os
from pathlib import Path
import subprocess
import tempfile


root = Path(__file__).resolve().parents[2]
suite = root / "test" / "adc_reader"
parser = argparse.ArgumentParser()
parser.add_argument("--check-legacy", action="store_true")
args = parser.parse_args()
library = root / ".pio" / "libdeps" / "m5stack-cores3" / "Adafruit ADS1X15"
if not (library / "Adafruit_ADS1X15.h").is_file():
    raise SystemExit("先に pio pkg install -e m5stack-cores3 で依存ライブラリを取得してください。")

with tempfile.TemporaryDirectory(prefix="adc-reader-") as directory:
    compiler = [
        os.environ.get("CXX", "c++"),
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Werror",
        "-Wno-unused-variable",
        f"-I{suite / 'stubs'}",
        f"-I{root / 'include'}",
        f"-I{library}",
    ]
    binary = Path(directory) / "test_adc_reader"
    subprocess.run(
        compiler
        + [
            str(suite / "test_adc_reader.cpp"),
            str(root / "src" / "modules" / "adc_reader.cpp"),
            "-o",
            str(binary),
        ],
        check=True,
    )
    failures = 0
    tests = subprocess.check_output([binary, "--list"], text=True).splitlines()
    for test in tests:
        # 静的な内部状態を共有せず、待機の無限ループも検出する。
        try:
            result = subprocess.run([binary, test], check=False, timeout=5)
            passed = result.returncode == 0
        except subprocess.TimeoutExpired:
            passed = False
        failures += not passed
        print(f"{test}: {'PASS' if passed else 'FAIL'}", flush=True)
    if args.check_legacy:
        legacy_source = Path(directory) / "legacy.cpp"
        legacy_source.write_text(
            '#include <Adafruit_ADS1X15.h>\n'
            'int main() { fake::present = false; Adafruit_ADS1015 adc; '
            'adc.begin(); adc.readADC_SingleEnded(2); }\n'
        )
        legacy = Path(directory) / "legacy"
        subprocess.run(
            compiler + [str(legacy_source), str(library / "Adafruit_ADS1X15.cpp"), "-o", str(legacy)],
            check=True,
        )
        try:
            subprocess.run([legacy], timeout=0.5, check=True)
        except subprocess.TimeoutExpired:
            print("legacy: 変換完了待ちから戻らないことを再現", flush=True)
        else:
            failures += 1
            print("legacy: 無期限待機を再現できませんでした", flush=True)
    print(f"{len(tests) - failures}/{len(tests)} passed", flush=True)
    raise SystemExit(bool(failures))
