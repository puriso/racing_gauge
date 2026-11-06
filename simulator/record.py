"""本番の SDL 描画を録画し、CI でダウンロードできる目視確認用レポートを作る。"""

import argparse
import html
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone

import imageio_ffmpeg
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
SCENARIOS = {
    "ramp": "油温 60→130℃・水温 60→120℃・油圧 0→10bar を徐々に上昇",
    "warnings": "旋回中の油圧低下、LOW 警告の保持と解除、R 表示、高温表示",
    "menu": "通常表示 → 詳細画面 → 通常表示（ピーク値・復帰時の残像確認）",
}


def convert_and_check(directory):
    metadata = json.loads((directory / "inputs.json").read_text())
    frames = metadata["frames"]
    sources = sorted(directory.glob("frame-*.ppm"))
    if len(frames) != 201 or len(sources) != len(frames):
        raise RuntimeError(f"録画が不完全です: {directory}")
    if [frame["time_ms"] for frame in frames] != list(range(0, 20001, 100)):
        raise RuntimeError("録画時刻が 100ms 間隔ではありません")
    seen = set()
    sheet = Image.new("RGB", (320 * 3, 264 * 2), "#252525")
    labels = ImageDraw.Draw(sheet)
    for index, source in enumerate(sources):
        with Image.open(source) as image:
            image.load()
            if image.size != (320, 240) or image.getbbox() is None:
                raise RuntimeError(f"空白またはサイズ不正のフレーム: {source}")
            seen.add(image.tobytes())
            image.save(source.with_suffix(".png"))
            if index in (0, 40, 80, 120, 160, 200):
                tile = index // 40
                x, y = (tile % 3) * 320, (tile // 3) * 264
                sheet.paste(image, (x, y))
                labels.text((x + 6, y + 243), f'{frames[index]["time_ms"] / 1000:.1f}s', fill="white")
        source.unlink()
    if len(seen) < 20:
        raise RuntimeError("描画がほとんど変化していません")
    if metadata["scenario"] == "ramp":
        for key, start, end in (("oil_c", 60, 130), ("water_c", 60, 120), ("pressure_bar", 0, 10)):
            values = [frame[key] for frame in frames]
            if values[0] != start or values[-1] != end or values != sorted(values):
                raise RuntimeError(f"上昇シナリオの入力が不正です: {key}")
    sheet.save(directory / "contact-sheet.png")
    return metadata


def encode(directory):
    subprocess.run(
        [imageio_ffmpeg.get_ffmpeg_exe(), "-nostdin", "-loglevel", "error", "-framerate", "10",
         "-i", str(directory / "frame-%04d.png"), "-c:v", "libx264", "-crf", "18",
         "-pix_fmt", "yuv420p", "-movflags", "+faststart", str(directory / "recording.mp4")],
        check=True, timeout=120,
    )
    # エンコード後のファイルもデコードし、欠損した動画を成果物にしない。
    subprocess.run(
        [imageio_ffmpeg.get_ffmpeg_exe(), "-nostdin", "-v", "error", "-xerror", "-i",
         str(directory / "recording.mp4"), "-f", "null", "-"],
        check=True, timeout=60,
    )


def write_report(output, scenarios):
    sections = []
    for scenario in scenarios:
        sections.append(f'''<section><h2>{html.escape(scenario)}</h2>
<p>{html.escape(SCENARIOS[scenario])}</p>
<video controls loop playsinline preload="metadata" poster="{scenario}/frame-0000.png"
 src="{scenario}/recording.mp4"></video>
<p><a href="{scenario}/recording.mp4">MP4 を開く</a> ·
<a href="{scenario}/contact-sheet.png">代表フレーム</a> ·
<a href="{scenario}/inputs.json">入力と時刻（JSON）</a></p>
<details><summary>代表フレームを表示</summary>
<img src="{scenario}/contact-sheet.png" alt="{scenario} の代表フレーム"></details></section>''')
    (output / "index.html").write_text('''<!doctype html>
<html lang="ja"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>racing_gauge — SDL visual review</title>
<style>body{font:16px system-ui;background:#16191d;color:#eee;max-width:1000px;margin:32px auto;padding:0 20px}
a{color:#8dcaff}section{border-top:1px solid #444;padding:20px 0}video{width:640px;max-width:100%;image-rendering:pixelated}
img{max-width:100%}p{line-height:1.7}</style>
<h1>racing_gauge SDL 録画</h1>
<p>本番の描画コードへ模擬入力を渡し、320×240 の転送先フレームバッファを録画しています。
20秒のシナリオを10fpsで保存（終点フレームを含め動画は20.1秒）。
センサー入力値は JSON、無劣化の描画は各 PNG で確認できます。
実機のフレームレートや ADC・電源回路を検証するものではありません。</p>
''' + "\n".join(sections) + "</html>\n", encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", choices=["all", *SCENARIOS], default="all")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--skip-build", action="store_true")
    args = parser.parse_args()
    output = args.output or ROOT / "out" / datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S-%f")
    output = output.resolve()
    # 既存の成果物やユーザーのファイルを上書きしない。
    output.mkdir(parents=True, exist_ok=False)
    if not args.skip_build:
        subprocess.run([sys.executable, "-m", "platformio", "run", "-d", str(ROOT), "-e", "desktop"], check=True)
    program = ROOT / ".pio" / "build" / "desktop" / ("program.exe" if os.name == "nt" else "program")
    scenarios = list(SCENARIOS) if args.scenario == "all" else [args.scenario]
    for scenario in scenarios:
        directory = output / scenario
        subprocess.run(
            [str(program), "--scenario", scenario, "--capture", str(directory)],
            env={**os.environ, "SDL_VIDEODRIVER": "dummy", "SDL_RENDER_DRIVER": "software"},
            check=True, timeout=180,
        )
        convert_and_check(directory)
        encode(directory)
    write_report(output, scenarios)
    print(f"録画とレポート: {output / 'index.html'}")


if __name__ == "__main__":
    main()
