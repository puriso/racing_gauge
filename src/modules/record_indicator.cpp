#include "record_indicator.h"

// 録画モードかどうかを保持
bool isRecordingMode = false;

// Rマークが描画済みかどうか
static bool indicatorDrawn = false;

// ────────────────────── 録画中表示 ──────────────────────
bool drawRecordingIndicator(M5Canvas &canvas)
{
  constexpr int INDICATOR_X = 2;
  constexpr int INDICATOR_Y = LCD_HEIGHT - 16;

  if (isRecordingMode)
  {
    if (!indicatorDrawn)
    {
      canvas.setFont(&fonts::Font0);
      canvas.setTextSize(0);
      canvas.setTextColor(COLOR_RED, COLOR_BLACK);
      canvas.setCursor(INDICATOR_X, INDICATOR_Y);
      canvas.print("R");
      indicatorDrawn = true;
      return true;
    }
  }
  else if (indicatorDrawn)
  {
    canvas.fillRect(INDICATOR_X, INDICATOR_Y, 8, 8, COLOR_BLACK);
    indicatorDrawn = false;
    return true;
  }
  return false;
}
