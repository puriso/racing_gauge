#include "compat.hpp"
#include "telemetry.hpp"

// 実機と同じ描画・平滑化・警告処理をリンクする。時刻アダプターはこの翻訳単位に限定する。
// NOLINTBEGIN(bugprone-suspicious-include)
#include "../src/modules/backlight.cpp"
#include "../src/modules/display.cpp"
#include "../src/modules/low_warning.cpp"
#include "../src/modules/racing_indicator.cpp"
#include "../src/modules/racing_mode.cpp"
#include "../src/modules/sensor.cpp"
// NOLINTEND(bugprone-suspicious-include)

// FPSの計測・表示はシミュレーターへ持ち込まない。
static_assert(FPS_DISPLAY_ENABLED == 0, "FPS measurement overlays are unavailable in the simulator");

void writeDisplayTelemetry(std::ostream& output)
{
  output << R"({"oil_c":)" << displayCache.oilTemp << R"(,"water_c":)" << displayCache.waterTempAvg << R"(,"pressure_bar":)"
         << displayCache.pressureAvg << R"(,"brightness_mode":)" << static_cast<int>(currentBrightnessMode)
         << R"(,"brightness_pwm":)" << static_cast<int>(display.getBrightness()) << R"(,"lux_sample":)" << latestLux
         << R"(,"lux_median":)" << medianLuxValue << R"(,"warning_history":)" << hasLowWarningHistory()
         << R"(,"low_event":{"g":)" << lastLowEventG << R"(,"pressure_bar":)" << lastLowEventPressure << R"(,"duration_s":)"
         << lastLowEventDuration << R"(,"direction":")" << lastLowEventDir << R"("}})";
}

void writeSensorSettings(std::ostream& output)
{
  output << R"({"adc_full_scale_v":6.144,"adc_max":2047,"supply_v":)" << SUPPLY_VOLTAGE << R"(,"voltage_drop_v":)"
         << VOLTAGE_DROP << R"(,"thermistor_r25":)" << THERMISTOR_R25 << R"(,"thermistor_beta":)" << THERMISTOR_B_CONSTANT
         << R"(,"reference_k":)" << ABSOLUTE_TEMPERATURE_25 << R"(,"series_resistance":)" << SERIES_REFERENCE_RES
         << R"(,"temp_interval_ms":)" << TEMP_SAMPLE_INTERVAL_MS << R"(,"pressure_samples":)" << PRESSURE_SAMPLE_SIZE
         << R"(,"oil_samples":)" << OIL_TEMP_SAMPLE_SIZE << R"(,"water_samples":)" << WATER_TEMP_SAMPLE_SIZE
         << R"(,"oil_channel":)" << static_cast<int>(ADC_CH_OIL_TEMP) << R"(,"water_channel":)"
         << static_cast<int>(ADC_CH_WATER_TEMP) << R"(,"pressure_channel":)" << static_cast<int>(ADC_CH_OIL_PRESSURE)
         << '}';
}

void writeSensorTelemetry(std::ostream& output)
{
  output << R"({"oil_c":)" << calculateAverage(oilTemperatureSamples) << R"(,"water_c":)"
         << calculateAverage(waterTemperatureSamples) << R"(,"pressure_bar":)" << calculateAverage(oilPressureSamples)
         << R"(,"pressure_fault":)" << oilPressureOverVoltage << R"(,"adc_reads":[)";
  for (size_t channel = 0; channel < simulator::ADC_CHANNEL_COUNT; ++channel)
  {
    if (channel != 0)
    {
      output << ',';
    }
    output << simulator::adcReadCounts[channel];
  }
  output << R"(],"adc_last":[)";
  for (size_t channel = 0; channel < simulator::ADC_CHANNEL_COUNT; ++channel)
  {
    if (channel != 0)
    {
      output << ',';
    }
    output << simulator::adcLastRead[channel];
  }
  output << "]}";
}
