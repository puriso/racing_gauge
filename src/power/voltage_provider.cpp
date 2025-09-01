#include "power/voltage_provider.h"

#include <algorithm>

#include "config.h"

// 最後に取得した正常値
static float s_lastValid = RG_VIN_MIN_FOR_RISE_V;
// 移動平均された電圧
static float s_filtered = RG_VIN_MIN_FOR_RISE_V;
// 前回更新時刻
static unsigned long s_lastMs = 0;

float VoltageProvider::readVin()
{
  float raw = M5.Power.getVBUSVoltage();  // USB給電電圧を取得
  if (raw > 0.0f)
  {
    s_lastValid = raw;
  }
  else
  {
    raw = s_lastValid;  // 異常値の場合は最後の正常値を利用
  }

  unsigned long now = millis();
  float alpha = 1.0f;
  if (s_lastMs != 0)
  {
    float dt = static_cast<float>(now - s_lastMs);
    alpha = std::min(dt / RG_VIN_SAMPLE_WINDOW_MS, 1.0f);  // 移動平均係数
  }
  s_filtered += (raw - s_filtered) * alpha;
  s_lastMs = now;
  return s_filtered;
}
