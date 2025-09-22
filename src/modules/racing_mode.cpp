#include "racing_mode.h"

#include "backlight.h"
#include "racing_indicator.h"

// レーシングモードの内部状態
static unsigned long gForceAboveThresholdSince = 0;          // 閾値を超えた時刻
static unsigned long racingStartMs = 0;                      // レーシングモード開始時刻
static BrightnessMode racingPrevMode = BrightnessMode::Day;  // レーシング開始前の輝度

// レーシングモードを開始する
static void startRacingMode(unsigned long nowMs)
{
  isRacingMode = true;
  racingStartMs = nowMs;
  racingPrevMode = currentBrightnessMode;
  applyBrightnessMode(BrightnessMode::Day);
}

// レーシングモードを終了し、輝度を元に戻す
static void finishRacingMode()
{
  isRacingMode = false;
  racingStartMs = 0;
  gForceAboveThresholdSince = 0;
#if SENSOR_AMBIENT_LIGHT_PRESENT
  updateBacklightLevel();
#else
  applyBrightnessMode(racingPrevMode);
#endif
}

void updateRacingMode(unsigned long nowMs, float gForce)  // NOLINT(bugprone-easily-swappable-parameters)
{
  if (isRacingMode)
  {
    if (racingStartMs != 0 && nowMs - racingStartMs >= RACING_MODE_DURATION_MS)
    {
      finishRacingMode();
    }
    return;
  }

  if (gForce > RACING_MODE_START_THRESHOLD_G)
  {
    if (gForceAboveThresholdSince == 0)
    {
      gForceAboveThresholdSince = nowMs;
    }
    else if (nowMs - gForceAboveThresholdSince >= RACING_MODE_START_HOLD_MS)
    {
      gForceAboveThresholdSince = 0;
      startRacingMode(nowMs);
    }
  }
  else
  {
    gForceAboveThresholdSince = 0;
  }
}

void forceStopRacingMode()
{
  isRacingMode = false;
  racingStartMs = 0;
  gForceAboveThresholdSince = 0;
}

auto getRacingPrevBrightnessMode() -> BrightnessMode { return racingPrevMode; }

void resetRacingModeState()
{
  isRacingMode = false;
  gForceAboveThresholdSince = 0;
  racingStartMs = 0;
  racingPrevMode = BrightnessMode::Day;
}
