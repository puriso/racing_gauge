#ifndef SENSOR_H
#define SENSOR_H

#include <Adafruit_ADS1X15.h>

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "config.h"

extern Adafruit_ADS1015 adsConverter;

extern float oilPressureSamples[PRESSURE_SAMPLE_SIZE];
extern float waterTemperatureSamples[WATER_TEMP_SAMPLE_SIZE];
extern float oilTemperatureSamples[OIL_TEMP_SAMPLE_SIZE];
extern bool oilPressureOverVoltage;
extern float currentGForce;            // 起動時からの水平加速度変化 [G]
extern const char *currentGDirection;  // 現在の加速度の向き (FR/RR/FL/RL, Front, Rear など)

void acquireSensorData();

// 既存の温度変換で使用する異常値と判定境界
constexpr float TEMPERATURE_ERROR_VALUE = 200.0F;
constexpr float TEMPERATURE_ERROR_THRESHOLD = 199.0F;

inline auto isValidTemperature(float temperature) -> bool
{
  return std::isfinite(temperature) && temperature < TEMPERATURE_ERROR_THRESHOLD;
}

// 平均計算テンプレート
template <size_t N>
inline auto calculateAverage(const float (&values)[N]) -> float
{
  // 配列サイズが0の場合は0を返す
  if (N == 0)
  {
    return 0.0F;
  }

  float sum = 0.0F;
  for (size_t i = 0; i < N; ++i)
  {
    sum += values[i];
  }
  return sum / static_cast<float>(N);
}

// 異常値を平均すると実在する高温に見えるため、1件でも異常なら異常値を返す
template <size_t N>
// 既存のセンサーバッファをコピーせず検証する
// NOLINTNEXTLINE(modernize-avoid-c-arrays)
inline auto calculateTemperatureAverage(const float (&values)[N]) -> float
{
  for (float value : values)
  {
    if (!isValidTemperature(value))
    {
      return TEMPERATURE_ERROR_VALUE;
    }
  }
  return calculateAverage(values);
}

#endif  // SENSOR_H
