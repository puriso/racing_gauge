#pragma once

#include <cstdio>

namespace simulator
{
inline constexpr int DEFAULT_AMBIENT_LUX = 100;
inline int ambientLux = DEFAULT_AMBIENT_LUX;
}  // namespace simulator

// 実機の照度センサーだけを置き換え、輝度制御は本番コードを使用する。
struct SimulatorHardware
{
  struct AmbientLight
  {
    static auto getAlsValue() -> int { return simulator::ambientLux; }
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
