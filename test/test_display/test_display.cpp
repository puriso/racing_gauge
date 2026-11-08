#include <unity.h>

#include <algorithm>
#include <limits>

#include "../../src/modules/display.cpp"

// 描画処理へ渡すセンサー値と、今回検証しない周辺機能のスタブ
float oilPressureSamples[PRESSURE_SAMPLE_SIZE] = {};
float waterTemperatureSamples[WATER_TEMP_SAMPLE_SIZE] = {};
float oilTemperatureSamples[OIL_TEMP_SAMPLE_SIZE] = {};
bool oilPressureOverVoltage = false;
float currentGForce = 0.0F;
const char* currentGDirection = "Right";
int currentFps = 0;
int latestLux = 0;
int medianLuxValue = 0;
float lastLowEventG = 0.0F;
const char* lastLowEventDir = "Right";
float lastLowEventDuration = 0.0F;
float lastLowEventPressure = 0.0F;

bool drawLowPressureWarning(M5Canvas&, float, float, bool& changed)
{
  changed = false;
  return false;
}
bool drawRacingIndicator(M5Canvas&) { return false; }

static void setTemperatures(float temperature)
{
  std::fill_n(waterTemperatureSamples, WATER_TEMP_SAMPLE_SIZE, temperature);
  std::fill_n(oilTemperatureSamples, OIL_TEMP_SAMPLE_SIZE, temperature);
}

void setUp()
{
  // 各テストを異常状態から始め、平滑化の履歴を残さない
  setTemperatures(200.0F);
  updateGauges();
  recordedMaxWaterTemp = 0.0F;
  recordedMaxOilTempTop = 0;
  resetGaugeState();
  mainCanvas.rectangles.clear();
  mainCanvas.pushCount = 0;
}

void tearDown() {}

void test_fault_sample_does_not_become_a_temperature_peak()
{
  recordedMaxWaterTemp = 90.0F;
  recordedMaxOilTempTop = 90;
  waterTemperatureSamples[0] = 100.0F;
  oilTemperatureSamples[0] = 100.0F;
  updateGauges();

  TEST_ASSERT_EQUAL_FLOAT(0.0F, displayCache.waterTempAvg);
  TEST_ASSERT_EQUAL_FLOAT(0.0F, displayCache.oilTemp);
  TEST_ASSERT_EQUAL_FLOAT(90.0F, recordedMaxWaterTemp);
  TEST_ASSERT_EQUAL(90, recordedMaxOilTempTop);
}

void test_fault_preserves_recorded_maxima()
{
  recordedMaxWaterTemp = 115.0F;
  recordedMaxOilTempTop = 125;
  setTemperatures(200.0F);
  updateGauges();

  TEST_ASSERT_EQUAL_FLOAT(115.0F, recordedMaxWaterTemp);
  TEST_ASSERT_EQUAL(125, recordedMaxOilTempTop);
}

void test_fault_clears_live_reading_immediately()
{
  setTemperatures(100.0F);
  updateGauges();
  setTemperatures(200.0F);
  updateGauges();

  TEST_ASSERT_EQUAL_FLOAT(0.0F, displayCache.waterTempAvg);
  TEST_ASSERT_EQUAL_FLOAT(0.0F, displayCache.oilTemp);
}

void test_recovery_does_not_smooth_from_a_fault_value()
{
  setTemperatures(100.0F);
  updateGauges();
  setTemperatures(200.0F);
  updateGauges();
  setTemperatures(90.0F);
  updateGauges();

  TEST_ASSERT_EQUAL_FLOAT(90.0F, displayCache.waterTempAvg);
  TEST_ASSERT_EQUAL_FLOAT(90.0F, displayCache.oilTemp);
}

void test_oil_bar_stays_in_bounds_above_scale()
{
  drawOilTemperatureTopBar(mainCanvas, 150.0F, 150);

  TEST_ASSERT_EQUAL(2, mainCanvas.rectangles.size());
  TEST_ASSERT_EQUAL(20, mainCanvas.rectangles.back().x);
  TEST_ASSERT_EQUAL(210, mainCanvas.rectangles.back().width);
  TEST_ASSERT_EQUAL(COLOR_RED, mainCanvas.rectangles.back().color);
  TEST_ASSERT_EQUAL_STRING("150", mainCanvas.lastRightString.c_str());
}

void test_nonfinite_samples_are_not_recorded()
{
  const float invalidValues[] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                                 -std::numeric_limits<float>::infinity()};
  recordedMaxWaterTemp = 105.0F;
  recordedMaxOilTempTop = 115;
  for (float invalid : invalidValues)
  {
    setTemperatures(100.0F);
    waterTemperatureSamples[0] = invalid;
    oilTemperatureSamples[0] = invalid;
    updateGauges();
    TEST_ASSERT_EQUAL_FLOAT(0.0F, displayCache.waterTempAvg);
    TEST_ASSERT_EQUAL_FLOAT(0.0F, displayCache.oilTemp);
    TEST_ASSERT_EQUAL_FLOAT(105.0F, recordedMaxWaterTemp);
    TEST_ASSERT_EQUAL(115, recordedMaxOilTempTop);
  }
}

