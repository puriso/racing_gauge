#include "racing_mode.h"

#include "backlight.h"
#include "racing_indicator.h"
#include "sensor.h"

// レーシングモードかどうかを保持
bool isRacingMode = false;
// レーシング開始時刻
static unsigned long racingStartMs = 0;
// レーシング開始前の輝度モード
static BrightnessMode racingPrevMode = BrightnessMode::Day;
// レーシング判定開始時刻
static unsigned long racingJudgeStartMs = 0;

// レーシングモード開始前の輝度モードを取得
BrightnessMode getRacingModePrevBrightness() { return racingPrevMode; }

// レーシングモード関連の状態を初期化
void resetRacingModeState()
{
  isRacingMode = false;
  racingStartMs = 0;
  racingJudgeStartMs = 0;
}

// レーシングモードの開始・終了判定を更新
void updateRacingModeState(unsigned long now)
{
  if (isRacingMode && currentGForce > 1.0F)
  {
    // レーシングモード中に1Gを超えたら経過タイマーをリセット
    racingStartMs = now;
    return;
  }

  if (isRacingMode)
  {
    const unsigned long elapsedMs = now - racingStartMs;  // unsigned long の減算でオーバーフローを吸収
    if (elapsedMs < RACING_MODE_DURATION_MS)
    {
      // 継続時間内であればレーシングモードを維持
      return;
    }

    // 一定時間経過でレーシングモードを終了
    isRacingMode = false;
#if SENSOR_AMBIENT_LIGHT_PRESENT
    updateBacklightLevel();
#else
    applyBrightnessMode(racingPrevMode);
#endif
    racingJudgeStartMs = 0;  // 判定タイマーをリセット
    return;
  }

  if (currentGForce <= 1.0F)
  {
    // 1G未満に戻ったら判定タイマーを破棄
    racingJudgeStartMs = 0;
    return;
  }

  if (racingJudgeStartMs == 0)
  {
    // 判定開始の初回だけタイマーをセット
    racingJudgeStartMs = now;
    return;
  }

  const unsigned long elapsedMs = now - racingJudgeStartMs;  // unsigned long 同士の減算はオーバーフローしても安全
  if (elapsedMs < RACING_MODE_START_DELAY_MS)
  {
    // 所定時間に満たない間は様子を見る
    return;
  }

  // 1G を所定時間継続したのでレーシングモードを開始
  isRacingMode = true;
  racingStartMs = now;
  racingPrevMode = currentBrightnessMode;
  racingJudgeStartMs = 0;
  applyBrightnessMode(BrightnessMode::Day);
}
