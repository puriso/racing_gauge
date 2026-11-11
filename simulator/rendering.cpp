#include <cmath>

#include "compat.hpp"
#include "telemetry.hpp"

// 実機と同じ描画・平滑化・警告処理をリンクする。時刻アダプターはこの翻訳単位に限定する。
// NOLINTBEGIN(bugprone-suspicious-include)
#include "../src/modules/adc_reader.cpp"
#include "../src/modules/backlight.cpp"
#include "../src/modules/display.cpp"
#include "../src/modules/low_warning.cpp"
#include "../src/modules/racing_indicator.cpp"
#include "../src/modules/racing_mode.cpp"
#include "../src/modules/risk_graph.cpp"
#include "../src/modules/risk_history.cpp"
#include "../src/modules/sensor.cpp"
#include "../src/modules/startup_error.cpp"
// NOLINTEND(bugprone-suspicious-include)

// FPSの計測・表示はシミュレーターへ持ち込まない。
static_assert(FPS_DISPLAY_ENABLED == 0, "FPS measurement overlays are unavailable in the simulator");

static void writeRenderedValue(std::ostream& output, float value)
{
  if (std::isfinite(value))
  {
    output << value;
  }
  else
  {
    output << "null";
  }
}

template <typename T>
static void writeMaximum(std::ostream& output, const RecordedMaximum<T>& maximum)
{
  if (maximum.hasValue())
    output << maximum.value();
  else
    output << "null";
}

void writeDisplayTelemetry(std::ostream& output)
{
  output << R"({"oil_c":)";
  writeRenderedValue(output, displayCache.oilTemp);
  output << R"(,"water_c":)";
  writeRenderedValue(output, displayCache.waterTempAvg);
  output << R"(,"pressure_bar":)";
  writeRenderedValue(output, displayCache.pressureAvg);
  output << R"(,"monitor":[)" << monitoredOilTemp << ',' << monitoredWaterTemp << ',' << monitoredPressure
         << R"(],"maxima":[)";
  writeMaximum(output, recordedMaxOilTempTop);
  output << ',';
  writeMaximum(output, recordedMaxWaterTemp);
  output << ',';
  writeMaximum(output, recordedMaxOilPressure);
  output << ']';
  output << R"(,"brightness_mode":)" << static_cast<int>(currentBrightnessMode) << R"(,"brightness_pwm":)"
         << static_cast<int>(display.getBrightness()) << R"(,"lux_sample":)" << latestLux << R"(,"lux_median":)"
         << medianLuxValue << R"(,"warning_history":)" << hasLowWarningHistory() << R"(,"low_event":{"g":)" << lastLowEventG
         << R"(,"pressure_bar":)" << lastLowEventPressure << R"(,"duration_s":)" << lastLowEventDuration
         << R"(,"direction":")" << lastLowEventDir << R"("}})";
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
         << R"(,"status":[)" << static_cast<int>(oilTemperatureStatus) << ',' << static_cast<int>(waterTemperatureStatus)
         << ',' << static_cast<int>(oilPressureStatus) << ']' << R"(,"pressure_fault":)" << oilPressureOverVoltage
         << R"(,"adc_reads":[)";
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
  output << R"(],"adc_connected":)" << adcInitialized << R"(,"adc_begin_calls":)" << simulator::adcBeginCalls
         << R"(,"adc_wait_ms":)" << simulator::adcWaitMs << '}';
}

void writeRiskTelemetry(std::ostream& output)
{
  const auto& history = risk::history;
  const auto& event = history.event();
  output << R"({"kind":)" << static_cast<int>(event.kind) << R"(,"at_ms":)" << event.occurredMs << R"(,"age_ms":)"
         << (history.hasEvent() ? history.ageMs() : 0) << R"(,"pressure_bar":)" << event.pressure << R"(,"g":)" << event.g
         << R"(,"oil_c":)";
  if (event.oilValid)
    output << event.oil;
  else
    output << "null";
  output << R"(,"pre_ms":)" << (history.hasEvent() ? history.preMs() : 0) << R"(,"post_ms":)"
         << (history.hasEvent() ? history.postMs() : 0) << '}';
}

void writeRiskWindow(std::ostream& output)
{
  output << '[';
  for (size_t index = 0; index < risk::WINDOW_SAMPLES; ++index)
  {
    if (index != 0) output << ',';
    const auto& sample = risk::history.sample(index);
    output << '[' << sample.pressureMin << ',' << sample.pressureMax << ',' << sample.gMax << ',' << sample.oilLast << ','
           << static_cast<int>(sample.flags) << ']';
  }
  output << ']';
}
