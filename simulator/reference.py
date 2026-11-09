"""24FPSの模擬入力から、描画処理に依存せず期待する平滑化値と状態を求める。"""

import csv
import math
from numeric import float32
from low_warning_reference import LowWarningReference
from sensor_reference import SensorReference
from timing import FRAME_RATE, frame_time_ms


class Reference:
    def __init__(self, settings, sensors):
        self.settings = settings
        self.sensors = sensors
        self.filtered = {}
        self.samples = [0] * settings['median_size']
        self.sample_index = 0
        self.lux_sample = self.lux_median = 0
        self.last_als = 0
        self.mode = self.previous_mode = 0
        self.menu = self.racing = False
        self.racing_until = self.g_since = None
        self.warning = LowWarningReference()
        self.initialized = False

    def read_light(self, lux):
        if not self.sensors['ambient']:
            self.mode = 0
            return
        self.lux_sample = lux
        self.samples[self.sample_index] = lux
        self.sample_index = (self.sample_index + 1) % len(self.samples)
        # 偶数個の上側中央値。起動時の未取得スロットも0として扱う製品仕様。
        self.lux_median = sorted(self.samples)[len(self.samples) // 2]
        self.mode = (0 if self.lux_median >= self.settings['lux_day'] else
                     1 if self.lux_median >= self.settings['lux_dusk'] else 2)

    def step(self, sample):
        now, menu = sample['time_ms'], sample['menu']
        if not self.initialized:
            self.read_light(sample['lux'])
            self.initialized = True
        if not self.menu and not self.racing and now - self.last_als >= self.settings['als_interval_ms']:
            self.read_light(sample['lux'])
            self.last_als = now
        if menu and not self.menu:
            self.racing = False
            self.racing_until = self.g_since = None
            self.mode = 0
        if self.menu and not menu:
            self.read_light(sample['lux'])
        self.menu = menu

        if self.racing:
            if now >= self.racing_until:
                self.racing = False
                self.mode = self.previous_mode
                self.g_since = None
        elif sample['g'] >= self.settings['racing_g']:
            if self.g_since is None:
                self.g_since = now
            elif now - self.g_since >= self.settings['racing_hold_ms']:
                self.racing = True
                self.racing_until = now + self.settings['racing_duration_ms']
                self.previous_mode, self.mode = self.mode, 0
                self.g_since = None
        else:
            self.g_since = None

        low = False
        if not menu:
            for key, sensor, alpha in [('oil_c', 'oil_temp', 0.1), ('water_c', 'water_temp', 0.1),
                                       ('pressure_bar', 'pressure', self.settings['pressure_alpha'])]:
                target = float32(sample[key]) if self.sensors[sensor] else 0.0
                if key == 'pressure_bar':
                    target = min(target, 15.0)
                    if target >= 11 or sample.get('pressure_fault', False):
                        target = 0.0
                elif target >= 199:
                    target = 0.0
                previous = self.filtered.get(key, target)
                self.filtered[key] = float32(previous + float32(float32(alpha) * float32(target - previous)))
            low = self.warning.step(now, sample['g'], self.filtered['pressure_bar'], sample.get('g_direction', 'Right'))
        return {**{key: round(value, 4) for key, value in self.filtered.items()},
                'racing': self.racing, 'low': low, 'low_event': self.warning.event.copy(),
                'warning_state': self.warning.state, 'brightness_mode': self.mode,
                'brightness_pwm': self.settings['brightness'][self.mode],
                'lux_sample': self.lux_sample, 'lux_median': self.lux_median}


def add_expectations(directory, metadata):
    if metadata['fps'] != FRAME_RATE:
        raise ValueError(f'{FRAME_RATE}FPSの生センサー入力が必要です')
    frames = metadata['frames']
    reference = Reference(metadata['settings'], metadata['sensors'])
    sensor = SensorReference(metadata['sensor_settings'], metadata['sensors'])
    count = 0
    tick = -1
    with (directory / 'trace.csv').open(newline='') as source:
        for tick, row in enumerate(csv.DictReader(source)):
            now = int(row['time_ms'])
            raw = {'adc': [int(row[f'adc{channel}']) for channel in range(4)],
                   'imu': [float(row[axis]) for axis in ('ax', 'ay', 'az')], 'als': int(row['lux'])}
            if (now != frame_time_ms(tick) or not all(0 <= value <= 2047 for value in raw['adc'])
                    or not all(math.isfinite(value) for value in raw['imu'])):
                raise ValueError('期待値の計算に使う24FPS入力が欠損・不正です')
            acquired = sensor.step(now, raw)
            sample = {**acquired, 'time_ms': now, 'menu': bool(int(row['menu'])), 'lux': raw['als']}
            expected = reference.step(sample)
            expected['sensor'] = {key: round(value, 6) if isinstance(value, float) else value
                                  for key, value in acquired.items()}
            expected['voltage'] = [round(sensor.voltage(value), 6) for value in raw['adc']]
            frame = frames[tick]
            if (frame['time_ms'] != now or frame['raw']['adc'] != raw['adc'] or frame['raw']['als'] != raw['als']
                    or frame['lux'] != raw['als'] or frame['menu'] != sample['menu']
                    or any(not math.isclose(before, after, abs_tol=0.000001)
                           for before, after in zip(frame['raw']['imu'], raw['imu']))):
                raise ValueError(f'録画と24FPSの生入力が一致しません: {frame}')
            frame['expected'] = expected
            count += 1
    if count != len(frames) or tick != len(frames) - 1:
        raise ValueError('24FPS入力と録画の長さが一致しません')


def check_expectations(frame):
    expected, actual = frame['expected'], frame['actual']
    for key in ('brightness_mode', 'brightness_pwm', 'lux_sample', 'lux_median'):
        if actual[key] != expected[key]:
            raise RuntimeError(f'期待する調光状態と実際の設定が一致しません: {key}, {frame}')
    if frame['racing'] != expected['racing'] or frame['low_visible'] != expected['low']:
        raise RuntimeError(f'期待するR・LOWと実際の判定・画素が一致しません: {frame}')
    for key, tolerance in [('oil_c', 0.002), ('water_c', 0.002), ('pressure_bar', 0.0001)]:
        if not math.isclose(frame['sensor'][key], expected['sensor'][key], abs_tol=tolerance):
            raise RuntimeError(f'ADC変換・取得周期・平均化の期待値が一致しません: {key}, {frame}')
    for key in ('pressure_fault', 'adc_reads', 'adc_last'):
        if frame['sensor'][key] != expected['sensor'][key]:
            raise RuntimeError(f'取得・異常判定の期待値が一致しません: {key}, {frame}')
    if (not math.isclose(frame['g'], expected['sensor']['g'], abs_tol=0.00001)
            or frame['g_direction'] != expected['sensor']['g_direction']):
        raise RuntimeError(f'IMUの補正・水平G・方向の期待値が一致しません: {frame}')
    for key in ('g', 'pressure_bar', 'duration_s'):
        if not math.isclose(expected['low_event'][key], actual['low_event'][key], abs_tol=0.0001):
            raise RuntimeError(f'低油圧履歴の期待値が一致しません: {key}, {frame}')
    if expected['low_event']['direction'] != actual['low_event']['direction']:
        raise RuntimeError(f'低油圧履歴の方向が一致しません: {frame}')
    # 部分描画は油温0.1℃、水温0.05℃、油圧0.05bar未満の変化を保持する。
    for key, tolerance in [('oil_c', 0.1006), ('water_c', 0.0506), ('pressure_bar', 0.0506)]:
        if not math.isclose(expected[key], actual[key], abs_tol=tolerance):
            raise RuntimeError(f'平滑化の期待値と描画値の差が更新幅を超えています: {key}, {frame}')
