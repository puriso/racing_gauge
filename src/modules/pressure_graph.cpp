#include "pressure_graph.h"

#include <algorithm>

#include "config.h"
#include "sensor.h"

// ── 油圧ログ用バッファ ──
constexpr int PRESSURE_LOG_SECONDS = 30 * 60;  // 30分のログ
static float oilPressureLog[PRESSURE_LOG_SECONDS] = {};
static int oilPressureLogIndex = 0;
static int oilPressureLogCount = 0;
static unsigned long lastPressureLogTime = 0;

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
  const int sectionCount = 3;
  const int sectionHeight = height / sectionCount;
  const int windowSeconds = 10 * 60;  // 各段は10分を表示

  canvas.setTextColor(COLOR_WHITE);
  // 縦軸ラベル
  canvas.setCursor(5, 5);
  canvas.print("油圧 / bar");

  for (int s = 0; s < sectionCount; ++s)
  {
    // 古い段から新しい段へ並べるためオフセットを計算
    int offset = (sectionCount - 1 - s) * windowSeconds;
    int available = oilPressureLogCount - offset;
    if (available <= 0) continue;  // データ不足なら描画しない

    int samplesToDraw = std::min(windowSeconds, available);
    int start = (oilPressureLogIndex - offset - samplesToDraw + PRESSURE_LOG_SECONDS) % PRESSURE_LOG_SECONDS;
    int yBase = (s + 1) * sectionHeight - 1;

    // 軸を描画
    canvas.drawLine(0, yBase, width, yBase, COLOR_WHITE);
    canvas.drawLine(0, yBase - sectionHeight + 1, 0, yBase, COLOR_WHITE);

    // 段のラベルを表示（例: 10分前, 20分前, 30分前）
    canvas.setCursor(5, yBase - sectionHeight + 5);
    canvas.printf("%d分前", (sectionCount - s) * 10);

    float samplesPerPixel = static_cast<float>(samplesToDraw) / width;
    int prevX = 0;
    float firstP = oilPressureLog[start % PRESSURE_LOG_SECONDS];
    int prevY = yBase - static_cast<int>((firstP / MAX_OIL_PRESSURE_DISPLAY) * (sectionHeight - 1));

    for (int x = 1; x < width; ++x)
    {
      int sampleIndex = start + static_cast<int>(x * samplesPerPixel);
      if (sampleIndex >= start + samplesToDraw) break;
      float p = oilPressureLog[sampleIndex % PRESSURE_LOG_SECONDS];
      int y = yBase - static_cast<int>((p / MAX_OIL_PRESSURE_DISPLAY) * (sectionHeight - 1));
      canvas.drawLine(prevX, prevY, x, y, COLOR_YELLOW);
      prevX = x;
      prevY = y;
    }
  }

  // 横軸ラベル
  canvas.setCursor(width - 40, height - 15);
  canvas.print("時間");

  canvas.pushSprite(0, 0);
}
