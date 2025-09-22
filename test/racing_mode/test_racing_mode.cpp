#include <unity.h>

#include "../../include/config.h"

// ────────────────────── テスト用スタブ ──────────────────────
bool isRacingMode = false;
BrightnessMode currentBrightnessMode = BrightnessMode::Day;
int latestLux = 0;
int medianLuxValue = 0;

static int applyBrightnessCallCount = 0;
static int backlightUpdateCallCount = 0;

void applyBrightnessMode(BrightnessMode mode)
{
  currentBrightnessMode = mode;
  ++applyBrightnessCallCount;
}

void updateBacklightLevel()
{
  currentBrightnessMode = BrightnessMode::Night;
  ++backlightUpdateCallCount;
}

#include "../../src/modules/racing_mode.cpp"

static void resetTestState()
{
  resetRacingModeState();
  isRacingMode = false;
  currentBrightnessMode = BrightnessMode::Night;
  applyBrightnessCallCount = 0;
  backlightUpdateCallCount = 0;
}

void setUp() { resetTestState(); }

void tearDown()
{
  // テスト終了時の処理は不要
}

// 0.1秒間の連続超過が必要であることを確認
void test_racing_mode_requires_hold()
{
  updateRacingMode(0UL, 1.2F);
  TEST_ASSERT_FALSE(isRacingMode);

  updateRacingMode(RACING_MODE_START_HOLD_MS - 1, 1.2F);
  TEST_ASSERT_FALSE(isRacingMode);
  TEST_ASSERT_EQUAL(0, applyBrightnessCallCount);

  updateRacingMode(RACING_MODE_START_HOLD_MS, 1.2F);
  TEST_ASSERT_TRUE(isRacingMode);
  TEST_ASSERT_EQUAL(BrightnessMode::Day, currentBrightnessMode);
  TEST_ASSERT_EQUAL(BrightnessMode::Night, getRacingPrevBrightnessMode());
  TEST_ASSERT_EQUAL(1, applyBrightnessCallCount);
}

// 起動直後であっても閾値超過から指定時間経過すれば起動することを確認
void test_racing_mode_starts_after_initial_hold()
{
  updateRacingMode(0UL, 1.2F);
  TEST_ASSERT_FALSE(isRacingMode);

  updateRacingMode(RACING_MODE_START_HOLD_MS, 1.2F);
  TEST_ASSERT_TRUE(isRacingMode);
  TEST_ASSERT_EQUAL(BrightnessMode::Day, currentBrightnessMode);
  TEST_ASSERT_EQUAL(BrightnessMode::Night, getRacingPrevBrightnessMode());
  TEST_ASSERT_EQUAL(1, applyBrightnessCallCount);
}

// 閾値と同じGでも保持時間経過で起動することを確認
void test_racing_mode_starts_when_g_equals_threshold()
{
  updateRacingMode(0UL, RACING_MODE_START_THRESHOLD_G);
  TEST_ASSERT_FALSE(isRacingMode);

  updateRacingMode(RACING_MODE_START_HOLD_MS, RACING_MODE_START_THRESHOLD_G);
  TEST_ASSERT_TRUE(isRacingMode);
  TEST_ASSERT_EQUAL(BrightnessMode::Day, currentBrightnessMode);
  TEST_ASSERT_EQUAL(BrightnessMode::Night, getRacingPrevBrightnessMode());
  TEST_ASSERT_EQUAL(1, applyBrightnessCallCount);
}

// 閾値未満に戻ると保持時間がリセットされることを確認
void test_racing_mode_resets_hold_when_g_drops()
{
  unsigned long halfHold = RACING_MODE_START_HOLD_MS / 2;
  updateRacingMode(0UL, 1.2F);
  updateRacingMode(halfHold, 0.5F);
  updateRacingMode(RACING_MODE_START_HOLD_MS, 1.2F);
  TEST_ASSERT_FALSE(isRacingMode);

  updateRacingMode(RACING_MODE_START_HOLD_MS + RACING_MODE_START_HOLD_MS, 1.2F);
  TEST_ASSERT_TRUE(isRacingMode);
  TEST_ASSERT_EQUAL(1, applyBrightnessCallCount);
}

// 強制停止時は輝度復帰を行わないことを確認
void test_force_stop_does_not_restore_brightness()
{
  updateRacingMode(0UL, 1.2F);
  updateRacingMode(RACING_MODE_START_HOLD_MS, 1.2F);
  TEST_ASSERT_TRUE(isRacingMode);
  TEST_ASSERT_EQUAL(BrightnessMode::Day, currentBrightnessMode);

  forceStopRacingMode();
  TEST_ASSERT_FALSE(isRacingMode);
  TEST_ASSERT_EQUAL(0, backlightUpdateCallCount);
  TEST_ASSERT_EQUAL(BrightnessMode::Day, currentBrightnessMode);
}

// レーシングモード外での強制停止時に現在の輝度を保持することを確認
void test_force_stop_updates_prev_mode_when_not_racing()
{
  currentBrightnessMode = BrightnessMode::Dusk;

  forceStopRacingMode();

  TEST_ASSERT_EQUAL(BrightnessMode::Dusk, getRacingPrevBrightnessMode());
  TEST_ASSERT_FALSE(isRacingMode);
}

// 規定時間経過で自動的に輝度が復帰することを確認
void test_racing_mode_auto_finish_restores_brightness()
{
  updateRacingMode(0UL, 1.2F);
  updateRacingMode(RACING_MODE_START_HOLD_MS, 1.2F);
  TEST_ASSERT_TRUE(isRacingMode);

  unsigned long finishTime = RACING_MODE_START_HOLD_MS + RACING_MODE_DURATION_MS + 1UL;
  updateRacingMode(finishTime, 0.0F);

  TEST_ASSERT_FALSE(isRacingMode);
  TEST_ASSERT_EQUAL(0, backlightUpdateCallCount);
  TEST_ASSERT_EQUAL(2, applyBrightnessCallCount);
  TEST_ASSERT_EQUAL(BrightnessMode::Night, currentBrightnessMode);
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_racing_mode_requires_hold);
  RUN_TEST(test_racing_mode_starts_after_initial_hold);
  RUN_TEST(test_racing_mode_starts_when_g_equals_threshold);
  RUN_TEST(test_racing_mode_resets_hold_when_g_drops);
  RUN_TEST(test_force_stop_does_not_restore_brightness);
  RUN_TEST(test_force_stop_updates_prev_mode_when_not_racing);
  RUN_TEST(test_racing_mode_auto_finish_restores_brightness);
  UNITY_END();
}

void loop()
{
  // ループ処理は不要
}
