#ifndef STUBS_M5CORES3_H
#define STUBS_M5CORES3_H

#include "Arduino.h"
#include "M5GFX.h"
#include "WiFi.h"

// ────────────────────── LTR-553 関連スタブ ──────────────────────
struct Ltr5xx_Init_Basic_Para
{
  int ps_led_pulse_freq = 0;
  int als_gain = 0;
  int als_integration_time = 0;
};

constexpr Ltr5xx_Init_Basic_Para LTR5XX_BASE_PARA_CONFIG_DEFAULT = {};
constexpr int LTR5XX_LED_PULSE_FREQ_40KHZ = 0;
constexpr int LTR5XX_ALS_GAIN_1X = 0;
constexpr int LTR5XX_ALS_INTEGRATION_TIME_100MS = 0;
constexpr int LTR5XX_ALS_ACTIVE_MODE = 0;

class Ltr553Class
{
public:
  void begin(Ltr5xx_Init_Basic_Para *) {}
  void setAlsMode(int) {}
  auto getAlsValue() const -> int { return 0; }
};

// ────────────────────── CoreS3 本体スタブ ──────────────────────
class CoreS3Class
{
public:
  void begin(int) {}
  Ltr553Class Ltr553;
};

inline CoreS3Class CoreS3;

// ────────────────────── M5CoreS3 メインクラス ──────────────────────
class M5Power
{
public:
  void begin() {}
  void setExtOutput(bool) {}
  void setLed(int) {}
  void setUsbOutput(bool) {}
  void setBatteryCharge(bool) {}
};

class M5Lcd
{
public:
  void clear() {}
  void fillScreen(uint16_t) {}
  void setTextSize(int) {}
  void setTextColor(uint16_t) {}
  void setCursor(int, int) {}
  void println(const char *) {}
};

class M5Imu
{
public:
  void begin() {}
  void getAccelData(float *x, float *y, float *z)
  {
    if (x) *x = 0.0F;
    if (y) *y = 0.0F;
    if (z) *z = 0.0F;
  }
};

class M5Touch
{
public:
  auto getCount() const -> int { return 0; }
};

class M5Class
{
public:
  void begin() {}
  auto config() const -> int { return 0; }
  void update() {}

  M5Power Power;
  M5Lcd Lcd;
  M5Imu Imu;
  M5Touch Touch;
};

inline M5Class M5;

inline void btStop() {}

#endif  // STUBS_M5CORES3_H
