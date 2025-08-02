#include "display.h"

#include <algorithm>
#include <cmath>
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

// ── 油圧ログ用バッファ ──
constexpr int PRESSURE_LOG_SECONDS = 30 * 60;  // 30分のログ
static float oilPressureLog[PRESSURE_LOG_SECONDS] = {};
static int oilPressureLogIndex = 0;
static int oilPressureLogCount = 0;
static unsigned long lastPressureLogTime = 0;

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

// ────────────────────── 油温バー描画 ──────────────────────
void drawOilTemperatureTopBar(M5Canvas& canvas, float oilTemp, int maxOilTemp)
{
  constexpr int MIN_TEMP = 80;
  constexpr int MAX_TEMP = 130;
  constexpr int ALERT_TEMP = 120;

  constexpr int X = 20;
  constexpr int Y = 15;
  constexpr int W = 210;
  constexpr int H = 20;
  constexpr float RANGE = MAX_TEMP - MIN_TEMP;

  canvas.fillRect(X + 1, Y + 1, W - 2, H - 2, 0x18E3);

  float drawTemp = oilTemp;
  if (drawTemp >= 199.0F)
  {
    // 異常値の場合はバーを 0 として扱う
    drawTemp = 0.0F;
  }

  if (drawTemp >= MIN_TEMP)
  {
    int barWidth = static_cast<int>(W * (drawTemp - MIN_TEMP) / RANGE);
    uint16_t barColor = (drawTemp >= ALERT_TEMP) ? COLOR_RED : COLOR_WHITE;
    canvas.fillRect(X, Y, barWidth, H, barColor);
  }

  const int marks[] = {80, 90, 100, 110, 120, 130};
  canvas.setTextSize(1);
  canvas.setTextColor(COLOR_WHITE);
  canvas.setFont(&fonts::Font0);

  for (int m : marks)
  {
    int tx = X + static_cast<int>(W * (m - MIN_TEMP) / RANGE);
    canvas.drawPixel(tx, Y - 2, COLOR_WHITE);
    canvas.setCursor(tx - 10, Y - 14);
    canvas.printf("%d", m);
    if (m == ALERT_TEMP) canvas.drawLine(tx, Y, tx, Y + H - 2, COLOR_GRAY);
  }

  canvas.setCursor(X, Y + H + 4);
  canvas.printf("OIL.T / Celsius,  MAX:%03d", maxOilTemp);
  // snprintf でバッファサイズを指定し、
  // 安全に文字列化する
  int displayOilTemp = oilTemp >= 199.0F ? 0 : static_cast<int>(oilTemp);
  char tempStr[8];
  snprintf(tempStr, sizeof(tempStr), "%d", displayOilTemp);
  canvas.setFont(&FreeSansBold24pt7b);
  canvas.drawRightString(tempStr, LCD_WIDTH - 1, 2);
}

