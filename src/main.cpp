#include <M5CoreS3.h>
#include <WiFi.h>  // WiFi 無効化用
#include <Wire.h>

#include "config.h"
#include "modules/backlight.h"
#include "modules/display.h"
#include "modules/sensor.h"

// ── FPS 計測用 ──
unsigned long lastFpsSecond = 0;  // 直近1秒判定用
int fpsFrameCounter = 0;
int currentFps = 0;
unsigned long lastDebugPrint = 0;                                    // デバッグ表示用タイマー
unsigned long lastFrameTimeUs = 0;                                   // 前回フレーム開始時刻
bool isMenuVisible = false;                                          // メニュー表示中かどうか
static bool wasTouched = false;                                      // 前回タッチされていたか
static BrightnessMode previousBrightnessMode = BrightnessMode::Day;  // メニュー前の輝度モード

// ────────────────────── デバッグ情報表示 ──────────────────────
static void printSensorDebugInfo()
{
  float pressure = calculateAverage(oilPressureSamples);
  float water = calculateAverage(waterTemperatureSamples);
  float oil = calculateAverage(oilTemperatureSamples);
  // 水平Gと各センサー値をシリアルに表示
  Serial.printf("G: %.2f%s, Oil.P: %.2f bar, Water.T: %.1f C, Oil.T: %.1f C\n", currentGForce, currentGDirection, pressure,
                water, oil);
}

// ────────────────────── setup() ──────────────────────
void setup()
{
  Serial.begin(115200);

  M5.begin();
  CoreS3.begin(M5.config());

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
  M5.Imu.begin();  // IMU を使用
  btStop();

  pinMode(9, INPUT_PULLUP);
  pinMode(8, INPUT_PULLUP);
  Wire.begin(9, 8);

  if (!DEMO_MODE_ENABLED)
  {
    // デモモードでなければADS1015を初期化し、失敗時は画面にエラーを表示
    if (!adsConverter.begin())
    {
      Serial.println("[ADS1015] init failed… all analog values will be 0");
      M5.Lcd.setTextSize(2);
      M5.Lcd.setTextColor(COLOR_RED);
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.println("ADS1015 init failed");
      M5.Lcd.println("Check wiring");
    }
    else
    {
      adsConverter.setDataRate(RATE_ADS1015_1600SPS);
    }
  }

  if (SENSOR_AMBIENT_LIGHT_PRESENT)
  {
    // ALS のゲインと積分時間を設定してから初期化
    Ltr5xx_Init_Basic_Para ltr553Params = LTR5XX_BASE_PARA_CONFIG_DEFAULT;
    ltr553Params.ps_led_pulse_freq = LTR5XX_LED_PULSE_FREQ_40KHZ;
    ltr553Params.als_gain = LTR5XX_ALS_GAIN_1X;
    ltr553Params.als_integration_time = LTR5XX_ALS_INTEGRATION_TIME_100MS;
    CoreS3.Ltr553.begin(&ltr553Params);
    CoreS3.Ltr553.setAlsMode(LTR5XX_ALS_ACTIVE_MODE);
    // 初回起動時に照度を取得して輝度を決定
    updateBacklightLevel();
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

  M5.update();

  if (!isMenuVisible && now - lastAlsMeasurementTime >= ALS_MEASUREMENT_INTERVAL_MS)
  {
    updateBacklightLevel();
    lastAlsMeasurementTime = now;
  }

  bool touched = M5.Touch.getCount() > 0;
  if (touched && !wasTouched)
  {
    isMenuVisible = !isMenuVisible;
    if (isMenuVisible)
    {
      previousBrightnessMode = currentBrightnessMode;  // 現在の輝度モードを保存
      drawMenuScreen();
      // メニュー表示中は輝度を最大にする
      applyBrightnessMode(BrightnessMode::Day);
    }
    else
    {
      resetGaugeState();
      // メニュー終了後は元の輝度に戻す
#if SENSOR_AMBIENT_LIGHT_PRESENT
      updateBacklightLevel();
#else
      applyBrightnessMode(previousBrightnessMode);
#endif
    }
  }
  wasTouched = touched;

  acquireSensorData();
  if (!isMenuVisible)
  {
    updateGauges();
  }

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
