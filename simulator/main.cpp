#include <M5CoreS3.h>
#include <SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "adc_steps.hpp"
#include "compat.hpp"
#include "fault_inputs.hpp"
#include "modules/adc_reader.h"
#include "modules/backlight.h"
#include "modules/display.h"
#include "modules/low_warning.h"
#include "modules/racing_indicator.h"
#include "modules/racing_mode.h"
#include "modules/risk_graph.h"
#include "modules/sensor.h"
#include "modules/startup_error.h"
#include "sensor_inputs.hpp"
#include "telemetry.hpp"
#include "tsukuba_lap.hpp"

namespace simulator
{
uint32_t nowMs = 0;
}

namespace
{
constexpr unsigned int TICKS_PER_SECOND = 24;
constexpr unsigned int DEFAULT_SECONDS = 20;
constexpr unsigned int MIN_SECONDS = 5;
constexpr unsigned int MAX_SECONDS = 240;
constexpr unsigned int MS_PER_SECOND = 1000;
constexpr unsigned int US_PER_SECOND = 1000000;
constexpr unsigned int SDL_STEP_MS = 128;
constexpr uint8_t PAUSE_PIN = 20;
constexpr uint8_t REPLAY_PIN = 21;
constexpr uint8_t MENU_PIN = 22;

struct Options
{
  std::string scenario = "ramp";
  std::filesystem::path capture;
  unsigned int seconds = 0;
};

auto options() -> Options&
{
  static Options result;
  return result;
}
std::atomic<int> exitStatus{1};
bool adcInitializationFailed = false;

using Inputs = simulator::RawInputs;

struct Targets
{
  float oil;
  float water;
  float pressure;
  float acceleration;
  bool menu;
  int lux = simulator::DEFAULT_AMBIENT_LUX;
};

auto isTsukubaScenario() -> bool { return options().scenario == "tsukuba-lap" || options().scenario == "tsukuba-low"; }
auto isRiskScenario() -> bool
{
  return isTsukubaScenario() || options().scenario == "risk-window" || options().scenario == "risk-predictive";
}

auto isWarningReview() -> bool
{
  return options().scenario == "warning-repeat" || options().scenario == "warning-pending" ||
         options().scenario == "warning-brief";
}

auto isAdcOrSensorFaultScenario() -> bool
{
  return options().scenario == "adc-startup" || options().scenario == "adc-communication" ||
         simulator::isFaultScenario(options().scenario);
}

// シナリオの物理量と時点を、README の表と対応するリテラルで記載する。
// NOLINTBEGIN(readability-magic-numbers)
auto defaultSeconds() -> unsigned int
{
  if (isTsukubaScenario()) return simulator::TSUKUBA_SECONDS;
  if (isRiskScenario()) return 160;
  if (options().scenario == "negative-maxima") return 24;
  if (options().scenario == "menu-monitoring") return 28;
  if (options().scenario == "fault-monitoring") return 20;
  if (options().scenario == "warning-history")
  {
    return 20;
  }
  if (isWarningReview())
  {
    return 12;
  }
  if (options().scenario == "adc-communication")
  {
    return 56;
  }
  if (options().scenario == "adc-startup")
  {
    return 16;
  }
  if (options().scenario == "adc-steps")
  {
    return 173;
  }
  if (options().scenario == "ramp")
  {
    return 200;
  }
  if (options().scenario == "warnings" || simulator::isFaultScenario(options().scenario))
  {
    return 40;
  }
  if (options().scenario == "startup" || options().scenario == "warm-start")
  {
    return 30;
  }
  if (options().scenario == "cooldown")
  {
    return 100;
  }
  if (options().scenario == "circuit")
  {
    return 36;
  }
  if (options().scenario == "racing")
  {
    return 196;
  }
  if (options().scenario == "ambient")
  {
    return 136;
  }
  if (options().scenario == "raw-faults")
  {
    return 24;
  }
  return DEFAULT_SECONDS;
}

auto interpolate(float start, float end, float progress) -> float
{
  return start + ((end - start) * std::clamp(progress, 0.0F, 1.0F));
}

auto runningPressure(float seconds) -> float
{
  // 4秒周期の三角波で3〜6barを連続的に往復する。
  const float phase = std::fmod(seconds, 4.0F) / 2.0F;
  return 3.0F + (3.0F * (phase <= 1.0F ? phase : 2.0F - phase));
}

auto pressureDip(float seconds) -> float
{
  // 境界の直上・一致・直下を短時間通過してから、警告成立と回復を確認する。
  if (seconds >= 8.0F && seconds < 8.25F)
  {
    return 3.1F;
  }
  if (seconds >= 12.0F && seconds < 12.25F)
  {
    return 3.0F;
  }
  if (seconds >= 16.0F && seconds < 16.25F)
  {
    return 2.9F;
  }
  if (seconds >= 22.0F && seconds < 24.0F)
  {
    return 2.9F;
  }
  return runningPressure(seconds);
}

auto sampleCircuit(float progress, float seconds, float pressure) -> Targets
{
  // 瞬間的なGと、継続するコーナリングでの油圧急落を分けて確認する。
  const bool shortSpike = (seconds >= 4.0F && seconds < 4.05F) || (seconds >= 18.0F && seconds < 18.2F);
  const bool corner = (seconds >= 8.0F && seconds < 10.5F) || (seconds >= 22.0F && seconds < 24.0F);
  const bool event = shortSpike || corner;
  return {
      interpolate(95.0F, 125.0F, progress),
      interpolate(90.0F, 112.0F, progress),
      event ? 0.8F : pressure,
      event ? 2.0F : 0.0F,
      false,
  };
}

auto sampleRacing(float seconds, float pressure) -> Targets
{
  const bool spike = seconds >= 2.0F && seconds < 2.05F;
  const bool start = (seconds >= 4.0F && seconds < 4.2F) || (seconds >= 187.0F && seconds < 187.2F);
  float acceleration = 0.0F;
  if (spike)
  {
    acceleration = 2.0F;
  }
  else if (start)
  {
    acceleration = 1.0F;
  }
  return {100.0F, 95.0F, pressure, acceleration, seconds >= 190.0F && seconds < 192.0F, 2};
}

auto sampleAmbient(float seconds, float pressure) -> Targets
{
  // 8秒周期の測定と6点中央値を経て、昼・夕方・夜・昼へ遷移する。
  int lux = 100;
  if (seconds >= 24.0F && seconds < 64.0F)
  {
    lux = 12;
  }
  else if (seconds >= 64.0F && seconds < 104.0F)
  {
    lux = 2;
  }
  return {100.0F, 95.0F, pressure, 0.0F, seconds >= 96.0F && seconds < 98.0F, lux};
}

auto sampleRawFaults(float seconds, float pressure) -> Targets
{
  return {100.0F, 95.0F, pressure, seconds >= 12.0F && seconds < 14.0F ? 2.0F : 0.0F, false};
}

auto sampleWarningReview(float seconds) -> Targets
{
  const bool first = seconds >= 2.0F && seconds < 3.5F;
  const bool repeat = options().scenario == "warning-repeat" && seconds >= 4.0F && seconds < 5.5F;
  const bool pending = options().scenario == "warning-pending" && seconds >= 6.0F && seconds < 7.5F;
  const bool brief = options().scenario == "warning-brief" && seconds >= 4.0F && seconds < 4.25F;
  const bool history = options().scenario == "warning-history" && seconds >= 14.0F && seconds < 15.5F;
  const bool second = repeat || pending || brief || history;
  float acceleration = 0.0F;
  if (first)
  {
    acceleration = 1.2F;
  }
  else if (second)
  {
    acceleration = 1.5F;
  }
  const float pressure = first || second ? 1.0F : runningPressure(seconds);
  return {
      100.0F, 95.0F, pressure, acceleration, options().scenario == "warning-history" && seconds >= 8.0F && seconds < 10.0F,
  };
}

auto sample(double ratio) -> Targets
{
  // 時間指定を変えても同じ順序のシナリオを伸縮する。
  const auto progress = static_cast<float>(ratio);
  const auto seconds = static_cast<float>(ratio * defaultSeconds());
  if (isTsukubaScenario())
  {
    const auto lap = simulator::tsukubaLap(seconds, options().scenario == "tsukuba-low");
    return {lap.oil, lap.water, lap.pressure, std::hypot(lap.longitudinal, lap.lateral), false};
  }
  if (options().scenario == "adc-steps")
  {
    return {0.0F, 0.0F, 0.0F, 0.0F, false};
  }
  const float pressure = runningPressure(seconds);
  if (isWarningReview() || options().scenario == "warning-history")
  {
    return sampleWarningReview(seconds);
  }
  if (isAdcOrSensorFaultScenario())
  {
    return {100.0F, 95.0F, pressure, 0.0F, false};
  }
  if (isRiskScenario())
  {
    if (options().scenario == "risk-window")
    {
      const bool corner = seconds >= 75.0F && seconds < 78.0F;
      const float g = corner                                ? 2.0F
                      : seconds >= 40.0F && seconds < 41.0F ? 1.4F
                      : seconds >= 55.0F && seconds < 56.0F ? 1.8F
                      : seconds >= 95.0F && seconds < 96.0F ? 1.2F
                                                            : 0.0F;
      return {95.0F + progress * 30.0F, 95.0F, corner ? 0.8F : pressure, g, false};
    }
    const bool baseline = seconds >= 63.0F && seconds < 65.0F;
    const bool drop = seconds >= 65.0F && seconds < 65.9F;
    const bool near = seconds >= 75.0F && seconds < 76.2F;
    const bool brief = seconds >= 90.0F && seconds < 90.1F;
    return {90.0F + progress * 20.0F, 95.0F,
            baseline ? 6.0F
            : drop   ? 3.8F
            : near   ? 3.2F
            : brief  ? 1.0F
                     : pressure,
            drop                                                                                          ? 0.9F
            : near                                                                                        ? 1.5F
            : brief || ((seconds >= 95.0F && seconds < 96.0F) || (seconds >= 105.0F && seconds < 106.0F)) ? 2.0F
                                                                                                          : 0.0F,
            false};
  }
  if (options().scenario == "negative-maxima")
  {
    const bool warmNegative = seconds >= 6.0F && seconds < 9.0F;
    const bool colder = seconds >= 9.0F;
    const bool positive = seconds >= 19.0F;
    return {positive       ? 5.0F
            : warmNegative ? -8.0F
            : colder       ? -18.0F
                           : -15.0F,
            positive       ? 10.0F
            : warmNegative ? -12.0F
            : colder       ? -19.0F
                           : -18.0F,
            pressure, 0.0F,
            (seconds >= 8.0F && seconds < 10.0F) || (seconds >= 13.0F && seconds < 14.5F) || seconds >= 22.0F};
  }
  if (options().scenario == "menu-monitoring")
  {
    const bool peak = seconds >= 8.0F && seconds < 11.0F;
    const bool low = seconds >= 6.0F && seconds < 9.0F;
    return {peak ? 145.0F : 100.0F,
            peak ? 120.0F : 95.0F,
            low                                  ? 0.8F
            : seconds >= 9.0F && seconds < 10.0F ? 8.0F
                                                 : pressure,
            seconds >= 3.0F && seconds < 15.0F ? 2.0F : 0.0F,
            seconds >= 4.0F && seconds < 12.0F,
            2};
  }
  if (options().scenario == "fault-monitoring")
  {
    return {100.0F, 95.0F, seconds >= 12.0F && seconds < 15.0F ? 0.0F : pressure,
            (seconds >= 3.0F && seconds < 9.0F) || (seconds >= 11.0F && seconds < 16.0F) ? 2.0F : 0.0F, false};
  }
  if (options().scenario == "circuit")
  {
    return sampleCircuit(progress, seconds, pressure);
  }
  if (options().scenario == "racing")
  {
    return sampleRacing(seconds, pressure);
  }
  if (options().scenario == "ambient")
  {
    return sampleAmbient(seconds, pressure);
  }
  if (options().scenario == "raw-faults")
  {
    return sampleRawFaults(seconds, pressure);
  }
  if (options().scenario == "pressure-sweep")
  {
    const float sweep = progress <= 0.5F ? progress * 2.0F : (1.0F - progress) * 2.0F;
    return {100.0F, 95.0F, interpolate(0.0F, 10.0F, sweep), 0.0F, false};
  }
  if (options().scenario == "cooldown")
  {
    const float cooling = (seconds - 2.0F) / 90.0F;
    return {interpolate(170.0F, -10.0F, cooling), interpolate(130.0F, -10.0F, cooling), pressure, 0.0F, false};
  }
  if (options().scenario == "warnings")
  {
    return {
        interpolate(70.0F, 90.0F, progress),
        interpolate(70.0F, 85.0F, progress),
        pressureDip(seconds),
        seconds >= 6.0F && seconds < 30.0F ? 1.2F : 0.0F,
        false,
    };
  }
  if (options().scenario == "startup")
  {
    // 24FPSでのIMU補正後も70℃相当のADCを取得できるよう、最初の2秒は保持する。
    const bool initialReading = seconds < 2.0F;
    const float warming = (seconds - 2.0F) / 28.0F;
    return {
        initialReading ? 70.0F : interpolate(-10.0F, 50.0F, warming),
        initialReading ? 70.0F : interpolate(-10.0F, 40.0F, warming),
        pressure * std::clamp(seconds, 0.0F, 1.0F),
        0.0F,
        false,
    };
  }
  if (options().scenario == "warm-start")
  {
    return {
        interpolate(70.0F, 95.0F, progress),
        interpolate(70.0F, 90.0F, progress),
        pressure * std::clamp(seconds, 0.0F, 1.0F),
        0.0F,
        false,
    };
  }
  if (options().scenario == "menu")
  {
    return {
        interpolate(70.0F, 90.0F, progress), interpolate(70.0F, 85.0F, progress), pressure, 0.0F,
        seconds >= 8.0F && seconds < 13.0F,
    };
  }
  // 両端を保持し、平滑化後の低温・高温も目視しやすくする。
  const float warming = seconds < 100.0F ? (seconds - 2.0F) / 90.0F : (190.0F - seconds) / 90.0F;
  return {
      interpolate(-10.0F, 170.0F, warming), interpolate(-10.0F, 130.0F, warming), pressure, 0.0F, false,
  };
}

auto makeInputs(unsigned int frame, const Targets& target, float seconds) -> Inputs
{
  if (options().scenario == "adc-steps")
  {
    // 温度・油圧の目標値から逆算せず、生カウント自体を増減させる。
    return {
        simulator::adcSteps(frame),
        {simulator::FIXTURE_AX_BIAS, simulator::FIXTURE_AY_BIAS, 1.0F},
        false,
        simulator::DEFAULT_AMBIENT_LUX,
    };
  }
  auto input = simulator::sensorInputs(target);
  simulator::applyFaultInputs(options().scenario, frame * MS_PER_SECOND / TICKS_PER_SECOND, input.adc);
  if (isTsukubaScenario())
  {
    const auto lap = simulator::tsukubaLap(seconds, options().scenario == "tsukuba-low");
    input.imu = {simulator::FIXTURE_AX_BIAS + lap.longitudinal, simulator::FIXTURE_AY_BIAS + lap.lateral, 1.0F};
  }
  if (options().scenario == "risk-window")
  {
    if (seconds >= 40.0F && seconds < 41.0F)
    {
      input.imu = {simulator::FIXTURE_AX_BIAS + target.acceleration, simulator::FIXTURE_AY_BIAS, 1.0F};
    }
    else if (seconds >= 75.0F && seconds < 78.0F)
    {
      input.imu = {simulator::FIXTURE_AX_BIAS, simulator::FIXTURE_AY_BIAS - target.acceleration, 1.0F};
    }
    else if (seconds >= 95.0F && seconds < 96.0F)
    {
      input.imu = {simulator::FIXTURE_AX_BIAS - target.acceleration, simulator::FIXTURE_AY_BIAS, 1.0F};
    }
  }
  if (options().scenario == "circuit")
  {
    if (seconds >= 4.0F && seconds < 4.05F)
    {
      input.imu = {simulator::FIXTURE_AX_BIAS + target.acceleration, simulator::FIXTURE_AY_BIAS, 1.0F};
    }
    else if (seconds >= 18.0F && seconds < 18.2F)
    {
      const float diagonal = target.acceleration / std::sqrt(2.0F);
      input.imu = {simulator::FIXTURE_AX_BIAS + diagonal, simulator::FIXTURE_AY_BIAS - diagonal, 1.0F};
    }
    else if (seconds >= 22.0F && seconds < 24.0F)
    {
      input.imu[1] = simulator::FIXTURE_AY_BIAS - target.acceleration;
    }
    if (seconds >= 14.0F && seconds < 15.0F)
    {
      input.imu[2] = 3.0F;  // 縦軸だけの変化では水平Gを発生させない。
    }
  }
  if (options().scenario == "raw-faults")
  {
    if (seconds >= 4.0F && seconds < 6.0F)
    {
      input.adc[0] = 0;
    }
    if (seconds >= 8.0F && seconds < 10.0F)
    {
      input.adc[1] = 2047;
    }
    if (seconds >= 12.0F && seconds < 14.0F)
    {
      input.adc[2] = 1633;  // 4.9V以上となり、油圧の過電圧判定を通る。
    }
    if (seconds >= 16.0F && seconds < 18.0F)
    {
      input.adc = {0, 0, 0, 0};
    }
  }
  if (options().scenario == "fault-monitoring")
  {
    if (seconds >= 15.0F && seconds < 18.0F) input.adc[0] = input.adc[1] = 24;
    if (seconds >= 16.0F && seconds < 18.0F) input.adc[2] = 0;
  }
  if (options().scenario == "negative-maxima")
  {
    // 初回正常値の前後に異常を挟み、負の最高値と未取得を区別する。
    if (seconds < 2.0F) input.adc[0] = input.adc[1] = 0;
    if (seconds >= 12.0F && seconds < 15.0F)
    {
      input.adc[0] = 24;
      input.adc[1] = 2047;
    }
  }
  return input;
}
// NOLINTEND(readability-magic-numbers)

void inject(const Inputs& input)
{
  simulator::adcWaitMs = 0;
  simulator::adcFault = (options().scenario == "adc-communication" || options().scenario == "adc-startup")
                            ? simulator::communicationFault(simulator::nowMs, options().scenario == "adc-startup")
                            : simulator::AdcFault::Healthy;
  // 温度の500ms取得間隔より短い切断でも、全チャンネルを直ちに無効化する。
  const uint32_t failureStart = options().scenario == "menu-monitoring" ? 17000 : 4000;
  if ((options().scenario == "menu-monitoring" || options().scenario == "fault-monitoring") &&
      simulator::nowMs >= failureStart && simulator::nowMs < failureStart + 200)
    simulator::adcFault = simulator::AdcFault::Missing;
  if (options().scenario == "risk-predictive" && simulator::nowMs >= 95000 && simulator::nowMs < 96000)
    simulator::adcFault = simulator::AdcFault::Missing;
  if (options().scenario == "risk-predictive" && simulator::nowMs >= 105000 && simulator::nowMs < 106000)
    simulator::adcFault = simulator::AdcFault::Busy;
  simulator::adcChannels = input.adc;
  simulator::acceleration = input.imu;
  simulator::ambientLux = input.lux;
  // 取得周期・起動時補正・リングバッファを含む本番処理をそのまま実行する。
  acquireSensorData();
}

void writeInputTrace(std::ostream& trace, const Inputs& input, const Targets& target, bool showingMenu, bool setupError)
{
  trace << simulator::nowMs;
  for (const auto count : input.adc)
  {
    trace << ',' << count;
  }
  for (const auto axis : input.imu)
  {
    trace << ',' << axis;
  }
  trace << ',' << static_cast<int>(showingMenu) << ',' << input.lux << ',' << target.oil << ',' << target.water << ','
        << target.pressure << ',' << target.acceleration << ',' << static_cast<int>(simulator::adcFault) << ','
        << setupError << ',' << static_cast<int>(reviewNavigation.screen()) << '\n';
}

void writeFrameManifest(std::ostream& manifest, unsigned int tick, const Inputs& input, const Targets& target,
                        bool showingMenu, bool setupError)
{
  manifest << R"({"time_ms":)" << tick * MS_PER_SECOND / TICKS_PER_SECOND << R"(,"raw":{"adc":[)";
  for (size_t channel = 0; channel < input.adc.size(); ++channel)
  {
    if (channel != 0)
    {
      manifest << ',';
    }
    manifest << input.adc[channel];
  }
  manifest << R"(],"imu":[)" << input.imu[0] << ',' << input.imu[1] << ',' << input.imu[2] << R"(],"als":)" << input.lux
           << R"(,"adc_fault":)" << static_cast<int>(simulator::adcFault) << '}';
  if (options().scenario != "adc-steps")
  {
    manifest << R"(,"target":{"oil_c":)" << target.oil << R"(,"water_c":)" << target.water << R"(,"pressure_bar":)"
             << target.pressure << R"(,"g":)" << target.acceleration << '}';
  }
  manifest << R"(,"screen":)" << static_cast<int>(reviewNavigation.screen());
  if (isTsukubaScenario())
  {
    const float seconds = static_cast<float>(tick) * defaultSeconds() / (options().seconds * TICKS_PER_SECOND);
    const auto lap = simulator::tsukubaLap(seconds, options().scenario == "tsukuba-low");
    manifest << R"(,"track":{"section":")" << lap.section << R"(","lap_s":)" << lap.lapSeconds << R"(,"synthetic":true})";
  }
  manifest << R"(,"setup_error":)" << setupError << R"(,"g":)" << currentGForce << R"(,"lux":)" << input.lux
           << R"(,"menu":)" << showingMenu << R"(,"g_direction":")" << currentGDirection << R"(","racing":)" << isRacingMode
           << R"(,"g_cardinal":)" << static_cast<int>(currentGCardinalDirection) << R"(,"sensor":)";
  writeSensorTelemetry(manifest);
  manifest << R"(,"actual":)";
  writeDisplayTelemetry(manifest);
  if (isRiskScenario())
  {
    manifest << R"(,"risk":)";
    writeRiskTelemetry(manifest);
  }
  manifest << '}';
}

void captureFrame(unsigned int frame)
{
  // 転送先のフレームバッファを読み、本番の pushSprite の結果を録画する。
  std::vector<lgfx::rgb888_t> pixels(static_cast<size_t>(LCD_WIDTH) * LCD_HEIGHT);
  display.readRect(0, 0, LCD_WIDTH, LCD_HEIGHT, pixels.data());
  // 部分転送の漏れを全画素で検出する。setupの直接描画だけは比較対象外。
  if (!(frame == 0 && adcInitializationFailed))
  {
    std::vector<lgfx::rgb888_t> expected(pixels.size());
    mainCanvas.readRect(0, 0, LCD_WIDTH, LCD_HEIGHT, expected.data());
    for (size_t index = 0; index < pixels.size(); ++index)
      if (pixels[index].r != expected[index].r || pixels[index].g != expected[index].g ||
          pixels[index].b != expected[index].b)
        throw std::runtime_error("Partial display transfer missed a pixel: " + std::to_string(index) + " frame " +
                                 std::to_string(frame));
  }
  std::ostringstream name;
  name << "frame-" << std::setw(4) << std::setfill('0') << frame << ".ppm";
  std::ofstream output(options().capture / name.str(), std::ios::binary);
  output.exceptions(std::ios::failbit | std::ios::badbit);
  output << "P6\n" << LCD_WIDTH << ' ' << LCD_HEIGHT << "\n255\n";
  constexpr size_t RGB_CHANNELS = 3;
  std::vector<char> rgb(pixels.size() * RGB_CHANNELS);
  for (size_t index = 0; index < pixels.size(); ++index)
  {
    rgb[index * RGB_CHANNELS] = static_cast<char>(pixels[index].r);
    rgb[(index * RGB_CHANNELS) + 1] = static_cast<char>(pixels[index].g);
    rgb[(index * RGB_CHANNELS) + 2] = static_cast<char>(pixels[index].b);
  }
  output.write(rgb.data(), static_cast<std::streamsize>(rgb.size()));
}

struct Controls
{
  unsigned int tick = 0;
  unsigned int pacingPhase = 0;
  bool paused = false;
  bool manualMenu = false;
  std::array<bool, 4> previousKeys{};
  std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now();

