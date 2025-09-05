#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>  // 整数型定義

// ────────────────────── 設定 ──────────────────────
// デバッグ用メッセージ表示の有無
#define DEBUG_MODE_ENABLED 0

// デモモードを有効にするかどうか
#define DEMO_MODE_ENABLED 0

// FPS表示を行うかどうか
#define FPS_DISPLAY_ENABLED 0

// ── センサー接続可否（0 にするとその項目は常に 0 表示） ──
#define SENSOR_OIL_PRESSURE_PRESENT 1
#define SENSOR_WATER_TEMP_PRESENT 1
// 油温センサーを使用するかどうか
#define SENSOR_OIL_TEMP_PRESENT 1
// 照度センサーを使用する場合は 1
#define SENSOR_AMBIENT_LIGHT_PRESENT 1

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
// 水温メーター下限と上限を80℃〜120℃に設定
constexpr float WATER_TEMP_METER_MIN = 85.0f;
constexpr float WATER_TEMP_METER_MAX = 115.0f;

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

constexpr int MEDIAN_BUFFER_SIZE = 6;

// レーシングモード継続時間 [ms]
constexpr unsigned long RACING_MODE_DURATION_MS = 180000UL;

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
