#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

// ALS 測定間隔 [ms]
constexpr uint16_t ALS_MEASUREMENT_INTERVAL_MS = 200;

// ── 自動調光アルゴリズム用パラメータ ──
constexpr float ALS_EWMA_ALPHA = 0.20F;         // EWMA 係数
constexpr float ALS_HYST_MARGIN_RATIO = 0.10F;  // ヒステリシス閾値比
constexpr uint8_t BRIGHTNESS_STEP_PERCENT = 5;  // 調光ステップ [%]

void updateBacklightLevel();

#endif  // BACKLIGHT_H
