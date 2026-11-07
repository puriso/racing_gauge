"""本番の SDL 描画を録画し、CI でダウンロードできる目視確認用レポートを作る。"""

import argparse
import hashlib
import html
import json
import math
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone

import imageio_ffmpeg
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent
SCENARIOS = {
    "ramp": {
        "description": "油温 -10→170℃・水温 -10→130℃を90秒で上昇。油圧 0→10bar、両端を保持",
        "seconds": 100, "snapshots": [0, 2, 20, 50, 92, 100],
    },
    "warnings": {
        "description": "油圧5→3.1/3.0/2.9→5barの各250ms低下と、2.9barを2秒保持するLOW警告・回復",
        "seconds": 40, "snapshots": [0, 8.1, 12.1, 16.2, 23, 24.2, 27.5, 30, 40],
    },
    "startup": {
        "description": "始動直後1秒は油温・水温70℃、その後-10℃に戻ってゆっくり暖機",
        "seconds": 30, "snapshots": [0, 0.9, 1, 2, 10, 30],
    },
    "warm-start": {
        "description": "油温・水温とも70℃から始動し、そのままゆっくり上昇",
        "seconds": 30, "snapshots": [0, 0.5, 1, 10, 20, 30],
    },
    "menu": {
        "description": "油温・水温がゆっくり上昇。8〜13秒は詳細画面、通常画面への復帰を確認",
        "seconds": 20, "snapshots": [0, 4, 8, 12, 16, 20],
    },
}


def validate_inputs(metadata):
    frames = metadata["frames"]
    scenario = metadata["scenario"]
    for frame in frames:
        for key, low, high in (("oil_c", -10, 170), ("water_c", -10, 130), ("pressure_bar", 0, 10)):
            if not low <= frame[key] <= high:
                raise RuntimeError(f"入力が検証範囲外です: {scenario}, {key}, {frame}")
    if scenario == "ramp":
        for key, start, end in (("oil_c", -10, 170), ("water_c", -10, 130), ("pressure_bar", 0, 10)):
            values = [frame[key] for frame in frames]
            if values[0] != start or values[-1] != end or values != sorted(values):
                raise RuntimeError(f"上昇シナリオの入力が不正です: {key}")
    if scenario == "warnings":
        samples = {frame["time_ms"]: frame for frame in frames}
        for time, pressure in ((8100, 3.1), (12100, 3.0), (16200, 2.9), (23000, 2.9), (30000, 5.0)):
            if not math.isclose(samples[time]["pressure_bar"], pressure, abs_tol=0.0001):
                raise RuntimeError(f"油圧の境界値・回復入力が不正です: {time}")
    if scenario in ("startup", "warm-start"):
        if frames[0]["oil_c"] != 70 or frames[0]["water_c"] != 70:
            raise RuntimeError("始動時の70℃入力がありません")
    if scenario == "startup":
        if frames[10]["oil_c"] != -10 or frames[10]["water_c"] != -10:
            raise RuntimeError("始動時70℃から低温へ戻る入力がありません")
    # 始動直後の70℃→低温の変化以外、温度はゆっくり上昇させる。
    for before, after in zip(frames, frames[1:]):
        for key in ("oil_c", "water_c"):
            if scenario == "startup" and after["time_ms"] == 1000:
                continue
            if not 0 <= after[key] - before[key] <= 0.21:
                raise RuntimeError(f"温度が急変しています: {scenario}, {after}")


def warning_visible(image):
    # 通常の赤い目盛りを避け、LOW ボックス中央の赤背景を検出する。
    return sum(red > 180 and green < 80 and blue < 80
               for red, green, blue in image.crop((40, 130, 103, 159)).getdata()) > 100


def convert_and_check(directory):
    metadata = json.loads((directory / "inputs.json").read_text())
    frames = metadata["frames"]
    config = SCENARIOS[metadata["scenario"]]
    sources = sorted(directory.glob("frame-*.ppm"))
    if len(frames) != config["seconds"] * 10 + 1 or len(sources) != len(frames):
        raise RuntimeError(f"録画が不完全です: {directory}")
    if [frame["time_ms"] for frame in frames] != list(range(0, config["seconds"] * 1000 + 1, 100)):
        raise RuntimeError("録画時刻が 100ms 間隔ではありません")
    validate_inputs(metadata)
    seen = set()
    snapshots = [round(seconds * 10) for seconds in config["snapshots"]]
    sheet = Image.new("RGB", (320 * 3, 264 * math.ceil(len(snapshots) / 3)), "#252525")
    labels = ImageDraw.Draw(sheet)
    for index, source in enumerate(sources):
        with Image.open(source) as image:
            image.load()
            if image.size != (320, 240) or image.getbbox() is None:
                raise RuntimeError(f"空白またはサイズ不正のフレーム: {source}")
            seen.add(hashlib.sha256(image.tobytes()).digest())
            image.save(source.with_suffix(".png"))
            # 130℃を超えた油温バーが右側の数値領域へはみ出さないことを確認する。
            if metadata["scenario"] == "ramp" and metadata["sensors"]["oil_temp"]:
                if any(red > 180 and green < 80 and blue < 80
                       for red, green, blue in image.crop((231, 16, 237, 34)).getdata()):
                    raise RuntimeError(f"油温バーが数値領域へはみ出しています: {frames[index]}")
            if metadata["scenario"] == "warnings" and metadata["sensors"]["pressure"]:
                visible = warning_visible(image)
                time = frames[index]["time_ms"]
                if time < 22000 and visible:
                    raise RuntimeError(f"短い油圧低下でLOWが点灯しています: {time}")
                if time in (23000, 24200) and not visible:
                    raise RuntimeError(f"継続する低油圧・解除後の保持でLOWが点灯していません: {time}")
                if time >= 27500 and visible:
                    raise RuntimeError(f"油圧回復後もLOWが残っています: {time}")
            if index in snapshots:
                tile = snapshots.index(index)
                x, y = (tile % 3) * 320, (tile // 3) * 264
                sheet.paste(image, (x, y))
                labels.text((x + 6, y + 243), f'{frames[index]["time_ms"] / 1000:.1f}s', fill="white")
        source.unlink()
    if len(seen) < 20:
        raise RuntimeError("描画がほとんど変化していません")
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
<p>{html.escape(SCENARIOS[scenario]["description"])}</p>
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
各シナリオを10fpsで保存し、終点フレームも含めます。全レンジ100秒・油圧変動40秒・始動各30秒・詳細画面20秒。
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
            [str(program), "--scenario", scenario, "--seconds", str(SCENARIOS[scenario]["seconds"]), "--capture", str(directory)],
            env={**os.environ, "SDL_VIDEODRIVER": "dummy", "SDL_RENDER_DRIVER": "software"},
            check=True, timeout=180,
        )
        convert_and_check(directory)
        encode(directory)
    write_report(output, scenarios)
    print(f"録画とレポート: {output / 'index.html'}")


if __name__ == "__main__":
    main()
