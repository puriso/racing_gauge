"""24FPSの時刻・確認箇所・GIFの総再生時間を検証する。"""

import unittest

from timing import FRAME_RATE, frame_at_or_after, frame_time_ms, gif_duration_ms


class FrameTimingTest(unittest.TestCase):
    def test_frame_times_keep_fractional_milliseconds(self):
        self.assertEqual(FRAME_RATE, 24)
        self.assertEqual([frame_time_ms(index) for index in range(7)], [0, 41, 83, 125, 166, 208, 250])
        self.assertEqual(frame_time_ms(24), 1000)
        self.assertEqual(frame_time_ms(24 * 240), 240000)

    def test_cue_uses_first_frame_at_or_after_target(self):
        self.assertEqual(frame_at_or_after(4100), 99)
        self.assertEqual(frame_time_ms(frame_at_or_after(4100)), 4125)
        self.assertEqual(frame_at_or_after(4000), 96)

    def test_gif_durations_total_one_second_per_24_frames(self):
        durations = [gif_duration_ms(index) for index in range(24)]
        self.assertEqual(set(durations), {40, 50})
        self.assertEqual(sum(durations), 1000)

    def test_long_gif_never_accumulates_timing_drift(self):
        elapsed = 0
        for index in range(196 * 24 + 1):
            self.assertLessEqual(abs(elapsed - frame_time_ms(index)), 9)
            elapsed += gif_duration_ms(index)
        self.assertEqual(elapsed, 196040)


if __name__ == '__main__':
    unittest.main()
