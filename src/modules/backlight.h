#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;

// ALS 測定間隔 [ms]
constexpr uint16_t ALS_MEASUREMENT_INTERVAL_MS = 8000;

void updateBacklightLevel();
void initializeBacklight();

#endif  // BACKLIGHT_H
