import unittest

from benchmark import aggregate, compare


class FrameRateAggregationTest(unittest.TestCase):
    def test_average_uses_frame_count_and_time_instead_of_mean_of_fps(self):
        # 10フレーム/1秒と90フレーム/3秒の合計は100フレーム/4秒となる。
        result = aggregate([
            {'measured_frames': 10, 'render_ns': 1_000_000_000},
            {'measured_frames': 90, 'render_ns': 3_000_000_000},
        ])
        self.assertEqual(25, result['average_fps'])
        self.assertEqual(40, result['mean_frame_ms'])
        self.assertEqual(20, result['median_run_fps'])

    def test_change_uses_same_metric_for_both_versions(self):
        result = compare(
            [{'measured_frames': 20, 'render_ns': 2_000_000_000}],
            [{'measured_frames': 20, 'render_ns': 1_000_000_000}],
        )
        self.assertEqual(10, result['before']['average_fps'])
        self.assertEqual(20, result['after']['average_fps'])
        self.assertEqual(100, result['change_percent'])


if __name__ == '__main__':
    unittest.main()
