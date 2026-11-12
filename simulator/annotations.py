"""動画とGIFで共用する、メーター領域の外側の入力・期待値パネル。"""

from PIL import Image, ImageDraw, ImageFont
from timing import FRAME_RATE
from fault_cases import TEMPERATURE_SENSOR_SPEC

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

    heading = f'{scenario} | {sample["time_ms"] / 1000:.3f}s | {FRAME_RATE}FPS | 1.0x'
    if 'input_temperature_c' in sample:
        spec = TEMPERATURE_SENSOR_SPEC
        heading += f" | PDF00903S op {spec['minimum_c']:g}..{spec['maximum_c']:g}C"
    if sample.get('setup_error'):
        heading += ' | SETUP / before first loop'
    label(16, 12, heading)
    label(352, 48, 'STATE', '#a7dfb4')
    label(466, 48, 'EXPECT', '#a7dfb4')
    label(552, 48, 'ACTUAL', '#8fc5ff')
    rows = [
        ('R mode', 'ON' if expected['racing'] else 'OFF', 'ON' if sample['racing'] else 'OFF'),
        ('LOW', 'ON' if expected['low'] else 'OFF', 'ON' if sample['low_visible'] else 'OFF'),
        ('W history', 'ON' if expected['warning_history'] else 'OFF', 'ON' if actual['warning_history'] else 'OFF'),
        ('Light mode', MODES[expected['brightness_mode']], MODES[actual['brightness_mode']]),
        ('PWM / 255', expected['brightness_pwm'], actual['brightness_pwm']),
        ('ALS sample', expected['lux_sample'], actual['lux_sample']),
        ('ALS median', expected['lux_median'], actual['lux_median']),
        ('Pressure OV', 'YES' if expected['sensor']['pressure_fault'] else 'NO',
         'YES' if sample['sensor']['pressure_fault'] else 'NO'),
    ]
    if scenario in ("adc-startup", "adc-communication"):
        rows.append(("ADC link", "UP" if expected["sensor"]["adc_connected"] else "DOWN", "UP" if sample["sensor"]["adc_connected"] else "DOWN"))
    for index, (name, wanted, observed) in enumerate(rows):
        y = 78 + index * 22
        label(352, y, name, font=small)
        label(466, y, str(wanted), '#a7dfb4')
        label(552, y, str(observed), '#8fc5ff')
    if not scenario.startswith('warning-'):
        label(352, 285, sample.get('case', 'SETUP ERROR' if sample.get('setup_error') else 'MENU' if sample['menu'] else 'GAUGES'), '#dfcc9d', font=small)
    if scenario.startswith('warning-'):
        label(352, 285, 'PR380: ' + expected['warning_state'], '#dfcc9d', font=small)
        observed, wanted = actual['low_event'], expected['low_event']
        label(16, 306, f"LAST EVENT act/exp: G {observed['g']:.2f}/{wanted['g']:.2f} | "
              f"min {observed['pressure_bar']:.2f}/{wanted['pressure_bar']:.2f}bar | "
              f"time {observed['duration_s']:.2f}/{wanted['duration_s']:.2f}s | "
              f"dir {observed['direction']}/{wanted['direction']}", font=small)
    elif 'risk' in sample:
        event = sample['risk']
        name = ('NONE', 'DROP(est)', 'NEAR(est)', 'LOW')[event['kind']]
        label(16, 297, f"RISK {name} | age {event['age_ms']/1000:.1f}s | pre/post {event['pre_ms']/1000:g}/{event['post_ms']/1000:g}s", font=small)
        direction = ('-', 'F', 'R', 'L', 'B')[event['g_direction']]
        label(16, 309, f'0s direction {direction} | Peak letters: F front / R right / L left / B back.', font=small)
    elif scenario in ('adc-startup', 'adc-communication'):
        modes = ('OK', 'NO DEVICE', 'BUSY', 'CONFIG WRITE FAIL', 'STATUS READ FAIL', 'RESULT READ FAIL', 'SECOND RESULT FAIL')
        state = sample['sensor']
        label(16, 297, f"I2C {modes[sample['raw']['adc_fault']]} | begin attempts {state['adc_begin_calls']} | virtual poll wait {state['adc_wait_ms']}ms", font=small)
        label(16, 309, f"LAST ADC {state['adc_last'][:3]} | successful conversion reads {state['adc_reads'][:3]}", font=small)
    elif 'input_temperature_c' in sample:
        oil, water = sample['input_temperature_c']
        label(16, 297, f'INPUT C (pre-avg): {oil:.2f} / {water:.2f}', font=small)
    else:
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
        filtered = 'not drawn' if expected[key] is None else f'{expected[key]:.2f} {unit}'
        target = f'{expected["sensor"][key]:.2f} / {filtered}'
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
    names = ('WAIT', 'OK', 'COMM ERR', 'INVALID', 'OFF')
    statuses = '/'.join(names[value] for value in sample['sensor']['status'])
    label(16, 482, f'Sensor O/W/P: {statuses}', font=small)
    observed, wanted = actual['maxima'], expected['maxima']
    def peak(value, digits):
        return '---' if value is None else f'{value:.{digits}f}'
    label(16, 506, f'MAX act/exp: oil {peak(observed[0], 0)}/{peak(wanted[0], 0)}C | water {peak(observed[1], 1)}/{peak(wanted[1], 1)}C | P {peak(observed[2], 2)}/{peak(wanted[2], 2)}bar', font=small)
    # 最下部は動画の操作バー用。R表示や比較値に重ならないよう余白を残す。
    return frame
