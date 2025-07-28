#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

// ALS 測定間隔 [ms]
constexpr uint16_t ALS_MEASUREMENT_INTERVAL_MS = 200;

// 平滑化係数
constexpr float ALS_SMOOTHING_ALPHA = 0.2F;

// ヒステリシス比率
constexpr float ALS_HYST_MARGIN_RATIO = 0.10F;

// 調光ステップ (約5%)
constexpr uint8_t ALS_BRIGHTNESS_STEP = 13;

// 画面寄与補正係数 (キャリブレーションで決定)
extern float kScreen;

void updateBacklightLevel();

#endif  // BACKLIGHT_H
