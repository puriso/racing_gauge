"""生ADCステップの入力順序と、各中間値が本番処理で取得されたことを検証する。"""

from timing import FRAME_RATE

# C++の波形生成から独立した、試験仕様の範囲。
CENTERS = (
    (8, 40, 80, 120, 151, 200, 333, 800, 1300, 1616, 1633, 2039),
    (8, 80, 120, 151, 168, 250, 333, 800, 1300, 1616, 1633, 2039),
    (8, 162, 551, 940, 1199, 1400, 1555, 1616, 1633, 1800, 2000, 2039),
)


def validate_adc_steps(frames):
    if len(frames) != 200 * FRAME_RATE + 1:
        raise ValueError('ADCステップの録画が200秒分そろっていません')
    warmup, hold, steps = 2 * FRAME_RATE, FRAME_RATE // 2, 33
    for frame in frames[:warmup]:
        if frame['raw']['adc'] != [151, 168, 551, 0]:
            raise ValueError('IMU補正中のADC入力が一定ではありません')
    for window in range(12):
        for step in range(steps):
            start = warmup + (window * steps + step) * hold
            end = start + hold if window * steps + step < 12 * steps - 1 else len(frames)
            offset = step if step <= 16 else 32 - step
            wanted = [centers[window] - 8 + offset for centers in CENTERS] + [0]
            samples = frames[start:end]
            if any(frame['raw']['adc'] != wanted for frame in samples):
                raise ValueError(f'生ADCが1カウント刻み・500ms保持になっていません: {window}, {step}')
            before, after = frames[start - 1]['sensor'], samples[-1]['sensor']
            for channel in range(3):
                if (after['adc_reads'][channel] <= before['adc_reads'][channel]
                        or after['adc_last'][channel] != wanted[channel]):
                    raise ValueError(f'中間カウントが実際に取得されていません: CH{channel}, {wanted[channel]}')
