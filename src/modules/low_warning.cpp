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

// 油圧警告表示。現在の表示状態とその変更の有無を返す
bool drawLowPressureWarning(M5Canvas &canvas, float gForce, float pressure, bool &stateChanged)
{
  // ゲージ位置とサイズ
  constexpr int GAUGE_X = 0;    // 油圧ゲージの左上X
  constexpr int GAUGE_Y = 60;   // 油圧ゲージの左上Y
  constexpr int GAUGE_W = 160;  // ゲージ幅
  constexpr int GAUGE_H = 170;  // ゲージ高さ

  canvas.setFont(&fonts::FreeSansBold12pt7b);
  // 警告文字列は固定で "LOW"
  constexpr char WARN_TEXT[] = "LOW";
  int textW = canvas.textWidth(WARN_TEXT);
  int textH = canvas.fontHeight();
  constexpr int PADDING = 4;  // ボックス余白
  int boxW = textW + (PADDING * 2);
  int boxH = textH + (PADDING * 2);
  int boxX = GAUGE_X + ((GAUGE_W - boxW) / 2);
  int boxY = GAUGE_Y + ((GAUGE_H - boxH) / 2);
  static int lastBoxX = boxX;  // 最後に描画したボックス座標
  static int lastBoxY = boxY;
  static int lastBoxW = boxW;
  static int lastBoxH = boxH;

  // 横Gが1Gを超えた場合に判定
  constexpr float G_FORCE_THRESHOLD = 1.0F;          // G判定値
  constexpr float PRESSURE_THRESHOLD = 3.0F;         // 油圧閾値
  constexpr unsigned long WARNING_DELAY_MS = 500UL;  // 継続時間
  bool conditionMet = (gForce > G_FORCE_THRESHOLD && pressure <= PRESSURE_THRESHOLD);
  static unsigned long startMs = 0;  // 条件成立開始時刻
  static bool isShowing = false;
  static float peakG = 0.0F;                                     // 期間中の最大G
  static float minPressure = std::numeric_limits<float>::max();  // 期間中の最低油圧
  static const char *eventDir = "Right";                         // 発生方向
  unsigned long now = millis();
  bool shouldShow = false;

  if (conditionMet)
  {
    if (startMs == 0)
    {
      startMs = now;
      peakG = gForce;
      minPressure = pressure;
      eventDir = currentGDirection;
    }
    else
    {
      peakG = std::max(peakG, gForce);
      minPressure = std::min(minPressure, pressure);
    }
    if (now - startMs >= WARNING_DELAY_MS)
    {
      // 0.5秒以上継続したら警告表示
      // サイズ変化に対応するため以前のボックスを消去
      if (isShowing)
      {
        canvas.fillRect(lastBoxX, lastBoxY, lastBoxW, lastBoxH, COLOR_BLACK);
      }
      // 赤背景に固定文言を表示
      canvas.fillRect(boxX, boxY, boxW, boxH, COLOR_RED);
      canvas.setTextColor(COLOR_WHITE, COLOR_RED);
      canvas.setCursor(boxX + ((boxW - textW) / 2), boxY + ((boxH - textH) / 2));
      canvas.print(WARN_TEXT);
      // 消去用にボックス位置とサイズを保持
      lastBoxX = boxX;
      lastBoxY = boxY;
      lastBoxW = boxW;
      lastBoxH = boxH;
      shouldShow = true;
    }
  }
  else
  {
    // 条件外になったらタイマーをリセットし、表示していれば消去
    if (startMs != 0)
    {
      // 条件解除時にイベント情報を記録
      lastLowEventG = peakG;
      lastLowEventDir = eventDir;
      lastLowEventDuration = (now - startMs) / 1000.0F;
      lastLowEventPressure = minPressure;
    }
    startMs = 0;
    if (isShowing)
    {
      // 直前に描画した領域を完全に消去
      canvas.fillRect(lastBoxX, lastBoxY, lastBoxW, lastBoxH, COLOR_BLACK);
    }
  }

  stateChanged = (shouldShow != isShowing);
  isShowing = shouldShow;
  return shouldShow;
}
