#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;

// カメラ輝度測定間隔 [ms]
constexpr uint16_t CAMERA_MEASUREMENT_INTERVAL_MS = 8000;

void updateBacklightLevel();

#endif  // BACKLIGHT_H
