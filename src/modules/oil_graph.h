#ifndef OIL_GRAPH_H
#define OIL_GRAPH_H

#include <M5GFX.h>

void initOilPressureHistory();
void addOilPressureHistory(float pressure);
void drawOilPressureGraph(int index);
int getOilGraphCount();

#endif  // OIL_GRAPH_H
