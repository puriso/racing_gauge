#ifndef BRIGHTNESS_GUARD_H
#define BRIGHTNESS_GUARD_H

#include <algorithm>
#include <cstdint>

#include "power/voltage_provider.h"

namespace GuardedBrightness
{
// 電圧と現在値に基づいて最終輝度を決定する
inline uint8_t decide(uint8_t current, uint8_t requested, float vin, float min_for_rise, float hysteresis, uint8_t cap)
{
  if (requested <= current)
  {
    // 下げ方向は常に許可
    return requested;
  }
  float rise_on = min_for_rise + hysteresis;
  float rise_off = min_for_rise - hysteresis;
  if (vin >= rise_on)
  {
    return requested;  // 十分な電圧があるため上昇許可
  }
  if (vin < rise_off)
  {
    return std::min<uint8_t>(requested, cap);  // 電圧不足時は上限に丸める
  }
  return current;  // ヒステリシス帯域では現状維持
}

void apply(uint8_t requested);
void setVoltageProvider(IVoltageProvider* provider);
}  // namespace GuardedBrightness

#endif  // BRIGHTNESS_GUARD_H
