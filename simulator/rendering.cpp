#include "compat.hpp"

// 実機と同じ描画・平滑化・警告処理をリンクする。時刻アダプターはこの翻訳単位に限定する。
// NOLINTBEGIN(bugprone-suspicious-include)
#include "../src/modules/backlight.cpp"
#include "../src/modules/display.cpp"
#include "../src/modules/fps_display.cpp"
#include "../src/modules/low_warning.cpp"
#include "../src/modules/racing_indicator.cpp"
#include "../src/modules/racing_mode.cpp"
// NOLINTEND(bugprone-suspicious-include)
