#include "display.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>

#include "DrawFillArcMeter.h"
#include "backlight.h"
#include "fps_display.h"

// ────────────────────── グローバル変数 ──────────────────────
M5GFX display;
M5Canvas mainCanvas(&display);

static bool pressureGaugeInitialized = false;
static bool waterGaugeInitialized = false;

float recordedMaxOilPressure = 0.0F;
float recordedMaxWaterTemp = 0.0F;
int recordedMaxOilTempTop = 0;
// 前回の油圧測定時刻
static unsigned long lastPressureCheckMs = 0;

// 前回描画したゲージ値
static float prevPressureValue = std::numeric_limits<float>::quiet_NaN();
static float prevWaterTempValue = std::numeric_limits<float>::quiet_NaN();

struct DisplayCache
{
  float pressureAvg;
  float waterTempAvg;
  float oilTemp;
  int16_t maxOilTemp;
} displayCache = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
                  std::numeric_limits<float>::quiet_NaN(), INT16_MIN};

// センサー異常と判定する温度
constexpr float SENSOR_ERROR_TEMP = 199.0F;
// 更新のしきい値
constexpr float OIL_TEMP_CHANGE_THRESHOLD = 0.1F;
constexpr float PRESSURE_CHANGE_THRESHOLD = 0.05F;
constexpr float WATER_TEMP_CHANGE_THRESHOLD = 0.05F;
// 数値表示用バッファ長
constexpr std::size_t VALUE_STR_LEN = 8;

// ────────────────────── 油温バー描画 ──────────────────────
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void drawOilTemperatureTopBar(M5Canvas& canvas, float oilTemp, int maxOilTemp)
{
  constexpr int MIN_TEMP = 80;
  constexpr int MAX_TEMP = 130;
  constexpr int ALERT_TEMP = 120;

  // バー描画位置とサイズ
  constexpr int BASE_X = 20;
  constexpr int BASE_Y = 15;
  constexpr int BAR_WIDTH = 210;
  constexpr int BAR_HEIGHT = 20;
  constexpr float RANGE = MAX_TEMP - MIN_TEMP;

  constexpr uint16_t BAR_BG_COLOR = 0x18E3;  // バー背景色
  canvas.fillRect(BASE_X + 1, BASE_Y + 1, BAR_WIDTH - 2, BAR_HEIGHT - 2, BAR_BG_COLOR);

  float drawTemp = oilTemp;
  if (drawTemp >= SENSOR_ERROR_TEMP)
  {
    // 異常値の場合はバーを 0 として扱う
    drawTemp = 0.0F;
  }

  if (drawTemp >= MIN_TEMP)
  {
    int barWidth = static_cast<int>(BAR_WIDTH * (drawTemp - MIN_TEMP) / RANGE);
    uint16_t barColor = (drawTemp >= ALERT_TEMP) ? COLOR_RED : COLOR_WHITE;
    canvas.fillRect(BASE_X, BASE_Y, barWidth, BAR_HEIGHT, barColor);
  }

  // 目盛りの値を std::array で管理
  constexpr std::array<int, 6> MARKS = {80, 90, 100, 110, 120, 130};
  canvas.setTextSize(1);
  canvas.setTextColor(COLOR_WHITE);
  canvas.setFont(&fonts::Font0);

  for (int mark : MARKS)
  {
    int textX = BASE_X + static_cast<int>(BAR_WIDTH * (mark - MIN_TEMP) / RANGE);
    canvas.drawPixel(textX, BASE_Y - 2, COLOR_WHITE);
    canvas.setCursor(textX - 10, BASE_Y - 14);
    canvas.printf("%d", mark);
    if (mark == ALERT_TEMP)
    {
      // 警告温度の位置に灰色のラインを描画
      canvas.drawLine(textX, BASE_Y, textX, BASE_Y + BAR_HEIGHT - 2, COLOR_GRAY);
    }
  }

  canvas.setCursor(BASE_X, BASE_Y + BAR_HEIGHT + 4);
  canvas.printf("OIL.T / Celsius,  MAX:%03d", maxOilTemp);
  // snprintf でバッファサイズを指定し、
  // 安全に文字列化する
  int displayOilTemp = oilTemp >= SENSOR_ERROR_TEMP ? 0 : static_cast<int>(oilTemp);
  std::array<char, VALUE_STR_LEN> tempStr{};
  snprintf(tempStr.data(), tempStr.size(), "%d", displayOilTemp);
  canvas.setFont(&FreeSansBold24pt7b);
  canvas.drawRightString(tempStr.data(), LCD_WIDTH - 1, 2);
}

