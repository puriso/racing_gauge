#pragma once

#include <cstddef>
#include <cstdint>

#include "../adc_bus.hpp"
#include "Adafruit_ADS1X15.h"
#include "Wire.h"

// ADS1015の外部レジスター応答。電気特性や実機の転送時間は模擬しない。
class Adafruit_I2CDevice
{
 public:
  Adafruit_I2CDevice([[maybe_unused]] uint8_t address, [[maybe_unused]] TwoWire* wire) {}

  static auto begin() -> bool
  {
    ++simulator::adcBeginCalls;
    simulator::adcConversions = 0;
    return simulator::adcFault != simulator::AdcFault::Missing;
  }

  auto write(const uint8_t* bytes, size_t size) -> bool
  {
    if (simulator::adcFault == simulator::AdcFault::Missing || simulator::adcFault == simulator::AdcFault::Write)
    {
      return false;
    }
    if (size != 3 || bytes[0] != ADS1X15_REG_POINTER_CONFIG)
    {
      return false;
    }
    config = static_cast<uint16_t>((static_cast<uint32_t>(bytes[1]) << REGISTER_SHIFT) | static_cast<uint32_t>(bytes[2]));
    channel = ((static_cast<uint32_t>(config) & static_cast<uint32_t>(MUX_MASK)) >> MUX_SHIFT) - FIRST_SINGLE_CHANNEL;
    ++simulator::adcConversions;
    return channel < simulator::ADC_CHANNEL_COUNT;
  }

  auto write_then_read(const uint8_t* outputBytes, size_t outSize, uint8_t* inputBytes, size_t inSize) const -> bool
  {
    if (outSize != 1 || inSize != 2 || simulator::adcFault == simulator::AdcFault::Missing)
    {
      return false;
    }
    uint16_t value = 0;
    if (outputBytes[0] == ADS1X15_REG_POINTER_CONFIG)
    {
      if (simulator::adcFault == simulator::AdcFault::Status)
      {
        return false;
      }
      value = static_cast<uint32_t>(config) & ~static_cast<uint32_t>(ADS1X15_REG_CONFIG_OS_SINGLE);
      if (simulator::adcFault != simulator::AdcFault::Busy)
      {
        value |= ADS1X15_REG_CONFIG_OS_SINGLE;
      }
    }
    else if (outputBytes[0] == ADS1X15_REG_POINTER_CONVERT)
    {
      if (simulator::adcFault == simulator::AdcFault::Result ||
          (simulator::adcFault == simulator::AdcFault::SecondResult && simulator::adcConversions % 2 == 0))
      {
        return false;
      }
      const auto sample = simulator::adcChannels.at(channel);
      ++simulator::adcReadCounts.at(channel);
      simulator::adcLastRead.at(channel) = sample;
      value = static_cast<uint16_t>(sample) << SAMPLE_SHIFT;
    }
    else
    {
      return false;
    }
    inputBytes[0] = value >> REGISTER_SHIFT;
    inputBytes[1] = value;
    return true;
  }

 private:
  static constexpr unsigned int REGISTER_SHIFT = 8;
  static constexpr unsigned int SAMPLE_SHIFT = 4;
  static constexpr unsigned int MUX_SHIFT = 12;
  static constexpr unsigned int FIRST_SINGLE_CHANNEL = 4;
  static constexpr uint16_t MUX_MASK = 0x7000;
  uint16_t config = 0;
  unsigned int channel = 0;
};
