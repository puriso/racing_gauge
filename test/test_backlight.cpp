#include <unity.h>

#define UNIT_TEST
#define DISPLAY_H
#include "../include/config.h"

// ダミー表示デバイス
struct MockDisplay
{
  int lastBrightness = -1;
  void setBrightness(int b) { lastBrightness = b; }
};
MockDisplay display;

// ダミーALSセンサー
struct
{
  struct
  {
    int getAlsValue() { return 0; }
  } Ltr553;
} CoreS3;

// ダミーシリアル出力
struct
{
  void printf(const char*, ...) {}
} Serial;

// テスト用VBUS電圧
static float testVbusVoltage = 5.0f;
static float getVBusVoltage() { return testVbusVoltage; }

#include "../src/modules/backlight.cpp"

void test_skip_when_vbus_low()
{
  currentBrightnessMode = BrightnessMode::Day;
  testVbusVoltage = VBUS_VOLTAGE_THRESHOLD - 0.1f;
  applyBrightnessMode(BrightnessMode::Night);
  TEST_ASSERT_EQUAL(BrightnessMode::Day, currentBrightnessMode);
  TEST_ASSERT_EQUAL(-1, display.lastBrightness);
}

void test_apply_when_vbus_ok()
{
  currentBrightnessMode = BrightnessMode::Day;
  testVbusVoltage = VBUS_VOLTAGE_THRESHOLD + 0.5f;
  applyBrightnessMode(BrightnessMode::Night);
  TEST_ASSERT_EQUAL(BrightnessMode::Night, currentBrightnessMode);
  TEST_ASSERT_EQUAL(BACKLIGHT_NIGHT, display.lastBrightness);
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_skip_when_vbus_low);
  RUN_TEST(test_apply_when_vbus_ok);
  UNITY_END();
}

void loop() {}