// ────────────────────── 画面更新＋ログ ──────────────────────
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void renderDisplayAndLog(float pressureAvg, float waterTempAvg, float oilTemp, int16_t maxOilTemp)
{
  const int TOPBAR_Y = 0;
  const int TOPBAR_H = 50;
  const int GAUGE_H = 170;
  constexpr int GAUGE_Y = 60;       // ゲージのY座標
  constexpr int GAUGE_WIDTH = 160;  // ゲージの幅
  constexpr float PRESSURE_DECIMAL_LIMIT = 9.95F;
  constexpr float PRESSURE_METER_STEP = 8.0F;
  constexpr float PRESSURE_VALUE_SMOOTH = 0.5F;
  constexpr int PRESSURE_GAUGE_X = 0;
  constexpr int WATER_GAUGE_X = 160;
  constexpr float WATER_METER_MAX = 110.0F;
  constexpr float WATER_METER_STEP = 5.0F;

  // 水温は0.05度以上、油温は0.1度以上、油圧は0.05bar以上変化したら更新する
  bool oilChanged = std::isnan(displayCache.oilTemp) ||
                    std::fabs(oilTemp - displayCache.oilTemp) >= OIL_TEMP_CHANGE_THRESHOLD ||
                    (maxOilTemp != displayCache.maxOilTemp);
  bool pressureChanged = std::isnan(displayCache.pressureAvg) ||
                         std::fabs(pressureAvg - displayCache.pressureAvg) >= PRESSURE_CHANGE_THRESHOLD;
  bool waterChanged = std::isnan(displayCache.waterTempAvg) ||
                      std::fabs(waterTempAvg - displayCache.waterTempAvg) >= WATER_TEMP_CHANGE_THRESHOLD;

  mainCanvas.setTextColor(COLOR_WHITE);

  if (oilChanged)
  {
    mainCanvas.fillRect(0, TOPBAR_Y, LCD_WIDTH, TOPBAR_H, COLOR_BLACK);
    if (oilTemp >= SENSOR_ERROR_TEMP)
    {
      // センサー異常時は最大値も 0 扱いにする
      maxOilTemp = 0;
    }
    else
    {
      maxOilTemp = std::max<float>(oilTemp, maxOilTemp);
    }
    drawOilTemperatureTopBar(mainCanvas, oilTemp, maxOilTemp);
    displayCache.oilTemp = oilTemp;
    displayCache.maxOilTemp = maxOilTemp;
  }

  if (pressureChanged || !pressureGaugeInitialized)
  {
    if (!pressureGaugeInitialized)
    {
      mainCanvas.fillRect(PRESSURE_GAUGE_X, GAUGE_Y, GAUGE_WIDTH, GAUGE_H, COLOR_BLACK);
    }
    bool isUseDecimal = pressureAvg < PRESSURE_DECIMAL_LIMIT;
    drawFillArcMeter(mainCanvas, pressureAvg, 0.0F, MAX_OIL_PRESSURE_METER, PRESSURE_METER_STEP, COLOR_RED, "x100kPa",
                     "OIL.P", recordedMaxOilPressure, prevPressureValue, PRESSURE_VALUE_SMOOTH, isUseDecimal,
                     PRESSURE_GAUGE_X, GAUGE_Y, !pressureGaugeInitialized);
    pressureGaugeInitialized = true;
    displayCache.pressureAvg = pressureAvg;
  }

  if (waterChanged || !waterGaugeInitialized)
  {
    if (!waterGaugeInitialized)
    {
      mainCanvas.fillRect(WATER_GAUGE_X, GAUGE_Y, GAUGE_WIDTH, GAUGE_H, COLOR_BLACK);
    }
    drawFillArcMeter(mainCanvas, waterTempAvg, WATER_TEMP_METER_MIN, WATER_TEMP_METER_MAX, WATER_METER_MAX, COLOR_RED,
                     "Celsius", "WATER.T", recordedMaxWaterTemp, prevWaterTempValue, 1.0F, false, WATER_GAUGE_X, GAUGE_Y,
                     !waterGaugeInitialized, WATER_METER_STEP, WATER_TEMP_METER_MIN);
    waterGaugeInitialized = true;
    displayCache.waterTempAvg = waterTempAvg;
  }

  bool fpsChanged = false;
  if (FPS_DISPLAY_ENABLED)
  {
    // FPS表示が有効な場合のみ描画する
    fpsChanged = drawFpsOverlay();
  }

  // 値が更新されたときのみスプライトを転送する
  if (oilChanged || pressureChanged || waterChanged || fpsChanged)
  {
    mainCanvas.pushSprite(0, 0);
  }
}

