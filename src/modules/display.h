#ifndef DISPLAY_H
#define DISPLAY_H

#include <M5GFX.h>

#include "config.h"
#include "recorded_maximum.h"
#include "sensor.h"

extern M5GFX display;
extern M5Canvas mainCanvas;
extern int currentFps;

void drawOilTemperatureTopBar(M5Canvas& canvas, float oilTemp, const RecordedMaximum<int>& maxOilTemp);
void renderDisplayAndLog(float pressureAvg, float waterTempAvg, float oilTemp, const RecordedMaximum<int>& maxOilTemp);
void updateGaugeMonitoring();
void updateGauges();
void refreshMenuScreen(bool force = false);
void drawMenuScreen();
void resetGaugeState();

#endif  // DISPLAY_H
