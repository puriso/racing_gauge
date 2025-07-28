#include "backlight.h"

#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
// 現在の輝度モード
BrightnessMode currentBrightnessMode = BrightnessMode::Day;
// 照度の指数移動平均値
static float filteredLux = 0.0f;
static bool isLuxInitialized = false;

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
    if (currentBrightnessMode != BrightnessMode::Day)
    {
      currentBrightnessMode = BrightnessMode::Day;
      display.setBrightness(BACKLIGHT_DAY);
    }
    return;
  }

  uint16_t measuredLux = measureLuxWithoutBacklight();

  if (!isLuxInitialized)
  {
    filteredLux = static_cast<float>(measuredLux);
    isLuxInitialized = true;
  }
  else
  {
    filteredLux = filteredLux * (1.0f - LUX_FILTER_ALPHA) + static_cast<float>(measuredLux) * LUX_FILTER_ALPHA;
  }

  float useLux = filteredLux;

  BrightnessMode newMode = (useLux >= LUX_THRESHOLD_DAY)    ? BrightnessMode::Day
                           : (useLux >= LUX_THRESHOLD_DUSK) ? BrightnessMode::Dusk
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