// ────────────────────── メーター描画更新 ──────────────────────
void updateGauges()
{
  static float smoothWaterTemp = std::numeric_limits<float>::quiet_NaN();
  static float smoothOilTemp = std::numeric_limits<float>::quiet_NaN();
  static float smoothOilPressure = std::numeric_limits<float>::quiet_NaN();
  unsigned long nowMs = millis();
  if (lastPressureCheckMs == 0)
  {
    lastPressureCheckMs = nowMs;
  }
  unsigned long deltaMs = nowMs - lastPressureCheckMs;
  lastPressureCheckMs = nowMs;

  float pressureAvg = calculateAverage(oilPressureSamples);
  pressureAvg = std::min(pressureAvg, MAX_OIL_PRESSURE_DISPLAY);
  constexpr float PRESSURE_OVER_LIMIT = 11.0F;
  if (pressureAvg >= PRESSURE_OVER_LIMIT || oilPressureOverVoltage)
  {
    // ショートエラー時は 0 として扱い、最大値もリセット
    pressureAvg = 0.0F;
    recordedMaxOilPressure = 0.0F;
  }
  float targetWaterTemp = calculateAverage(waterTemperatureSamples);
  if (targetWaterTemp >= SENSOR_ERROR_TEMP)
  {
    // 199℃以上ならセンサー異常として扱い0を返す
    targetWaterTemp = 0.0F;
    recordedMaxWaterTemp = 0.0F;
  }

  float targetOilTemp = calculateAverage(oilTemperatureSamples);
  if (targetOilTemp >= SENSOR_ERROR_TEMP)
  {
    // 199℃以上はセンサー異常として 0 扱いにする
    targetOilTemp = 0.0F;
    recordedMaxOilTempTop = 0;
  }

  if (std::isnan(smoothWaterTemp))
  {
    smoothWaterTemp = targetWaterTemp;
  }
  if (std::isnan(smoothOilTemp))
  {
    smoothOilTemp = targetOilTemp;
  }
  if (std::isnan(smoothOilPressure))
  {
    smoothOilPressure = pressureAvg;
  }

  constexpr float SMOOTHING_ALPHA = 0.1F;
  smoothWaterTemp += SMOOTHING_ALPHA * (targetWaterTemp - smoothWaterTemp);
  smoothOilTemp += SMOOTHING_ALPHA * (targetOilTemp - smoothOilTemp);
  smoothOilPressure += OIL_PRESSURE_SMOOTHING_ALPHA * (pressureAvg - smoothOilPressure);

  float oilTempValue = smoothOilTemp;
  float pressureValue = smoothOilPressure;
  if (!SENSOR_OIL_TEMP_PRESENT)
  {
    // センサーが無い場合は常に 0 表示
    oilTempValue = 0.0F;
  }

  recordedMaxOilPressure = std::max(recordedMaxOilPressure, pressureAvg);
  recordedMaxWaterTemp = std::max(recordedMaxWaterTemp, smoothWaterTemp);
  recordedMaxOilTempTop = std::max(recordedMaxOilTempTop, static_cast<int>(targetOilTemp));
  renderDisplayAndLog(pressureValue, smoothWaterTemp, oilTempValue, static_cast<int16_t>(recordedMaxOilTempTop));
}

