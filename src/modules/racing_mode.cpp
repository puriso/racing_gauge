#include "racing_mode.h"

#include <cmath>

// レーシングモード継続判定と延長処理
bool updateRacingMode(unsigned long &startTime, unsigned long now, float ax, float ay, float az, float baseAx, float baseAy,
                      float baseAz)
{
  float dx = ax - baseAx;
  float dy = ay - baseAy;
  float dz = az - baseAz;
  // ルート計算を避けるため二乗値で比較
  float diffSq = dx * dx + dy * dy + dz * dz;
  float thresholdSq = RACING_MODE_ACCEL_THRESHOLD_G * RACING_MODE_ACCEL_THRESHOLD_G;
  if (diffSq >= thresholdSq)
  {
    startTime = now;  // 1g超過で時間延長
  }
  if (now - startTime >= RACING_MODE_DURATION_MS)
  {
    return false;  // 規定時間経過で終了
  }
  return true;  // 継続
}
