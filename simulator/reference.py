"""24FPSの模擬入力から、描画処理に依存せず期待する平滑化値と状態を求める。"""

import csv
import math
from numeric import float32
from low_warning_reference import LowWarningReference
from sensor_reference import SensorReference
from timing import FRAME_RATE, frame_time_ms
from risk_reference import RiskReference


class Reference:
    def __init__(self, settings, sensors):
        self.settings = settings
        self.sensors = sensors
        self.filtered = {}
        self.monitored = {}
        self.maxima = [None, None, None]
        self.samples = [0] * settings['median_size']
        self.sample_index = 0
        self.lux_sample = self.lux_median = 0
        self.last_als = 0
        self.mode = self.previous_mode = 0
        self.menu = self.racing = False
        self.racing_until = self.g_since = None
        self.warning = LowWarningReference()
        self.risk = RiskReference()
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
        if sample.get('setup_error'):
            return self.snapshot(False)
        if not self.menu and not self.racing and now - self.last_als >= self.settings['als_interval_ms']:
            self.read_light(sample['lux'])
            self.last_als = now
        if menu and not self.menu:
            self.racing = False
            self.racing_until = self.g_since = None
            self.mode = 0
        if self.menu and not menu:
            self.warning.history = False
            self.read_light(sample['lux'])
        self.menu = menu

        if menu:
            self.racing = False
            self.g_since = None
        elif self.racing:
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

        for index, (key, sensor, alpha) in enumerate([
                ('oil_c', 'oil_temp', 0.1), ('water_c', 'water_temp', 0.1),
                ('pressure_bar', 'pressure', self.settings['pressure_alpha'])]):
            target = float32(sample[key]) if self.sensors[sensor] else 0.0
            valid = bool(self.sensors[sensor]) and sample.get('status', [1, 1, 1])[index] == 1
            if key == 'pressure_bar':
                target = min(target, 15.0)
                valid = valid and target < 11 and not sample.get('pressure_fault', False)
            else:
                valid = valid and math.isfinite(target) and target < 199
            if valid:
                candidate = int(target) if index == 0 else target
                self.maxima[index] = candidate if self.maxima[index] is None else max(self.maxima[index], candidate)
                previous = self.filtered.get(key, target)
                self.filtered[key] = float32(previous + float32(float32(alpha) * float32(target - previous)))
                self.monitored[key] = self.filtered[key]
            else:
                self.filtered.pop(key, None)
                self.monitored[key] = 0.0
        pressure_valid = sample.get('status', [1, 1, 1])[2] == 1 and sample['pressure_bar'] < 11 and not sample.get('pressure_fault', False)
        low = self.warning.step(now + sample.get('adc_wait_ms', 0), sample['g'] if pressure_valid else 0, self.monitored['pressure_bar'], sample.get('g_direction', 'Right'))
        result = self.snapshot(low and not menu)
        if sample.get('risk_scenario'):
            status = sample['status']
            result['risk'] = self.risk.step(now + sample.get('adc_wait_ms', 0), self.monitored['pressure_bar'], sample['g'], self.monitored['oil_c'],
                                           pressure_valid, status[0] == 1 and math.isfinite(sample['oil_c']) and sample['oil_c'] < 199,
                                           self.warning.qualified)
        return result

    def snapshot(self, low):
        return {**{key: round(self.monitored[key], 4) if key in self.monitored else None
                   for key in ('oil_c', 'water_c', 'pressure_bar')},
                'monitor': [round(self.monitored.get(key, 0), 4) for key in ('oil_c', 'water_c', 'pressure_bar')],
                'maxima': self.maxima.copy(), 'racing': self.racing, 'low': low, 'warning_history': self.warning.history, 'low_event': self.warning.event.copy(),
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
                   'imu': [float(row[axis]) for axis in ('ax', 'ay', 'az')], 'als': int(row['lux']), 'adc_fault': int(row['adc_fault'])}
            if (now != frame_time_ms(tick) or not all(0 <= value <= 2047 for value in raw['adc'])
                    or not all(math.isfinite(value) for value in raw['imu'])):
                raise ValueError('期待値の計算に使う24FPS入力が欠損・不正です')
            setup_error = bool(int(row['setup_error']))
            if setup_error:
                sensor.bus.frame(now, raw['adc_fault'])
                acquired = sensor.snapshot()
            else:
                acquired = sensor.step(now, raw)
            sample = {**acquired, 'time_ms': now, 'menu': bool(int(row['menu'])), 'lux': raw['als'], 'setup_error': setup_error, 'risk_scenario': metadata['scenario'].startswith('risk-')}
            expected = reference.step(sample)
            expected['sensor'] = {key: round(value, 6) if isinstance(value, float) else value
                                  for key, value in acquired.items()}
            expected['voltage'] = [round(sensor.voltage(value), 6) for value in raw['adc']]
            frame = frames[tick]
            if (frame['screen'] != int(row['screen']) or frame['setup_error'] != setup_error or frame['time_ms'] != now or frame['raw']['adc'] != raw['adc'] or frame['raw']['als'] != raw['als']
                    or frame['raw']['adc_fault'] != raw['adc_fault']
                    or frame['lux'] != raw['als'] or frame['menu'] != sample['menu']
                    or any(not math.isclose(before, after, abs_tol=0.000001)
                           for before, after in zip(frame['raw']['imu'], raw['imu']))):
                raise ValueError(f'録画と24FPSの生入力が一致しません: {frame}')
            frame['expected'] = expected
            count += 1
    if 'risk_window' in metadata and metadata['risk_window'] != reference.risk.window:
        raise ValueError('危険点の前後1分データが独立モデルと一致しません')
    if count != len(frames) or tick != len(frames) - 1:
        raise ValueError('24FPS入力と録画の長さが一致しません')


