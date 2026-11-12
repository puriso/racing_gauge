#include "risk_graph.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "display.h"
#include "risk_graph_layout.h"
#include "risk_history.h"

namespace
{
constexpr int LEFT = 30;
constexpr int WIDTH = 258;
constexpr int PRESSURE_TOP = 52;
constexpr int PRESSURE_BOTTOM = 151;
constexpr int OIL_TOP = 177;
constexpr int OIL_BOTTOM = 210;
constexpr int OIL_LABEL_PADDING = 5;
constexpr uint16_t PRESSURE_COLOR = COLOR_YELLOW;
constexpr uint16_t G_COLOR = rgb565(64, 208, 255);
constexpr uint16_t GRID_COLOR = rgb565(45, 52, 61);
constexpr uint16_t OIL_COLOR = rgb565(255, 160, 64);

auto yFor(float value, float minimum, float maximum, int top, int bottom) -> int
{
  const float fraction = std::clamp((value - minimum) / (maximum - minimum), 0.0F, 1.0F);
  return bottom - static_cast<int>(std::lround(fraction * (bottom - top)));
}

void header()
{
  mainCanvas.fillRect(0, 0, LCD_WIDTH, 24, COLOR_BLACK);
  char title[64];
  if (risk::history.hasSelection())
  {
    const auto seconds = risk::history.ageMs() / 1000;
    snprintf(title, sizeof(title), "%s %llum%02llus ago",
             risk::history.hasEvent() ? risk::name(risk::history.event().kind) : "PEAK G",
             static_cast<unsigned long long>(seconds / 60), static_cast<unsigned long long>(seconds % 60));
  }
  else
  {
    snprintf(title, sizeof(title), "WAITING");
  }

  // 詳細画面と同じ書体に揃え、長い経過時間もDETAILや下段へはみ出させない。
  mainCanvas.setTextSize(1);
  mainCanvas.setFont(&fonts::FreeSansBold12pt7b);
  if (mainCanvas.textWidth(title) > 248) mainCanvas.setFont(&fonts::FreeSansBold9pt7b);
  mainCanvas.setTextColor(!risk::history.hasEvent()                       ? G_COLOR
                          : risk::history.event().kind == risk::Kind::Low ? COLOR_RED
                                                                          : COLOR_ORANGE,
                          COLOR_BLACK);
  mainCanvas.setClipRect(4, 0, 248, 24);
  mainCanvas.setCursor(4, (24 - mainCanvas.fontHeight()) / 2);
  mainCanvas.print(title);
  mainCanvas.clearClipRect();

  mainCanvas.setFont(&fonts::Font0);
  mainCanvas.setTextColor(COLOR_WHITE, COLOR_BLACK);
  mainCanvas.drawRect(256, 0, 64, 23, COLOR_GRAY);
  mainCanvas.setCursor(270, 8);
  mainCanvas.print("DETAIL");
}

// 1画素に複数の100ms区間が入る場合も、短い低圧・Gピークを縦線で残す。
void series(uint8_t flag, float minimum, float maximum, int top, int bottom, uint16_t color)
{
  bool previousValid = false;
  int previousY = bottom;
  for (int column = 0; column <= WIDTH; ++column)
  {
    size_t first = static_cast<size_t>(column) * risk::WINDOW_SAMPLES / (WIDTH + 1);
    size_t end = static_cast<size_t>(column + 1) * risk::WINDOW_SAMPLES / (WIDTH + 1);
    float low = std::numeric_limits<float>::max();
    float high = std::numeric_limits<float>::lowest();
    float last = 0;
    bool valid = true;
    for (size_t index = first; index < end; ++index)
    {
      const auto& sample = risk::history.sample(index);
      const bool error = (flag == risk::PressureValid && (sample.flags & risk::PressureError) != 0) ||
                         (flag == risk::OilValid && (sample.flags & risk::OilError) != 0);
      if ((sample.flags & flag) == 0 || error)
      {
        valid = false;
        continue;
      }
      float value = flag == risk::PressureValid ? sample.pressureMin / 100.0F
                    : flag == risk::GValid      ? sample.gMax / 100.0F
                                                : sample.oilLast / 10.0F;
      low = std::min(low, value);
      high = std::max(high, flag == risk::PressureValid ? sample.pressureMax / 100.0F : value);
      last = value;
    }
    if (valid && low <= high)
    {
      const int x = LEFT + column;
      const int y = yFor(last, minimum, maximum, top, bottom);
      mainCanvas.drawLine(x, yFor(low, minimum, maximum, top, bottom), x, yFor(high, minimum, maximum, top, bottom), color);
      if (previousValid)
      {
        mainCanvas.drawLine(x - 1, previousY, x, y, color);
      }
      previousY = y;
      previousValid = true;
    }
    else
    {
      {
        previousValid = false;
      }
    }
  }
}

void graph()
{
  mainCanvas.fillScreen(COLOR_BLACK);
  header();
  mainCanvas.setFont(&fonts::Font0);
  mainCanvas.setTextSize(1);
  if (!risk::history.hasSelection())
  {
    mainCanvas.setTextColor(COLOR_WHITE, COLOR_BLACK);
    mainCanvas.setCursor(34, 78);
    mainCanvas.print("Waiting for valid sensor data");
    mainCanvas.setCursor(34, 105);
    mainCanvas.print("Pressure + G + oil temperature");
    mainCanvas.setCursor(34, 132);
    mainCanvas.print("Peak G, then lowest pressure");
    mainCanvas.setCursor(4, 230);
    mainCanvas.print("Tap: return to gauges");
    return;
  }

  const auto& event = risk::history.event();
  mainCanvas.setTextColor(COLOR_WHITE, COLOR_BLACK);
  mainCanvas.setCursor(4, 27);
  mainCanvas.print("0s:");
  mainCanvas.setTextColor(PRESSURE_COLOR, COLOR_BLACK);
  mainCanvas.printf(" %.2fbar", event.pressure);
  mainCanvas.setTextColor(G_COLOR, COLOR_BLACK);
  mainCanvas.printf(" %.2fG %s", event.g, gDirectionLabel(event.gDirection));
  mainCanvas.setTextColor(OIL_COLOR, COLOR_BLACK);
  mainCanvas.print(" OIL.T:");
  if (event.oilValid)
  {
    mainCanvas.printf("%.1fC", event.oil);
  }
  else
  {
    mainCanvas.print("---");
  }
  mainCanvas.setCursor(105, 39);
  mainCanvas.setTextColor(COLOR_GRAY, COLOR_BLACK);
  mainCanvas.print(!risk::history.hasEvent()       ? "100ms (peak G)"
                   : event.kind == risk::Kind::Low ? "100ms"
                                                   : "100ms (estimate)");
  mainCanvas.setCursor(4, 39);
  mainCanvas.setTextColor(PRESSURE_COLOR);
  mainCanvas.print("OIL.P");
  mainCanvas.setTextColor(G_COLOR);
  mainCanvas.setCursor(265, 39);
  mainCanvas.print("G");

  float maxPressure = 10, maxG = 2;
  const auto oilRange = risk::oilTemperatureRange(risk::history);
  const float minOil = oilRange.minimum;
  const float maxOil = oilRange.maximum;
  for (size_t index = 0; index < risk::WINDOW_SAMPLES; ++index)
  {
    const auto& sample = risk::history.sample(index);
    if ((sample.flags & risk::PressureValid) != 0)
    {
      maxPressure = std::max(maxPressure, sample.pressureMax / 100.0F);
    }
    if ((sample.flags & risk::GValid) != 0)
    {
      maxG = std::max(maxG, sample.gMax / 100.0F);
    }
  }
  maxPressure = std::ceil(maxPressure);
  maxG = std::ceil(maxG);

  for (int position : {0, WIDTH / 2, WIDTH})
  {
    mainCanvas.drawLine(LEFT + position, PRESSURE_TOP, LEFT + position, OIL_BOTTOM, GRID_COLOR);
  }
  for (int y : {PRESSURE_TOP, PRESSURE_BOTTOM, OIL_TOP, OIL_BOTTOM})
  {
    mainCanvas.drawLine(LEFT, y, LEFT + WIDTH, y, GRID_COLOR);
  }
  const int warningY = yFor(3, 0, maxPressure, PRESSURE_TOP, PRESSURE_BOTTOM);
  for (int x = LEFT; x < LEFT + WIDTH; x += 6)
  {
    mainCanvas.drawFastHLine(x, warningY, 3, COLOR_RED);
  }
  mainCanvas.setTextColor(PRESSURE_COLOR, COLOR_BLACK);
  mainCanvas.setCursor(4, PRESSURE_TOP);
  mainCanvas.printf("%.0f", maxPressure);
  mainCanvas.setCursor(4, warningY - 4);
  mainCanvas.print("3");
  mainCanvas.setCursor(4, PRESSURE_BOTTOM - 7);
  mainCanvas.print("0");
  mainCanvas.setTextColor(G_COLOR, COLOR_BLACK);
  mainCanvas.setCursor(295, PRESSURE_TOP);
  mainCanvas.printf("%.0f", maxG);
  mainCanvas.setCursor(295, PRESSURE_BOTTOM - 7);
  mainCanvas.print("0");
  mainCanvas.setTextColor(OIL_COLOR, COLOR_BLACK);
  mainCanvas.setCursor(LEFT, 164);
  mainCanvas.print("OIL.T [C]");
  mainCanvas.setCursor(0, OIL_TOP);
  mainCanvas.printf("%.0f", maxOil);
  mainCanvas.setCursor(0, OIL_BOTTOM - 7);
  mainCanvas.printf("%.0f", minOil);

  series(risk::PressureValid, 0, maxPressure, PRESSURE_TOP, PRESSURE_BOTTOM, PRESSURE_COLOR);
  series(risk::GValid, 0, maxG, PRESSURE_TOP, PRESSURE_BOTTOM, G_COLOR);
  series(risk::OilValid, minOil, maxOil, OIL_TOP, OIL_BOTTOM, OIL_COLOR);
  const int center = LEFT + WIDTH / 2;
  for (int y = PRESSURE_TOP; y <= OIL_BOTTOM; y += 4)
  {
    mainCanvas.drawPixel(center, y, COLOR_WHITE);
  }
  mainCanvas.fillCircle(center, yFor(event.pressure, 0, maxPressure, PRESSURE_TOP, PRESSURE_BOTTOM), 2, PRESSURE_COLOR);
  mainCanvas.fillCircle(center, yFor(event.g, 0, maxG, PRESSURE_TOP, PRESSURE_BOTTOM), 2, G_COLOR);
  if (event.oilValid) mainCanvas.fillCircle(center, yFor(event.oil, minOil, maxOil, OIL_TOP, OIL_BOTTOM), 2, OIL_COLOR);
  // 中央の記録点に対応する油温を、曲線に重ならない帯へ表示する。
  mainCanvas.setFont(&fonts::FreeSansBold9pt7b);
  mainCanvas.setTextColor(OIL_COLOR, COLOR_BLACK);
  const int oilLabelLeft = center + OIL_LABEL_PADDING;
  mainCanvas.setClipRect(oilLabelLeft - 1, PRESSURE_BOTTOM + 1, LEFT + WIDTH - oilLabelLeft + 1,
                         OIL_TOP - PRESSURE_BOTTOM - 2);
  mainCanvas.setCursor(oilLabelLeft, OIL_TOP - mainCanvas.fontHeight() - 1);
  mainCanvas.printf("%s ", risk::history.hasEvent() ? risk::name(event.kind) : "0s");
  if (event.oilValid)
  {
    mainCanvas.printf("%.1fC", event.oil);
  }
  else
  {
    mainCanvas.print("---");
  }
  mainCanvas.clearClipRect();
  mainCanvas.setFont(&fonts::Font0);
  mainCanvas.setTextColor(G_COLOR, COLOR_BLACK);
  const auto peaks = risk::gPeaks(risk::history);
  for (size_t index = 0; index < peaks.count; ++index)
  {
    const auto& peak = peaks.values[index];
    const int x = LEFT + static_cast<int>(peak.index * WIDTH / (risk::WINDOW_SAMPLES - 1));
    const int y = yFor(peak.g / 100.0F, 0, maxG, PRESSURE_TOP, PRESSURE_BOTTOM);
    const bool above = y >= PRESSURE_TOP + 10;
    const int labelY = above ? y - 10 : y + 4;
    const int labelX = std::clamp(above ? x - 2 : x + 4, LEFT, LEFT + WIDTH - 6);
    mainCanvas.fillCircle(x, y, 2, G_COLOR);
    mainCanvas.setCursor(labelX, labelY);
    mainCanvas.print(gDirectionLabel(peak.direction));
  }
  mainCanvas.setTextColor(COLOR_WHITE, COLOR_BLACK);
  mainCanvas.setCursor(LEFT - 8, 216);
  mainCanvas.print("-60s");
  mainCanvas.setCursor(center - 3, 216);
  mainCanvas.print("0");
  mainCanvas.setCursor(LEFT + WIDTH - 23, 216);
  mainCanvas.print("+60s");
  mainCanvas.setCursor(4, 231);
  mainCanvas.printf("RAM PRE %us POST %us/60 | Tap:Back", risk::history.preMs() / 1000, risk::history.postMs() / 1000);
}

void push(bool full)
{
  display.startWrite();
  if (!full)
  {
    display.setClipRect(0, 0, LCD_WIDTH, 24);
  }
  mainCanvas.pushSprite(0, 0);
  display.waitDMA();
  display.clearClipRect();
  display.endWrite();
}
}  // namespace

void refreshRiskGraph(bool force)
{
  static uint32_t previousDraw = 0;
  static uint32_t previousRevision = 0;
  static uint64_t previousAge = 0;
  const uint32_t now = millis();
  const uint64_t age = risk::history.ageMs() / 1000;
  if (force || (previousRevision != risk::history.revision() && now - previousDraw >= 500))
  {
    graph();
    push(true);
    previousDraw = now;
    previousRevision = risk::history.revision();
    previousAge = age;
  }
  else if (risk::history.hasSelection() && previousRevision == risk::history.revision() && age != previousAge)
  {
    header();
    push(false);
    previousAge = age;
  }
}
