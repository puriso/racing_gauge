#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;

// ALS 測定間隔 [ms]
constexpr uint16_t ALS_MEASUREMENT_INTERVAL_MS = 8000;
// ALS 積分時間 [ms]
constexpr uint16_t ALS_INTEGRATION_TIME_MS = 300;

void updateBacklightLevel();

#endif  // BACKLIGHT_H
