#include "menu.h"

// display.cpp 側で定義している最大値を使用する
extern float recordedMaxOilPressure;
extern float recordedMaxWaterTemp;
extern int recordedMaxOilTempTop;

// メニュー画面を描画する処理
void drawMenuScreen()
{
  mainCanvas.fillScreen(COLOR_BLACK);
  mainCanvas.setFont(&fonts::Font0);
  mainCanvas.setTextSize(1);
  mainCanvas.setTextColor(COLOR_WHITE);

  mainCanvas.setCursor(10, 30);
  mainCanvas.printf("OIL.P MAX: %.1f bar", recordedMaxOilPressure);

  mainCanvas.setCursor(10, 60);
  mainCanvas.printf("WATER.T MAX: %.1f C", recordedMaxWaterTemp);

  mainCanvas.setCursor(10, 90);
  mainCanvas.printf("OIL.T MAX: %d C", recordedMaxOilTempTop);

  int lux = SENSOR_AMBIENT_LIGHT_PRESENT ? CoreS3.Ltr553.getAlsValue() : 0;
  mainCanvas.setCursor(10, 120);
  mainCanvas.printf("LUX: %d", lux);

  mainCanvas.pushSprite(0, 0);
}
