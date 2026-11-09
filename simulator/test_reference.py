"""境界時刻と既知のステップ入力で、録画用期待値の意味を検証する。"""

import unittest

from reference import Reference


class ReferenceTest(unittest.TestCase):
    def setUp(self):
        self.reference = Reference(
            dict(median_size=6, als_interval_ms=8000, lux_day=15, lux_dusk=10, brightness=[255, 200, 60],
                 pressure_alpha=0.3, racing_g=1.0, racing_hold_ms=100, racing_duration_ms=180000),
            dict(oil_temp=1, water_temp=1, pressure=1, ambient=1))

    def sample(self, time, **values):
        return self.reference.step(dict(time_ms=time, oil_c=100, water_c=95, pressure_bar=5,
                                        g=0, menu=False, lux=100) | values)

    def test_known_step_response(self):
        self.sample(0, pressure_bar=0, oil_c=0)
        first = self.sample(16, pressure_bar=10, oil_c=100)
        second = self.sample(33, pressure_bar=10, oil_c=100)
        self.assertEqual((first['pressure_bar'], second['pressure_bar']), (3.0, 5.1))
        self.assertEqual((first['oil_c'], second['oil_c']), (10.0, 19.0))

    def test_racing_hold_and_expiry_boundaries(self):
        self.assertFalse(self.sample(0, g=1)['racing'])
        self.assertFalse(self.sample(99, g=1)['racing'])
        self.assertTrue(self.sample(100, g=1)['racing'])
        self.assertTrue(self.sample(180099)['racing'])
        self.assertFalse(self.sample(180100)['racing'])

    def test_short_g_does_not_enter_racing(self):
        self.sample(0, g=2)
        self.assertFalse(self.sample(50)['racing'])

    def test_low_delay_and_hold_boundaries(self):
        self.assertFalse(self.sample(1000, g=2, pressure_bar=2)['low'])
        self.assertFalse(self.sample(1499, g=2, pressure_bar=2)['low'])
        self.assertTrue(self.sample(1500, g=2, pressure_bar=2)['low'])
        self.assertTrue(self.sample(4499)['low'])
        self.assertFalse(self.sample(4500)['low'])

    def test_upper_median_startup_and_light_changes(self):
        self.assertEqual(self.sample(0)['brightness_pwm'], 60)
        self.assertEqual(self.sample(8000)['brightness_pwm'], 60)
        self.assertEqual(self.sample(16000)['brightness_pwm'], 255)
        for time in (24000, 32000, 40000):
            self.assertEqual(self.sample(time, lux=12)['brightness_pwm'], 255)
        self.assertEqual(self.sample(48000, lux=12)['brightness_pwm'], 200)

    def test_night_menu_and_racing_restore_brightness(self):
        self.sample(0, lux=2)
        self.assertEqual(self.sample(1000, lux=2, menu=True)['brightness_pwm'], 255)
        self.assertEqual(self.sample(2000, lux=2)['brightness_pwm'], 60)
        self.sample(3000, lux=2, g=1)
        self.assertEqual(self.sample(3100, lux=2, g=1)['brightness_pwm'], 255)
        self.assertEqual(self.sample(183100, lux=2)['brightness_pwm'], 60)


if __name__ == '__main__':
    unittest.main()
