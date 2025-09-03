#include "backlight.h"

#include <algorithm>
#include <cstring>

#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
// 現在の輝度モード
BrightnessMode currentBrightnessMode = BrightnessMode::Day;
// ALS サンプルバッファ
int luxSamples[MEDIAN_BUFFER_SIZE] = {};
int luxSampleIndex = 0;  // 次に書き込むインデックス

// 直近取得した照度値
int latestLux = 0;
// 中央値フィルタ適用後の照度値
int medianLuxValue = 0;

// ────────────────────── 中央値計算 ──────────────────────
// サンプル配列から中央値を計算する
static auto calculateMedian(const int *samples) -> int
{
  int sortedSamples[MEDIAN_BUFFER_SIZE];
  memcpy(sortedSamples, samples, sizeof(sortedSamples));
  std::nth_element(sortedSamples, sortedSamples + MEDIAN_BUFFER_SIZE / 2, sortedSamples + MEDIAN_BUFFER_SIZE);
  return sortedSamples[MEDIAN_BUFFER_SIZE / 2];
}

// 指定された輝度モードを適用
void applyBrightnessMode(BrightnessMode mode)
{
  // 電圧を取得し、閾値未満なら輝度を変更しない
  int16_t voltage = M5.Power.getBatteryVoltage();
  if (voltage < MIN_BATTERY_VOLTAGE_MV)
  {
    if (DEBUG_MODE_ENABLED)
    {
      Serial.printf("[Power] voltage:%dmV, skip brightness change\n", voltage);
    }
    return;
  }
  currentBrightnessMode = mode;
  int targetBrightness = (mode == BrightnessMode::Day)    ? BACKLIGHT_DAY
                         : (mode == BrightnessMode::Dusk) ? BACKLIGHT_DUSK
                                                          : BACKLIGHT_NIGHT;
  display.setBrightness(targetBrightness);
}

// ────────────────────── 輝度更新 ──────────────────────
void updateBacklightLevel()
{
  if (!SENSOR_AMBIENT_LIGHT_PRESENT)
  {
    if (currentBrightnessMode != BrightnessMode::Day)
    {
      applyBrightnessMode(BrightnessMode::Day);
    }
    return;
  }

  int currentLux = CoreS3.Ltr553.getAlsValue();
  latestLux = currentLux;
  // サンプルをリングバッファへ格納
  luxSamples[luxSampleIndex] = currentLux;
  luxSampleIndex = (luxSampleIndex + 1) % MEDIAN_BUFFER_SIZE;

  int medianLux = calculateMedian(luxSamples);
  medianLuxValue = medianLux;

  // デバッグモードでは照度を出力
  if (DEBUG_MODE_ENABLED)
  {
    Serial.printf("[ALS] lux:%u, median:%u\n", currentLux, medianLux);
  }

  BrightnessMode newMode = (medianLux >= LUX_THRESHOLD_DAY)    ? BrightnessMode::Day
                           : (medianLux >= LUX_THRESHOLD_DUSK) ? BrightnessMode::Dusk
                                                               : BrightnessMode::Night;

  if (newMode != currentBrightnessMode)
  {
    applyBrightnessMode(newMode);
  }
}