def check_expectations(frame):
    expected, actual = frame['expected'], frame['actual']
    if frame['setup_error_visible'] != frame['setup_error']:
        raise RuntimeError('初期化失敗画面の画素と録画フェーズが一致しません')
    for key in ('brightness_mode', 'brightness_pwm', 'lux_sample', 'lux_median'):
        if actual[key] != expected[key]:
            raise RuntimeError(f'期待する調光状態と実際の設定が一致しません: {key}, {frame}')
    if frame['racing'] != expected['racing'] or frame['low_visible'] != expected['low']:
        raise RuntimeError(f'期待するR・LOWと実際の判定・画素が一致しません: {frame}')
    if actual['warning_history'] != expected['warning_history'] or frame['w_visible'] != (expected['warning_history'] and not frame['menu']):
        raise RuntimeError(f'W履歴の保持・消去・画素が一致しません: {frame}')
    for key, tolerance in [('oil_c', 0.002), ('water_c', 0.002), ('pressure_bar', 0.0001)]:
        if not math.isclose(frame['sensor'][key], expected['sensor'][key], abs_tol=tolerance):
            raise RuntimeError(f'ADC変換・取得周期・平均化の期待値が一致しません: {key}, {frame}')
    for key in ('status', 'pressure_fault', 'adc_reads', 'adc_last', 'adc_connected', 'adc_begin_calls', 'adc_wait_ms'):
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
        if frame['setup_error']:
            if expected[key] is not None or actual[key] is not None:
                raise RuntimeError('初回loop前のゲージ値は未描画です')
        elif not frame['menu'] and not math.isclose(expected[key], actual[key], abs_tol=tolerance):
            raise RuntimeError(f'平滑化の期待値と描画値の差が更新幅を超えています: {key}, {frame}')

    for key, tolerance in [('monitor', 0.002), ('maxima', 0.002)]:
        for wanted, observed in zip(expected[key], actual[key]):
            if (wanted is None or observed is None):
                if wanted is not observed:
                    raise RuntimeError(f'最高値の未取得状態が一致しません: {key}, {frame}')
            elif not math.isclose(wanted, observed, abs_tol=tolerance):
                raise RuntimeError(f'監視値・最高値が一致しません: {key}, {frame}')

    if 'risk' in frame:
        for key, observed in frame['risk'].items():
            wanted = expected['risk'][key]
            if wanted is None or observed is None:
                correct = wanted is observed
            elif isinstance(wanted, float):
                correct = math.isclose(wanted, observed, abs_tol=0.002)
            else:
                correct = wanted == observed
            if not correct:
                raise RuntimeError(f'危険場面の選択・経過時間が一致しません: {key}, {frame}')
