#include "display.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "DrawFillArcMeter.h"
#include "backlight.h"
#include "fps_display.h"
#include "low_warning.h"
#include "racing_indicator.h"
#include "sensor.h"

// ────────────────────── グローバル変数 ──────────────────────
M5GFX display;
M5Canvas mainCanvas(&display);

static bool pressureGaugeInitialized = false;
static bool waterGaugeInitialized = false;

RecordedMaximum<float> recordedMaxOilPressure;
RecordedMaximum<float> recordedMaxWaterTemp;
RecordedMaximum<int> recordedMaxOilTempTop;
// 監視値は詳細画面の表示中も更新し、描画キャッシュとは分ける。
static float monitoredPressure = 0.0F;
static float monitoredWaterTemp = 0.0F;
static float monitoredOilTemp = 0.0F;
static MeterValueCache pressureTextCache;
static MeterValueCache waterTextCache;
static SensorStatus drawnPressureStatus = SensorStatus::Pending;
static SensorStatus drawnWaterStatus = SensorStatus::Pending;
static SensorStatus drawnOilStatus = SensorStatus::Pending;
static bool oilBarInitialized = false;

static auto sensorText(SensorStatus status) -> const char*
{
  if (status == SensorStatus::Valid) return nullptr;
  return status == SensorStatus::Pending || status == SensorStatus::Disabled ? "---" : "ERR";
}

static void transferRegion(int x, int y, int width, int height)
{
  // 転送先をクリップすると、元スプライトの行幅を保ったまま部分転送できる。
  display.setClipRect(x, y, width, height);
  mainCanvas.pushSprite(0, 0);
}

static void finishTransfer()
{
  // 転送元を次フレームで書き換える前に、転送先のDMA完了を待つ。
  display.waitDMA();
  display.clearClipRect();
  display.endWrite();
}

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
void drawOilTemperatureTopBar(M5Canvas& canvas, float oilTemp, const RecordedMaximum<int>& maxOilTemp)
{
  constexpr int MIN_TEMP = 80;
  constexpr int MAX_TEMP = 130;
  constexpr int ALERT_TEMP = 120;

  constexpr int X = 20;
  constexpr int Y = 15;
  constexpr int W = 210;
  constexpr int H = 20;
  constexpr float RANGE = MAX_TEMP - MIN_TEMP;

  static int previousWidth = -1;
  static uint16_t previousColor = 0;
  static int previousMax = -1;
  static char previousText[16] = {};
  const bool initialize = !oilBarInitialized;
  const bool valid = oilTemperatureStatus == SensorStatus::Valid;
  const float drawTemp = valid ? oilTemp : 0.0F;
  const int barWidth = static_cast<int>(
      W * (clampValue(drawTemp, static_cast<float>(MIN_TEMP), static_cast<float>(MAX_TEMP)) - MIN_TEMP) / RANGE);
  const uint16_t barColor = drawTemp >= ALERT_TEMP ? COLOR_RED : COLOR_WHITE;
  if (initialize || barColor != previousColor)
  {
    canvas.fillRect(X, Y, W, H, 0x18E3);
    if (barWidth > 0) canvas.fillRect(X, Y, barWidth, H, barColor);
  }
  else if (barWidth > previousWidth)
  {
    canvas.fillRect(X + previousWidth, Y, barWidth - previousWidth, H, barColor);
  }
  else if (barWidth < previousWidth)
  {
    canvas.fillRect(X + barWidth, Y, previousWidth - barWidth, H, 0x18E3);
  }
  previousWidth = barWidth;
  previousColor = barColor;

  canvas.setTextSize(1);
  canvas.setTextColor(COLOR_WHITE, COLOR_BLACK);
  canvas.setFont(&fonts::Font0);
  if (initialize)
  {
    const int marks[] = {80, 90, 100, 110, 120, 130};
    for (int m : marks)
    {
      int tx = X + static_cast<int>(W * (m - MIN_TEMP) / RANGE);
      canvas.drawPixel(tx, Y - 2, COLOR_WHITE);
      canvas.setCursor(tx - 10, Y - 14);
      canvas.printf("%d", m);
    }
  }
  {
    const int alertX = X + static_cast<int>(W * (ALERT_TEMP - MIN_TEMP) / RANGE);
    canvas.drawLine(alertX, Y, alertX, Y + H - 2, COLOR_GRAY);
  }
  const int maximumValue = maxOilTemp.hasValue() ? maxOilTemp.value() : INT16_MIN;
  if (initialize || maximumValue != previousMax)
  {
    canvas.fillRect(X, Y + H + 4, 215, 9, COLOR_BLACK);
    canvas.setCursor(X, Y + H + 4);
    if (maxOilTemp.hasValue())
      canvas.printf("OIL.T / Celsius,  MAX:%03d", maxOilTemp.value());
    else
      canvas.print("OIL.T / Celsius,  MAX:---");
    previousMax = maximumValue;
  }
  char tempStr[16];
  const char* status = sensorText(oilTemperatureStatus);
  if (status != nullptr)
    snprintf(tempStr, sizeof(tempStr), "%s", status);
  else
    snprintf(tempStr, sizeof(tempStr), "%d", static_cast<int>(oilTemp));
  if (initialize || strcmp(previousText, tempStr) != 0)
  {
    canvas.fillRect(238, 0, 82, 50, COLOR_BLACK);
    canvas.setFont(&FreeSansBold24pt7b);
    if (status != nullptr || canvas.textWidth(tempStr) > 80) canvas.setFont(&fonts::FreeSansBold12pt7b);
    canvas.drawRightString(tempStr, LCD_WIDTH - 1, 2);
    snprintf(previousText, sizeof(previousText), "%s", tempStr);
  }
  oilBarInitialized = true;
}

