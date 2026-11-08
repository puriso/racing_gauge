"""録画済みのPNGから、README紹介用の短いGIFを再生成する。"""

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent


def load_segment(recordings, scenario, indices, label, delay):
    directory = recordings / scenario
    metadata = json.loads((directory / "inputs.json").read_text())
    if metadata["scenario"] != scenario or metadata["fps"] != 10:
        raise ValueError(f"10fps の {scenario} 録画を指定してください")
    frames = []
    durations = []
    for index in indices:
        with Image.open(directory / f"frame-{index:04d}.png") as source:
            if source.size != (320, 240):
                raise ValueError("録画フレームは320×240が必要です")
            # 場面と再生速度を画面の外側に示し、別シナリオの切り替えを伝える。
            frame = Image.new("RGB", (320, 260), "#16191d")
            frame.paste(source, (0, 20))
            ImageDraw.Draw(frame).text((6, 3), label, fill="#dce4ed")
            frames.append(frame)
            durations.append(delay)
    durations[-1] = 800
    return frames, durations


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recordings", type=Path, required=True, help="record.py が生成した出力フォルダー")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/images/gauge-demo.gif")
    args = parser.parse_args()
    ramp, ramp_delays = load_segment(
        args.recordings, "ramp", range(0, 1001, 10), "Temperature sweep | 12.5x", 80,
    )
    warnings, warning_delays = load_segment(
        args.recordings, "warnings", range(215, 286, 2), "Oil pressure / LOW | 1x", 200,
    )
    frames = ramp + warnings
    durations = ramp_delays + warning_delays
    # 共通パレットで白・赤・文字の色を揃え、フレーム間の色のちらつきを避ける。
    palette_source = Image.new("RGB", (320, 260 * len(frames)))
    for index, frame in enumerate(frames):
        palette_source.paste(frame, (0, 260 * index))
    palette = palette_source.quantize(colors=128)
    indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    indexed[0].save(
        args.output, save_all=True, append_images=indexed[1:], duration=durations,
        loop=0, optimize=True, disposal=1,
    )
    print(f"GIF: {args.output} ({args.output.stat().st_size:,} bytes, {sum(durations) / 1000:.2f}s)")


if __name__ == "__main__":
    main()
