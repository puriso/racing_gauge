#include <SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "compat.hpp"
#include "modules/backlight.h"
#include "modules/display.h"
#include "modules/racing_mode.h"

// 本番のセンサー境界と同じ配列型で接続する。
// NOLINTBEGIN(modernize-avoid-c-arrays)
float oilPressureSamples[PRESSURE_SAMPLE_SIZE] = {};
float waterTemperatureSamples[WATER_TEMP_SAMPLE_SIZE] = {};
float oilTemperatureSamples[OIL_TEMP_SAMPLE_SIZE] = {};
// NOLINTEND(modernize-avoid-c-arrays)
bool oilPressureOverVoltage = false;
float currentGForce = 0.0F;
const char* currentGDirection = "Right";
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
constexpr unsigned int MAX_SECONDS = 120;
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

struct Inputs
{
  float oil;
  float water;
  float pressure;
  float acceleration;
  bool menu;
};

// シナリオの物理量と時点を、README の表と対応するリテラルで記載する。
// NOLINTBEGIN(readability-magic-numbers)
auto defaultSeconds() -> unsigned int
{
  if (options().scenario == "ramp")
  {
    return 100;
  }
  if (options().scenario == "warnings")
  {
    return 40;
  }
  if (options().scenario == "startup" || options().scenario == "warm-start")
  {
    return 30;
  }
  return DEFAULT_SECONDS;
}

auto interpolate(float start, float end, float progress) -> float
{
  return start + ((end - start) * std::clamp(progress, 0.0F, 1.0F));
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
  return 5.0F;
}

auto sample(float progress) -> Inputs
{
  // 時間指定を変えても同じ順序のシナリオを伸縮する。
  const float seconds = progress * static_cast<float>(defaultSeconds());
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
        interpolate(0.0F, 5.0F, seconds),
        0.0F,
        false,
    };
  }
  if (options().scenario == "warm-start")
  {
    return {
        interpolate(70.0F, 95.0F, progress),
        interpolate(70.0F, 90.0F, progress),
        interpolate(0.0F, 5.0F, seconds),
        0.0F,
        false,
    };
  }
  if (options().scenario == "menu")
  {
    return {
        interpolate(70.0F, 90.0F, progress), interpolate(70.0F, 85.0F, progress), 5.0F, 0.0F,
        seconds >= 8.0F && seconds < 13.0F,
    };
  }
  // 両端を保持し、平滑化後の低温・高温も目視しやすくする。
  const float warming = (seconds - 2.0F) / 90.0F;
  return {
      interpolate(-10.0F, 170.0F, warming),
      interpolate(-10.0F, 130.0F, warming),
      interpolate(0.0F, 10.0F, (seconds - 2.0F) / 10.0F),
      0.0F,
      false,
  };
}

// NOLINTEND(readability-magic-numbers)

void inject(const Inputs& input)
{
  std::fill_n(oilTemperatureSamples, OIL_TEMP_SAMPLE_SIZE, SENSOR_OIL_TEMP_PRESENT ? input.oil : 0.0F);
  std::fill_n(waterTemperatureSamples, WATER_TEMP_SAMPLE_SIZE, SENSOR_WATER_TEMP_PRESENT ? input.water : 0.0F);
  std::fill_n(oilPressureSamples, PRESSURE_SAMPLE_SIZE, SENSOR_OIL_PRESSURE_PRESENT ? input.pressure : 0.0F);
  currentGForce = input.acceleration;
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
  updateRacingMode(simulator::nowMs, currentGForce);
  const bool menu = input.menu || manualMenu;
  if (showingMenu && !menu)
  {
    resetGaugeState();
  }
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

// SDL コールバックのシグネチャに合わせる。
auto run(bool* running) -> int  // NOLINT(readability-non-const-parameter)
{
  try
  {
    initializeDisplay();

    std::ofstream manifest;
    const bool recording = !options().capture.empty();
    if (recording)
    {
      manifest.open(options().capture / "inputs.json");
      manifest.exceptions(std::ios::failbit | std::ios::badbit);
      manifest << R"({"scenario":")" << options().scenario << R"(","fps":10,"sensors":{"oil_temp":)"
               << SENSOR_OIL_TEMP_PRESENT << R"(,"water_temp":)" << SENSOR_WATER_TEMP_PRESENT << R"(,"pressure":)"
               << SENSOR_OIL_PRESSURE_PRESENT << "},\"frames\":[\n";
    }
    std::cout << "Scenario: " << options().scenario << " | Space: pause | Home: replay | M/click: menu\n";
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
      const auto input = sample(static_cast<float>(tick) / static_cast<float>(totalTicks));
      // 再生を戻しても時計は単調増加させ、警告の継続時間やピーク値を壊さない。
      if (!controls.paused)
      {
        showingMenu = renderFrame(input, controls.manualMenu, showingMenu);
      }

      if (recording && tick % CAPTURE_STRIDE == 0)
      {
        const auto frame = tick / CAPTURE_STRIDE;
        captureFrame(frame);
        if (frame != 0)
        {
          manifest << ",\n";
        }
        manifest << "{\"time_ms\":" << tick * MS_PER_SECOND / TICKS_PER_SECOND << ",\"oil_c\":" << input.oil
                 << ",\"water_c\":" << input.water << ",\"pressure_bar\":" << input.pressure
                 << ",\"g\":" << input.acceleration << ",\"menu\":" << (showingMenu ? "true" : "false") << '}';
      }
      if (recording && tick == totalTicks)
      {
        manifest << "\n]}\n";
        manifest.close();
        exitStatus = 0;
        quit();
        return 0;
      }
      if (!controls.paused && tick < totalTicks)
      {
        ++controls.tick;
        simulator::nowMs += (controls.tick * MS_PER_SECOND / TICKS_PER_SECOND) - (tick * MS_PER_SECOND / TICKS_PER_SECOND);
      }
      if (!recording)
      {
        controls.deadline += std::chrono::microseconds(US_PER_SECOND / TICKS_PER_SECOND);
        std::this_thread::sleep_until(controls.deadline);
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
        std::cout << "Usage: program [--scenario ramp|warnings|startup|warm-start|menu] [--seconds N] [--capture "
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
          throw std::runtime_error("Duration must be 5..120 seconds");
        }
        options().seconds = static_cast<unsigned int>(seconds);
      }
      else
      {
        throw std::runtime_error("Unknown option or invalid duration: " + argument);
      }
    }
    if (options().scenario != "ramp" && options().scenario != "warnings" && options().scenario != "startup" &&
        options().scenario != "warm-start" && options().scenario != "menu")
    {
      throw std::runtime_error("Unknown scenario: " + options().scenario);
    }
    if (options().seconds == 0)
    {
      options().seconds = defaultSeconds();
    }
    if (options().seconds < MIN_SECONDS || options().seconds > MAX_SECONDS)
    {
      throw std::runtime_error("Duration must be 5..120 seconds");
    }
    if (!options().capture.empty() && !std::filesystem::create_directory(options().capture))
    {
      throw std::runtime_error("Capture directory must not already exist");
    }
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
