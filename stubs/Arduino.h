#ifndef STUBS_ARDUINO_H
#define STUBS_ARDUINO_H

#include <cstdint>

// ────────────────────── Arduino 互換スタブ ──────────────────────
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
inline auto millis() -> unsigned long { return 0; }
inline auto micros() -> unsigned long { return 0; }
inline auto radians(float deg) -> float { return deg * 3.14159265358979323846F / 180.0F; }

constexpr int INPUT_PULLUP = 0;

inline void pinMode(int, int) {}

class HardwareSerial
{
public:
  void begin(unsigned long) {}
  void println(const char *) {}
  void printf(const char *, ...) {}
  void setTextSize(int) {}
  void setTextColor(uint16_t) {}
  void setCursor(int, int) {}
};

// デバッグ出力用シリアルのスタブ
inline HardwareSerial Serial;

#endif  // STUBS_ARDUINO_H
