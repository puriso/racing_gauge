"""ADC刺激の仕様と、画面外に表示する断線・異常温度のケース情報。"""

import math

from sensor_reference import SensorReference
from timing import FRAME_RATE


TEMPERATURE_SENSOR_SPEC = dict(
    model='Defi PDF00903S', minimum_c=-20.0, maximum_c=150.0, range_kind='operating',
    source_url='https://defi.nippon-seiki.co.jp/wp-content/uploads/2025/03/manual_din.pdf',
)


def outside_temperature_spec(value):
    return (not math.isfinite(value) or value < TEMPERATURE_SENSOR_SPEC['minimum_c']
            or value > TEMPERATURE_SENSOR_SPEC['maximum_c'])


CASES = {
    'sensor-disconnect': [
        (4000, 8000, (1666, None, None), 'oil-open', '油温センサー断線相当（入力5V）'),
        (12000, 16000, (None, 1666, None), 'water-open', '水温センサー断線相当（入力5V）'),
        (20000, 24000, (None, None, 0), 'pressure-zero', '油圧センサー信号なし相当（入力0V）'),
        (28000, 32000, (1666, 1666, 0), 'all-disconnected', '温度2本断線・油圧信号0V'),
    ],
    'temperature-high': [
        (4000, 8000, (25, None, None), 'oil-below-200', '油温：200℃の直前（約197.57℃）'),
        (8000, 12000, (24, None, None), 'oil-above-200', '油温：200℃以上（約200.30℃）'),
        (16000, 20000, (None, 25, None), 'water-below-200', '水温：200℃の直前（約197.57℃）'),
        (20000, 24000, (None, 24, None), 'water-above-200', '水温：200℃以上（約200.30℃）'),
        (28000, 32000, (1, 1, None), 'both-over-500', '油温・水温：極端な高温（約583.29℃）'),
    ],
    'temperature-low': [
        (4000, 8000, (1429, None, None), 'oil-inside-limit', '油温：動作下限-20℃の範囲内側（約-19.93℃）'),
        (8000, 12000, (1430, None, None), 'oil-outside-limit', '油温：動作下限-20℃未満（約-20.04℃）'),
        (16000, 20000, (None, 1429, None), 'water-inside-limit', '水温：動作下限-20℃の範囲内側（約-19.93℃）'),
        (20000, 24000, (None, 1430, None), 'water-outside-limit', '水温：動作下限-20℃未満（約-20.04℃）'),
        (28000, 32000, (1430, 1430, None), 'both-outside-limit', '油温・水温：両方が動作下限未満（約-20.04℃）'),
    ],
}


def fault_case(scenario, now):
    for begin, end, counts, name, label in CASES.get(scenario, []):
        if begin <= now < end:
            return counts, name, label
    return (None, None, None), 'healthy', '正常入力・復帰'


def validate_fault_inputs(metadata):
    frames = metadata['frames']
    if len(frames) != 40 * FRAME_RATE + 1:
        raise ValueError('断線・異常温度の録画が40秒分そろっていません')
    sensor = SensorReference(metadata['sensor_settings'], metadata['sensors'])
    scenario = metadata['scenario']
    metadata['temperature_sensor_spec'] = TEMPERATURE_SENSOR_SPEC.copy()
    seen = set()
    for frame in frames:
        counts, name, label = fault_case(scenario, frame['time_ms'])
        seen.add(name)
        for channel, count in enumerate(counts):
            if count is not None and frame['raw']['adc'][channel] != count:
                raise ValueError(f'断線・異常温度の生ADCが不正です: {name}, CH{channel}')
        # 未指定チャンネルは健康な温度入力と3〜6barの通常油圧を継続する。
        if counts[0] is None and frame['raw']['adc'][0] != 151:
            raise ValueError('油温の正常入力・復帰がありません')
        if counts[1] is None and frame['raw']['adc'][1] != 168:
            raise ValueError('水温の正常入力・復帰がありません')
        if counts[2] is None:
            phase = (frame['time_ms'] % 4000) / 2000
            pressure = 3 + 3 * (phase if phase <= 1 else 2 - phase)
            voltage = (0.5 + pressure / 2.5) / sensor.correction
            expected = round(voltage * metadata['sensor_settings']['adc_max'] / metadata['sensor_settings']['adc_full_scale_v'])
            if abs(frame['raw']['adc'][2] - expected) > 1:
                raise ValueError('通常油圧が3〜6barで動いていません')
        converted = [sensor.temperature(sensor.voltage(frame['raw']['adc'][channel])) for channel in (0, 1)]
        for channel in (0, 1):
            value = converted[channel]
            if counts[channel] is not None:
                if name.endswith('below-200') and not value < 200:
                    raise ValueError('200℃直前の入力がありません')
                if name.endswith('above-200') and not value >= 200:
                    raise ValueError('200℃以上の入力がありません')
                if name.endswith('outside-limit') and not value < TEMPERATURE_SENSOR_SPEC['minimum_c']:
                    raise ValueError('センサー動作下限未満の入力がありません')
                if name.endswith('inside-limit') and not value >= TEMPERATURE_SENSOR_SPEC['minimum_c']:
                    raise ValueError('センサー動作下限の範囲内入力がありません')
            if not math.isfinite(value):
                raise ValueError('温度換算値が有限値ではありません')
        frame['case'] = name
        frame['case_label'] = label
        frame['input_temperature_c'] = converted
        frame['input_temperature_abnormal'] = [outside_temperature_spec(value) for value in converted]
    if seen != {'healthy', *(entry[3] for entry in CASES[scenario])}:
        raise ValueError('断線・異常温度・復帰のケースが不足しています')
