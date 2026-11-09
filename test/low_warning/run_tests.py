import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


root = Path(__file__).resolve().parents[2]
suite = root / "test" / "low_warning"
parser = argparse.ArgumentParser()
parser.add_argument("--baseline-ref")
parser.add_argument("--report", type=Path)
args = parser.parse_args()

with tempfile.TemporaryDirectory(prefix="low-warning-") as directory:
    binary = Path(directory) / "test_low_warning"
    source = root / "src" / "modules" / "low_warning.cpp"
    clock_adaptation = None
    original_digest = hashlib.sha256(source.read_bytes()).hexdigest()
    if args.baseline_ref:
        source = Path(directory) / "low_warning.cpp"
        baseline = subprocess.check_output(
            ["git", "show", f"{args.baseline_ref}:src/modules/low_warning.cpp"], cwd=root,
        )
        original_digest = hashlib.sha256(baseline).hexdigest()
        # ESP32のunsigned longは32ビット。64ビットホストでも同じ桁あふれで比較する。
        source.write_bytes(baseline.replace(b"unsigned long", b"uint32_t"))
        clock_adaptation = "ESP32 unsigned long -> uint32_t"
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
    results = []
    digest = hashlib.sha256(source.read_bytes()).hexdigest()
    for test in tests:
        # 本番の静的状態をテスト間で共有しないよう、ケースごとにプロセスを分ける。
        result = subprocess.run([binary, test], check=False, capture_output=True, text=True, timeout=5)
        results.append(dict(name=test, passed=result.returncode == 0, detail=result.stderr.strip()))
        failures += result.returncode != 0
        print(f"{test}: {'PASS' if result.returncode == 0 else 'FAIL'}", flush=True)
    print(f"{len(tests) - failures}/{len(tests)} passed", flush=True)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(dict(source_sha256=digest, source_original_sha256=original_digest, clock_adaptation=clock_adaptation, baseline_ref=args.baseline_ref, results=results), ensure_ascii=False))
    raise SystemExit(bool(failures))
