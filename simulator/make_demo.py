"""録画済みのPNGから、指定シナリオの全区間を等速GIFにする。"""

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent


def load_segment(recordings, scenario):
    directory = recordings / scenario
    metadata = json.loads((directory / "inputs.json").read_text())
    if metadata["scenario"] != scenario or metadata["fps"] != 10:
        raise ValueError(f"10fpsの{scenario}録画を指定してください")
    frames = []
    for index, sample in enumerate(metadata["frames"]):
        if sample["time_ms"] != index * 100:
            raise ValueError("録画時刻が100ms間隔ではありません")
        with Image.open(directory / f"frame-{index:04d}.png") as source:
            if source.size != (320, 240):
                raise ValueError("録画フレームは320×240が必要です")
            frame = Image.new("RGB", (320, 260), "#16191d")
            frame.paste(source, (0, 20))
            label = f'{scenario} | {sample["time_ms"] / 1000:.1f}s | 1.0x'
            ImageDraw.Draw(frame).text((6, 3), label, fill="#dce4ed")
            frames.append(frame)
    if not frames:
        raise ValueError("録画フレームがありません")
    return frames


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recordings", type=Path, required=True)
    parser.add_argument("--scenario", default="circuit", help="全区間を等速でGIFにするシナリオ")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/images/gauge-demo.gif")
    args = parser.parse_args()
    frames = load_segment(args.recordings, args.scenario)
    # パレット用のサンプルを制限し、長い等速動画でも巨大な縦長画像を作らない。
    samples = frames[::max(1, len(frames) // 12)][:12]
    palette_source = Image.new("RGB", (320, 260 * len(samples)))
    for index, frame in enumerate(samples):
        palette_source.paste(frame, (0, 260 * index))
    palette = palette_source.quantize(colors=128)
    indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    indexed[0].save(args.output, save_all=True, append_images=indexed[1:], duration=100,
                    loop=0, optimize=True, disposal=1)
    print(f"GIF: {args.output} ({args.output.stat().st_size:,} bytes, {len(frames) / 10:.2f}s, 1.0x)")


if __name__ == "__main__":
    main()
