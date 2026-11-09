#ifndef STARTUP_ERROR_H
#define STARTUP_ERROR_H

#include <M5GFX.h>

constexpr auto ADC_INIT_ERROR_TITLE = "ADS1015 init failed";
constexpr auto ADC_INIT_ERROR_HINT = "Check wiring";

// setup直後の実機画面と録画で、同じ描画処理を使う。
void drawAdcInitializationError(lgfx::LGFXBase& screen);

#endif  // STARTUP_ERROR_H
