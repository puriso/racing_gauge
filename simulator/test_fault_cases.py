"""断線・温度異常・復帰の入力と、取得前の換算値を確認する。"""

import unittest

from fault_cases import CASES, TEMPERATURE_SENSOR_SPEC, outside_temperature_spec, fault_case, validate_fault_inputs
from sensor_reference import SensorReference
from timing import frame_time_ms


SETTINGS = dict(adc_full_scale_v=6.144, adc_max=2047, supply_v=5, voltage_drop_v=0.137,
                thermistor_r25=10000, thermistor_beta=3380, reference_k=298.16, series_resistance=10000,
                temp_interval_ms=500, pressure_samples=5, oil_samples=2, water_samples=2,
                oil_channel=0, water_channel=1, pressure_channel=2)
ENABLED = dict(oil_temp=1, water_temp=1, pressure=1, ambient=1)


def fixture(scenario):
    frames = []
    model = SensorReference(SETTINGS, ENABLED)
    for tick in range(961):
        now = frame_time_ms(tick)
        phase = (now % 4000) / 2000
        pressure = 3 + 3 * (phase if phase <= 1 else 2 - phase)
        adc = [151, 168, round((0.5 + pressure / 2.5) / model.correction * 2047 / 6.144), 0]
        counts, _, _ = fault_case(scenario, now)
        for channel, value in enumerate(counts):
            if value is not None:
                adc[channel] = value
        frames.append(dict(time_ms=now, raw=dict(adc=adc)))
    return dict(scenario=scenario, frames=frames, sensor_settings=SETTINGS, sensors=ENABLED)


class FaultCasesTest(unittest.TestCase):
    def test_each_fault_case_and_recovery_is_present(self):
        for scenario in CASES:
            data = fixture(scenario)
            validate_fault_inputs(data)
            self.assertEqual(data['frames'][-1]['case'], 'healthy')

    def test_open_thermistors_and_pressure_zero_are_independent(self):
        data = fixture('sensor-disconnect')
        validate_fault_inputs(data)
        self.assertEqual(data['frames'][6 * 24]['raw']['adc'][:2], [1666, 168])
        self.assertEqual(data['frames'][14 * 24]['raw']['adc'][:2], [151, 1666])
        self.assertEqual(data['frames'][22 * 24]['raw']['adc'][:3], [151, 168, 0])
        self.assertEqual(data['frames'][30 * 24]['raw']['adc'][:3], [1666, 1666, 0])

    def test_high_temperature_crosses_200_from_raw_adc(self):
        data = fixture('temperature-high')
        validate_fault_inputs(data)
        self.assertLess(data['frames'][7 * 24]['input_temperature_c'][0], 200)
        self.assertGreaterEqual(data['frames'][10 * 24]['input_temperature_c'][0], 200)
        self.assertGreaterEqual(data['frames'][22 * 24]['input_temperature_c'][1], 200)
        self.assertTrue(all(value >= 500 for value in data['frames'][30 * 24]['input_temperature_c']))

    def test_low_temperature_crosses_sensor_operating_minimum_from_raw_adc(self):
        data = fixture('temperature-low')
        validate_fault_inputs(data)
        self.assertGreater(data['frames'][7 * 24]['input_temperature_c'][0], -20)
        self.assertLess(data['frames'][10 * 24]['input_temperature_c'][0], -20)
        self.assertLess(data['frames'][22 * 24]['input_temperature_c'][1], -20)
        self.assertEqual(data['frames'][30 * 24]['input_temperature_abnormal'], [True, True])

    def test_operating_limits_are_inclusive_and_outside_values_are_flagged(self):
        self.assertEqual(TEMPERATURE_SENSOR_SPEC['model'], 'Defi PDF00903S')
        self.assertFalse(outside_temperature_spec(-20))
        self.assertFalse(outside_temperature_spec(150))
        self.assertTrue(outside_temperature_spec(-20.001))
        self.assertTrue(outside_temperature_spec(150.001))
        self.assertTrue(outside_temperature_spec(float('nan')))
        self.assertTrue(outside_temperature_spec(float('inf')))

    def test_rejects_fault_not_injected(self):
        data = fixture('sensor-disconnect')
        data['frames'][6 * 24]['raw']['adc'][0] = 151
        with self.assertRaisesRegex(ValueError, '生ADC'):
            validate_fault_inputs(data)

    def test_rejects_failure_to_reconnect(self):
        data = fixture('temperature-low')
        data['frames'][-1]['raw']['adc'][0] = 1430
        with self.assertRaisesRegex(ValueError, '復帰'):
            validate_fault_inputs(data)

    def test_rejects_stalled_normal_pressure(self):
        data = fixture('temperature-high')
        data['frames'][6 * 24]['raw']['adc'][2] = 551
        with self.assertRaisesRegex(ValueError, '通常油圧'):
            validate_fault_inputs(data)

    def test_rejects_truncated_recording(self):
        data = fixture('temperature-high')
        data['frames'].pop()
        with self.assertRaisesRegex(ValueError, '40秒'):
            validate_fault_inputs(data)


if __name__ == '__main__':
    unittest.main()
