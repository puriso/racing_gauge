#include "compat.hpp"
#include "telemetry.hpp"

// 実機と同じ描画・平滑化・警告処理をリンクする。時刻アダプターはこの翻訳単位に限定する。
// NOLINTBEGIN(bugprone-suspicious-include)
#include "../src/modules/backlight.cpp"
#include "../src/modules/display.cpp"
#include "../src/modules/fps_display.cpp"
#include "../src/modules/low_warning.cpp"
#include "../src/modules/racing_indicator.cpp"
#include "../src/modules/racing_mode.cpp"
// NOLINTEND(bugprone-suspicious-include)

void writeDisplayTelemetry(std::ostream& output)
{
  output << R"({"oil_c":)" << displayCache.oilTemp << R"(,"water_c":)" << displayCache.waterTempAvg << R"(,"pressure_bar":)"
         << displayCache.pressureAvg << R"(,"brightness_mode":)" << static_cast<int>(currentBrightnessMode)
         << R"(,"brightness_pwm":)" << static_cast<int>(display.getBrightness()) << R"(,"lux_sample":)" << latestLux
         << R"(,"lux_median":)" << medianLuxValue << '}';
}
