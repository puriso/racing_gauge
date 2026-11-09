"""ADCカウントとIMU各軸から、本番センサー取得処理の期待値を独立計算する。"""

import math

from numeric import float32 as f32


class SensorReference:
    def __init__(self, settings, enabled):
        self.settings = settings
        self.enabled = enabled
        self.correction = f32(f32(settings['supply_v']) /
                              f32(f32(settings['supply_v']) - f32(settings['voltage_drop_v'])))
        self.buffers = {key: [0.0] * settings[size] for key, size in
                        [('oil_c', 'oil_samples'), ('water_c', 'water_samples'), ('pressure_bar', 'pressure_samples')]}
        self.indices = dict.fromkeys(self.buffers, 0)
        self.first = {'oil_c': True, 'water_c': True}
        self.last_temp = {'oil_c': 0, 'water_c': 0}
        self.adc_reads = [0] * 4
        self.adc_last = [-1] * 4
        self.imu_start = 0
        self.offset_count = 0
        self.offset = [0.0] * 3
        self.imu_ready = False
        self.g = 0.0
        self.direction = 'Right'
        self.pressure_fault = False

    def voltage(self, raw):
        return f32(f32(raw * f32(self.settings['adc_full_scale_v'])) / self.settings['adc_max'])

    def pressure(self, voltage):
        if voltage >= f32(4.9):
            return 0.0
        corrected = f32(voltage * self.correction)
        return f32(f32(2.5) * f32(corrected - f32(0.5))) if corrected > f32(0.5) else 0.0

    def temperature(self, voltage):
        corrected = f32(voltage * self.correction)
        supply = f32(self.settings['supply_v'])
        if corrected <= 0 or corrected >= supply:
            return 200.0
        resistance = f32(f32(self.settings['series_resistance']) * f32(corrected / f32(supply - corrected)))
        denominator = math.log(f32(resistance / f32(self.settings['thermistor_r25']))) + f32(
            f32(self.settings['thermistor_beta']) / f32(self.settings['reference_k']))
        kelvin = f32(f32(self.settings['thermistor_beta']) / denominator)
        return f32(kelvin - f32(273.16))

    def read(self, adc, channel):
        # ダミー変換とセトリング後の本取得を、それぞれ1回として検証する。
        self.adc_reads[channel] += 2
        self.adc_last[channel] = adc[channel]
        return self.voltage(adc[channel])

    def update_imu(self, now, axes):
        if not self.imu_ready:
            if self.imu_start == 0:
                self.imu_start = now
            if now - self.imu_start < 200:
                return False
            self.offset = [f32(before + value) for before, value in zip(self.offset, axes)]
            self.offset_count += 1
            if self.offset_count < 20:
                return False
            self.offset = [f32(value / self.offset_count) for value in self.offset]
            self.imu_ready = True
        longitudinal = f32(axes[0] - self.offset[0])
        lateral = f32(axes[1] - self.offset[1])
        self.g = f32(math.sqrt(f32(f32(lateral * lateral) + f32(longitudinal * longitudinal))))
        if abs(lateral) <= f32(abs(longitudinal) * f32(0.17632698)):
            self.direction = 'Front' if longitudinal >= 0 else 'Rear'
        elif abs(longitudinal) <= f32(abs(lateral) * f32(0.17632698)):
            self.direction = 'Right' if lateral >= 0 else 'Left'
        else:
            self.direction = ('F' if longitudinal >= 0 else 'R') + ('R' if lateral >= 0 else 'L')
        return True

    def step(self, now, raw):
        adc, axes = raw['adc'], [f32(value) for value in raw['imu']]
        if self.update_imu(now, axes):
            voltage = self.read(adc, self.settings['pressure_channel']) if self.enabled['pressure'] else 0.0
            self.pressure_fault = voltage >= f32(4.9) if self.enabled['pressure'] else False
            pressure = self.pressure(voltage) if self.enabled['pressure'] else 0.0
            self.buffers['pressure_bar'][self.indices['pressure_bar']] = pressure
            self.indices['pressure_bar'] = (self.indices['pressure_bar'] + 1) % len(self.buffers['pressure_bar'])
            for key, sensor, channel in [('oil_c', 'oil_temp', 'oil_channel'), ('water_c', 'water_temp', 'water_channel')]:
                if now - self.last_temp[key] < self.settings['temp_interval_ms']:
                    continue
                value = self.temperature(self.read(adc, self.settings[channel])) if self.enabled[sensor] else 0.0
                if self.first[key]:
                    self.buffers[key] = [value] * len(self.buffers[key])
                    self.first[key] = False
                else:
                    self.buffers[key][self.indices[key]] = value
                    self.indices[key] = (self.indices[key] + 1) % len(self.buffers[key])
                self.last_temp[key] = now
        averages = {}
        for key, values in self.buffers.items():
            total = 0.0
            for value in values:
                total = f32(total + value)
            averages[key] = f32(total / len(values))
        return {**averages, 'g': self.g, 'g_direction': self.direction, 'pressure_fault': self.pressure_fault,
                'adc_reads': self.adc_reads.copy(), 'adc_last': self.adc_last.copy()}
