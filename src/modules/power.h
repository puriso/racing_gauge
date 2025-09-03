#ifndef POWER_H
#define POWER_H

#include <algorithm>

#include "config.h"

// VBUS監視や復帰制御の状態を保持する構造体
struct VbusState
{
  bool isVoltageLow = false;                            // 電圧低下状態か
  bool isRecovering = false;                            // 復帰中か
  BrightnessMode prevBrightness = BrightnessMode::Day;  // 電圧低下前の輝度モード
  unsigned long lastVbusCheckMs = 0;                    // VBUS監視を行った時刻
  unsigned long lastBrightnessStepMs = 0;               // 輝度復帰ステップ時刻
  uint8_t recoverBrightness = BACKLIGHT_NIGHT;          // 復帰中の現在輝度
};

// 電圧状態に応じてメイン側で行うべき処理
enum class PowerAction
{
  None,              // 何もしない
  ReduceBrightness,  // 輝度を夜間モードに落とす
  StepBrightness,    // 段階的に輝度を上げる
  RestoreBrightness  // 元の輝度モードへ戻す
};

// VBUS電圧と現在時刻、現在の輝度モードから状態を更新し、必要な処理を返す
PowerAction processVbus(float vbus, unsigned long now, BrightnessMode currentMode, VbusState &state);

#endif  // POWER_H
