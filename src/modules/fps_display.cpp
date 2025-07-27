#include "fps_display.h"

#include <M5CoreS3.h>

#include "display.h"

// FPSラベルが描画済みかどうかを保持
static bool fpsLabelDrawn = false;
static unsigned long lastFpsDrawTime = 0;

// ────────────────────── FPS表示 ──────────────────────
auto drawFpsOverlay() -> bool
{
  mainCanvas.setFont(&fonts::Font0);
  mainCanvas.setTextSize(0);

  // ラベルがメーターに重ならないよう画面最下部へ配置
  constexpr int FPS_Y = LCD_HEIGHT - 16;  // 下端に合わせる
  unsigned long now = millis();
  if (!fpsLabelDrawn)
  {
    // 数値表示用に領域を初期化
    mainCanvas.fillRect(0, FPS_Y, 30, 16, COLOR_BLACK);
    fpsLabelDrawn = true;
    lastFpsDrawTime = 0;  // 初回はすぐ更新するため0に設定
  }

  if (now - lastFpsDrawTime >= 1000UL)
  {
    // 数値表示部のみ塗り直して更新
    mainCanvas.fillRect(0, FPS_Y + 8, 30, 8, COLOR_BLACK);
    mainCanvas.setCursor(0, FPS_Y + 8);
    mainCanvas.printf("%d", currentFps);
    lastFpsDrawTime = now;
    return true;
  }
  return false;
}
