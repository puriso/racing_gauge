#include "backlight.h"

#include <esp_camera.h>

#include <algorithm>
#include <cstring>

#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
// 現在の輝度モード
BrightnessMode currentBrightnessMode = BrightnessMode::Day;
// ALS サンプルバッファ
uint16_t luxSamples[MEDIAN_BUFFER_SIZE] = {};
int luxSampleIndex = 0;  // 次に書き込むインデックス

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

// カメラ画像から輝度を測定
static auto measureBrightnessWithCamera() -> uint16_t
{
  camera_fb_t *frame = esp_camera_fb_get();
  if (frame == nullptr)
  {
    return 0;
  }
  uint32_t sum = 0;
  for (size_t i = 0; i < frame->len; ++i)
  {
    sum += frame->buf[i];
  }
  esp_camera_fb_return(frame);
  return static_cast<uint16_t>(sum / frame->len);
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

// ────────────────────── 輝度更新 ──────────────────────
void updateBacklightLevel()
{
  if (!SENSOR_CAMERA_PRESENT && !SENSOR_AMBIENT_LIGHT_PRESENT)
  {
    if (currentBrightnessMode != BrightnessMode::Day)
    {
      currentBrightnessMode = BrightnessMode::Day;
      display.setBrightness(BACKLIGHT_DAY);
    }
    return;
  }

  uint16_t measuredLux = SENSOR_CAMERA_PRESENT ? measureBrightnessWithCamera() : measureLuxWithoutBacklight();

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