void test_healthy_samples_are_averaged_and_display_is_smoothed()
{
  setTemperatures(100.0F);
  updateGauges();
  waterTemperatureSamples[0] = 120.0F;
  oilTemperatureSamples[0] = 120.0F;
  updateGauges();

  TEST_ASSERT_EQUAL_FLOAT(101.0F, displayCache.waterTempAvg);
  TEST_ASSERT_EQUAL_FLOAT(101.0F, displayCache.oilTemp);
  TEST_ASSERT_EQUAL_FLOAT(110.0F, recordedMaxWaterTemp);
  TEST_ASSERT_EQUAL(110, recordedMaxOilTempTop);
}

void test_menu_keeps_recording_without_drawing()
{
  setTemperatures(120.0F);
  updateGauges(false);

  TEST_ASSERT_EQUAL_FLOAT(120.0F, recordedMaxWaterTemp);
  TEST_ASSERT_EQUAL(120, recordedMaxOilTempTop);
  TEST_ASSERT_EQUAL(0, mainCanvas.pushCount);
  TEST_ASSERT_TRUE(mainCanvas.rectangles.empty());

  // メニュー中の異常でも記録を維持し、戻った画面へ反映する
  setTemperatures(200.0F);
  updateGauges(false);
  setTemperatures(100.0F);
  updateGauges();
  TEST_ASSERT_EQUAL_FLOAT(120.0F, recordedMaxWaterTemp);
  TEST_ASSERT_EQUAL(120, displayCache.maxOilTemp);
  TEST_ASSERT_EQUAL_FLOAT(100.0F, displayCache.waterTempAvg);
  TEST_ASSERT_EQUAL(1, mainCanvas.pushCount);
}

void test_temperature_channels_fail_independently()
{
  setTemperatures(100.0F);
  waterTemperatureSamples[0] = 200.0F;
  updateGauges();
  TEST_ASSERT_EQUAL_FLOAT(0.0F, displayCache.waterTempAvg);
  TEST_ASSERT_EQUAL_FLOAT(100.0F, displayCache.oilTemp);
  TEST_ASSERT_EQUAL(100, recordedMaxOilTempTop);

  setTemperatures(110.0F);
  oilTemperatureSamples[0] = 200.0F;
  updateGauges();
  TEST_ASSERT_EQUAL_FLOAT(110.0F, displayCache.waterTempAvg);
  TEST_ASSERT_EQUAL_FLOAT(0.0F, displayCache.oilTemp);
  TEST_ASSERT_EQUAL_FLOAT(110.0F, recordedMaxWaterTemp);
  TEST_ASSERT_EQUAL(100, recordedMaxOilTempTop);
}

void test_oil_bar_boundary_widths_and_colors()
{
  const float temperatures[] = {80.0F, 105.0F, 120.0F, 130.0F, 198.0F};
  const int widths[] = {0, 105, 168, 210, 210};
  for (size_t index = 0; index < 5; ++index)
  {
    mainCanvas.rectangles.clear();
    drawOilTemperatureTopBar(mainCanvas, temperatures[index], 198);
    TEST_ASSERT_EQUAL(widths[index], mainCanvas.rectangles.back().width);
    TEST_ASSERT_EQUAL(temperatures[index] >= 120.0F ? COLOR_RED : COLOR_WHITE, mainCanvas.rectangles.back().color);
  }
}

void test_invalid_oil_temperature_has_no_active_bar()
{
  const float invalidValues[] = {199.0F, 200.0F, std::numeric_limits<float>::quiet_NaN(),
                                 std::numeric_limits<float>::infinity()};
  for (float invalid : invalidValues)
  {
    mainCanvas.rectangles.clear();
    drawOilTemperatureTopBar(mainCanvas, invalid, 125);
    TEST_ASSERT_EQUAL(1, mainCanvas.rectangles.size());
    TEST_ASSERT_EQUAL_STRING("0", mainCanvas.lastRightString.c_str());
  }
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_fault_sample_does_not_become_a_temperature_peak);
  RUN_TEST(test_fault_preserves_recorded_maxima);
  RUN_TEST(test_fault_clears_live_reading_immediately);
  RUN_TEST(test_recovery_does_not_smooth_from_a_fault_value);
  RUN_TEST(test_oil_bar_stays_in_bounds_above_scale);
  RUN_TEST(test_nonfinite_samples_are_not_recorded);
  RUN_TEST(test_healthy_samples_are_averaged_and_display_is_smoothed);
  RUN_TEST(test_menu_keeps_recording_without_drawing);
  RUN_TEST(test_temperature_channels_fail_independently);
  RUN_TEST(test_oil_bar_boundary_widths_and_colors);
  RUN_TEST(test_invalid_oil_temperature_has_no_active_bar);
  return UNITY_END();
}
