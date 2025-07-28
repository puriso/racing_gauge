#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;

// ALS 測定間隔 [ms]
constexpr uint16_t ALS_MEASUREMENT_INTERVAL_MS = 1000;

// ALS フィルタの係数
constexpr float LUX_FILTER_ALPHA = 0.2f;

void updateBacklightLevel();

#endif  // BACKLIGHT_H
