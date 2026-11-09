#include "adc_reader.h"

#include <Adafruit_ADS1X15.h>
#include <Adafruit_I2CDevice.h>
#include <Arduino.h>
#include <Wire.h>

#include <array>

namespace
{
constexpr uint32_t CONVERSION_TIMEOUT_MS = 5;
constexpr uint32_t RETRY_INTERVAL_MS = 1000;
constexpr uint8_t ADC_CHANNEL_COUNT = 4;
constexpr int SETTLING_US = 50;
constexpr uint8_t REGISTER_BYTE_SHIFT = 8;
constexpr uint8_t CONVERSION_BIT_SHIFT = 4;
constexpr uint16_t CONVERSION_SIGN_BIT = 0x0800;
constexpr uint16_t SIGN_EXTENSION_MASK = 0xF000;

bool adcInitialized = false;
uint32_t lastInitializationAttemptMs = 0;

auto getAdcDevice() -> Adafruit_I2CDevice&
{
  static Adafruit_I2CDevice device(ADS1X15_ADDRESS, &Wire);
  return device;
}

auto writeRegister(uint8_t address, uint16_t value) -> bool
{
  const std::array<uint8_t, 3> bytes = {
      address,
      static_cast<uint8_t>(value >> REGISTER_BYTE_SHIFT),
      static_cast<uint8_t>(value),
  };
  return getAdcDevice().write(bytes.data(), bytes.size());
}

auto readRegister(uint8_t address, uint16_t& value) -> bool
{
  std::array<uint8_t, 2> bytes = {};
  if (!getAdcDevice().write_then_read(&address, 1, bytes.data(), bytes.size()))
  {
    return false;
  }
  value = static_cast<uint16_t>((static_cast<uint32_t>(bytes[0]) << REGISTER_BYTE_SHIFT) | static_cast<uint32_t>(bytes[1]));
  return true;
}

auto readConversion(uint8_t channel, int16_t& value) -> bool
{
  // ±6.144V、1600SPS、単発変換。ALERTピンは使用しない。
  uint16_t config = static_cast<uint32_t>(ADS1X15_REG_CONFIG_CQUE_NONE) |
                    static_cast<uint32_t>(ADS1X15_REG_CONFIG_MODE_SINGLE) | static_cast<uint32_t>(GAIN_TWOTHIRDS) |
                    static_cast<uint32_t>(RATE_ADS1015_1600SPS) | static_cast<uint32_t>(MUX_BY_CHANNEL[channel]) |
                    static_cast<uint32_t>(ADS1X15_REG_CONFIG_OS_SINGLE);
  if (!writeRegister(ADS1X15_REG_POINTER_CONFIG, config))
  {
    return false;
  }

  uint32_t startedMs = millis();
  while (static_cast<uint32_t>(millis() - startedMs) < CONVERSION_TIMEOUT_MS)
  {
    uint16_t status = 0;
    if (!readRegister(ADS1X15_REG_POINTER_CONFIG, status))
    {
      return false;
    }
    if ((static_cast<uint32_t>(status) & static_cast<uint32_t>(ADS1X15_REG_CONFIG_OS_SINGLE)) != 0)
    {
      uint16_t raw = 0;
      if (!readRegister(ADS1X15_REG_POINTER_CONVERT, raw))
      {
        return false;
      }
      raw >>= CONVERSION_BIT_SHIFT;
      if ((raw & CONVERSION_SIGN_BIT) != 0)
      {
        raw |= SIGN_EXTENSION_MASK;
      }
      value = static_cast<int16_t>(raw);
      return true;
    }
    delay(1);
  }
  return false;
}
}  // namespace

auto initializeAdc() -> bool
{
  lastInitializationAttemptMs = millis();
  adcInitialized = getAdcDevice().begin();
  return adcInitialized;
}

auto readAdcChannel(uint8_t channel, int16_t& value) -> bool
{
  if (channel >= ADC_CHANNEL_COUNT)
  {
    return false;
  }
  if (!adcInitialized)
  {
    uint32_t nowMs = millis();
    uint32_t elapsedMs = nowMs - lastInitializationAttemptMs;
    if (elapsedMs < RETRY_INTERVAL_MS || !initializeAdc())
    {
      return false;
    }
  }

  int16_t discarded = 0;
  int16_t sample = 0;
  if (readConversion(channel, discarded))
  {
    delayMicroseconds(SETTLING_US);
    if (readConversion(channel, sample))
    {
      value = sample;
      return true;
    }
  }

  // 通信失敗や変換タイムアウト後は全チャンネルを保留し、次の再接続を待つ。
  adcInitialized = false;
  lastInitializationAttemptMs = millis();
  return false;
}
