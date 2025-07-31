#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;

// ALS 測定間隔 [ms]
constexpr int ALS_MEASUREMENT_INTERVAL_MS = 8000;

void updateBacklightLevel();

#endif  // BACKLIGHT_H
