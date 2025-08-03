#include "display.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "DrawFillArcMeter.h"
#include "backlight.h"
#include "fps_display.h"
#include "sensor.h"

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

// 直近の低油圧イベント情報
float lastLowEventG = 0.0F;         // 発生時のG値
char lastLowEventDir = 'R';         // Gの向き
float lastLowEventDuration = 0.0F;  // 継続時間[s]
float lastLowEventPressure = 0.0F;  // そのときの油圧[bar]

// 油圧警告表示。現在の表示状態とその変更の有無を返す
static bool drawLowPressureWarning(M5Canvas& canvas, float gForce, float pressure, bool& stateChanged)
{
  // ゲージ位置とサイズ
  constexpr int GAUGE_X = 0;    // 油圧ゲージの左上X
  constexpr int GAUGE_Y = 60;   // 油圧ゲージの左上Y
  constexpr int GAUGE_W = 160;  // ゲージ幅
  constexpr int GAUGE_H = 170;  // ゲージ高さ

  canvas.setFont(&fonts::FreeSansBold12pt7b);
  // 警告文字列を生成（例: G2.3L）
  char warnStr[16];
  snprintf(warnStr, sizeof(warnStr), "G%.1f%c", gForce, currentGDirection);
  int textW = canvas.textWidth(warnStr);
  int textH = canvas.fontHeight();
  constexpr int PADDING = 4;  // ボックス余白
  int boxW = textW + (PADDING * 2);
  int boxH = textH + (PADDING * 2);
  int boxX = GAUGE_X + ((GAUGE_W - boxW) / 2);
  int boxY = GAUGE_Y + ((GAUGE_H - boxH) / 2);
  static int lastBoxX = boxX;  // 最後に描画したボックス座標
  static int lastBoxY = boxY;
  static int lastBoxW = boxW;
  static int lastBoxH = boxH;

  // 横Gが1G以上で判定
  constexpr float G_FORCE_THRESHOLD = 1.0F;          // G判定値
  constexpr float PRESSURE_THRESHOLD = 3.0F;         // 油圧閾値
  constexpr unsigned long WARNING_DELAY_MS = 500UL;  // 継続時間
  bool conditionMet = (gForce >= G_FORCE_THRESHOLD && pressure <= PRESSURE_THRESHOLD);
  static unsigned long startMs = 0;  // 条件成立開始時刻
  static bool isShowing = false;
  static float peakG = 0.0F;                                     // 期間中の最大G
  static float minPressure = std::numeric_limits<float>::max();  // 期間中の最低油圧
  static char eventDir = 'R';                                    // 発生方向
  unsigned long now = millis();
  bool shouldShow = false;

  if (conditionMet)
  {
    if (startMs == 0)
    {
      startMs = now;
      peakG = gForce;
      minPressure = pressure;
      eventDir = currentGDirection;
    }
    else
    {
      peakG = std::max(peakG, gForce);
      minPressure = std::min(minPressure, pressure);
    }
    if (now - startMs >= WARNING_DELAY_MS)
    {
      // 0.5秒以上継続したら警告表示
      // サイズ変化に対応するため以前のボックスを消去
      if (isShowing)
      {
        canvas.fillRect(lastBoxX, lastBoxY, lastBoxW, lastBoxH, COLOR_BLACK);
      }
      // 赤背景に方向付きG値を表示
      canvas.fillRect(boxX, boxY, boxW, boxH, COLOR_RED);
      canvas.setTextColor(COLOR_WHITE, COLOR_RED);
      canvas.setCursor(boxX + ((boxW - textW) / 2), boxY + ((boxH - textH) / 2));
      canvas.print(warnStr);
      // 消去用にボックス位置とサイズを保持
      lastBoxX = boxX;
      lastBoxY = boxY;
      lastBoxW = boxW;
      lastBoxH = boxH;
      shouldShow = true;
    }
  }
  else
  {
    // 条件外になったらタイマーをリセットし、表示していれば消去
    if (startMs != 0)
    {
      // 条件解除時にイベント情報を記録
      lastLowEventG = peakG;
      lastLowEventDir = eventDir;
      lastLowEventDuration = (now - startMs) / 1000.0F;
      lastLowEventPressure = minPressure;
    }
    startMs = 0;
    if (isShowing)
    {
      // 直前に描画した領域を完全に消去
      canvas.fillRect(lastBoxX, lastBoxY, lastBoxW, lastBoxH, COLOR_BLACK);
    }
  }

  stateChanged = (shouldShow != isShowing);
  isShowing = shouldShow;
  return shouldShow;
}

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

  bool warnChanged = false;
  bool isWarnShowing = drawLowPressureWarning(mainCanvas, currentGForce, pressureAvg, warnChanged);
  if (warnChanged && !isWarnShowing)
  {
    // 警告が消えたら油圧ゲージを再描画して元に戻す
    bool isUseDecimal = pressureAvg < 9.95F;
    drawFillArcMeter(mainCanvas, pressureAvg, 0.0f, MAX_OIL_PRESSURE_METER, 8.0f, COLOR_RED, "x100kPa", "OIL.P",
                     recordedMaxOilPressure, prevPressureValue, 0.5f, isUseDecimal, 0, 60, false);
  }
  bool fpsChanged = drawFpsOverlay();

  // 値が更新されたときのみスプライトを転送する
  if (oilChanged || pressureChanged || waterChanged || fpsChanged || warnChanged)
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
  constexpr int MENU_LINES = SENSOR_AMBIENT_LIGHT_PRESENT ? 8 : 7;                          // 表示行数
  const int lineHeight = (LCD_HEIGHT - MENU_TOP_MARGIN - MENU_BOTTOM_MARGIN) / MENU_LINES;  // 行間

  int y = MENU_TOP_MARGIN;

  // 直近の低油圧イベント情報を表示
  mainCanvas.setCursor(10, y);
  mainCanvas.print("OIL.P LOW:");
  if (lastLowEventDuration > 0.0F)
  {
    char valStr[32];
    snprintf(valStr, sizeof(valStr), "%4.1f%c,%4.1fs,%5.1f", lastLowEventG, lastLowEventDir, lastLowEventDuration,
             lastLowEventPressure);
    mainCanvas.drawRightString(valStr, LCD_WIDTH - 10, y);
  }
  else
  {
    mainCanvas.drawRightString("None", LCD_WIDTH - 10, y);
  }

  y += lineHeight;
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