// ────────────────────── 画面更新＋ログ ──────────────────────
void renderDisplayAndLog(float pressureAvg, float waterTempAvg, float oilTemp, const RecordedMaximum<int>& maxOilTemp)
{
  const int TOPBAR_Y = 0;
  const int TOPBAR_H = 50;
  const int GAUGE_H = 170;

  const int16_t maximumValue = maxOilTemp.hasValue() ? maxOilTemp.value() : INT16_MIN;
  // 水温は0.05度以上、油温は0.1度以上、油圧は0.05bar以上変化したら更新する
  bool oilChanged = std::isnan(displayCache.oilTemp) || fabs(oilTemp - displayCache.oilTemp) >= 0.1F ||
                    (maximumValue != displayCache.maxOilTemp) || drawnOilStatus != oilTemperatureStatus;
  bool pressureChanged = std::isnan(displayCache.pressureAvg) || fabs(pressureAvg - displayCache.pressureAvg) >= 0.05F ||
                         drawnPressureStatus != oilPressureStatus;
  bool waterChanged = std::isnan(displayCache.waterTempAvg) || fabs(waterTempAvg - displayCache.waterTempAvg) >= 0.05F ||
                      drawnWaterStatus != waterTemperatureStatus;

  mainCanvas.setTextColor(COLOR_WHITE);

  if (oilChanged)
  {
    if (!oilBarInitialized) mainCanvas.fillRect(0, TOPBAR_Y, LCD_WIDTH, TOPBAR_H, COLOR_BLACK);
    drawOilTemperatureTopBar(mainCanvas, oilTemp, maxOilTemp);
    displayCache.oilTemp = oilTemp;
    displayCache.maxOilTemp = maximumValue;
    drawnOilStatus = oilTemperatureStatus;
  }

  if (pressureChanged || !pressureGaugeInitialized)
  {
    if (!pressureGaugeInitialized)
    {
      mainCanvas.fillRect(0, 60, 160, GAUGE_H, COLOR_BLACK);
    }
    bool isUseDecimal = pressureAvg < 9.95F;
    drawFillArcMeter(mainCanvas, pressureAvg, 0.0f, MAX_OIL_PRESSURE_METER, 8.0f, COLOR_RED, "x100kPa", "OIL.P",
                     prevPressureValue, 0.5f, isUseDecimal, 0, 60, !pressureGaugeInitialized, -1.0F, 0.0F,
                     &pressureTextCache, sensorText(oilPressureStatus));
    drawnPressureStatus = oilPressureStatus;
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
                     "WATER.T", prevWaterTempValue, 1.0f, false, 160, 60, !waterGaugeInitialized, 5.0f,
                     WATER_TEMP_METER_MIN, &waterTextCache, sensorText(waterTemperatureStatus));
    drawnWaterStatus = waterTemperatureStatus;
    waterGaugeInitialized = true;
    displayCache.waterTempAvg = waterTempAvg;
  }

  bool warnChanged = false;
  bool isWarnShowing = drawLowPressureWarning(mainCanvas, warnChanged, pressureChanged);
  if (warnChanged && !isWarnShowing)
  {
    // 警告が消えたら油圧ゲージを再描画して元に戻す
    bool isUseDecimal = pressureAvg < 9.95F;
    drawFillArcMeter(mainCanvas, pressureAvg, 0.0f, MAX_OIL_PRESSURE_METER, 8.0f, COLOR_RED, "x100kPa", "OIL.P",
                     prevPressureValue, 0.5f, isUseDecimal, 0, 60, false, -1.0F, 0.0F, &pressureTextCache,
                     sensorText(oilPressureStatus));
  }
  bool fpsChanged = false;
