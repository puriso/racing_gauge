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
// メニュー表示中も計測値と最大値を更新し、描画だけを省略できる
void updateGauges(bool render = true);
void drawMenuScreen();
void resetGaugeState();

#endif  // DISPLAY_H
