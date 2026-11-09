#include "startup_error.h"

#include "config.h"

void drawAdcInitializationError(lgfx::LGFXBase& screen)
{
  screen.setTextSize(2);
  screen.setTextColor(COLOR_RED);
  screen.setCursor(0, 0);
  screen.println(ADC_INIT_ERROR_TITLE);
  screen.println(ADC_INIT_ERROR_HINT);
}
