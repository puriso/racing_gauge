#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "config.h"

namespace simulator
{
struct RawInputs
{
  std::array<int16_t, 4> adc;
  std::array<float, 3> imu;
  bool menu;
  int lux;
};

// 目標波形からADC刺激を作るだけの逆変換。表示・判定にはこの物理量を渡さない。
inline constexpr float FIXTURE_ADC_SCALE_V = 6.144F;
inline constexpr float FIXTURE_ADC_MAX = 2047.0F;
inline constexpr float FIXTURE_SUPPLY_V = 5.0F;
inline constexpr float FIXTURE_CORRECTION = FIXTURE_SUPPLY_V / (FIXTURE_SUPPLY_V - VOLTAGE_DROP);
inline constexpr float FIXTURE_BETA = 3380.0F;
inline constexpr float FIXTURE_REFERENCE_K = 298.16F;
inline constexpr float FIXTURE_CELSIUS_OFFSET = 273.16F;
inline constexpr float FIXTURE_AX_BIAS = 0.125F;
inline constexpr float FIXTURE_AY_BIAS = -0.0625F;

inline auto voltageToAdc(float voltage) -> int16_t
{
  return static_cast<int16_t>(
      std::lround(std::clamp(voltage, 0.0F, FIXTURE_ADC_SCALE_V) * FIXTURE_ADC_MAX / FIXTURE_ADC_SCALE_V));
}

inline auto temperatureToAdc(float celsius) -> int16_t
{
  // 分圧抵抗はR25と同じ10kΩなので、比だけで入力電圧を作れる。
  const float resistanceRatio =
      std::exp(FIXTURE_BETA * ((1.0F / (celsius + FIXTURE_CELSIUS_OFFSET)) - (1.0F / FIXTURE_REFERENCE_K)));
  const float voltage = FIXTURE_SUPPLY_V * resistanceRatio / (1.0F + resistanceRatio) / FIXTURE_CORRECTION;
  return voltageToAdc(voltage);
}

template <typename Target>
auto sensorInputs(const Target& target) -> RawInputs
{
  constexpr float PRESSURE_ZERO_V = 0.5F;
  constexpr float PRESSURE_BAR_PER_V = 2.5F;
  const auto pressure = voltageToAdc((PRESSURE_ZERO_V + (target.pressure / PRESSURE_BAR_PER_V)) / FIXTURE_CORRECTION);
  // 配線通りCH0=油温、CH1=水温、CH2=油圧。温度・Gの取得後の変数には書き込まない。
  return {
      {temperatureToAdc(target.oil), temperatureToAdc(target.water), pressure, 0},
      {FIXTURE_AX_BIAS, FIXTURE_AY_BIAS + target.acceleration, 1.0F},
      target.menu,
      target.lux,
  };
}
}  // namespace simulator
