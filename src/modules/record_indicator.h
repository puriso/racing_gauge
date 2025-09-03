#ifndef RECORD_INDICATOR_H
#define RECORD_INDICATOR_H

#include <M5GFX.h>

#include "config.h"

// 現在録画モードかどうか
extern bool isRecordingMode;

// 録画中表示を描画。描画の更新があれば true を返す
bool drawRecordingIndicator(M5Canvas &canvas);

#endif  // RECORD_INDICATOR_H
