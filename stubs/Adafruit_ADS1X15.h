#ifndef STUBS_ADAFRUIT_ADS1X15_H
#define STUBS_ADAFRUIT_ADS1X15_H

#include <cstdint>

// ────────────────────── ADS1015 スタブ ──────────────────────
constexpr int RATE_ADS1015_1600SPS = 0;

class Adafruit_ADS1015
{
public:
  bool begin() { return true; }
  void setDataRate(int) {}
  auto readADC_SingleEnded(uint8_t) -> int16_t { return 0; }
};

#endif  // STUBS_ADAFRUIT_ADS1X15_H
