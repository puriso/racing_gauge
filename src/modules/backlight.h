#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

// 現在の輝度モード
extern BrightnessMode currentBrightnessMode;

// 平滑化係数 (小さくするほど変化が遅くなる)
constexpr float ALS_SMOOTHING_ALPHA = 0.1F;

// 画面寄与補正係数 (キャリブレーションで決定)
extern float kScreen;

void updateBacklightLevel();

#endif  // BACKLIGHT_H
