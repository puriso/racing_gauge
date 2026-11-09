#pragma once

#include <array>
#include <cstdio>

namespace simulator
{
inline constexpr int DEFAULT_AMBIENT_LUX = 100;
inline int ambientLux = DEFAULT_AMBIENT_LUX;
inline constexpr unsigned int IMU_AXIS_COUNT = 3;
inline std::array<float, IMU_AXIS_COUNT> acceleration{};
}  // namespace simulator

// 照度の取得境界を置き換え、輝度制御は本番コードを使用する。
struct SimulatorHardware
{
  struct AmbientLight
  {
    static auto getAlsValue() -> int { return simulator::ambientLux; }
  } Ltr553;
};
inline SimulatorHardware CoreS3;

struct SimulatorM5
{
  struct ImuSensor
  {
    // 本番APIと同じ各軸のG値を返し、起動時補正・水平G・方向の算出は省略しない。
    static void getAccelData(float* axisX, float* axisY, float* axisZ)  // NOLINT(bugprone-easily-swappable-parameters)
    {
      *axisX = simulator::acceleration[0];
      *axisY = simulator::acceleration[1];
      *axisZ = simulator::acceleration[2];
    }
  } Imu;
};
// 実機と同じAPI名を公開する。
inline SimulatorM5 M5;  // NOLINT(readability-identifier-length)

struct SimulatorSerial
{
  template <typename... Args>
  static void printf(const char* format, Args... args)
  {
    std::printf(format, args...);
  }
};
inline SimulatorSerial Serial;
