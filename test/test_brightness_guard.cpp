#include <unity.h>

#include "../src/display/brightness_guard.h"

// vin が低い場合はキャップまで丸める
void test_decide_cap_under_voltage()
{
  uint8_t result = GuardedBrightness::decide(50, 120, 4.6f, 4.8f, 0.1f, 80);
  TEST_ASSERT_EQUAL_UINT8(80, result);
}

// 十分な電圧なら要求輝度を適用
void test_decide_allow_when_voltage_high()
{
  uint8_t result = GuardedBrightness::decide(50, 120, 5.0f, 4.8f, 0.1f, 80);
  TEST_ASSERT_EQUAL_UINT8(120, result);
}

// 下げ要求は常に許可
void test_decide_always_allow_decrease()
{
  uint8_t result = GuardedBrightness::decide(120, 80, 4.6f, 4.8f, 0.1f, 80);
  TEST_ASSERT_EQUAL_UINT8(80, result);
}

// ヒステリシス帯域では現状維持
void test_decide_hysteresis()
{
  uint8_t result = GuardedBrightness::decide(80, 120, 4.75f, 4.8f, 0.1f, 80);
  TEST_ASSERT_EQUAL_UINT8(80, result);
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_decide_cap_under_voltage);
  RUN_TEST(test_decide_allow_when_voltage_high);
  RUN_TEST(test_decide_always_allow_decrease);
  RUN_TEST(test_decide_hysteresis);
  UNITY_END();
}

void loop()
{
  // ループ処理は不要
}
