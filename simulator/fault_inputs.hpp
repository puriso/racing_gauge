#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace simulator
{
struct AdcFaultPhase
{
  unsigned int beginMs;
  unsigned int endMs;
  std::array<int16_t, 3> channels;
};

// 試験仕様に対応する時刻と生カウント。-1のチャンネルは通常入力を継続する。
// NOLINTBEGIN(readability-magic-numbers)
inline constexpr std::array<AdcFaultPhase, 4> DISCONNECT_PHASES{
    AdcFaultPhase{4000, 8000, {1666, -1, -1}},
    AdcFaultPhase{12000, 16000, {-1, 1666, -1}},
    AdcFaultPhase{20000, 24000, {-1, -1, 0}},
    AdcFaultPhase{28000, 32000, {1666, 1666, 0}},
};
inline constexpr std::array<AdcFaultPhase, 5> HIGH_TEMPERATURE_PHASES{
    AdcFaultPhase{4000, 8000, {25, -1, -1}},   AdcFaultPhase{8000, 12000, {24, -1, -1}},
    AdcFaultPhase{16000, 20000, {-1, 25, -1}}, AdcFaultPhase{20000, 24000, {-1, 24, -1}},
    AdcFaultPhase{28000, 32000, {1, 1, -1}},
};
inline constexpr std::array<AdcFaultPhase, 5> LOW_TEMPERATURE_PHASES{
    AdcFaultPhase{4000, 8000, {1429, -1, -1}},     AdcFaultPhase{8000, 12000, {1430, -1, -1}},
    AdcFaultPhase{16000, 20000, {-1, 1429, -1}},   AdcFaultPhase{20000, 24000, {-1, 1430, -1}},
    AdcFaultPhase{28000, 32000, {1430, 1430, -1}},
};
// NOLINTEND(readability-magic-numbers)

inline auto isFaultScenario(std::string_view scenario) -> bool
{
  return scenario == "sensor-disconnect" || scenario == "temperature-high" || scenario == "temperature-low";
}

template <size_t Count>
void applyFaultPhases(const std::array<AdcFaultPhase, Count>& phases, unsigned int nowMs, std::array<int16_t, 4>& adc)
{
  for (const auto& phase : phases)
  {
    if (nowMs < phase.beginMs || nowMs >= phase.endMs)
    {
      continue;
    }
    for (size_t channel = 0; channel < phase.channels.size(); ++channel)
    {
      if (phase.channels[channel] >= 0)
      {
        adc[channel] = phase.channels[channel];
      }
    }
  }
}

inline void applyFaultInputs(std::string_view scenario, unsigned int nowMs, std::array<int16_t, 4>& adc)
{
  if (scenario == "sensor-disconnect")
  {
    applyFaultPhases(DISCONNECT_PHASES, nowMs, adc);
  }
  else if (scenario == "temperature-high")
  {
    applyFaultPhases(HIGH_TEMPERATURE_PHASES, nowMs, adc);
  }
  else if (scenario == "temperature-low")
  {
    applyFaultPhases(LOW_TEMPERATURE_PHASES, nowMs, adc);
  }
}
}  // namespace simulator
