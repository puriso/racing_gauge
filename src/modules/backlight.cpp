#include "backlight.h"

#include <algorithm>

#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
float kScreen = 0.0F;                                         // 画面寄与補正係数
BrightnessMode currentBrightnessMode = BrightnessMode::Day;   // 現在のモード
static float prevSmoothedLux = 0.0F;                          // EWMA 用前回値
static uint8_t currentBrightness = BACKLIGHT_DAY;             // 出力中の輝度
static float luxSamples[MEDIAN_BUFFER_SIZE] = {};             // ルクスサンプル
static int luxSampleIndex = 0;                                // 書き込み位置
static unsigned long lastUpdateTime = 0;                      // 最終更新時刻
static unsigned long updateInterval = ALS_FIRST_INTERVAL_MS;  // 判定間隔
static bool initialized = false;                              // 初回更新済みか

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

// ────────────────────── 輝度更新 ──────────────────────
void updateBacklightLevel()
{
  if (!SENSOR_AMBIENT_LIGHT_PRESENT)
  {
    if (currentBrightness != BACKLIGHT_DAY)
    {
      currentBrightness = BACKLIGHT_DAY;
      display.setBrightness(currentBrightness);
    }
    return;
  }

  uint16_t measuredLux = measureLuxWithoutBacklight();

  // 画面光補正
  float screenComp = kScreen * (static_cast<float>(currentBrightness) / 2.55F);
  float envLux = (measuredLux > screenComp) ? measuredLux - screenComp : 0.0F;

  // EWMA で平滑化
  float smoothed = ALS_SMOOTHING_ALPHA * envLux + (1.0F - ALS_SMOOTHING_ALPHA) * prevSmoothedLux;
  prevSmoothedLux = smoothed;

  // デバッグ表示
  if (DEBUG_MODE_ENABLED)
  {
    Serial.printf("RAW:%u COMP:%.1f ENV:%.1f SMOOTH:%.1f\n", measuredLux, screenComp, envLux, smoothed);
  }

  // サンプルをバッファへ保存
  luxSamples[luxSampleIndex] = smoothed;
  luxSampleIndex = (luxSampleIndex + 1) % MEDIAN_BUFFER_SIZE;
  static int filledSamples = 0;
  if (filledSamples < MEDIAN_BUFFER_SIZE)
  {
    filledSamples++;
  }

  unsigned long now = millis();
  if (now - lastUpdateTime < updateInterval)
  {
    return;
  }
  lastUpdateTime = now;

  int requiredSamples = updateInterval / ALS_MEASUREMENT_INTERVAL_MS;
  if (filledSamples < requiredSamples)
  {
    return;
  }

  float temp[MEDIAN_BUFFER_SIZE];
  for (int i = 0; i < requiredSamples; ++i)
  {
    int idx = (luxSampleIndex - 1 - i + MEDIAN_BUFFER_SIZE) % MEDIAN_BUFFER_SIZE;
    temp[i] = luxSamples[idx];
  }
  std::nth_element(temp, temp + requiredSamples / 2, temp + requiredSamples);
  float medianLux = temp[requiredSamples / 2];

  BrightnessMode newMode = (medianLux >= LUX_THRESHOLD_DAY)    ? BrightnessMode::Day
                           : (medianLux >= LUX_THRESHOLD_DUSK) ? BrightnessMode::Dusk
                                                               : BrightnessMode::Night;

  if (newMode != currentBrightnessMode)
  {
    currentBrightnessMode = newMode;
    uint8_t targetBrightness = (newMode == BrightnessMode::Day)    ? BACKLIGHT_DAY
                               : (newMode == BrightnessMode::Dusk) ? BACKLIGHT_DUSK
                                                                   : BACKLIGHT_NIGHT;
    currentBrightness = targetBrightness;
    display.setBrightness(currentBrightness);
  }

  if (!initialized)
  {
    updateInterval = ALS_UPDATE_INTERVAL_MS;
    initialized = true;
  }
}