  void poll()
  {
    uint16_t touchX = 0;
    uint16_t touchY = 0;
    const std::array<bool, 4> keys{
        !lgfx::gpio_in(PAUSE_PIN),
        !lgfx::gpio_in(REPLAY_PIN),
        !lgfx::gpio_in(MENU_PIN),
        display.getTouch(&touchX, &touchY) != 0,
    };
    if (keys[0] && !previousKeys[0])
    {
      paused = !paused;
    }
    if (keys[1] && !previousKeys[1])
    {
      tick = 0;
      pacingPhase = 0;
      paused = false;
      deadline = std::chrono::steady_clock::now();
    }
    if ((keys[2] && !previousKeys[2]) || (keys[3] && !previousKeys[3]))
    {
      manualMenu = true;
      reviewNavigation.tap(keys[3] ? touchX : 100, keys[3] ? touchY : 100);
    }
    previousKeys = keys;
  }

  void waitForNextFrame()
  {
    // 端数を配分して24フレームを1秒とし、処理遅延後の追い付き連続実行を防ぐ。
    const auto frameUs =
        ((pacingPhase + 1) * US_PER_SECOND / TICKS_PER_SECOND) - (pacingPhase * US_PER_SECOND / TICKS_PER_SECOND);
    pacingPhase = (pacingPhase + 1) % TICKS_PER_SECOND;
    deadline = std::max(deadline + std::chrono::microseconds(frameUs), std::chrono::steady_clock::now());
    std::this_thread::sleep_until(deadline);
  }
};

void quit()
{
  SDL_Event event{};
  event.type = SDL_QUIT;
  SDL_PushEvent(&event);
}

void initializeDisplay()
{
  if (!display.init())
  {
    throw std::runtime_error("SDL display initialization failed");
  }
  // SDL の CoreS3 パネルは回転 0 が横向き。実機の回転設定とは異なる。
  display.setRotation(0);
  if (display.width() != LCD_WIDTH || display.height() != LCD_HEIGHT)
  {
    throw std::runtime_error("Unexpected display dimensions");
  }
  display.setBrightness(BACKLIGHT_DAY);
  mainCanvas.setColorDepth(DISPLAY_COLOR_DEPTH);
  if (mainCanvas.createSprite(LCD_WIDTH, LCD_HEIGHT) == nullptr)
  {
    throw std::runtime_error("Sprite allocation failed");
  }
  resetGaugeState();
  simulator::adcFault = options().scenario == "adc-startup" ? simulator::AdcFault::Missing : simulator::AdcFault::Healthy;
  adcInitializationFailed = !initializeAdc();
  if (adcInitializationFailed)
  {
    markAdcInitializationFailure();
    drawAdcInitializationError(display);
  }
  updateBacklightLevel();
}

auto fixtureScreen(bool menu) -> ReviewScreen
{
  if (!isRiskScenario()) return menu ? ReviewScreen::Details : ReviewScreen::Gauges;
  const uint32_t now = simulator::nowMs;
  if (now >= 3000 && now < 5000) return ReviewScreen::Graph;
  if (isTsukubaScenario())
  {
    if ((now >= 145000 && now < 170000) || now >= 174000) return ReviewScreen::Graph;
    if (now >= 170000 && now < 172000) return ReviewScreen::Details;
    return ReviewScreen::Gauges;
  }
  if (options().scenario == "risk-window")
  {
    if ((now >= 76000 && now < 82000) || (now >= 86000 && now < 90000) || (now >= 145000 && now < 149000) || now >= 153000)
      return ReviewScreen::Graph;
    if ((now >= 82000 && now < 84000) || (now >= 149000 && now < 151000)) return ReviewScreen::Details;
  }
  else
  {
    if ((now >= 66000 && now < 68000) || (now >= 76000 && now < 79000) || (now >= 96000 && now < 98000) ||
        (now >= 145000 && now < 152000) || now >= 154000)
      return ReviewScreen::Graph;
    if (now >= 79000 && now < 81000) return ReviewScreen::Details;
    if (now >= 152000 && now < 154000) return ReviewScreen::Details;
  }
  return ReviewScreen::Gauges;
}

void navigateTo(ReviewScreen desired)
{
  if (reviewNavigation.screen() == desired) return;
  if (reviewNavigation.screen() == ReviewScreen::Graph && desired == ReviewScreen::Details)
  {
    reviewNavigation.tap(280, 10);
    return;
  }
  if (reviewNavigation.reviewing()) reviewNavigation.tap(100, 100);
  if (desired != ReviewScreen::Gauges) reviewNavigation.tap(100, 100);
  if (desired == ReviewScreen::Details) reviewNavigation.tap(280, 10);
}

auto renderFrame(const Inputs& input, bool manualMenu, bool showingMenu) -> bool
{
  inject(input);
  static unsigned long lastAlsMeasurementTime = 0;
  static ReviewScreen previousScreen = ReviewScreen::Gauges;
  if (!showingMenu && !isRacingMode && simulator::nowMs - lastAlsMeasurementTime >= ALS_MEASUREMENT_INTERVAL_MS)
  {
    updateBacklightLevel();
    lastAlsMeasurementTime = simulator::nowMs;
  }
  if (!manualMenu) navigateTo(fixtureScreen(input.menu));
  const auto screen = reviewNavigation.screen();
  const bool menu = reviewNavigation.reviewing();
  const bool changed = screen != previousScreen;
  if (!showingMenu && menu)
  {
    forceStopRacingMode();
    applyBrightnessMode(BrightnessMode::Day);
  }
  if (showingMenu && !menu)
  {
    resetLowWarningHistory();
    resetGaugeState();
    updateBacklightLevel();
  }
  updateGaugeMonitoring();
  if (!menu) updateRacingMode(simulator::nowMs, currentGForce);
  if (screen == ReviewScreen::Graph)
    refreshRiskGraph(changed);
  else if (screen == ReviewScreen::Details)
    refreshMenuScreen(changed);
  else
    updateGauges();
  previousScreen = screen;
  return menu;
}

auto finishRecording(std::ofstream& manifest) -> int
{
  manifest << "\n]";
  if (isRiskScenario())
  {
    manifest << R"(,"risk_window":)";
    writeRiskWindow(manifest);
  }
  manifest << "}\n";
  manifest.close();
  exitStatus = 0;
  quit();
  return 0;
}

void prepareOfflineOutput()
{
  if (!options().capture.empty() && !std::filesystem::create_directory(options().capture))
  {
    throw std::runtime_error("Capture directory must not already exist");
  }
}

// SDL コールバックのシグネチャに合わせる。
auto run(bool* running) -> int  // NOLINT(readability-non-const-parameter)
{
  try
  {
    simulator::ambientLux = sample(0.0F).lux;
    initializeDisplay();

    std::ofstream manifest;
    std::ofstream trace;
    const bool recording = !options().capture.empty();
    if (recording)
    {
      manifest.open(options().capture / "inputs.json");
      manifest.exceptions(std::ios::failbit | std::ios::badbit);
      manifest << std::boolalpha << std::setprecision(std::numeric_limits<float>::max_digits10);
      trace.open(options().capture / "trace.csv");
      trace.exceptions(std::ios::failbit | std::ios::badbit);
      trace << std::setprecision(std::numeric_limits<float>::max_digits10);
      trace << "time_ms,adc0,adc1,adc2,adc3,ax,ay,az,menu,lux,target_oil_c,target_water_c,target_pressure_bar,target_g,adc_"
               "fault,setup_error,screen\n";
      manifest << R"({"schema_version":2,"scenario":")" << options().scenario << R"(","fps":)" << TICKS_PER_SECOND
               << R"(,"sensors":{"oil_temp":)" << SENSOR_OIL_TEMP_PRESENT << R"(,"water_temp":)"
               << SENSOR_WATER_TEMP_PRESENT << R"(,"pressure":)" << SENSOR_OIL_PRESSURE_PRESENT << R"(,"ambient":)"
               << SENSOR_AMBIENT_LIGHT_PRESENT << R"(},"settings":{"pressure_alpha":)" << OIL_PRESSURE_SMOOTHING_ALPHA
               << R"(,"als_interval_ms":)" << ALS_MEASUREMENT_INTERVAL_MS << R"(,"median_size":)" << MEDIAN_BUFFER_SIZE
               << R"(,"lux_day":)" << static_cast<int>(LUX_THRESHOLD_DAY) << R"(,"lux_dusk":)"
               << static_cast<int>(LUX_THRESHOLD_DUSK) << R"(,"brightness":[)" << static_cast<int>(BACKLIGHT_DAY) << ','
               << static_cast<int>(BACKLIGHT_DUSK) << ',' << static_cast<int>(BACKLIGHT_NIGHT) << R"(],"racing_hold_ms":)"
               << RACING_MODE_START_HOLD_MS << R"(,"racing_duration_ms":)" << RACING_MODE_DURATION_MS << R"(,"racing_g":)"
               << RACING_MODE_START_THRESHOLD_G << R"(},"sensor_settings":)";
      writeSensorSettings(manifest);
      manifest << R"(,"startup_error_title":")" << ADC_INIT_ERROR_TITLE << R"(","startup_error_hint":")"
               << ADC_INIT_ERROR_HINT << R"(","frames":[)" << '\n';
    }
    std::cout << "Scenario: " << options().scenario << " | 24FPS | Space: pause | Home: replay | M/click: menu\n";
    const auto totalTicks = options().seconds * TICKS_PER_SECOND;
    Controls controls;
    bool showingMenu = false;
    while (*running)
    {
      if (!recording)
      {
        controls.poll();
      }
      const auto tick = controls.tick;
      const double progress = static_cast<double>(tick) / static_cast<double>(totalTicks);
      const auto target = sample(progress);
      const auto input = makeInputs(tick, target, static_cast<float>(progress * defaultSeconds()));
      // 再生を戻しても時計は単調増加させ、警告の継続時間やピーク値を壊さない。
      const bool setupError = tick == 0 && simulator::nowMs == 0 && adcInitializationFailed;
      if (!controls.paused && !setupError)
      {
        showingMenu = renderFrame(input, controls.manualMenu, showingMenu);
      }
      if (recording)
      {
        writeInputTrace(trace, input, target, showingMenu, setupError);
      }

      if (recording)
      {
        captureFrame(tick);
        if (tick != 0)
        {
          manifest << ",\n";
        }
        writeFrameManifest(manifest, tick, input, target, showingMenu, setupError);
      }
      if (recording && tick == totalTicks)
      {
        return finishRecording(manifest);
      }
      if (!controls.paused && tick < totalTicks)
      {
        ++controls.tick;
        simulator::nowMs += (controls.tick * MS_PER_SECOND / TICKS_PER_SECOND) - (tick * MS_PER_SECOND / TICKS_PER_SECOND);
      }
      if (!recording)
      {
        controls.waitForNextFrame();
      }
    }
    if (!recording)
    {
      exitStatus = 0;
    }
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    quit();
  }
  return exitStatus.load();
}
}  // namespace

