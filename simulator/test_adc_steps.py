"""全中間カウントの保持・取得と、欠損時の検出を確認する。"""

import copy
import unittest

from adc_steps import CENTERS, validate_adc_steps


def fixture():
    frames = []
    reads = [0, 0, 0, 0]
    last = [-1, -1, -1, -1]
    for frame in range(4801):
        step = min(max(0, frame - 48) // 12, 395)
        window, local = divmod(step, 33)
        offset = local if local <= 16 else 32 - local
        adc = ([151, 168, 551, 0] if frame < 48 else
               [centers[window] - 8 + offset for centers in CENTERS] + [0])
        if frame >= 25:
            reads[2] += 2
            last[2] = adc[2]
            if (frame - 25) % 12 == 0:
                for channel in (0, 1):
                    reads[channel] += 2
                    last[channel] = adc[channel]
        frames.append({'raw': {'adc': adc}, 'sensor': {'adc_reads': reads.copy(), 'adc_last': last.copy()}})
    return frames


class AdcStepsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.frames = fixture()

    def test_accepts_every_count_read_on_both_legs(self):
        validate_adc_steps(self.frames)
        self.assertEqual(self.frames[48]['raw']['adc'], [0, 0, 0, 0])
        self.assertEqual(self.frames[48 + 16 * 12]['raw']['adc'], [16, 16, 16, 0])
        self.assertEqual(self.frames[48 + 32 * 12]['raw']['adc'], [0, 0, 0, 0])
        self.assertEqual(self.frames[48 + (11 * 33 + 16) * 12]['raw']['adc'], [2047, 2047, 2047, 0])

    def test_rejects_skipped_intermediate_count(self):
        frames = copy.deepcopy(self.frames)
        frames[60]['raw']['adc'][0] = 2
        with self.assertRaisesRegex(ValueError, '1カウント'):
            validate_adc_steps(frames)

    def test_rejects_count_not_acquired_by_temperature_sampling(self):
        frames = copy.deepcopy(self.frames)
        frames[71]['sensor']['adc_last'][1] = 0
        with self.assertRaisesRegex(ValueError, '中間カウント'):
            validate_adc_steps(frames)

    def test_rejects_stale_reads_even_when_adc_value_matches(self):
        frames = copy.deepcopy(self.frames)
        frames[71]['sensor']['adc_reads'][0] = frames[59]['sensor']['adc_reads'][0]
        with self.assertRaisesRegex(ValueError, '中間カウント'):
            validate_adc_steps(frames)

    def test_rejects_truncated_recording(self):
        with self.assertRaisesRegex(ValueError, '200秒'):
            validate_adc_steps(self.frames[:-1])


if __name__ == '__main__':
    unittest.main()
