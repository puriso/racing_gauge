#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;

// 直近取得した照度値
extern int latestLux;
// 中央値フィルタを適用した照度値
extern int medianLuxValue;

// ALS 測定間隔 [ms]
constexpr int ALS_MEASUREMENT_INTERVAL_MS = 8000;

void updateBacklightLevel();
// 指定された輝度モードを適用
void applyBrightnessMode(BrightnessMode mode);

#endif  // BACKLIGHT_H
