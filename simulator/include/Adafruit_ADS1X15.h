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

// 本番adc_reader.cppが使用するADS1015のレジスター定義。
inline constexpr uint8_t ADS1X15_ADDRESS = 0x48;
inline constexpr uint8_t ADS1X15_REG_POINTER_CONFIG = 1;
inline constexpr uint8_t ADS1X15_REG_POINTER_CONVERT = 0;
inline constexpr uint16_t ADS1X15_REG_CONFIG_CQUE_NONE = 3;
inline constexpr uint16_t ADS1X15_REG_CONFIG_MODE_SINGLE = 0x0100;
inline constexpr uint16_t GAIN_TWOTHIRDS = 0;
inline constexpr uint16_t RATE_ADS1015_1600SPS = 0x0080;
inline constexpr uint16_t ADS1X15_REG_CONFIG_OS_SINGLE = 0x8000;
inline constexpr std::array<uint16_t, 4> MUX_BY_CHANNEL = {0x4000, 0x5000, 0x6000, 0x7000};
