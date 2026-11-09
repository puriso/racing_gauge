"""動画とGIFで共用する、メーター領域の外側の入力・期待値パネル。"""

from PIL import Image, ImageDraw, ImageFont
from timing import FRAME_RATE

SIZE = (640, 540)
SCREEN_ORIGIN = (16, 44)
MODES = ('DAY', 'DUSK', 'NIGHT')
NORMAL_FONT = ImageFont.load_default(size=15)
SMALL_FONT = ImageFont.load_default(size=12)


def annotated_frame(source, sample, scenario, sensor_settings):
    if source.size != (320, 240):
        raise ValueError('メーターの原画像は320×240が必要です')
    expected, actual = sample['expected'], sample['actual']
    frame = Image.new('RGB', SIZE, '#10151b')
    frame.paste(source, SCREEN_ORIGIN)
    draw = ImageDraw.Draw(frame)
    normal, small = NORMAL_FONT, SMALL_FONT

    def label(x, y, text, color='#e9edf3', font=normal):
        draw.text((x, y), text, fill=color, font=font)

    label(16, 12, f'{scenario} | {sample["time_ms"] / 1000:.3f}s | {FRAME_RATE}FPS | 1.0x')
    label(352, 48, 'STATE', '#a7dfb4')
    label(466, 48, 'EXPECT', '#a7dfb4')
    label(552, 48, 'ACTUAL', '#8fc5ff')
    rows = [
        ('R mode', 'ON' if expected['racing'] else 'OFF', 'ON' if sample['racing'] else 'OFF'),
        ('LOW', 'ON' if expected['low'] else 'OFF', 'ON' if sample['low_visible'] else 'OFF'),
        ('Light mode', MODES[expected['brightness_mode']], MODES[actual['brightness_mode']]),
        ('PWM / 255', expected['brightness_pwm'], actual['brightness_pwm']),
        ('ALS sample', expected['lux_sample'], actual['lux_sample']),
        ('ALS median', expected['lux_median'], actual['lux_median']),
        ('Pressure OV', 'YES' if expected['sensor']['pressure_fault'] else 'NO',
         'YES' if sample['sensor']['pressure_fault'] else 'NO'),
    ]
    for index, (name, wanted, observed) in enumerate(rows):
        y = 78 + index * 28
        label(352, y, name, font=small)
        label(466, y, str(wanted), '#a7dfb4')
        label(552, y, str(observed), '#8fc5ff')
    label(352, 285, 'MENU' if sample['menu'] else 'GAUGES', '#dfcc9d')
    label(16, 297, 'Production screen 320 x 240 (original pixels)', font=small)
    draw.line((16, 322, 624, 322), fill='#354353')
    label(16, 329, 'SIGNAL')
    label(188, 329, 'RAW SENSOR / VOLTAGE', '#8fc5ff')
    label(386, 329, 'EXPECTED AVG / FILTER', '#a7dfb4')
    signals = []
    for name, key, channel_key, unit in [('Oil temp', 'oil_c', 'oil_channel', 'C'),
                                         ('Water temp', 'water_c', 'water_channel', 'C'),
                                         ('Oil pressure', 'pressure_bar', 'pressure_channel', 'bar')]:
        channel = sensor_settings[channel_key]
        raw = f'CH{channel} {sample["raw"]["adc"][channel]} / {expected["voltage"][channel]:.3f}V'
        target = f'{expected["sensor"][key]:.2f} / {expected[key]:.2f} {unit}'
        signals.append((name, raw, target))
    axes = '/'.join(f'{axis:+.2f}' for axis in sample['raw']['imu'])
    signals.extend([
        ('IMU x/y/z (g)', axes, f'{expected["sensor"]["g"]:.2f} G / {expected["sensor"]["g_direction"]}'),
        ('Ambient light', f'{sample["lux"]} lux', f'{MODES[expected["brightness_mode"]]} / PWM {expected["brightness_pwm"]}'),
    ])
    for index, (name, raw, target) in enumerate(signals):
        y = 354 + index * 24
        label(16, y, name)
        label(188, y, raw, '#8fc5ff')
        label(386, y, target, '#a7dfb4')
    label(16, 482, 'RAW ADC -> calibration -> timed sampling / averages -> filter. IMU axes in g.', font=small)
    # 最下部は動画の操作バー用。R表示や比較値に重ならないよう余白を残す。
    return frame
