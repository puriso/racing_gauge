import unittest
from risk_reference import RiskReference


class RiskReferenceTest(unittest.TestCase):
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
