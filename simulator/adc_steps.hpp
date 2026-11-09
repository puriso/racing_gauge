#pragma once

#include <array>
#include <cstdint>

namespace simulator
{
inline constexpr unsigned int ADC_STEPS_WARMUP_FRAMES = 48;
inline constexpr unsigned int ADC_STEPS_MAX_COUNT = 2047;
inline constexpr std::array<int16_t, 4> ADC_STEPS_INITIAL{151, 168, 551, 0};  // NOLINT(readability-magic-numbers)

inline auto adcSteps(unsigned int frame) -> std::array<int16_t, 4>
{
  if (frame < ADC_STEPS_WARMUP_FRAMES)
  {
    return ADC_STEPS_INITIAL;
  }
  // 温度の取得周期とは独立に、毎フレーム1カウント増減させる。
  const auto step = (frame - ADC_STEPS_WARMUP_FRAMES) % (ADC_STEPS_MAX_COUNT * 2);
  const auto value = static_cast<int16_t>(step <= ADC_STEPS_MAX_COUNT ? step : (ADC_STEPS_MAX_COUNT * 2) - step);
  return {value, value, value, 0};
}
}  // namespace simulator
