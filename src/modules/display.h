#ifndef DISPLAY_H
#define DISPLAY_H

#include <M5GFX.h>

#include "config.h"
#include "sensor.h"

extern M5GFX display;
extern M5Canvas mainCanvas;
extern int currentFps;

void drawOilTemperatureTopBar(M5Canvas& canvas, float oilTemp, int maxOilTemp);
void renderDisplayAndLog(float pressureAvg, float waterTempAvg, float oilTemp, int16_t maxOilTemp);
void updateGauges();
void resetGaugeState();

#endif  // DISPLAY_H
