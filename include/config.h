#ifndef CONFIG_H
#define CONFIG_H

#include <M5CoreS3.h>

// ────────────────────── 設定 ──────────────────────
// デバッグ用メッセージ表示の有無
constexpr bool DEBUG_MODE_ENABLED = false;

// デモモードを有効にするかどうか
constexpr bool DEMO_MODE_ENABLED = false;

// ── センサー接続可否（false にするとその項目は常に 0 表示） ──
constexpr bool SENSOR_OIL_PRESSURE_PRESENT = true;
constexpr bool SENSOR_WATER_TEMP_PRESENT = true;
// 油温センサーを使用するかどうか
constexpr bool SENSOR_OIL_TEMP_PRESENT = true;
// 照度センサーを使用する場合は true
constexpr bool SENSOR_AMBIENT_LIGHT_PRESENT = true;

// ── 電圧降下補正 ──
// 0.3sq ケーブル往復14mで約0.137Vの降下を想定
constexpr float VOLTAGE_DROP = 0.137f;

// ── 色設定 (16 bit) ──
// RGB888 から 565 形式へ変換する constexpr 関数
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b)
{
  return (static_cast<uint16_t>(r & 0xF8) << 8) | (static_cast<uint16_t>(g & 0xFC) << 3) | (static_cast<uint16_t>(b) >> 3);
}

constexpr uint16_t COLOR_WHITE = rgb565(255, 255, 255);
constexpr uint16_t COLOR_BLACK = rgb565(0, 0, 0);
constexpr uint16_t COLOR_ORANGE = rgb565(255, 165, 0);
constexpr uint16_t COLOR_YELLOW = rgb565(255, 255, 0);
constexpr uint16_t COLOR_RED = rgb565(255, 0, 0);
constexpr uint16_t COLOR_GRAY = rgb565(169, 169, 169);

// ディスプレイの色深度
constexpr int DISPLAY_COLOR_DEPTH = 16;

// ── 油圧の表示設定 ──
// 数値表示の上限 (バー単位)
constexpr float MAX_OIL_PRESSURE_DISPLAY = 15.0f;
// メーター目盛の上限
constexpr float MAX_OIL_PRESSURE_METER = 10.0f;
// 0.25bar 以下なら接続エラーとして扱う閾値
constexpr float OIL_PRESSURE_DISCONNECT_THRESHOLD = 0.25f;
// 油圧の平滑化係数
// レスポンス向上のため平滑化係数を大きめに
constexpr float OIL_PRESSURE_SMOOTHING_ALPHA = 0.3f;

// ── 水温メーター設定 ──
// 水温メーター下限と上限を80℃〜105℃に設定
constexpr float WATER_TEMP_METER_MIN = 80.0f;
constexpr float WATER_TEMP_METER_MAX = 105.0f;

// ── 画面サイズ ──
constexpr int LCD_WIDTH = 320;
constexpr int LCD_HEIGHT = 240;

// ── ALS/輝度自動制御 ──
enum class BrightnessMode
{
  Day,
  Dusk,
  Night
};

// ALS の輝度判定閾値
constexpr uint8_t LUX_THRESHOLD_DAY = 15;
constexpr uint8_t LUX_THRESHOLD_DUSK = 10;

constexpr uint8_t BACKLIGHT_DAY = 255;
constexpr uint8_t BACKLIGHT_DUSK = 200;
constexpr uint8_t BACKLIGHT_NIGHT = 60;

// ALS 取得間隔 [ms]
constexpr uint16_t ALS_MEASUREMENT_INTERVAL_MS = 500;

// 初回は5秒分のデータで中央値を計算
constexpr unsigned long ALS_FIRST_INTERVAL_MS = 5000UL;
// 2回目以降は10秒ごとに中央値を計算
constexpr unsigned long ALS_UPDATE_INTERVAL_MS = 10000UL;
// 10秒分のサンプルを保持するためバッファサイズを計算
constexpr int MEDIAN_BUFFER_SIZE = ALS_UPDATE_INTERVAL_MS / ALS_MEASUREMENT_INTERVAL_MS;

// FPS 更新間隔 [ms]
constexpr unsigned long FPS_INTERVAL_MS = 1000UL;

// 最大60FPSに制御するためのフレーム間隔 [us]
constexpr unsigned long FRAME_INTERVAL_US = 1000000UL / 60;

// ── ADS1015 のチャンネル定義 ──
constexpr uint8_t ADC_CH_WATER_TEMP = 1;
constexpr uint8_t ADC_CH_OIL_PRESSURE = 2;
constexpr uint8_t ADC_CH_OIL_TEMP = 0;

// サンプリング数設定
constexpr int PRESSURE_SAMPLE_SIZE = 5;
constexpr int WATER_TEMP_SAMPLE_SIZE = 2;  // 500ms間隔×2サンプルで約1秒平均
constexpr int OIL_TEMP_SAMPLE_SIZE = 2;    // 500ms間隔×2サンプルで約1秒平均

#endif  // CONFIG_H
