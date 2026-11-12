#ifndef LOW_WARNING_H
#define LOW_WARNING_H

#include <M5GFX.h>

// 直近の低油圧イベント情報
extern float lastLowEventG;          // 発生時のG値
extern const char* lastLowEventDir;  // Gの向き
extern float lastLowEventDuration;   // 継続時間[s]
extern float lastLowEventPressure;   // そのときの油圧[bar]

// 低油圧警告表示。解除後も3秒間表示を継続し、現在の表示状態とその変更の有無を返す
bool drawLowPressureWarning(M5Canvas& canvas, float gForce, float pressure, bool& stateChanged);
void updateLowPressureWarning(float gForce, float pressure, bool pressureValid = true);
bool drawLowPressureWarning(M5Canvas& canvas, bool& stateChanged, bool backgroundChanged = false);
void invalidateLowPressureWarning();
auto isLowPressureQualified() -> bool;

// LOW成立の履歴。画面再描画では保持し、明示的なリセットで消去する。
auto hasLowWarningHistory() -> bool;
void resetLowWarningHistory();
auto drawLowWarningHistory(M5Canvas& canvas) -> bool;
void invalidateLowWarningHistoryIndicator();

#endif  // LOW_WARNING_H
