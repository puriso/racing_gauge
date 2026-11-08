#pragma once

#include <cstdio>

// 実機の照度センサーだけを置き換え、輝度制御は本番コードを使用する。
struct SimulatorHardware
{
  struct AmbientLight
  {
    static auto getAlsValue() -> int { return 100; }
  } Ltr553;
};
inline SimulatorHardware CoreS3;

struct SimulatorSerial
{
  template <typename... Args>
  static void printf(const char* format, Args... args)
  {
    std::printf(format, args...);
  }
};
inline SimulatorSerial Serial;