#if FPS_DISPLAY_ENABLED
  // FPS表示が有効な場合のみ描画する
  fpsChanged = drawFpsOverlay();
#endif
  bool racingChanged = drawRacingIndicator(mainCanvas);
  bool historyChanged = drawLowWarningHistory(mainCanvas);

  const bool pressureRegionChanged = pressureChanged || warnChanged;
  if (!(oilChanged || pressureRegionChanged || waterChanged || fpsChanged || racingChanged || historyChanged)) return;

  // 接する領域だけを結合し、離れた更新の間にある未変更部分は転送しない。
  display.startWrite();
  if (oilChanged && pressureRegionChanged && waterChanged)
  {
    transferRegion(0, 0, LCD_WIDTH, LCD_HEIGHT);
  }
  else
  {
    if (oilChanged) transferRegion(0, 0, LCD_WIDTH, 60);
    if (pressureRegionChanged && waterChanged)
      transferRegion(0, 60, LCD_WIDTH, 180);
    else
    {
      if (pressureRegionChanged) transferRegion(0, 60, 160, 180);
      if (waterChanged) transferRegion(160, 60, 160, 180);
    }
  }
  if (!pressureRegionChanged && (fpsChanged || racingChanged || historyChanged))
    transferRegion(0, LCD_HEIGHT - 16, 32, fpsChanged ? 16 : 8);
  finishTransfer();
}

// ────────────────────── 描画から独立した監視 ──────────────────────
void updateGaugeMonitoring()
{
  static float smoothWaterTemp = std::numeric_limits<float>::quiet_NaN();
  static float smoothOilTemp = std::numeric_limits<float>::quiet_NaN();
  static float smoothOilPressure = std::numeric_limits<float>::quiet_NaN();
  float pressure = std::min(calculateAverage(oilPressureSamples), MAX_OIL_PRESSURE_DISPLAY);
  float water = calculateAverage(waterTemperatureSamples);
  float oil = calculateAverage(oilTemperatureSamples);
  bool pressureValid = oilPressureStatus == SensorStatus::Valid && std::isfinite(pressure) && pressure < 11.0F;
  bool waterValid = waterTemperatureStatus == SensorStatus::Valid && std::isfinite(water) && water < 199.0F;
  bool oilValid = oilTemperatureStatus == SensorStatus::Valid && std::isfinite(oil) && oil < 199.0F;
  recordedMaxOilPressure.observe(pressure, pressureValid);
  recordedMaxWaterTemp.observe(water, waterValid);
  if (oilValid) recordedMaxOilTempTop.observe(static_cast<int>(oil), true);

  // 異常値を平滑化に混ぜず、復帰後の最初の値から再開する。最高値は保持する。
  auto smooth = [](float target, bool valid, float alpha, float& previous) -> float
  {
    if (!valid)
    {
      previous = std::numeric_limits<float>::quiet_NaN();
      return 0.0F;
    }
    if (std::isnan(previous)) previous = target;
    previous += alpha * (target - previous);
    return previous;
  };
  monitoredPressure = smooth(pressure, pressureValid, OIL_PRESSURE_SMOOTHING_ALPHA, smoothOilPressure);
  monitoredWaterTemp = smooth(water, waterValid, 0.1F, smoothWaterTemp);
  monitoredOilTemp = smooth(oil, oilValid, 0.1F, smoothOilTemp);
  updateLowPressureWarning(currentGForce, monitoredPressure, pressureValid);
}

