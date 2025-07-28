#ifndef BRIGHTNESS_H
#define BRIGHTNESS_H

#include <stdint.h>

void setBrightness(uint8_t value);
void saveBrightness();
void loadBrightness();
void drawBrightnessMenu();
void handleTouchMenu();

#endif  // BRIGHTNESS_H
