#include "pressure_graph.h"

#include <M5CoreS3.h>

#include <algorithm>
#include <cmath>

#include "config.h"
#include "sensor.h"

// ── 定数設定 ──
constexpr int PRESSURE_LOG_SECONDS = 30 * 60;            // 30分のログ秒数
constexpr int PANEL_SECONDS = PRESSURE_LOG_SECONDS / 3;  // 各段の秒数(10分)
constexpr int HEADER_HEIGHT = 16;                        // 見出しの高さ
constexpr int PANEL_GAP = 10;                            // 段間の隙間を広げる
constexpr float HIGHLIGHT_LINE = 3.0F;                   // 色付き強調線
constexpr float Y_TICKS[] = {0.0F, 3.0F, 7.0F, 10.0F};   // Y軸目盛
constexpr float MAX_G_FORCE = 3.0F;                      // G表示の上限

// ── ログ用バッファ ──
static float oilPressureLog[PRESSURE_LOG_SECONDS] = {};
static float gForceLog[PRESSURE_LOG_SECONDS] = {};
static int logIndex = 0;  // 次に書き込む位置
static int logCount = 0;  // 実際に記録されたサンプル数
static unsigned long lastLogTime = 0;

// グラフ表示スクロール量（秒単位、0=最新）
static int scrollOffset = 0;
// タッチ位置記録用
static int lastTouchX = -1;

// ────────────────────── グラフ状態リセット ──────────────────────
void resetPressureGraph() { scrollOffset = 0; }

// ────────────────────── 油圧とGのログ追加 ──────────────────────
void logPressureAndG()
{
  unsigned long now = millis();
  if (now - lastLogTime >= 1000)
  {
    // 最新の油圧平均値を取得しログへ保存
    float pressure = calculateAverage(oilPressureSamples);
    oilPressureLog[logIndex] = pressure;

    // IMUから加速度を取得しG値を計算
    float accX, accY, accZ;
    M5.Imu.getAccel(&accX, &accY, &accZ);
    float gValue = std::sqrt((accX * accX) + (accY * accY) + (accZ * accZ));
    gForceLog[logIndex] = gValue;

    logIndex = (logIndex + 1) % PRESSURE_LOG_SECONDS;
    if (logCount < PRESSURE_LOG_SECONDS)
    {
      logCount++;
    }
    lastLogTime = now;
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
    for (float tick : Y_TICKS)
    {
      int gy = panelY + panelHeight - 1 - static_cast<int>((tick / MAX_OIL_PRESSURE_METER) * (panelHeight - 1));
      if (tick == HIGHLIGHT_LINE)
      {
        // 色付き強調線
        canvas.drawLine(0, gy, width - 1, gy, COLOR_YELLOW);
      }
      else
      {
        drawDashedHLine(canvas, gy, 0, width - 1, COLOR_GRAY);
      }
      canvas.setCursor(0, gy - 7);
      canvas.printf("%.0f", tick);
    }

    // データを描画
    int prevPX = -1;
    int prevPY = 0;
    int prevGX = -1;
    int prevGY = 0;
    for (int x = width - 1; x >= 0; --x)
    {
      int secAgo = endSec + (width - 1 - x);
      if (secAgo >= logCount) continue;  // データ不足
      int index = (logIndex - 1 - secAgo + PRESSURE_LOG_SECONDS) % PRESSURE_LOG_SECONDS;

      float pressure = oilPressureLog[index];
      int py = panelY + panelHeight - 1 - static_cast<int>((pressure / MAX_OIL_PRESSURE_METER) * (panelHeight - 1));
      if (prevPX >= 0)
      {
        canvas.drawLine(prevPX, prevPY, x, py, COLOR_WHITE);
      }
      prevPX = x;
      prevPY = py;

      float gValue = gForceLog[index];
      gValue = std::min(gValue, MAX_G_FORCE);
      int gy = panelY + panelHeight - 1 - static_cast<int>((gValue / MAX_G_FORCE) * (panelHeight - 1));
      if (prevGX >= 0)
      {
        canvas.drawLine(prevGX, prevGY, x, gy, COLOR_RED);
      }
      prevGX = x;
      prevGY = gy;
    }

    // 段の境界線
    if (panel < 2)
    {
      canvas.drawLine(0, panelY + panelHeight, width - 1, panelY + panelHeight, COLOR_GRAY);
    }
  }

  canvas.pushSprite(0, 0);
}
