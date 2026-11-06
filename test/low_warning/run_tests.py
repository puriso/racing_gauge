import argparse
import os
from pathlib import Path
import subprocess
import tempfile


root = Path(__file__).resolve().parents[2]
suite = root / "test" / "low_warning"
parser = argparse.ArgumentParser()
parser.add_argument("--baseline-ref")
args = parser.parse_args()

with tempfile.TemporaryDirectory(prefix="low-warning-") as directory:
    binary = Path(directory) / "test_low_warning"
    source = root / "src" / "modules" / "low_warning.cpp"
    if args.baseline_ref:
        source = Path(directory) / "low_warning.cpp"
        source.write_bytes(
            subprocess.check_output(
                ["git", "show", f"{args.baseline_ref}:src/modules/low_warning.cpp"],
                cwd=root,
            )
        )
    subprocess.run(
        [
            os.environ.get("CXX", "c++"),
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            f"-I{suite / 'stubs'}",
            f"-I{root / 'include'}",
            f"-I{root / 'src' / 'modules'}",
            str(suite / "test_low_warning.cpp"),
            str(source),
            "-o",
            str(binary),
        ],
        check=True,
    )
    tests = subprocess.check_output([binary, "--list"], text=True).splitlines()
    failures = 0
    for test in tests:
        # 本番の静的状態をテスト間で共有しないよう、ケースごとにプロセスを分ける。
        result = subprocess.run([binary, test], check=False)
        failures += result.returncode != 0
        print(f"{test}: {'PASS' if result.returncode == 0 else 'FAIL'}", flush=True)
    print(f"{len(tests) - failures}/{len(tests)} passed", flush=True)
    raise SystemExit(bool(failures))
