#include <unity.h>

// VBUS制御ロジックをヘッダ経由でテスト
#include "../src/modules/power.h"

void test_vbus_reduce_and_recover()
{
  VbusState st;
  unsigned long now = 0;
  BrightnessMode mode = BrightnessMode::Day;

  // 1. 電圧が閾値未満になった場合は夜間モードへ
  PowerAction act = processVbus(VBUS_LOW_THRESHOLD - 0.1f, now, mode, st);
  TEST_ASSERT_EQUAL(PowerAction::ReduceBrightness, act);
  TEST_ASSERT_TRUE(st.isVoltageLow);
  TEST_ASSERT_EQUAL(mode, st.prevBrightness);

  // 2. 電圧が回復すると復帰モードへ移行
  now += VBUS_CHECK_INTERVAL_MS;
  act = processVbus(VBUS_RECOVER_THRESHOLD + 0.1f, now, mode, st);
  TEST_ASSERT_FALSE(st.isVoltageLow);
  TEST_ASSERT_TRUE(st.isRecovering);
  TEST_ASSERT_EQUAL(PowerAction::None, act);

  // 3. 復帰中は輝度が段階的に上昇
  now += 100;
  act = processVbus(VBUS_RECOVER_THRESHOLD + 0.1f, now, mode, st);
  TEST_ASSERT_EQUAL(PowerAction::StepBrightness, act);
  uint8_t first = st.recoverBrightness;
  TEST_ASSERT_TRUE(first > BACKLIGHT_NIGHT);

  // 4. 目標輝度まで進むと元のモードに戻る
  PowerAction lastAct = PowerAction::None;
  for (int i = 0; i < 50 && st.isRecovering; ++i)
  {
    now += 100;
    lastAct = processVbus(VBUS_RECOVER_THRESHOLD + 0.1f, now, mode, st);
  }
  TEST_ASSERT_EQUAL(PowerAction::RestoreBrightness, lastAct);
  TEST_ASSERT_FALSE(st.isRecovering);
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_vbus_reduce_and_recover);
  UNITY_END();
}

void loop()
{
  // ループは未使用
}
