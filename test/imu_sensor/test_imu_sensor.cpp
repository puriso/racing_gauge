#include <Adafruit_I2CDevice.h>
#include <M5CoreS3.h>

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "adc_reader.h"
#include "low_warning.h"
#include "risk_history.h"
#include "sensor.h"

// 外部のIMU・I2C・描画装置と時刻だけを置き換える。取得・校正・LOW・履歴は本番コードを使う。
// NOLINTBEGIN(readability-magic-numbers)
static uint32_t clockMs = 0;
auto millis() -> unsigned long { return clockMs; }
void delay(unsigned long milliseconds) { clockMs += milliseconds; }
void delayMicroseconds(unsigned int /*microseconds*/) {}

static void expect(bool condition, const char* message)
{
  if (!condition)
  {
    throw std::runtime_error(message);
  }
}

static void tick(uint32_t stepMs = 17)
{
  acquireSensorData();
  const float pressure = calculateAverage(oilPressureSamples);
  const bool valid = oilPressureStatus == SensorStatus::Valid;
  updateLowPressureWarning(currentGForce, pressure, valid);
  risk::history.update(clockMs, {
                                    pressure,
                                    currentGForce,
                                    calculateAverage(oilTemperatureSamples),
                                    valid,
                                    oilTemperatureStatus == SensorStatus::Valid,
                                    isLowPressureQualified(),
                                    currentGCardinalDirection,
                                });
  clockMs += stepMs;
}

static void ticks(int count)
{
  for (int index = 0; index < count; ++index)
  {
    tick();
  }
}

static void initialize()
{
  fake::sample = 1000;
  simulator::acceleration = {0.125F, -0.0625F, 1};
  expect(initializeAdc(), "ADCを初期化できる");
}

static void calibrated()
{
  initialize();
  ticks(100);
  expect(std::fabs(currentGForce) < 0.0001F, "起動時バイアスを補正する");
}

static void expectInvalidG()
{
  expect(std::isnan(currentGForce), "失敗・未校正のGは正常な0Gと区別する");
  expect(currentGCardinalDirection == GDirection::None && std::string(currentGDirection) == "---", "古い方向も無効化する");
}

static void staleHighG()
{
  calibrated();
  simulator::acceleration[0] += 1.8F;
  ticks(10);
  expect(currentGForce > 1.7F, "切断直前の高Gを取得する");
  simulator::imuConnected = false;
  simulator::acceleration[0] = 0.125F;
  fake::sample = 400;
  const int before = fake::conversions;
  ticks(90);
  expectInvalidG();
  expect(M5.Imu.getImuData().accel.x > 1.8F, "外部ライブラリは切断前の高Gを返し続ける");
  expect(fake::conversions > before && calculateAverage(oilPressureSamples) < 2,
         "IMU切断中もADCから低い油圧を取得し続ける");
  expect(!isLowPressureQualified() && !hasLowWarningHistory(), "古い高GでLOWやWを誤成立させない");
  expect(!risk::history.hasEvent(), "古い高Gで危険点を誤記録しない");
}

static void gyroOnlyUpdate()
{
  calibrated();
  simulator::acceleration[0] += 1.8F;
  ticks(5);
  simulator::imuAccelUpdated = false;
  fake::sample = 400;
  ticks(90);
  expectInvalidG();
  expect(!hasLowWarningHistory(), "ジャイロだけ成功しても古い加速度でLOWにしない");
}

static void unavailableAtStartup()
{
  initialize();
  simulator::imuConnected = false;
  ticks(100);
  expectInvalidG();
  expect(oilPressureStatus == SensorStatus::Valid && oilTemperatureStatus == SensorStatus::Valid &&
             waterTemperatureStatus == SensorStatus::Valid,
         "起動時からIMUがなくても油圧・油温・水温の取得を続ける");
  expect(!risk::history.hasSelection(), "未取得のGを代表点に記録しない");
  simulator::imuConnected = true;
  tick();
  clockMs += 200;
  ticks(19);
  expectInvalidG();
  tick();
  expect(std::fabs(currentGForce) < 0.0001F, "復帰後の正常な20サンプルだけで校正する");
}

