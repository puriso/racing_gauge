#include "backlight.h"

#include <algorithm>
#include <cmath>

#include "display.h"

// ────────────────────── グローバル変数 ──────────────────────
// 現在の輝度[%]
uint8_t currentBrightnessPercent = 100;
// EWMA フィルタ後の照度値
static float filteredLux = 0.0F;
// 画面由来光補正用係数（キャリブレーションで決定）
static float kScreen = 0.0F;

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
    // センサー未接続時は常に最大輝度
    display.setBrightness(255);
    currentBrightnessPercent = 100;
    return;
  }

  uint16_t measuredLux = measureLuxWithoutBacklight();

  // 画面由来光の差分補正
  float envLux = static_cast<float>(measuredLux) - kScreen * currentBrightnessPercent;
  envLux = std::max(envLux, 0.0F);

  // EWMA で平滑化
  filteredLux = ALS_EWMA_ALPHA * envLux + (1.0F - ALS_EWMA_ALPHA) * filteredLux;

  // 照度を0-100%に単純マッピング
  float targetPercent = std::clamp(filteredLux, 0.0F, 100.0F);

  // ヒステリシス判定
  float margin = ALS_HYST_MARGIN_RATIO * static_cast<float>(currentBrightnessPercent);
  if (std::fabs(targetPercent - static_cast<float>(currentBrightnessPercent)) <= margin)
  {
    return;  // 変化が小さければ無視
  }

  // ステップ変化で滑らかに調整
  if (targetPercent > static_cast<float>(currentBrightnessPercent))
  {
    currentBrightnessPercent =
        static_cast<uint8_t>(std::min<float>(100.0F, currentBrightnessPercent + BRIGHTNESS_STEP_PERCENT));
  }
  else
  {
    currentBrightnessPercent =
        static_cast<uint8_t>(std::max<float>(0.0F, currentBrightnessPercent - BRIGHTNESS_STEP_PERCENT));
  }

  // PWM同期読み取りを行えば更に精度向上可能（仮設）
  display.setBrightness(static_cast<uint8_t>((currentBrightnessPercent * 255) / 100));
}
