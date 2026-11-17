"""既知のADC値と時刻境界で、生値からの変換・取得・補正の意味を検証する。"""

import unittest

from sensor_reference import SensorReference
from timing import frame_time_ms


SETTINGS = dict(adc_full_scale_v=6.144, adc_max=2047, supply_v=5, voltage_drop_v=0.137,
                thermistor_r25=10000, thermistor_beta=3380, reference_k=298.16, series_resistance=10000,
                temp_interval_ms=500, pressure_samples=5, oil_samples=2, water_samples=2,
                oil_channel=0, water_channel=1, pressure_channel=2)
ENABLED = dict(oil_temp=1, water_temp=1, pressure=1, ambient=1)


class SensorReferenceTest(unittest.TestCase):
    def setUp(self):
        self.sensor = SensorReference(SETTINGS, ENABLED)
        self.raw = dict(adc=[333, 333, 551, 0], imu=[0.125, -0.0625, 1], als=100)

    def settle(self):
        result = None
        for tick in range(26):
            result = self.sensor.step(frame_time_ms(tick), self.raw)
        return result

    def test_known_adc_calibration_values(self):
        self.assertAlmostEqual(self.sensor.voltage(1023), 3.070499, places=5)
        self.assertAlmostEqual(self.sensor.pressure(self.sensor.voltage(551)), 3.000996, places=5)
        self.assertAlmostEqual(self.sensor.pressure(self.sensor.voltage(333)), 1.319114, places=5)
        self.assertAlmostEqual(self.sensor.temperature(self.sensor.voltage(333)), 65.37805, places=4)

    def test_fault_voltage_boundaries(self):
        self.assertGreater(self.sensor.pressure(self.sensor.voltage(1632)), 11)
        self.assertEqual(self.sensor.pressure(self.sensor.voltage(1633)), 0)
        self.assertEqual(self.sensor.temperature(self.sensor.voltage(0)), 200)
        self.assertEqual(self.sensor.temperature(self.sensor.voltage(2047)), 200)

    def test_adc_reads_continue_during_imu_calibration(self):
        for tick in range(24):
            value = self.sensor.step(frame_time_ms(tick), self.raw)
            self.assertEqual(value['adc_reads'][2], (tick + 1) * 2)
            self.assertIsNone(value['g'])
        value = self.sensor.step(1000, self.raw)
        self.assertEqual(value['adc_reads'], [4, 4, 50, 0])
        self.assertEqual(value['g'], 0)

    def test_pressure_five_samples_start_with_zeros(self):
        first = self.sensor.step(0, self.raw)
        self.assertAlmostEqual(first['pressure_bar'], 3.000996 / 5, places=5)
        for now in (17, 34, 51, 68):
            value = self.sensor.step(now, self.raw)
        self.assertAlmostEqual(value['pressure_bar'], 3.000996, places=5)
        self.raw['adc'][2] = 0
        value = self.sensor.step(1250, self.raw)
        self.assertAlmostEqual(value['pressure_bar'], 3.000996 * 4 / 5, places=5)

    def test_temperature_holds_for_500ms_and_averages_two_readings(self):
        first = self.settle()
        self.raw['adc'][0] = 168
        held = self.sensor.step(1083, self.raw)
        self.assertEqual(held['oil_c'], first['oil_c'])
        self.assertEqual(held['adc_reads'][0], 4)
        changed = self.sensor.step(1541, self.raw)
        self.assertAlmostEqual(changed['oil_c'], (65.37805 + 95.05905) / 2, places=4)
        final = self.sensor.step(2041, self.raw)
        self.assertAlmostEqual(final['oil_c'], 95.05905, places=4)

    def test_temperature_fault_clears_buffers_and_recovery_restarts_averaging(self):
        self.settle()
        self.raw['adc'][0] = 0
        fault = self.sensor.step(1541, self.raw)
        self.assertEqual(fault['oil_c'], 200)
        self.raw['adc'][0] = 168
        recovered = self.sensor.step(2041, self.raw)
        self.assertAlmostEqual(recovered['oil_c'], 95.05905, places=4)

    def test_brief_communication_failure_invalidates_all_channels_immediately(self):
        before = self.settle()
        failed = self.sensor.step(1100, self.raw | {'adc_fault': 1})
        self.assertEqual(failed['status'], [2, 2, 2])
        self.assertEqual((failed['oil_c'], failed['water_c'], failed['pressure_bar']), (200, 200, 0))
        self.assertEqual(failed['adc_reads'][:2], before['adc_reads'][:2])
        waiting = self.sensor.step(1300, self.raw)
        self.assertEqual(waiting['status'], [2, 2, 2])
        recovered = self.sensor.step(2300, self.raw)
        self.assertEqual(recovered['status'], [1, 1, 1])
        self.assertLess(recovered['oil_c'], 100)

    def test_pressure_zero_voltage_is_invalid_but_half_volt_is_valid(self):
        self.settle()
        missing = self.sensor.step(1100, self.raw | {'adc': [333, 333, 0, 0]})
        self.assertEqual(missing['status'][2], 3)
        zero = self.sensor.step(1200, self.raw | {'adc': [333, 333, 162, 0]})
        self.assertEqual(zero['status'][2], 1)
        self.assertEqual(self.sensor.pressure(self.sensor.voltage(162)), 0)

    def test_axis_bias_direction_and_vertical_exclusion(self):
        self.settle()
        for axes, direction, cardinal in [([2.125, -0.0625, 1], 'Front', 1), ([-1.875, -0.0625, 1], 'Rear', 4),
                                          ([0.125, 1.9375, 1], 'Right', 2), ([0.125, -2.0625, 1], 'Left', 3)]:
            result = self.sensor.step(1250, self.raw | {'imu': axes})
            self.assertEqual(result['g'], 2)
            self.assertEqual(result['g_direction'], direction)
            self.assertEqual(result['g_cardinal'], cardinal)
        result = self.sensor.step(1291, self.raw | {'imu': [0.125, -0.0625, 3]})
        self.assertEqual(result['g'], 0)
        self.assertEqual(result['g_cardinal'], 0)

    def test_imu_failure_and_gyro_only_update_invalidate_g_without_stopping_adc(self):
        self.settle()
        high = self.sensor.step(1100, self.raw | {'imu': [1.925, -0.0625, 1]})
        self.assertAlmostEqual(high['g'], 1.8, places=5)
        for now, fault in [(1200, {'imu_connected': False}), (1500, {'imu_accel_updated': False})]:
            failed = self.sensor.step(now, self.raw | fault)
            self.assertIsNone(failed['g'])
            self.assertEqual((failed['g_direction'], failed['g_cardinal']), ('---', 0))
            self.assertEqual(failed['status'], [1, 1, 1])
            self.assertGreater(failed['adc_reads'][2], high['adc_reads'][2])
        self.assertEqual(self.sensor.step(1600, self.raw)['g'], 0)

    def test_failed_imu_samples_do_not_complete_startup_calibration(self):
        for now in range(0, 1000, 20):
            value = self.sensor.step(now, self.raw | {'imu_connected': False})
            self.assertIsNone(value['g'])
        self.assertEqual(self.sensor.offset_count, 0)
        self.assertEqual(value['status'], [1, 1, 1])
        self.sensor.step(1000, self.raw)
        for now in range(1200, 1580, 20):
            self.assertIsNone(self.sensor.step(now, self.raw)['g'])
        self.assertEqual(self.sensor.step(1580, self.raw)['g'], 0)

    def test_setup_snapshot_does_not_acquire_adc_or_calibrate_imu(self):
        self.sensor.bus.frame(0, 1)
        initial = self.sensor.snapshot()
        self.assertEqual(initial['adc_begin_calls'], 1)
        self.assertFalse(initial['adc_connected'])
        self.assertEqual(initial['adc_reads'], [0] * 4)
        self.assertEqual(self.sensor.offset_count, 0)
        self.assertEqual(initial['oil_c'], 0)


if __name__ == '__main__':
    unittest.main()
