#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;

// ALS 測定間隔 [ms]
constexpr uint16_t ALS_MEASUREMENT_INTERVAL_MS = 8000;
// カメラ輝度測定間隔 [ms]
constexpr uint16_t CAMERA_MEASUREMENT_INTERVAL_MS = 8000;

void updateBacklightLevel();

#endif  // BACKLIGHT_H
