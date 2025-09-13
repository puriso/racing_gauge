#include "power_monitor.h"

#include <M5CoreS3.h>

#include "config.h"

// AXP2101の警告を有効化し初期化
void initPowerMonitor()
{
  uint64_t irqMask = m5::AXP2101_IRQ_LDO_OVER_CURR | m5::AXP2101_IRQ_BATFET_OVER_CURR | m5::AXP2101_IRQ_BAT_OVER_VOLTAGE |
                     m5::AXP2101_IRQ_WARNING_LEVEL1 | m5::AXP2101_IRQ_WARNING_LEVEL2;
  M5.Power.Axp2101.enableIRQ(irqMask);
  M5.Power.Axp2101.clearIRQStatuses();
}

// AXP2101から警告を取得し画面に表示して停止
void checkPowerWarnings()
{
  uint64_t status = M5.Power.Axp2101.getIRQStatuses();
  if (status == 0)
  {
    return;
  }

  // 画面を黒で塗りつぶしエラー内容を表示
  M5.Lcd.fillScreen(COLOR_BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.setTextColor(COLOR_RED);

  if (status & m5::AXP2101_IRQ_LDO_OVER_CURR)
  {
    M5.Lcd.println("[Power] LDO over-current");
  }
  if (status & m5::AXP2101_IRQ_BATFET_OVER_CURR)
  {
    M5.Lcd.println("[Power] BATFET over-current");
  }
  if (status & m5::AXP2101_IRQ_BAT_OVER_VOLTAGE)
  {
    M5.Lcd.println("[Power] Battery over-voltage");
  }
  if (status & m5::AXP2101_IRQ_WARNING_LEVEL1)
  {
    M5.Lcd.println("[Power] Warning level 1 (low battery)");
  }
  if (status & m5::AXP2101_IRQ_WARNING_LEVEL2)
  {
    M5.Lcd.println("[Power] Warning level 2 (critical battery)");
  }

  M5.Power.Axp2101.clearIRQStatuses();

  // 無限ループで以降の処理を停止
  const uint16_t errorLoopDelayMs = 1000;  // 遅延時間 (ms)
  while (true)
  {
    delay(errorLoopDelayMs);
  }
}