void updateGauges() { renderDisplayAndLog(monitoredPressure, monitoredWaterTemp, monitoredOilTemp, recordedMaxOilTempTop); }

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
  constexpr int MENU_TOP_MARGIN = 20;     // 上端の余白
  constexpr int MENU_BOTTOM_MARGIN = 40;  // 下端の余白（戻る案内分）
  // 表示行数を減らして行間を確保
  // OIL.P WARN の詳細表示を2行で確保するため1行分多く確保
#if SENSOR_AMBIENT_LIGHT_PRESENT
  constexpr int MENU_LINES = 7;  // 表示行数
#else
  constexpr int MENU_LINES = 5;  // 表示行数
#endif
  const int lineHeight = (LCD_HEIGHT - MENU_TOP_MARGIN - MENU_BOTTOM_MARGIN) / MENU_LINES;  // 行間

  int y = MENU_TOP_MARGIN;
  char valStr[8];  // 数値表示用バッファ

  // 最高水温を表示
  mainCanvas.setCursor(10, y);
  mainCanvas.print("WATER.T MAX:");
#if SENSOR_WATER_TEMP_PRESENT
  // 小数点を表示しない
  if (recordedMaxWaterTemp.hasValue())
    snprintf(valStr, sizeof(valStr), "%6.0f", recordedMaxWaterTemp.value());
  else
    snprintf(valStr, sizeof(valStr), "---");
  mainCanvas.drawRightString(valStr, LCD_WIDTH - 10, y);
#else
  // センサー無効時は Disabled と表示
  mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - 10, y);
#endif

  y += lineHeight;
  // 最高油温を表示
  mainCanvas.setCursor(10, y);
  mainCanvas.print("OIL.T MAX:");
#if SENSOR_OIL_TEMP_PRESENT
  if (recordedMaxOilTempTop.hasValue())
    snprintf(valStr, sizeof(valStr), "%6d", recordedMaxOilTempTop.value());
  else
    snprintf(valStr, sizeof(valStr), "---");
  mainCanvas.drawRightString(valStr, LCD_WIDTH - 10, y);
#else
  // センサー無効時は Disabled と表示
  mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - 10, y);
#endif

  y += lineHeight;
  // 最高油圧を表示
  mainCanvas.setCursor(10, y);
  mainCanvas.print("OIL.P MAX:");
#if SENSOR_OIL_PRESSURE_PRESENT
  if (recordedMaxOilPressure.hasValue())
    snprintf(valStr, sizeof(valStr), "%6.1f", recordedMaxOilPressure.value());
  else
    snprintf(valStr, sizeof(valStr), "---");
  mainCanvas.drawRightString(valStr, LCD_WIDTH - 10, y);
#else
  // センサー無効時は Disabled と表示
  mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - 10, y);
