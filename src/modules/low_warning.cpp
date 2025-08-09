#include "low_warning.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "config.h"
#include "sensor.h"

// 直近の低油圧イベント情報
float lastLowEventG = 0.0F;             // 発生時のG値
const char *lastLowEventDir = "Right";  // Gの向き
float lastLowEventDuration = 0.0F;      // 継続時間[s]
float lastLowEventPressure = 0.0F;      // そのときの油圧[bar]

// 警告表示の状態をまとめた構造体
struct LowWarningState
{
  unsigned long startMs = 0;                              // 条件成立開始時刻
  bool isShowing = false;                                 // 現在表示中か
  float peakG = 0.0F;                                     // 期間中の最大G
  float minPressure = std::numeric_limits<float>::max();  // 期間中の最低油圧
  const char *eventDir = "Right";                         // 発生方向
  int boxX = 0;                                           // 最後に描画したボックス座標
  int boxY = 0;
  int boxW = 0;
  int boxH = 0;
};

// 油圧警告表示。現在の表示状態とその変更の有無を返す
bool drawLowPressureWarning(M5Canvas &canvas, float gForce, float pressure, bool &stateChanged)
{
  constexpr int GAUGE_X = 0;    // 油圧ゲージの左上X
  constexpr int GAUGE_Y = 60;   // 油圧ゲージの左上Y
  constexpr int GAUGE_W = 160;  // ゲージ幅
  constexpr int GAUGE_H = 170;  // ゲージ高さ

  canvas.setFont(&fonts::FreeSansBold12pt7b);
  constexpr char WARN_TEXT[] = "LOW";  // 警告文字列
  int textW = canvas.textWidth(WARN_TEXT);
  int textH = canvas.fontHeight();
  constexpr int PADDING = 4;  // ボックス余白
  int boxW = textW + (PADDING * 2);
  int boxH = textH + (PADDING * 2);
  int boxX = GAUGE_X + ((GAUGE_W - boxW) / 2);
  int boxY = GAUGE_Y + ((GAUGE_H - boxH) / 2);

  static LowWarningState state;  // 表示状態

  constexpr float G_FORCE_THRESHOLD = 1.0F;          // G判定値
  constexpr float PRESSURE_THRESHOLD = 3.0F;         // 油圧閾値
  constexpr unsigned long WARNING_DELAY_MS = 500UL;  // 継続時間
  bool conditionMet = (gForce > G_FORCE_THRESHOLD && pressure <= PRESSURE_THRESHOLD);
  unsigned long now = millis();
  bool shouldShow = false;

  if (conditionMet)
  {
    if (state.startMs == 0)
    {
      state.startMs = now;
      state.peakG = gForce;
      state.minPressure = pressure;
      state.eventDir = currentGDirection;
    }
    else
    {
      state.peakG = std::max(state.peakG, gForce);
      state.minPressure = std::min(state.minPressure, pressure);
    }
    if (now - state.startMs >= WARNING_DELAY_MS)
    {
      // 0.5秒以上継続したら警告表示
      if (state.isShowing)
      {
        canvas.fillRect(state.boxX, state.boxY, state.boxW, state.boxH, COLOR_BLACK);
      }
      canvas.fillRect(boxX, boxY, boxW, boxH, COLOR_RED);
      canvas.setTextColor(COLOR_WHITE, COLOR_RED);
      canvas.setTextDatum(m5gfx::textdatum_t::middle_center);
      canvas.drawString(WARN_TEXT, boxX + (boxW / 2), boxY + (boxH / 2));
      canvas.setTextDatum(m5gfx::textdatum_t::top_left);
      state.boxX = boxX;
      state.boxY = boxY;
      state.boxW = boxW;
      state.boxH = boxH;
      shouldShow = true;
    }
  }
  else
  {
    if (state.startMs != 0)
    {
      // 条件解除時にイベント情報を記録
      lastLowEventG = state.peakG;
      lastLowEventDir = state.eventDir;
      lastLowEventDuration = (now - state.startMs) / 1000.0F;
      lastLowEventPressure = state.minPressure;
    }
    state.startMs = 0;
    if (state.isShowing)
    {
      canvas.fillRect(state.boxX, state.boxY, state.boxW, state.boxH, COLOR_BLACK);
    }
  }

  stateChanged = (shouldShow != state.isShowing);
  state.isShowing = shouldShow;
  return shouldShow;
}
