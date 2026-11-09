#pragma once

#include <M5GFX.h>

namespace simulator
{
extern uint32_t nowMs;
}

// 描画・警告の時刻だけを仮想化し、SDL 内部の実時間には影響させない。
inline auto millis() -> unsigned long { return simulator::nowMs; }
// セトリング待ちの呼び出しは通すが、実機のI2C時間を推測して仮想時計を進めない。
inline void delayMicroseconds([[maybe_unused]] unsigned int durationUs) {}
inline auto radians(float degrees) -> float { return degrees * 0.017453292519943295F; }
using lgfx::fonts::FreeSansBold24pt7b;
