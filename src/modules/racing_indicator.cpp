#include "racing_indicator.h"

// レーシングモードかどうかを保持
bool isRacingMode = false;

// Rマークが描画済みかどうか
static bool indicatorDrawn = false;

// ────────────────────── レーシング中表示 ──────────────────────
bool drawRacingIndicator(M5Canvas &canvas)
{
  constexpr int INDICATOR_X = 2;
  constexpr int INDICATOR_Y = LCD_HEIGHT - 16;

  if (isRacingMode)
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
