"""再接続の厳密な時刻・本取得失敗・動画入力の欠損を検証する。"""

import unittest

from adc_bus_reference import AdcBusReference, fault_input, validate_communication


class AdcBusTest(unittest.TestCase):
    def setUp(self):
        self.reads, self.last = [0] * 4, [-1] * 4
        self.bus = AdcBusReference(self.reads, self.last)
        self.adc = [151, 168, 551, 0]

    def test_missing_at_boot_retries_at_1000_not_999(self):
        self.bus.frame(0, 1)
        self.bus.frame(999, 0)
        self.assertIsNone(self.bus.read(self.adc, 2))
        self.assertEqual(self.bus.attempts, 1)
        self.bus.frame(1000, 0)
        self.assertEqual(self.bus.read(self.adc, 2), 551)
        self.assertEqual(self.bus.attempts, 2)

    def test_busy_wait_ends_at_five_and_retry_is_relative_to_failure(self):
        self.bus.frame(1000, 2)
        self.assertIsNone(self.bus.read(self.adc, 2))
        self.assertEqual(self.bus.wait, 5)
        self.bus.frame(2004, 0)
        self.assertIsNone(self.bus.read(self.adc, 2))
        self.bus.frame(2005, 0)
        self.assertEqual(self.bus.read(self.adc, 2), 551)

    def test_second_result_failure_does_not_commit_dummy_sample(self):
        self.bus.frame(0, 6)
        self.assertIsNone(self.bus.read(self.adc, 2))
        self.assertEqual(self.reads[2], 1)
        self.bus.frame(1000, 0)
        self.assertEqual(self.bus.read(self.adc, 2), 551)
        self.assertEqual(self.reads[2], 3)

    def test_all_transfer_failures_wait_for_reconnection(self):
        for fault in (1, 3, 4, 5):
            bus = AdcBusReference([0] * 4, [-1] * 4)
            bus.frame(0, 0)
            bus.frame(4000, fault)
            self.assertIsNone(bus.read(self.adc, 2))
            bus.frame(4999, 0)
            self.assertIsNone(bus.read(self.adc, 0))
            bus.frame(5000, 0)
            self.assertEqual(bus.read(self.adc, 0), 151)

    def test_profile_contains_all_faults_and_final_recovery(self):
        self.assertEqual([fault_input('adc-communication', t) for t in (4000, 12000, 20000, 28000, 36000, 44000)], list(range(1, 7)))
        self.assertEqual(fault_input('adc-communication', 55000), 0)
        self.assertEqual(fault_input('adc-startup', 4999), 1)
        self.assertEqual(fault_input('adc-startup', 5000), 0)

    def test_missing_fault_in_video_input_is_rejected(self):
        with self.assertRaisesRegex(ValueError, '外部応答'):
            validate_communication(dict(scenario='adc-startup', frames=[dict(time_ms=0, raw=dict(adc_fault=0))]))
