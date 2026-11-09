#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

namespace simulator
{
// 通常域・表示境界・入力異常の近傍を、各カウント500ms保持して往復する。
inline constexpr unsigned int ADC_STEPS_WARMUP_FRAMES = 48;
inline constexpr unsigned int ADC_STEPS_HOLD_FRAMES = 12;
inline constexpr unsigned int ADC_STEPS_WIDTH = 16;
inline constexpr unsigned int ADC_STEPS_PER_WINDOW = (ADC_STEPS_WIDTH * 2) + 1;
inline constexpr unsigned int ADC_STEPS_WINDOW_COUNT = 12;
// 入力値はREADMEの試験仕様表と対応させる。
// NOLINTBEGIN(readability-magic-numbers)
inline constexpr std::array<int16_t, 4> ADC_STEPS_INITIAL{151, 168, 551, 0};
inline constexpr std::array<std::array<int16_t, ADC_STEPS_WINDOW_COUNT>, 3> ADC_STEPS_CENTERS{
    {
        {8, 40, 80, 120, 151, 200, 333, 800, 1300, 1616, 1633, 2039},
        {8, 80, 120, 151, 168, 250, 333, 800, 1300, 1616, 1633, 2039},
        {8, 162, 551, 940, 1199, 1400, 1555, 1616, 1633, 1800, 2000, 2039},
    },
};
// NOLINTEND(readability-magic-numbers)

inline auto adcSteps(unsigned int frame) -> std::array<int16_t, 4>
{
  if (frame < ADC_STEPS_WARMUP_FRAMES)
  {
    return ADC_STEPS_INITIAL;
  }
  constexpr unsigned int TOTAL_STEPS = ADC_STEPS_PER_WINDOW * ADC_STEPS_WINDOW_COUNT;
  const auto step = std::min((frame - ADC_STEPS_WARMUP_FRAMES) / ADC_STEPS_HOLD_FRAMES, TOTAL_STEPS - 1);
  const auto window = step / ADC_STEPS_PER_WINDOW;
  const auto local = step % ADC_STEPS_PER_WINDOW;
  const auto offset = local <= ADC_STEPS_WIDTH ? local : (ADC_STEPS_WIDTH * 2) - local;
  std::array<int16_t, 4> result{};
  for (size_t channel = 0; channel < ADC_STEPS_CENTERS.size(); ++channel)
  {
    result[channel] = static_cast<int16_t>(ADC_STEPS_CENTERS[channel][window] - (ADC_STEPS_WIDTH / 2) + offset);
  }
  return result;
}
}  // namespace simulator
