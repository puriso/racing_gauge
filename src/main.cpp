#include <M5CoreS3.h>
#include <Preferences.h>  // NVS へ状態保存用
#include <WiFi.h>         // WiFi 無効化用
#include <Wire.h>

#include "config.h"
#include "modules/backlight.h"
#include "modules/display.h"
#include "modules/sensor.h"

// ── FPS 計測用 ──
unsigned long lastFpsSecond = 0;  // 直近1秒判定用
int fpsFrameCounter = 0;
int currentFps = 0;
unsigned long lastDebugPrint = 0;   // デバッグ表示用タイマー
unsigned long lastFrameTimeUs = 0;  // 前回フレーム開始時刻

Preferences preferences;        // NVS へのアクセス用
bool racingMode = false;        // レーシングモードの状態
static int lastTouchCount = 0;  // タッチイベント検出用

// ────────────────────── デバッグ情報表示 ──────────────────────
static void printSensorDebugInfo()
{
  float pressure = calculateAverage(oilPressureSamples);
  float water = calculateAverage(waterTemperatureSamples);
  float oil = calculateAverage(oilTemperatureSamples);
  Serial.printf("Oil.P: %.2f bar, Water.T: %.1f C, Oil.T: %.1f C\n", pressure, water, oil);
}

// ────────────────────── タッチ処理 ──────────────────────
static void handleTouch()
{
  M5.update();
  int count = M5.Touch.getCount();
  if (count > 0 && lastTouchCount == 0)
  {
    // タップでモードを切り替え、NVS へ保存
    racingMode = !racingMode;
    preferences.putBool("r_mode", racingMode);
    Serial.printf("RacingMode: %s\n", racingMode ? "ON" : "OFF");
  }
  lastTouchCount = count;
}

// ────────────────────── setup() ──────────────────────
void setup()
{
  Serial.begin(115200);

  M5.begin();
  CoreS3.begin(M5.config());

  // NVS からモード状態を読み込み
  preferences.begin("rgauge", false);
  racingMode = preferences.getBool("r_mode", false);

  // WiFi を完全に停止
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true);

  // 電源管理を初期化し、処理順序を明確にする
  M5.Power.begin();              // まず電源モジュールを初期化
  M5.Power.setExtOutput(false);  // 外部給電時は 5V ピン出力を停止

  display.init();
  // DMA を初期化
  display.initDMA();
  display.setRotation(3);
  display.setColorDepth(DISPLAY_COLOR_DEPTH);
  display.setBrightness(BACKLIGHT_DAY);

  mainCanvas.setColorDepth(DISPLAY_COLOR_DEPTH);
  mainCanvas.setTextSize(1);
  // スプライトを PSRAM ではなく DMA メモリに確保
  mainCanvas.setPsram(false);
  // スプライト用の DMA を初期化
  mainCanvas.initDMA();
  mainCanvas.createSprite(LCD_WIDTH, LCD_HEIGHT);

  M5.Lcd.clear();
  M5.Lcd.fillScreen(COLOR_BLACK);

  // M5.Speaker.begin();  // スピーカーを使用しないため無効化
  // M5.Imu.begin();      // IMU を使用しないため無効化
  btStop();

  pinMode(9, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  Wire.begin(9, 8);

  if (!adsConverter.begin())
  {
    Serial.println("[ADS1015] init failed… all analog values will be 0");
  }
  adsConverter.setDataRate(RATE_ADS1015_1600SPS);

  if (SENSOR_AMBIENT_LIGHT_PRESENT)
  {
    // ALS のゲインと積分時間を設定してから初期化
    Ltr5xx_Init_Basic_Para ltr553Params = LTR5XX_BASE_PARA_CONFIG_DEFAULT;
    // 感度を1倍、積分時間を100msに設定
    ltr553Params.als_gain = LTR5XX_ALS_GAIN_1X;
    ltr553Params.als_integration_time = LTR5XX_ALS_INTEGRATION_TIME_100MS;
    CoreS3.Ltr553.begin(&ltr553Params);
    // 通常はスタンバイにしておき、測定時のみアクティブにする
    CoreS3.Ltr553.setAlsMode(LTR5XX_ALS_STAND_BY_MODE);
  }
}

// ────────────────────── loop() ──────────────────────
void loop()
{
  static unsigned long lastAlsMeasurementTime = 0;
  unsigned long nowUs = micros();
  // 前のフレームから16.6ms未満なら待機
  if (lastFrameTimeUs != 0 && nowUs - lastFrameTimeUs < FRAME_INTERVAL_US)
  {
    delayMicroseconds(FRAME_INTERVAL_US - (nowUs - lastFrameTimeUs));
    nowUs = micros();
  }
  lastFrameTimeUs = nowUs;
  unsigned long now = millis();

  handleTouch();  // タッチ入力を処理

  if (now - lastAlsMeasurementTime >= ALS_MEASUREMENT_INTERVAL_MS)
  {
    updateBacklightLevel();
    lastAlsMeasurementTime = now;
  }

  acquireSensorData();
  updateGauges();

  fpsFrameCounter++;
  if (now - lastFpsSecond >= FPS_INTERVAL_MS)
  {
    currentFps = fpsFrameCounter;
    if (DEBUG_MODE_ENABLED)
    {
      Serial.printf("FPS:%d\n", currentFps);
    }
    fpsFrameCounter = 0;
    lastFpsSecond = now;
  }

  if (DEBUG_MODE_ENABLED && now - lastDebugPrint >= 1000UL)
  {
    // FPS更新とは別に1秒ごとにデータを出力
    printSensorDebugInfo();
    lastDebugPrint = now;
  }
}