// ────────────────────── メニュー画面描画 ──────────────────────
void drawMenuScreen()
{
  mainCanvas.fillScreen(COLOR_BLACK);
  mainCanvas.setFont(&fonts::FreeSansBold12pt7b);
  mainCanvas.setTextSize(1);
  mainCanvas.setTextColor(COLOR_WHITE);

  // フラットデザインの枠を描く
  constexpr uint16_t BORDER_COLOR = rgb565(80, 80, 80);
  // センサー無効時に表示する文字列
  constexpr const char* DISABLED_STR = "Disabled";
  mainCanvas.drawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, BORDER_COLOR);

  // 画面高さに合わせて行間を自動計算し、下にはみ出さないようにする
  constexpr int MENU_TOP_MARGIN = 20;                                                       // 上端の余白
  constexpr int MENU_BOTTOM_MARGIN = 40;                                                    // 下端の余白（戻る案内分）
  constexpr int MENU_LINES = SENSOR_AMBIENT_LIGHT_PRESENT ? 7 : 6;                          // 表示行数
  constexpr int MENU_X = 10;                                                                // 左端のX座標
  constexpr int RIGHT_MARGIN = 10;                                                          // 右端の余白
  constexpr int LUX_EXTRA_Y = 25;                                                           // LUX表示の追加オフセット
  constexpr int CURSOR_BOTTOM_OFFSET = 20;                                                  // 戻る案内のYオフセット
  const int lineHeight = (LCD_HEIGHT - MENU_TOP_MARGIN - MENU_BOTTOM_MARGIN) / MENU_LINES;  // 行間

  int cursorY = MENU_TOP_MARGIN;
  mainCanvas.setCursor(MENU_X, cursorY);
  // ラベルは左寄せ、値は右寄せで表示
  mainCanvas.print("OIL.P MAX:");
  if (SENSOR_OIL_PRESSURE_PRESENT)
  {
    std::array<char, VALUE_STR_LEN> valStr{};
    snprintf(valStr.data(), valStr.size(), "%6.1f", recordedMaxOilPressure);
    mainCanvas.drawRightString(valStr.data(), LCD_WIDTH - RIGHT_MARGIN, cursorY);
  }
  else
  {
    // センサー無効時は Disabled と表示
    mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - RIGHT_MARGIN, cursorY);
  }

  cursorY += lineHeight;
  mainCanvas.setCursor(MENU_X, cursorY);
  mainCanvas.print("WATER.T MAX:");
  if (SENSOR_WATER_TEMP_PRESENT)
  {
    std::array<char, VALUE_STR_LEN> valStr{};
    snprintf(valStr.data(), valStr.size(), "%6.1f", recordedMaxWaterTemp);
    mainCanvas.drawRightString(valStr.data(), LCD_WIDTH - RIGHT_MARGIN, cursorY);
  }
  else
  {
    // センサー無効時は Disabled と表示
    mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - RIGHT_MARGIN, cursorY);
  }

  cursorY += lineHeight;
  mainCanvas.setCursor(MENU_X, cursorY);
  mainCanvas.print("OIL.T MAX:");
  if (SENSOR_OIL_TEMP_PRESENT)
  {
    std::array<char, VALUE_STR_LEN> valStr{};
    snprintf(valStr.data(), valStr.size(), "%6d", recordedMaxOilTempTop);
    mainCanvas.drawRightString(valStr.data(), LCD_WIDTH - RIGHT_MARGIN, cursorY);
  }
  else
  {
    // センサー無効時は Disabled と表示
    mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - RIGHT_MARGIN, cursorY);
  }

  cursorY += lineHeight;
  mainCanvas.setCursor(MENU_X, cursorY);
  if (SENSOR_AMBIENT_LIGHT_PRESENT)
  {
    // 現在のLUX値を表示
    mainCanvas.print("LUX NOW:");
    std::array<char, VALUE_STR_LEN> valStr{};
    snprintf(valStr.data(), valStr.size(), "%6d", latestLux);
    mainCanvas.drawRightString(valStr.data(), LCD_WIDTH - RIGHT_MARGIN, cursorY);

    cursorY += lineHeight;
    mainCanvas.setCursor(MENU_X, cursorY);
    // 照度の中央値を表示
    mainCanvas.print("LUX MEDIAN:");
    std::array<char, VALUE_STR_LEN> medStr{};
    snprintf(medStr.data(), medStr.size(), "%6d", medianLuxValue);
    mainCanvas.drawRightString(medStr.data(), LCD_WIDTH - RIGHT_MARGIN, cursorY);
  }
  else
  {
    // LUX センサーが無い場合は両方 Disabled を表示
    mainCanvas.print("LUX NOW:");
    mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - RIGHT_MARGIN, cursorY);

    cursorY += LUX_EXTRA_Y;
    mainCanvas.setCursor(MENU_X, cursorY);
    mainCanvas.print("LUX MEDIAN:");
    mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - RIGHT_MARGIN, cursorY);
  }

  // 戻る案内を左下へ配置
  mainCanvas.setCursor(MENU_X, LCD_HEIGHT - CURSOR_BOTTOM_OFFSET);
  mainCanvas.setFont(&fonts::Font0);
  mainCanvas.printf("Tap screen to return");

  mainCanvas.pushSprite(0, 0);
}

// ────────────────────── ゲージ状態リセット ──────────────────────
void resetGaugeState()
{
  // メニュー画面の残像を防ぐため一度画面をクリアする
  mainCanvas.fillScreen(COLOR_BLACK);
  mainCanvas.pushSprite(0, 0);

  pressureGaugeInitialized = false;
  waterGaugeInitialized = false;
  prevPressureValue = std::numeric_limits<float>::quiet_NaN();
  prevWaterTempValue = std::numeric_limits<float>::quiet_NaN();
  displayCache = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
                  std::numeric_limits<float>::quiet_NaN(), INT16_MIN};
}
