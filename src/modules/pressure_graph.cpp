#include "pressure_graph.h"

#include <M5CoreS3.h>

#include <algorithm>

#include "config.h"
#include "sensor.h"

// ── 油圧ログ用バッファ ──
constexpr int PRESSURE_LOG_SECONDS = 30 * 60;  // 30分のログ
static float oilPressureLog[PRESSURE_LOG_SECONDS] = {};
static int oilPressureLogIndex = 0;  // 次に書き込む位置
static int oilPressureLogCount = 0;  // 実際に記録されたサンプル数
static unsigned long lastPressureLogTime = 0;

// グラフ表示開始位置（0=最新）
static int scrollOffset = 0;
// タッチ位置記録用
static int lastTouchX = -1;

// ────────────────────── グラフ状態リセット ──────────────────────
void resetPressureGraph() { scrollOffset = 0; }

// ────────────────────── 油圧ログ追加 ──────────────────────
void logOilPressure()
{
  unsigned long now = millis();
  if (now - lastPressureLogTime >= 1000)
  {
    // 最新の油圧平均値を取得しログへ保存
    float pressure = calculateAverage(oilPressureSamples);
    oilPressureLog[oilPressureLogIndex] = pressure;
    oilPressureLogIndex = (oilPressureLogIndex + 1) % PRESSURE_LOG_SECONDS;
    if (oilPressureLogCount < PRESSURE_LOG_SECONDS)
    {
      oilPressureLogCount++;
    }
    lastPressureLogTime = now;
  }
}

// ────────────────────── 油圧グラフ描画 ──────────────────────
void drawPressureGraph(M5Canvas& canvas)
{
  canvas.fillScreen(COLOR_BLACK);

  const int width = LCD_WIDTH;
  const int height = LCD_HEIGHT;
  const int windowSeconds = width;  // 1秒=1px で表示

  // タッチでスクロール量を更新
  if (M5.Touch.getCount() > 0)
  {
    auto detail = M5.Touch.getDetail(0);
    if (lastTouchX >= 0)
    {
      scrollOffset += lastTouchX - detail.x;
      int maxOffset = std::max(0, oilPressureLogCount - windowSeconds);
      if (scrollOffset < 0) scrollOffset = 0;
      if (scrollOffset > maxOffset) scrollOffset = maxOffset;
    }
    lastTouchX = detail.x;
  }
  else
  {
    lastTouchX = -1;
  }

  canvas.setTextColor(COLOR_WHITE);
  // 軸を描画
  canvas.drawLine(0, 0, 0, height - 1, COLOR_WHITE);
  canvas.drawLine(0, height - 1, width, height - 1, COLOR_WHITE);

  // 表示するサンプル数を計算
  int samplesToDraw = std::min(windowSeconds, oilPressureLogCount - scrollOffset);
  if (samplesToDraw <= 0)
  {
    canvas.pushSprite(0, 0);
    return;
  }
  int start = (oilPressureLogIndex - scrollOffset - samplesToDraw + PRESSURE_LOG_SECONDS) % PRESSURE_LOG_SECONDS;
  float samplesPerPixel = static_cast<float>(samplesToDraw) / width;

  float firstP = oilPressureLog[start % PRESSURE_LOG_SECONDS];
  int prevX = 0;
  int prevY = height - 1 - static_cast<int>((firstP / MAX_OIL_PRESSURE_DISPLAY) * (height - 1));

  for (int x = 1; x < width; ++x)
  {
    int sampleIndex = start + static_cast<int>(x * samplesPerPixel);
    if (sampleIndex >= start + samplesToDraw) break;
    float p = oilPressureLog[sampleIndex % PRESSURE_LOG_SECONDS];
    int y = height - 1 - static_cast<int>((p / MAX_OIL_PRESSURE_DISPLAY) * (height - 1));
    canvas.drawLine(prevX, prevY, x, y, COLOR_YELLOW);
    prevX = x;
    prevY = y;
  }

  // 軸ラベル
  canvas.setCursor(5, 5);
  canvas.print("油圧 / bar");
  canvas.setCursor(width - 40, height - 15);
  canvas.print("時間");

  canvas.pushSprite(0, 0);
}
