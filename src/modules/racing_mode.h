#ifndef RACING_MODE_H
#define RACING_MODE_H

#include "config.h"

// レーシングモード開始前の輝度モードを取得
BrightnessMode getRacingModePrevBrightness();
// レーシングモード関連の状態を初期化
void resetRacingModeState();
// レーシングモードの開始・終了判定を更新
void updateRacingModeState(unsigned long now);

#endif  // RACING_MODE_H
