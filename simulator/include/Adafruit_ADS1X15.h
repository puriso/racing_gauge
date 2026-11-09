#pragma once

#include <array>
#include <cstdint>

namespace simulator
{
inline constexpr unsigned int ADC_CHANNEL_COUNT = 4;
inline std::array<int16_t, ADC_CHANNEL_COUNT> adcChannels{};
inline std::array<uint32_t, ADC_CHANNEL_COUNT> adcReadCounts{};
inline std::array<int16_t, ADC_CHANNEL_COUNT> adcLastRead{-1, -1, -1, -1};
}  // namespace simulator

// 生のADCカウントだけを置き換え、電圧・物理量への変換は本番sensor.cppに任せる。
class Adafruit_ADS1015
{
 public:
  static auto readADC_SingleEnded(uint8_t channel) -> int16_t
  {
    ++simulator::adcReadCounts.at(channel);
    simulator::adcLastRead.at(channel) = simulator::adcChannels.at(channel);
    return simulator::adcLastRead.at(channel);
  }
};
