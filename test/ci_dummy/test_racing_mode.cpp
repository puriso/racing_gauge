#include <unity.h>

#include "../../src/modules/racing_mode.h"

// 自動終了のテスト
void test_auto_timeout()
{
  unsigned long start = 0;
  bool active = updateRacingMode(start, RACING_MODE_DURATION_MS + 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  TEST_ASSERT_FALSE(active);
}

// 加速度による延長のテスト
void test_extension_by_accel()
{
  unsigned long start = 0;
  unsigned long now = RACING_MODE_DURATION_MS - 1;
  bool active = updateRacingMode(start, now, RACING_MODE_ACCEL_THRESHOLD_G + 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  TEST_ASSERT_TRUE(active);
  active = updateRacingMode(start, start + RACING_MODE_DURATION_MS - 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  TEST_ASSERT_TRUE(active);
  active = updateRacingMode(start, start + RACING_MODE_DURATION_MS + 1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  TEST_ASSERT_FALSE(active);
}

// 0.1秒継続でレーシングモード開始するテスト
void test_start_after_delay()
{
  unsigned long over = 0;
  bool start = shouldStartRacingMode(over, 0, RACING_MODE_ACCEL_THRESHOLD_G + 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
  TEST_ASSERT_FALSE(start);
  start = shouldStartRacingMode(over, RACING_MODE_START_DELAY_MS - 1, RACING_MODE_ACCEL_THRESHOLD_G + 0.1f, 0.0f, 0.0f,
                                0.0f, 0.0f, 0.0f);
  TEST_ASSERT_FALSE(start);
  start = shouldStartRacingMode(over, RACING_MODE_START_DELAY_MS, RACING_MODE_ACCEL_THRESHOLD_G + 0.1f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.0f);
  TEST_ASSERT_TRUE(start);
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_auto_timeout);
  RUN_TEST(test_extension_by_accel);
  RUN_TEST(test_start_after_delay);
  UNITY_END();
}

void loop()
{
  // テストでは使用しない
}
