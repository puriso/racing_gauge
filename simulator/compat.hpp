#pragma once

#include <M5GFX.h>

#include "adc_bus.hpp"

namespace simulator
{
extern uint32_t nowMs;
}

// 製品処理の時刻を仮想化し、SDL 内部の実時間には影響させない。
inline auto millis() -> unsigned long { return simulator::nowMs + simulator::adcWaitMs; }
// ADCのポーリング待ちだけ仮想時計へ加算する。取得60Hz・描画24FPSの基準時刻は変えない。
inline void delay(unsigned long durationMs) { simulator::adcWaitMs += durationMs; }
// セトリング待ちの呼び出しは通すが、実機のI2C時間を推測して仮想時計を進めない。
inline void delayMicroseconds([[maybe_unused]] unsigned int durationUs) {}
inline auto radians(float degrees) -> float { return degrees * 0.017453292519943295F; }
using lgfx::fonts::FreeSansBold24pt7b;
