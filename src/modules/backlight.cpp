#include "backlight.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
// 現在の輝度モード
BrightnessMode currentBrightnessMode = BrightnessMode::Day;
// ALS サンプルバッファ
uint16_t luxSamples[MEDIAN_BUFFER_SIZE] = {};
int luxSampleIndex = 0;  // 次に書き込むインデックス
// 異常値判定用の前回有効値
static uint16_t lastValidLux = 0;

// ────────────────────── 輝度測定 ──────────────────────
// バックライトを消して輝度を測定
static auto measureLuxWithoutBacklight() -> uint16_t
{
  uint8_t prevBrightness = display.getBrightness();
  display.setBrightness(0);
  delayMicroseconds(500);
  uint16_t lux = CoreS3.Ltr553.getAlsValue();
  display.setBrightness(prevBrightness);
  return lux;
}

// ────────────────────── 中央値計算 ──────────────────────
// サンプル配列から中央値を計算する
static auto calculateMedian(const uint16_t *samples) -> uint16_t
{
  uint16_t sortedSamples[MEDIAN_BUFFER_SIZE];
  memcpy(sortedSamples, samples, sizeof(sortedSamples));
  std::nth_element(sortedSamples, sortedSamples + MEDIAN_BUFFER_SIZE / 2, sortedSamples + MEDIAN_BUFFER_SIZE);
  return sortedSamples[MEDIAN_BUFFER_SIZE / 2];
}

// サンプル配列から MAD (中央値絶対偏差) を計算する
static auto calculateMedianAbsoluteDeviation(const uint16_t *samples) -> uint16_t
{
  uint16_t median = calculateMedian(samples);
  uint16_t deviations[MEDIAN_BUFFER_SIZE];
  for (int i = 0; i < MEDIAN_BUFFER_SIZE; i++)
  {
    deviations[i] = std::abs(static_cast<int>(samples[i]) - static_cast<int>(median));
  }
  return calculateMedian(deviations);
}

// ────────────────────── 輝度更新 ──────────────────────
void updateBacklightLevel()
{
  if (!SENSOR_AMBIENT_LIGHT_PRESENT)
  {
    if (currentBrightnessMode != BrightnessMode::Day)
    {
      currentBrightnessMode = BrightnessMode::Day;
      display.setBrightness(BACKLIGHT_DAY);
    }
    return;
  }

  uint16_t measuredLux = measureLuxWithoutBacklight();

  // 直近サンプルの中央値と MAD を計算
  uint16_t prevMedian = calculateMedian(luxSamples);
  uint16_t mad = calculateMedianAbsoluteDeviation(luxSamples);

  bool isOutlier = false;
  if (mad == 0)
  {
    // 初期段階や変動が少ない場合は前回値で判定
    if (lastValidLux != 0 && measuredLux >= lastValidLux * 5 && measuredLux >= 5000)
    {
      isOutlier = true;
    }
  }
  else if (measuredLux > prevMedian + mad * 5)
  {
    isOutlier = true;
  }

  if (isOutlier)
  {
    // 異常値は前回有効値を使用
    measuredLux = (lastValidLux != 0) ? lastValidLux : prevMedian;
  }
  else
  {
    lastValidLux = measuredLux;
  }

  // サンプルをリングバッファへ格納
  luxSamples[luxSampleIndex] = measuredLux;
  luxSampleIndex = (luxSampleIndex + 1) % MEDIAN_BUFFER_SIZE;

  uint16_t medianLux = calculateMedian(luxSamples);

  BrightnessMode newMode = (medianLux >= LUX_THRESHOLD_DAY)    ? BrightnessMode::Day
                           : (medianLux >= LUX_THRESHOLD_DUSK) ? BrightnessMode::Dusk
                                                               : BrightnessMode::Night;

  if (newMode != currentBrightnessMode)
  {
    currentBrightnessMode = newMode;
    uint8_t targetBrightness = (newMode == BrightnessMode::Day)    ? BACKLIGHT_DAY
                               : (newMode == BrightnessMode::Dusk) ? BACKLIGHT_DUSK
                                                                   : BACKLIGHT_NIGHT;
    display.setBrightness(targetBrightness);
  }
}
