"""生ADCの全域往復と、中間カウントの欠損を検証する。"""

import copy
import unittest

from adc_steps import validate_adc_steps


def fixture():
    frames = [{'raw': {'adc': [151, 168, 551, 0]}} for _ in range(48)]
    values = list(range(2048)) + list(range(2046, -1, -1)) + list(range(1, 11))
    frames.extend({'raw': {'adc': [value, value, value, 0]}} for value in values)
    return frames


class AdcStepsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.frames = fixture()

    def test_accepts_every_count_on_both_legs_without_holds(self):
        validate_adc_steps(self.frames)
        self.assertEqual(self.frames[48]['raw']['adc'], [0, 0, 0, 0])
        self.assertEqual(self.frames[49]['raw']['adc'], [1, 1, 1, 0])
        self.assertEqual(self.frames[2095]['raw']['adc'], [2047, 2047, 2047, 0])
        self.assertEqual(self.frames[2096]['raw']['adc'], [2046, 2046, 2046, 0])
        self.assertEqual(self.frames[4142]['raw']['adc'], [0, 0, 0, 0])

    def test_rejects_skipped_intermediate_count(self):
        frames = copy.deepcopy(self.frames)
        frames[49]['raw']['adc'][0] = 2
        with self.assertRaisesRegex(ValueError, '1カウント'):
            validate_adc_steps(frames)

    def test_rejects_a_held_count(self):
        frames = copy.deepcopy(self.frames)
        frames[49]['raw']['adc'] = [0, 0, 0, 0]
        with self.assertRaisesRegex(ValueError, '1カウント'):
            validate_adc_steps(frames)

    def test_rejects_truncated_recording(self):
        with self.assertRaisesRegex(ValueError, '173秒'):
            validate_adc_steps(self.frames[:-1])


if __name__ == '__main__':
    unittest.main()
