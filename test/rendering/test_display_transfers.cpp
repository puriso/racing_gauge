#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"

// 本番設定を書き換えず、FPS表示の有効・無効を両方検証する
#ifdef TEST_FPS_ENABLED
#undef FPS_DISPLAY_ENABLED
#define FPS_DISPLAY_ENABLED 1
#endif

#ifndef DISPLAY_IMPLEMENTATION
#define DISPLAY_IMPLEMENTATION "../../../src/modules/display.cpp"
#endif
#include DISPLAY_IMPLEMENTATION
#include "../../../src/modules/fps_display.cpp"
#include "../../../src/modules/low_warning.cpp"
#include "../../../src/modules/racing_indicator.cpp"

float oilPressureSamples[PRESSURE_SAMPLE_SIZE] = {};
float waterTemperatureSamples[WATER_TEMP_SAMPLE_SIZE] = {};
float oilTemperatureSamples[OIL_TEMP_SAMPLE_SIZE] = {};
bool oilPressureOverVoltage = false;
float currentGForce = 0.0F;
const char* currentGDirection = "Right";
int currentFps = 60;
int latestLux = 0;
int medianLuxValue = 0;

static void check(bool condition, const char* expression, int line)
{
  if (!condition)
  {
    std::fprintf(stderr, "Line %d: %s\n", line, expression);
    std::exit(1);
  }
}
#define CHECK(expression) check((expression), #expression, __LINE__)

static void clearTransfers()
{
  display.transfers.clear();
  display.transferredPixels = 0;
}

static void expectRegion(int x, int y, int width, int height)
{
  CHECK(display.transfers.size() == 1);
  const auto& region = display.transfers.front();
  CHECK(region.x == x && region.y == y && region.width == width && region.height == height);
  CHECK(display.pixels == mainCanvas.pixels);
  CHECK(display.clip.x == 0 && display.clip.y == 0 && display.clip.width == 320 && display.clip.height == 240);
}

static void startScreen()
{
  display.clearClipRect();
  currentGForce = 0.0F;
  isRacingMode = false;
  testTimeMs += 10000;
  renderDisplayAndLog(2.0F, 95.0F, 100.0F, 100);
  resetGaugeState();
  clearTransfers();
  renderDisplayAndLog(2.0F, 95.0F, 100.0F, 100);
  expectRegion(0, 0, 320, 240);
  clearTransfers();
}

static void test_single_gauge_updates()
{
  startScreen();
  renderDisplayAndLog(2.2F, 95.0F, 100.0F, 100);
  expectRegion(0, 60, 160, 180);
  clearTransfers();
  renderDisplayAndLog(2.2F, 95.2F, 100.0F, 100);
  expectRegion(160, 60, 160, 180);
  clearTransfers();
  renderDisplayAndLog(2.2F, 95.2F, 100.2F, 100);
  expectRegion(0, 0, 320, 50);
  clearTransfers();
  renderDisplayAndLog(2.2F, 95.2F, 100.2F, 101);
  expectRegion(0, 0, 320, 50);
}

static void test_combined_updates()
{
  startScreen();
  renderDisplayAndLog(2.2F, 96.0F, 100.0F, 100);
  expectRegion(0, 60, 320, 180);
  clearTransfers();
  renderDisplayAndLog(2.4F, 96.0F, 101.0F, 101);
  expectRegion(0, 0, 320, 240);
}

static void test_unchanged_values_do_not_transfer()
{
  startScreen();
  renderDisplayAndLog(2.0F, 95.0F, 100.0F, 100);
  CHECK(display.transfers.empty());
  renderDisplayAndLog(2.01F, 95.01F, 100.01F, 100);
  CHECK(display.transfers.empty());
  CHECK(display.pixels == mainCanvas.pixels);
}

static void test_warning_transitions()
{
  startScreen();
  currentGForce = 1.2F;
  renderDisplayAndLog(2.0F, 95.0F, 100.0F, 100);
  CHECK(display.transfers.empty());
  testTimeMs += 500;
  renderDisplayAndLog(2.0F, 95.0F, 100.0F, 100);
  expectRegion(0, 60, 160, 180);
  clearTransfers();
  renderDisplayAndLog(2.2F, 95.0F, 100.0F, 100);
  expectRegion(0, 60, 160, 180);
  currentGForce = 0.0F;
  clearTransfers();
  renderDisplayAndLog(2.2F, 95.0F, 100.0F, 100);
  CHECK(display.transfers.empty());
  testTimeMs += 3000;
  renderDisplayAndLog(2.2F, 95.0F, 100.0F, 100);
#if FPS_DISPLAY_ENABLED
  expectRegion(0, 60, 320, 180);
#else
  expectRegion(0, 60, 160, 180);
#endif
}

