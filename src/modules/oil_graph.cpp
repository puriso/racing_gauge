#include "oil_graph.h"

#include "config.h"
#include "display.h"

// ────────────────────── 定数 ──────────────────────
constexpr int GRAPH_MINUTES = 30;
constexpr int POINTS_PER_MINUTE = 60;  // 1秒ごとの点
constexpr int HISTORY_SIZE = GRAPH_MINUTES * POINTS_PER_MINUTE;

// ────────────────────── 履歴バッファ ──────────────────────
static float pressureHistory[HISTORY_SIZE] = {};
static int historyIndex = 0;
static int validPoints = 0;

void initOilPressureHistory()
{
  for (float &v : pressureHistory)
  {
    v = 0.0f;
  }
  historyIndex = 0;
  validPoints = 0;
}

void addOilPressureHistory(float pressure)
{
  pressureHistory[historyIndex] = pressure;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  if (validPoints < HISTORY_SIZE)
  {
    validPoints++;
  }
}

int getOilGraphCount()
{
  // 1分=60点ごとのグラフ数
  return validPoints / POINTS_PER_MINUTE;
}

void drawOilPressureGraph(int index)
{
  if (index < 0 || index >= GRAPH_MINUTES)
  {
    return;
  }
  mainCanvas.fillScreen(COLOR_BLACK);
  mainCanvas.drawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, COLOR_GRAY);

  int start = (historyIndex - (index + 1) * POINTS_PER_MINUTE + HISTORY_SIZE) % HISTORY_SIZE;
  int xStep = LCD_WIDTH / POINTS_PER_MINUTE;
  int prevX = 0;
  float prevYValue = pressureHistory[start];
  for (int i = 1; i < POINTS_PER_MINUTE; i++)
  {
    int idx = (start + i) % HISTORY_SIZE;
    float value = pressureHistory[idx];
    int x0 = prevX;
    int x1 = i * xStep;
    int y0 = LCD_HEIGHT - static_cast<int>(prevYValue / MAX_OIL_PRESSURE_METER * LCD_HEIGHT);
    int y1 = LCD_HEIGHT - static_cast<int>(value / MAX_OIL_PRESSURE_METER * LCD_HEIGHT);
    mainCanvas.drawLine(x0, y0, x1, y1, COLOR_ORANGE);
    prevX = x1;
    prevYValue = value;
  }
  mainCanvas.setCursor(4, 4);
  mainCanvas.setTextColor(COLOR_WHITE);
  mainCanvas.setFont(&fonts::Font0);
  mainCanvas.printf("OIL.P Graph %d/%d", index + 1, getOilGraphCount());
  mainCanvas.pushSprite(0, 0);
}
