#include "backlight.h"

#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
BrightnessMode currentBrightnessMode = BrightnessMode::Day;
static uint8_t targetBrightness = BACKLIGHT_DAY;
static uint8_t currentBrightness = BACKLIGHT_DAY;
static float smoothedLux = -1.0F;  // 初期化未完了を示す

// ────────────────────── 輝度更新 ──────────────────────
void updateBacklightLevel()
{
  if (!SENSOR_AMBIENT_LIGHT_PRESENT)
  {
    // ALS が無い場合は常に昼モード
    if (currentBrightnessMode != BrightnessMode::Day)
    {
      currentBrightnessMode = BrightnessMode::Day;
      targetBrightness = BACKLIGHT_DAY;
    }
  }
  else
  {
    uint16_t rawLux = CoreS3.Ltr553.getAlsValue();
    if (smoothedLux < 0.0F)
    {
      // 初回のみ即座に設定
      smoothedLux = static_cast<float>(rawLux);
    }
    else
    {
      // 平滑化して急激な変化を抑える
      smoothedLux = smoothedLux * (1.0F - LUX_SMOOTHING_ALPHA) + static_cast<float>(rawLux) * LUX_SMOOTHING_ALPHA;
    }

    BrightnessMode newMode = BrightnessMode::Night;
    if (smoothedLux >= static_cast<float>(LUX_THRESHOLD_DAY))
    {
      newMode = BrightnessMode::Day;
    }
    else if (smoothedLux >= static_cast<float>(LUX_THRESHOLD_DUSK))
    {
      newMode = BrightnessMode::Dusk;
    }

    if (newMode != currentBrightnessMode)
    {
      currentBrightnessMode = newMode;
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
    }
  }

  // 現在の輝度を目標に徐々に近づける
  if (currentBrightness < targetBrightness)
  {
    ++currentBrightness;
    display.setBrightness(currentBrightness);
  }
  else if (currentBrightness > targetBrightness)
  {
    --currentBrightness;
    display.setBrightness(currentBrightness);
  }
}
