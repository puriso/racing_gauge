#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

// ALS 取得間隔 [ms]
constexpr uint16_t ALS_MEASUREMENT_INTERVAL_MS = 500;

// 平滑化係数 (小さくするほど変化が遅くなる)
constexpr float ALS_SMOOTHING_ALPHA = 0.1F;

// ヒステリシス比率 (大きいほど変化しにくい)
constexpr float ALS_HYST_MARGIN_RATIO = 0.15F;

// 調光ステップ (約5%)
constexpr uint8_t ALS_BRIGHTNESS_STEP = 13;

// 画面寄与補正係数 (キャリブレーションで決定)
extern float kScreen;

void updateBacklightLevel();

#endif  // BACKLIGHT_H
