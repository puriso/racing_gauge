#ifndef RACING_MODE_H
#define RACING_MODE_H

#include "config.h"

// レーシングモードの状態更新を行う
void updateRacingMode(unsigned long nowMs, float gForce);

// レーシングモードを強制的に停止し、判定状態を初期化する
void forceStopRacingMode();

// レーシングモード開始前の輝度モードを取得する
BrightnessMode getRacingPrevBrightnessMode();

// レーシングモードの内部状態を初期化する
void resetRacingModeState();

#endif  // RACING_MODE_H
