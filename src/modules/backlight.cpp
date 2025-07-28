#include "backlight.h"

#include <algorithm>

#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
float kScreen = 0.0F;                              // 画面寄与補正係数
static float prevSmoothedLux = 0.0F;               // EWMA 用前回値
static uint8_t currentBrightness = BACKLIGHT_DAY;  // 現在の輝度

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

// ────────────────────── 辞書関数 ──────────────────────
// パーセンテージから輝度値(0-255)へ変換
static auto percentToBrightness(uint8_t percent) -> uint8_t { return static_cast<uint8_t>(percent * 255 / 100); }

// ルクスから輝度値(0-255)を求める
static auto luxToBrightness(float lux) -> uint8_t
{
  if (lux < 10.0F) return percentToBrightness(20);
  if (lux < 50.0F) return percentToBrightness(40);
  if (lux < 200.0F) return percentToBrightness(60);
  if (lux < 800.0F) return percentToBrightness(80);
  return percentToBrightness(100);
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

  uint8_t target = luxToBrightness(smoothed);

  uint8_t upper = static_cast<uint8_t>(currentBrightness * (1.0F + ALS_HYST_MARGIN_RATIO));
  uint8_t lower = static_cast<uint8_t>(currentBrightness * (1.0F - ALS_HYST_MARGIN_RATIO));

  if (target > upper)
  {
    currentBrightness = std::min<uint8_t>(static_cast<uint8_t>(currentBrightness + ALS_BRIGHTNESS_STEP), 255);
    display.setBrightness(currentBrightness);
  }
  else if (target < lower)
  {
    if (currentBrightness > ALS_BRIGHTNESS_STEP)
    {
      currentBrightness = static_cast<uint8_t>(currentBrightness - ALS_BRIGHTNESS_STEP);
    }
    else
    {
      currentBrightness = 0;
    }
    display.setBrightness(currentBrightness);
  }
}
