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

#include "benchmark.hpp"
#include "compat.hpp"
#include "modules/backlight.h"
#include "modules/display.h"
#include "modules/racing_indicator.h"
#include "modules/racing_mode.h"
#include "modules/sensor.h"
#include "sensor_inputs.hpp"
#include "telemetry.hpp"

int currentFps = 0;

namespace simulator
{
uint32_t nowMs = 0;
}

namespace
{
constexpr unsigned int TICKS_PER_SECOND = 60;
constexpr unsigned int CAPTURE_STRIDE = 6;
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
  std::filesystem::path benchmark;
  unsigned int seconds = 0;
};

auto options() -> Options&
{
  static Options result;
  return result;
}
std::atomic<int> exitStatus{1};

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

// シナリオの物理量と時点を、README の表と対応するリテラルで記載する。
// NOLINTBEGIN(readability-magic-numbers)
auto defaultSeconds() -> unsigned int
{
  if (options().scenario == "ramp")
  {
    return 200;
  }
  if (options().scenario == "warnings")
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

auto sample(float progress) -> Targets
{
  // 時間指定を変えても同じ順序のシナリオを伸縮する。
  const float seconds = progress * static_cast<float>(defaultSeconds());
  const float pressure = runningPressure(seconds);
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
    // 始動直後だけ両温度が 70℃を示し、低温に戻ってからゆっくり暖まる。
    const bool initialReading = seconds < 1.0F;
    const float warming = (seconds - 1.0F) / 29.0F;
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

auto makeInputs(const Targets& target, float seconds) -> Inputs
{
  auto input = simulator::sensorInputs(target);
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
  return input;
}
// NOLINTEND(readability-magic-numbers)

void inject(const Inputs& input)
{
  simulator::adcChannels = input.adc;
  simulator::acceleration = input.imu;
  simulator::ambientLux = input.lux;
  // 取得周期・起動時補正・リングバッファを含む本番処理をそのまま実行する。
  acquireSensorData();
}

void writeInputTrace(std::ostream& trace, const Inputs& input, const Targets& target, bool showingMenu)
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
        << target.pressure << ',' << target.acceleration << '\n';
}

void writeFrameManifest(std::ostream& manifest, unsigned int tick, const Inputs& input, const Targets& target,
                        bool showingMenu)
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
           << R"(},"target":{"oil_c":)" << target.oil << R"(,"water_c":)" << target.water << R"(,"pressure_bar":)"
           << target.pressure << R"(,"g":)" << target.acceleration << R"(},"g":)" << currentGForce << R"(,"lux":)"
           << input.lux << R"(,"menu":)" << showingMenu << R"(,"g_direction":")" << currentGDirection << R"(","racing":)"
           << isRacingMode << R"(,"sensor":)";
  writeSensorTelemetry(manifest);
  manifest << R"(,"actual":)";
  writeDisplayTelemetry(manifest);
  manifest << '}';
}

void captureFrame(unsigned int frame)
{
  // 転送先のフレームバッファを読み、本番の pushSprite の結果を録画する。
  std::vector<lgfx::rgb888_t> pixels(static_cast<size_t>(LCD_WIDTH) * LCD_HEIGHT);
  display.readRect(0, 0, LCD_WIDTH, LCD_HEIGHT, pixels.data());
  std::ostringstream name;
  name << "frame-" << std::setw(4) << std::setfill('0') << frame << ".ppm";
  std::ofstream output(options().capture / name.str(), std::ios::binary);
  output.exceptions(std::ios::failbit | std::ios::badbit);
  output << "P6\n" << LCD_WIDTH << ' ' << LCD_HEIGHT << "\n255\n";
  for (const auto& pixel : pixels)
  {
    const std::array<char, 3> rgb{static_cast<char>(pixel.r), static_cast<char>(pixel.g), static_cast<char>(pixel.b)};
    output.write(rgb.data(), static_cast<std::streamsize>(rgb.size()));
  }
}

struct Controls
{
  unsigned int tick = 0;
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
      paused = false;
      deadline = std::chrono::steady_clock::now();
    }
    if ((keys[2] && !previousKeys[2]) || (keys[3] && !previousKeys[3]))
    {
      manualMenu = !manualMenu;
    }
    previousKeys = keys;
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
  updateBacklightLevel();
}

auto renderFrame(const Inputs& input, bool manualMenu, bool showingMenu) -> bool
{
  inject(input);
  static unsigned long lastAlsMeasurementTime = 0;
  // 実機loopと同じ順序で測定し、R・詳細画面の間は自動調光を止める。
  if (!showingMenu && !isRacingMode && simulator::nowMs - lastAlsMeasurementTime >= ALS_MEASUREMENT_INTERVAL_MS)
  {
    updateBacklightLevel();
    lastAlsMeasurementTime = simulator::nowMs;
  }
  const bool menu = input.menu || manualMenu;
  if (!showingMenu && menu)
  {
    // 実機の詳細画面への遷移と同じく、Rモードを解除する。
    forceStopRacingMode();
    applyBrightnessMode(BrightnessMode::Day);
  }
  if (showingMenu && !menu)
  {
    resetGaugeState();
    updateBacklightLevel();
  }
  updateRacingMode(simulator::nowMs, currentGForce);
  if (menu)
  {
    drawMenuScreen();
  }
  else
  {
    updateGauges();
  }
  return menu;
}

