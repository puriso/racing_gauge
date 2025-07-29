#ifndef BRIGHTNESS_H
#define BRIGHTNESS_H

#include <stdint.h>

// 輝度値を設定
void setBrightness(uint8_t value);
// 現在の輝度を保存
void saveBrightness();
// 保存済みの輝度を読み出す
void loadBrightness();
// 輝度設定メニュー描画
void drawBrightnessMenu();
// タッチ操作を処理
void handleTouchMenu();

#endif  // BRIGHTNESS_H
