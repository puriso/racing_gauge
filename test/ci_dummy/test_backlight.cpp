#include <unity.h>

// スタブヘッダーと対象モジュールをインクルード
// clang-format off
#include "M5CoreS3.h"
#include "M5GFX.h"
#include "../../src/modules/backlight.cpp"
// clang-format on

MockM5 M5;      // M5 スタブの実体
M5GFX display;  // 画面制御スタブ

void test_apply_brightness_mode_smooth()
{
  currentBrightnessMode = BrightnessMode::Day;
  applyBrightnessMode(BrightnessMode::Night);
  TEST_ASSERT_EQUAL_INT(BACKLIGHT_DAY, display.history.front());
  TEST_ASSERT_EQUAL_INT(BACKLIGHT_NIGHT, display.history.back());
  TEST_ASSERT_EQUAL_INT((BACKLIGHT_DAY - BACKLIGHT_NIGHT) + 1, display.history.size());
  for (size_t i = 1; i < display.history.size(); ++i)
  {
    TEST_ASSERT_EQUAL_INT(display.history[i - 1] - 1, display.history[i]);
  }
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_apply_brightness_mode_smooth);
  UNITY_END();
}

void loop() {}
