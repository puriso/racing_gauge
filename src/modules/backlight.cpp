#include "backlight.h"

#include <algorithm>
#include <array>

#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
// 現在の輝度モード

BrightnessMode currentBrightnessMode = BrightnessMode::Day;
// ALS サンプルバッファ
std::array<int, MEDIAN_BUFFER_SIZE> luxSamples{};
int luxSampleIndex = 0;  // 次に書き込むインデックス

// 直近取得した照度値
int latestLux = 0;
// 中央値フィルタ適用後の照度値
int medianLuxValue = 0;

// ────────────────────── 中央値計算 ──────────────────────
// サンプル配列から中央値を計算する
static auto calculateMedian(const std::array<int, MEDIAN_BUFFER_SIZE> &samples) -> int
{
  auto sortedSamples = samples;  // コピーしてソート用配列を作成
  std::nth_element(sortedSamples.begin(), sortedSamples.begin() + (MEDIAN_BUFFER_SIZE / 2), sortedSamples.end());
  return sortedSamples[MEDIAN_BUFFER_SIZE / 2];
}

// 指定された輝度モードを適用
void applyBrightnessMode(BrightnessMode mode)
{
  currentBrightnessMode = mode;
  int targetBrightness = 0;
  if (mode == BrightnessMode::Day)
  {
    targetBrightness = BACKLIGHT_DAY;
  }
  else if (mode == BrightnessMode::Dusk)
  {
    targetBrightness = BACKLIGHT_DUSK;
  }
  else
  {
    targetBrightness = BACKLIGHT_NIGHT;
  }
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

  BrightnessMode newMode = BrightnessMode::Night;
  if (medianLux >= LUX_THRESHOLD_DAY)
  {
    newMode = BrightnessMode::Day;
  }
  else if (medianLux >= LUX_THRESHOLD_DUSK)
  {
    newMode = BrightnessMode::Dusk;
  }

  if (newMode != currentBrightnessMode)
  {
    applyBrightnessMode(newMode);
  }
}