// ────────────────────── 画面更新＋ログ ──────────────────────
void renderDisplayAndLog(float pressureAvg, float waterTempAvg, float oilTemp, int16_t maxOilTemp)
{
  const int TOPBAR_Y = 0;
  const int TOPBAR_H = 50;
  const int GAUGE_H = 170;

  // 水温は0.05度以上、油温は0.1度以上、油圧は0.05bar以上変化したら更新する
  bool oilChanged = std::isnan(displayCache.oilTemp) || fabs(oilTemp - displayCache.oilTemp) >= 0.1F ||
                    (maxOilTemp != displayCache.maxOilTemp);
  bool pressureChanged = std::isnan(displayCache.pressureAvg) || fabs(pressureAvg - displayCache.pressureAvg) >= 0.05F;
  bool waterChanged = std::isnan(displayCache.waterTempAvg) || fabs(waterTempAvg - displayCache.waterTempAvg) >= 0.05F;

  mainCanvas.setTextColor(COLOR_WHITE);

  if (oilChanged)
  {
    mainCanvas.fillRect(0, TOPBAR_Y, LCD_WIDTH, TOPBAR_H, COLOR_BLACK);
    if (oilTemp >= 199.0F)
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
      mainCanvas.fillRect(0, 60, 160, GAUGE_H, COLOR_BLACK);
    }
    bool isUseDecimal = pressureAvg < 9.95F;
    drawFillArcMeter(mainCanvas, pressureAvg, 0.0f, MAX_OIL_PRESSURE_METER, 8.0f, COLOR_RED, "x100kPa", "OIL.P",
                     recordedMaxOilPressure, prevPressureValue, 0.5f, isUseDecimal, 0, 60, !pressureGaugeInitialized);
    pressureGaugeInitialized = true;
    displayCache.pressureAvg = pressureAvg;
  }

  if (waterChanged || !waterGaugeInitialized)
  {
    if (!waterGaugeInitialized)
    {
      mainCanvas.fillRect(160, 60, 160, GAUGE_H, COLOR_BLACK);
    }
    drawFillArcMeter(mainCanvas, waterTempAvg, WATER_TEMP_METER_MIN, WATER_TEMP_METER_MAX, 110.0f, COLOR_RED, "Celsius",
                     "WATER.T", recordedMaxWaterTemp, prevWaterTempValue, 1.0f, false, 160, 60, !waterGaugeInitialized,
                     5.0f, WATER_TEMP_METER_MIN);
    waterGaugeInitialized = true;
    displayCache.waterTempAvg = waterTempAvg;
  }

  bool fpsChanged = drawFpsOverlay();

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
  if (pressureAvg >= 11.0F || oilPressureOverVoltage)
  {
    // ショートエラー時は 0 として扱い、最大値もリセット
    pressureAvg = 0.0F;
    recordedMaxOilPressure = 0.0F;
  }
  float targetWaterTemp = calculateAverage(waterTemperatureSamples);
  if (targetWaterTemp >= 199.0F)
  {
    // 199℃以上ならセンサー異常として扱い0を返す
    targetWaterTemp = 0.0F;
    recordedMaxWaterTemp = 0.0F;
  }

  float targetOilTemp = calculateAverage(oilTemperatureSamples);
  if (targetOilTemp >= 199.0F)
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

  smoothWaterTemp += 0.1F * (targetWaterTemp - smoothWaterTemp);
  smoothOilTemp += 0.1F * (targetOilTemp - smoothOilTemp);
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
  renderDisplayAndLog(pressureValue, smoothWaterTemp, oilTempValue, recordedMaxOilTempTop);
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
  constexpr char DISABLED_STR[] = "Disabled";
  mainCanvas.drawRect(0, 0, LCD_WIDTH, LCD_HEIGHT, BORDER_COLOR);

  // 画面高さに合わせて行間を自動計算し、下にはみ出さないようにする
  constexpr int MENU_TOP_MARGIN = 20;                                                       // 上端の余白
  constexpr int MENU_BOTTOM_MARGIN = 40;                                                    // 下端の余白（戻る案内分）
  constexpr int MENU_LINES = SENSOR_AMBIENT_LIGHT_PRESENT ? 7 : 6;                          // 表示行数
  const int lineHeight = (LCD_HEIGHT - MENU_TOP_MARGIN - MENU_BOTTOM_MARGIN) / MENU_LINES;  // 行間

  int y = MENU_TOP_MARGIN;
  mainCanvas.setCursor(10, y);
  // ラベルは左寄せ、値は右寄せで表示
  mainCanvas.print("OIL.P MAX:");
  if (SENSOR_OIL_PRESSURE_PRESENT)
  {
    char valStr[8];
    snprintf(valStr, sizeof(valStr), "%6.1f", recordedMaxOilPressure);
    mainCanvas.drawRightString(valStr, LCD_WIDTH - 10, y);
  }
  else
  {
    // センサー無効時は Disabled と表示
    mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - 10, y);
  }

  y += lineHeight;
  mainCanvas.setCursor(10, y);
  mainCanvas.print("WATER.T MAX:");
  if (SENSOR_WATER_TEMP_PRESENT)
  {
    char valStr[8];
    snprintf(valStr, sizeof(valStr), "%6.1f", recordedMaxWaterTemp);
    mainCanvas.drawRightString(valStr, LCD_WIDTH - 10, y);
  }
  else
  {
    // センサー無効時は Disabled と表示
    mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - 10, y);
  }

  y += lineHeight;
  mainCanvas.setCursor(10, y);
  mainCanvas.print("OIL.T MAX:");
  if (SENSOR_OIL_TEMP_PRESENT)
  {
    char valStr[8];
    snprintf(valStr, sizeof(valStr), "%6d", recordedMaxOilTempTop);
    mainCanvas.drawRightString(valStr, LCD_WIDTH - 10, y);
  }
  else
  {
    // センサー無効時は Disabled と表示
    mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - 10, y);
  }

  y += lineHeight;
  mainCanvas.setCursor(10, y);
  mainCanvas.print("OIL.T NOW:");
  if (SENSOR_OIL_TEMP_PRESENT)
  {
    char valStr[8];
    snprintf(valStr, sizeof(valStr), "%6d", static_cast<int>(displayCache.oilTemp));
    mainCanvas.drawRightString(valStr, LCD_WIDTH - 10, y);
  }
  else
  {
    // センサー無効時は Disabled と表示
    mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - 10, y);
  }

  y += lineHeight;
  mainCanvas.setCursor(10, y);
  if (SENSOR_AMBIENT_LIGHT_PRESENT)
  {
    // 現在のLUX値を表示
    mainCanvas.print("LUX NOW:");
    char valStr[8];
    snprintf(valStr, sizeof(valStr), "%6d", latestLux);
    mainCanvas.drawRightString(valStr, LCD_WIDTH - 10, y);

    y += lineHeight;
    mainCanvas.setCursor(10, y);
    // 照度の中央値を表示
    mainCanvas.print("LUX MEDIAN:");
    char medStr[8];
    snprintf(medStr, sizeof(medStr), "%6d", medianLuxValue);
    mainCanvas.drawRightString(medStr, LCD_WIDTH - 10, y);
  }
  else
  {
    // LUX センサーが無い場合は両方 Disabled を表示
    mainCanvas.print("LUX NOW:");
    mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - 10, y);

    y += 25;
    mainCanvas.setCursor(10, y);
    mainCanvas.print("LUX MEDIAN:");
    mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - 10, y);
  }

  // 戻る案内を左下へ配置
  mainCanvas.setCursor(10, LCD_HEIGHT - 20);
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
void drawPressureGraph()
{
  mainCanvas.fillScreen(COLOR_BLACK);

  const int width = LCD_WIDTH;
  const int height = LCD_HEIGHT;

  // 軸を描画
  mainCanvas.drawLine(0, height - 1, width, height - 1, COLOR_WHITE);
  mainCanvas.drawLine(0, 0, 0, height, COLOR_WHITE);
  mainCanvas.setTextColor(COLOR_WHITE);
  mainCanvas.setCursor(5, 5);
  mainCanvas.print("油圧 / bar");
  mainCanvas.setCursor(width - 40, height - 15);
  mainCanvas.print("時間");

  // 表示するサンプル数は画面幅まで
  int samplesToDraw = std::min(oilPressureLogCount, width);
  int start = (oilPressureLogIndex - samplesToDraw + PRESSURE_LOG_SECONDS) % PRESSURE_LOG_SECONDS;
  int prevX = 0;
  int prevY = height - 1;
  for (int i = 0; i < samplesToDraw; ++i)
  {
    float p = oilPressureLog[(start + i) % PRESSURE_LOG_SECONDS];
    int x = i;
    int y = height - 1 - static_cast<int>((p / MAX_OIL_PRESSURE_DISPLAY) * (height - 1));
    mainCanvas.drawLine(prevX, prevY, x, y, COLOR_YELLOW);
    prevX = x;
    prevY = y;
  }

  mainCanvas.pushSprite(0, 0);
}
