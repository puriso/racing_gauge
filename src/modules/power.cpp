#include "power.h"

// VBUS電圧を解析し、状態を更新する処理
PowerAction processVbus(float vbus, unsigned long now, BrightnessMode currentMode, VbusState &state)
{
  PowerAction action = PowerAction::None;

  // 一定間隔ごとにVBUS電圧を確認
  if (now - state.lastVbusCheckMs >= VBUS_CHECK_INTERVAL_MS)
  {
    if (!state.isVoltageLow && vbus < VBUS_LOW_THRESHOLD)
    {
      // 閾値を下回ったら輝度を最小にする
      state.isVoltageLow = true;
      state.prevBrightness = currentMode;
      state.recoverBrightness = BACKLIGHT_NIGHT;
      action = PowerAction::ReduceBrightness;
    }
    else if (state.isVoltageLow && vbus >= VBUS_RECOVER_THRESHOLD)
    {
      // 電圧が回復したら復帰処理を開始
      state.isVoltageLow = false;
      state.isRecovering = true;
      state.lastBrightnessStepMs = now;
    }
    state.lastVbusCheckMs = now;
  }

  if (state.isRecovering)
  {
    // 目標輝度を判断
    uint8_t target = (state.prevBrightness == BrightnessMode::Day)    ? BACKLIGHT_DAY
                     : (state.prevBrightness == BrightnessMode::Dusk) ? BACKLIGHT_DUSK
                                                                      : BACKLIGHT_NIGHT;
    // 100ms ごとに10ずつ増加させる
    if (state.recoverBrightness < target && now - state.lastBrightnessStepMs >= 100)
    {
      state.recoverBrightness = std::min<uint8_t>(state.recoverBrightness + 10, target);
      state.lastBrightnessStepMs = now;
      action = PowerAction::StepBrightness;
    }
    if (state.recoverBrightness >= target)
    {
      state.isRecovering = false;
      action = PowerAction::RestoreBrightness;
    }
  }

  return action;
}
