#include "backlight.h"

#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
// 現在の輝度モード
BrightnessMode currentBrightnessMode = BrightnessMode::Day;
// 平滑化後の照度値
static float filteredLux = -1.0F;

// ────────────────────── 輝度測定 ──────────────────────
// バックライトを消して輝度を測定
static auto measureLuxWithoutBacklight() -> uint16_t
{
  uint8_t prevBrightness = display.getBrightness();
  display.setBrightness(0);
  constexpr int MEASURE_DELAY_US = 500;
  delayMicroseconds(MEASURE_DELAY_US);
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

  if (filteredLux < 0.0F)
  {
    // 初回はそのまま値を採用
    filteredLux = static_cast<float>(measuredLux);
  }
  else
  {
    // 平滑化処理
    filteredLux += ALS_SMOOTHING_ALPHA * (static_cast<float>(measuredLux) - filteredLux);
  }

  BrightnessMode newMode;
  if (filteredLux >= LUX_THRESHOLD_DAY)
  {
    newMode = BrightnessMode::Day;
  }
  else if (filteredLux >= LUX_THRESHOLD_DUSK)
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
    uint8_t targetBrightness;
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
