#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;

// ALS 測定間隔 [ms]
constexpr int ALS_MEASUREMENT_INTERVAL_MS = 8000;

void updateBacklightLevel();

// 現在の照度を取得
auto getCurrentLux() -> int;

// 中央値の照度を取得
auto getMedianLux() -> int;

#endif  // BACKLIGHT_H
