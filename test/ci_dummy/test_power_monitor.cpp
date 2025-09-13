#include <unity.h>

// スタブヘッダーと対象モジュールをインクルード
// clang-format off
#include "M5CoreS3.h"
#include "../../src/modules/power_monitor.h"
// clang-format on

MockM5 M5;  // スタブの実体

// initPowerMonitor の動作を確認
void test_init_power_monitor()
{
  initPowerMonitor();
  uint64_t expected = m5::AXP2101_IRQ_LDO_OVER_CURR | m5::AXP2101_IRQ_BATFET_OVER_CURR | m5::AXP2101_IRQ_BAT_OVER_VOLTAGE |
                      m5::AXP2101_IRQ_WARNING_LEVEL1 | m5::AXP2101_IRQ_WARNING_LEVEL2;
  TEST_ASSERT_EQUAL_UINT64(expected, M5.Power.Axp2101.irqMask);
  TEST_ASSERT_TRUE(M5.Power.Axp2101.clearCalled);
}

// 警告がない場合に何もしないことを確認
void test_check_power_warnings_no_status()
{
  M5.Power.Axp2101.irqStatus = 0;
  checkPowerWarnings();
  TEST_ASSERT_FALSE(M5.Lcd.fillCalled);
  TEST_ASSERT_FALSE(M5.Lcd.printlnCalled);
}

void setup()
{
  UNITY_BEGIN();
  RUN_TEST(test_init_power_monitor);
  RUN_TEST(test_check_power_warnings_no_status);
  UNITY_END();
}

void loop()
{
  // ループ処理は不要
}