auto processFrame(FrameBenchmark& benchmark, unsigned int tick, const Inputs& input, bool manualMenu, bool showingMenu)
    -> bool
{
  if (options().benchmark.empty())
  {
    return renderFrame(input, manualMenu, showingMenu);
  }
  benchmark.recordInput(simulator::nowMs, input);
  return benchmark.measure(tick, [&]() -> bool { return renderFrame(input, manualMenu, showingMenu); });
}

auto finishOfflineRun(std::ofstream& manifest, const FrameBenchmark& benchmark) -> int
{
  if (!options().capture.empty())
  {
    manifest << "\n]}\n";
    manifest.close();
  }
  if (!options().benchmark.empty())
  {
    benchmark.write(options().benchmark, options().scenario);
  }
  exitStatus = 0;
  quit();
  return 0;
}

void prepareOfflineOutput()
{
  if (!options().capture.empty() && !options().benchmark.empty())
  {
    throw std::runtime_error("Capture and benchmark must run separately");
  }
  if (!options().benchmark.empty() && std::filesystem::exists(options().benchmark))
  {
    throw std::runtime_error("Benchmark output file must not already exist");
  }
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
    const bool benchmarking = !options().benchmark.empty();
    const bool offline = recording || benchmarking;
    FrameBenchmark benchmark;
    if (recording)
    {
      manifest.open(options().capture / "inputs.json");
      manifest.exceptions(std::ios::failbit | std::ios::badbit);
      manifest << std::boolalpha << std::setprecision(std::numeric_limits<float>::max_digits10);
      trace.open(options().capture / "trace.csv");
      trace.exceptions(std::ios::failbit | std::ios::badbit);
      trace << std::setprecision(std::numeric_limits<float>::max_digits10);
      trace << "time_ms,adc0,adc1,adc2,adc3,ax,ay,az,menu,lux,target_oil_c,target_water_c,target_pressure_bar,target_g\n";
      manifest << R"({"schema_version":2,"scenario":")" << options().scenario << R"(","fps":10,"sensors":{"oil_temp":)"
               << SENSOR_OIL_TEMP_PRESENT << R"(,"water_temp":)" << SENSOR_WATER_TEMP_PRESENT << R"(,"pressure":)"
               << SENSOR_OIL_PRESSURE_PRESENT << R"(,"ambient":)" << SENSOR_AMBIENT_LIGHT_PRESENT
               << R"(},"settings":{"pressure_alpha":)" << OIL_PRESSURE_SMOOTHING_ALPHA << R"(,"als_interval_ms":)"
               << ALS_MEASUREMENT_INTERVAL_MS << R"(,"median_size":)" << MEDIAN_BUFFER_SIZE << R"(,"lux_day":)"
               << static_cast<int>(LUX_THRESHOLD_DAY) << R"(,"lux_dusk":)" << static_cast<int>(LUX_THRESHOLD_DUSK)
               << R"(,"brightness":[)" << static_cast<int>(BACKLIGHT_DAY) << ',' << static_cast<int>(BACKLIGHT_DUSK) << ','
               << static_cast<int>(BACKLIGHT_NIGHT) << R"(],"racing_hold_ms":)" << RACING_MODE_START_HOLD_MS
               << R"(,"racing_duration_ms":)" << RACING_MODE_DURATION_MS << R"(,"racing_g":)"
               << RACING_MODE_START_THRESHOLD_G << R"(},"sensor_settings":)";
      writeSensorSettings(manifest);
      manifest << R"(,"frames":[)" << '\n';
    }
    std::cout << "Scenario: " << options().scenario << " | Space: pause | Home: replay | M/click: menu\n";
    const auto totalTicks = options().seconds * TICKS_PER_SECOND;
    Controls controls;
    bool showingMenu = false;
    while (*running)
    {
      if (!offline)
      {
        controls.poll();
      }
      const auto tick = controls.tick;
      const float progress = static_cast<float>(tick) / static_cast<float>(totalTicks);
      const auto target = sample(progress);
      const auto input = makeInputs(target, progress * static_cast<float>(defaultSeconds()));
      // 再生を戻しても時計は単調増加させ、警告の継続時間やピーク値を壊さない。
      if (!controls.paused)
      {
        showingMenu = processFrame(benchmark, tick, input, controls.manualMenu, showingMenu);
      }
      if (recording)
      {
        writeInputTrace(trace, input, target, showingMenu);
      }

      if (recording && tick % CAPTURE_STRIDE == 0)
      {
        const auto frame = tick / CAPTURE_STRIDE;
        captureFrame(frame);
        if (frame != 0)
        {
          manifest << ",\n";
        }
        writeFrameManifest(manifest, tick, input, target, showingMenu);
      }
      if (offline && tick == totalTicks)
      {
        return finishOfflineRun(manifest, benchmark);
      }
      if (!controls.paused && tick < totalTicks)
      {
        ++controls.tick;
        simulator::nowMs += (controls.tick * MS_PER_SECOND / TICKS_PER_SECOND) - (tick * MS_PER_SECOND / TICKS_PER_SECOND);
      }
      if (!offline)
      {
        controls.deadline += std::chrono::microseconds(US_PER_SECOND / TICKS_PER_SECOND);
        std::this_thread::sleep_until(controls.deadline);
      }
    }
    if (!offline)
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
                     "ramp|cooldown|pressure-sweep|warnings|circuit|racing|ambient|raw-faults|startup|warm-start|menu] "
                     "[--seconds N] [--benchmark NEW_FILE] [--capture "
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
      else if (argument == "--benchmark")
      {
        options().benchmark = value;
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
        options().scenario != "ambient" && options().scenario != "raw-faults")
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
