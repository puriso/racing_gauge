#include "backlight.h"

#include <Preferences.h>

#include <algorithm>
#include <cstring>

#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
// 現在の輝度モード
BrightnessMode currentBrightnessMode = BrightnessMode::Day;
Preferences backlightPrefs;

float kScreen = DEFAULT_K_SCREEN;
static float prevSmoothedLux = 0.0F;
static uint8_t currentBrightness = BACKLIGHT_DAY;
static unsigned long lastAlsTime = 0;

// ────────────────────── 輝度変換テーブル ──────────────────────
// lux から PWM 値 (0-255) へ変換
static auto luxToBrightness(uint32_t lux) -> uint8_t
{
  if (lux < 10)
  {
    return 51;  // 20 %
  }
  if (lux < 50)
  {
    return 102;  // 40 %
  }
  if (lux < 200)
  {
    return 153;  // 60 %
  }
  if (lux < 800)
  {
    return 204;  // 80 %
  }
  return 255;
}

// ────────────────────── 初期化 ──────────────────────
void initBacklight()
{
  backlightPrefs.begin("backlight", false);
  kScreen = backlightPrefs.getFloat("k_screen", DEFAULT_K_SCREEN);
  backlightPrefs.end();
  currentBrightness = BACKLIGHT_DAY;
  prevSmoothedLux = 0.0F;
  lastAlsTime = 0;
}

// ────────────────────── キャリブレーション ──────────────────────
void calibrateScreenComp()
{
  uint8_t prev = display.getBrightness();
  display.setBrightness(0);
  delay(300);
  uint16_t lux0 = CoreS3.Ltr553.getAlsValue();
  display.setBrightness(255);
  delay(300);
  uint16_t lux100 = CoreS3.Ltr553.getAlsValue();
  display.setBrightness(prev);

  kScreen = static_cast<float>(lux100 - lux0) / 255.0F;
  backlightPrefs.begin("backlight", false);
  backlightPrefs.putFloat("k_screen", kScreen);
  backlightPrefs.end();
}

// ────────────────────── 輝度更新 ──────────────────────
void updateBacklightLevel()
{
  unsigned long now = millis();
  if (now - lastAlsTime < ALS_SAMPLE_INTERVAL_MS)
  {
    return;  // サンプリング間隔未満なら何もしない
  }
  lastAlsTime = now;

  if (!SENSOR_AMBIENT_LIGHT_PRESENT)
  {
    if (currentBrightness != BACKLIGHT_DAY)
    {
      currentBrightness = BACKLIGHT_DAY;
      display.setBrightness(currentBrightness);
    }
    return;
  }

  uint32_t rawLux = CoreS3.Ltr553.getAlsValue();

  float screenComp = kScreen * static_cast<float>(currentBrightness);
  float envLux = (rawLux > screenComp) ? static_cast<float>(rawLux) - screenComp : 0.0F;

  float smoothed = ALS_EWMA_ALPHA * envLux + (1.0F - ALS_EWMA_ALPHA) * prevSmoothedLux;
  prevSmoothedLux = smoothed;

  uint8_t target = luxToBrightness(static_cast<uint32_t>(smoothed));

  float upper = static_cast<float>(currentBrightness) * (1.0F + ALS_HYST_MARGIN_RATIO);
  float lower = static_cast<float>(currentBrightness) * (1.0F - ALS_HYST_MARGIN_RATIO);

  if (target > upper)
  {
    currentBrightness = std::min<uint8_t>(currentBrightness + BRIGHTNESS_STEP, 255);
  }
  else if (target < lower)
  {
    currentBrightness = (currentBrightness > BRIGHTNESS_STEP) ? currentBrightness - BRIGHTNESS_STEP : 0;
  }

  display.setBrightness(currentBrightness);
}
