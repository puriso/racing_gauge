#ifndef PRESSURE_GRAPH_H
#define PRESSURE_GRAPH_H

#include <M5GFX.h>

// 油圧ログ追加
void logOilPressure();

// 油圧グラフ描画
void drawPressureGraph(M5Canvas& canvas);

#endif  // PRESSURE_GRAPH_H
