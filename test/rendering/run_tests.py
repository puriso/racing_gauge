"""描画範囲と転送量を、実機を使わずに検証する。"""

import argparse
from pathlib import Path
import os
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-ref", help="指定したGit参照の表示処理でも転送量を計測する")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    compiler = os.environ.get("CXX", "g++")
    with tempfile.TemporaryDirectory(prefix="gauge-rendering-") as temporary:
        scratch = Path(temporary)
        variants = [("current", None)]
        if args.baseline_ref:
            source = scratch / "baseline_display.cpp"
            source.write_bytes(subprocess.check_output(
                ["git", "show", f"{args.baseline_ref}:src/modules/display.cpp"], cwd=repo))
            variants.append(("baseline", source))

        for label, source in variants:
            for fps_enabled in (False, True):
                binary = scratch / f"{label}-{int(fps_enabled)}.exe"
                command = [compiler, "-std=c++17", "-O2", "-Iinclude", "-Isrc", "-Isrc/modules",
                           "-Itest/rendering/stubs", "test/rendering/test_display_transfers.cpp", "-o", str(binary)]
                if fps_enabled:
                    command.append("-DTEST_FPS_ENABLED=1")
                if source:
                    command.append(f'-DDISPLAY_IMPLEMENTATION="{source.as_posix()}"')
                subprocess.run(command, cwd=repo, check=True)
                print(f"{label}, FPS overlay {'on' if fps_enabled else 'off'}", flush=True)
                subprocess.run([str(binary)] + (["--measure"] if source else []), cwd=repo, check=True)


if __name__ == "__main__":
    main()
