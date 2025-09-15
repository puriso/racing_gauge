#ifndef RACING_INDICATOR_H
#define RACING_INDICATOR_H

#include <M5GFX.h>

#include "config.h"

// 現在レーシングモードかどうか
extern bool isRacingMode;

// レーシング中表示を描画。描画の更新があれば true を返す
bool drawRacingIndicator(M5Canvas &canvas);

// R表示の描画状態をリセット
void resetRacingIndicator();
#endif  // RACING_INDICATOR_H
