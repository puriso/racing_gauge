#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;

// ALS 測定間隔 [ms]
constexpr uint16_t ALS_MEASUREMENT_INTERVAL_MS = 1000;

// 照度変化の平滑化係数
constexpr float ALS_SMOOTHING_ALPHA = 0.3f;

void updateBacklightLevel();

#endif  // BACKLIGHT_H
