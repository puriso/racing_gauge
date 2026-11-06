#include "low_warning.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "config.h"
#include "sensor.h"

// 直近の低油圧イベント情報
float lastLowEventG = 0.0F;             // 発生時のG値
const char* lastLowEventDir = "Right";  // Gの向き
float lastLowEventDuration = 0.0F;      // 継続時間[s]
float lastLowEventPressure = 0.0F;      // そのときの油圧[bar]

// 警告表示の状態をまとめた構造体
struct LowWarningState
{
  uint32_t startMs = 0;                                   // 条件成立開始時刻
  uint32_t lastWarningMs = 0;                             // 最後に警告条件を満たした時刻
  bool eventActive = false;                               // 条件成立中か
  bool eventQualified = false;                            // 0.5秒継続して警告対象になったか
  bool warningHeld = false;                               // 警告の表示保持中か
  bool isShowing = false;                                 // 現在表示中か
  float peakG = 0.0F;                                     // 期間中の最大G
  float minPressure = std::numeric_limits<float>::max();  // 期間中の最低油圧
  const char* eventDir = "Right";                         // 発生方向
  int boxX = 0;                                           // 最後に描画したボックス座標
  int boxY = 0;
  int boxW = 0;
  int boxH = 0;
};

// 油圧警告表示。現在の表示状態とその変更の有無を返す
bool drawLowPressureWarning(M5Canvas& canvas, float gForce, float pressure, bool& stateChanged)
{
  constexpr int GAUGE_X = 0;    // 油圧ゲージの左上X
  constexpr int GAUGE_Y = 60;   // 油圧ゲージの左上Y
  constexpr int GAUGE_W = 160;  // ゲージ幅
  constexpr int GAUGE_H = 170;  // ゲージ高さ

  canvas.setFont(&fonts::FreeSansBold12pt7b);
  constexpr char WARN_TEXT[] = "LOW";  // 警告文字列
  constexpr int PADDING = 4;           // ボックス余白

  // テキスト幅などを初回だけ計算し、以降はキャッシュした値を再利用して描画処理を軽量化する
  struct WarningLayout
  {
    bool initialized = false;
    int boxX = 0;
    int boxY = 0;
    int boxW = 0;
    int boxH = 0;
  };
  static WarningLayout layout;

  if (!layout.initialized)
  {
    int textW = canvas.textWidth(WARN_TEXT);
    int textH = canvas.fontHeight();
    layout.boxW = textW + (PADDING * 2) - 1;
    layout.boxH = textH + (PADDING * 2) - 2;
    layout.boxX = GAUGE_X + ((GAUGE_W - layout.boxW) / 2 - 8);
    layout.boxY = GAUGE_Y + ((GAUGE_H - layout.boxH) / 2);
    layout.initialized = true;
  }

  static LowWarningState state;  // 表示状態

  constexpr float G_FORCE_THRESHOLD = 1.0F;   // G判定値
  constexpr float PRESSURE_THRESHOLD = 3.0F;  // 油圧閾値
  constexpr uint32_t WARNING_DELAY_MS = 500;  // 継続時間
  constexpr uint32_t WARNING_HOLD_MS = 3000;  // 表示継続時間
  bool conditionMet = (gForce > G_FORCE_THRESHOLD && pressure <= PRESSURE_THRESHOLD);
  uint32_t now = millis();
  bool prevShowing = state.isShowing;  // 前回表示中だったか

  if (conditionMet)
  {
    if (!state.eventActive)
    {
      // 新しいイベント開始
      state.startMs = now;
      state.peakG = gForce;
      state.minPressure = pressure;
      state.eventDir = currentGDirection;
      state.eventActive = true;
      state.eventQualified = false;
    }
    else
    {
      // イベント継続中は最大/最小値を更新
      state.peakG = std::max(state.peakG, gForce);
      state.minPressure = std::min(state.minPressure, pressure);
    }
    uint32_t elapsedMs = now - state.startMs;
    if (elapsedMs >= WARNING_DELAY_MS)
    {
      // 0.5秒以上継続したら警告表示
      state.lastWarningMs = now;
      state.warningHeld = true;
      state.eventQualified = true;
    }
  }
  else
  {
    if (state.eventActive && state.eventQualified)
    {
      // 条件解除時にイベント情報を記録
      lastLowEventG = state.peakG;
      lastLowEventDir = state.eventDir;
      uint32_t eventDurationMs = now - state.startMs;
      lastLowEventDuration = static_cast<float>(eventDurationMs) / 1000.0F;
      lastLowEventPressure = state.minPressure;
    }
    state.eventActive = false;
    state.eventQualified = false;
  }

  // 表示保持とイベント判定を独立させ、再発時も前の警告を保持する。
  // 絶対時刻を比較せず、millis() の32ビット折り返しを扱える差分で判定する。
  uint32_t warningElapsedMs = now - state.lastWarningMs;
  bool shouldShow = state.warningHeld && warningElapsedMs < WARNING_HOLD_MS;
  if (!shouldShow)
  {
    state.warningHeld = false;
  }

  if (shouldShow)
  {
    // 警告表示を毎フレーム再描画
    canvas.fillRect(layout.boxX, layout.boxY, layout.boxW, layout.boxH, COLOR_RED);
    canvas.setTextColor(COLOR_WHITE, COLOR_RED);
    canvas.setTextDatum(m5gfx::textdatum_t::middle_center);
    canvas.drawString(WARN_TEXT, layout.boxX + (layout.boxW / 2), layout.boxY + (layout.boxH / 2));
    canvas.setTextDatum(m5gfx::textdatum_t::top_left);
    state.boxX = layout.boxX;
    state.boxY = layout.boxY;
    state.boxW = layout.boxW;
    state.boxH = layout.boxH;
  }
  else if (prevShowing)
  {
    // 表示継続時間が過ぎたので警告を消去
    canvas.fillRect(state.boxX, state.boxY, state.boxW, state.boxH, COLOR_BLACK);
    // 表示の消去では、判定待ちの次のイベントをリセットしない。
  }

  stateChanged = (shouldShow != prevShowing);
  state.isShowing = shouldShow;
  return shouldShow;
}
