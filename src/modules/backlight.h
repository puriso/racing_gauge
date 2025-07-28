#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include "config.h"

extern BrightnessMode currentBrightnessMode;

extern float kScreen;

/**
 * @brief ALS 関連の初期化を行う
 */
void initBacklight();

/**
 * @brief 画面光寄与係数を計測して保存する
 */
void calibrateScreenComp();

/**
 * @brief 周囲光に応じてバックライトを更新する
 */
void updateBacklightLevel();

#endif  // BACKLIGHT_H
