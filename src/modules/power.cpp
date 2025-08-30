#include "power.h"

#include <M5Unified.h>

#include "config.h"
#include "display.h"

// VBUS 電圧を定期的に監視し、低下時にバックライトを暗くする
void monitorVbusVoltage()
{
  float vbus = M5.Power.getVBusVoltage();
  if (vbus > 0 && vbus < VBUS_LOW_THRESHOLD)
  {
    // 電圧低下を検出したら輝度を最低にする
    display.setBrightness(BACKLIGHT_LOW_VBUS);
  }
}
