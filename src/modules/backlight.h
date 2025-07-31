#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;
// 現在の照度値
extern int currentLuxValue;
// 照度の中央値
extern int medianLuxValue;

// ALS 測定間隔 [ms]
constexpr int ALS_MEASUREMENT_INTERVAL_MS = 8000;

void updateBacklightLevel();

#endif  // BACKLIGHT_H