auto main(int argc, char** argv) -> int
{
  try
  {
    for (int i = 1; i < argc; ++i)
    {
      const std::string argument = argv[i];
      if (argument == "--help")
      {
        std::cout << "Usage: program [--scenario "
                     "ramp|cooldown|pressure-sweep|warnings|circuit|racing|ambient|raw-faults|adc-communication|adc-"
                     "startup|adc-steps|sensor-disconnect|"
                     "temperature-high|temperature-low|warning-history|warning-repeat|warning-pending|warning-brief|"
                     "startup|warm-start|menu|tsukuba-lap|tsukuba-low] "
                     "[--seconds N] [--capture "
                     "NEW_DIRECTORY]\n";
        return 0;
      }
      if (i + 1 >= argc)
      {
        throw std::runtime_error("Missing option value");
      }
      const std::string value = argv[++i];
      if (argument == "--scenario")
      {
        options().scenario = value;
      }
      else if (argument == "--capture")
      {
        options().capture = value;
      }
      else if (argument == "--seconds" && value.find_first_not_of("0123456789") == std::string::npos)
      {
        const auto seconds = std::stoul(value);
        if (seconds < MIN_SECONDS || seconds > MAX_SECONDS)
        {
          throw std::runtime_error("Duration must be 5..240 seconds");
        }
        options().seconds = static_cast<unsigned int>(seconds);
      }
      else
      {
        throw std::runtime_error("Unknown option or invalid duration: " + argument);
      }
    }
    if (options().scenario != "ramp" && options().scenario != "warnings" && options().scenario != "startup" &&
        options().scenario != "warm-start" && options().scenario != "menu" && options().scenario != "cooldown" &&
        options().scenario != "pressure-sweep" && options().scenario != "circuit" && options().scenario != "racing" &&
        options().scenario != "ambient" && options().scenario != "raw-faults" &&
        options().scenario != "adc-communication" && options().scenario != "adc-startup" &&
        options().scenario != "adc-steps" && !simulator::isFaultScenario(options().scenario) && !isWarningReview() &&
        options().scenario != "warning-history" && options().scenario != "menu-monitoring" &&
        options().scenario != "fault-monitoring" && options().scenario != "negative-maxima" && !isRiskScenario())
    {
      throw std::runtime_error("Unknown scenario: " + options().scenario);
    }
    if (options().seconds == 0)
    {
      options().seconds = defaultSeconds();
    }
    if (options().seconds < MIN_SECONDS || options().seconds > MAX_SECONDS)
    {
      throw std::runtime_error("Duration must be 5..240 seconds");
    }
    prepareOfflineOutput();
    lgfx::Panel_sdl::addKeyCodeMapping(SDLK_SPACE, PAUSE_PIN);
    lgfx::Panel_sdl::addKeyCodeMapping(SDLK_HOME, REPLAY_PIN);
    lgfx::Panel_sdl::addKeyCodeMapping(SDLK_m, MENU_PIN);
    const int result = lgfx::Panel_sdl::main(run, SDL_STEP_MS);
    return result == 0 ? exitStatus.load() : result;
  }
  catch (const std::exception& error)
  {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