#endif

  y += lineHeight;
  // 直近の低油圧イベント情報を2行で表示
  mainCanvas.setCursor(10, y);
  mainCanvas.print("OIL.P WARN:");
  y += lineHeight;
  if (lastLowEventDuration > 0.0F)
  {
    // 方向, G値, 継続秒数, 油圧をカンマ区切りで作成（カンマ後にスペースを入れる）
    char detailStr[40];
    snprintf(detailStr, sizeof(detailStr), "%s, %.1fG, %.1fs, %.1f", lastLowEventDir, lastLowEventG, lastLowEventDuration,
             lastLowEventPressure);

    const int right = LCD_WIDTH - 10;  // 右端位置
    // 詳細文字列の幅と高さを測定（通常フォント）
    mainCanvas.setFont(&fonts::FreeSansBold12pt7b);
    int textWidth = mainCanvas.textWidth(detailStr);
    int textHeight = mainCanvas.fontHeight();

    // 単位 "x100kPa" の幅と高さを小さいフォントで測定
    mainCanvas.setFont(&fonts::Font0);
    int unitWidth = mainCanvas.textWidth("x100kPa");
    int unitHeight = mainCanvas.fontHeight();

    int startX = right - unitWidth - textWidth;

    // 詳細文字列を描画
    mainCanvas.setFont(&fonts::FreeSansBold12pt7b);
    mainCanvas.setCursor(startX, y);
    mainCanvas.print(detailStr);

    // 単位部分を小さいフォントで描画（数値の下端に揃える）
    mainCanvas.setFont(&fonts::Font0);
    mainCanvas.setCursor(startX + textWidth + 5, y + textHeight - unitHeight - 4);
    mainCanvas.print("x100kPa");
    // フォントを元に戻す
    mainCanvas.setFont(&fonts::FreeSansBold12pt7b);
  }
  else
  {
    mainCanvas.drawRightString("None", LCD_WIDTH - 10, y);
  }

  y += lineHeight;
  mainCanvas.setCursor(10, y);
#if SENSOR_AMBIENT_LIGHT_PRESENT
  // 現在のLUX値を表示
  mainCanvas.print("LUX LATEST:");
  snprintf(valStr, sizeof(valStr), "%6d", latestLux);
  mainCanvas.drawRightString(valStr, LCD_WIDTH - 10, y);

  y += lineHeight;
  mainCanvas.setCursor(10, y);
  // 照度の中央値を表示
  mainCanvas.print("LUX MEDIAN:");
  char medStr[8];
  snprintf(medStr, sizeof(medStr), "%6d", medianLuxValue);
  mainCanvas.drawRightString(medStr, LCD_WIDTH - 10, y);
#else
  // LUX センサーが無い場合は両方 Disabled を表示
  mainCanvas.print("LUX LATEST:");
  mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - 10, y);

  y += 25;
  mainCanvas.setCursor(10, y);
  mainCanvas.print("LUX MEDIAN:");
  mainCanvas.drawRightString(DISABLED_STR, LCD_WIDTH - 10, y);
#endif

  // 戻る案内を左下へ配置
  mainCanvas.setCursor(10, LCD_HEIGHT - 20);
  mainCanvas.setFont(&fonts::Font0);
  mainCanvas.printf("Tap screen to return");

  mainCanvas.setFont(&fonts::Font0);
  mainCanvas.setTextColor(COLOR_WHITE, COLOR_BLACK);
  mainCanvas.setCursor(10, 199);
  mainCanvas.printf("OIL.P:%s  OIL.T:%s  WATER.T:%s", sensorText(oilPressureStatus) ? sensorText(oilPressureStatus) : "OK",
                    sensorText(oilTemperatureStatus) ? sensorText(oilTemperatureStatus) : "OK",
                    sensorText(waterTemperatureStatus) ? sensorText(waterTemperatureStatus) : "OK");
  mainCanvas.pushSprite(0, 0);
  display.waitDMA();
}

void refreshMenuScreen(bool force)
{
  static uint32_t previousDrawMs = 0;
  uint32_t now = millis();
  if (force || now - previousDrawMs >= 250)
  {
    drawMenuScreen();
    previousDrawMs = now;
  }
}

// ────────────────────── ゲージ状態リセット ──────────────────────
void resetGaugeState()
{
  invalidateLowWarningHistoryIndicator();
  invalidateLowPressureWarning();
  invalidateRacingIndicator();
  oilBarInitialized = false;
  pressureTextCache = {};
  waterTextCache = {};
  // メニュー画面の残像を防ぐため一度画面をクリアする
  mainCanvas.fillScreen(COLOR_BLACK);
  mainCanvas.pushSprite(0, 0);
  display.waitDMA();

  pressureGaugeInitialized = false;
  waterGaugeInitialized = false;
  prevPressureValue = std::numeric_limits<float>::quiet_NaN();
  prevWaterTempValue = std::numeric_limits<float>::quiet_NaN();
  displayCache = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
                  std::numeric_limits<float>::quiet_NaN(), INT16_MIN};
}
