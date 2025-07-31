#include "backlight.h"

#include <algorithm>
#include <array>

#include "Serial.h"
#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
// 現在の輝度モード
BrightnessMode currentBrightnessMode = BrightnessMode::Day;
// ALS サンプルバッファ
std::array<int, MEDIAN_BUFFER_SIZE> luxSamples{};
int luxSampleIndex = 0;  // 次に書き込むインデックス
// 現在の照度と中央値を保持
static int lastLux = 0;
static int lastMedianLux = 0;

// ────────────────────── 中央値計算 ──────────────────────
// サンプル配列から中央値を計算する
static auto calculateMedian(const std::array<int, MEDIAN_BUFFER_SIZE> &samples) -> int
{
  std::array<int, MEDIAN_BUFFER_SIZE> sortedSamples{};
  std::copy(samples.begin(), samples.end(), sortedSamples.begin());
  std::nth_element(sortedSamples.begin(), sortedSamples.begin() + (MEDIAN_BUFFER_SIZE / 2), sortedSamples.end());
  return sortedSamples[MEDIAN_BUFFER_SIZE / 2];
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

  int currentLux = CoreS3.Ltr553.getAlsValue();
  lastLux = currentLux;
  // サンプルをリングバッファへ格納
  luxSamples[luxSampleIndex] = currentLux;
  luxSampleIndex = (luxSampleIndex + 1) % MEDIAN_BUFFER_SIZE;

  int medianLux = calculateMedian(luxSamples);
  lastMedianLux = medianLux;

  // デバッグモードでは照度を出力
  if (DEBUG_MODE_ENABLED)
  {
    Serial.printf("[ALS] lux:%u, median:%u\n", currentLux, medianLux);
  }

  BrightnessMode newMode;
  if (medianLux >= LUX_THRESHOLD_DAY)
  {
    newMode = BrightnessMode::Day;
  }
  else if (medianLux >= LUX_THRESHOLD_DUSK)
  {
    newMode = BrightnessMode::Dusk;
  }
  else
  {
    newMode = BrightnessMode::Night;
  }

  if (newMode != currentBrightnessMode)
  {
    currentBrightnessMode = newMode;
    int targetBrightness;
    if (newMode == BrightnessMode::Day)
    {
      targetBrightness = BACKLIGHT_DAY;
    }
    else if (newMode == BrightnessMode::Dusk)
    {
      targetBrightness = BACKLIGHT_DUSK;
    }
    else
    {
      targetBrightness = BACKLIGHT_NIGHT;
    }
    display.setBrightness(targetBrightness);
  }
}

// ────────────────────── 照度取得 ──────────────────────
auto getCurrentLux() -> int { return lastLux; }

auto getMedianLux() -> int { return lastMedianLux; }