static void test_racing_indicator_transitions()
{
  startScreen();
  isRacingMode = true;
  renderDisplayAndLog(2.0F, 95.0F, 100.0F, 100);
  expectRegion(0, 224, 320, 16);
  clearTransfers();
  isRacingMode = false;
  renderDisplayAndLog(2.0F, 95.0F, 100.0F, 100);
  expectRegion(0, 224, 320, 16);
}

static void test_fps_overlay()
{
  startScreen();
  testTimeMs += 1000;
  currentFps = 59;
  renderDisplayAndLog(2.0F, 95.0F, 100.0F, 100);
#if FPS_DISPLAY_ENABLED
  expectRegion(0, 224, 320, 16);
#else
  CHECK(display.transfers.empty());
#endif
}

static void test_menu_return_redraws_full_screen()
{
  startScreen();
  renderDisplayAndLog(2.2F, 95.0F, 100.0F, 100);
  clearTransfers();
  drawMenuScreen();
  expectRegion(0, 0, 320, 240);
  clearTransfers();
  resetGaugeState();
  expectRegion(0, 0, 320, 240);
  clearTransfers();
  renderDisplayAndLog(2.2F, 95.0F, 100.0F, 100);
  expectRegion(0, 0, 320, 240);
}

static void test_existing_clip_is_preserved()
{
  startScreen();
  display.setClipRect(100, 100, 100, 100);
  renderDisplayAndLog(2.2F, 95.0F, 100.0F, 100);
  CHECK(display.transfers.size() == 1);
  const auto& region = display.transfers.front();
  CHECK(region.x == 100 && region.y == 100 && region.width == 60 && region.height == 100);
  CHECK(display.clip.x == 100 && display.clip.y == 100 && display.clip.width == 100 && display.clip.height == 100);
  clearTransfers();
  display.setClipRect(160, 0, 160, 50);
  renderDisplayAndLog(2.4F, 95.0F, 100.0F, 100);
  CHECK(display.transfers.empty());
  CHECK(display.clip.x == 160 && display.clip.y == 0 && display.clip.width == 160 && display.clip.height == 50);
  display.clearClipRect();
}

static void measure_transfer_volume()
{
  startScreen();
  constexpr int FRAMES = 600;
  unsigned long startMs = testTimeMs;
  for (int frame = 0; frame < FRAMES; ++frame)
  {
    // 60FPSで10秒間の更新を模擬し、FPS表示の毎秒更新も転送量に含める
    testTimeMs = startMs + static_cast<unsigned long>(frame + 1) * 1000UL / 60UL;
    float pressure = frame % 2 == 0 ? 2.2F : 2.0F;
    float temperatureStep = static_cast<float>((frame / 30) % 2);
    renderDisplayAndLog(pressure, 95.0F + temperatureStep, 100.0F + temperatureStep, 101);
    CHECK(display.pixels == mainCanvas.pixels);
  }
  CHECK(display.transfers.size() == FRAMES);
  std::printf("600 frames, FPS overlay %s: %llu pixels (%llu bytes at RGB565)\n", FPS_DISPLAY_ENABLED ? "on" : "off",
              static_cast<unsigned long long>(display.transferredPixels),
              static_cast<unsigned long long>(display.transferredPixels * 2));
}

int main(int argc, char** argv)
{
  if (argc > 1 && std::strcmp(argv[1], "--measure") == 0)
  {
    measure_transfer_volume();
    return 0;
  }
  test_single_gauge_updates();
  test_combined_updates();
  test_unchanged_values_do_not_transfer();
  test_warning_transitions();
  test_racing_indicator_transitions();
  test_fps_overlay();
  test_menu_return_redraws_full_screen();
  test_existing_clip_is_preserved();
  std::puts("8 rendering scenarios passed");
  measure_transfer_volume();
  return 0;
}
