import subprocess

Import("env")

# SDL のインクルードとリンク設定を各 OS の pkg-config から取得する。
env.MergeFlags(subprocess.check_output(["pkg-config", "--cflags", "--libs", "sdl2"], text=True))
