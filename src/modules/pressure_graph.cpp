#include "pressure_graph.h"

#include <M5CoreS3.h>

#include <algorithm>

#include "config.h"
#include "sensor.h"

// ── 定数設定 ──
constexpr int PRESSURE_LOG_SECONDS = 30 * 60;            // 30分のログ秒数
constexpr int PANEL_SECONDS = PRESSURE_LOG_SECONDS / 3;  // 各段の秒数(10分)
constexpr int HEADER_HEIGHT = 16;                        // 見出しの高さ
constexpr int PANEL_GAP = 4;                             // 段間の隙間
constexpr float GUIDE_LINES[] = {3.0f, 6.0f, 8.0f};      // 強調線の油圧値

// ── 油圧ログ用バッファ ──
static float oilPressureLog[PRESSURE_LOG_SECONDS] = {};
static int oilPressureLogIndex = 0;  // 次に書き込む位置
static int oilPressureLogCount = 0;  // 実際に記録されたサンプル数
static unsigned long lastPressureLogTime = 0;

// グラフ表示スクロール量（秒単位、0=最新）
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

// ────────────────────── 破線描画ヘルパ ──────────────────────
static void drawDashedVLine(M5Canvas& canvas, int x, int y1, int y2, uint16_t color)
{
  // y方向の破線を描画する
  const int dash = 2;
  const int gap = 2;
  for (int y = y1; y <= y2; y += dash + gap)
  {
    int y2p = std::min(y + dash - 1, y2);
    canvas.drawLine(x, y, x, y2p, color);
  }
}

static void drawDashedHLine(M5Canvas& canvas, int y, int x1, int x2, uint16_t color)
{
  // x方向の破線を描画する
  const int dash = 2;
  const int gap = 2;
  for (int x = x1; x <= x2; x += dash + gap)
  {
    int x2p = std::min(x + dash - 1, x2);
    canvas.drawLine(x, y, x2p, y, color);
  }
}

// ────────────────────── 油圧グラフ描画 ──────────────────────
void drawPressureGraph(M5Canvas& canvas)
{
  canvas.fillScreen(COLOR_BLACK);

  const int width = LCD_WIDTH;
  const int height = LCD_HEIGHT;
  // 隙間を考慮した各段の高さ
  const int panelHeight = (height - HEADER_HEIGHT - PANEL_GAP * 2) / 3;
  const int maxScroll = std::max(0, PANEL_SECONDS - width);

  // ── タッチによるスクロール処理 ──
  if (M5.Touch.getCount() > 0)
  {
    auto detail = M5.Touch.getDetail(0);
    if (lastTouchX >= 0)
    {
      scrollOffset += lastTouchX - detail.x;
      if (scrollOffset < 0) scrollOffset = 0;
      if (scrollOffset > maxScroll) scrollOffset = maxScroll;
    }
    lastTouchX = detail.x;
  }
  else
  {
    lastTouchX = -1;
  }

  canvas.setTextColor(COLOR_WHITE);
  canvas.setTextSize(1);
  canvas.setCursor(0, 0);
  canvas.print("OIL.P Log");

  // ── 各段の描画 ──
  for (int panel = 0; panel < 3; ++panel)
  {
    int panelY = HEADER_HEIGHT + panel * (panelHeight + PANEL_GAP);
    int endSec = panel * PANEL_SECONDS + scrollOffset;  // 右端の秒数

    // 背景グリッドを描画
    for (int x = 0; x < width; ++x)
    {
      int secAgo = endSec + (width - 1 - x);
      if (secAgo % 60 == 0)
      {
        drawDashedVLine(canvas, x, panelY, panelY + panelHeight - 1, COLOR_GRAY);
        canvas.setCursor(x + 2, panelY + panelHeight - 10);
        canvas.printf("%d", secAgo / 60);
      }
    }
    for (int y = 0; y <= static_cast<int>(MAX_OIL_PRESSURE_METER); ++y)
    {
      int gy = panelY + panelHeight - 1 - static_cast<int>((y / MAX_OIL_PRESSURE_METER) * (panelHeight - 1));
      drawDashedHLine(canvas, gy, 0, width - 1, COLOR_GRAY);
      canvas.setCursor(0, gy - 7);
      canvas.printf("%d", y);
    }

    // 強調線を描画
    for (float g : GUIDE_LINES)
    {
      int gy = panelY + panelHeight - 1 - static_cast<int>((g / MAX_OIL_PRESSURE_METER) * (panelHeight - 1));
      uint16_t color = COLOR_YELLOW;
      if (g >= 6.0f) color = COLOR_ORANGE;
      if (g >= 8.0f) color = COLOR_RED;
      canvas.drawLine(0, gy, width, gy, color);
      canvas.drawLine(0, gy + 1, width, gy + 1, color);  // 太線にする
    }

    // データを描画
    int prevX = -1;
    int prevY = 0;
    for (int x = width - 1; x >= 0; --x)
    {
      int secAgo = endSec + (width - 1 - x);
      if (secAgo >= oilPressureLogCount) continue;  // データ不足
      int index = (oilPressureLogIndex - 1 - secAgo + PRESSURE_LOG_SECONDS) % PRESSURE_LOG_SECONDS;
      float p = oilPressureLog[index];
      int y = panelY + panelHeight - 1 - static_cast<int>((p / MAX_OIL_PRESSURE_METER) * (panelHeight - 1));
      if (prevX >= 0)
      {
        canvas.drawLine(prevX, prevY, x, y, COLOR_WHITE);
      }
      prevX = x;
      prevY = y;
    }
  }

  canvas.pushSprite(0, 0);
}