static void interruptedCalibration()
{
  initialize();
  tick();
  clockMs = 200;
  ticks(10);
  simulator::imuConnected = false;
  simulator::acceleration[0] = 2;
  ticks(50);
  expectInvalidG();
  simulator::imuConnected = true;
  simulator::acceleration[0] = 0.125F;
  ticks(9);
  expectInvalidG();
  tick();
  expect(std::fabs(currentGForce) < 0.0001F, "途中の通信失敗を校正サンプル数やオフセットへ加算しない");
}

static void nonfiniteAxes()
{
  calibrated();
  for (float& axis : simulator::acceleration)
  {
    const float saved = axis;
    for (const float invalid : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()})
    {
      axis = invalid;
      tick();
      expectInvalidG();
    }
    axis = saved;
    tick();
    expect(std::fabs(currentGForce) < 0.0001F, "非数値から正常取得へ復帰できる");
  }
}

static void recoveryAndWarningDelay()
{
  calibrated();
  fake::sample = 400;
  ticks(10);
  simulator::acceleration[0] += 1.8F;
  ticks(20);
  expect(!isLowPressureQualified(), "500ms未満ではLOWにしない");
  simulator::imuConnected = false;
  ticks(90);
  expect(!hasLowWarningHistory(), "失敗中の時間をLOWの継続時間に加算しない");
  simulator::imuConnected = true;
  const uint32_t recovered = clockMs;
  tick();
  expect(std::fabs(currentGForce - 1.8F) < 0.0001F, "元の校正値で即座に取得再開する");
  clockMs = recovered + 499;
  tick();
  expect(!isLowPressureQualified(), "復帰後499msではまだLOWにしない");
  clockMs = recovered + 500;
  tick();
  expect(isLowPressureQualified() && hasLowWarningHistory(), "正常取得が500ms継続すればLOWとWを記録する");
  expect(risk::history.event().kind == risk::Kind::Low, "正常なLOWを危険点へ記録できる");
  simulator::imuConnected = false;
  tick();
  expect(!isLowPressureQualified() && hasLowWarningHistory(), "成立済みWはIMU失敗で消さない");
}

static void historyGap()
{
  calibrated();
  expect(risk::history.hasSelection(), "正常時の代表点がある");
  const uint64_t selectedMs = risk::history.event().occurredMs;
  simulator::imuConnected = false;
  ticks(30);
  const uint64_t lastTick = risk::history.nowMs() / risk::SAMPLE_MS;
  const size_t index = risk::HALF_WINDOW + lastTick - (selectedMs / risk::SAMPLE_MS);
  const auto& sample = risk::history.sample(index);
  expect((sample.flags & risk::GValid) == 0, "失敗中のGをグラフの正常線へ含めない");
  expect((sample.flags & risk::PressureValid) != 0 && (sample.flags & risk::OilValid) != 0,
         "失敗中も油圧・油温のグラフを記録する");
  expect(risk::history.event().occurredMs == selectedMs, "過去の正常な記録点を保持する");
}

auto main(int argc, char** argv) -> int
try
{
  struct TestCase
  {
    const char* name;
    void (*run)();
  };
  const std::array<TestCase, 7> tests = {
      {
          {"stale_high_g", staleHighG},
          {"gyro_only_update", gyroOnlyUpdate},
          {"startup_failure", unavailableAtStartup},
          {"interrupted_calibration", interruptedCalibration},
          {"nonfinite_axes", nonfiniteAxes},
          {"recovery_warning_delay", recoveryAndWarningDelay},
          {"history_gap", historyGap},
      },
  };
  if (argc != 2)
  {
    return 2;
  }
  for (const auto& test : tests)
  {
    if (std::string(argv[1]) == "--list")
    {
      std::cout << test.name << '\n';
    }
    else if (std::string(argv[1]) == test.name)
    {
      try
      {
        test.run();
        return 0;
      }
      catch (const std::exception& error)
      {
        std::cerr << test.name << ": " << error.what() << '\n';
        return 1;
      }
    }
  }
  return std::string(argv[1]) == "--list" ? 0 : 2;
}
catch (...)
{
  return 1;
}
// NOLINTEND(readability-magic-numbers)
