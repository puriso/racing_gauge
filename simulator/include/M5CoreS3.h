#pragma once

#include <array>
#include <cstdint>
#include <cstdio>

namespace simulator
{
inline constexpr int DEFAULT_AMBIENT_LUX = 100;
inline int ambientLux = DEFAULT_AMBIENT_LUX;
inline constexpr unsigned int IMU_AXIS_COUNT = 3;
inline std::array<float, IMU_AXIS_COUNT> acceleration{};
inline bool imuConnected = true;
inline bool imuAccelUpdated = true;
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
    enum SensorMask : uint8_t
    {
      sensor_mask_accel = 1,
      sensor_mask_gyro = 2,
    };
    struct ImuData
    {
      struct Acceleration
      {
        float x = 0;
        float y = 0;
        float z = 0;
      } accel;
    };
    ImuData cached;

    auto update() -> uint8_t
    {
      if (!simulator::imuConnected)
      {
        return 0;
      }
      if (!simulator::imuAccelUpdated)
      {
        return sensor_mask_gyro;
      }
      cached.accel = {simulator::acceleration[0], simulator::acceleration[1], simulator::acceleration[2]};
      return sensor_mask_accel | sensor_mask_gyro;
    }
    // 通信失敗時も前回値を返す実ライブラリの仕様を再現する。
    [[nodiscard]] auto getImuData() const -> const ImuData& { return cached; }
    auto getAccelData(float* axisX, float* axisY, float* axisZ) -> bool  // NOLINT(bugprone-easily-swappable-parameters)
    {
      const bool result = update() != 0;
      *axisX = cached.accel.x;
      *axisY = cached.accel.y;
      *axisZ = cached.accel.z;
      return result;
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
