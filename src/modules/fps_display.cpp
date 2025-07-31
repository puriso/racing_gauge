#include "fps_display.h"

#include <M5CoreS3.h>

#include "backlight.h"
#include "display.h"

// ────────────────────── 補助関数 ──────────────────────
// 照度値を文字列へ変換する（1000以上は1.1k形式で表示）
static auto formatLuxValue(int lux) -> const char*
{
  static char buf[8];
  if (lux >= 1000)
  {
    int kilo = lux / 1000;
    int tenth = (lux % 1000) / 100;
    snprintf(buf, sizeof(buf), "%d.%dk", kilo, tenth);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%d", lux);
  }
  return buf;
}

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
    mainCanvas.fillRect(0, FPS_Y, 60, 16, COLOR_BLACK);
    fpsLabelDrawn = true;
    lastFpsDrawTime = 0;  // 初回はすぐ更新するため0に設定
  }

  if (now - lastFpsDrawTime >= 1000UL)
  {
    // 数値表示部を塗り直して更新
    mainCanvas.fillRect(0, FPS_Y, 60, 16, COLOR_BLACK);
    mainCanvas.setCursor(0, FPS_Y);
    mainCanvas.printf("L%s", formatLuxValue(lastLuxValue));
    mainCanvas.setCursor(0, FPS_Y + 8);
    mainCanvas.printf("F%d", currentFps);
    lastFpsDrawTime = now;
    return true;
  }
  return false;
}
