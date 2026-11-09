"""ADC自体が毎フレーム1カウントずつ増減していることを検証する。"""

from timing import FRAME_RATE


def validate_adc_steps(frames):
    if len(frames) != 173 * FRAME_RATE + 1:
        raise ValueError('ADCステップの録画が173秒分そろっていません')
    warmup = 2 * FRAME_RATE
    for frame in frames[:warmup]:
        if frame['raw']['adc'] != [151, 168, 551, 0]:
            raise ValueError('IMU補正中のADC入力が一定ではありません')
    for index, frame in enumerate(frames[warmup:]):
        phase = index % 4094
        value = phase if phase <= 2047 else 4094 - phase
        if frame['raw']['adc'] != [value, value, value, 0]:
            raise ValueError(f'生ADCが毎フレーム1カウントずつ増減していません: {index}')
