"""録画済みのPNGから、指定シナリオの全区間を等速GIFにする。"""

import argparse
import json
from pathlib import Path

from PIL import Image

from annotations import SIZE, annotated_frame

ROOT = Path(__file__).resolve().parent.parent


def load_segment(recordings, scenario, indices=None):
    directory = recordings / scenario
    metadata = json.loads((directory / "inputs.json").read_text())
    if metadata["scenario"] != scenario or metadata["fps"] != 10:
        raise ValueError(f"10fpsの{scenario}録画を指定してください")
    samples = metadata['frames']
    if not samples:
        raise ValueError("録画フレームがありません")
    for index, sample in enumerate(samples):
        if sample["time_ms"] != index * 100:
            raise ValueError("録画時刻が100ms間隔ではありません")
    for index in range(len(samples)) if indices is None else indices:
        sample = samples[index]
        with Image.open(directory / f"frame-{index:04d}.png") as source:
            if source.size != (320, 240):
                raise ValueError("録画フレームは320×240が必要です")
            yield annotated_frame(source, sample, scenario)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recordings", type=Path, required=True)
    parser.add_argument("--scenario", default="circuit", help="全区間を等速でGIFにするシナリオ")
    parser.add_argument("--output", type=Path, default=ROOT / "docs/images/gauge-demo.gif")
    args = parser.parse_args()
    metadata = json.loads((args.recordings / args.scenario / 'inputs.json').read_text())
    count = len(metadata['frames'])
    # パレット用のサンプルを制限し、長い等速動画でも巨大な縦長画像を作らない。
    indices = list(range(0, count, max(1, count // 12)))[:12]
    samples = list(load_segment(args.recordings, args.scenario, indices))
    palette_source = Image.new("RGB", (SIZE[0], SIZE[1] * len(samples)))
    for index, frame in enumerate(samples):
        palette_source.paste(frame, (0, SIZE[1] * index))
    palette = palette_source.quantize(colors=128)
    # RGBの全区間をメモリーへ保持せず、パレット変換も1フレームずつ行う。
    indexed = (frame.quantize(palette=palette, dither=Image.Dither.NONE)
               for frame in load_segment(args.recordings, args.scenario))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    next(indexed).save(args.output, save_all=True, append_images=indexed, duration=100,
                       loop=0, optimize=True, disposal=1)
    print(f"GIF: {args.output} ({args.output.stat().st_size:,} bytes, {count / 10:.2f}s, 1.0x)")


if __name__ == "__main__":
    main()
