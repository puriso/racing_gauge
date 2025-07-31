#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;
// 直近に取得した照度値
extern int lastLuxValue;

// ALS 測定間隔 [ms]
constexpr int ALS_MEASUREMENT_INTERVAL_MS = 8000;

void updateBacklightLevel();

#endif  // BACKLIGHT_H
