"""動画とGIFで共用する、メーター領域の外側の入力・期待値パネル。"""

from PIL import Image, ImageDraw, ImageFont

SIZE = (640, 540)
SCREEN_ORIGIN = (16, 44)
MODES = ('DAY', 'DUSK', 'NIGHT')


def annotated_frame(source, sample, scenario):
    if source.size != (320, 240):
        raise ValueError('メーターの原画像は320×240が必要です')
    expected, actual = sample['expected'], sample['actual']
    frame = Image.new('RGB', SIZE, '#10151b')
    frame.paste(source, SCREEN_ORIGIN)
    draw = ImageDraw.Draw(frame)
    normal = ImageFont.load_default(size=15)
    small = ImageFont.load_default(size=12)

    def label(x, y, text, color='#e9edf3', font=normal):
        draw.text((x, y), text, fill=color, font=font)

    label(16, 12, f'{scenario} | {sample["time_ms"] / 1000:.1f}s | 1.0x')
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
    ]
    for index, (name, wanted, observed) in enumerate(rows):
        y = 78 + index * 28
        label(352, y, name, font=small)
        label(466, y, str(wanted), '#a7dfb4')
        label(552, y, str(observed), '#8fc5ff')
    label(352, 257, 'MENU' if sample['menu'] else 'GAUGES', '#dfcc9d')
    label(16, 297, 'Production screen 320 x 240 (original pixels)', font=small)
    draw.line((16, 322, 624, 322), fill='#354353')
    label(16, 329, 'SIGNAL')
    label(204, 329, 'RAW INPUT', '#8fc5ff')
    label(386, 329, 'EXPECTED FILTER / TARGET', '#a7dfb4')
    signals = [
        ('Oil temp', f'{sample["oil_c"]:.2f} C', f'{expected["oil_c"]:.2f} C'),
        ('Water temp', f'{sample["water_c"]:.2f} C', f'{expected["water_c"]:.2f} C'),
        ('Oil pressure', f'{sample["pressure_bar"]:.2f} bar', f'{expected["pressure_bar"]:.2f} bar'),
        ('G / direction', f'{sample["g"]:.2f} G / {sample["g_direction"]}', 'R >= 1.0 G for 100 ms'),
        ('Ambient light', f'{sample["lux"]} lux', f'{MODES[expected["brightness_mode"]]} / PWM {expected["brightness_pwm"]}'),
    ]
    for index, (name, raw, target) in enumerate(signals):
        y = 354 + index * 24
        label(16, y, name)
        label(204, y, raw, '#8fc5ff')
        label(386, y, target, '#a7dfb4')
    label(16, 482, 'RAW = simulated physical input, before smoothing. ALS = sampled light.', font=small)
    # 最下部は動画の操作バー用。R表示や比較値に重ならないよう余白を残す。
    return frame
