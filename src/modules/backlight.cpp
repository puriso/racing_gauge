#include "backlight.h"

#include <M5CoreS3.h>

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

// 現在の輝度レベル
static int currentBrightnessLevel = BACKLIGHT_DAY;

// 輝度を段階的に変更する
static void setBrightnessSmooth(int target)
{
  int step = (target > currentBrightnessLevel) ? 1 : -1;
  for (int level = currentBrightnessLevel; level != target; level += step)
  {
    display.setBrightness(level);
    delay(5);  // 電流変動を抑えるため少し待機
  }
  display.setBrightness(target);
  currentBrightnessLevel = target;
}

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
  currentBrightnessMode = mode;
  int targetBrightness = (mode == BrightnessMode::Day)    ? BACKLIGHT_DAY
                         : (mode == BrightnessMode::Dusk) ? BACKLIGHT_DUSK
                                                          : BACKLIGHT_NIGHT;
  setBrightnessSmooth(targetBrightness);
}

// ────────────────────── 輝度更新 ──────────────────────
void updateBacklightLevel()
{
#if !SENSOR_AMBIENT_LIGHT_PRESENT
  if (currentBrightnessMode != BrightnessMode::Day)
  {
    applyBrightnessMode(BrightnessMode::Day);
  }
  return;
#endif

  int currentLux = CoreS3.Ltr553.getAlsValue();
  latestLux = currentLux;
  // サンプルをリングバッファへ格納
  luxSamples[luxSampleIndex] = currentLux;
  luxSampleIndex = (luxSampleIndex + 1) % MEDIAN_BUFFER_SIZE;

  int medianLux = calculateMedian(luxSamples);
  medianLuxValue = medianLux;

  // デバッグモードでは照度を出力
#if DEBUG_MODE_ENABLED
  Serial.printf("[ALS] lux:%u, median:%u\n", currentLux, medianLux);
#endif

  BrightnessMode newMode = (medianLux >= LUX_THRESHOLD_DAY)    ? BrightnessMode::Day
                           : (medianLux >= LUX_THRESHOLD_DUSK) ? BrightnessMode::Dusk
                                                               : BrightnessMode::Night;

  if (newMode != currentBrightnessMode)
  {
    applyBrightnessMode(newMode);
  }
}
