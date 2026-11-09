#pragma once

#include <Arduino.h>
#include <Wire.h>

#include <vector>

// 外部のI2C装置だけを置き換え、本番の読み取り処理を検証する。
namespace fake
{
inline bool present = true;
inline bool writeFailure = false;
inline bool readFailure = false;
inline bool resultFailure = false;
inline int resultFailureAtConversion = 0;
inline bool neverComplete = false;
inline int completeAfterPolls = 1;
inline int polls = 0;
inline int beginCalls = 0;
inline int conversions = 0;
inline int conversionsSinceBegin = 0;
inline int16_t sample = 1234;
inline uint8_t selectedRegister = 0;
inline uint16_t config = 0;
inline std::vector<uint16_t> configs;
}  // namespace fake

class Adafruit_I2CDevice
{
 public:
  Adafruit_I2CDevice(uint8_t, TwoWire*) {}

  bool begin()
  {
    ++fake::beginCalls;
    fake::conversionsSinceBegin = 0;
    return fake::present;
  }

  bool write(const uint8_t* bytes, size_t size)
  {
    if (!fake::present || fake::writeFailure) return false;
    fake::selectedRegister = bytes[0];
    if (size == 3 && bytes[0] == 1)
    {
      fake::config = (static_cast<uint16_t>(bytes[1]) << 8) | bytes[2];
      fake::configs.push_back(fake::config);
      ++fake::conversions;
      ++fake::conversionsSinceBegin;
      fake::polls = 0;
    }
    return true;
  }

  bool read(uint8_t* bytes, size_t size)
  {
    if (!fake::present || fake::readFailure || size != 2) return false;
    uint16_t value = 0;
    if (fake::selectedRegister == 1)
    {
      value = fake::config & 0x7FFF;
      if (!fake::neverComplete && ++fake::polls >= fake::completeAfterPolls) value |= 0x8000;
    }
    else if (fake::selectedRegister == 0)
    {
      if (fake::resultFailure || fake::conversionsSinceBegin == fake::resultFailureAtConversion) return false;
      int16_t sample = fake::conversionsSinceBegin % 2 == 1 ? 111 : fake::sample;
      value = static_cast<uint16_t>(sample) << 4;
    }
    bytes[0] = value >> 8;
    bytes[1] = value;
    return true;
  }

  bool write_then_read(const uint8_t* out, size_t outSize, uint8_t* in, size_t inSize)
  {
    return write(out, outSize) && read(in, inSize);
  }
};
