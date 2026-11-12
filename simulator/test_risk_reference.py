import unittest
from risk_reference import RiskReference


class RiskReferenceTest(unittest.TestCase):
    def test_normal_g_peak_prefers_lower_pressure_and_risk_overrides_it(self):
        r = RiskReference()
        r.step(0, 5, 0, 100, True, True, False)
        self.assertFalse(r.snapshot()['recorded'])
        r.step(1000, 5, 2, 100, True, True, False)
        self.assertTrue(r.snapshot()['recorded'])
        r.step(1100, 4, 2.01, 100, True, True, False)
        self.assertEqual(r.snapshot()['at_ms'], 1100)
        r.step(1200, 3.8, 1.8, 100, True, True, False)
        self.assertEqual(r.snapshot()['at_ms'], 1100)
        r.step(1300, 3.4, 1.2, 100, True, True, False)
        r.step(1600, 3.4, 1.2, 100, True, True, False)
        self.assertEqual(r.snapshot()['kind'], 2)

    def test_peak_direction_belongs_to_maximum_not_latest_value(self):
        r = RiskReference()
        r.step(0, 1, 2, 100, True, True, True, 1)
        r.step(40, 5, 1, 100, True, True, False, 4)
        self.assertEqual(r.window[600][2:3] + r.window[600][5:6], [200, 1])
        r.step(60, 5, 2, 100, True, True, False, 4)
        self.assertEqual(r.window[600][5], 1)
        r.step(80, 5, 2.5, 100, True, True, False, 3)
        self.assertEqual((r.window[600][2], r.window[600][5]), (250, 3))
        self.assertEqual(r.snapshot()['g_direction'], 1)

    def test_near_low_does_not_need_low_warning(self):
        r = RiskReference()
        for now in range(0, 2001, 50):
            r.step(now, 4, 0, 100, True, True, False)
        self.assertEqual(r.step(2100, 3.4, 1.5, 100, True, True, False)['kind'], 0)
        self.assertEqual(r.step(2400, 3.4, 1.5, 100, True, True, False)['kind'], 2)

    def test_actual_low_and_age_keep_center(self):
        r = RiskReference()
        r.step(0, 5, 0, 100, True, True, False)
        r.step(70000, 1, 2, 110, True, True, True)
        result = r.step(140000, 5, 0, 120, True, True, False)
        self.assertEqual((result['kind'], result['age_ms'], result['post_ms']), (3, 70000, 60000))
        self.assertEqual(r.window[600][0], 100)

    def test_invalid_values_leave_gaps_and_do_not_replace_event(self):
        r = RiskReference()
        r.step(0, 1, 2, 110, True, True, True)
        result = r.step(100, 0, 2, 0, False, False, False)
        self.assertEqual(result['at_ms'], 0)
        self.assertEqual(r.window[601][4] & (16 | 32), 16 | 32)
        self.assertEqual(r.window[1200][4], 0)

    def test_transient_does_not_qualify(self):
        r = RiskReference()
        for now in range(0, 2001, 50):
            r.step(now, 6, 0, 100, True, True, False)
        r.step(2100, 2, 2, 100, True, True, False)
        self.assertEqual(r.step(2200, 6, 0, 100, True, True, False)['kind'], 0)
